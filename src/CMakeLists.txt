cmake_minimum_required (VERSION 3.13)

target_sources(teesoe PRIVATE
	main.cpp

	common/log.cpp
	common/sse_util.cpp

	frontend/audio.cpp 
	frontend/gui.cpp
	frontend/input.cpp
	frontend/loader.cpp
	frontend/message.cpp
	frontend/vulkan.cpp

	jit/jit.cpp	
	jit/x86.cpp
)

target_include_directories(teesoe PRIVATE
	.
	common
)

add_subdirectory(n64)