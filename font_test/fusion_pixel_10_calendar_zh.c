/*******************************************************************************
 * Size: 10 px
 * Bpp: 1
 * Opts: --font fusion-pixel-10px-monospaced-zh_hans.ttf --size 10 --bpp 1 --format lvgl --lv-font-name fusion_pixel_10_calendar_zh --range 0x20-0x7e --symbols 年月日星期一二三四五六天上下午夜间晴阴雨雪多云温度湿时分秒今日明后前设置返回确认取消加载同步失败成功电量网络蓝牙农历节气 --output font_test/fusion_pixel_10_calendar_zh.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef FUSION_PIXEL_10_CALENDAR_ZH
#define FUSION_PIXEL_10_CALENDAR_ZH 1
#endif

#if FUSION_PIXEL_10_CALENDAR_ZH

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xfa,

    /* U+0022 "\"" */
    0xb4,

    /* U+0023 "#" */
    0x5f, 0x59, 0xaf, 0xa0,

    /* U+0024 "$" */
    0x27, 0xaa, 0x63, 0x3e, 0x20,

    /* U+0025 "%" */
    0x91, 0x22, 0x48, 0x90,

    /* U+0026 "&" */
    0x4a, 0xa4, 0xba, 0x50,

    /* U+0027 "'" */
    0xc0,

    /* U+0028 "(" */
    0x29, 0x49, 0x12, 0x20,

    /* U+0029 ")" */
    0x89, 0x12, 0x52, 0x80,

    /* U+002A "*" */
    0xab, 0xaa,

    /* U+002B "+" */
    0x22, 0xf2, 0x20,

    /* U+002C "," */
    0x60,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x11, 0x22, 0x44, 0x48, 0x80,

    /* U+0030 "0" */
    0x69, 0xbd, 0x99, 0x60,

    /* U+0031 "1" */
    0x59, 0x24, 0xb8,

    /* U+0032 "2" */
    0x69, 0x12, 0x48, 0xf0,

    /* U+0033 "3" */
    0x69, 0x12, 0x19, 0x60,

    /* U+0034 "4" */
    0x35, 0x59, 0x9f, 0x10,

    /* U+0035 "5" */
    0xf8, 0x8e, 0x11, 0xe0,

    /* U+0036 "6" */
    0x69, 0x8e, 0x99, 0x60,

    /* U+0037 "7" */
    0xf1, 0x12, 0x22, 0x20,

    /* U+0038 "8" */
    0x69, 0x96, 0x99, 0x60,

    /* U+0039 "9" */
    0x69, 0x97, 0x19, 0x60,

    /* U+003A ":" */
    0x88,

    /* U+003B ";" */
    0x40, 0x60,

    /* U+003C "<" */
    0x2a, 0x22,

    /* U+003D "=" */
    0xf0, 0xf0,

    /* U+003E ">" */
    0x88, 0xa8,

    /* U+003F "?" */
    0x69, 0x12, 0x20, 0x20,

    /* U+0040 "@" */
    0x69, 0xbb, 0xb8, 0x60,

    /* U+0041 "A" */
    0x69, 0x99, 0xf9, 0x90,

    /* U+0042 "B" */
    0xe9, 0x9e, 0x99, 0xe0,

    /* U+0043 "C" */
    0x69, 0x88, 0x89, 0x60,

    /* U+0044 "D" */
    0xe9, 0x99, 0x99, 0xe0,

    /* U+0045 "E" */
    0xf8, 0x8e, 0x88, 0xf0,

    /* U+0046 "F" */
    0xf8, 0x8e, 0x88, 0x80,

    /* U+0047 "G" */
    0x69, 0x88, 0xb9, 0x70,

    /* U+0048 "H" */
    0x99, 0x9f, 0x99, 0x90,

    /* U+0049 "I" */
    0xf2, 0x22, 0x22, 0xf0,

    /* U+004A "J" */
    0x11, 0x11, 0x99, 0x60,

    /* U+004B "K" */
    0x99, 0xac, 0xa9, 0x90,

    /* U+004C "L" */
    0x88, 0x88, 0x88, 0xf0,

    /* U+004D "M" */
    0x9f, 0xf9, 0x99, 0x90,

    /* U+004E "N" */
    0x9d, 0xdb, 0xb9, 0x90,

    /* U+004F "O" */
    0x69, 0x99, 0x99, 0x60,

    /* U+0050 "P" */
    0xe9, 0x9e, 0x88, 0x80,

    /* U+0051 "Q" */
    0x69, 0x99, 0x9a, 0x50,

    /* U+0052 "R" */
    0xe9, 0x9e, 0xa9, 0x90,

    /* U+0053 "S" */
    0x69, 0x86, 0x19, 0x60,

    /* U+0054 "T" */
    0xf2, 0x22, 0x22, 0x20,

    /* U+0055 "U" */
    0x99, 0x99, 0x99, 0x60,

    /* U+0056 "V" */
    0x99, 0x9a, 0xaa, 0xc0,

    /* U+0057 "W" */
    0x99, 0x99, 0xff, 0x90,

    /* U+0058 "X" */
    0x99, 0x96, 0x99, 0x90,

    /* U+0059 "Y" */
    0x99, 0x55, 0x22, 0x20,

    /* U+005A "Z" */
    0xf1, 0x24, 0x48, 0xf0,

    /* U+005B "[" */
    0xf2, 0x49, 0x24, 0xe0,

    /* U+005C "\\" */
    0x88, 0x44, 0x22, 0x21, 0x10,

    /* U+005D "]" */
    0xe4, 0x92, 0x49, 0xe0,

    /* U+005E "^" */
    0x54,

    /* U+005F "_" */
    0xf0,

    /* U+0060 "`" */
    0x90,

    /* U+0061 "a" */
    0x61, 0x79, 0x70,

    /* U+0062 "b" */
    0x88, 0xe9, 0x99, 0xe0,

    /* U+0063 "c" */
    0x69, 0x89, 0x60,

    /* U+0064 "d" */
    0x11, 0x79, 0x99, 0x70,

    /* U+0065 "e" */
    0x69, 0xf8, 0x60,

    /* U+0066 "f" */
    0x34, 0xf4, 0x44, 0x40,

    /* U+0067 "g" */
    0x79, 0x97, 0x16,

    /* U+0068 "h" */
    0x88, 0xe9, 0x99, 0x90,

    /* U+0069 "i" */
    0x20, 0xe2, 0x22, 0xf0,

    /* U+006A "j" */
    0x23, 0x92, 0x4e,

    /* U+006B "k" */
    0x88, 0x9a, 0xca, 0x90,

    /* U+006C "l" */
    0xc4, 0x44, 0x44, 0x30,

    /* U+006D "m" */
    0x9f, 0xf9, 0x90,

    /* U+006E "n" */
    0xe9, 0x99, 0x90,

    /* U+006F "o" */
    0x69, 0x99, 0x60,

    /* U+0070 "p" */
    0xe9, 0x9e, 0x88,

    /* U+0071 "q" */
    0x79, 0x97, 0x11,

    /* U+0072 "r" */
    0xbc, 0x88, 0x80,

    /* U+0073 "s" */
    0x78, 0x61, 0xe0,

    /* U+0074 "t" */
    0x44, 0xf4, 0x44, 0x30,

    /* U+0075 "u" */
    0x99, 0x99, 0x70,

    /* U+0076 "v" */
    0x99, 0xaa, 0xc0,

    /* U+0077 "w" */
    0x99, 0xff, 0x90,

    /* U+0078 "x" */
    0x99, 0x69, 0x90,

    /* U+0079 "y" */
    0x99, 0x55, 0x2c,

    /* U+007A "z" */
    0xf2, 0x48, 0xf0,

    /* U+007B "{" */
    0x34, 0x44, 0x84, 0x44, 0x30,

    /* U+007C "|" */
    0xff, 0x80,

    /* U+007D "}" */
    0xc2, 0x22, 0x12, 0x22, 0xc0,

    /* U+007E "~" */
    0x5a,

    /* U+4E00 "一" */
    0xff, 0x80,

    /* U+4E09 "三" */
    0x7f, 0x0, 0x0, 0x0, 0x3, 0xe0, 0x0, 0x0,
    0x0, 0xff, 0x80,

    /* U+4E0A "上" */
    0x8, 0x4, 0x2, 0x1, 0xe0, 0x80, 0x40, 0x20,
    0x10, 0xff, 0x80,

    /* U+4E0B "下" */
    0xff, 0x84, 0x2, 0x1, 0x80, 0xa0, 0x48, 0x20,
    0x10, 0x8, 0x0,

    /* U+4E8C "二" */
    0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0xfe,

    /* U+4E91 "云" */
    0x7f, 0x0, 0x0, 0x1f, 0xf1, 0x0, 0x90, 0x88,
    0x8a, 0x79, 0x0,

    /* U+4E94 "五" */
    0x7f, 0x4, 0x2, 0x1, 0x7, 0xf0, 0x88, 0x44,
    0x22, 0xff, 0x80,

    /* U+4ECA "今" */
    0x1c, 0x11, 0x10, 0x57, 0xd0, 0x3, 0xf8, 0x4,
    0x4, 0x1c, 0x0,

    /* U+516D "六" */
    0x8, 0x4, 0x3f, 0xe0, 0x0, 0x1, 0x8, 0x84,
    0x81, 0x80, 0x80,

    /* U+519C "农" */
    0x8, 0x7f, 0xe2, 0x22, 0x82, 0x4f, 0x28, 0x88,
    0x42, 0x70, 0x80,

    /* U+5206 "分" */
    0x2f, 0x10, 0x90, 0x48, 0x1b, 0xfc, 0x88, 0x44,
    0x42, 0x26, 0x0,

    /* U+524D "前" */
    0x24, 0x7f, 0xc0, 0x1e, 0x99, 0x4f, 0xa6, 0x53,
    0xe1, 0x91, 0x80,

    /* U+529F "功" */
    0x2, 0x71, 0x17, 0xe8, 0x94, 0x4a, 0x25, 0x23,
    0xd1, 0x3, 0x0,

    /* U+52A0 "加" */
    0x20, 0x11, 0xfe, 0xa9, 0x54, 0xaa, 0x56, 0x2b,
    0x15, 0x33, 0x80,

    /* U+5348 "午" */
    0x40, 0x3f, 0xa2, 0x1, 0xf, 0xf8, 0x40, 0x20,
    0x10, 0x8, 0x0,

    /* U+5386 "历" */
    0x7f, 0xa4, 0x12, 0xf, 0xf4, 0x8a, 0x45, 0x42,
    0xa1, 0xa3, 0x0,

    /* U+53D6 "取" */
    0xff, 0xa4, 0x5e, 0x29, 0x57, 0xaa, 0x49, 0x25,
    0xf5, 0xc, 0x80,

    /* U+540C "同" */
    0xff, 0xc0, 0x6f, 0xb0, 0x1b, 0xed, 0x16, 0xfb,
    0x1, 0x81, 0x80,

    /* U+540E "后" */
    0x7, 0x3c, 0x10, 0xf, 0xf4, 0x2, 0xfd, 0x42,
    0xa1, 0x9f, 0x80,

    /* U+56DB "四" */
    0xff, 0xca, 0x65, 0x32, 0x99, 0x4d, 0x1f, 0x3,
    0x1, 0xff, 0x80,

    /* U+56DE "回" */
    0xff, 0xc0, 0x6f, 0xb4, 0x5a, 0x2d, 0x16, 0xfb,
    0x1, 0xff, 0x80,

    /* U+591A "多" */
    0x8, 0xf, 0x18, 0x82, 0x71, 0xcf, 0x44, 0x14,
    0xc, 0x38, 0x0,

    /* U+591C "夜" */
    0x8, 0x7f, 0xca, 0x5, 0xf4, 0xae, 0x85, 0x94,
    0x8c, 0x59, 0x80,

    /* U+5929 "天" */
    0x7f, 0x4, 0x2, 0x1f, 0xf0, 0x80, 0x40, 0x50,
    0x44, 0xc1, 0x80,

    /* U+5931 "失" */
    0x48, 0x3f, 0x92, 0x11, 0xf, 0xf8, 0x40, 0x50,
    0x44, 0xc1, 0x80,

    /* U+5E74 "年" */
    0x20, 0x1f, 0xd1, 0x17, 0xe2, 0x41, 0x23, 0xfe,
    0x8, 0x4, 0x0,

    /* U+5EA6 "度" */
    0x4, 0x3f, 0xd2, 0x8f, 0xf4, 0xa2, 0xf9, 0x44,
    0x9c, 0xb1, 0x80,

    /* U+6210 "成" */
    0x5, 0x3f, 0xd1, 0xe, 0xa5, 0x52, 0xa9, 0x48,
    0xe5, 0x8d, 0x80,

    /* U+65E5 "日" */
    0xff, 0x6, 0xc, 0x1f, 0xf0, 0x60, 0xc1, 0xfe,

    /* U+65F6 "时" */
    0x1, 0x70, 0xab, 0xf4, 0x2e, 0x95, 0x2a, 0x85,
    0xc2, 0x3, 0x0,

    /* U+660E "明" */
    0xef, 0xd4, 0x6b, 0xfd, 0x1a, 0x8d, 0x7f, 0xa2,
    0x11, 0x11, 0x80,

    /* U+661F "星" */
    0x7f, 0x20, 0x9f, 0xc8, 0x27, 0xf4, 0x41, 0xfc,
    0x10, 0xff, 0x80,

    /* U+6674 "晴" */
    0x2, 0x7f, 0xe8, 0x95, 0xfe, 0x25, 0xfe, 0xa3,
    0xdf, 0x8, 0x80,

    /* U+6708 "月" */
    0x3f, 0x90, 0x4f, 0xe4, 0x12, 0x9, 0xfc, 0x82,
    0x81, 0x81, 0x80,

    /* U+671F "期" */
    0x57, 0xfe, 0x55, 0xee, 0x95, 0x4f, 0xfc, 0x12,
    0xa9, 0x89, 0x80,

    /* U+6B65 "步" */
    0x8, 0x27, 0x92, 0x1f, 0xf0, 0x82, 0x42, 0x66,
    0xc, 0x78, 0x0,

    /* U+6C14 "气" */
    0x40, 0x3f, 0xe0, 0x1f, 0xe0, 0x7, 0xf8, 0x4,
    0x1, 0x0, 0x80,

    /* U+6D88 "消" */
    0x82, 0x29, 0x42, 0xd0, 0x45, 0xf8, 0x84, 0x7e,
    0xa1, 0x91, 0x80,

    /* U+6E29 "温" */
    0x9f, 0x28, 0x87, 0xd2, 0x25, 0xf0, 0x0, 0xfe,
    0xd5, 0xbf, 0x80,

    /* U+6E7F "湿" */
    0x9f, 0xa8, 0x47, 0xf2, 0x15, 0xf8, 0x50, 0x6a,
    0x94, 0xbf, 0x80,

    /* U+7259 "牙" */
    0x7f, 0x11, 0x8, 0x84, 0x4f, 0xf8, 0x50, 0x48,
    0x44, 0xc6, 0x0,

    /* U+7535 "电" */
    0x10, 0x7f, 0xa4, 0x5f, 0xe9, 0x14, 0x8b, 0xfc,
    0x21, 0xf, 0x80,

    /* U+786E "确" */
    0xe7, 0x2c, 0x93, 0xf1, 0x5e, 0xfd, 0x56, 0xbf,
    0x51, 0xf1, 0x80,

    /* U+79D2 "秒" */
    0x1a, 0x71, 0xa, 0xbf, 0x52, 0x23, 0x32, 0xc2,
    0x46, 0x2c, 0x0,

    /* U+7EDC "络" */
    0x2f, 0xa4, 0x61, 0x5c, 0x64, 0x4c, 0xff, 0xa2,
    0x11, 0xef, 0x80,

    /* U+7F51 "网" */
    0xff, 0xc0, 0x75, 0x74, 0x5a, 0x2e, 0xaf, 0x57,
    0x1, 0x81, 0x80,

    /* U+7F6E "置" */
    0x7f, 0x2a, 0xbf, 0xe1, 0x7, 0xf2, 0x9, 0xfc,
    0x82, 0xff, 0x80,

    /* U+8282 "节" */
    0x22, 0x7f, 0xc8, 0x80, 0x7, 0xf0, 0x88, 0x44,
    0x26, 0x10, 0x0,

    /* U+84DD "蓝" */
    0x22, 0x7f, 0xc9, 0x14, 0xfa, 0xa5, 0x9, 0xfc,
    0xaa, 0xff, 0x80,

    /* U+8BA4 "认" */
    0x84, 0x22, 0x1, 0x18, 0x84, 0x42, 0x51, 0xa8,
    0xa2, 0x60, 0x80,

    /* U+8BBE "设" */
    0x4f, 0x14, 0x82, 0x59, 0x34, 0x2, 0xf9, 0x24,
    0xcc, 0x59, 0x80,

    /* U+8D25 "败" */
    0xfa, 0x45, 0xeb, 0x35, 0x1a, 0xad, 0x55, 0x4,
    0xa6, 0x94, 0x80,

    /* U+8F7D "载" */
    0x25, 0x7a, 0x49, 0x1f, 0xf2, 0x27, 0xd5, 0x45,
    0xf5, 0x14, 0x80,

    /* U+8FD4 "返" */
    0x8f, 0xa4, 0x23, 0xe9, 0x10, 0xae, 0x19, 0x33,
    0x40, 0x9f, 0x80,

    /* U+91CF "量" */
    0x7f, 0x20, 0xbf, 0xe9, 0x27, 0xf2, 0x49, 0xfc,
    0x10, 0xff, 0x80,

    /* U+95F4 "间" */
    0x9f, 0xa0, 0x4f, 0xb4, 0x5b, 0xed, 0x16, 0x8b,
    0x7d, 0x81, 0x80,

    /* U+9634 "阴" */
    0xef, 0xd4, 0x6b, 0xf9, 0x1a, 0x8d, 0x7f, 0x23,
    0x21, 0x91, 0x80,

    /* U+96E8 "雨" */
    0xff, 0x84, 0x3f, 0xf1, 0x1a, 0xac, 0x46, 0xab,
    0x11, 0x89, 0x80,

    /* U+96EA "雪" */
    0x7f, 0x4, 0x3f, 0xf5, 0x57, 0xf0, 0x9, 0xfc,
    0x2, 0x7f, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 80, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 80, .box_w = 1, .box_h = 7, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 80, .box_w = 3, .box_h = 2, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 3, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 7, .adv_w = 80, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 12, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 16, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 20, .adv_w = 80, .box_w = 1, .box_h = 2, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 21, .adv_w = 80, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 25, .adv_w = 80, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 29, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 31, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 34, .adv_w = 80, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 35, .adv_w = 80, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 36, .adv_w = 80, .box_w = 1, .box_h = 1, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 37, .adv_w = 80, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 42, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 46, .adv_w = 80, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 49, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 53, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 57, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 61, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 65, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 69, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 73, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 77, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 81, .adv_w = 80, .box_w = 1, .box_h = 5, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 82, .adv_w = 80, .box_w = 2, .box_h = 6, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 84, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 86, .adv_w = 80, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 88, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 90, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 94, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 98, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 102, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 106, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 110, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 114, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 118, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 122, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 130, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 138, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 146, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 150, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 154, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 158, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 162, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 170, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 182, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 186, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 190, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 194, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 198, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 202, .adv_w = 80, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 206, .adv_w = 80, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 211, .adv_w = 80, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 215, .adv_w = 80, .box_w = 3, .box_h = 2, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 216, .adv_w = 80, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 217, .adv_w = 80, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 218, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 221, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 225, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 228, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 232, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 235, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 239, .adv_w = 80, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 242, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 246, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 250, .adv_w = 80, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 253, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 257, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 261, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 264, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 267, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 270, .adv_w = 80, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 273, .adv_w = 80, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 276, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 279, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 282, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 286, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 292, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 295, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 298, .adv_w = 80, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 301, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 304, .adv_w = 80, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 309, .adv_w = 80, .box_w = 1, .box_h = 9, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 311, .adv_w = 80, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 316, .adv_w = 80, .box_w = 4, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 317, .adv_w = 160, .box_w = 9, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 319, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 330, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 341, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 352, .adv_w = 160, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 360, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 371, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 382, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 393, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 404, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 415, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 426, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 437, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 448, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 459, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 470, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 481, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 492, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 503, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 514, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 525, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 536, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 547, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 558, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 569, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 580, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 591, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 602, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 613, .adv_w = 160, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 621, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 632, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 643, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 654, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 665, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 676, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 687, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 698, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 709, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 720, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 731, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 742, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 753, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 764, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 775, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 786, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 797, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 808, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 819, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 830, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 841, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 852, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 863, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 874, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 885, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 896, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 907, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 918, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 929, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 940, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0x9, 0xa, 0xb, 0x8c, 0x91, 0x94, 0xca,
    0x36d, 0x39c, 0x406, 0x44d, 0x49f, 0x4a0, 0x548, 0x586,
    0x5d6, 0x60c, 0x60e, 0x8db, 0x8de, 0xb1a, 0xb1c, 0xb29,
    0xb31, 0x1074, 0x10a6, 0x1410, 0x17e5, 0x17f6, 0x180e, 0x181f,
    0x1874, 0x1908, 0x191f, 0x1d65, 0x1e14, 0x1f88, 0x2029, 0x207f,
    0x2459, 0x2735, 0x2a6e, 0x2bd2, 0x30dc, 0x3151, 0x316e, 0x3482,
    0x36dd, 0x3da4, 0x3dbe, 0x3f25, 0x417d, 0x41d4, 0x43cf, 0x47f4,
    0x4834, 0x48e8, 0x48ea
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 19968, .range_length = 18667, .glyph_id_start = 96,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 59, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 2,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t fusion_pixel_10_calendar_zh = {
#else
lv_font_t fusion_pixel_10_calendar_zh = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 9,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FUSION_PIXEL_10_CALENDAR_ZH*/

