cmake_minimum_required(VERSION 3.13)

project(teesoe LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(teesoe)

set_property(TARGET teesoe PROPERTY CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86-64")
	set(X64 1)
endif()

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "ARM64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
	set(ARM64 1)
endif()

if (NOT X64 AND NOT ARM64)
	message(FATAL_ERROR "Unsupported architecture; only x86-64 and arm64 are supported.")
endif()

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	set(DEBUG 1)
endif()

if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set(RELEASE 1)
endif()

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
	set(CLANG 1)
endif()

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
	set(GCC 1)
endif()

list(APPEND clang_flags
	-Wall
	-Wcast-align
	-Wconversion
	-Wdouble-promotion
	-Weverything
	-Wextra
	-Wimplicit-fallthrough
	-Wmissing-include-dirs
	-Wno-bitwise-op-parentheses
	-Wno-c++98-compat
	-Wno-c++98-compat-pedantic
	-Wno-exit-time-destructors
	-Wno-global-constructors
	-Wno-gnu-anonymous-struct
	-Wno-pre-c++14-compat
	-Wno-pre-c++17-compat
	-Wno-pre-c++20-compat
	-Wno-pre-c++2b-compat
	-Wno-shadow-uncaptured-local
	-Wno-sign-conversion
	-Wnon-virtual-dtor
	-Wnull-dereference
	-Wpedantic
	-Wunreachable-code
	-Wunused
)

if (DEBUG)
	list(APPEND clang_flags
		-Og
	)
else()
	list(APPEND clang_flags
		-flto
		-O3
	)
endif()

if (X64)
	list(APPEND clang_flags
		-mavx2
		-arch:AVX2
	)
endif()

list(APPEND gcc_flags
	-std=c++23
	-Wall
	-Wcast-align
	-Wconversion
	-Wdouble-promotion
	-Wduplicated-cond
	-Wduplicated-branches
	-Wextra
	-Wimplicit-fallthrough
	-Wmissing-include-dirs
	-Wno-c++11-compat
	-Wno-c++14-compat
	-Wno-c++17-compat
	-Wno-c++20-compat
	-Wno-exit-time-destructors
	-Wno-global-constructors
	-Wno-gnu-anonymous-struct
	-Wno-sign-conversion
	-Wnon-virtual-dtor
	-Wnull-dereference
	-Wpedantic
	-Wunreachable-code
	-Wunused
	-Wuseless-cast
)

if (DEBUG)
	list(APPEND gcc_flags
		-Og
	)
else()
	list(APPEND gcc_flags
		-flto
		-O3
	)
endif()

if (X64)
	list(APPEND gcc_flags
		-mavx2
	)
endif()

list(APPEND msvc_flags
	/EHsc          # Standard stack unwinding
	/fp:except     # Allows proper reading of floating-point exceptions
	/fp:strict     # Strict floating-point model
	/GR            # Enable run-time type information
	/MP            # Build with multiple processes
	/permissive-   # Disallow non-standard behaviour
	/sdl           # Enable Additional Security Checks
	/std:c++latest # C++ standard
	/volatile:iso  # Use iso standard for 'volatile'
	/W4            # Warning level 4
	/w14265        # Class has virtual functions, but destructor is not virtual.
	/w14555        # Expression has no effect; expected expression with side-effect
	/wd2401        # Ignore "nonstandard extension used: nameless struct/union"
	/we4289        # Nonstandard extension used: 'variable': for-loop control variable declared in the for-loop is used outside
                   # the for-loop scope
)

if (DEBUG)
	list(APPEND msvc_flags
		/GS  # Buffer security check
		/JMC # Just My Code debugging
		/MDd # Use run-time library MSVCRTD.lib
		/Od  # Disable (Debug)
		/Zi  # Debug information format; supporting the 'Edit and Continue' feature of VS
	)
else()
	list(APPEND msvc_flags
		/GL # Whole program optimization
		/MD # Use run-time library MSVCRT.lib
		/O2 # Maximize speed
		/Oi # Generate intrinsic functions
	)
endif()


if (X64)
	list(APPEND msvc_flags
		/arch:AVX2
	)
endif()

# Correctly replace default /W3 with /W4
if (MSVC)
	string(REGEX REPLACE "/W[1-3]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

if (CLANG)
	target_compile_options(teesoe PUBLIC ${clang_flags})
elseif (GCC)
	target_compile_options(teesoe PUBLIC ${gcc_flags})
elseif (MSVC)
	target_compile_options(teesoe PUBLIC ${msvc_flags})
endif()

add_subdirectory(ext)
add_subdirectory(src)