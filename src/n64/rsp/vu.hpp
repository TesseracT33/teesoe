#pragma once

#include "types.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstring>
#include <format>

#include <immintrin.h>

// https://github.com/rasky/r64emu/blob/master/doc/rsp.md
//  https://n64brew.dev/wiki/Reality_Signal_Processor

namespace n64::rsp {

enum class VectorInstruction {
    /* Load instructions */
    LBV,
    LSV,
    LLV,
    LDV,
    LQV,
    LRV,
    LTV,
    LPV,
    LUV,
    LHV,
    LFV,
    LWV,
    /* Store instructions*/
    SBV,
    SSV,
    SLV,
    SDV,
    SQV,
    SRV,
    STV,
    SPV,
    SUV,
    SHV,
    SFV,
    SWV,
    /* Move instructions*/
    MTC2,
    MFC2,
    CTC2,
    CFC2,
    /* Single-lane instructions */
    VMOV,
    VRCP,
    VRSQ,
    VRCPH,
    VRSQH,
    VRCPL,
    VRSQL,
    VRNDN,
    VRNDP,
    VNOP,
    /* Computational instructions */
    VMULF,
    VMULU,
    VMULQ,
    VMUDL,
    VMUDM,
    VMUDN,
    VMUDH,
    VMACF,
    VMACU,
    VMACQ,
    VMADL,
    VMADM,
    VADMN,
    VADMH,
    VADD,
    VABS,
    VADDC,
    VSUB,
    VSUBC,
    VMADN,
    VMADH,
    VSAR,
    VAND,
    VNAND,
    VOR,
    VNOR,
    VXOR,
    VNXOR,
    VZERO,
    /* Select instructions */
    VLT,
    VEQ,
    VNE,
    VGE,
    VCH,
    VCR,
    VCL,
    VMRG
};

template<VectorInstruction> void VectorLoadStore(u32 instr_code);
template<VectorInstruction> void Move(u32 instr_code);
template<VectorInstruction> void SingleLaneInstr(u32 instr_code);
template<VectorInstruction> void ComputeInstr(u32 instr_code);
template<VectorInstruction> void SelectInstr(u32 instr_code);

} // namespace n64::rsp
