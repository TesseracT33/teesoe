#include "sdl_render_context.hpp"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <print>

SdlRenderContext::SdlRenderContext(SDL_Renderer* sdl_renderer, SDL_Window* sdl_window, UpdateGuiCallback draw_gui)
  : RenderContext(sdl_window, draw_gui),
    sdl_renderer(sdl_renderer)
{
}

SdlRenderContext::~SdlRenderContext()
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(sdl_window);
}

std::unique_ptr<SdlRenderContext> SdlRenderContext::Create(UpdateGuiCallback update_gui)
{
    if (!update_gui) {
        std::println("null gui update callbackprovided to SDL render context");
        return {};
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::println("Failed call to SDL_Init: {}", SDL_GetError());
        return {};
    }

    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    SDL_Window* sdl_window =
      SDL_CreateWindow("teesoe", 1280, 960, SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_RESIZABLE);
    if (!sdl_window) {
        std::println("Failed call to SDL_CreateWindow: {}", SDL_GetError());
        return {};
    }

    SDL_Renderer* sdl_renderer = SDL_CreateRenderer(sdl_window, 0, SDL_RENDERER_ACCELERATED);
    if (!sdl_renderer) {
        std::println("Failed call to SDL_CreateRenderer: {}", SDL_GetError());
        return {};
    }

    IMGUI_CHECKVERSION();
    (void)ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForSDLRenderer(sdl_window, sdl_renderer)) {
        std::println("Failed call to ImGui_ImplSDL3_InitForSDLRenderer: {}", SDL_GetError());
        return {};
    }

    if (!ImGui_ImplSDLRenderer3_Init(sdl_renderer)) {
        std::println("Failed call to ImGui_ImplSDLRenderer3_Init: {}", SDL_GetError());
        return {};
    }

    return std::unique_ptr<SdlRenderContext>(new SdlRenderContext(sdl_renderer, sdl_window, update_gui));
}

void SdlRenderContext::EnableFullscreen(bool enable)
{
    if (enable) {
        SDL_SetWindowFullscreen(sdl_window, SDL_TRUE);
        SDL_DisplayMode const* display_mode = SDL_GetCurrentDisplayMode(0);
        window.width = window.game_width = display_mode->w;
        window.height = window.game_height = display_mode->h;
        EvaluateWindowProperties();
    } else {
        // TODO
        SDL_SetWindowFullscreen(sdl_window, SDL_FALSE);
    }
}

void SdlRenderContext::EnableRendering(bool enable)
{
    rendering_is_enabled = enable;
}

void SdlRenderContext::EvaluateWindowProperties()
{
    if (framebuffer.width != 0 && framebuffer.height != 0) {
        window.scale = std::min(window.game_width / framebuffer.width, window.game_height / framebuffer.height);
    } else {
        window.scale = 0;
    }
    window.game_inner_render_offset_x = (window.game_width - window.scale * framebuffer.width) / 2;
    window.game_inner_render_offset_y = (window.game_height - window.scale * framebuffer.height) / 2;
    dst_rect.w = f32(window.scale * framebuffer.width);
    dst_rect.h = f32(window.scale * framebuffer.height);
    dst_rect.x = f32(window.game_offset_x + window.game_inner_render_offset_x);
    dst_rect.y = f32(window.game_offset_y + window.game_inner_render_offset_y);
}

void SdlRenderContext::NotifyNewGameFrameReady()
{
    if (++frame_counter == 60) {
        auto microsecs_to_render_60_frames =
          std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - time_now).count();
        f32 fps = 60.0f * 1'000'000.0f / f32(microsecs_to_render_60_frames);
        UpdateWindowsFpsLabel(fps);
        time_now = std::chrono::steady_clock::now();
        frame_counter = 0;
    }
}

