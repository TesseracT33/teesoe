set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 26)

list(APPEND MSVC_FLAGS
	/MP # Build with multiple processes
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	list(APPEND GNU_FLAGS
		-Og
	)
	list(APPEND MSVC_FLAGS
		/JMC # Just My Code debugging
	)
else()
	# list(APPEND GNU_FLAGS
	# 	-flto
	# )
	list(APPEND MSVC_FLAGS
		/GL        # Whole program optimization
		/Oi        # Generate intrinsic functions
		/Zc:inline # Removes unreferenced data or functions that are COMDATs, or that only have internal linkage.
	)
endif()

if (PLATFORM_X64)
	list(APPEND GNU_FLAGS
		-mavx2
		-mbmi2
	)
	list(APPEND MSVC_FLAGS /arch:AVX2)
endif()

list(APPEND CLANG_FLAGS ${GNU_FLAGS})
list(APPEND GCC_FLAGS ${GNU_FLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_compile_options(${CLANG_FLAGS})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(${GCC_FLAGS})
elseif (MSVC)
	add_compile_options(${MSVC_FLAGS})
endif()
