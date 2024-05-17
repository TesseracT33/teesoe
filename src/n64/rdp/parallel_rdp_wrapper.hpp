#pragma once

#include "vulkan_headers.hpp"

#include "numtypes.hpp"
#include "parallel-rdp/rdp_device.hpp"
#include "rdp_implementation.hpp"
#include "SDL_vulkan.h"
#include "status.hpp"
#include "volk/volk.h"
#include "vulkan/wsi.hpp"

#include <array>
#include <functional>
#include <vector>

/* https://github.com/Themaister/parallel-rdp/blob/master/integration_example.cpp */

namespace n64::rdp {

class ParallelRdpWrapper final : public RdpImplementation {
public:
    using RenderCallback = std::function<void(VkCommandBuffer)>;

    static std::unique_ptr<ParallelRdpWrapper> Create(SDL_Window* sdl_window);

    ~ParallelRdpWrapper() override;

    void EnqueueCommand(int cmd_len, u32* cmd_ptr) override;
    void OnFullSync() override;
    void UpdateScreen() override;

    VkCommandBuffer GetVkCommandBuffer();
    VkDevice GetVkDevice();
    VkFormat GetVkFormat();
    u32 GetVkQueueFamily();
    VkInstance GetVkInstance();
    VkPhysicalDevice GetVkPhysicalDevice();
    VkQueue GetVkQueue();
    void SetRenderCallback(RenderCallback render);
    void SubmitRequestedVkCommandBuffer();

private:
    struct SDLWSIPlatform final : public Vulkan::WSIPlatform {
        SDLWSIPlatform(SDL_Window* sdl_window);
        bool alive(Vulkan::WSI& wsi) override;
        VkSurfaceKHR create_surface(VkInstance instance, VkPhysicalDevice gpu) override;
        std::vector<char const*> get_instance_extensions() override;
        void event_frame_tick(double frame, double elapsed) override;
        VkApplicationInfo const* get_application_info() override;
        u32 get_surface_height() override;
        u32 get_surface_width() override;
        void poll_input() override;
        SDL_Window* const sdl_window;
    };

    ParallelRdpWrapper(std::unique_ptr<SDLWSIPlatform> sdl_wsi_platform,
      std::unique_ptr<Vulkan::WSI> wsi,
      std::unique_ptr<::RDP::CommandProcessor> cmd_processor);

    friend struct SDLWSIPlatform;

    void ReloadViRegisters();

    RenderCallback render_;
    std::unique_ptr<SDLWSIPlatform> sdl_wsi_platform_;
    std::unique_ptr<Vulkan::WSI> wsi_;
    std::unique_ptr<::RDP::CommandProcessor> cmd_processor_;
    Vulkan::CommandBufferHandle requested_command_buffer_;
};

} // namespace n64::rdp
