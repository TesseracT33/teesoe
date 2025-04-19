list(APPEND CLANG_FLAGS
	-Wno-bitwise-op-parentheses
	-Wno-c++14-compat
	-Wno-c++17-compat
	-Wno-c++20-compat
	-Wno-c++98-compat
	-Wno-c++98-compat-pedantic
	-Wno-global-constructors
	-Wno-gnu-anonymous-struct
	-Wno-pre-c++14-compat
	-Wno-pre-c++17-compat
	-Wno-pre-c++20-compat
	-Wno-shadow-uncaptured-local
	-Wno-undefined-func-template
	-Wno-unsafe-buffer-usage
)

list(APPEND GCC_FLAGS
	-Wduplicated-cond
	-Wduplicated-branches
    -Wnrvo
	-Wuseless-cast
)

list(APPEND GNU_FLAGS
	-Wall
	-Wcast-align
	-Wconversion
	-Wdouble-promotion
	-Werror=return-type
	-Werror=switch
	-Werror=unused-parameter
	-Werror=unused-variable
	-Wextra
	-Wimplicit-fallthrough
	-Wmissing-include-dirs
	-Wno-c++11-compat
	-Wno-float-equal
	-Wno-missing-braces
	-Wno-parentheses
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

list(APPEND CLANG_FLAGS ${GNU_FLAGS})
list(APPEND GCC_FLAGS ${GNU_FLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC ${CLANG_FLAGS})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC ${GCC_FLAGS})
elseif (MSVC)
	target_compile_definitions(${CMAKE_PROJECT_NAME}PUBLIC _CRT_SECURE_NO_WARNINGS)
	target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC ${MSVC_FLAGS})
endif()
