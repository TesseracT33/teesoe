#include "parallel_rdp_wrapper.hpp"
#include "frontend/gui.hpp"
#include "frontend/vulkan.hpp"
#include "interface/vi.hpp"
#include "log.hpp"
#include "memory/rdram.hpp"
#include "parallel-rdp-standalone/parallel-rdp/rdp_device.hpp"
#include "parallel-rdp-standalone/volk/volk.h"
#include "parallel-rdp-standalone/vulkan/wsi.hpp"
#include "rdp.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

namespace n64::rdp {

void ParallelRDPWrapper::EnqueueCommand(int cmd_len, u32* cmd_ptr)
{
    cmd_processor->enqueue_command(cmd_len, cmd_ptr);
}

VkCommandBuffer ParallelRDPWrapper::GetVkCommandBuffer()
{
    requested_command_buffer = wsi.get_device().request_command_buffer();
    return requested_command_buffer->get_command_buffer();
}

VkDevice ParallelRDPWrapper::GetVkDevice()
{
    return wsi.get_device().get_device();
}

VkFormat ParallelRDPWrapper::GetVkFormat()
{
    return wsi.get_device().get_swapchain_view().get_format();
}

u32 ParallelRDPWrapper::GetVkQueueFamily()
{
    return wsi.get_context().get_queue_info().family_indices[Vulkan::QueueIndices::QUEUE_INDEX_GRAPHICS];
}

VkInstance ParallelRDPWrapper::GetVkInstance()
{
    return wsi.get_context().get_instance();
}

VkPhysicalDevice ParallelRDPWrapper::GetVkPhysicalDevice()
{
    return wsi.get_device().get_physical_device();
}

VkQueue ParallelRDPWrapper::GetVkQueue()
{
    return wsi.get_context().get_queue_info().queues[Vulkan::QueueIndices::QUEUE_INDEX_GRAPHICS];
}

Status ParallelRDPWrapper::Initialize()
{
    if (volkInitialize() != VK_SUCCESS) {
        return FailureStatus("[parallel-rdp] Failed to initialize volk.");
    }

    SDL_Window* sdl_window = frontend::gui::GetSdlWindow();
    assert(sdl_window);
    assert(SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_VULKAN);

    sdl_wsi_platform = std::make_unique<SDLWSIPlatform>(sdl_window);
    wsi.set_platform(sdl_wsi_platform.get());
    wsi.set_backbuffer_srgb(false);
    wsi.set_present_mode(Vulkan::PresentMode::UnlockedMaybeTear);
    Vulkan::Context::SystemHandles handles{};
    if (!wsi.init_simple(1, handles)) {
        return FailureStatus("Failed to init wsi.");
    }

    /* Construct command processor, which we later supply with RDP commands */
    u8* rdram_ptr = rdram::GetPointerToMemory();
    size_t rdram_offset{};
    size_t rdram_size = rdram::GetSize();
    size_t hidden_rdram_size = rdram_size / 8;
    ::RDP::CommandProcessorFlags flags{};
    cmd_processor = std::make_unique<::RDP::CommandProcessor>(wsi.get_device(),
      rdram_ptr,
      rdram_offset,
      rdram_size,
      hidden_rdram_size,
      flags);
    if (!cmd_processor->device_is_supported()) {
        return FailureStatus("Vulkan device not supported.");
    }

    if (Status status = vulkan::Init(); !status.Ok()) {
        return status;
    }

    ReloadViRegisters();

    return OkStatus();
}

void ParallelRDPWrapper::OnFullSync()
{
    cmd_processor->wait_for_timeline(cmd_processor->signal_timeline());
}

void ParallelRDPWrapper::ReloadViRegisters()
{
    /* TODO: only call set_vi_register when a VI register is actually written to, if that does not lead to race
     * conditions */
    vi::Registers const& vi = vi::ReadAllRegisters();
    cmd_processor->set_vi_register(::RDP::VIRegister::Control, vi.ctrl);
    cmd_processor->set_vi_register(::RDP::VIRegister::Origin, vi.origin);
    cmd_processor->set_vi_register(::RDP::VIRegister::Width, vi.width);
    cmd_processor->set_vi_register(::RDP::VIRegister::Intr, vi.v_intr);
    cmd_processor->set_vi_register(::RDP::VIRegister::VCurrentLine, vi.v_current);
    cmd_processor->set_vi_register(::RDP::VIRegister::Timing, vi.burst);
    cmd_processor->set_vi_register(::RDP::VIRegister::VSync, vi.v_sync);
    cmd_processor->set_vi_register(::RDP::VIRegister::HSync, vi.h_sync);
    cmd_processor->set_vi_register(::RDP::VIRegister::Leap, vi.h_sync_leap);
    cmd_processor->set_vi_register(::RDP::VIRegister::HStart, vi.h_video);
    cmd_processor->set_vi_register(::RDP::VIRegister::VStart, vi.v_video);
    cmd_processor->set_vi_register(::RDP::VIRegister::VBurst, vi.v_burst);
    cmd_processor->set_vi_register(::RDP::VIRegister::XScale, vi.x_scale);
    cmd_processor->set_vi_register(::RDP::VIRegister::YScale, vi.y_scale);
}

void ParallelRDPWrapper::SubmitRequestedVkCommandBuffer()
{
    wsi.get_device().submit(requested_command_buffer);
}

void ParallelRDPWrapper::TearDown()
{
}

void ParallelRDPWrapper::UpdateScreen()
{
    ReloadViRegisters();

    Vulkan::Device& device = wsi.get_device();

    wsi.begin_frame();

    static constexpr ::RDP::ScanoutOptions scanout_opts = [] {
        ::RDP::ScanoutOptions opts;
        opts.persist_frame_on_invalid_input = true;
        opts.vi.aa = true;
        opts.vi.scale = true;
        opts.vi.dither_filter = true;
        opts.vi.divot_filter = true;
        opts.vi.gamma_dither = true;
        opts.downscale_steps = true;
        opts.crop_overscan_pixels = true;
        opts.persist_frame_on_invalid_input = true;
        return opts;
    }();

    Vulkan::ImageHandle image = cmd_processor->scanout(scanout_opts);

    // Normally reflection is automated.
    Vulkan::ResourceLayout vertex_layout = {};
    Vulkan::ResourceLayout fragment_layout = {};
    fragment_layout.output_mask = 1;
    fragment_layout.sets[0].sampled_image_mask = 1;

    // This request is cached.
    Vulkan::Program* program = device.request_program(vertex_spirv,
      sizeof(vertex_spirv),
      fragment_spirv,
      sizeof(fragment_spirv),
      &vertex_layout,
      &fragment_layout);

    // Blit image on screen.
    Vulkan::CommandBufferHandle cmd = device.request_command_buffer();
    {
        Vulkan::RenderPassInfo rp = device.get_swapchain_render_pass(Vulkan::SwapchainRenderPass::ColorOnly);
        cmd->begin_render_pass(rp);

        cmd->set_program(program);

        // Basic default render state.
        cmd->set_opaque_state();
        cmd->set_depth_test(false, false);
        cmd->set_cull_mode(VK_CULL_MODE_NONE);

        // If we don't have an image, we just get a cleared screen in the render pass.
        if (image) {
            cmd->set_texture(0, 0, image->get_view(), Vulkan::StockSampler::LinearClamp);
            // The vertices are constants in the shader.
            // Draws fullscreen quad using oversized triangle.
            cmd->draw(3);
        }

        frontend::gui::FrameVulkan(cmd->get_command_buffer());

        cmd->end_render_pass();
    }
    device.submit(cmd);
    wsi.end_frame();
}

ParallelRDPWrapper::SDLWSIPlatform::SDLWSIPlatform(SDL_Window* sdl_window) : sdl_window(sdl_window)
{
}

bool ParallelRDPWrapper::SDLWSIPlatform::alive([[maybe_unused]] Vulkan::WSI& wsi)
{
    return true;
}

VkSurfaceKHR ParallelRDPWrapper::SDLWSIPlatform::create_surface(VkInstance instance,
  [[maybe_unused]] VkPhysicalDevice gpu)
{
    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(sdl_window, instance, nullptr, &vk_surface)) {
        LogError(std::format("Failed to create Vulkan surface: {}", SDL_GetError()));
        return VK_NULL_HANDLE;
    }
    return vk_surface;
}

void ParallelRDPWrapper::SDLWSIPlatform::event_frame_tick([[maybe_unused]] double frame,
  [[maybe_unused]] double elapsed)
{
}

VkApplicationInfo const* ParallelRDPWrapper::SDLWSIPlatform::get_application_info()
{
    static constexpr VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "N63.5",
        .apiVersion = VK_API_VERSION_1_1,
    };
    return &app_info;
}

std::vector<char const*> ParallelRDPWrapper::SDLWSIPlatform::get_instance_extensions()
{
    uint num_extensions;
    char const* const* extensions = SDL_Vulkan_GetInstanceExtensions(&num_extensions);
    return std::vector<char const*>{ extensions, extensions + num_extensions };
}

u32 ParallelRDPWrapper::SDLWSIPlatform::get_surface_width()
{
    return 640;
}

u32 ParallelRDPWrapper::SDLWSIPlatform::get_surface_height()
{
    return 480;
}

void ParallelRDPWrapper::SDLWSIPlatform::poll_input()
{
    frontend::gui::PollEvents();
}

} // namespace n64::rdp
