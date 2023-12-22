#pragma once

#include "vulkan_headers.hpp"

#include "imgui_impl_vulkan.h"
#include "n64/rdp/parallel_rdp_wrapper.hpp"
#include "render_context.hpp"
#include "SDL.h"
#include "SDL_vulkan.h"
#include "status.hpp"
#include "types.hpp"

#include <memory>

class VulkanRenderContext : public RenderContext {
public:
    ~VulkanRenderContext();

    static std::unique_ptr<VulkanRenderContext> Create(DrawGuiCallbackFunc draw_gui);

    void EnableFullscreen(bool enable) override;
    void EnableRendering(bool enable) override;
    void NotifyNewGameFrameReady() override;
    void Render() override;
    void SetFramebufferHeight(uint height) override;
    void SetFramebufferPtr(u8 const* ptr) override;
    void SetFramebufferSize(uint width, uint height) override;
    void SetFramebufferWidth(uint width) override;
    void SetGameRenderAreaOffsetX(uint offset) override;
    void SetGameRenderAreaOffsetY(uint offset) override;
    void SetGameRenderAreaSize(uint width, uint height) override;
    void SetWindowSize(uint width, uint height) override;

private:
    VulkanRenderContext(SDL_Window* sdl_window,
      DrawGuiCallbackFunc draw_gui,
      n64::rdp::ParallelRDPWrapper* parallel_rdp,
      VkDescriptorPool vk_descriptor_pool);

    n64::rdp::ParallelRDPWrapper* parallel_rdp;
    VkDescriptorPool vk_descriptor_pool;
};
