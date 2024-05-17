#include "parallel_rdp_wrapper.hpp"
#include "frontend/gui.hpp"
#include "frontend/message.hpp"
#include "interface/vi.hpp"
#include "log.hpp"
#include "memory/rdram.hpp"
#include "n64/rdp/rdp.hpp"
#include "rdp.hpp"
#include "SDL.h"
#include "SDL_vulkan.h"
#include "status.hpp"
#include "volk/volk.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <vector>

namespace n64::rdp {

// clang-format off

constexpr u32 vertex_spirv[] = {
	0x07230203, 0x00010000, 0x000d000a, 0x00000034,
	0x00000000, 0x00020011, 0x00000001, 0x0006000b,
	0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
	0x00000000, 0x0003000e, 0x00000000, 0x00000001,
	0x0008000f, 0x00000000, 0x00000004, 0x6e69616d,
	0x00000000, 0x00000008, 0x00000016, 0x0000002b,
	0x00040047, 0x00000008, 0x0000000b, 0x0000002a,
	0x00050048, 0x00000014, 0x00000000, 0x0000000b,
	0x00000000, 0x00050048, 0x00000014, 0x00000001,
	0x0000000b, 0x00000001, 0x00050048, 0x00000014,
	0x00000002, 0x0000000b, 0x00000003, 0x00050048,
	0x00000014, 0x00000003, 0x0000000b, 0x00000004,
	0x00030047, 0x00000014, 0x00000002, 0x00040047,
	0x0000002b, 0x0000001e, 0x00000000, 0x00020013,
	0x00000002, 0x00030021, 0x00000003, 0x00000002,
	0x00040015, 0x00000006, 0x00000020, 0x00000001,
	0x00040020, 0x00000007, 0x00000001, 0x00000006,
	0x0004003b, 0x00000007, 0x00000008, 0x00000001,
	0x0004002b, 0x00000006, 0x0000000a, 0x00000000,
	0x00020014, 0x0000000b, 0x00030016, 0x0000000f,
	0x00000020, 0x00040017, 0x00000010, 0x0000000f,
	0x00000004, 0x00040015, 0x00000011, 0x00000020,
	0x00000000, 0x0004002b, 0x00000011, 0x00000012,
	0x00000001, 0x0004001c, 0x00000013, 0x0000000f,
	0x00000012, 0x0006001e, 0x00000014, 0x00000010,
	0x0000000f, 0x00000013, 0x00000013, 0x00040020,
	0x00000015, 0x00000003, 0x00000014, 0x0004003b,
	0x00000015, 0x00000016, 0x00000003, 0x0004002b,
	0x0000000f, 0x00000017, 0xbf800000, 0x0004002b,
	0x0000000f, 0x00000018, 0x00000000, 0x0004002b,
	0x0000000f, 0x00000019, 0x3f800000, 0x0007002c,
	0x00000010, 0x0000001a, 0x00000017, 0x00000017,
	0x00000018, 0x00000019, 0x00040020, 0x0000001b,
	0x00000003, 0x00000010, 0x0004002b, 0x00000006,
	0x0000001f, 0x00000001, 0x0004002b, 0x0000000f,
	0x00000023, 0x40400000, 0x0007002c, 0x00000010,
	0x00000024, 0x00000017, 0x00000023, 0x00000018,
	0x00000019, 0x0007002c, 0x00000010, 0x00000027,
	0x00000023, 0x00000017, 0x00000018, 0x00000019,
	0x00040017, 0x00000029, 0x0000000f, 0x00000002,
	0x00040020, 0x0000002a, 0x00000003, 0x00000029,
	0x0004003b, 0x0000002a, 0x0000002b, 0x00000003,
	0x0004002b, 0x0000000f, 0x0000002f, 0x3f000000,
	0x0005002c, 0x00000029, 0x00000033, 0x0000002f,
	0x0000002f, 0x00050036, 0x00000002, 0x00000004,
	0x00000000, 0x00000003, 0x000200f8, 0x00000005,
	0x0004003d, 0x00000006, 0x00000009, 0x00000008,
	0x000500aa, 0x0000000b, 0x0000000c, 0x00000009,
	0x0000000a, 0x000300f7, 0x0000000e, 0x00000000,
	0x000400fa, 0x0000000c, 0x0000000d, 0x0000001d,
	0x000200f8, 0x0000000d, 0x00050041, 0x0000001b,
	0x0000001c, 0x00000016, 0x0000000a, 0x0003003e,
	0x0000001c, 0x0000001a, 0x000200f9, 0x0000000e,
	0x000200f8, 0x0000001d, 0x000500aa, 0x0000000b,
	0x00000020, 0x00000009, 0x0000001f, 0x000300f7,
	0x00000022, 0x00000000, 0x000400fa, 0x00000020,
	0x00000021, 0x00000026, 0x000200f8, 0x00000021,
	0x00050041, 0x0000001b, 0x00000025, 0x00000016,
	0x0000000a, 0x0003003e, 0x00000025, 0x00000024,
	0x000200f9, 0x00000022, 0x000200f8, 0x00000026,
	0x00050041, 0x0000001b, 0x00000028, 0x00000016,
	0x0000000a, 0x0003003e, 0x00000028, 0x00000027,
	0x000200f9, 0x00000022, 0x000200f8, 0x00000022,
	0x000200f9, 0x0000000e, 0x000200f8, 0x0000000e,
	0x00050041, 0x0000001b, 0x0000002c, 0x00000016,
	0x0000000a, 0x0004003d, 0x00000010, 0x0000002d,
	0x0000002c, 0x0007004f, 0x00000029, 0x0000002e,
	0x0000002d, 0x0000002d, 0x00000000, 0x00000001,
	0x0005008e, 0x00000029, 0x00000030, 0x0000002e,
	0x0000002f, 0x00050081, 0x00000029, 0x00000032,
	0x00000030, 0x00000033, 0x0003003e, 0x0000002b,
	0x00000032, 0x000100fd, 0x00010038
};

constexpr u32 fragment_spirv[] = {
	0x07230203, 0x00010000, 0x000d000a, 0x00000015,
	0x00000000, 0x00020011, 0x00000001, 0x0006000b,
	0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
	0x00000000, 0x0003000e, 0x00000000, 0x00000001,
	0x0007000f, 0x00000004, 0x00000004, 0x6e69616d,
	0x00000000, 0x00000009, 0x00000011, 0x00030010,
	0x00000004, 0x00000007, 0x00040047, 0x00000009,
	0x0000001e, 0x00000000, 0x00040047, 0x0000000d,
	0x00000022, 0x00000000, 0x00040047, 0x0000000d,
	0x00000021, 0x00000000, 0x00040047, 0x00000011,
	0x0000001e, 0x00000000, 0x00020013, 0x00000002,
	0x00030021, 0x00000003, 0x00000002, 0x00030016,
	0x00000006, 0x00000020, 0x00040017, 0x00000007,
	0x00000006, 0x00000004, 0x00040020, 0x00000008,
	0x00000003, 0x00000007, 0x0004003b, 0x00000008,
	0x00000009, 0x00000003, 0x00090019, 0x0000000a,
	0x00000006, 0x00000001, 0x00000000, 0x00000000,
	0x00000000, 0x00000001, 0x00000000, 0x0003001b,
	0x0000000b, 0x0000000a, 0x00040020, 0x0000000c,
	0x00000000, 0x0000000b, 0x0004003b, 0x0000000c,
	0x0000000d, 0x00000000, 0x00040017, 0x0000000f,
	0x00000006, 0x00000002, 0x00040020, 0x00000010,
	0x00000001, 0x0000000f, 0x0004003b, 0x00000010,
	0x00000011, 0x00000001, 0x0004002b, 0x00000006,
	0x00000013, 0x00000000, 0x00050036, 0x00000002,
	0x00000004, 0x00000000, 0x00000003, 0x000200f8,
	0x00000005, 0x0004003d, 0x0000000b, 0x0000000e,
	0x0000000d, 0x0004003d, 0x0000000f, 0x00000012,
	0x00000011, 0x00070058, 0x00000007, 0x00000014,
	0x0000000e, 0x00000012, 0x00000002, 0x00000013,
	0x0003003e, 0x00000009, 0x00000014, 0x000100fd,
	0x00010038
};

// clang-format on

ParallelRdpWrapper::ParallelRdpWrapper(std::unique_ptr<SDLWSIPlatform> sdl_wsi_platform,
  std::unique_ptr<Vulkan::WSI> wsi,
  std::unique_ptr<::RDP::CommandProcessor> cmd_processor)
  : sdl_wsi_platform_(std::move(sdl_wsi_platform)),
    wsi_(std::move(wsi)),
    cmd_processor_(std::move(cmd_processor)),
    requested_command_buffer_()
{
    implementation = this;
    ReloadViRegisters();
}

ParallelRdpWrapper::~ParallelRdpWrapper()
{
    implementation = nullptr;
}

std::unique_ptr<ParallelRdpWrapper> ParallelRdpWrapper::Create(SDL_Window* sdl_window)
{
    if (!sdl_window) {
        std::println("null SDL_Window provided to ParallelRdpWrapper");
        return {};
    }

    if (!(SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_VULKAN)) {
        std::println("SDL_Window provided to ParallelRdpWrapper not created with SDL_WINDOW_VULKAN window flag!");
        return {};
    }

    if (volkInitialize() != VK_SUCCESS) {
        std::println("Failed to initialize volk.");
        return {};
    }

    auto sdl_wsi_platform = std::make_unique<SDLWSIPlatform>(sdl_window);
    auto wsi = std::make_unique<Vulkan::WSI>();
    wsi->set_platform(sdl_wsi_platform.get());
    wsi->set_backbuffer_srgb(false);
    wsi->set_present_mode(Vulkan::PresentMode::UnlockedMaybeTear);
    Vulkan::Context::SystemHandles handles{};
    if (!wsi->init_simple(1, handles)) {
        std::println("Failed to init ParallelRDP wsi");
        return {};
    }

    /* Construct command processor, which we later supply with RDP commands */
    u8* rdram_ptr = rdram::GetPointerToMemory();
    size_t rdram_offset{};
    size_t rdram_size = rdram::GetSize();
    size_t hidden_rdram_size = rdram_size / 8;
    ::RDP::CommandProcessorFlags flags{};
    auto cmd_processor = std::make_unique<::RDP::CommandProcessor>(wsi->get_device(),
      rdram_ptr,
      rdram_offset,
      rdram_size,
      hidden_rdram_size,
      flags);
    if (!cmd_processor->device_is_supported()) {
        std::println("Vulkan device not supported.");
        return {};
    }

    return std::unique_ptr<ParallelRdpWrapper>(
      new ParallelRdpWrapper(std::move(sdl_wsi_platform), std::move(wsi), std::move(cmd_processor)));
}

void ParallelRdpWrapper::EnqueueCommand(int cmd_len, u32* cmd_ptr)
{
    cmd_processor_->enqueue_command(cmd_len, cmd_ptr);
}

void ParallelRdpWrapper::OnFullSync()
{
    cmd_processor_->wait_for_timeline(cmd_processor_->signal_timeline());
}

VkCommandBuffer ParallelRdpWrapper::GetVkCommandBuffer()
{
    requested_command_buffer_ = wsi_->get_device().request_command_buffer();
    return requested_command_buffer_->get_command_buffer();
}

VkDevice ParallelRdpWrapper::GetVkDevice()
{
    return wsi_->get_device().get_device();
}

VkFormat ParallelRdpWrapper::GetVkFormat()
{
    return wsi_->get_device().get_swapchain_view().get_format();
}

u32 ParallelRdpWrapper::GetVkQueueFamily()
{
    return wsi_->get_context().get_queue_info().family_indices[Vulkan::QueueIndices::QUEUE_INDEX_GRAPHICS];
}

VkInstance ParallelRdpWrapper::GetVkInstance()
{
    return wsi_->get_context().get_instance();
}

VkPhysicalDevice ParallelRdpWrapper::GetVkPhysicalDevice()
{
    return wsi_->get_device().get_physical_device();
}

VkQueue ParallelRdpWrapper::GetVkQueue()
{
    return wsi_->get_context().get_queue_info().queues[Vulkan::QueueIndices::QUEUE_INDEX_GRAPHICS];
}

void ParallelRdpWrapper::SetRenderCallback(RenderCallback render)
{
    if (!render) {
        std::println("null render callback provided to ParallelRdpWrapper");
    }
    render_ = render;
}

void ParallelRdpWrapper::ReloadViRegisters()
{
    /* TODO: only call set_vi_register when a VI register is actually written to, if that does not lead to race
     * conditions */
    vi::Registers const& vi = vi::ReadAllRegisters();
    cmd_processor_->set_vi_register(::RDP::VIRegister::Control, vi.ctrl);
    cmd_processor_->set_vi_register(::RDP::VIRegister::Origin, vi.origin);
    cmd_processor_->set_vi_register(::RDP::VIRegister::Width, vi.width);
    cmd_processor_->set_vi_register(::RDP::VIRegister::Intr, vi.v_intr);
    cmd_processor_->set_vi_register(::RDP::VIRegister::VCurrentLine, vi.v_current);
    cmd_processor_->set_vi_register(::RDP::VIRegister::Timing, vi.burst);
    cmd_processor_->set_vi_register(::RDP::VIRegister::VSync, vi.v_sync);
    cmd_processor_->set_vi_register(::RDP::VIRegister::HSync, vi.h_sync);
    cmd_processor_->set_vi_register(::RDP::VIRegister::Leap, vi.h_sync_leap);
    cmd_processor_->set_vi_register(::RDP::VIRegister::HStart, vi.h_video);
    cmd_processor_->set_vi_register(::RDP::VIRegister::VStart, vi.v_video);
    cmd_processor_->set_vi_register(::RDP::VIRegister::VBurst, vi.v_burst);
    cmd_processor_->set_vi_register(::RDP::VIRegister::XScale, vi.x_scale);
    cmd_processor_->set_vi_register(::RDP::VIRegister::YScale, vi.y_scale);
}

void ParallelRdpWrapper::SubmitRequestedVkCommandBuffer()
{
    wsi_->get_device().submit(requested_command_buffer_);
}

void ParallelRdpWrapper::UpdateScreen()
{
    ReloadViRegisters();

    Vulkan::Device& device = wsi_->get_device();

    wsi_->begin_frame();

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

    Vulkan::ImageHandle image = cmd_processor_->scanout(scanout_opts);

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

        render_(cmd->get_command_buffer());

        cmd->end_render_pass();
    }
    device.submit(cmd);
    wsi_->end_frame();
}

