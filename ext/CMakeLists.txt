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
target_include_directories(teesoe PRIVATE SDL/include/SDL3)

##### imgui #########################
add_library(imgui STATIC
	imgui/imgui.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/backends/imgui_impl_sdl3.cpp
	imgui/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC 
	imgui
	imgui/backends
	parallel-rdp-standalone/vulkan-headers/include
	SDL/include
)
target_compile_definitions(imgui PUBLIC VK_NO_PROTOTYPES)
target_link_libraries(imgui SDL3-static)
target_link_libraries(teesoe imgui)

##### parallel-rdp-standalone #########################
add_library(parallel-rdp-standalone STATIC
	parallel-rdp-standalone/parallel-rdp/command_ring.cpp
	parallel-rdp-standalone/parallel-rdp/rdp_device.cpp
	parallel-rdp-standalone/parallel-rdp/rdp_dump_write.cpp
	parallel-rdp-standalone/parallel-rdp/rdp_renderer.cpp
	parallel-rdp-standalone/parallel-rdp/video_interface.cpp
	parallel-rdp-standalone/util/aligned_alloc.cpp
	parallel-rdp-standalone/util/arena_allocator.cpp
	parallel-rdp-standalone/util/logging.cpp
	parallel-rdp-standalone/util/thread_id.cpp
	parallel-rdp-standalone/util/thread_name.cpp
	parallel-rdp-standalone/util/timeline_trace_file.cpp
	parallel-rdp-standalone/util/timer.cpp
	parallel-rdp-standalone/volk/volk.c
	parallel-rdp-standalone/vulkan/buffer.cpp
	parallel-rdp-standalone/vulkan/buffer_pool.cpp
	parallel-rdp-standalone/vulkan/command_buffer.cpp
	parallel-rdp-standalone/vulkan/command_pool.cpp
	parallel-rdp-standalone/vulkan/context.cpp
	parallel-rdp-standalone/vulkan/cookie.cpp
	parallel-rdp-standalone/vulkan/descriptor_set.cpp
	parallel-rdp-standalone/vulkan/device.cpp
	parallel-rdp-standalone/vulkan/event_manager.cpp
	parallel-rdp-standalone/vulkan/fence.cpp
	parallel-rdp-standalone/vulkan/fence_manager.cpp
	parallel-rdp-standalone/vulkan/image.cpp
	parallel-rdp-standalone/vulkan/memory_allocator.cpp
	parallel-rdp-standalone/vulkan/pipeline_event.cpp
	parallel-rdp-standalone/vulkan/query_pool.cpp
	parallel-rdp-standalone/vulkan/render_pass.cpp
	parallel-rdp-standalone/vulkan/sampler.cpp
	parallel-rdp-standalone/vulkan/semaphore.cpp
	parallel-rdp-standalone/vulkan/semaphore_manager.cpp
	parallel-rdp-standalone/vulkan/shader.cpp
	parallel-rdp-standalone/vulkan/texture_format.cpp
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
target_link_libraries(teesoe parallel-rdp-standalone)

##### nativefiledialog-extended #########################
add_subdirectory(nativefiledialog-extended)
target_link_libraries(teesoe nfd)

#### yaml-cpp #########################
add_subdirectory(yaml-cpp)
target_link_libraries(teesoe yaml-cpp)

##### asmjit ####################
set(ASMJIT_BUILD_TEST OFF)
set(ASMJIT_STATIC ON)
set(ASMJIT_BUILD_X86 X64)
set(ASMJIT_BUILD_ARM ARM64)
add_subdirectory(asmjit)
target_link_libraries(teesoe asmjit)