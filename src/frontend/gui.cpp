#include "gui.hpp"
#include "audio.hpp"
#include "core.hpp"
#include "frontend/message.hpp"
#include "input.hpp"
#include "loader.hpp"
#include "log.hpp"
#include "serializer.hpp"
#include "vulkan.hpp"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "nfd.h"
#include "parallel-rdp-standalone/volk/volk.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace rng = std::ranges;

namespace frontend::gui {

static void Draw();
static void DrawGameSelectionWindow();
static void DrawInputBindingsWindow();
static void DrawMenu();
static void DrawRdpConfWindow();
static bool EnterFullscreen();
static bool ExitFullscreen();
static std::optional<fs::path> FileDialog();
static std::optional<fs::path> FolderDialog();
static Status InitGraphics();
static Status InitImgui();
static Status InitSdl();
static bool NeedsDraw();
static void OnExit();
static void OnGameSelected(size_t list_index);
static void OnInputBindingsWindowResetAll();
static void OnInputBindingsWindowSave();
static void OnInputBindingsWindowUseControllerDefaults();
static void OnInputBindingsWindowUseKeyboardDefaults();
static void OnMenuConfigureBindings();
static void OnMenuEnableAudio();
static void OnMenuFullscreen();
static void OnMenuLoadState();
static void OnMenuOpen();
static void OnMenuOpenBios();
static void OnMenuOpenRecent();
static void OnMenuPause();
static void OnMenuQuit();
static void OnMenuReset();
static void OnMenuSaveState();
static void OnMenuShowGameList();
static void OnMenuStop();
static void OnMenuWindowScale();
static void OnRdpWindowParallelRdpSelected();
static void OnSdlQuit();
static void OnSetGameDirectory();
static void OnWindowResizeEvent(SDL_Event const& event);
static Status ReadConfigFile();
static void RefreshGameList();
static void StartGame();
static void StopGame();
static void UpdateWindowTitle();
static void UseDefaultConfig();

static bool filter_game_list_to_n64_files;
static bool game_is_running;
static bool menu_enable_audio;
static bool menu_fullscreen;
static bool menu_pause_emulation;
static bool quit;
static bool show_game_selection_window;
static bool show_input_bindings_window;
static bool show_menu;
static bool show_rdp_conf_window;
static bool start_game;

static int frame_counter;
static int window_height, window_width;

static float fps;

static std::string current_game_title;
static fs::path game_list_directory;

struct GameListEntry {
    fs::path path;
    std::string name; // store file name with char value type so that ImGui::gui can display it
};

static std::vector<GameListEntry> game_list;

static SDL_Window* sdl_window;

void Draw()
{
    if (show_menu) {
        DrawMenu();
    }
    if (show_game_selection_window) {
        DrawGameSelectionWindow();
    }
    if (show_input_bindings_window) {
        DrawInputBindingsWindow();
    }
    if (show_rdp_conf_window) {
        DrawRdpConfWindow();
    }
}

void DrawGameSelectionWindow()
{
    if (ImGui::Begin("Game selection", &show_game_selection_window)) {
        if (ImGui::Button("Set game directory")) {
            OnSetGameDirectory();
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Filter to common n64 file types", &filter_game_list_to_n64_files)) {
            RefreshGameList();
        }
        if (game_list_directory.empty()) {
            ImGui::Text("No directory set!");
        } else {
            // TODO: see if conversion to char* which ImGui::Gui requires can be done in a better way.
            // On Windows: directories containing non-ASCII characters display incorrectly
            ImGui::Text(game_list_directory.string().c_str());
        }
        if (ImGui::BeginListBox("Game selection")) {
            static size_t item_current_idx = 0; // Here we store our selection data as an index.
            for (size_t n = 0; n < game_list.size(); ++n) {
                bool is_selected = item_current_idx == n;
                if (ImGui::Selectable(game_list[n].name.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    item_current_idx = n;
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        OnGameSelected(size_t(n));
                    }
                }
                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }
        ImGui::End();
    }
}

void DrawInputBindingsWindow()
{
    static constexpr std::string_view unbound_label = "Unbound";
    static constexpr std::string_view waiting_for_input_label = "...";

    struct Button {
        std::string_view const text_label;
        std::string_view button_label =
          unbound_label; // can be non-owning thanks to SDL's SDL_GameControllerGetStringForButton etc
        std::string_view prev_button_label = button_label;
        bool waiting_for_input = false;
    };

    static constinit std::array control_buttons = { Button{ "A" },
        Button{ "B" },
        Button{ "START" },
        Button{ "Z" },
        Button{ "Shoulder L" },
        Button{ "Shoulder R" },
        Button{ "D-pad up" },
        Button{ "D-pad down" },
        Button{ "D-pad left" },
        Button{ "D-pad right" },
        Button{ "C up" },
        Button{ "C down" },
        Button{ "C left" },
        Button{ "C right" },
        Button{ "Joy X" },
        Button{ "Joy Y" } };

    static constinit std::array hotkey_buttons = { Button{ "Load state" },
        Button{ "Save state" },
        Button{ "Toggle fullscreen" } };

    auto OnButtonPressed = [](Button& button) {
        if (button.waiting_for_input) {
            button.button_label = button.prev_button_label;
        } else {
            button.prev_button_label = button.button_label;
            button.button_label = waiting_for_input_label;
        }
        button.waiting_for_input = !button.waiting_for_input;
    };

    if (ImGui::Begin("Input configuration", &show_input_bindings_window)) {
        if (ImGui::Button("Reset all")) {
            OnInputBindingsWindowResetAll();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            OnInputBindingsWindowSave();
        }
        if (ImGui::Button("Use controller defaults")) {
            OnInputBindingsWindowUseControllerDefaults();
        }
        ImGui::SameLine();
        if (ImGui::Button("Use keyboard defaults")) {
            OnInputBindingsWindowUseKeyboardDefaults();
        }

        ImGui::Separator();

        static constexpr size_t num_horizontal_elements = std::max(control_buttons.size(), hotkey_buttons.size());
        for (size_t i = 0; i < num_horizontal_elements; ++i) {
            if (i < control_buttons.size()) {
                Button& button = control_buttons[i];
                ImGui::Text(button.text_label.data());
                ImGui::SameLine(100);
                if (ImGui::Button(button.button_label.data())) {
                    OnButtonPressed(button);
                }
            }
            if (i < hotkey_buttons.size()) {
                Button& button = control_buttons[i];
                ImGui::SameLine();
                ImGui::Text(button.text_label.data());
                ImGui::SameLine(250);
                if (ImGui::Button(button.button_label.data())) {
                    OnButtonPressed(button);
                }
            }
        }

        ImGui::End();
    }
}

void DrawMenu()
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                OnMenuOpen();
            }
            if (ImGui::MenuItem("Open recent")) {
                OnMenuOpenRecent();
            }
            if (ImGui::MenuItem("Open BIOS")) {
                OnMenuOpenBios();
            }
            if (ImGui::MenuItem(show_game_selection_window ? "Hide game list" : "Show game list")) {
                OnMenuShowGameList();
            }
            if (ImGui::MenuItem("Load state", "Ctrl+L")) {
                OnMenuLoadState();
            }
            if (ImGui::MenuItem("Save state", "Ctrl+S")) {
                OnMenuSaveState();
            }
            if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
                OnMenuQuit();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation")) {
            if (ImGui::MenuItem("Pause", "Ctrl+P", &menu_pause_emulation, true)) {
                OnMenuPause();
            }
            if (ImGui::MenuItem("Reset", "Ctrl+R")) {
                OnMenuReset();
            }
            if (ImGui::MenuItem("Stop", "Ctrl+X")) {
                OnMenuStop();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Audio")) {
            if (ImGui::MenuItem("Enable", "Ctrl+A", &menu_enable_audio, true)) {
                OnMenuEnableAudio();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Video")) {
            if (ImGui::MenuItem("Set window scale")) {
                OnMenuWindowScale();
            }
            if (ImGui::MenuItem("Fullscreen", "Ctrl+Enter", &menu_fullscreen, true)) {
                OnMenuFullscreen();
            }
            ImGui::MenuItem("RDP settings", nullptr, &show_rdp_conf_window, true);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Input")) {
            if (ImGui::MenuItem("Configure bindings")) {
                OnMenuConfigureBindings();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            // TODO
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void DrawRdpConfWindow()
{
    if (ImGui::Begin("RDP configuration", &show_rdp_conf_window)) {
        ImGui::Text("Implementation");
        {
            static int e = 0;
            if (ImGui::RadioButton("Parallel RDP (Vulkan)", &e, 0)) {
                OnRdpWindowParallelRdpSelected();
            }
        }
        ImGui::Separator();
        // TODO: parallel-rdp-specific settings
    }
    ImGui::End();
}

bool EnterFullscreen()
{
    /*if (SDL_SetWindowFullscreen(sdl_window, true)) {
        message::error(std::format("Failed to enter fullscreen: {}", SDL_GetError()));
        return false;
    }*/
    return true;
}

bool ExitFullscreen()
{
    // SDL_SetWindowFullscreen(sdl_window, 0);
    return true;
}

std::optional<fs::path> FileDialog()
{
    nfdnchar_t* path{};
    nfdresult_t result = NFD_OpenDialogN(&path, nullptr, 0, fs::current_path().c_str());
    if (result == NFD_OKAY) {
        fs::path fs_path{ path };
        NFD_FreePathN(path);
        return fs_path;
    } else if (result == NFD_ERROR) {
        message::error("nativefiledialog returned NFD_ERROR for NFD_OpenDialogN");
    }
    return {};
}

std::optional<fs::path> FolderDialog()
{
    nfdnchar_t* path{};
    nfdresult_t result = NFD_PickFolderN(&path, fs::current_path().c_str());
    if (result == NFD_OKAY) {
        fs::path fs_path{ path };
        NFD_FreePathN(path);
        return fs_path;
    } else if (result == NFD_ERROR) {
        message::error("nativefiledialog returned NFD_ERROR for NFD_PickFolderN");
    }
    return {};
}

void FrameVulkan(VkCommandBuffer vk_command_buffer)
{
    PollEvents();
    if (NeedsDraw()) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        Draw();
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk_command_buffer);
    }
    if (++frame_counter == 60) {
        static std::chrono::time_point time = std::chrono::steady_clock::now();
        auto microsecs_to_render_60_frames =
          std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - time).count();
        fps = 60.0f * 1'000'000.0f / float(microsecs_to_render_60_frames);
        UpdateWindowTitle();
        frame_counter = 0;
        time = std::chrono::steady_clock::now();
    }
}

SDL_Window* GetSdlWindow()
{
    if (!sdl_window) {
        Status status = InitSdl();
        if (!status.ok()) {
            log_fatal(std::format("Failed to init SDL; {}", status.message()));
            exit(1);
        }
    }
    return sdl_window;
}

void GetWindowSize(int* w, int* h)
{
    SDL_GetWindowSize(sdl_window, w, h);
}

Status Init()
{
    window_width = 640, window_height = 480;

    game_is_running = false;
    menu_enable_audio = true;
    menu_fullscreen = false;
    menu_pause_emulation = false;
    quit = false;
    show_input_bindings_window = false;
    show_menu = true;
    show_rdp_conf_window = false;
    start_game = false;

    show_game_selection_window = !game_is_running;

    Status status{ Status::Code::Ok };
    if (status = ReadConfigFile(); !status.ok()) {
        log_error(status.message());
        log_info("Using default configuration.");
        UseDefaultConfig();
    }
    if (status = InitSdl(); !status.ok()) {
        return status;
    }
    if (status = InitGraphics(); !status.ok()) {
        return status;
    }
    if (status = InitImgui(); !status.ok()) {
        return status;
    }
    if (status = audio::init(); !status.ok()) {
        message::error(std::format("Failed to init audio system; {}", status.message()));
    }
    if (status = input::Init(); !status.ok()) {
        message::error("Failed to init input system!");
    }
    if (nfdresult_t result = NFD_Init(); result != NFD_OKAY) {
        message::error(
          std::format("Failed to init nativefiledialog; NFD_Init returned {}", std::to_underlying(result)));
    }
    UpdateWindowTitle();

    return status_ok();
}

Status InitGraphics()
{
    if (core_loaded()) {
        return get_core()->init_graphics_system();
    } else {
        // TODO: temporary solution to get vulkan rendering while n64 core is not supposed to be running
        Status status = load_core(System::N64);
        if (!status.ok()) {
            return status;
        }
    }
    return get_core()->init_graphics_system();
}

Status InitImgui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImGui_ImplVulkan_LoadFunctions(
      [](char const* fn, void*) { return vkGetInstanceProcAddr(vulkan::GetInstance(), fn); });

