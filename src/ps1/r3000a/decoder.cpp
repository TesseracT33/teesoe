#include "decoder.hpp"
#include "cop0.hpp"
#include "disassembler.hpp"
#include "gte.hpp"
#include "interpreter.hpp"
#include "log.hpp"
#include "ps1_build_options.hpp"
#include "recompiler.hpp"

#include <format>

#define IMM16 (instr & 0xFFFF)
#define IMM26 (instr & 0x3FF'FFFF)
#define SA    (instr >> 6 & 31)
#define RD    (instr >> 11 & 31)
#define RT    (instr >> 16 & 31)
#define RS    (instr >> 21 & 31)
#define BASE  (instr >> 21 & 31)

#define LOG(instr, ...)                                             \
    if constexpr (log_cpu_instructions) {                           \
        log(std::format("${:08X}  {}",                              \
          cpu_impl == CpuImpl::Interpreter ? u32(pc) : u32(jit_pc), \
          disassembler.instr(__VA_ARGS__)));                        \
    }

#define CPU(instr, ...)                                   \
    {                                                     \
        LOG(instr, __VA_ARGS__);                          \
        if constexpr (cpu_impl == CpuImpl::Interpreter) { \
            cpu_interpreter.instr(__VA_ARGS__);           \
        } else if constexpr (arch.a64) {                  \
            /*a64::cpu_recompiler.instr(__VA_ARGS__);*/   \
        } else {                                          \
            /*x64::cpu_recompiler.instr(__VA_ARGS__);*/   \
        }                                                 \
    }

#define COP0(instr, ...)                                  \
    {                                                     \
        LOG(instr, __VA_ARGS__);                          \
        if constexpr (cpu_impl == CpuImpl::Interpreter) { \
            instr(__VA_ARGS__);                           \
        } else if constexpr (arch.a64) {                  \
            /*a64::instr(__VA_ARGS__);*/                  \
        } else {                                          \
            /*x64::instr(__VA_ARGS__);*/                  \
        }                                                 \
    }

#define GTE(instr, ...) assert(false);

