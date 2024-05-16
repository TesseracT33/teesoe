#pragma once

#include "numtypes.hpp"

#include <chrono>

struct SDL_Window;

class RenderContext {
public:
    using UpdateGuiCallback = void (*)();

    enum class PixelFormat {
        ABGR8888,
        INDEX1LSB,
        BGR888,
        RGB888,
        RGBA8888,
    };

    RenderContext(SDL_Window* sdl_window, UpdateGuiCallback update_gui);
    virtual ~RenderContext() = default;

    virtual void EnableFullscreen(bool enable) = 0;
    virtual void EnableRendering(bool enable) = 0;
    virtual void NotifyNewGameFrameReady() = 0;
    virtual void Render() = 0;
    virtual void SetFramebufferHeight(uint height) = 0;
    virtual void SetFramebufferPtr(u8 const* ptr) = 0;
    virtual void SetFramebufferSize(uint width, uint height) = 0;
    virtual void SetFramebufferWidth(uint width) = 0;
    virtual void SetGameRenderAreaOffsetX(uint offset) = 0;
    virtual void SetGameRenderAreaOffsetY(uint offset) = 0;
    virtual void SetGameRenderAreaSize(uint width, uint height) = 0;
    virtual void SetPixelFormat(PixelFormat pixel_format) = 0;
    virtual void SetWindowSize(uint width, uint height) = 0;

    SDL_Window* GetWindow();
    void UpdateFrameCounter();

protected:
    SDL_Window* sdl_window{};
    UpdateGuiCallback update_gui;
    int frame_counter;
};