    if (!ImGui_ImplSDL3_InitForVulkan(sdl_window)) {
        return status_failure("ImGui_ImplSDL3_InitForVulkan failed");
    }

    ImGui_ImplVulkan_InitInfo init_info = { .Instance = vulkan::GetInstance(),
        .PhysicalDevice = vulkan::GetPhysicalDevice(),
        .Device = vulkan::GetDevice(),
        .QueueFamily = vulkan::GetGraphicsQueueFamily(),
        .Queue = vulkan::GetQueue(),
        .PipelineCache = vulkan::GetPipelineCache(),
        .DescriptorPool = vulkan::GetDescriptorPool(),
        .MinImageCount = 2,
        .ImageCount = 2,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .Allocator = vulkan::GetAllocator(),
        .CheckVkResultFn = vulkan::CheckVkResult };

    if (!ImGui_ImplVulkan_Init(&init_info, vulkan::GetRenderPass())) {
        return status_failure("ImGui_ImplVulkan_Init failed");
    }

    io.Fonts->AddFontDefault();
    ImGui_ImplVulkan_CreateFontsTexture(vulkan::GetCommandBuffer());
    vulkan::SubmitRequestedCommandBuffer();

    return status_ok();
}

Status InitSdl()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        return status_failure(std::format("Failed to init SDL: {}\n", SDL_GetError()));
    }
    sdl_window = SDL_CreateWindow("N63.5",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      window_width,
      window_height,
      SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    if (!sdl_window) {
        return status_failure(std::format("Failed to create SDL window: {}\n", SDL_GetError()));
    }
    if (Status status = message::init(sdl_window); !status.ok()) {
        log_error(std::format("Failed to initialize user message system; {}", status.message()));
    }
    return status_ok();
}