void SdlRenderContext::RecreateTexture()
{
    SDL_DestroyTexture(sdl_texture);
    sdl_texture = SDL_CreateTexture(sdl_renderer,
      framebuffer.pixel_format,
      SDL_TEXTUREACCESS_STREAMING,
      framebuffer.width,
      framebuffer.height);
}

void SdlRenderContext::Render()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    update_gui();
    ImGui::Render();

    SDL_RenderClear(sdl_renderer);

    if (rendering_is_enabled) {
        void* locked_pixels;
        int locked_pixels_pitch;
        SDL_LockTexture(sdl_texture, nullptr, &locked_pixels, &locked_pixels_pitch);

        SDL_ConvertPixels(framebuffer.width, // framebuffer width
          framebuffer.height, // framebuffer height
          framebuffer.pixel_format, // source format
          framebuffer.ptr, // source
          framebuffer.pitch, // source pitch
          framebuffer.pixel_format, // destination format
          locked_pixels, // destination
          framebuffer.pitch // destination pitch
        );

        SDL_UnlockTexture(sdl_texture);
        SDL_RenderTexture(sdl_renderer, sdl_texture, nullptr, &dst_rect);
    }

    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(sdl_renderer);

    UpdateFrameCounter();
}

void SdlRenderContext::SetFramebufferHeight(uint height)
{
    framebuffer.height = height;
    EvaluateWindowProperties();
    RecreateTexture();
}

void SdlRenderContext::SetFramebufferPtr(u8 const* ptr)
{
    if (!ptr) {
        throw std::invalid_argument("Framebuffer pointer given to SdlRenderContext was null!");
    }
    framebuffer.ptr = ptr;
}

void SdlRenderContext::SetFramebufferSize(uint width, uint height)
{
    framebuffer.width = width;
    framebuffer.height = height;
    framebuffer.pitch = width * framebuffer.bytes_per_pixel;
    EvaluateWindowProperties();
    RecreateTexture();
}

void SdlRenderContext::SetFramebufferWidth(uint width)
{
    framebuffer.width = width;
    framebuffer.pitch = width * framebuffer.bytes_per_pixel;
    EvaluateWindowProperties();
    RecreateTexture();
}

void SdlRenderContext::SetGameRenderAreaOffsetX(uint offset)
{
    window.game_offset_x = offset;
    EvaluateWindowProperties();
}

void SdlRenderContext::SetGameRenderAreaOffsetY(uint offset)
{
    window.game_offset_y = offset;
    EvaluateWindowProperties();
}

void SdlRenderContext::SetGameRenderAreaSize(uint width, uint height)
{
    window.game_width = width;
    window.game_height = height;
    EvaluateWindowProperties();
}

void SdlRenderContext::SetPixelFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::ABGR8888:
        framebuffer.bytes_per_pixel = 4;
        framebuffer.pixel_format = SDL_PIXELFORMAT_ABGR8888;
        break;
    case PixelFormat::BGR888:
        framebuffer.bytes_per_pixel = 3;
        framebuffer.pixel_format = SDL_PIXELFORMAT_BGR24;
        break;
    case PixelFormat::RGB888:
        framebuffer.bytes_per_pixel = 3;
        framebuffer.pixel_format = SDL_PIXELFORMAT_RGB24;
        break;
    case PixelFormat::RGBA8888:
        framebuffer.bytes_per_pixel = 4;
        framebuffer.pixel_format = SDL_PIXELFORMAT_RGBA8888;
        break;
    default: throw std::invalid_argument("Unrecognized PixelFormat supplied");
    }
    framebuffer.pitch = framebuffer.width * framebuffer.bytes_per_pixel;
    RecreateTexture();
}

void SdlRenderContext::SetWindowSize(uint width, uint height)
{
    window.width = width;
    window.height = height;
    EvaluateWindowProperties();
}

void SdlRenderContext::UpdateWindowsFpsLabel(f32 new_fps)
{
    SDL_SetWindowTitle(sdl_window, std::format("FPS: {}", new_fps).data());
}
