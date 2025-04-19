#pragma once

#include "numtypes.hpp"
#include "render_context.hpp"
#include "SDL3/SDL.h"

#include <chrono>
#include <memory>

class SdlRenderContext : public RenderContext {
public:
    ~SdlRenderContext() override;

    static std::unique_ptr<SdlRenderContext> Create(UpdateGuiCallback update_gui);

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
    void SetPixelFormat(PixelFormat format) override;
    void SetWindowSize(uint width, uint height) override;

private:
    SdlRenderContext(SDL_Renderer* sdl_renderer, SDL_Window* sdl_window, UpdateGuiCallback update_gui);

    void EvaluateWindowProperties();
    void RecreateTexture();
    void UpdateWindowsFpsLabel(f32 new_fps);

    struct Framebuffer {
        u8 const* ptr;
        uint width, height, pitch;
        uint bytes_per_pixel;
        SDL_PixelFormat pixel_format;
    } framebuffer{};

    struct Window {
        uint width, height; /* the dimensions of the sdl window */
        uint game_width, game_height; /* the dimensions of the game render area */
        uint game_offset_x, game_offset_y; /* offset of game render area on window */
        uint game_inner_render_offset_x, game_inner_render_offset_y;
        uint scale; /* scale of game render area in relation to the base core resolution. */
    } window{};

    bool rendering_is_enabled{};

    uint frame_counter{};

    SDL_FRect dst_rect{};
    SDL_Renderer* sdl_renderer{};
    SDL_Texture* sdl_texture{};

    std::chrono::time_point<std::chrono::steady_clock> time_now = std::chrono::steady_clock::now();
};
