cmake_minimum_required (VERSION 3.25)

target_sources(teesoe PRIVATE
	main.cpp

	common/log.cpp
	common/sse_util.cpp

	frontend/audio.cpp
	frontend/config.cpp
	frontend/gui.cpp
	frontend/input.cpp
	frontend/loader.cpp
	frontend/message.cpp
	frontend/vulkan.cpp

	mips/disassembler.hpp
	mips/interpreter.hpp
	mips/recompiler.hpp
	mips/recompiler_x64.hpp
	mips/register_allocator.hpp
	mips/types.hpp
)

target_include_directories(teesoe PRIVATE
	.
	common
)

add_subdirectory(gba)
add_subdirectory(n64)