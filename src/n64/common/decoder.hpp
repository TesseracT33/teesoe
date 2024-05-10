#pragma once

#include "n64.hpp"
#include "rsp/rsp.hpp"
#include "numtypes.hpp"
#include "vr4300/vr4300.hpp"

#include <string>

namespace n64::decoder {

template<CpuImpl> void exec_cpu(u32 instr);
template<CpuImpl> void exec_rsp(u32 instr);

} // namespace n64::decoder
