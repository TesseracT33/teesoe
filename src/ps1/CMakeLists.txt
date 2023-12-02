cmake_minimum_required (VERSION 3.25)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
	r3000a/cop0.cpp
	r3000a/decoder.cpp
	r3000a/dma.cpp
	r3000a/exceptions.cpp
	r3000a/gte.cpp
	r3000a/interpreter.cpp
	r3000a/memory.cpp
	r3000a/r3000a.cpp
	r3000a/recompiler.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
	.
	common
)