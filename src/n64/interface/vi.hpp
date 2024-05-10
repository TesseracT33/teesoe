#pragma once

#include "numtypes.hpp"

namespace n64::vi {

enum Register {
    Ctrl,
    Origin,
    Width,
    VIntr,
    VCurrent,
    Burst,
    VSync,
    HSync,
    HSyncLeap,
    HVideo,
    VVideo,
    VBurst,
    XScale,
    YScale,
    TestAddr,
    StagedData
};

struct Registers {
    u32 ctrl, origin, width, v_intr, v_current, burst, v_sync, h_sync, h_sync_leap, h_video, v_video, v_burst, x_scale,
      y_scale, test_addr, staged_data;
};

void AddInitialEvents();
void Initialize();
Registers const& ReadAllRegisters();
u32 ReadReg(u32 addr);
void WriteReg(u32 addr, u32 data);

} // namespace n64::vi