Status LoadGame(std::filesystem::path const& path)
{
    Status status = get_core()->load_rom(path);
    if (status.ok()) {
        if (game_is_running) {
            StopGame();
        }
        start_game = true;
        current_game_title = path.filename().string();
        game_list_directory = path.parent_path();
        RefreshGameList();
    }
    return status;
}

bool NeedsDraw()
{
    return show_menu || show_input_bindings_window || show_game_selection_window || show_rdp_conf_window;
}

void OnCtrlKeyPress(SDL_Keycode keycode)
{
    switch (keycode) {
    case SDLK_a:
        menu_enable_audio = !menu_enable_audio;
        OnMenuEnableAudio();
        break;

    case SDLK_l: OnMenuLoadState(); break;

    case SDLK_m: show_menu = !show_menu; break;

    case SDLK_o: OnMenuOpen(); break;

    case SDLK_p:
        menu_pause_emulation = !menu_pause_emulation;
        OnMenuPause();
        break;

    case SDLK_q: OnMenuQuit(); break;

    case SDLK_r: OnMenuReset(); break;

    case SDLK_RETURN:
        menu_fullscreen = !menu_fullscreen;
        OnMenuFullscreen();
        break;

    case SDLK_s: OnMenuSaveState(); break;

    case SDLK_x: OnMenuStop(); break;
    }
}

