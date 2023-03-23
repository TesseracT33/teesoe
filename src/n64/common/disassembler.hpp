#pragma once

#include "n64.hpp"
#include "rsp/rsp.hpp"
#include "types.hpp"
#include "vr4300/vr4300.hpp"

#include <string>

namespace n64::disassembler {

template<CpuImpl> void exec_cpu(u32 instr);
template<CpuImpl> void exec_rsp(u32 instr);
std::string make_string(u32 instr);

} // namespace n64::disassembler
