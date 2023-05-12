#pragma once

#include "serializer.hpp"
#include "types.hpp"

#include <array>
#include <concepts>
#include <vector>

namespace gba::ppu {

void AddInitialEvents();
void Initialize();
template<std::integral Int> Int ReadOam(u32 addr);
template<std::integral Int> Int ReadPaletteRam(u32 addr);
template<std::integral Int> Int ReadReg(u32 addr);
template<std::integral Int> Int ReadVram(u32 addr);
void StreamState(Serializer& stream);
template<std::integral Int> void WriteOam(u32 addr, Int data);
template<std::integral Int> void WritePaletteRam(u32 addr, Int data);
template<std::integral Int> void WriteReg(u32 addr, Int data);
template<std::integral Int> void WriteVram(u32 addr, Int data);

struct RGB {
    u8 r, g, b;
};

struct BgColorData {
    u16 r           : 5;
    u16 g           : 5;
    u16 b           : 5;
    u16 transparent : 1;
    RGB ToRGB() const { return { u8(r), u8(g), u8(b) }; }
};

struct ObjColorData {
    u32 r           : 5;
    u32 g           : 5;
    u32 b           : 5;
    u32 transparent : 1;
    u32 obj_mode    : 2;
    u32 priority    : 2;
    u32             : 12;
    RGB ToRGB() const { return { u8(r), u8(g), u8(b) }; }
};

struct ObjData {
    u16 y_coord                 : 8;
    u16 rotate_scale            : 1;
    u16 double_size_obj_disable : 1;
    u16 obj_mode                : 2;
    u16 obj_mosaic              : 1;
    u16 palette_mode            : 1;
    u16 obj_shape               : 2;

    u16 x_coord         : 9;
    u16 rot_scale_param : 5;
    u16 obj_size        : 2;

    u16 tile_num    : 10;
    u16 priority    : 2;
    u16 palette_num : 4;

