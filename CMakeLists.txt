cmake_minimum_required(VERSION 3.25)

project(teesoe LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
	set(PLATFORM_X64 1)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"
		OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
	set(PLATFORM_A64 1)
else()
    message(FATAL_ERROR "Unsupported architecture \"${CMAKE_SYSTEM_PROCESSOR}\"; only x86-64 and arm64 are supported.")
endif()

include(GlobalCompilerOptions.cmake)

add_executable(${CMAKE_PROJECT_NAME})

add_subdirectory(ext)

include(CompilerOptions.cmake)

add_subdirectory(src)
