cmake_minimum_required(VERSION 3.25)

project(teesoe LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${CMAKE_PROJECT_NAME})

set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
	set(X64 1)
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" 
		OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
	set(ARM64 1)
endif()

if (NOT X64 AND NOT ARM64)
	message(FATAL_ERROR "Unsupported architecture \"${CMAKE_SYSTEM_PROCESSOR}\"; only x86-64 and arm64 are supported.")
endif()

if (CMAKE_BUILD_TYPE MATCHES "Debug")
	set(DEBUG 1)
endif()

if (CMAKE_BUILD_TYPE MATCHES "Release")
	set(RELEASE 1)
endif()

if (CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
	set(RELEASE_WITH_DEBUG_INFO 1)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CLANG 1)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set(GCC 1)
endif()

list(APPEND CLANG_FLAGS
	-std=c++2b
	-Weverything
)

list(APPEND GCC_FLAGS
	-std=c++23
	-Wduplicated-cond
	-Wduplicated-branches
	-Wuseless-cast
)

list(APPEND GNU_FLAGS
	-Wall
	-Wcast-align
	-Wconversion
	-Wdouble-promotion
	-Wextra
	-Wimplicit-fallthrough
	-Wmissing-include-dirs
	-Wno-bitwise-op-parentheses
	-Wno-c++11-compat
	-Wno-c++14-compat
	-Wno-c++17-compat
	-Wno-c++20-compat
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

list(APPEND MSVC_FLAGS
	/EHsc          # Standard stack unwinding
	/fp:precise    # Precise floating-point model
	/GR            # Enable run-time type information
	/MP            # Build with multiple processes
	/permissive-   # Disallow non-standard behaviour
	/sdl           # Enable Additional Security Checks
	/std:c++latest # C++ standard
	/volatile:iso  # Use iso standard for 'volatile'
	/W4            # Warning level 4
	/w14265        # Enable warning "Class has virtual functions, but destructor is not virtual"
	/w14555        # Enable warning "Expression has no effect; expected expression with side-effect"
	/we4289        # Treat as an error "Nonstandard extension used: 'variable': for-loop control variable declared in the for-loop is used
                   # outside the for-loop scope"
)

if (DEBUG)
	list(APPEND GNU_FLAGS
		-Og
	)
	list(APPEND MSVC_FLAGS
		/GS  # Buffer security check
		/JMC # Just My Code debugging
		/MDd # Use run-time library MSVCRTD.lib
		/Od  # Disable optimizations (Debug)
		/Zi  # Debug information format; supporting the 'Edit and Continue' feature of VS
	)
else()
	list(APPEND GNU_FLAGS
		-flto
		-O3
	)
	list(APPEND MSVC_FLAGS
		/GL # Whole program optimization
		/MD # Use run-time library MSVCRT.lib
		/O2 # Maximize speed
		/Oi # Generate intrinsic functions
	)
endif()

if (RELEASE_WITH_DEBUG_INFO)
	list(APPEND GNU_FLAGS -g)
	list(APPEND MSVC_FLAGS /DEBUG:FULL)
endif()

if (X64)
	list(APPEND GNU_FLAGS -mavx2)
	list(APPEND MSVC_FLAGS /arch:AVX2)
endif()

list(APPEND CLANG_FLAGS ${GNU_FLAGS})
list(APPEND GCC_FLAGS ${GNU_FLAGS})

if (CLANG)
	target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC ${CLANG_FLAGS})
elseif (GCC)
	target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC ${GCC_FLAGS})
elseif (MSVC)
	string(REGEX REPLACE "/W3" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")	
	target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC ${MSVC_FLAGS})
endif()

add_subdirectory(ext)
add_subdirectory(src)