ParallelRdpWrapper::SDLWSIPlatform::SDLWSIPlatform(SDL_Window* sdl_window) : sdl_window(sdl_window)
{
}

bool ParallelRdpWrapper::SDLWSIPlatform::alive([[maybe_unused]] Vulkan::WSI& wsi)
{
    return true;
}

VkSurfaceKHR ParallelRdpWrapper::SDLWSIPlatform::create_surface(VkInstance instance,
  [[maybe_unused]] VkPhysicalDevice gpu)
{
    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(sdl_window, instance, nullptr, &vk_surface)) {
        LogError(std::format("Failed to create Vulkan surface: {}", SDL_GetError()));
        return VK_NULL_HANDLE;
    }
    return vk_surface;
}

void ParallelRdpWrapper::SDLWSIPlatform::event_frame_tick([[maybe_unused]] double frame,
  [[maybe_unused]] double elapsed)
{
}

VkApplicationInfo const* ParallelRdpWrapper::SDLWSIPlatform::get_application_info()
{
    static constexpr VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "teesoe",
        .apiVersion = VK_API_VERSION_1_1,
    };
    return &app_info;
}

std::vector<char const*> ParallelRdpWrapper::SDLWSIPlatform::get_instance_extensions()
{
    uint num_extensions;
    char const* const* extensions = SDL_Vulkan_GetInstanceExtensions(&num_extensions);
    return std::vector<char const*>{ extensions, extensions + num_extensions };
}

u32 ParallelRdpWrapper::SDLWSIPlatform::get_surface_width()
{
    return 640;
}

u32 ParallelRdpWrapper::SDLWSIPlatform::get_surface_height()
{
    return 480;
}

void ParallelRdpWrapper::SDLWSIPlatform::poll_input()
{
    frontend::gui::PollEvents();
}

} // namespace n64::rdp
