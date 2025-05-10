#pragma once

#include "numtypes.hpp"

#define RSP_INSTRUCTIONS_NAMESPACE n64::rsp
#include "instructions.hpp"
#undef RSP_INSTRUCTIONS_NAMESPACE

namespace n64::rsp {

void InterpretOneInstruction();
void OnSingleStep();
u32 RunInterpreter(u32 rsp_cycles);

} // namespace n64::rsp
