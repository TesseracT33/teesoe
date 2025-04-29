cmake_minimum_required(VERSION 3.25)

##### asmjit ####################
set(ASMJIT_BUILD_TEST OFF)
set(ASMJIT_STATIC ON)
set(ASMJIT_BUILD_X86 X64)
set(ASMJIT_BUILD_ARM ARM64)
add_subdirectory(asmjit)
target_compile_options(asmjit PRIVATE -w) # disable warnings
target_link_libraries(${CMAKE_PROJECT_NAME} asmjit)

##### bit7z ####################
set(BIT7Z_BUILD_DOCS OFF)
set(BIT7Z_BUILD_TESTS OFF)
add_subdirectory(bit7z)
if(MSVC AND CLANG)
	# prevent clang-cl "argument unused during compilation: /Zc:throwingNew, /Zc:referenceBinding" errors
	target_compile_options(bit7z64 PRIVATE -Wno-error=unused-command-line-argument)
endif()
target_compile_options(bit7z64 PRIVATE -w) # disable warnings
target_link_libraries(${CMAKE_PROJECT_NAME} bit7z64)

##### imgui #########################
add_library(imgui STATIC
	imgui/imgui.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/backends/imgui_impl_sdl3.cpp
	imgui/backends/imgui_impl_sdlrenderer3.cpp
	imgui/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC
	imgui
	imgui/backends
	parallel-rdp-standalone/vulkan-headers/include
	SDL/include
)
target_compile_definitions(imgui PUBLIC VK_NO_PROTOTYPES)
target_compile_options(imgui PRIVATE -w) # disable warnings
target_link_libraries(${CMAKE_PROJECT_NAME} imgui)

##### parallel-rdp-standalone #########################
# Do not use vulkan/device_fossilize.cpp
add_library(parallel-rdp-standalone STATIC
	parallel-rdp-standalone/parallel-rdp/command_ring.cpp
	parallel-rdp-standalone/parallel-rdp/rdp_device.cpp
	parallel-rdp-standalone/parallel-rdp/rdp_dump_write.cpp
	parallel-rdp-standalone/parallel-rdp/rdp_renderer.cpp
	parallel-rdp-standalone/parallel-rdp/video_interface.cpp
	parallel-rdp-standalone/util/aligned_alloc.cpp
	parallel-rdp-standalone/util/arena_allocator.cpp
	parallel-rdp-standalone/util/environment.cpp
	parallel-rdp-standalone/util/logging.cpp
	parallel-rdp-standalone/util/thread_id.cpp
	parallel-rdp-standalone/util/thread_name.cpp
	parallel-rdp-standalone/util/timeline_trace_file.cpp
	parallel-rdp-standalone/util/timer.cpp
	parallel-rdp-standalone/volk/volk.c
	parallel-rdp-standalone/vulkan/texture/texture_format.cpp
	parallel-rdp-standalone/vulkan/buffer_pool.cpp
	parallel-rdp-standalone/vulkan/buffer.cpp
	parallel-rdp-standalone/vulkan/command_buffer.cpp
	parallel-rdp-standalone/vulkan/command_pool.cpp
	parallel-rdp-standalone/vulkan/context.cpp
	parallel-rdp-standalone/vulkan/cookie.cpp
	parallel-rdp-standalone/vulkan/descriptor_set.cpp
	parallel-rdp-standalone/vulkan/device.cpp
	parallel-rdp-standalone/vulkan/event_manager.cpp
	parallel-rdp-standalone/vulkan/fence_manager.cpp
	parallel-rdp-standalone/vulkan/fence.cpp
	parallel-rdp-standalone/vulkan/image.cpp
	parallel-rdp-standalone/vulkan/indirect_layout.cpp
	parallel-rdp-standalone/vulkan/memory_allocator.cpp
	parallel-rdp-standalone/vulkan/pipeline_event.cpp
	parallel-rdp-standalone/vulkan/query_pool.cpp
	parallel-rdp-standalone/vulkan/render_pass.cpp
	parallel-rdp-standalone/vulkan/sampler.cpp
	parallel-rdp-standalone/vulkan/semaphore_manager.cpp
	parallel-rdp-standalone/vulkan/semaphore.cpp
	parallel-rdp-standalone/vulkan/shader.cpp
	parallel-rdp-standalone/vulkan/wsi.cpp
)
target_include_directories(parallel-rdp-standalone PUBLIC
	parallel-rdp-standalone
	parallel-rdp-standalone/util
	parallel-rdp-standalone/volk
	parallel-rdp-standalone/vulkan
	parallel-rdp-standalone/vulkan-headers/include
)
target_compile_definitions(parallel-rdp-standalone PUBLIC VK_NO_PROTOTYPES)
if(WIN32)
	target_compile_definitions(parallel-rdp-standalone PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()
target_compile_options(parallel-rdp-standalone PRIVATE -w) # disable warnings
target_link_libraries(${CMAKE_PROJECT_NAME} parallel-rdp-standalone)

##### SDL #########################
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_TEST_LIBRARY OFF)
set(SDL_TESTS OFF)
set(SDL_SHARED_ENABLED_BY_DEFAULT OFF)
set(SDL_STATIC_ENABLED_BY_DEFAULT ON)
set(SDL_TEST_ENABLED_BY_DEFAULT OFF)
set(SDL2_DISABLE_SDL2MAIN ON)
set(SDL2_DISABLE_INSTALL ON)
set(SDL2_DISABLE_UNINSTALL ON)
add_subdirectory(SDL)
target_compile_options(SDL3-static PRIVATE -w) # disable warnings
target_link_libraries(${CMAKE_PROJECT_NAME} SDL3-static)

#### yaml-cpp #########################
add_subdirectory(yaml-cpp)
target_link_libraries(${CMAKE_PROJECT_NAME} yaml-cpp)
target_compile_options(yaml-cpp PRIVATE -w) # disable warnings

##### General #################
set_target_properties(
	asmjit bit7z64 imgui parallel-rdp-standalone SDL3-static yaml-cpp
	PROPERTIES
	SYSTEM TRUE
)
