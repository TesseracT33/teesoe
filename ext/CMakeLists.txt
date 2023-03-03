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
target_include_directories(imgui PRIVATE imgui SDL/include)
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
target_include_directories(parallel-rdp-standalone PRIVATE 
	parallel-rdp-standalone
	parallel-rdp-standalone/util
	parallel-rdp-standalone/volk
	parallel-rdp-standalone/vulkan
	parallel-rdp-standalone/vulkan-headers/include
)
target_link_libraries(teesoe parallel-rdp-standalone)

##### nativefiledialog-extended #########################
add_subdirectory(nativefiledialog-extended)
target_link_libraries(teesoe nfd)