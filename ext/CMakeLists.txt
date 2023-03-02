cmake_minimum_required(VERSION 3.13)

target_include_directories(teesoe PRIVATE .)

##### SDL #########################
set(SDL2_DISABLE_SDL2MAIN ON)
set(SDL2_DISABLE_INSTALL ON)
set(SDL2_DISABLE_UNINSTALL ON)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_TEST OFF)
set(SDL_SHARED_ENABLED_BY_DEFAULT OFF)
set(SDL_STATIC_ENABLED_BY_DEFAULT ON)
set(SDL_TEST_ENABLED_BY_DEFAULT OFF)
add_subdirectory(SDL)
target_link_libraries(teesoe SDL3-static)

##### imgui #########################
add_library(imgui STATIC
	imgui/imgui.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/backends/imgui_impl_sdl3.cpp
)
target_include_directories(imgui PRIVATE imgui)
target_link_libraries(imgui SDL3-static)
target_link_libraries(teesoe imgui)