namespace ps1::r3000a {

template<CpuImpl cpu_impl> static void decode_cop0(u32 instr);
template<CpuImpl cpu_impl> static void decode_cop1(u32 instr);
template<CpuImpl cpu_impl> static void decode_cop2(u32 instr);
template<CpuImpl cpu_impl> static void decode_cop3(u32 instr);
template<CpuImpl cpu_impl> static void decode_regimm(u32 instr);
template<CpuImpl cpu_impl> static void decode_special(u32 instr);
static void on_reserved_instruction(auto instr);

template<CpuImpl cpu_impl> void decode_cop0(u32 instr)
{
    switch (instr >> 21 & 31) {
    case 0: COP0(mfc0, RD, RT); break;
    case 4: COP0(mtc0, RD, RT); break;
    case 16:
        if ((instr & 63) == 0x10) {
            COP0(rfe);
        } else {
            on_reserved_instruction(instr);
        }
        break;

    default: on_reserved_instruction(instr);
    }
}

template<CpuImpl cpu_impl> void decode_cop1(u32 instr)
{
    on_reserved_instruction(instr);
}

template<CpuImpl cpu_impl> void decode_cop2(u32 instr)
{
    switch (instr >> 21 & 31) {
    case 0: GTE(mfc2, RT, RD); break;
    case 2: GTE(cfc2, RT, RD); break;
    case 4: GTE(mtc2, RT, RD); break;
    case 6: GTE(ctc2, RT, RD); break;
    default:
        if (!(instr >> 25 & 1)) break;
        switch (instr & 63) {
        case 0x01: GTE(rtps); break;
        case 0x06: GTE(nclip); break;
        case 0x0C: GTE(rtps); break;
        case 0x10: GTE(dpcs); break;
        case 0x11: GTE(intpl); break;
        case 0x12: GTE(mvmva); break;
        case 0x13: GTE(ncds); break;
        case 0x14: GTE(cdp); break;
        case 0x16: GTE(ncdt); break;
        case 0x1B: GTE(nccs); break;
        case 0x1C: GTE(cc); break;
        case 0x1E: GTE(ncs); break;
        case 0x20: GTE(nct); break;
        case 0x28: GTE(rtps); break;
        case 0x29: GTE(dcpl); break;
        case 0x2A: GTE(dpct); break;
        case 0x2D: GTE(avsz3); break;
        case 0x2E: GTE(avsz4); break;
        case 0x30: GTE(rtpt); break;
        case 0x3D: GTE(rtps); break;
        case 0x3E: GTE(rtps); break;
        case 0x3F: GTE(ncct); break;
        default: on_reserved_instruction(instr);
        }
    }
}

template<CpuImpl cpu_impl> void decode_cop3(u32 instr)
{
    on_reserved_instruction(instr);
}

template<CpuImpl cpu_impl> void decode(u32 instr)
{
    switch (instr >> 26 & 63) {
    case 0x00: decode_special<cpu_impl>(instr); break;
    case 0x01: decode_regimm<cpu_impl>(instr); break;
    case 0x02: CPU(j, IMM26); break;
    case 0x03: CPU(jal, IMM26); break;
    case 0x04: CPU(beq, RS, RT, IMM16); break;
    case 0x05: CPU(bne, RS, RT, IMM16); break;
    case 0x06: CPU(blez, RS, IMM16); break;
    case 0x07: CPU(bgtz, RS, IMM16); break;
    case 0x08: CPU(addi, RS, RT, IMM16); break;
    case 0x09: CPU(addiu, RS, RT, IMM16); break;
    case 0x0A: CPU(slti, RS, RT, IMM16); break;
    case 0x0B: CPU(sltiu, RS, RT, IMM16); break;
    case 0x0C: CPU(andi, RS, RT, IMM16); break;
    case 0x0D: CPU(ori, RS, RT, IMM16); break;
    case 0x0E: CPU(xori, RS, RT, IMM16); break;
    case 0x0F: CPU(lui, RT, IMM16); break;
    case 0x10: decode_cop0<cpu_impl>(instr); break;
    case 0x11: decode_cop1<cpu_impl>(instr); break;
    case 0x12: decode_cop2<cpu_impl>(instr); break;
    case 0x13: decode_cop3<cpu_impl>(instr); break;
    case 0x20: CPU(lb, RS, RT, IMM16); break;
    case 0x21: CPU(lh, RS, RT, IMM16); break;
    case 0x22: CPU(lwl, RS, RT, IMM16); break;
    case 0x23: CPU(lw, RS, RT, IMM16); break;
    case 0x24: CPU(lbu, RS, RT, IMM16); break;
    case 0x25: CPU(lhu, RS, RT, IMM16); break;
    case 0x26: CPU(lwr, RS, RT, IMM16); break;
    case 0x27: CPU(lwu, RS, RT, IMM16); break;
    case 0x28: CPU(sb, RS, RT, IMM16); break;
    case 0x29: CPU(sh, RS, RT, IMM16); break;
    case 0x2A: CPU(swl, RS, RT, IMM16); break;
    case 0x2B: CPU(sw, RS, RT, IMM16); break;
    case 0x2E: CPU(swr, RS, RT, IMM16); break;
    default: on_reserved_instruction(instr);
    }
}

template<CpuImpl cpu_impl> void decode_regimm(u32 instr)
{
    switch (instr >> 16 & 31) {
    case 0x00: CPU(bltz, RS, IMM16); break;
    case 0x01: CPU(bgez, RS, IMM16); break;
    case 0x10: CPU(bltzal, RS, IMM16); break;
    case 0x11: CPU(bgezal, RS, IMM16); break;
    default: on_reserved_instruction(instr);
    }
}

template<CpuImpl cpu_impl> void decode_special(u32 instr)
{
    switch (instr & 63) {
    case 0x00: CPU(sll, RT, RD, SA); break;
    case 0x02: CPU(srl, RT, RD, SA); break;
    case 0x03: CPU(sra, RT, RD, SA); break;
    case 0x04: CPU(sllv, RS, RT, RD); break;
    case 0x06: CPU(srlv, RS, RT, RD); break;
    case 0x07: CPU(srav, RS, RT, RD); break;
    case 0x08: CPU(jr, RS); break;
    case 0x09: CPU(jalr, RS, RD); break;
    case 0x0C: CPU(syscall); break;
    case 0x0D: CPU(break_); break;
    case 0x10: CPU(mfhi, RD); break;
    case 0x11: CPU(mthi, RS); break;
    case 0x12: CPU(mflo, RD); break;
    case 0x13: CPU(mtlo, RS); break;
    case 0x18: CPU(mult, RS, RT); break;
    case 0x19: CPU(multu, RS, RT); break;
    case 0x1A: CPU(div, RS, RT); break;
    case 0x1B: CPU(divu, RS, RT); break;
    case 0x20: CPU(add, RS, RT, RD); break;
    case 0x21: CPU(addu, RS, RT, RD); break;
    case 0x22: CPU(sub, RS, RT, RD); break;
    case 0x23: CPU(subu, RS, RT, RD); break;
    case 0x24: CPU(and_, RS, RT, RD); break;
    case 0x25: CPU(or_, RS, RT, RD); break;
    case 0x26: CPU(xor_, RS, RT, RD); break;
    case 0x27: CPU(nor, RS, RT, RD); break;
    case 0x2A: CPU(slt, RS, RT, RD); break;
    case 0x2B: CPU(sltu, RS, RT, RD); break;
    default: on_reserved_instruction(instr);
    }
}

void on_reserved_instruction(auto instr)
{
    reserved_instruction_exception();
}

template void decode<CpuImpl::Interpreter>(u32);
template void decode<CpuImpl::Recompiler>(u32);

} // namespace ps1::r3000a
