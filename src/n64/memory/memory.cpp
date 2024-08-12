#include "memory.hpp"
#include "cart.hpp"
#include "interface/ai.hpp"
#include "interface/mi.hpp"
#include "interface/pi.hpp"
#include "interface/ri.hpp"
#include "interface/si.hpp"
#include "interface/vi.hpp"
#include "log.hpp"
#include "n64_build_options.hpp"
#include "pif.hpp"
#include "rdp/rdp.hpp"
#include "rdram.hpp"
#include "rsp/rsp.hpp"

#include <bit>
#include <format>

namespace n64::memory {

#define READ_INTERFACE(io, INT, addr)                                                                             \
    [=] {                                                                                                         \
        if constexpr (sizeof(INT) == 4) {                                                                         \
            return io::ReadReg(addr);                                                                             \
        } else {                                                                                                  \
            LogWarn(                                                                                              \
              std::format("Attempted to read IO region at address ${:08X} for sized int {}", addr, sizeof(INT))); \
            return INT{};                                                                                         \
        }                                                                                                         \
    }()

#define WRITE_INTERFACE(io, access_size, addr, data)                                                               \
    do {                                                                                                           \
        if constexpr (access_size == 4) {                                                                          \
            io::WriteReg(addr, u32(data));                                                                         \
        } else {                                                                                                   \
            LogWarn(                                                                                               \
              std::format("Attempted to write IO region at address ${:08X} for sized int {}", addr, access_size)); \
        }                                                                                                          \
    } while (0)

template<std::signed_integral Int> Int Read(u32 addr)
{ /* Precondition: 'addr' is aligned according to the size of 'Int' */
    if (addr <= 0x03EF'FFFF) {
        return rdram::Read<Int>(addr);
    }
    if (addr <= 0x048F'FFFF) {
        switch ((addr >> 20) - 0x3F) {
        case 0: /* $03F0'0000 - $03FF'FFFF */ return READ_INTERFACE(rdram, Int, addr);
        case 1: /* $0400'0000 - $040F'FFFF */ return rsp::ReadMemoryCpu<Int>(addr);
        case 2: /* $0410'0000 - $041F'FFFF */ return READ_INTERFACE(rdp, Int, addr);
        case 3: /* $0420'0000 - $042F'FFFF */
            LogWarn(std::format("Unexpected cpu read to address ${:08X}", addr));
            return Int{};
        case 4: /* $0430'0000 - $043F'FFFF */ return READ_INTERFACE(mi, Int, addr);
        case 5: /* $0440'0000 - $044F'FFFF */ return READ_INTERFACE(vi, Int, addr);
        case 6: /* $0450'0000 - $045F'FFFF */ return READ_INTERFACE(ai, Int, addr);
        case 7: /* $0460'0000 - $046F'FFFF */ return READ_INTERFACE(pi, Int, addr);
        case 8: /* $0470'0000 - $047F'FFFF */ return READ_INTERFACE(ri, Int, addr);
        case 9: /* $0480'0000 - $048F'FFFF */ return READ_INTERFACE(si, Int, addr);
        default: std::unreachable();
        }
    }
    if (addr >= 0x800'0000 && addr <= 0xFFF'FFFF) {
        return cart::ReadSram<Int>(addr);
    }
    if (addr >= 0x1000'0000 && addr <= 0x1FBF'FFFF) {
        return cart::ReadRom<Int>(addr);
    }
    if ((addr & 0xFFFF'F800) == 0x1FC0'0000) { /* $1FC0'0000 - $1FC0'07FF */
        return si::ReadMemory<Int>(addr);
    }
    LogWarn(std::format("Unexpected cpu read from address ${:08X}", addr));
    return Int{};
}

template<size_t access_size, typename... MaskT> void Write(u32 addr, s64 data, MaskT... mask)
{
    static_assert(std::has_single_bit(access_size) && access_size <= 8);
    static_assert(sizeof...(mask) <= 1);
    if (addr <= 0x03EF'FFFF) {
        rdram::Write<access_size>(addr, data, mask...);
    } else if (addr <= 0x048F'FFFF) {
        switch ((addr >> 20) - 0x3F) {
        case 0: /* $03F0'0000 - $03FF'FFFF */ WRITE_INTERFACE(rdram, access_size, addr, data); break;
        case 1: /* $0400'0000 - $040F'FFFF */ rsp::WriteMemoryCpu<access_size>(addr, data); break;
        case 2: /* $0410'0000 - $041F'FFFF */ WRITE_INTERFACE(rdp, access_size, addr, data); break;
        case 3: /* $0420'0000 - $042F'FFFF */
            LogWarn(std::format("Unexpected cpu write to address ${:08X}", addr));
            break;
        case 4: /* $0430'0000 - $043F'FFFF */ WRITE_INTERFACE(mi, access_size, addr, data); break;
        case 5: /* $0440'0000 - $044F'FFFF */ WRITE_INTERFACE(vi, access_size, addr, data); break;
        case 6: /* $0450'0000 - $045F'FFFF */ WRITE_INTERFACE(ai, access_size, addr, data); break;
        case 7: /* $0460'0000 - $046F'FFFF */ WRITE_INTERFACE(pi, access_size, addr, data); break;
        case 8: /* $0470'0000 - $047F'FFFF */ WRITE_INTERFACE(ri, access_size, addr, data); break;
        case 9: /* $0480'0000 - $048F'FFFF */ WRITE_INTERFACE(si, access_size, addr, data); break;
        default: std::unreachable();
        }
    } else if (addr >= 0x800'0000 && addr <= 0xFFF'FFFF) {
        cart::WriteSram<access_size>(addr, data);
    } else if (addr >= 0x1000'0000 && addr <= 0x1FBF'FFFF) {
        cart::WriteRom<access_size>(addr, data);
    } else if ((addr & 0xFFFF'F800) == 0x1FC0'0000) { /* $1FC0'0000 - $1FC0'07FF */
        si::WriteMemory<access_size>(addr, data);
    } else {
        LogWarn(std::format("Unexpected cpu write to address ${:08X}", addr));
    }
}

template s8 Read<s8>(u32);
template s16 Read<s16>(u32);
template s32 Read<s32>(u32);
template s64 Read<s64>(u32);
template void Write<1>(u32, s64);
template void Write<2>(u32, s64);
template void Write<4>(u32, s64);
template void Write<8>(u32, s64);
template void Write<4, u32>(u32, s64, u32);
template void Write<8, u64>(u32, s64, u64);

} // namespace n64::memory
