#pragma once

#include "vulkan_headers.hpp"

#include "n64/rdp/parallel_rdp_wrapper.hpp"
#include "numtypes.hpp"
#include "render_context.hpp"
#include "SDL_vulkan.h"
#include "status.hpp"

#include <memory>

class VulkanRenderContext : public RenderContext {
public:
    ~VulkanRenderContext() override;

    static std::unique_ptr<VulkanRenderContext> Create(UpdateGuiCallback update_gui);

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
    void SetPixelFormat(PixelFormat pixel_format) override;
    void SetWindowSize(uint width, uint height) override;

    void Render(VkCommandBuffer vk_command_buffer);

private:
    VulkanRenderContext(SDL_Window* sdl_window,
      UpdateGuiCallback update_gui,
      std::unique_ptr<n64::rdp::ParallelRdpWrapper> parallel_rdp,
      VkDescriptorPool vk_descriptor_pool);

    std::unique_ptr<n64::rdp::ParallelRdpWrapper> parallel_rdp_;
    VkDescriptorPool vk_descriptor_pool_;
};
