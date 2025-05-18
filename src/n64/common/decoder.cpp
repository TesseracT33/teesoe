#include "decoder.hpp"
#include "log.hpp"
#include "n64.hpp"
#include "n64_build_options.hpp"
#include "numeric.hpp"
#include "platform.hpp"
#include "rsp/disassembler.hpp"
#include "rsp/interpreter.hpp"
#include "rsp/recompiler.hpp"
#include "rsp/vu.hpp"
#include "vr4300/cache.hpp"
#include "vr4300/cop0.hpp"
#include "vr4300/cop1.hpp"
#include "vr4300/cop2.hpp"
#include "vr4300/disassembler.hpp"
#include "vr4300/exceptions.hpp"
#include "vr4300/interpreter.hpp"
#include "vr4300/recompiler.hpp"

#include <utility>

#define RSP_NAMESPACE    n64::rsp
#define VR4300_NAMESPACE n64::vr4300

#if PLATFORM_A64
#    define RSP_JIT_NAMESPACE    n64::rsp::a64
#    define VR4300_JIT_NAMESPACE n64::vr4300::a64
#elif PLATFORM_X64
#    define RSP_JIT_NAMESPACE    n64::rsp::x64
#    define VR4300_JIT_NAMESPACE n64::vr4300::x64
#else
#    error "Unrecognized platform"
#endif

#define IMM7    (SignExtend<s32, 7>(instr & 127))
#define IMM16   (s16(instr))
#define COND    (instr & 15)
#define FMT     (instr & 63)
#define SA      (instr >> 6 & 31)
#define FD      (instr >> 6 & 31)
#define VD      (instr >> 6 & 31)
#define ELEM_LO (instr >> 7 & 15)
#define VD_E    (instr >> 11 & 7)
#define RD      (instr >> 11 & 31)
#define FS      (instr >> 11 & 31)
#define VS      (instr >> 11 & 31)
#define RT      (instr >> 16 & 31)
#define FT      (instr >> 16 & 31)
#define VT      (instr >> 16 & 31)
#define ELEM_HI (instr >> 21 & 15)
#define VT_E    (instr >> 21 & 15)
#define RS      (instr >> 21 & 31)
#define BASE    (instr >> 21 & 31)
// #define VT_E    (vt_e_bug(instr >> 21 & 15, VD_E))

#define LOG_RSP(instr, ...)                                             \
    do {                                                                \
        if constexpr (cpu == Cpu::RSP && log_rsp_instructions) {        \
            LogInfo("${:03X}  {}",                                      \
              cpu_impl == CpuImpl::Interpreter ? rsp::pc : rsp::jit_pc, \
              rsp_disassembler.instr(__VA_ARGS__));                     \
        }                                                               \
    } while (0)

#define LOG_VR4300(instr, ...)                                                \
    do {                                                                      \
        if constexpr (cpu == Cpu::VR4300 && log_cpu_instructions) {           \
            LogInfo("${:016X}  {}",                                           \
              cpu_impl == CpuImpl::Interpreter ? vr4300::pc : vr4300::jit_pc, \
              vr4300_disassembler.instr(__VA_ARGS__));                        \
        }                                                                     \
    } while (0)

#define LOG(instr, ...)                         \
    do {                                        \
        if constexpr (cpu == Cpu::VR4300) {     \
            LOG_VR4300(instr, __VA_ARGS__);     \
        } else if constexpr (cpu == Cpu::RSP) { \
            LOG_RSP(instr, __VA_ARGS__);        \
        }                                       \
    } while (0)

#define COP1_FMT_IMPL(instr, fmt, ...)                     \
    do {                                                   \
        LOG_VR4300(instr<fmt>, __VA_ARGS__);               \
        if constexpr (cpu_impl == CpuImpl::Interpreter) {  \
            VR4300_NAMESPACE::instr<fmt>(__VA_ARGS__);     \
        } else {                                           \
            VR4300_JIT_NAMESPACE::instr<fmt>(__VA_ARGS__); \
        }                                                  \
    } while (0)

