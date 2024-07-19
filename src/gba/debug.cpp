#include "debug.hpp"

#include <format>
#include <fstream>
#include <optional>

namespace gba {

static std::string FormatCPSR(u32 cpsr);
static std::string FormatRegisters(std::span<u32 const, 16> reg);
static std::ofstream log;

std::string FormatCPSR(u32 cpsr)
{
    static constexpr std::array flag_strs = {
        "nzcv",
        "nzcV",
        "nzCv",
        "nzCV",
        "nZcv",
        "nZcV",
        "nZCv",
        "nZCV",
        "Nzcv",
        "NzcV",
        "NzCv",
        "NzCV",
        "NZcv",
        "NZcV",
        "NZCv",
        "NZCV",
    };
    static constexpr std::array ift_strs = { "ift", "ifT", "iFt", "iFT", "Ift", "IfT", "IFt", "IFT" };
    return std::format("{}/{}/{:02X}", flag_strs[cpsr >> 28], ift_strs[cpsr >> 5 & 7], cpsr & 0x1F);
}

std::string FormatRegisters(std::span<u32 const, 16> reg)
{
    return std::format("r0:{:08X} r1:{:08X} r2:{:08X} r3:{:08X} r4:{:08X} r5:{:08X} r6:{:08X} r7:{:08X} "
                       "r8:{:08X} r9:{:08X} r10:{:08X} r11:{:08X} r12:{:08X} sp:{:08X} lr:{:08X} pc:{:08X}",
      reg[0],
      reg[1],
      reg[2],
      reg[3],
      reg[4],
      reg[5],
      reg[6],
      reg[7],
      reg[8],
      reg[9],
      reg[10],
      reg[11],
      reg[12],
      reg[13],
      reg[14],
      reg[15]);
}

void LogInstruction(u32 pc, u32 opcode, std::string_view cond_str, bool cond, std::span<u32 const, 16> reg, u32 cpsr)
{
    if (!log.is_open()) {
        return;
    }
    log << std::format("{:08X}  ARM:{:08X}  cond:{} ({}) {} cpsr:{}\n",
      pc,
      opcode,
      cond_str,
      int(cond),
      FormatRegisters(reg),
      FormatCPSR(cpsr));
    std::flush(log);
}

void LogInstruction(u32 pc, u16 opcode, std::span<u32 const, 16> reg, u32 cpsr)
{
    if (!log.is_open()) {
        return;
    }
    log << std::format("{:08X}  THUMB:{:04X} {} cpsr:{}\n", pc, opcode, FormatRegisters(reg), FormatCPSR(cpsr));
    std::flush(log);
}

void LogInstruction(u32 pc, std::string instr_output, std::span<u32 const, 16> reg, u32 cpsr)
{
    if (!log.is_open()) {
        return;
    }
    log << std::format("{:08X}  {} {} cpsr:{}\n", pc, instr_output, FormatRegisters(reg), FormatCPSR(cpsr));
    std::flush(log);
}

template<bus::IoOperation op> void LogIoAccess(u32 addr, std::integral auto data)
{
    static_assert(sizeof(data) <= 4);
    if (!log.is_open()) {
        return;
    }
    std::optional<std::string_view> opt_io_reg_str = bus::IoAddrToStr(addr);
    std::string_view op_fmt = [&] {
        if constexpr (op == bus::IoOperation::Read) return "=>";
        else return "<=";
    }();
    std::string_view data_fmt = [&] {
        if constexpr (sizeof(data) == 1) return "byte";
        if constexpr (sizeof(data) == 2) return "hword";
        if constexpr (sizeof(data) == 4) return "word";
    }();
    if (opt_io_reg_str) {
        log << std::format("IO: {} {} {:08X} ({})\n", opt_io_reg_str.value(), op_fmt, u32(data), data_fmt);
    } else {
        log << std::format("IO: {:08X} {} {:08X} ({})\n", addr, op_fmt, u32(data), data_fmt);
    }
    std::flush(log);
}

void SetLogPath(std::string const& log_path)
{
    if (log.is_open()) {
        log.close();
    }
    log.open(log_path, std::ofstream::out | std::ofstream::binary);
}

template void LogIoAccess<bus::IoOperation::Read>(u32, s8);
template void LogIoAccess<bus::IoOperation::Read>(u32, u8);
template void LogIoAccess<bus::IoOperation::Read>(u32, s16);
template void LogIoAccess<bus::IoOperation::Read>(u32, u16);
template void LogIoAccess<bus::IoOperation::Read>(u32, s32);
template void LogIoAccess<bus::IoOperation::Read>(u32, u32);
template void LogIoAccess<bus::IoOperation::Write>(u32, s8);
template void LogIoAccess<bus::IoOperation::Write>(u32, u8);
template void LogIoAccess<bus::IoOperation::Write>(u32, s16);
template void LogIoAccess<bus::IoOperation::Write>(u32, u16);
template void LogIoAccess<bus::IoOperation::Write>(u32, s32);
template void LogIoAccess<bus::IoOperation::Write>(u32, u32);

} // namespace gba
