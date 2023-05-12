#include "../bus.hpp"
#include "arm7tdmi.hpp"
#include "util.hpp"

#define sp (r[13])
#define lr (r[14])
#define pc (r[15])

namespace gba::arm7tdmi {

enum class ThumbAluInstruction {
    ADC,
    AND,
    ASR,
    BIC,
    CMN,
    CMP,
    EOR,
    LSL,
    LSR,
    MUL,
    MVN,
    NEG,
    ORR,
    ROR,
    SBC,
    TST
};

static void AddOffsetToStackPointer(u16 opcode);
static void AddSubtract(u16 opcode);
template<ThumbAluInstruction> static void AluOperation(u16 opcode);
static void ConditionalBranch(u16 opcode);
static void HiReg(u16 opcode);
static void LoadAddress(u16 opcode);
template<bool> static void LoadStoreImmOffset(u16 opcode);
static void LoadStoreHalfword(u16 opcode);
static void LoadStoreRegOffset(u16 opcode);
static void LoadStoreSignExtendedByteHalfword(u16 opcode);
static void LongBranchWithLink(u16 opcode);
static void MoveCompareAddSubtractImm(u16 opcode);
static void MultipleLoadStore(u16 opcode);
static void PcRelativeLoad(u16 opcode);
static void PushPopRegisters(u16 opcode);
static void Shift(u16 opcode);
static void SpRelativeLoadStore(u16 opcode);
static void UnconditionalBranch(u16 opcode);

constexpr std::string_view ThumbAluInstructionToStr(ThumbAluInstruction instr)
{
    using enum ThumbAluInstruction;
    switch (instr) {
    case ADC: return "ADC";
    case AND: return "AND";
    case ASR: return "ASR";
    case BIC: return "BIC";
    case CMN: return "CMN";
    case CMP: return "CMP";
    case EOR: return "EOR";
    case LSL: return "LSL";
    case LSR: return "LSR";
    case MUL: return "MUL";
    case MVN: return "MVN";
    case NEG: return "NEG";
    case ORR: return "ORR";
    case ROR: return "ROR";
    case SBC: return "SBC";
    case TST: return "TST";
    default: assert(false); return "";
    }
}

void DecodeExecuteTHUMB(u16 opcode)
{
    switch (opcode >> 12 & 0xF) {
    case 0b0000:
    case 0b0001: (opcode & 0x1800) == 0x1800 ? AddSubtract(opcode) : Shift(opcode); break;

    case 0b0010:
    case 0b0011: MoveCompareAddSubtractImm(opcode); break;

    case 0b0100:
        if (opcode & 0x800) {
            PcRelativeLoad(opcode);
        } else if (opcode & 0x400) {
            HiReg(opcode);
        } else {
            using enum ThumbAluInstruction;
            switch (opcode >> 6 & 0xF) {
            case 0: AluOperation<AND>(opcode); break;
            case 1: AluOperation<EOR>(opcode); break;
            case 2: AluOperation<LSL>(opcode); break;
            case 3: AluOperation<LSR>(opcode); break;
            case 4: AluOperation<ASR>(opcode); break;
            case 5: AluOperation<ADC>(opcode); break;
            case 6: AluOperation<SBC>(opcode); break;
            case 7: AluOperation<ROR>(opcode); break;
            case 8: AluOperation<TST>(opcode); break;
            case 9: AluOperation<NEG>(opcode); break;
            case 10: AluOperation<CMP>(opcode); break;
            case 11: AluOperation<CMN>(opcode); break;
            case 12: AluOperation<ORR>(opcode); break;
            case 13: AluOperation<MUL>(opcode); break;
            case 14: AluOperation<BIC>(opcode); break;
            case 15: AluOperation<MVN>(opcode); break;
            default: std::unreachable();
            }
        }
        break;

    case 0b0101: opcode & 0x200 ? LoadStoreSignExtendedByteHalfword(opcode) : LoadStoreRegOffset(opcode); break;

    case 0b0110: LoadStoreImmOffset<0>(opcode); break;

    case 0b0111: LoadStoreImmOffset<1>(opcode); break;

    case 0b1000: LoadStoreHalfword(opcode); break;

    case 0b1001: SpRelativeLoadStore(opcode); break;

    case 0b1010: LoadAddress(opcode); break;

    case 0b1011: opcode & 0x400 ? PushPopRegisters(opcode) : AddOffsetToStackPointer(opcode); break;

    case 0b1100: MultipleLoadStore(opcode); break;

    case 0b1101: (opcode & 0xF00) == 0xF00 ? SoftwareInterrupt() : ConditionalBranch(opcode); break;

    case 0b1110: UnconditionalBranch(opcode); break;

    case 0b1111: LongBranchWithLink(opcode); break;

    default: std::unreachable();
    }
}

void Shift(u16 opcode) /* Format 1: ASR, LSL, LSR */
{
    auto rd = opcode & 7;
    auto rs = opcode >> 3 & 7;
    auto shift_amount = opcode >> 6 & 0x1F;
    auto op = opcode >> 11 & 3;

    auto result = [&] {
        switch (op) {
        case 0b00: /* LSL */
            if (shift_amount == 0) {
                /* LSL#0: No shift performed, ie. directly Rd=Rs, the C flag is NOT affected. */
                return r[rs];
            } else {
                cpsr.carry = get_bit(r[rs], 32 - shift_amount);
                return r[rs] << shift_amount;
            }

        case 0b01: /* LSR */
            if (shift_amount == 0) {
                /* LSR#0: Interpreted as LSR#32, ie. Rd becomes zero, C becomes Bit 31 of Rs */
                cpsr.carry = get_bit(r[rs], 31);
                return 0u;
            } else {
                cpsr.carry = get_bit(r[rs], shift_amount - 1);
                return u32(r[rs]) >> shift_amount;
            }

        case 0b10: /* ASR */
            if (shift_amount == 0) {
                /* ASR#0: Interpreted as ASR#32, ie. Rd and C are filled by Bit 31 of Rs. */
                cpsr.carry = get_bit(r[rs], 31);
                return cpsr.carry ? 0xFFFF'FFFF : 0;
            } else {
                cpsr.carry = get_bit(r[rs], shift_amount - 1);
                return u32(s32(r[rs]) >> shift_amount);
            }

        default: /* 0b11 => ADD/SUB; already covered by other function */ std::unreachable();
        }
    }();

    r[rd] = result;
    cpsr.zero = result == 0;
    cpsr.negative = get_bit(result, 31);
}

void AddSubtract(u16 opcode) /* Format 2: ADD, SUB */
{
    auto rd = opcode & 7;
    auto rs = opcode >> 3 & 7;
    auto offset = opcode >> 6 & 7;
    bool op = opcode >> 9 & 1;
    bool reg_or_imm = opcode >> 10 & 1; /* 0=Register; 1=Immediate */

    u32 oper1 = r[rs];
    u32 oper2 = reg_or_imm ? offset : r[offset];

    auto result = [&] {
        if (op == 0) { /* ADD */
            u64 result = u64(oper1) + u64(oper2);
            cpsr.carry = result > std::numeric_limits<u32>::max();
            cpsr.overflow = get_bit((oper1 ^ result) & (oper2 ^ result), 31);
            return u32(result);
        } else { /* SUB */
            u32 result = oper1 - oper2;
            cpsr.carry = oper2 <= oper1; /* this is not borrow */
            cpsr.overflow = get_bit((oper1 ^ oper2) & (oper1 ^ result), 31);
            return result;
        }
    }();

    r[rd] = result;
    cpsr.zero = result == 0;
    cpsr.negative = get_bit(result, 31);
}

void MoveCompareAddSubtractImm(u16 opcode) /* Format 3: ADD, CMP, MOV, SUB */
{
    u8 imm = opcode & 0xFF;
    auto rd = opcode >> 8 & 7;
    auto op = opcode >> 11 & 3;

    switch (op) {
    case 0b00: /* MOV */
        r[rd] = imm;
        cpsr.negative = 0;
        cpsr.zero = r[rd] == 0;
        break;

    case 0b01: { /* CMP */
        u32 result = r[rd] - imm;
        cpsr.overflow = get_bit((r[rd] ^ imm) & (r[rd] ^ result), 31);
        cpsr.carry = imm <= r[rd];
        cpsr.negative = get_bit(result, 31);
        cpsr.zero = result == 0;
        break;
    }

    case 0b10: { /* ADD */
        u64 result = u64(r[rd]) + u64(imm);
        cpsr.overflow = get_bit((r[rd] ^ result) & (imm ^ result), 31);
        cpsr.carry = result > std::numeric_limits<u32>::max();
        cpsr.negative = get_bit(result, 31);
        cpsr.zero = u32(result) == 0;
        r[rd] = u32(result);
        break;
    }

    case 0b11: { /* SUB */
        u32 result = r[rd] - imm;
        cpsr.overflow = get_bit((r[rd] ^ imm) & (r[rd] ^ result), 31);
        cpsr.carry = imm <= r[rd];
        cpsr.negative = get_bit(result, 31);
        cpsr.zero = result == 0;
        r[rd] = result;
        break;
    }

    default: std::unreachable();
    }
}

template<ThumbAluInstruction instr>
void AluOperation(
  u16 opcode) /* Format 4: ADC, AND, ASR, BIC, CMN, CMP, EOR, LSL, LSR, MUL, MVN, NEG, ORR, ROR, SBC, TST */
{
    using enum ThumbAluInstruction;

    static constexpr bool is_arithmetic_instr = one_of(instr, ADC, CMN, CMP, NEG, SBC);

    auto rd = opcode & 7;
    auto rs = opcode >> 3 & 7;
    auto op1 = r[rd];
    auto op2 = r[rs];

    /* Affected Flags:
        N,Z,C,V for  ADC,SBC,NEG,CMP,CMN
        N,Z,C   for  LSL,LSR,ASR,ROR (carry flag unchanged if zero shift amount)
        N,Z,C   for  MUL on ARMv4 and below: carry flag destroyed
        N,Z     for  MUL on ARMv5 and above: carry flag unchanged
        N,Z     for  AND,EOR,TST,ORR,BIC,MVN
    */

    auto result = [&] {
        if constexpr (instr == ADC) {
            u64 result = u64(op1) + u64(op2) + u64(cpsr.carry);
            cpsr.carry = result > std::numeric_limits<u32>::max();
            return u32(result);
        }
        if constexpr (instr == AND || instr == TST) {
            return op1 & op2;
        }
        if constexpr (instr == ASR) {
            auto shift_amount = op2 & 0xFF;
            if (shift_amount == 0) {
                return op1;
            } else if (shift_amount < 32) {
                cpsr.carry = get_bit(op1, shift_amount - 1);
                return u32(s32(op1) >> shift_amount);
            } else {
                bool bit31 = get_bit(op1, 31);
                cpsr.carry = bit31;
                return bit31 ? 0xFFFF'FFFFu : 0u;
            }
        }
        if constexpr (instr == BIC) {
            return op1 & ~op2;
        }
        if constexpr (instr == CMN) {
            u64 result = u64(op1) + u64(op2);
            cpsr.carry = result > std::numeric_limits<u32>::max();
            return u32(result);
        }
        if constexpr (instr == CMP) {
            cpsr.carry = op2 <= op1;
            return op1 - op2;
        }
        if constexpr (instr == EOR) {
            return op1 ^ op2;
        }
        if constexpr (instr == LSL) {
            auto shift_amount = op2 & 0xFF;
            if (shift_amount == 0) {
                return op1;
            } else if (shift_amount < 32) {
                cpsr.carry = get_bit(op1, 32 - shift_amount);
                return op1 << shift_amount;
            } else {
                cpsr.carry = shift_amount == 32 ? get_bit(op1, 0) : 0;
                return 0u;
            }
        }
        if constexpr (instr == LSR) {
            auto shift_amount = op2 & 0xFF;
            if (shift_amount == 0) {
                return op1;
            } else if (shift_amount < 32) {
                cpsr.carry = get_bit(op1, shift_amount - 1);
                return u32(op1) >> shift_amount;
            } else {
                cpsr.carry = shift_amount > 32 ? 0 : get_bit(op1, 31);
                return 0u;
            }
        }
        if constexpr (instr == MUL) {
            cpsr.carry = 0;
            return op1 * op2;
        }
        if constexpr (instr == MVN) {
            return ~op2;
        }
        if constexpr (instr == NEG) {
            cpsr.carry = op2 == 0; /* TODO: unsure */
            return u32(-s32(op2));
        }
        if constexpr (instr == ORR) {
            return op1 | op2;
        }
        if constexpr (instr == ROR) {
            auto shift_amount = op2 & 0xFF;
            if (shift_amount == 0) {
                return op1;
            } else {
                cpsr.carry = op1 >> ((shift_amount - 1) & 0x1F) & 1;
                return std::rotr(op1, shift_amount);
            }
        }
        if constexpr (instr == SBC) {
            auto result = op1 - op2 - !cpsr.carry;
            cpsr.carry = u64(op2) + u64(!cpsr.carry) <= u64(op1);
            return result;
        }
    }();

    if constexpr (!one_of(instr, CMP, CMN, TST)) {
        r[rd] = result;
    }
    cpsr.zero = result == 0;
    cpsr.negative = get_bit(result, 31);
    if constexpr (is_arithmetic_instr) {
        auto cond = [&] {
            if constexpr (instr == ADC || instr == CMN) return (op1 ^ result) & (op2 ^ result);
            if constexpr (instr == CMP || instr == SBC) return (op1 ^ op2) & (op1 ^ result);
            if constexpr (instr == NEG) return op2 & result; /* SUB with op1 == 0 */
        }();
        cpsr.overflow = get_bit(cond, 31);
    }
}

void HiReg(u16 opcode) /* Format 5: ADD, BX, CMP, MOV */
{
    auto rs = opcode >> 3 & 7;
    auto h2 = opcode >> 6 & 1;
    auto op = opcode >> 8 & 3;
    rs += h2 << 3; /* add 8 to register indeces if h flags are set */
    auto oper = r[rs];
    if (rs == 15) { /* If R15 is used as an operand, the value will be the address of the instruction + 4 with bit 0
                       cleared. */
        oper &= ~1;
    }
    /* In this group only CMP (Op = 01) sets the CPSR condition codes. */
    switch (op) {
    case 0b00: { /* ADD */
        auto rd = opcode & 7;
        auto h1 = opcode >> 7 & 1;
        rd += h1 << 3;
        r[rd] += oper;
        if (rd == 15) {
            pc &= ~1;
            FlushPipeline();
        }
        break;
    }

    case 0b01: { /* CMP */
        auto rd = opcode & 7;
        auto h1 = opcode >> 7 & 1;
        rd += h1 << 3;
        auto result = r[rd] - oper;
        cpsr.overflow = get_bit((r[rd] ^ oper) & (r[rd] ^ result), 31);
        cpsr.carry = oper <= r[rd];
        cpsr.zero = result == 0;
        cpsr.negative = get_bit(result, 31);
        break;
    }

    case 0b10: { /* MOV */
        auto rd = opcode & 7;
        auto h1 = opcode >> 7 & 1;
        rd += h1 << 3;
        r[rd] = oper;
        if (rd == 15) {
            pc &= ~1;
            FlushPipeline();
        }
        break;
    }

    case 0b11: /* BX */
        pc = oper;
        if (pc & 1) { /* Bit 0 of the address determines the processor state on entry to the routine */
            SetExecutionState(ExecutionState::THUMB);
            pc &= ~1;
        } else {
            SetExecutionState(ExecutionState::ARM);
            pc &= ~3;
        }
        FlushPipeline();
        break;

    default: std::unreachable();
    }
}

void PcRelativeLoad(u16 opcode) /* Format 6: LDR */
{
    u32 offset = (opcode & 0xFF) << 2;
    auto rd = opcode >> 8 & 7;
    r[rd] = bus::Read<u32>((pc & ~2) + offset); /* The PC will be 4 bytes greater than the address of this instruction,
                                                   but bit 1 of the PC is forced to 0 to ensure it is word aligned. */
}

void LoadStoreRegOffset(u16 opcode) /* Format 7: LDR, LDRB, STR, STRB */
{
    auto rd = opcode & 7;
    auto rb = opcode >> 3 & 7;
    auto ro = opcode >> 6 & 7;
    bool byte_or_word = opcode >> 10 & 1; /* 0: word; 1: byte */
    bool load_or_store = opcode >> 11 & 1; /* 0: store; 1: load */
    auto addr = r[rb] + r[ro];
    if (load_or_store == 0) { /* store */
        byte_or_word ? bus::Write<u8>(addr, u8(r[rd])) : bus::Write<u32>(addr, r[rd]);
    } else { /* load */
        r[rd] = byte_or_word ? bus::Read<u8>(addr) : bus::Read<u32>(addr);
    }
}

void LoadStoreSignExtendedByteHalfword(u16 opcode) /* Format 8: LDSB, LDRH, LDSH, STRH */
{
    auto rd = opcode & 7;
    auto rb = opcode >> 3 & 7;
    auto ro = opcode >> 6 & 7;
    auto op = opcode >> 10 & 3;
    auto addr = r[rb] + r[ro];
    switch (op) {
    case 0b00: /* Store halfword */ bus::Write<s16>(addr, s16(r[rd])); break;

    case 0b01: /* Load sign-extended byte */ r[rd] = bus::Read<s8>(addr); break;

    case 0b10: /* Load zero-extended halfword */ r[rd] = bus::Read<u16>(addr); break;

    case 0b11: /* Load sign-extended halfword */ r[rd] = bus::Read<s16>(addr); break;

    default: std::unreachable();
    }
}

template<bool byte_or_word /* 0: word; 1: byte */>
void LoadStoreImmOffset(u16 opcode) /* Format 9: LDR, LDRB, STR, STRB */
{
    auto rd = opcode & 7;
    auto rb = opcode >> 3 & 7;
    bool load_or_store = opcode >> 11 & 1; /* 0: store; 1: load */
    /* unsigned offset is 0-31 for byte, 0-124 (step 4) for word */
    if constexpr (byte_or_word == 0) { /* word */
        auto offset = opcode >> 4 & 0x7C; /* == (opcode >> 6 & 0x1F) << 2 */
        auto addr = r[rb] + offset;
        if (load_or_store == 0) {
            bus::Write<u32>(addr, r[rd]);
        } else {
            r[rd] = bus::Read<u32>(addr);
        }
    } else { /* byte */
        auto offset = opcode >> 6 & 0x1F;
        auto addr = r[rb] + offset;
        if (load_or_store == 0) {
            bus::Write<u8>(addr, u8(r[rd]));
        } else {
            r[rd] = bus::Read<u8>(addr);
        }
    }
}

void LoadStoreHalfword(u16 opcode) /* Format 10: LDRH, STRH */
{
    auto rd = opcode & 7;
    auto rb = opcode >> 3 & 7;
    auto offset = opcode >> 5 & 0x3E; /* == (opcode >> 6 & 0x1F) << 1 */
    bool load_or_store = opcode >> 11 & 1; /* 0: store; 1: load */
    auto addr = r[rb] + offset;
    if (load_or_store == 0) {
        bus::Write<u16>(addr, r[rd]);
    } else {
        r[rd] = bus::Read<u16>(addr);
    }
}

void SpRelativeLoadStore(u16 opcode) /* Format 11: LDR, STR */
{
    u8 immediate = opcode & 0xFF;
    auto rd = opcode >> 8 & 7;
    bool load_or_store = opcode >> 11 & 1; /* 0: store; 1: load */
    auto addr = sp + (immediate << 2);
    if (load_or_store == 0) {
        bus::Write<u32>(addr, r[rd]);
    } else {
        r[rd] = bus::Read<u32>(addr);
    }
}

void LoadAddress(u16 opcode) /* Format 12: ADD Rd,PC,#nn; ADD Rd,SP,#nn */
{
    u8 immediate = opcode & 0xFF;
    auto rd = opcode >> 8 & 7;
    auto src = opcode >> 11 & 1; /* 0: PC (r15); 1: SP (r13) */
    if (src == 0) {
        r[rd] = (pc & ~2) + (immediate << 2);
    } else {
        r[rd] = sp + (immediate << 2);
    }
}

void AddOffsetToStackPointer(u16 opcode) /* Format 13: ADD SP,#nn */
{
    s16 offset = (opcode & 0x7F) << 2;
    bool sign = opcode >> 7 & 1; /* 0: positive offset; 1: negative offset */
    if (sign == 1) {
        offset = -offset; /* [-508, 508] in steps of 4 */
    }
    sp += offset;
}

void PushPopRegisters(u16 opcode) /* Format 14: PUSH, POP */
{
    auto reg_list = opcode & 0xFF;
    bool transfer_lr_pc = opcode >> 8 & 1; /* 0: Do not store LR / load PC; 1: Store LR / Load PC */
    bool load_or_store = opcode >> 11 & 1; /* 0: store; 1: load */

    auto LoadReg = [&] {
        u32 ret = bus::Read<u32>(sp);
        sp += 4;
        return ret;
    };

    auto StoreReg = [&](u32 reg) {
        sp -= 4;
        bus::Write<u32>(sp, reg);
    };

    /* The lowest register gets transferred to/from the lowest memory address. */
    if (load_or_store == 0) {
        if (transfer_lr_pc) {
            StoreReg(lr);
        }
        for (int i = 7; i >= 0; --i) {
            if (reg_list & 1 << i) {
                StoreReg(r[i]);
            }
        }
    } else {
        for (int i = 0; i < 8; ++i) {
            if (reg_list & 1 << i) {
                r[i] = LoadReg();
            }
        }
        if (transfer_lr_pc) {
            pc = LoadReg() & ~1;
            FlushPipeline();
        }
    }
}

void MultipleLoadStore(u16 opcode) /* Format 15: LDMIA, STMIA */
{
    auto reg_list = opcode & 0xFF;
    auto rb = opcode >> 8 & 7;
    bool load_or_store = opcode >> 11 & 1; /* 0: store; 1: load */
    /* Strange Effects on Invalid Rlist's
        * Empty Rlist: R15 loaded/stored (ARMv4 only), and Rb=Rb+40h (ARMv4-v5).
        * Writeback with Rb included in Rlist: Store OLD base if Rb is FIRST entry in Rlist,
        otherwise store NEW base (STM/ARMv4). Always store OLD base (STM/ARMv5), no writeback (LDM/ARMv4/ARMv5).
        TODO: emulate 2nd point
    */
    if (load_or_store) {
        if (reg_list == 0) {
            pc = bus::Read<u32>(r[rb]);
            r[rb] += 0x40;
            FlushPipeline();
        } else {
            for (int i = 0; i < 8; ++i) {
                if (reg_list & 1 << i) {
                    r[i] = bus::Read<u32>(r[rb]);
                    r[rb] += 4;
                }
            }
        }
    } else {
        if (reg_list == 0) {
            pc = bus::Read<u32>(r[rb]);
            r[rb] += 0x40;
            FlushPipeline();
        } else {
            for (int i = 0; i < 8; ++i) {
                if (reg_list & 1 << i) {
                    bus::Write<u32>(r[rb], r[i]);
                    r[rb] += 4;
                }
            }
        }
    }
}

void ConditionalBranch(u16 opcode) /* Format 16: BEQ, BNE, BCS, BCC, BMI, BPL, BVS, BVC, BHI, BLS, BGE, BLT, BGT, BLE */
{
    auto cond = opcode >> 8 & 0xF;
    bool branch = CheckCondition(cond);
    if (branch) {
        s32 offset = sign_extend<s32, 9>(opcode << 1 & 0x1FE); /* [-256, 254] in steps of 2 */
        pc += offset;
        FlushPipeline();
    }
}

void SoftwareInterrupt() /* Format 17: SWI */
{
    SignalException<Exception::SoftwareInterrupt>();
}

void UnconditionalBranch(u16 opcode) /* Format 18: B */
{
    s32 offset = sign_extend<s32, 12>(opcode << 1 & 0xFFE); /* [-2048, 2046] in steps of 2 */
    pc += offset;
    FlushPipeline();
}

void LongBranchWithLink(u16 opcode) /* Format 19: BL */
{
    auto immediate = opcode & 0x7FF;
    bool low_or_high_offset = opcode >> 11 & 1; /* 0: offset high; 1: offset low */
    if (low_or_high_offset == 0) {
        s32 offset = sign_extend<s32, 23>(immediate << 12);
        lr = pc + offset;
    } else {
        s32 offset = immediate << 1;
        lr += offset;
        auto prev_pc = pc;
        pc = lr;
        lr = (prev_pc - 2)
           | 1; /* the address of the instruction following the BL is placed in LR and bit 0 of LR is set */
        FlushPipeline();
    }
}
} // namespace gba::arm7tdmi
