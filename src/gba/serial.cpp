#include "serial.hpp"

namespace gba::serial {

void Initialize()
{
}

template<std::integral Int> Int ReadReg(u32 addr)
{
    return Int(0);
}

template<std::integral Int> void WriteReg(u32 addr, Int data)
{
}

template u8 ReadReg<u8>(u32);
template s8 ReadReg<s8>(u32);
template u16 ReadReg<u16>(u32);
template s16 ReadReg<s16>(u32);
template u32 ReadReg<u32>(u32);
template s32 ReadReg<s32>(u32);
template void WriteReg<u8>(u32, u8);
template void WriteReg<s8>(u32, s8);
template void WriteReg<u16>(u32, u16);
template void WriteReg<s16>(u32, s16);
template void WriteReg<u32>(u32, u32);
template void WriteReg<s32>(u32, s32);

} // namespace gba::serial
