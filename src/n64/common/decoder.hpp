#pragma once

#include "numtypes.hpp"

namespace n64 {

void decode_and_emit_cpu(u32 instr);
void decode_and_emit_rsp(u32 instr);
void decode_and_interpret_cpu(u32 instr);
void decode_and_interpret_rsp(u32 instr);

} // namespace n64
