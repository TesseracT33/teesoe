#include "bit.hpp"
#include "ppu.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <utility>

namespace gba::ppu {

RGB AlphaBlend(RGB target_1, RGB target_2)
{
    return { .r = u8(std::min(31, target_1.r * eva + target_2.r * evb)),
        .g = u8(std::min(31, target_1.g * eva + target_2.g * evb)),
        .b = u8(std::min(31, target_1.b * eva + target_2.b * evb)) };
}

void BlendLayers()
{
    bool const win0_enable = dispcnt.win0_display;
    bool const win1_enable = dispcnt.win1_display;
    bool const obj_win_enable = dispcnt.obj_win_display;
    bool const scanline_falls_on_win0 = win0_enable && v_counter >= winv_y1[0] && v_counter < winv_y2[0];
    bool const scanline_falls_on_win1 = win1_enable && v_counter >= winv_y1[1] && v_counter < winv_y2[1];
    bool win0_active = false;
    bool win1_active = false;
    bool obj_win_active = false;
    uint bg_skip;

    auto GetNextTopmostOpaqueBgLayer = [&](uint dot) -> int {
        auto it = std::find_if(bg_by_prio.begin() + bg_skip, bg_by_prio.end(), [&](int bg) {
            ++bg_skip;
            if (!GetBit(dispcnt.screen_display_bg, bg) || bg_render[bg][dot].transparent) {
                return false;
            }
            if (win0_active) {
                return GetBit(winin.window0_bg_enable, bg);
            }
            if (win1_active) {
                return GetBit(winin.window1_bg_enable, bg);
            }
            if (obj_win_active) {
                return GetBit(winout.obj_window_bg_enable, bg);
            }
            if (win0_enable || win1_enable || obj_win_enable) {
                return GetBit(winout.outside_bg_enable, bg);
            }
            return true;
        });
        return it != bg_by_prio.end() ? *it : 4;
    };

    for (uint dot = 0; dot < dots_per_line; ++dot) {
        bg_skip = 0;
        if (scanline_falls_on_win0) {
            win0_active = dot >= winh_x1[0] && dot < winh_x2[0];
        }
        if (scanline_falls_on_win1) {
            win1_active = dot >= winh_x1[1] && dot < winh_x2[1];
        }
        if (obj_win_enable) {
            obj_win_active = obj_render[dot].obj_mode == obj_mode_obj_window_mask && !obj_render[dot].transparent;
        }
        int topmost_opaque_bg = GetNextTopmostOpaqueBgLayer(dot);
        int fx_1st_target_index;
        RGB rgb_1st_layer;
        if (obj_win_active) {
            /* objects are not displayed */
            /* TODO: are objects displayed if win0 or win1 are also active? */
            if (topmost_opaque_bg < 4) {
                fx_1st_target_index = topmost_opaque_bg;
                rgb_1st_layer = bg_render[topmost_opaque_bg][dot].ToRGB();
            } else {
                fx_1st_target_index = fx_backdrop_index;
                rgb_1st_layer = GetBackdropColor().ToRGB();
            }
            bool special_effect_enable = [&] {
                if (bldcnt.color_special_effect == fx_disable_mask) return false;
                if (win0_active) return bool(winin.window0_color_special_effect);
                if (win1_active) return bool(winin.window1_color_special_effect);
                return bool(winout.obj_window_color_special_effect);
            }();
            if (special_effect_enable) {
                u16 const bldcnt_u16 = std::bit_cast<u16>(bldcnt);
                switch (bldcnt.color_special_effect) {
                case fx_alpha_blending_mask: { /* 1st+2nd Target mixed */
                    int second_topmost_opaque_bg = GetNextTopmostOpaqueBgLayer(dot);
                    int fx_2nd_target_index;
                    RGB rgb_2nd_layer;
                    if (second_topmost_opaque_bg < 4) {
                        fx_2nd_target_index = second_topmost_opaque_bg;
                        rgb_2nd_layer = bg_render[second_topmost_opaque_bg][dot].ToRGB();
                    } else {
                        fx_2nd_target_index = fx_backdrop_index;
                        rgb_2nd_layer = GetBackdropColor().ToRGB();
                    }
                    if (GetBit(bldcnt_u16, fx_1st_target_index) && GetBit(bldcnt_u16, fx_2nd_target_index + 8)) {
                        rgb_1st_layer = AlphaBlend(rgb_1st_layer, rgb_2nd_layer);
                    }
                    break;
                }

                case fx_brightness_increase_mask: /* 1st Target becomes whiter */
                    if (GetBit(bldcnt_u16, fx_1st_target_index)) {
                        rgb_1st_layer = BrightnessIncrease(rgb_1st_layer);
                    }
                    break;

                case fx_brightness_decrease_mask: /* 1st Target becomes blacker */
                    if (GetBit(bldcnt_u16, fx_1st_target_index)) {
                        rgb_1st_layer = BrightnessDecrease(rgb_1st_layer);
                    }
                    break;

                default: std::unreachable();
                }
            }
        } else {
            /* objects are displayed */
            auto ChooseBgOrObj = [&](int bg_index, bool& obj_chosen, RGB& rgb, int& fx_target_index) -> void {
                if (obj_render[dot].transparent) {
                    obj_chosen = false;
                    if (bg_index < 4) {
                        fx_target_index = bg_index;
                        rgb = bg_render[bg_index][dot].ToRGB();
                    } else {
                        fx_target_index = fx_backdrop_index;
                        rgb = GetBackdropColor().ToRGB();
                    }
                } else if (bg_index == 4 || bgcnt[bg_index].bg_priority >= obj_render[dot].priority) {
                    obj_chosen = true;
                    fx_target_index = fx_obj_index;
                    rgb = obj_render[dot].ToRGB();
                } else {
                    obj_chosen = false;
                    fx_target_index = bg_index;
                    rgb = bg_render[bg_index][dot].ToRGB();
                }
            };

            bool top_layer_is_obj;
            ChooseBgOrObj(topmost_opaque_bg, top_layer_is_obj, rgb_1st_layer, fx_1st_target_index);

            if (obj_render[dot].obj_mode == obj_mode_semi_transparent_mask) {
                /* OBJs that are defined as 'Semi-Transparent' in OAM memory are always selected as 1st Target
                   (regardless of BLDCNT Bit 4), and are always using Alpha Blending mode (regardless of BLDCNT Bit
                   6-7). */
                int fx_2nd_target_index;
                RGB rgb_2nd_layer;
                if (top_layer_is_obj) {
                    if (topmost_opaque_bg < 4) {
                        fx_2nd_target_index = topmost_opaque_bg;
                        rgb_2nd_layer = bg_render[topmost_opaque_bg][dot].ToRGB();
                    } else {
                        fx_2nd_target_index = fx_backdrop_index;
                        rgb_2nd_layer = GetBackdropColor().ToRGB();
                    }
                    if (GetBit(std::bit_cast<u16>(bldcnt), fx_2nd_target_index)) {
                        rgb_1st_layer = AlphaBlend(rgb_1st_layer, rgb_2nd_layer);
                    }
                } else {
                    bool second_layer_is_obj;
                    ChooseBgOrObj(GetNextTopmostOpaqueBgLayer(dot),
                      second_layer_is_obj,
                      rgb_2nd_layer,
                      fx_2nd_target_index);
                    if (second_layer_is_obj) {
                        if (GetBit(std::bit_cast<u16>(bldcnt), fx_1st_target_index + 8)) {
                            rgb_2nd_layer = AlphaBlend(rgb_2nd_layer, rgb_1st_layer);
                        }
                    }
                }
            } else {
                bool special_effect_enable = [&] {
                    if (bldcnt.color_special_effect == fx_disable_mask) return false;
                    if (win0_active) return bool(winin.window0_color_special_effect);
                    if (win1_active) return bool(winin.window1_color_special_effect);
                    if (win0_enable || win1_enable) return bool(winout.outside_color_special_effect);
                    return true;
                }();
                if (special_effect_enable) {
                    u16 const bldcnt_u16 = std::bit_cast<u16>(bldcnt);
                    switch (bldcnt.color_special_effect) {
                    case fx_alpha_blending_mask: { /* 1st+2nd Target mixed */
                        int fx_2nd_target_index;
                        RGB rgb_2nd_layer;
                        if (top_layer_is_obj) {
                            if (topmost_opaque_bg < 4) {
                                fx_2nd_target_index = topmost_opaque_bg;
                                rgb_2nd_layer = bg_render[topmost_opaque_bg][dot].ToRGB();
                            } else {
                                fx_2nd_target_index = fx_backdrop_index;
                                rgb_2nd_layer = GetBackdropColor().ToRGB();
                            }
                        } else {
                            ChooseBgOrObj(GetNextTopmostOpaqueBgLayer(dot),
                              top_layer_is_obj,
                              rgb_2nd_layer,
                              fx_2nd_target_index);
                        }
                        if (GetBit(bldcnt_u16, fx_1st_target_index) && GetBit(bldcnt_u16, fx_2nd_target_index + 8)) {
                            rgb_1st_layer = AlphaBlend(rgb_1st_layer, rgb_2nd_layer);
                        }
                        break;
                    }

                    case fx_brightness_increase_mask: /* 1st Target becomes whiter */
                        if (GetBit(bldcnt_u16, fx_1st_target_index)) {
                            rgb_1st_layer = BrightnessIncrease(rgb_1st_layer);
                        }
                        break;

                    case fx_brightness_decrease_mask: /* 1st Target becomes blacker */
                        if (GetBit(bldcnt_u16, fx_1st_target_index)) {
                            rgb_1st_layer = BrightnessDecrease(rgb_1st_layer);
                        }
                        break;

                    default: std::unreachable();
                    }
                }
            }
        }
        PushPixel(Rgb555ToRgb888(rgb_1st_layer));
    }
}

RGB BrightnessDecrease(RGB pixel)
{
    return { .r = u8(std::max(0, pixel.r - pixel.r * evy)),
        .g = u8(std::max(0, pixel.g - pixel.g * evy)),
        .b = u8(std::max(0, pixel.b - pixel.b * evy)) };
}

RGB BrightnessIncrease(RGB pixel)
{
    return { .r = u8(std::min(31, pixel.r + (31 - pixel.r) * evy)),
        .g = u8(std::min(31, pixel.g + (31 - pixel.g) * evy)),
        .b = u8(std::min(31, pixel.b + (31 - pixel.b) * evy)) };
}

BgColorData GetBackdropColor()
{
    BgColorData col;
    std::memcpy(&col, palette_ram.data(), sizeof(BgColorData));
    col.transparent = false;
    return col;
}

void PushPixel(auto color_data)
{
    if (color_data.transparent) {
        std::memset(framebuffer.data() + framebuffer_index, 0xFF, 3);
        framebuffer_index += 3;
    } else {
        framebuffer[framebuffer_index++] = color_data.r;
        framebuffer[framebuffer_index++] = color_data.g;
        framebuffer[framebuffer_index++] = color_data.b;
    }
}

void PushPixel(RGB rgb)
{
    framebuffer[framebuffer_index++] = rgb.r;
    framebuffer[framebuffer_index++] = rgb.g;
    framebuffer[framebuffer_index++] = rgb.b;
}

void PushPixel(u8 r, u8 g, u8 b)
{
    framebuffer[framebuffer_index++] = r;
    framebuffer[framebuffer_index++] = g;
    framebuffer[framebuffer_index++] = b;
}

template<void (*RenderFun)(), bool vertical_mosaic> void RenderBackground(uint bg)
{
    if (GetBit(dispcnt.screen_display_bg, bg)) {
        if (!vertical_mosaic || !bgcnt[bg].mosaic_enable) {
            RenderFun();
        }
    } else {
        RenderTransparentBackground(bg);
    }
}

template<void (*RenderFun)(uint), bool vertical_mosaic> void RenderBackground(uint bg)
{
    if (GetBit(dispcnt.screen_display_bg, bg)) {
        if (!vertical_mosaic || !bgcnt[bg].mosaic_enable) {
            RenderFun(bg);
        }
    } else {
        RenderTransparentBackground(bg);
    }
}

template<bool vertical_mosaic> void RenderBackgrounds()
{
    switch (dispcnt.bg_mode) {
    case 0:
        for (uint bg = 0; bg < 4; ++bg) {
            RenderBackground<ScanlineBackgroundTextMode, vertical_mosaic>(bg);
        }
        break;

    case 1:
        RenderBackground<ScanlineBackgroundTextMode, vertical_mosaic>(0);
        RenderBackground<ScanlineBackgroundTextMode, vertical_mosaic>(1);
        RenderBackground<ScanlineBackgroundRotateScaleMode, vertical_mosaic>(2);
        RenderTransparentBackground(3);
        break;

    case 2:
        RenderTransparentBackground(0);
        RenderTransparentBackground(1);
        RenderBackground<ScanlineBackgroundRotateScaleMode, vertical_mosaic>(2);
        RenderBackground<ScanlineBackgroundRotateScaleMode, vertical_mosaic>(3);
        break;

    case 3:
        RenderTransparentBackground(0);
        RenderTransparentBackground(1);
        RenderTransparentBackground(3);
        RenderBackground<ScanlineBackgroundBitmapMode3, vertical_mosaic>(2);
        break;

    case 4:
        RenderTransparentBackground(0);
        RenderTransparentBackground(1);
        RenderTransparentBackground(3);
        RenderBackground<ScanlineBackgroundBitmapMode4, vertical_mosaic>(2);
        break;

    case 5:
        RenderTransparentBackground(0);
        RenderTransparentBackground(1);
        RenderTransparentBackground(3);
        RenderBackground<ScanlineBackgroundBitmapMode5, vertical_mosaic>(2);
        break;

    case 6:
    case 7: break;

    default: std::unreachable();
    }
}

void RenderScanline()
{
    if (dispcnt.forced_blank) {
        /* output only white pixels */
        std::memset(framebuffer.data() + framebuffer_index, 0xFF, framebuffer_pitch);
        framebuffer_index += framebuffer_pitch;
        return;
    }

    if (mosaic.bg_v_size == 0) {
        RenderBackgrounds<false>();
    } else {
        if (mosaic_v_counter++ == 0) {
            RenderBackgrounds<false>();
        } else {
            RenderBackgrounds<true>();
        }
        if (mosaic_v_counter == mosaic.bg_v_size - 1) {
            mosaic_v_counter = 0;
        }
    }

    if (dispcnt.screen_display_obj) {
        ScanOam();
        ScanlineObjects();
    } else {
        obj_render.fill(transparent_obj_pixel);
    }

    BlendLayers();
}

void RenderTransparentBackground(uint bg)
{
    for (uint dot = 0; dot < dots_per_line; ++dot) {
        bg_render[bg][dot] = transparent_bg_pixel;
    }
}

RGB Rgb555ToRgb888(RGB rgb)
{
    /* Convert each 5-bit channel to 8-bit channels (https://github.com/mattcurrie/dmg-acid2) */
    return { .r = u8(rgb.r << 3 | rgb.r >> 2), .g = u8(rgb.g << 3 | rgb.g >> 2), .b = u8(rgb.b << 3 | rgb.b >> 2) };
}

void ScanlineBackgroundTextMode(uint bg)
{
    static constexpr uint tile_size = 8;
    static constexpr uint map_entry_size = 2;
    uint const bg_width = 256 << (bgcnt[bg].screen_size & 1); /* 0 => 256; 1 => 512; 2 => 256; 3 = 512 */
    uint const base_tile_map_addr =
      [&] { /* already takes into account which vertical tile we're on (it's constant), but not horizontal */
          static constexpr uint bytes_per_bg_map_area_row = 256 / tile_size * map_entry_size;
          uint const bg_height = 256 << (bgcnt[bg].screen_size >> 1); /* 0 => 256; 1 => 256; 2 => 512; 3 => 512 */
          uint const bg_tile_index_y = ((bgvofs[bg] + v_counter) & 255) / tile_size;
          uint base_tile_map_addr = bgcnt[bg].screen_base_block;
          if (bg_height == 512 && ((bgvofs[bg] + v_counter) & 511) > 255) {
              base_tile_map_addr +=
                1 + (bg_width == 512); /* BG width == 256: SC0 => SC1; BG width == 512: SC0/SC1 => SC2/SC3 */
          }
          base_tile_map_addr *= 0x800;
          return base_tile_map_addr + bg_tile_index_y * bytes_per_bg_map_area_row;
      }();
    // uint const mosaic_incr = bgcnt[bg].mosaic_enable ? mosaic.bg_h_size + 1 : 1; /* TODO */
    uint bg_tile_index_x = (bghofs[bg] & (bg_width - 1)) / tile_size; /* note: 0-63, but masked to 0-31 when needed */
    uint base_tile_data_addr = bgcnt[bg].char_base_block * 0x4000;
    uint dot = 0;

    auto FetchPushTile = [&]<bool palette_mode>(uint pixels_to_ignore_left, uint pixels_to_ignore_right) {
        uint tile_map_addr = base_tile_map_addr + map_entry_size * (bg_tile_index_x & 31);
        if (bg_width == 512 && bg_tile_index_x > 31) {
            tile_map_addr += 0x800; /* SC0/SC2 => SC1/SC3 */
        }
        /* VRAM BG Screen Data Format (BG Map)
            0-9   Tile Number     (0-1023) (a bit less in 256 color mode, because there'd be otherwise no room for the
           bg map) 10    Horizontal Flip (0=Normal, 1=Mirrored) 11    Vertical Flip   (0=Normal, 1=Mirrored) 12-15
           Palette Number  (0-15)    (Not used in 256 color/1 palette mode) */
        static constexpr uint col_size = 2;
        uint tile_num = vram[tile_map_addr] | vram[tile_map_addr + 1] << 8 & 0x300;
        bool const flip_x = vram[tile_map_addr + 1] & 4;
        bool const flip_y = vram[tile_map_addr + 1] & 8;
        uint tile_pixel_offset_y = (v_counter + bgvofs[bg]) % 8;
        if (flip_y) {
            tile_pixel_offset_y = 7 - tile_pixel_offset_y;
        }
        /* 4-bit depth (16 colors, 16 palettes): Each tile occupies 32 bytes of memory, the first 4 bytes for the
           topmost row of the tile, and so on. Each byte representing two dots, the lower 4 bits define the color for
           the left dot, the upper 4 bits the color for the right dot. 8-bit depth (256 colors, 1 palette): Each tile
           occupies 64 bytes of memory, the first 8 bytes for the topmost row of the tile, etc.. Each byte selects the
           palette entry for each dot. */
        static constexpr uint tile_row_size = [&] {
            if constexpr (palette_mode == 0) return 4;
            else return 8;
        }();
        /* When using the 256 Colors/1 Palette mode, only each second tile may be used, the lower bit
            of the tile number should be zero (in 2-dimensional mapping mode, the bit is completely ignored). */
        if constexpr (palette_mode == 1) {
            tile_num &= ~1;
        }
        u32 tile_data_addr_offset = 32 * tile_num + tile_row_size * tile_pixel_offset_y;
        u32 tile_data_addr = base_tile_data_addr + (tile_data_addr_offset);
        uint col_shift;
        auto FetchPushPixel = [&](uint pixel_index) {
            BgColorData col;
            u8 col_id;
            u8 const* palette_start_ptr;
            if constexpr (palette_mode == 0) {
                col_id = vram[tile_data_addr + pixel_index / 2] >> col_shift & 0xF;
                u8 palette_num = vram[tile_map_addr + 1] >> 4;
                palette_start_ptr = palette_ram.data() + 16 * col_size * palette_num;
            } else {
                col_id = vram[tile_data_addr + pixel_index];
                palette_start_ptr = palette_ram.data();
            }
            std::memcpy(&col, palette_start_ptr + col_size * col_id, col_size);
            col.transparent = col_id == 0;
            bg_render[bg][dot++] = col;
        };
        if (flip_x) {
            col_shift = (pixels_to_ignore_right & 1)
                        ? 0
                        : 4; /* access lower nibble of byte if tile pixel index is even, else higher nibble. */
            for (int i = 7 - pixels_to_ignore_right; i >= (int)pixels_to_ignore_left; --i, col_shift ^= 4) {
                FetchPushPixel(i);
            }
        } else {
            col_shift = (pixels_to_ignore_left & 1) ? 4 : 0;
            for (int i = pixels_to_ignore_left; i < 8 - (int)pixels_to_ignore_right; ++i, col_shift ^= 4) {
                FetchPushPixel(i);
            }
        }
        bg_tile_index_x = (bg_tile_index_x + 1) & ((bg_width - 1) / 8);
    };

    auto Render = [&]<bool palette_mode> {
#define FETCH_PUSH_TILE FetchPushTile.template operator()
        /* The LCD being 240 pixels wide means 30 tiles to fetch if bghofs lands perfectly at the beginning of a tile.
         * Else: 31 tiles. */
        auto offset = bghofs[bg] & 7;
        FETCH_PUSH_TILE<palette_mode>(offset, 0);
        for (int i = 0; i < 29; ++i) {
            FETCH_PUSH_TILE<palette_mode>(0, 0);
        }
        if (offset > 0) {
            FETCH_PUSH_TILE<palette_mode>(0, 8 - offset);
        }
#undef FETCH_PUSH_TILE
    };

    if (bgcnt[bg].palette_mode == 0) Render.template operator()<0>();
    else Render.template operator()<1>();
}

void ScanlineBackgroundRotateScaleMode(uint bg)
{
    // TODO
    (void)bg;
}

void ScanlineBackgroundBitmapMode3()
{
    static constexpr uint col_size = 2;
    static constexpr uint bytes_per_scanline = dots_per_line * col_size;
    uint const mosaic_incr = bgcnt[2].mosaic_enable ? mosaic.bg_h_size + 1 : 1;
    for (uint dot = 0; dot < dots_per_line;) {
        BgColorData col;
        std::memcpy(&col, vram.data() + v_counter * bytes_per_scanline + dot * col_size, col_size);
        col.transparent = false; /* The two bytes directly define one of the 32768 colors (without using palette data,
                                    and thus not supporting a 'transparent' BG color) */
        for (uint i = 0; i < mosaic_incr; ++i) {
            bg_render[2][dot++] = col;
        }
    }
}

void ScanlineBackgroundBitmapMode4()
{
    static constexpr uint col_size = 2;
    static constexpr uint bytes_per_scanline = dots_per_line;
    uint const mosaic_incr = bgcnt[2].mosaic_enable ? mosaic.bg_h_size + 1 : 1;
    uint const vram_frame_offset = dispcnt.display_frame_select ? 0xA000 : 0;
    for (uint dot = 0; dot < dots_per_line;) {
        u8 palette_index = vram[vram_frame_offset + v_counter * bytes_per_scanline + dot];
        BgColorData col;
        std::memcpy(&col, palette_ram.data() + palette_index * col_size, col_size);
        col.transparent = palette_index == 0;
        for (uint i = 0; i < mosaic_incr; ++i) {
            bg_render[2][dot++] = col;
        }
    }
}

void ScanlineBackgroundBitmapMode5()
{
    static constexpr uint col_size = 2;
    static constexpr uint bytes_per_scanline = dots_per_line * col_size;
    uint const mosaic_incr = bgcnt[2].mosaic_enable ? mosaic.bg_h_size + 1 : 1;
    uint const vram_frame_offset = dispcnt.display_frame_select ? 0xA000 : 0;
    uint dot = 0;
    while (dot < 40) {
        bg_render[2][dot++] = transparent_bg_pixel;
    }
    while (dot < 200) {
        BgColorData col;
        std::memcpy(&col, vram.data() + vram_frame_offset + v_counter * bytes_per_scanline + dot * col_size, col_size);
        col.transparent = false; /* The two bytes directly define one of the 32768 colors (without using palette data,
                                    and thus not supporting a 'transparent' BG color) */
        for (uint i = 0; i < mosaic_incr; ++i) {
            bg_render[2][dot++] = col;
        }
    }
    while (dot < 240) {
        bg_render[2][dot++] = transparent_bg_pixel;
    }
}

void ScanlineObjects()
{
    obj_render.fill(transparent_obj_pixel);
    if (objects.empty()) {
        return;
    }
    bool const char_vram_mapping = dispcnt.obj_char_vram_mapping;
    uint const vram_base_addr = dispcnt.bg_mode < 3 ? 0x10000 : 0x14000;
    uint const vram_addr_mask = 0x17FFF - vram_base_addr;

    auto RenderObject = [&](ObjData const& obj) {
        if (!obj.rotate_scale) {
            auto RenderObject = [&]<bool palette_mode> {
                uint dot, render_len, tile_offset_x, tile_pixel_offset_x;
                if (obj.x_coord < dots_per_line) {
                    dot = obj.x_coord;
                    render_len = std::min((uint)obj.size_x, dots_per_line - obj.x_coord);
                    tile_offset_x = tile_pixel_offset_x = 0;
                } else if (obj.x_coord + obj.size_x > 512) { /* wrap around to x = 0 */
                    dot = 0;
                    render_len = obj.size_x - (512 - obj.x_coord);
                    tile_offset_x = (512 - obj.x_coord) / 8;
                    tile_pixel_offset_x = (512 - obj.x_coord) % 8;
                } else {
                    return;
                }
                bool const flip_x = obj.rot_scale_param & 8;
                bool const flip_y = obj.rot_scale_param & 16;
                auto const tile_offset_y = (v_counter - obj.y_coord) / 8;
                auto tile_pixel_offset_y = (v_counter - obj.y_coord) % 8;
                if (flip_y) {
                    tile_pixel_offset_y = 7 - tile_pixel_offset_y;
                }
                /* Palette Mode 0: 4-bit depth (16 colors, 16 palettes). Each tile occupies 32 bytes of memory, the
                   first 4 bytes for the topmost row of the tile, and so on. Each byte representing two dots, the lower
                   4 bits define the color for the left dot, the upper 4 bits the color for the right dot. Palette Mode
                   1: 8-bit depth (256 colors, 1 palette). Each tile occupies 64 bytes of memory, the first 8 bytes for
                   the topmost row of the tile, etc.. Each byte selects the palette entry for each dot. */
                static constexpr uint tile_size = [&] {
                    if constexpr (palette_mode == 0) return 32;
                    else return 64;
                }();
                static constexpr uint tile_row_size = tile_size / 8;
                /* When using the 256 Colors/1 Palette mode, only each second tile may be used, the lower bit
                    of the tile number should be zero (in 2-dimensional mapping mode, the bit is completely ignored). */
                u32 base_tile_num = obj.tile_num;
                if constexpr (palette_mode == 1) {
                    base_tile_num &= ~1;
                }
                u32 tile_data_addr_offset;
                auto FetchPushTile = [&](uint pixels_to_ignore_left, uint pixels_to_ignore_right) {
                    u32 tile_data_addr = vram_base_addr + (tile_data_addr_offset & vram_addr_mask);
                    uint col_shift;
                    auto FetchPushPixel = [&](uint pixel_index) {
                        if (obj_render[dot].transparent) {
                            static constexpr uint col_size = 2;
                            static constexpr uint obj_palette_offset = 0x200;
                            ObjColorData col;
                            u8 col_id;
                            u8 const* palette_start_ptr = palette_ram.data() + obj_palette_offset;
                            if constexpr (palette_mode == 0) {
                                col_id = vram[tile_data_addr + pixel_index / 2] >> col_shift & 0xF;
                                palette_start_ptr += 16 * col_size * obj.palette_num;
                            } else {
                                col_id = vram[tile_data_addr + pixel_index];
                            }
                            std::memcpy(&col, palette_start_ptr + col_size * col_id, col_size);
                            col.transparent = col_id == 0;
                            col.obj_mode = obj.obj_mode;
                            col.priority = obj.priority;
                            obj_render[dot] = col;
                        }
                        ++dot;
                    };
                    if (flip_x) {
                        col_shift =
                          (pixels_to_ignore_right & 1)
                            ? 0
                            : 4; /* access lower nibble of byte if tile pixel index is even, else higher nibble. */
                        for (int i = 7 - pixels_to_ignore_right; i >= (int)pixels_to_ignore_left; --i, col_shift ^= 4) {
                            FetchPushPixel(i);
                        }
                    } else {
                        col_shift = (pixels_to_ignore_left & 1) ? 4 : 0;
                        for (int i = pixels_to_ignore_left; i < 8 - (int)pixels_to_ignore_right; ++i, col_shift ^= 4) {
                            FetchPushPixel(i);
                        }
                    }
                };
                /* Char VRAM Mapping = 0: The 1024 OBJ tiles are arranged as a matrix of 32x32 tiles / 256x256 pixels
                   (In 256 color mode: 16x32 tiles / 128x256 pixels) E.g., when displaying a 16x16 pixel OBJ with tile
                   number 04h, the upper row of the OBJ will consist of tile 04h and 05h, the next row of 24h and 25h.
                   (In 256 color mode: 04h and 06h, 24h and 26h.) Char VRAM Mapping = 1: Tiles are mapped each after
                   each other from 00h-3FFh. Using the same example as above, the upper row of the OBJ will consist of
                   tile 04h and 05h, the next row of tile 06h and 07h. (In 256 color mode: 04h and 06h, 08h and 0Ah.)*/
                auto const base_rel_tile_num =
                  tile_offset_x + tile_offset_y * (char_vram_mapping == 0 ? 32 : obj.size_x / 8);
                tile_data_addr_offset = 32 * (base_tile_num + base_rel_tile_num) + tile_row_size * tile_pixel_offset_y;
                auto pixels_to_ignore_left = tile_pixel_offset_x;
                auto pixels_to_ignore_right = std::max(0, 8 - int(tile_pixel_offset_x) - int(render_len));
                FetchPushTile(pixels_to_ignore_left, pixels_to_ignore_right);
                render_len -= 8 - pixels_to_ignore_left - pixels_to_ignore_right;
                for (uint tile = 0; tile < render_len / 8; ++tile) {
                    tile_data_addr_offset += tile_size;
                    FetchPushTile(0, 0);
                }
                if (render_len % 8) {
                    tile_data_addr_offset += tile_size;
                    FetchPushTile(0, 8 - render_len % 8);
                }
            };

            if (obj.palette_mode == 0) RenderObject.template operator()<0>();
            else RenderObject.template operator()<1>();
        }
    };

    std::ranges::for_each(objects, [&](ObjData const& obj) { RenderObject(obj); });
}

void ScanOam()
{
    /* OBJ Attribute 0 (R/W)
          Bit   Expl.
          0-7   Y-Coordinate           (0-255)
          8     Rotation/Scaling Flag  (0=Off, 1=On)
          When Rotation/Scaling used (Attribute 0, bit 8 set):
            9     Double-Size Flag     (0=Normal, 1=Double)
          When Rotation/Scaling not used (Attribute 0, bit 8 cleared):
            9     OBJ Disable          (0=Normal, 1=Not displayed)
          10-11 OBJ Mode  (0=Normal, 1=Semi-Transparent, 2=OBJ Window, 3=Prohibited)
          12    OBJ Mosaic             (0=Off, 1=On)
          13    Colors/Palettes        (0=16/16, 1=256/1)
          14-15 OBJ Shape              (0=Square,1=Horizontal,2=Vertical,3=Prohibited)

        OBJ Attribute 1 (R/W)
          Bit   Expl.
          0-8   X-Coordinate           (0-511)
          When Rotation/Scaling used (Attribute 0, bit 8 set):
            9-13  Rotation/Scaling Parameter Selection (0-31)
                  (Selects one of the 32 Rotation/Scaling Parameters that
                  can be defined in OAM, for details read next chapter.)
          When Rotation/Scaling not used (Attribute 0, bit 8 cleared):
            9-11  Not used
            12    Horizontal Flip      (0=Normal, 1=Mirrored)
            13    Vertical Flip        (0=Normal, 1=Mirrored)
          14-15 OBJ Size               (0..3, depends on OBJ Shape, see Attr 0)
                  Size  Square   Horizontal  Vertical
                  0     8x8      16x8        8x16
                  1     16x16    32x8        8x32
                  2     32x32    32x16       16x32
                  3     64x64    64x32       32x64

        OBJ Attribute 2 (R/W)
          Bit   Expl.
          0-9   Character Name          (0-1023=Tile Number)
          10-11 Priority relative to BG (0-3; 0=Highest)
          12-15 Palette Number   (0-15) (Not used in 256 color/1 palette mode)
    */
    objects.clear();
    for (uint oam_addr = 0; oam_addr < oam.size(); oam_addr += 8) {
        bool rotate_scale = oam[oam_addr + 1] & 1;
        bool double_size_obj_disable = oam[oam_addr + 1] & 2;
        if (!rotate_scale && double_size_obj_disable) {
            continue;
        }
        u8 y_coord = oam[oam_addr];
        if (y_coord > v_counter) {
            continue;
        }
        u8 obj_shape = oam[oam_addr + 1] >> 6 & 3;
        u8 obj_size = oam[oam_addr + 3] >> 6 & 3;
        /* OBJ Size (0-3) * OBJ Shape (0-2 (3 prohibited)) */
        static constexpr u8 obj_height_table[4][4] = { 8, 8, 16, 8, 16, 8, 32, 16, 32, 16, 32, 32, 64, 32, 64, 64 };
        static constexpr u8 obj_width_table[4][4] = { 8, 16, 8, 8, 16, 32, 8, 16, 32, 32, 16, 32, 64, 64, 32, 64 };
        u8 obj_height = obj_height_table[obj_size][obj_shape] << (double_size_obj_disable & rotate_scale);
        if (y_coord + obj_height <= v_counter) {
            continue;
        }
        ObjData obj_data;
        std::memcpy(&obj_data, oam.data() + oam_addr, 6);
        obj_data.size_x = obj_width_table[obj_size][obj_shape] << (double_size_obj_disable & rotate_scale);
        obj_data.oam_index = oam_addr / 8;
        objects.push_back(obj_data);
    }
}

void SortBackgroundsAfterPriority()
{
    /* TODO: also take into account if a bg is enabled? */
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (bgcnt[bg_by_prio[j]].bg_priority > bgcnt[bg_by_prio[j + 1]].bg_priority
                || bgcnt[bg_by_prio[j]].bg_priority == bgcnt[bg_by_prio[j + 1]].bg_priority
                     && bg_by_prio[j] > bg_by_prio[j + 1]) {
                std::swap(bg_by_prio[j], bg_by_prio[j + 1]);
            }
        }
    }
}
} // namespace gba::ppu
