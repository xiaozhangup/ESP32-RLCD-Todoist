#pragma once

#include <lvgl.h>

typedef void (*LvglFlushCb)(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map);

bool lvglPortInit(int width, int height, LvglFlushCb flush_cb);
bool lvglLock(int timeout_ms = -1);
void lvglUnlock();
