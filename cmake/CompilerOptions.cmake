set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)

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

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CLANG 1)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set(GCC 1)
endif()

list(APPEND GCC_FLAGS
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
	-Wno-exit-time-destructors
	-Wno-global-constructors
	-Wno-gnu-anonymous-struct
	-Wno-shadow-uncaptured-local
	-Wno-sign-conversion
	-Wnon-virtual-dtor
	-Wnull-dereference
	-Wunreachable-code
	-Wunused
)

list(APPEND MSVC_FLAGS
	/fp:precise     # Precise floating-point model
	/GR             # Enable run-time type information
	/MP             # Build with multiple processes
	/permissive-    # Disallow non-standard behaviour
	/volatile:iso   # Use iso standard for 'volatile'
	/W4             # Warning level 4
	/w14265         # Enable warning "Class has virtual functions, but destructor is not virtual"
	/w14555         # Enable warning "Expression has no effect; expected expression with side-effect"
	/we4289         # Treat as an error "Nonstandard extension used: 'variable': for-loop control variable declared in the for-loop is used
                    # outside the for-loop scope"
	/Zc:__cplusplus # Report an updated value for recent C++ language standards support
)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
	list(APPEND MSVC_FLAGS
		/GS  # Buffer security check
		/JMC # Just My Code debugging
	    /sdl # Enable Additional Security Checks
	)
else()
	list(APPEND GNU_FLAGS
		-flto
	)
	list(APPEND MSVC_FLAGS
		/GL        # Whole program optimization
		/Oi        # Generate intrinsic functions
		/Zc:inline # Removes unreferenced data or functions that are COMDATs, or that only have internal linkage.
	)
endif()

if (CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
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
	add_compile_options(${CLANG_FLAGS})
elseif (GCC)
	add_compile_options(${GCC_FLAGS})
elseif (MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	add_compile_options(${MSVC_FLAGS})
endif()
	