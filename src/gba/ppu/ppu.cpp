#include "ppu.hpp"
#include "../dma.hpp"
#include "../irq.hpp"
#include "../scheduler.hpp"

namespace gba::ppu {

void AddInitialEvents()
{
    scheduler::AddEvent(scheduler::EventType::HBlank, cycles_until_hblank, OnHBlank);
}

void Initialize()
{
    dispcnt = {};
    green_swap = {};
    dispstat = {};
    v_counter = {};
    bgcnt = {};
    bghofs = {};
    bgvofs = {};
    bgpa = {};
    bgpb = {};
    bgpc = {};
    bgpd = {};
    bgx = {};
    bgy = {};
    winh_x1 = {};
    winh_x2 = {};
    winv_y1 = {};
    winv_y2 = {};
    winin = {};
    winout = {};
    mosaic = {};
    bldcnt = {};
    eva = {};
    evb = {};
    evy = {};
    oam = {};
    palette_ram = {};
    vram = {};
    objects.clear();
    objects.reserve(128);
    in_hblank = in_vblank = false;
    cycle = dot = framebuffer_index = 0;

    framebuffer.resize(framebuffer_width * framebuffer_height * 3, 0);
    // Video::SetFramebufferPtr(framebuffer.data());
    // Video::SetFramebufferSize(framebuffer_width, framebuffer_height);
    // Video::SetPixelFormat(Video::PixelFormat::RGB888);
}

void OnHBlank()
{
    scheduler::AddEvent(scheduler::EventType::HBlankSetFlag,
      cycles_until_set_hblank_flag - cycles_until_hblank,
      OnHBlankSetFlag);
    in_hblank = true;
    dma::OnHBlank();
}

void OnHBlankSetFlag()
{
    scheduler::AddEvent(scheduler::EventType::NewScanline,
      cycles_per_line - cycles_until_set_hblank_flag,
      OnNewScanline);
    dispstat.hblank = 1;
    if (dispstat.hblank_irq_enable) { /* TODO: here or in OnHBlank? */
        irq::Raise(irq::Source::HBlank);
    }
}

void OnNewScanline()
{
    if (v_counter < lines_until_vblank) {
        RenderScanline();
    }
    scheduler::AddEvent(scheduler::EventType::HBlank, cycles_until_hblank, OnHBlank);
    dispstat.hblank = in_hblank = false;
    ++v_counter;
    if (dispstat.v_counter_irq_enable) {
        bool prev_v_counter_match = dispstat.v_counter_match;
        dispstat.v_counter_match = v_counter == dispstat.v_count_setting;
        if (dispstat.v_counter_match && !prev_v_counter_match) {
            irq::Raise(irq::Source::VCounter);
        }
    } else {
        dispstat.v_counter_match = v_counter == dispstat.v_count_setting;
    }
    if (v_counter < lines_until_vblank) {
        UpdateRotateScalingRegisters();
    } else if (v_counter == lines_until_vblank) {
        framebuffer_index = 0;
        // Video::NotifyNewGameFrameReady();
        dispstat.vblank = in_vblank = true;
        if (dispstat.vblank_irq_enable) {
            irq::Raise(irq::Source::VBlank);
        }
        dma::OnVBlank();
    } else if (v_counter == total_num_lines - 1) {
        dispstat.vblank = 0; /* not set in the last line */
        in_vblank = false;
        // bg_rot_coord_x = 0x0FFF'FFFF & std::bit_cast<u64>(bgx);
        // bg_rot_coord_y = 0x0FFF'FFFF & std::bit_cast<u64>(bgy);
    } else {
        v_counter %= total_num_lines; /* cheaper than comparison on ly == total_num_lines to set ly = 0? */
    }
}

void StreamState(Serializer& stream)
{
}

void UpdateRotateScalingRegisters()
{
    // s16 dmx = 0;
    // bg_rot_coord_x = (bg_rot_coord_x + dmx) & 0x0FFF'FFF;
    // s16 dmy = 0;
    // bg_rot_coord_y = (bg_rot_coord_y + dmy) & 0x0FFF'FFF;
}
} // namespace gba::ppu
