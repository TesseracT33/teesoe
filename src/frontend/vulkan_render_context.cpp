#include "vulkan_render_context.hpp"
#include "frontend/message.hpp"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "log.hpp"
#include "n64/rdp/rdp.hpp"
#include "parallel-rdp-standalone/volk/volk.h"

#include <cstdlib>
#include <format>
#include <utility>

VulkanRenderContext::VulkanRenderContext(SDL_Window* sdl_window,
  DrawGuiCallbackFunc draw_gui,
  n64::rdp::ParallelRDPWrapper* parallel_rdp,
  VkDescriptorPool vk_descriptor_pool)
  : RenderContext(sdl_window, draw_gui),
    parallel_rdp(parallel_rdp),
    vk_descriptor_pool(vk_descriptor_pool)
{
}

VulkanRenderContext::~VulkanRenderContext()
{
    vkDeviceWaitIdle(parallel_rdp->GetVkDevice());
    vkDestroyDescriptorPool(parallel_rdp->GetVkDevice(), vk_descriptor_pool, nullptr);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    // ImGui::Gui_ImplVulkanH_DestroyWindow(vulkan::GetInstance(), vulkan::GetDevice(), &vk_main_window_data,
    // vulkan::GetAllocator());
    SDL_DestroyWindow(sdl_window);
}

std::unique_ptr<VulkanRenderContext> VulkanRenderContext::Create(DrawGuiCallbackFunc draw_gui)
{
    if (!draw_gui) {
        // std::println("null gui draw function provided to vulkan render context");
        return {};
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        // std::println("Failed call to SDL_Init: {}", SDL_GetError());
        return {};
    }

    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    SDL_Window* sdl_window =
      SDL_CreateWindow("tessoe", 500, 500, SDL_WINDOW_VULKAN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_RESIZABLE);
    if (!sdl_window) {
        // std::println("Failed call to SDL_CreateWindow: {}", SDL_GetError());
        return {};
    }

    if (!n64::rdp::implementation) {
        Status status = n64::rdp::MakeParallelRdp();
        if (!status.Ok()) {
            return {};
        }
    }
    n64::rdp::ParallelRDPWrapper* const parallel_rdp =
      dynamic_cast<n64::rdp::ParallelRDPWrapper*>(n64::rdp::implementation.get());
    if (!parallel_rdp) {
        throw std::exception("Failed to create ParallelRDPWrapper object when creating vulkan context");
    }

    VkInstance vk_instance = parallel_rdp->GetVkInstance();
    VkPhysicalDevice vk_physical_device = parallel_rdp->GetVkPhysicalDevice();
    VkDevice vk_device = parallel_rdp->GetVkDevice();
    u32 vk_queue_family = parallel_rdp->GetVkQueueFamily();
    VkQueue vk_queue = parallel_rdp->GetVkQueue();
    VkCommandBuffer vk_command_buffer = parallel_rdp->GetVkCommandBuffer();

    VkDescriptorPool vk_descriptor_pool;
    VkRenderPass vk_render_pass;

    auto CheckVkResult = [](VkResult vk_result) {
        if (vk_result != 0) {
            LogError(std::format("[vulkan]: Error: VkResult = {}", std::to_underlying(vk_result)));
        }
    };

    VkResult vk_result;

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
        attachment.format = parallel_rdp->GetVkFormat();
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

    // ImGui_ImplVulkan_LoadFunctions([](char const* fn, void*) { return vkGetInstanceProcAddr(vk_instance, fn); });

    if (!ImGui_ImplSDL3_InitForVulkan(sdl_window)) {
        LogError("Failed call to ImGui_ImplSDL3_InitForVulkan");
        return {};
    }

    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance = parallel_rdp->GetVkInstance(),
        .PhysicalDevice = parallel_rdp->GetVkPhysicalDevice(),
        .Device = parallel_rdp->GetVkDevice(),
        .QueueFamily = parallel_rdp->GetVkQueueFamily(),
        .Queue = parallel_rdp->GetVkQueue(),
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

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        LogWarn("Failed call to ImGui_ImplVulkan_CreateFontsTexture");
    }

    parallel_rdp->SubmitRequestedVkCommandBuffer();

    return std::unique_ptr<VulkanRenderContext>(
      new VulkanRenderContext(sdl_window, draw_gui, parallel_rdp, vk_descriptor_pool));
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
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    UpdateFrameCounter();
    draw_gui();
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), parallel_rdp->GetVkCommandBuffer());
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

void VulkanRenderContext::SetWindowSize(uint width, uint height)
{
}