    u8 size_x;
    u8 oam_index;
};

RGB AlphaBlend(RGB target_1, RGB target_2);
void BlendLayers();
RGB BrightnessDecrease(RGB pixel);
RGB BrightnessIncrease(RGB pixel);
BgColorData GetBackdropColor();
void OnHBlank();
void OnHBlankSetFlag();
void OnNewScanline();
void PushPixel(auto color_data);
void PushPixel(RGB rgb);
void PushPixel(u8 r, u8 g, u8 b);
RGB Rgb555ToRgb888(RGB rgb);
template<void (*RenderFun)(uint), bool vertical_mosaic> void RenderBackground(uint bg);
template<void (*RenderFun)(), bool vertical_mosaic> void RenderBackground(uint bg);
void RenderTransparentBackground(uint bg);
void RenderScanline();
void ScanlineBackgroundRotateScaleMode(uint bg);
void ScanlineBackgroundTextMode(uint bg);
void ScanlineBackgroundBitmapMode3();
void ScanlineBackgroundBitmapMode4();
void ScanlineBackgroundBitmapMode5();
void ScanlineObjects();
void ScanOam();
void SortBackgroundsAfterPriority();
void UpdateRotateScalingRegisters();

constexpr u16 fx_obj_index = 4;
constexpr u16 fx_backdrop_index = 5;
constexpr u16 fx_disable_mask = 0;
constexpr u16 fx_alpha_blending_mask = 1;
constexpr u16 fx_brightness_increase_mask = 2;
constexpr u16 fx_brightness_decrease_mask = 3;
constexpr u16 obj_mode_normal_mask = 0;
constexpr u16 obj_mode_semi_transparent_mask = 1;
constexpr u16 obj_mode_obj_window_mask = 2;
constexpr uint cycles_per_line = 1232;
constexpr uint cycles_until_hblank = 960;
constexpr uint cycles_until_set_hblank_flag = 1006;
constexpr uint dots_per_line = 240;
constexpr uint framebuffer_height = 160;
constexpr uint framebuffer_pitch = dots_per_line * 3;
constexpr uint framebuffer_width = 240;
constexpr uint framebuffer_size = framebuffer_height * framebuffer_width * 3;
constexpr uint lines_until_vblank = 160;
constexpr uint max_objects = 128;
constexpr uint total_num_lines = 228;
constexpr BgColorData transparent_bg_pixel{ .r = 0, .g = 0, .b = 0, .transparent = true };
constexpr ObjColorData transparent_obj_pixel{ .r = 0, .g = 0, .b = 0, .transparent = true, .obj_mode = 0 };

struct DISPCNT {
    u16 bg_mode               : 3; /* 0-5=Video Mode 0-5, 6-7=Prohibited */
    u16 cgb_mode              : 1; /* 0=GBA, 1=CGB; can be set only by BIOS opcodes */
    u16 display_frame_select  : 1; /* 0-1=Frame 0-1 (for BG Modes 4,5 only) */
    u16 hblank_interval_free  : 1; /* 1=Allow access to OAM during H-Blank */
    u16 obj_char_vram_mapping : 1; /* 0=Two dimensional, 1=One dimensional */
    u16 forced_blank          : 1; /* 1=Allow FAST access to VRAM,Palette,OAM */
    u16 screen_display_bg     : 4; /* 0=Off, 1=On (bg 0,1,2,3) */
    u16 screen_display_obj    : 1; /* 0=Off, 1=On */
    u16 win0_display          : 1; /* 0=Off, 1=On */
    u16 win1_display          : 1; /* 0=Off, 1=On */
    u16 obj_win_display       : 1; /* 0=Off, 1=On */
} inline dispcnt;

inline bool green_swap;

struct DISPSTAT {
    u16 vblank               : 1;
    u16 hblank               : 1;
    u16 v_counter_match      : 1;
    u16 vblank_irq_enable    : 1;
    u16 hblank_irq_enable    : 1;
    u16 v_counter_irq_enable : 1;
    u16                      : 2;
    u16 v_count_setting      : 8;
} inline dispstat;

inline u8 v_counter;

struct BGCNT {
    u16 bg_priority                            : 2; /* 0-3, 0=Highest */
    u16 char_base_block                        : 2; /* 0-3, in units of 16 KBytes (=BG Tile Data) */
    u16                                        : 2;
    u16 mosaic_enable                          : 1; /* 0=Disable, 1=Enable */
    u16 palette_mode                           : 1; /* 0=16/16, 1=256/1 */
    u16 screen_base_block                      : 5; /* 0-31, in units of 2 KBytes (=BG Map Data) */
    u16 ext_palette_slot_display_area_overflow : 1;
    u16 screen_size                            : 2;
};

inline std::array<BGCNT, 4> bgcnt;
inline std::array<u16, 4> bghofs;
inline std::array<u16, 4> bgvofs;

struct BGP {
    u16 fractional : 8;
    u16 integer    : 7;
    u16 sign       : 1;
};

inline std::array<BGP, 2> bgpa, bgpb, bgpc, bgpd;

struct BGXY {
    u32 fractional : 8;
    u32 integer    : 19;
    u32 sign       : 1;
    u32            : 4;
};

inline std::array<BGXY, 2> bgx, bgy;

struct WININ {
    u16 window0_bg_enable            : 4;
    u16 window0_obj_enable           : 1;
    u16 window0_color_special_effect : 1;
    u16                              : 2;
    u16 window1_bg_enable            : 4;
    u16 window1_obj_enable           : 1;
    u16 window1_color_special_effect : 1;
    u16                              : 2;
} inline winin;

struct WINOUT {
    u16 outside_bg_enable               : 4;
    u16 outside_obj_enable              : 1;
    u16 outside_color_special_effect    : 1;
    u16                                 : 2;
    u16 obj_window_bg_enable            : 4;
    u16 obj_window_obj_enable           : 1;
    u16 obj_window_color_special_effect : 1;
    u16                                 : 2;
} inline winout;

struct MOSAIC {
    u16 bg_h_size  : 4;
    u16 bg_v_size  : 4;
    u16 obj_h_size : 4;
    u16 obj_v_size : 4;
} inline mosaic;

struct BLDCNT {
    u16 bg_1st_target_pixel  : 4;
    u16 obj_1st_target_pixel : 1;
    u16 bd_1st_target_pixel  : 1;
    u16 color_special_effect : 2;
    u16 bg_2nd_target_pixel  : 4;
    u16 obj_2nd_target_pixel : 1;
    u16 bd_2nd_target_pixel  : 1;
    u16                      : 2;
} inline bldcnt;

inline u8 eva, evb, evy;
inline std::array<u8, 2> winh_x1, winh_x2, winv_y1, winv_y2;

inline bool in_hblank;
inline bool in_vblank;

inline std::array<u32, 2> bg_rot_coord_x;
inline std::array<u32, 2> bg_rot_coord_y;

inline uint cycle;
inline uint dot;
inline uint framebuffer_index;
inline uint mosaic_v_counter;

inline std::array<std::array<BgColorData, dots_per_line>, 4> bg_render;
inline std::array<ObjColorData, dots_per_line> obj_render;
inline std::array<uint, 4> bg_by_prio = {
    0,
    1,
    2,
    3,
}; // i:th element holds the index of the background with the i:th priority
inline std::array<u8, 0x400> oam;
inline std::array<u8, 0x400> palette_ram;
inline std::array<u8, 0x18000> vram;
inline std::vector<u8> framebuffer;
inline std::vector<ObjData> objects;

} // namespace gba::ppu