#define COP1_FMT(instr_name, ...)                                                                  \
    do {                                                                                           \
        switch (instr >> 21 & 31) {                                                                \
        case std::to_underlying(VR4300_NAMESPACE::FpuFmt::Float32):                                \
            COP1_FMT_IMPL(instr_name, VR4300_NAMESPACE::FpuFmt::Float32, __VA_ARGS__);             \
            break;                                                                                 \
        case std::to_underlying(VR4300_NAMESPACE::FpuFmt::Float64):                                \
            COP1_FMT_IMPL(instr_name, VR4300_NAMESPACE::FpuFmt::Float64, __VA_ARGS__);             \
            break;                                                                                 \
        case std::to_underlying(VR4300_NAMESPACE::FpuFmt::Int32):                                  \
            COP1_FMT_IMPL(instr_name, VR4300_NAMESPACE::FpuFmt::Int32, __VA_ARGS__);               \
            break;                                                                                 \
        case std::to_underlying(VR4300_NAMESPACE::FpuFmt::Int64):                                  \
            COP1_FMT_IMPL(instr_name, VR4300_NAMESPACE::FpuFmt::Int64, __VA_ARGS__);               \
            break;                                                                                 \
        default: COP1_FMT_IMPL(instr_name, VR4300_NAMESPACE::FpuFmt::Invalid, __VA_ARGS__); break; \
        }                                                                                          \
    } while (0)