void OnExit()
{
    StopGame();

    VkResult vk_result = vkDeviceWaitIdle(vulkan::GetDevice());
    vulkan::CheckVkResult(vk_result);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    // ImGui::Gui_ImplVulkanH_DestroyWindow(vulkan::GetInstance(), vulkan::GetDevice(), &vk_main_window_data,
    // vulkan::GetAllocator());
    vulkan::TearDown();

    NFD_Quit();
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
}

void OnGameSelected(size_t list_index)
{
    StopGame();
    /*if (get_core()->load_rom(game_list[list_index].path).ok()) {
        current_game_title = game_list[list_index].name;
        start_game = true;
    } else {
        message::error("Failed to load game!");
    }*/
}

void OnInputBindingsWindowResetAll()
{
    // TODO
}

void OnInputBindingsWindowSave()
{
    Status status = input::SaveBindingsToDisk();
    if (!status.ok()) {
        message::error(status.message());
    }
}

void OnInputBindingsWindowUseControllerDefaults()
{
    // TODO
}

void OnInputBindingsWindowUseKeyboardDefaults()
{
    // TODO
}

void OnMenuConfigureBindings()
{
    show_input_bindings_window = !show_input_bindings_window;
}

void OnMenuEnableAudio()
{
    // menu_enable_audio ? audio::Enable() : audio::Disable();
}

void OnMenuFullscreen()
{
    bool success = menu_fullscreen ? EnterFullscreen() : ExitFullscreen();
    if (!success) {
        menu_fullscreen = !menu_fullscreen; // revert change
    }
}

void OnMenuLoadState()
{
}

void OnMenuOpen()
{
    std::optional<fs::path> path = FileDialog();
    if (path) {
        Status status = load_core_and_game(path.value());
        if (!status.ok()) {
            message::error(status.message());
        }
    }
}

void OnMenuOpenBios()
{
    std::optional<fs::path> path = FileDialog();
    if (path.has_value()) {
        fs::path path_val = std::move(path.value());
        // if (!N64::LoadBios(path_val)) {
        //     message::error(std::format("Could not load bios at path \"{}\"", path_val.string()));
        // }
    }
}

