#include "gui.hpp"
#include "audio.hpp"
#include "config.hpp"
#include "core_configuration.hpp"
#include "frontend/message.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "input.hpp"
#include "loader.hpp"
#include "log.hpp"
#include "nfd.h"
#include "parallel-rdp-standalone/volk/volk.h"
#include "serializer.hpp"
#include "vulkan.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace rng = std::ranges;

namespace frontend::gui {

struct GameListEntry {
    fs::path path;
    std::string name; // store file name with char value type so that ImGui::gui can display it
};

struct GameList {
    std::vector<GameListEntry> games;
    fs::path directory;
    bool apply_filter;
};

static void Draw();
static void DrawCoreSettingsWindow();
static void DrawGameSelectionWindow();
static void DrawInputBindingsWindow();
static void DrawMenu();
static Status EnableFullscreen(bool enable);
static std::optional<fs::path> FileDialog();
static std::optional<fs::path> FolderDialog();
static Status InitGraphics();
static Status InitImgui();
static Status InitSdl();
static bool NeedsDraw();
static void OnExit();
static void OnGameSelected(System system, size_t list_index);
static void OnInputBindingsWindowResetAll();
static void OnInputBindingsWindowSave();
static void OnInputBindingsWindowUseControllerDefaults();
static void OnInputBindingsWindowUseKeyboardDefaults();
static void OnMenuConfigureBindings();
static void OnMenuCoreSettings();
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
static void OnSdlQuit();
static void OnWindowResizeEvent(SDL_Event const& event);
static Status ReadConfig();
static void RefreshGameList(System system);
static void StartGame();
static void StopGame();
static void UpdateWindowTitle(float fps = 0.f);
static void UseDefaultConfig();

static bool game_is_running;
static bool menu_enable_audio;
static bool menu_fullscreen;
static bool menu_pause_emulation;
static bool quit;
static bool show_game_selection_window;
static bool show_input_bindings_window;
static bool show_core_settings_window;
static bool show_menu;
static bool start_game;

static int frame_counter;
static int window_height, window_width;

static fs::path exe_path;

static std::string current_game_title;

static std::unordered_map<System, GameList> game_lists;

static SDL_Window* sdl_window;

static CoreConfiguration n64_configuration;

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
    if (show_core_settings_window) {
        DrawCoreSettingsWindow();
    }
}

void DrawCoreSettingsWindow()
{
    if (ImGui::Begin("Core settings", &show_core_settings_window)) {
        static int tab{};
        if (ImGui::Button("GBA", ImVec2(100.0f, 0.0f))) tab = 0;
        ImGui::SameLine(0.0, 2.0f);
        if (ImGui::Button("N64", ImVec2(100.0f, 0.0f))) tab = 1;

        auto DrawGBA = [] {

        };

        auto DrawN64 = [] {
            static int cpu_impl_sel{};
            ImGui::Text("CPU implementation");
            if (ImGui::RadioButton("Interpreter##cpu", &cpu_impl_sel, 0)) {
                n64_configuration.n64.use_cpu_recompiler = false;
            }
            if (ImGui::RadioButton("Recompiler##cpu", &cpu_impl_sel, 1)) {
                n64_configuration.n64.use_cpu_recompiler = true;
            }

            static int rsp_impl_sel{};
            ImGui::Text("RSP implementation");
            if (ImGui::RadioButton("Interpreter##rsp", &rsp_impl_sel, 0)) {
                n64_configuration.n64.use_rsp_recompiler = false;
            }
            if (ImGui::RadioButton("Recompiler##rsp", &rsp_impl_sel, 1)) {
                n64_configuration.n64.use_rsp_recompiler = true;
            }
        };

        switch (tab) {
        case 0: DrawGBA(); break;
        case 1: DrawN64(); break;
        }

        if (ImGui::Button("Apply and save")) {
            if (game_is_running && GetSystem() == System::N64) {
                GetCore()->ApplyConfig(n64_configuration);
                // TODO: save to file
                n64_configuration = {};
            }
            show_core_settings_window = !show_core_settings_window;
        }

        ImGui::End();
    }
}

