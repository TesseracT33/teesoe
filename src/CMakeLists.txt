cmake_minimum_required (VERSION 3.25)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
	main.cpp

	common/files.cpp
	common/jit_common.cpp
	common/log.cpp
	common/sse_util.cpp

	frontend/audio.cpp
	frontend/config.cpp
	frontend/gui.cpp
	frontend/input.cpp
	frontend/loader.cpp
	frontend/message.cpp
	frontend/render_context.cpp
	frontend/sdl_render_context.cpp
	frontend/vulkan_render_context.cpp

	mips/disassembler.hpp
	mips/register_allocator_state.hpp
	mips/types.hpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
	.
	common
)

add_subdirectory(gba)
add_subdirectory(n64)
