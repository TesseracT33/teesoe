cmake_minimum_required(VERSION 3.25)

project(teesoe LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CompilerOptions.cmake)

add_executable(${CMAKE_PROJECT_NAME})

add_subdirectory(ext)
add_subdirectory(src)