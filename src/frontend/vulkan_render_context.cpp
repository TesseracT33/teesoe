#include "vulkan_render_context.hpp"
#include "frontend/message.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "SDL.h"

#include "log.hpp"

#include <cstdlib>
#include <print>
#include <utility>

VkInstance vk_instance;

VulkanRenderContext::VulkanRenderContext(SDL_Window* sdl_window,
  UpdateGuiCallback update_gui,
  std::unique_ptr<n64::rdp::ParallelRdpWrapper> parallel_rdp,
  VkDescriptorPool vk_descriptor_pool)
  : RenderContext(sdl_window, update_gui),
    parallel_rdp_(std::move(parallel_rdp)),
    vk_descriptor_pool_(vk_descriptor_pool)
{
    parallel_rdp_->SetRenderCallback(std::function<void(VkCommandBuffer)>(
      std::bind(static_cast<void (VulkanRenderContext::*)(VkCommandBuffer)>(&VulkanRenderContext::Render),
        this,
        std::placeholders::_1)));
}

VulkanRenderContext::~VulkanRenderContext()
{
    vkDeviceWaitIdle(parallel_rdp_->GetVkDevice());
    vkDestroyDescriptorPool(parallel_rdp_->GetVkDevice(), vk_descriptor_pool_, nullptr);
    parallel_rdp_ = {};
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    // ImGui::Gui_ImplVulkanH_DestroyWindow(vulkan::GetInstance(), vulkan::GetDevice(), &vk_main_window_data,
    // vulkan::GetAllocator());
    ImGui::DestroyContext();
    SDL_DestroyWindow(sdl_window);
}

std::unique_ptr<VulkanRenderContext> VulkanRenderContext::Create(UpdateGuiCallback update_gui)
{
    if (!update_gui) {
        std::println("null gui update callbackprovided to vulkan render context");
        return {};
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::println("Failed call to SDL_Init: {}", SDL_GetError());
        return {};
    }

    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    SDL_Window* sdl_window =
      SDL_CreateWindow("teesoe", 1280, 960, SDL_WINDOW_VULKAN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_RESIZABLE);
    if (!sdl_window) {
        std::println("Failed call to SDL_CreateWindow: {}", SDL_GetError());
        return {};
    }

    std::unique_ptr<n64::rdp::ParallelRdpWrapper> parallel_rdp = n64::rdp::ParallelRdpWrapper::Create(sdl_window);
    if (!parallel_rdp) {
        std::println("Failed to create ParallelRdpWrapper object!");
        return {};
    }

    VkInstance vk_instance = parallel_rdp->GetVkInstance();
    VkPhysicalDevice vk_physical_device = parallel_rdp->GetVkPhysicalDevice();
    VkDevice vk_device = parallel_rdp->GetVkDevice();
    u32 vk_queue_family = parallel_rdp->GetVkQueueFamily();
    VkQueue vk_queue = parallel_rdp->GetVkQueue();
    VkCommandBuffer vk_command_buffer = parallel_rdp->GetVkCommandBuffer();
    VkFormat vk_format = parallel_rdp->GetVkFormat();

    ::vk_instance = vk_instance;

    VkDescriptorPool vk_descriptor_pool{};
    VkRenderPass vk_render_pass{};
    VkResult vk_result{};

    auto CheckVkResult = [](VkResult vk_result) {
        if (vk_result != 0) {
            LogError(std::format("[vulkan]: Error: VkResult = {}", std::to_underlying(vk_result)));
        }
    };

    { // Create Descriptor Pool
        static constexpr VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * (u32)std::size(pool_sizes);
        pool_info.poolSizeCount = (u32)std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        vk_result = vkCreateDescriptorPool(vk_device, &pool_info, nullptr, &vk_descriptor_pool);
        CheckVkResult(vk_result);
    }

    { // Create the Render Pass
        VkAttachmentDescription attachment = {};
        attachment.format = vk_format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        vk_result = vkCreateRenderPass(vk_device, &info, nullptr, &vk_render_pass);
        CheckVkResult(vk_result);
    }

    IMGUI_CHECKVERSION();
    (void)ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplVulkan_LoadFunctions(
          [](char const* fn, void*) { return vkGetInstanceProcAddr(::vk_instance, fn); })) {
        LogError("Failed call to ImGui_ImplVulkan_LoadFunctions");
        return {};
    }

    if (!ImGui_ImplSDL3_InitForVulkan(sdl_window)) {
        LogError("Failed call to ImGui_ImplSDL3_InitForVulkan");
        return {};
    }

    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance = vk_instance,
        .PhysicalDevice = vk_physical_device,
        .Device = vk_device,
        .QueueFamily = vk_queue_family,
        .Queue = vk_queue,
        .DescriptorPool = vk_descriptor_pool,
        .MinImageCount = 2,
        .ImageCount = 2,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .CheckVkResultFn = CheckVkResult,
    };

    if (!ImGui_ImplVulkan_Init(&init_info, vk_render_pass)) {
        LogError("Failed call to ImGui_ImplVulkan_Init");
        return {};
    }

    (void)io.Fonts->AddFontDefault();

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        LogWarn("Failed call to ImGui_ImplVulkan_CreateFontsTexture");
    }

    parallel_rdp->SubmitRequestedVkCommandBuffer();

    return std::unique_ptr<VulkanRenderContext>(
      new VulkanRenderContext(sdl_window, std::move(update_gui), std::move(parallel_rdp), vk_descriptor_pool));
}

void VulkanRenderContext::EnableFullscreen(bool enable)
{
}

void VulkanRenderContext::EnableRendering(bool enable)
{
}

void VulkanRenderContext::NotifyNewGameFrameReady()
{
}

void VulkanRenderContext::Render()
{
    Render(parallel_rdp_->GetVkCommandBuffer());
}

void VulkanRenderContext::Render(VkCommandBuffer vk_command_buffer)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    UpdateFrameCounter();
    update_gui();
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk_command_buffer);
}

void VulkanRenderContext::SetFramebufferHeight(uint height)
{
}

void VulkanRenderContext::SetFramebufferPtr(u8 const* ptr)
{
}

void VulkanRenderContext::SetFramebufferSize(uint width, uint height)
{
}

void VulkanRenderContext::SetFramebufferWidth(uint width)
{
}

void VulkanRenderContext::SetGameRenderAreaOffsetX(uint offset)
{
}

void VulkanRenderContext::SetGameRenderAreaOffsetY(uint offset)
{
}

void VulkanRenderContext::SetGameRenderAreaSize(uint width, uint height)
{
}

void VulkanRenderContext::SetPixelFormat(PixelFormat pixel_format)
{
    (void)pixel_format;
}

void VulkanRenderContext::SetWindowSize(uint width, uint height)
{
}