void DrawGameSelectionWindow()
{
    auto DrawCoreList = [](System system) {
        GameList& game_list = game_lists[system];
        if (ImGui::Button("Set game directory")) {
            std::optional<fs::path> dir = FolderDialog();
            if (dir) {
                game_list.directory = std::move(dir.value());
                RefreshGameList(system);
                config::SetGamePath(system, game_list.directory);
            }
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Filter to common file types", &game_list.apply_filter)) {
            RefreshGameList(system);
            config::SetFilterGameList(system, game_list.apply_filter);
        }
        if (game_list.directory.empty()) {
            ImGui::Text("No directory set!");
        } else {
            // TODO: see if conversion to char* which ImGui::Gui requires can be done in a better way.
            // On Windows: directories containing non-ASCII characters display incorrectly
            ImGui::Text(game_list.directory.string().c_str());
        }
        if (ImGui::BeginListBox("Game selection")) {
            static size_t item_current_idx = 0; // Here we store our selection data as an index.
            for (size_t n = 0; n < game_list.games.size(); ++n) {
                bool is_selected = item_current_idx == n;
                if (ImGui::Selectable(game_lists[system].games[n].name.c_str(),
                      is_selected,
                      ImGuiSelectableFlags_AllowDoubleClick)) {
                    item_current_idx = n;
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        OnGameSelected(system, size_t(n));
                    }
                }
                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }
    };

    if (ImGui::Begin("Game selection", &show_game_selection_window)) {
        static int tab{};
        if (ImGui::Button("GBA", ImVec2(100.0f, 0.0f))) tab = 0;
        ImGui::SameLine(0.0, 2.0f);
        if (ImGui::Button("N64", ImVec2(100.0f, 0.0f))) tab = 1;

        switch (tab) {
        case 0: DrawCoreList(System::GBA); break;
        case 1: DrawCoreList(System::N64); break;
        }

        ImGui::End();
    }
}