#define COP_VR4300(instr, ...)                                \
    do {                                                      \
        LOG_VR4300(instr, __VA_ARGS__);                       \
        if constexpr (cpu == Cpu::VR4300) {                   \
            if constexpr (cpu_impl == CpuImpl::Interpreter) { \
                VR4300_NAMESPACE::instr(__VA_ARGS__);         \
            } else {                                          \
                VR4300_JIT_NAMESPACE::instr(__VA_ARGS__);     \
            }                                                 \
        } else {                                              \
            rsp::NotifyIllegalInstr(#instr);                  \
        }                                                     \
    } while (0)

#define CPU_VR4300(instr, ...)                                \
    do {                                                      \
        LOG_VR4300(instr, __VA_ARGS__);                       \
        if constexpr (cpu == Cpu::VR4300) {                   \
            if constexpr (cpu_impl == CpuImpl::Interpreter) { \
                VR4300_NAMESPACE::instr(__VA_ARGS__);         \
            } else {                                          \
                VR4300_JIT_NAMESPACE::instr(__VA_ARGS__);     \
            }                                                 \
        } else {                                              \
            RSP_NAMESPACE::NotifyIllegalInstr(#instr);        \
        }                                                     \
    } while (0)

#define RSP(instr, ...)                                   \
    do {                                                  \
        LOG_RSP(instr, __VA_ARGS__);                      \
        if constexpr (cpu_impl == CpuImpl::Interpreter) { \
            RSP_NAMESPACE::instr(__VA_ARGS__);            \
        } else {                                          \
            RSP_JIT_NAMESPACE::instr(__VA_ARGS__);        \
        }                                                 \
    } while (0)

#define CPU(instr, ...)                     \
    do {                                    \
        LOG(instr, __VA_ARGS__);            \
        if constexpr (cpu == Cpu::VR4300) { \
            CPU_VR4300(instr, __VA_ARGS__); \
        } else {                            \
            RSP(instr, __VA_ARGS__);        \
        }                                   \
    } while (0)

namespace n64 {

static rsp::Disassembler rsp_disassembler;
static vr4300::Disassembler vr4300_disassembler;

template<Cpu cpu, CpuImpl cpu_impl, bool make_string> static void cop0(u32 instr);
template<Cpu cpu, CpuImpl cpu_impl, bool make_string> static void cop1(u32 instr);
template<Cpu cpu, CpuImpl cpu_impl, bool make_string> static void cop2(u32 instr);
template<Cpu cpu, CpuImpl cpu_impl, bool make_string> static void cop3(u32 instr);
template<Cpu cpu, CpuImpl cpu_impl, bool make_string> static void disassemble(u32 instr);
template<Cpu cpu, CpuImpl cpu_impl, bool make_string> static void regimm(u32 instr);
template<Cpu cpu, CpuImpl cpu_impl, bool make_string> static void reserved_instruction(u32 instr);
template<Cpu cpu, CpuImpl cpu_impl, bool make_string> static void special(u32 instr);
u32 vt_e_bug(u32 vt_e, u32 vd_e);

template<Cpu cpu, CpuImpl cpu_impl, bool make_string> void cop0(u32 instr)
{
    if constexpr (cpu == Cpu::VR4300) {
        switch (instr >> 21 & 31) {
        case 0x10: {
            switch (instr & 63) {
            case 0x01: COP_VR4300(tlbr); break;
            case 0x02: COP_VR4300(tlbwi); break;
            case 0x06: COP_VR4300(tlbwr); break;
            case 0x08: COP_VR4300(tlbp); break;
            case 0x18: COP_VR4300(eret); break;
            default:
                /* "Invalid", but does not cause a reserved instruction exception. */
                vr4300::NotifyIllegalInstrCode(instr);
            }
            break;
        }
        case 0: COP_VR4300(mfc0, RT, RD); break;
        case 1: COP_VR4300(dmfc0, RT, RD); break;
        case 4: COP_VR4300(mtc0, RT, RD); break;
        case 5: COP_VR4300(dmtc0, RT, RD); break;
        default: reserved_instruction<cpu, cpu_impl, make_string>(instr);
        }
    } else {
        switch (instr >> 21 & 31) {
        case 0: RSP(mfc0, RT, RD); break;
        case 4: RSP(mtc0, RT, RD); break;
        default: rsp::NotifyIllegalInstrCode(instr);
        }
    }
}

template<Cpu cpu, CpuImpl cpu_impl, bool make_string> void cop1(u32 instr)
{
    if constexpr (cpu == Cpu::RSP) {
        return rsp::NotifyIllegalInstrCode(instr);
    }

    switch (instr >> 21 & 31) {
    case 0: COP_VR4300(mfc1, FS, RT); break;
    case 1: COP_VR4300(dmfc1, FS, RT); break;
    case 2: COP_VR4300(cfc1, FS, RT); break;
    case 3: COP_VR4300(dcfc1); break;
    case 4: COP_VR4300(mtc1, FS, RT); break;
    case 5: COP_VR4300(dmtc1, FS, RT); break;
    case 6: COP_VR4300(ctc1, FS, RT); break;
    case 7: COP_VR4300(dctc1); break;
    case 8: {
        switch (instr >> 16 & 31) {
        case 0: COP_VR4300(bc1f, IMM16); break;
        case 1: COP_VR4300(bc1t, IMM16); break;
        case 2: COP_VR4300(bc1fl, IMM16); break;
        case 3: COP_VR4300(bc1tl, IMM16); break;
        default: reserved_instruction<cpu, cpu_impl, make_string>(instr);
        }
        break;
    }
    default: {
        if ((instr & 0x30) == 0x30) {
            COP1_FMT(compare, FS, FT, COND);
        } else {
            switch (instr & 63) {
            case 0x00: COP1_FMT(add, FS, FT, FD); break;
            case 0x01: COP1_FMT(sub, FS, FT, FD); break;
            case 0x02: COP1_FMT(mul, FS, FT, FD); break;
            case 0x03: COP1_FMT(div, FS, FT, FD); break;
            case 0x04: COP1_FMT(sqrt, FS, FD); break;
            case 0x05: COP1_FMT(abs, FS, FD); break;
            case 0x06: COP1_FMT(mov, FS, FD); break;
            case 0x07: COP1_FMT(neg, FS, FD); break;
            case 0x08: COP1_FMT(round_l, FS, FD); break;
            case 0x09: COP1_FMT(trunc_l, FS, FD); break;
            case 0x0A: COP1_FMT(ceil_l, FS, FD); break;
            case 0x0B: COP1_FMT(floor_l, FS, FD); break;
            case 0x0C: COP1_FMT(round_w, FS, FD); break;
            case 0x0D: COP1_FMT(trunc_w, FS, FD); break;
            case 0x0E: COP1_FMT(ceil_w, FS, FD); break;
            case 0x0F: COP1_FMT(floor_w, FS, FD); break;
            case 0x20: COP1_FMT(cvt_s, FS, FD); break;
            case 0x21: COP1_FMT(cvt_d, FS, FD); break;
            case 0x24: COP1_FMT(cvt_w, FS, FD); break;
            case 0x25: COP1_FMT(cvt_l, FS, FD); break;
            default: /* TODO: Reserved instruction exception?? */ vr4300::NotifyIllegalInstrCode(instr);
            }
        }
    }
    }
}

template<Cpu cpu, CpuImpl cpu_impl, bool make_string> void cop2(u32 instr)
{
    if constexpr (cpu == Cpu::VR4300) {
        switch (instr >> 21 & 31) {
        case 0: COP_VR4300(mfc2, RT); break;
        case 1: COP_VR4300(dmfc2, RT); break;
        case 2: COP_VR4300(cfc2, RT); break;
        case 3: COP_VR4300(dcfc2); break;
        case 4: COP_VR4300(mtc2, RT); break;
        case 5: COP_VR4300(dmtc2, RT); break;
        case 6: COP_VR4300(ctc2, RT); break;
        case 7: COP_VR4300(dctc2); break;
        default: COP_VR4300(cop2_reserved); break;
        }
    } else {
        if (instr & 1 << 25) {
            switch (instr & 63) {
            case 0x00: RSP(vmulf, VS, VT, VD, ELEM_HI); break;
            case 0x01: RSP(vmulu, VS, VT, VD, ELEM_HI); break;
            case 0x02: RSP(vrndp, VT, VT_E, VD, VD_E); break;
            case 0x03: RSP(vmulq, VS, VT, VD, ELEM_HI); break;
            case 0x04: RSP(vmudl, VS, VT, VD, ELEM_HI); break;
            case 0x05: RSP(vmudm, VS, VT, VD, ELEM_HI); break;
            case 0x06: RSP(vmudn, VS, VT, VD, ELEM_HI); break;
            case 0x07: RSP(vmudh, VS, VT, VD, ELEM_HI); break;
            case 0x08: RSP(vmacf, VS, VT, VD, ELEM_HI); break;
            case 0x09: RSP(vmacu, VS, VT, VD, ELEM_HI); break;
            case 0x0A: RSP(vrndn, VT, VT_E, VD, VD_E); break;
            case 0x0B: RSP(vmacq, VD); break;
            case 0x0C: RSP(vmadl, VS, VT, VD, ELEM_HI); break;
            case 0x0D: RSP(vmadm, VS, VT, VD, ELEM_HI); break;
            case 0x0E: RSP(vmadn, VS, VT, VD, ELEM_HI); break;
            case 0x0F: RSP(vmadh, VS, VT, VD, ELEM_HI); break;
            case 0x10: RSP(vadd, VS, VT, VD, ELEM_HI); break;
            case 0x11: RSP(vsub, VS, VT, VD, ELEM_HI); break;
            case 0x13: RSP(vabs, VS, VT, VD, ELEM_HI); break;
            case 0x14: RSP(vaddc, VS, VT, VD, ELEM_HI); break;
            case 0x15: RSP(vsubc, VS, VT, VD, ELEM_HI); break;
            case 0x1D: RSP(vsar, VD, ELEM_HI); break;
            case 0x20: RSP(vlt, VS, VT, VD, ELEM_HI); break;
            case 0x21: RSP(veq, VS, VT, VD, ELEM_HI); break;
            case 0x22: RSP(vne, VS, VT, VD, ELEM_HI); break;
            case 0x23: RSP(vge, VS, VT, VD, ELEM_HI); break;
            case 0x24: RSP(vcl, VS, VT, VD, ELEM_HI); break;
            case 0x25: RSP(vch, VS, VT, VD, ELEM_HI); break;
            case 0x26: RSP(vcr, VS, VT, VD, ELEM_HI); break;
            case 0x27: RSP(vmrg, VS, VT, VD, ELEM_HI); break;
            case 0x28: RSP(vand, VS, VT, VD, ELEM_HI); break;
            case 0x29: RSP(vnand, VS, VT, VD, ELEM_HI); break;
            case 0x2A: RSP(vor, VS, VT, VD, ELEM_HI); break;
            case 0x2B: RSP(vnor, VS, VT, VD, ELEM_HI); break;
            case 0x2C: RSP(vxor, VS, VT, VD, ELEM_HI); break;
            case 0x2D: RSP(vnxor, VS, VT, VD, ELEM_HI); break;
            case 0x30: RSP(vrcp, VT, VT_E, VD, VD_E); break;
            case 0x31: RSP(vrcpl, VT, VT_E, VD, VD_E); break;
            case 0x32: RSP(vrcph, VT, VT_E, VD, VD_E); break;
            case 0x33: RSP(vmov, VT, VT_E, VD, VD_E); break;
            case 0x34: RSP(vrsq, VT, VT_E, VD, VD_E); break;
            case 0x35: RSP(vrsql, VT, VT_E, VD, VD_E); break;
            case 0x36: RSP(vrsqh, VT, VT_E, VD, VD_E); break;
            case 0x37:
            case 0x3F: RSP(vnop); break;
            default: RSP(vzero, VS, VT, VD, ELEM_HI); break;
            }
        } else {
            switch (instr >> 21 & 31) {
            case 0: RSP(mfc2, RT, VS, ELEM_LO); break;
            case 2: RSP(cfc2, RT, VS); break;
            case 4: RSP(mtc2, RT, VS, ELEM_LO); break;
            case 6: RSP(ctc2, RT, VS); break;
            default: rsp::NotifyIllegalInstrCode(instr);
            }
        }
    }
}

template<Cpu cpu, CpuImpl cpu_impl, bool make_string> void cop3(u32 instr)
{
    if constexpr (cpu == Cpu::VR4300) {
        if (instr >> 21 & 31) {
            if constexpr (cpu_impl == CpuImpl::Interpreter) {
                vr4300::Cop3();
            } else {
                vr4300::Cop3Jit();
            }
        } else { // MFC3
            reserved_instruction<cpu, cpu_impl, make_string>(instr);
        }
    } else {
        reserved_instruction<cpu, cpu_impl, make_string>(instr);
    }
}

void decode_and_emit_cpu(u32 instr)
{
    disassemble<Cpu::VR4300, CpuImpl::Recompiler, false>(instr);
}

void decode_and_emit_rsp(u32 instr)
{
    disassemble<Cpu::RSP, CpuImpl::Recompiler, false>(instr);
}

void decode_and_interpret_cpu(u32 instr)
{
    disassemble<Cpu::VR4300, CpuImpl::Interpreter, false>(instr);
}

void decode_and_interpret_rsp(u32 instr)
{
    disassemble<Cpu::RSP, CpuImpl::Interpreter, false>(instr);
}

template<Cpu cpu, CpuImpl cpu_impl, bool make_string> void disassemble(u32 instr)
{
    switch (instr >> 26 & 63) {
    case 0x00: special<cpu, cpu_impl, make_string>(instr); break;
    case 0x01: regimm<cpu, cpu_impl, make_string>(instr); break;
    case 0x02: CPU(j, instr); break;
    case 0x03: CPU(jal, instr); break;
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
    case 0x10: cop0<cpu, cpu_impl, make_string>(instr); break;
    case 0x11: cop1<cpu, cpu_impl, make_string>(instr); break;
    case 0x12: cop2<cpu, cpu_impl, make_string>(instr); break;
    case 0x13: cop3<cpu, cpu_impl, make_string>(instr); break;
    case 0x14: CPU_VR4300(beql, RS, RT, IMM16); break;
    case 0x15: CPU_VR4300(bnel, RS, RT, IMM16); break;
    case 0x16: CPU_VR4300(blezl, RS, IMM16); break;
    case 0x17: CPU_VR4300(bgtzl, RS, IMM16); break;
    case 0x18: CPU_VR4300(daddi, RS, RT, IMM16); break;
    case 0x19: CPU_VR4300(daddiu, RS, RT, IMM16); break;
    case 0x1A: CPU_VR4300(ldl, RS, RT, IMM16); break;
    case 0x1B: CPU_VR4300(ldr, RS, RT, IMM16); break;
    case 0x20: CPU(lb, RS, RT, IMM16); break;
    case 0x21: CPU(lh, RS, RT, IMM16); break;
    case 0x22: CPU_VR4300(lwl, RS, RT, IMM16); break;
    case 0x23: CPU(lw, RS, RT, IMM16); break;
    case 0x24: CPU(lbu, RS, RT, IMM16); break;
    case 0x25: CPU(lhu, RS, RT, IMM16); break;
    case 0x26: CPU_VR4300(lwr, RS, RT, IMM16); break;
    case 0x27: CPU(lwu, RS, RT, IMM16); break;
    case 0x28: CPU(sb, RS, RT, IMM16); break;
    case 0x29: CPU(sh, RS, RT, IMM16); break;
    case 0x2A: CPU_VR4300(swl, RS, RT, IMM16); break;
    case 0x2B: CPU(sw, RS, RT, IMM16); break;
    case 0x2C: CPU_VR4300(sdl, RS, RT, IMM16); break;
    case 0x2D: CPU_VR4300(sdr, RS, RT, IMM16); break;
    case 0x2E: CPU_VR4300(swr, RS, RT, IMM16); break;
    case 0x2F: COP_VR4300(cache, RS, RT, IMM16); break;
    case 0x30: CPU_VR4300(ll, RS, RT, IMM16); break;
    case 0x31: COP_VR4300(lwc1, BASE, FT, IMM16); break;
    case 0x32:
        switch (instr >> 11 & 31) {
        case 0: RSP(lbv, BASE, VT, ELEM_LO, IMM7); break;
        case 1: RSP(lsv, BASE, VT, ELEM_LO, IMM7); break;
        case 2: RSP(llv, BASE, VT, ELEM_LO, IMM7); break;
        case 3: RSP(ldv, BASE, VT, ELEM_LO, IMM7); break;
        case 4: RSP(lqv, BASE, VT, ELEM_LO, IMM7); break;
        case 5: RSP(lrv, BASE, VT, ELEM_LO, IMM7); break;
        case 6: RSP(lpv, BASE, VT, ELEM_LO, IMM7); break;
        case 7: RSP(luv, BASE, VT, ELEM_LO, IMM7); break;
        case 8: RSP(lhv, BASE, VT, ELEM_LO, IMM7); break;
        case 9: RSP(lfv, BASE, VT, ELEM_LO, IMM7); break;
        case 11: RSP(ltv, BASE, VT, ELEM_LO, IMM7); break;
        default: reserved_instruction<cpu, cpu_impl, make_string>(instr);
        }
        break;
    case 0x34: CPU_VR4300(lld, RS, RT, IMM16); break;
    case 0x35: COP_VR4300(ldc1, BASE, FT, IMM16); break;
    case 0x37: CPU_VR4300(ld, RS, RT, IMM16); break;
    case 0x38: CPU_VR4300(sc, RS, RT, IMM16); break;
    case 0x39: COP_VR4300(swc1, BASE, FT, IMM16); break;
    case 0x3A:
        switch (instr >> 11 & 31) {
        case 0: RSP(sbv, BASE, VT, ELEM_LO, IMM7); break;
        case 1: RSP(ssv, BASE, VT, ELEM_LO, IMM7); break;
        case 2: RSP(slv, BASE, VT, ELEM_LO, IMM7); break;
        case 3: RSP(sdv, BASE, VT, ELEM_LO, IMM7); break;
        case 4: RSP(sqv, BASE, VT, ELEM_LO, IMM7); break;
        case 5: RSP(srv, BASE, VT, ELEM_LO, IMM7); break;
        case 6: RSP(spv, BASE, VT, ELEM_LO, IMM7); break;
        case 7: RSP(suv, BASE, VT, ELEM_LO, IMM7); break;
        case 8: RSP(shv, BASE, VT, ELEM_LO, IMM7); break;
        case 9: RSP(sfv, BASE, VT, ELEM_LO, IMM7); break;
        case 10: RSP(swv, BASE, VT, ELEM_LO, IMM7); break;
        case 11: RSP(stv, BASE, VT, ELEM_LO, IMM7); break;
        default: reserved_instruction<cpu, cpu_impl, make_string>(instr);
        }
        break;
    case 0x3C: CPU_VR4300(scd, RS, RT, IMM16); break;
    case 0x3D: COP_VR4300(sdc1, BASE, FT, IMM16); break;
    case 0x3F: CPU_VR4300(sd, RS, RT, IMM16); break;
    default: reserved_instruction<cpu, cpu_impl, make_string>(instr);
    }
}

template<Cpu cpu, CpuImpl cpu_impl, bool make_string> void regimm(u32 instr)
{
    switch (instr >> 16 & 31) {
    case 0x00: CPU(bltz, RS, IMM16); break;
    case 0x01: CPU(bgez, RS, IMM16); break;
    case 0x02: CPU_VR4300(bltzl, RS, IMM16); break;
    case 0x03: CPU_VR4300(bgezl, RS, IMM16); break;
    case 0x08: CPU_VR4300(tgei, RS, IMM16); break;
    case 0x09: CPU_VR4300(tgeiu, RS, IMM16); break;
    case 0x0A: CPU_VR4300(tlti, RS, IMM16); break;
    case 0x0B: CPU_VR4300(tltiu, RS, IMM16); break;
    case 0x0C: CPU_VR4300(teqi, RS, IMM16); break;
    case 0x0E: CPU_VR4300(tnei, RS, IMM16); break;
    case 0x10: CPU(bltzal, RS, IMM16); break;
    case 0x11: CPU(bgezal, RS, IMM16); break;
    case 0x12: CPU_VR4300(bltzall, RS, IMM16); break;
    case 0x13: CPU_VR4300(bgezall, RS, IMM16); break;
    default: reserved_instruction<cpu, cpu_impl, make_string>(instr);
    }
}

template<Cpu cpu, CpuImpl cpu_impl, bool make_string> void reserved_instruction(u32 instr)
{
    if constexpr (make_string) {
    } else if constexpr (cpu == Cpu::VR4300) {
        if constexpr (cpu_impl == CpuImpl::Interpreter) {
            vr4300::ReservedInstructionException();
        } else {
            vr4300::OnReservedInstruction();
        }
    } else {
        rsp::NotifyIllegalInstrCode(instr);
    }
}

template<Cpu cpu, CpuImpl cpu_impl, bool make_string> void special(u32 instr)
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
    case 0x0C: CPU_VR4300(syscall); break;
    case 0x0D: CPU(break_); break;
    case 0x0F: CPU_VR4300(sync); break;
    case 0x10: CPU_VR4300(mfhi, RD); break;
    case 0x11: CPU_VR4300(mthi, RS); break;
    case 0x12: CPU_VR4300(mflo, RD); break;
    case 0x13: CPU_VR4300(mtlo, RS); break;
    case 0x14: CPU_VR4300(dsllv, RS, RT, RD); break;
    case 0x16: CPU_VR4300(dsrlv, RS, RT, RD); break;
    case 0x17: CPU_VR4300(dsrav, RS, RT, RD); break;
    case 0x18: CPU_VR4300(mult, RS, RT); break;
    case 0x19: CPU_VR4300(multu, RS, RT); break;
    case 0x1A: CPU_VR4300(div, RS, RT); break;
    case 0x1B: CPU_VR4300(divu, RS, RT); break;
    case 0x1C: CPU_VR4300(dmult, RS, RT); break;
    case 0x1D: CPU_VR4300(dmultu, RS, RT); break;
    case 0x1E: CPU_VR4300(ddiv, RS, RT); break;
    case 0x1F: CPU_VR4300(ddivu, RS, RT); break;
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
    case 0x2C: CPU_VR4300(dadd, RS, RT, RD); break;
    case 0x2D: CPU_VR4300(daddu, RS, RT, RD); break;
    case 0x2E: CPU_VR4300(dsub, RS, RT, RD); break;
    case 0x2F: CPU_VR4300(dsubu, RS, RT, RD); break;
    case 0x30: CPU_VR4300(tge, RS, RT); break;
    case 0x31: CPU_VR4300(tgeu, RS, RT); break;
    case 0x32: CPU_VR4300(tlt, RS, RT); break;
    case 0x33: CPU_VR4300(tltu, RS, RT); break;
    case 0x34: CPU_VR4300(teq, RS, RT); break;
    case 0x36: CPU_VR4300(tne, RS, RT); break;
    case 0x38: CPU_VR4300(dsll, RT, RD, SA); break;
    case 0x3A: CPU_VR4300(dsrl, RT, RD, SA); break;
    case 0x3B: CPU_VR4300(dsra, RT, RD, SA); break;
    case 0x3C: CPU_VR4300(dsll32, RT, RD, SA); break;
    case 0x3E: CPU_VR4300(dsrl32, RT, RD, SA); break;
    case 0x3F: CPU_VR4300(dsra32, RT, RD, SA); break;
    default: reserved_instruction<cpu, cpu_impl, make_string>(instr);
    }
}

u32 vt_e_bug(u32 vt_e, u32 vd_e)
{
    (void)vt_e;
    (void)vd_e;
    // if (vt_e & 7) {
    //     bool vt_e_bit_3 = vt_e & 8;
    //     vt_e &= 7;
    //     if (vt_e_bit_3 == 0) {
    //         if (vt_e & 4) vt_e = vd_e & 4 | vt_e & 3;
    //         else if (vt_e & 2) vt_e = vd_e & 6 | vt_e & 1;
    //         else vt_e = vd_e;
    //     }
    // } else {
    //     vt_e = 0; /* effectively clear bit 3 */
    // }
    return vt_e;
}

} // namespace n64
