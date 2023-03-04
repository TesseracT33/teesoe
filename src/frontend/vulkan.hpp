#pragma once

#include "types.hpp"

#include "imgui_impl_vulkan.h"
#include "SDL.h"
#include "SDL_vulkan.h"

namespace vulkan {

void CheckVkResult(VkResult vk_result);
void FramePresent(ImGui_ImplVulkanH_Window* wd);
void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data);
VkAllocationCallbacks* GetAllocator();
VkCommandBuffer GetCommandBuffer();
VkDescriptorPool GetDescriptorPool();
VkDevice GetDevice();
VkFormat GetFormat();
u32 GetGraphicsQueueFamily();
VkInstance GetInstance();
VkPhysicalDevice GetPhysicalDevice();
VkPipelineCache GetPipelineCache();
VkQueue GetQueue();
VkRenderPass GetRenderPass();
bool Init(SDL_Window* sdl_window);
bool InitForParallelRDP();
bool InitGeneric(SDL_Window* sdl_window);
void SubmitRequestedCommandBuffer();
void TearDown();

} // namespace vulkan
