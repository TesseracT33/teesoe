cmake_minimum_required (VERSION 3.13)

target_sources(teesoe PRIVATE
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

	rsp/disassembler.cpp
	rsp/interface.cpp
	rsp/rsp.cpp
	rsp/su.cpp
	rsp/vu.cpp

	vr4300/cache.cpp
	vr4300/cop0.cpp
	vr4300/cop1.cpp
	vr4300/cop2.cpp
	vr4300/cpu.cpp
	vr4300/disassembler.cpp
	vr4300/exceptions.cpp
	vr4300/mmu.cpp
	vr4300/recompiler.cpp
	vr4300/vr4300.cpp
)

target_include_directories(teesoe PRIVATE
	.
	common
)