void DrawInputBindingsWindow()
{
    static constexpr std::string_view unbound_label = "Unbound";
    static constexpr std::string_view waiting_for_input_label = "...";

    struct Button {
        std::string_view label =
          unbound_label; // can be non-owning thanks to SDL's SDL_GameControllerGetStringForButton etc
        std::string_view prev_label = label;
        bool waiting_for_input = false;

        void OnPressed()
        {
            if (waiting_for_input) {
                label = prev_label;
            } else {
                prev_label = label;
                label = waiting_for_input_label;
            }
            waiting_for_input = !waiting_for_input;
        }
    };

    static constexpr std::array hotkey_labels = { "Load state", "Save state", "Toggle fullscreen" };

    static constinit std::array hotkey_buttons = {
        Button{},
        Button{},
        Button{},
    };

    if (ImGui::Begin("Input configuration", &show_input_bindings_window)) {
        if (!CoreIsLoaded()) {
            ImGui::Text("Load a core before configuring inputs.");
            return;
        }

        // TODO: buffer this somewhere so it doesn't have to be computed on every draw
        std::unique_ptr<Core> const& core = GetCore();
        std::span<std::string_view const> control_button_labels = core->GetInputNames();

        static std::vector<Button> control_buttons;
        control_buttons.reserve(control_button_labels.size());
        for (size_t i = 0; i < control_button_labels.size(); ++i) {
            control_buttons.emplace_back(Button{});
        }

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

        size_t num_horizontal_elements = std::max(control_button_labels.size(), hotkey_buttons.size());
        for (size_t i = 0; i < num_horizontal_elements; ++i) {
            if (i < control_button_labels.size()) {
                Button& button = control_buttons[i];
                ImGui::Text(control_button_labels[i].data());
                ImGui::SameLine(100);
                if (ImGui::Button(button.label.data())) {
                    button.OnPressed();
                }
            }
            if (i < hotkey_buttons.size()) {
                Button& button = hotkey_buttons[i];
                ImGui::SameLine();
                ImGui::Text(hotkey_labels[i]);
                ImGui::SameLine(250);
                if (ImGui::Button(button.label.data())) {
                    button.OnPressed();
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
            if (ImGui::MenuItem("Core settings")) {
                OnMenuCoreSettings();
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

Status EnableFullscreen(bool enable)
{
    if (SDL_SetWindowFullscreen(sdl_window, static_cast<SDL_bool>(enable))) {
        return FailureStatus(std::format("Failed to toggle fullscreen: {}", SDL_GetError()));
    } else {
        return OkStatus();
    }
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
        message::Error("nativefiledialog returned NFD_ERROR for NFD_OpenDialogN");
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
        message::Error("nativefiledialog returned NFD_ERROR for NFD_PickFolderN");
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
        float fps = 60.0f * 1'000'000.0f / float(microsecs_to_render_60_frames);
        UpdateWindowTitle(fps);
        frame_counter = 0;
        time = std::chrono::steady_clock::now();
    }
}

SDL_Window* GetSdlWindow()
{
    if (!sdl_window) {
        Status status = InitSdl();
        if (!status.Ok()) {
            LogFatal(std::format("Failed to init SDL; {}", status.Message()));
            exit(1);
        }
    }
    return sdl_window;
}

void GetWindowSize(int* w, int* h)
{
    SDL_GetWindowSize(sdl_window, w, h);
}

Status Init(fs::path work_path)
{
    exe_path = work_path;

    window_width = 960, window_height = 720;

    game_is_running = false;
    menu_enable_audio = true;
    menu_fullscreen = false;
    menu_pause_emulation = false;
    quit = false;
    show_input_bindings_window = false;
    show_menu = true;
    start_game = false;

    show_game_selection_window = !game_is_running;

    Status status{ Status::Code::Ok };
    config::Open(work_path);
    if (status = ReadConfig(); !status.Ok()) {
        LogError(status.Message());
        LogInfo("Using default configuration.");
        UseDefaultConfig();
    }
    if (status = InitSdl(); !status.Ok()) {
        return status;
    }
    if (status = InitGraphics(); !status.Ok()) {
        return status;
    }
    if (status = InitImgui(); !status.Ok()) {
        return status;
    }
    if (status = audio::Init(); !status.Ok()) {
        message::Error(std::format("Failed to init audio system; {}", status.Message()));
    }
    if (status = input::Init(); !status.Ok()) {
        message::Error("Failed to init input system!");
    }
    if (nfdresult_t result = NFD_Init(); result != NFD_OKAY) {
        message::Error(
          std::format("Failed to init nativefiledialog; NFD_Init returned {}", std::to_underlying(result)));
    }
    UpdateWindowTitle();

    return OkStatus();
}

Status InitGraphics()
{
    if (CoreIsLoaded()) {
        return GetCore()->InitGraphics();
    } else {
        // TODO: temporary solution to get vulkan rendering while n64 core is not supposed to be running
        Status status = LoadCore(System::N64);
        if (!status.Ok()) {
            return status;
        }
    }
    return GetCore()->InitGraphics();
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
        return FailureStatus("ImGui_ImplSDL3_InitForVulkan failed");
    }

    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance = vulkan::GetInstance(),
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
        .CheckVkResultFn = vulkan::CheckVkResult,
    };

    if (!ImGui_ImplVulkan_Init(&init_info, vulkan::GetRenderPass())) {
        return FailureStatus("ImGui_ImplVulkan_Init failed");
    }

    io.Fonts->AddFontDefault();
    ImGui_ImplVulkan_CreateFontsTexture();
    vulkan::SubmitRequestedCommandBuffer();

    return OkStatus();
}

Status InitSdl()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        return FailureStatus(std::format("Failed to init SDL: {}\n", SDL_GetError()));
    }
    sdl_window = SDL_CreateWindow("tessoe",
      window_width,
      window_height,
      SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    if (!sdl_window) {
        return FailureStatus(std::format("Failed to create SDL window: {}\n", SDL_GetError()));
    }
    if (Status status = message::Init(sdl_window); !status.Ok()) {
        LogError(std::format("Failed to initialize user message system; {}", status.Message()));
    }
    return OkStatus();
}

Status LoadGame(fs::path const& path)
{
    std::unique_ptr<Core> const& core = GetCore();
    assert(core);
    System system = GetSystem();
    switch (system) {
    case System::N64: core->ApplyConfig(n64_configuration); break;
    default: assert(false);
    }
    Status status = GetCore()->LoadRom(path);
    if (status.Ok()) {
        if (game_is_running) {
            StopGame();
        }
        start_game = true;
        current_game_title = path.filename().string();
        GameList& game_list = game_lists[system];
        if (game_list.directory.empty()) {
            game_list.directory = path.parent_path();
            RefreshGameList(system);
        }
    }
    return status;
}

bool NeedsDraw()
{
    return show_menu || show_input_bindings_window || show_game_selection_window;
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

void OnGameSelected(System system, size_t list_index)
{
    GameListEntry const& entry = game_lists[system].games[list_index];
    if (CoreIsLoaded()) {
        StopGame();
    }
    Status status = LoadCoreAndGame(entry.path);
    if (status.Ok()) {
        current_game_title = entry.name;
        start_game = true;
    } else {
        message::Error(status.Message());
    }
}

void OnInputBindingsWindowResetAll()
{
    // TODO
}

void OnInputBindingsWindowSave()
{
    Status status = input::SaveBindingsToDisk();
    if (!status.Ok()) {
        message::Error(status.Message());
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

void OnMenuCoreSettings()
{
    show_core_settings_window = !show_core_settings_window;
}

void OnMenuEnableAudio()
{
    // menu_enable_audio ? audio::Enable() : audio::Disable();
}

void OnMenuFullscreen()
{
    Status status = EnableFullscreen(menu_fullscreen);
    if (!status.Ok()) {
        menu_fullscreen = !menu_fullscreen; // revert change
        message::Error(status.Message());
    }
}

void OnMenuLoadState()
{
}

void OnMenuOpen()
{
    std::optional<fs::path> path = FileDialog();
    if (path) {
        Status status = LoadCoreAndGame(path.value());
        if (!status.Ok()) {
            message::Error(status.Message());
        }
    }
}

void OnMenuOpenBios()
{
    std::optional<fs::path> path = FileDialog();
    if (path) {
        fs::path path_val = std::move(path.value());
        // if (!N64::LoadBios(path_val)) {
        //     message::Error(std::format("Could not load bios at path \"{}\"", path_val.string()));
        // }
    }
}

void OnMenuOpenRecent()
{
    // TODO
}

void OnMenuPause()
{
    if (CoreIsLoaded()) {
        menu_pause_emulation ? GetCore()->Pause() : GetCore()->Resume();
    }
}

void OnMenuQuit()
{
    OnSdlQuit();
}

void OnMenuReset()
{
    if (CoreIsLoaded()) {
        GetCore()->Reset();
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

void OnSdlQuit()
{
    quit = true;
    if (CoreIsLoaded()) {
        GetCore()->Stop();
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
        case SDL_EVENT_AUDIO_DEVICE_ADDED: audio::OnDeviceAdded(event); break;
        case SDL_EVENT_AUDIO_DEVICE_REMOVED: audio::OnDeviceRemoved(event); break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: input::OnGamepadAxisMotion(event); break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: input::OnGamepadButtonChange(event, true); break;
        case SDL_EVENT_GAMEPAD_BUTTON_UP: input::OnGamepadButtonChange(event, false); break;
        case SDL_EVENT_GAMEPAD_ADDED: input::OnGamepadAdded(event); break;
        case SDL_EVENT_GAMEPAD_REMOVED: input::OnGamepadRemoved(event); break;
        case SDL_EVENT_KEY_DOWN: input::OnKeyChange(event, true); break;
        case SDL_EVENT_KEY_UP: input::OnKeyChange(event, false); break;
        case SDL_EVENT_QUIT: OnSdlQuit(); break;
        case SDL_EVENT_WINDOW_RESIZED: OnWindowResizeEvent(event); break;
        }
    }
}

Status ReadConfig()
{
    for (System system : systems) {
        GameList& game_list = game_lists[system];

        if (std::optional<std::string> rom_path = config::GetGamePath(system); rom_path) {
            game_list.directory = rom_path.value();
        } else {
            game_list.directory = exe_path;
        }

        if (std::optional<bool> apply_filter = config::GetFilterGameList(system); apply_filter) {
            game_list.apply_filter = apply_filter.value();
        } else {
            game_list.apply_filter = false;
        }

        RefreshGameList(system);
    }
    return OkStatus();
}

void RefreshGameList(System system)
{
    GameList& game_list = game_lists[system];
    game_list.games.clear();
    std::vector<fs::path> const& known_exts = system_to_rom_exts.at(system);
    auto IsKnownExt = [&known_exts](fs::path const& ext) {
        return rng::find_if(known_exts, [&ext](fs::path const& known_ext) { return ext.compare(known_ext) == 0; })
            != known_exts.end();
    };
    if (fs::exists(game_list.directory)) {
        for (fs::directory_entry const& entry : fs::directory_iterator(game_list.directory)) {
            if (entry.is_regular_file()
                && (!game_list.apply_filter || IsKnownExt(entry.path().filename().extension()))) {
                game_list.games.push_back(GameListEntry{ entry.path(), entry.path().filename().string() });
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
            if (CoreIsLoaded()) {
                GetCore()->UpdateScreen();
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
    GetCore()->Reset();
    GetCore()->Run();
}

void StopGame()
{
    if (CoreIsLoaded()) {
        GetCore()->Stop();
    }
    game_is_running = false;
    UpdateWindowTitle();
    show_game_selection_window = true;
    // TODO: show nice n64 background on window
}

void UpdateWindowTitle(float fps)
{
    if (game_is_running) {
        std::string title =
          std::format("teesoe | {} | {} | FPS: {}", SystemToString(GetSystem()), current_game_title, fps);
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
