// Colors and sizing shared by the game UI. Palette matches the web flasher:
// solder-mask yellow for selection, ink for givens, blue for player digits.
#pragma once

#include <lvgl.h>

namespace ui {

inline lv_color_t c_screen()     { return lv_color_hex(0xF4F5F2); }
inline lv_color_t c_cell()       { return lv_color_hex(0xFFFFFF); }
inline lv_color_t c_line_thin()  { return lv_color_hex(0xBFC4CB); }
inline lv_color_t c_line_thick() { return lv_color_hex(0x222833); }
inline lv_color_t c_given()      { return lv_color_hex(0x222833); }
inline lv_color_t c_entry()      { return lv_color_hex(0x1F5FBF); }
inline lv_color_t c_note()       { return lv_color_hex(0x59616E); }
inline lv_color_t c_conflict()   { return lv_color_hex(0xC62828); }
inline lv_color_t c_conflict_bg(){ return lv_color_hex(0xFBE1E1); }
inline lv_color_t c_peer()       { return lv_color_hex(0xFBF2D2); }   // row/col/box of selection
inline lv_color_t c_same()       { return lv_color_hex(0xF3D77E); }   // same digit elsewhere
inline lv_color_t c_selected()   { return lv_color_hex(0xE3B53A); }   // the selected cell
inline lv_color_t c_key()        { return lv_color_hex(0xFFFFFF); }
inline lv_color_t c_key_border() { return lv_color_hex(0xCDD2D8); }
inline lv_color_t c_key_on()     { return lv_color_hex(0xE3B53A); }
inline lv_color_t c_key_off_txt(){ return lv_color_hex(0xC3C8CF); }
inline lv_color_t c_ink()        { return lv_color_hex(0x222833); }
inline lv_color_t c_muted()      { return lv_color_hex(0x5B6472); }

// Fonts picked by the size they must fit
inline const lv_font_t* font_for_cell(int cell)
{
    return cell >= 33 ? &lv_font_montserrat_28 : cell >= 22 ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
}
inline const lv_font_t* note_font_for_cell(int cell)
{
    return cell >= 33 ? &lv_font_montserrat_10 : &lv_font_montserrat_8;
}

} // namespace ui