void OnMenuOpenRecent()
{
    // TODO
}

void OnMenuPause()
{
    if (core_loaded()) {
        menu_pause_emulation ? get_core()->pause() : get_core()->resume();
    }
}

void OnMenuQuit()
{
    OnSdlQuit();
}

void OnMenuReset()
{
    if (core_loaded()) {
        get_core()->reset();
    }
}

void OnMenuSaveState()
{
}

void OnMenuShowGameList()
{
    show_game_selection_window = !show_game_selection_window;
}

void OnMenuStop()
{
    StopGame();
}

void OnMenuWindowScale()
{
    // TODO
}

void OnRdpWindowParallelRdpSelected()
{
    // TODO
}

void OnSdlQuit()
{
    quit = true;
    if (core_loaded()) {
        get_core()->stop();
    }
}

void OnSetGameDirectory()
{
    std::optional<fs::path> dir = FolderDialog();
    if (dir.has_value()) {
        game_list_directory = std::move(dir.value());
        RefreshGameList();
    }
}

void OnWindowResizeEvent(SDL_Event const& event)
{
}

void PollEvents()
{
    static SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        switch (event.type) {
        case SDL_EVENT_AUDIO_DEVICE_ADDED: audio::on_device_added(event); break;
        case SDL_EVENT_AUDIO_DEVICE_REMOVED: audio::on_device_removed(event); break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: input::OnControllerAxisMotion(event); break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: input::OnControllerButtonDown(event); break;
        case SDL_EVENT_GAMEPAD_BUTTON_UP: input::OnControllerButtonUp(event); break;
        case SDL_EVENT_GAMEPAD_ADDED: input::OnControllerDeviceAdded(event); break;
        case SDL_EVENT_GAMEPAD_REMOVED: input::OnControllerDeviceRemoved(event); break;
        case SDL_EVENT_KEY_DOWN: input::OnKeyDown(event); break;
        case SDL_EVENT_KEY_UP: input::OnKeyUp(event); break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: input::OnMouseButtonDown(event); break;
        case SDL_EVENT_MOUSE_BUTTON_UP: input::OnMouseButtonUp(event); break;
        case SDL_EVENT_QUIT: OnSdlQuit(); break;
        case SDL_EVENT_WINDOW_RESIZED: OnWindowResizeEvent(event); break;
        }
    }
}

Status ReadConfigFile()
{
    return status_unimplemented(); // TODO
}

void RefreshGameList()
{
    game_list.clear();
    if (fs::exists(game_list_directory)) {
        for (fs::directory_entry const& entry : fs::directory_iterator(game_list_directory)) {
            if (entry.is_regular_file()) {
                auto IsKnownExt = [](fs::path const& ext) {
                    static const std::array<fs::path, 8>
                      n64_rom_exts = { ".n64", ".N64", ".v64", ".V64", ".z64", ".Z64", ".zip", ".7z" };
                    return std::find_if(n64_rom_exts.begin(), n64_rom_exts.end(), [&ext](fs::path const& known_ext) {
                        return ext.compare(known_ext) == 0;
                    }) != n64_rom_exts.end();
                };
                if (!filter_game_list_to_n64_files || IsKnownExt(entry.path().filename().extension())) {
                    game_list.push_back(GameListEntry{ entry.path(), entry.path().filename().string() });
                }
            }
        }
    }
}

void Run(bool boot_game_immediately)
{
    if (boot_game_immediately) {
        start_game = true;
    }
    while (!quit) {
        if (start_game) {
            StartGame();
        } else {
            PollEvents();
            if (core_loaded()) {
                get_core()->update_screen();
            } else {
                vulkan::UpdateScreenNoCore();
            }
        }
    }
    OnExit();
}

void StartGame()
{
    game_is_running = true;
    start_game = false;
    show_game_selection_window = false;
    UpdateWindowTitle();
    get_core()->reset();
    get_core()->run();
}

void StopGame()
{
    if (core_loaded()) {
        get_core()->stop();
    }
    game_is_running = false;
    UpdateWindowTitle();
    show_game_selection_window = true;
    // TODO: show nice n64 background on window
}

void UpdateWindowTitle()
{
    if (game_is_running) {
        std::string title = std::format("teesoe | {} | FPS: {}", current_game_title, fps); // TODO: current core
        SDL_SetWindowTitle(sdl_window, title.c_str());
    } else {
        SDL_SetWindowTitle(sdl_window, "teesoe");
    }
}

void UseDefaultConfig()
{
    // TODO
}

} // namespace frontend::gui
