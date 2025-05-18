cmake_minimum_required (VERSION 3.25)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
	common/decoder.cpp
	common/n64.cpp
	common/scheduler.cpp

	interface/ai.cpp
	interface/mi.cpp
	interface/pi.cpp
	interface/ri.cpp
	interface/si.cpp
	interface/vi.cpp

	memory/cart.cpp
	memory/memory.cpp
	memory/pif.cpp
	memory/rdram.cpp

	rdp/parallel_rdp_wrapper.cpp
	rdp/rdp.cpp

	rsp/interpreter.cpp
	rsp/recompiler.cpp
	rsp/register_allocator.cpp
	rsp/rsp.cpp
	rsp/vu_interpreter.cpp

	vr4300/cache.cpp
	vr4300/cop0.cpp
	vr4300/cop1.cpp
	vr4300/cop2.cpp
	vr4300/exceptions.cpp
	vr4300/interpreter.cpp
	vr4300/ipu.cpp
	vr4300/mmu.cpp
	vr4300/recompiler.cpp
	vr4300/register_allocator.cpp
	vr4300/vr4300.cpp
)

if (PLATFORM_X64)
	target_sources(${CMAKE_PROJECT_NAME} PRIVATE
		rsp/ipu_x64.cpp
		rsp/vu_x64.cpp
		vr4300/cop0_x64.cpp
		vr4300/cop1_x64.cpp
		vr4300/cop2_x64.cpp
		vr4300/ipu_x64.cpp
	)
endif()

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
	.
	common
)
