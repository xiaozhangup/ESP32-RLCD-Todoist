static const char *wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:
      return "SCAN_COMPLETED";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

static String urlEncode(const String &value) {
  String encoded;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); i++) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += hex[c >> 4];
      encoded += hex[c & 0x0f];
    }
  }
  return encoded;
}

static String truncateUtf8(const String &text, int max_columns) {
  String out;
  int columns = 0;
  for (size_t i = 0; i < text.length();) {
    const uint8_t c = static_cast<uint8_t>(text[i]);
    const int char_len = (c & 0x80) == 0 ? 1 : ((c & 0xe0) == 0xc0 ? 2 : ((c & 0xf0) == 0xe0 ? 3 : 4));
    const int char_cols = char_len == 1 ? 1 : 2;
    if (columns + char_cols > max_columns) {
      out += "...";
      return out;
    }
    for (int j = 0; j < char_len && i + j < text.length(); j++) {
      out += text[i + j];
    }
    columns += char_cols;
    i += char_len;
  }
  return out;
}

static std::vector<String> wrapUtf8Lines(const String &text, int max_columns, int max_lines) {
  std::vector<String> lines;
  String current;
  int columns = 0;
  for (size_t i = 0; i < text.length() && static_cast<int>(lines.size()) < max_lines;) {
    const uint8_t c = static_cast<uint8_t>(text[i]);
    const int char_len = (c & 0x80) == 0 ? 1 : ((c & 0xe0) == 0xc0 ? 2 : ((c & 0xf0) == 0xe0 ? 3 : 4));
    if (c == '\n') {
      lines.push_back(current);
      current = "";
      columns = 0;
      i++;
      continue;
    }

    const int char_cols = char_len == 1 ? 1 : 2;
    if (columns + char_cols > max_columns && current.length() > 0) {
      lines.push_back(current);
      current = "";
      columns = 0;
      if (static_cast<int>(lines.size()) >= max_lines) {
        break;
      }
    }

    for (int j = 0; j < char_len && i + j < text.length(); j++) {
      current += text[i + j];
    }
    columns += char_cols;
    i += char_len;
  }

  if (current.length() > 0 && static_cast<int>(lines.size()) < max_lines) {
    lines.push_back(current);
  }

  if (lines.empty()) {
    lines.push_back("");
  }
  return lines;
}

static int utf8ColumnCount(const String &text) {
  int columns = 0;
  for (size_t i = 0; i < text.length();) {
    const uint8_t c = static_cast<uint8_t>(text[i]);
    const int char_len = (c & 0x80) == 0 ? 1 : ((c & 0xe0) == 0xc0 ? 2 : ((c & 0xf0) == 0xe0 ? 3 : 4));
    columns += char_len == 1 ? 1 : 2;
    i += char_len;
  }
  return columns;
}

static TaskDisplayItem buildTaskDisplayItem(const TodoistTaskItem &task, int index) {
  String prefix;
  prefix += String(index + 1);
  prefix += ". ";

  String body = task.content;
  if (task.due.length() > 0) {
    body += " 截止:";
    body += task.due;
  }

  TaskDisplayItem item;
  const int prefix_columns = utf8ColumnCount(prefix);
  const int body_columns = max(8, TASK_TEXT_COLUMNS - prefix_columns);
  const std::vector<String> body_lines = wrapUtf8Lines(body, body_columns, TASK_MAX_LINES);
  const int body_x = measureTextWidth(prefix, BODY_SCALE);

  item.lines.reserve(body_lines.size());
  item.x_offsets.reserve(body_lines.size());
  for (size_t i = 0; i < body_lines.size(); i++) {
    if (i == 0) {
      item.lines.push_back(prefix + body_lines[i]);
      item.x_offsets.push_back(0);
    } else {
      item.lines.push_back(body_lines[i]);
      item.x_offsets.push_back(body_x);
    }
  }

  if (item.lines.empty()) {
    item.lines.push_back(prefix);
    item.x_offsets.push_back(0);
  }
  return item;
}

static int taskDisplayLineCount(const TodoistTaskItem &task, int index) {
  return min(TASK_MAX_LINES, max(1, static_cast<int>(buildTaskDisplayItem(task, index).lines.size())));
}

static int countTaskPages(const std::vector<TodoistTaskItem> &source_tasks) {
  if (source_tasks.empty()) {
    return 1;
  }

  int page_count = 1;
  int used_lines = 0;
  for (size_t i = 0; i < source_tasks.size(); i++) {
    const int line_count = taskDisplayLineCount(source_tasks[i], static_cast<int>(i));
    if (used_lines > 0 && used_lines + line_count > TASK_LINE_SLOTS_PER_PAGE) {
      page_count++;
      used_lines = 0;
    }
    used_lines += line_count;
  }
  return page_count;
}

static std::vector<TaskDisplayItem> buildTaskPageItems(const std::vector<TodoistTaskItem> &source_tasks, int page) {
  std::vector<TaskDisplayItem> page_items;
  int current_page = 0;
  int used_lines = 0;

  for (size_t i = 0; i < source_tasks.size(); i++) {
    TaskDisplayItem item = buildTaskDisplayItem(source_tasks[i], static_cast<int>(i));
    const int line_count = min(TASK_MAX_LINES, max(1, static_cast<int>(item.lines.size())));
    if (used_lines > 0 && used_lines + line_count > TASK_LINE_SLOTS_PER_PAGE) {
      current_page++;
      used_lines = 0;
    }

    if (current_page == page) {
      page_items.push_back(item);
    } else if (current_page > page) {
      break;
    }

    used_lines += line_count;
  }

  return page_items;
}

static uint32_t decodeUtf8Char(const char *text, size_t len, size_t &index) {
  if (index >= len) {
    return 0;
  }

  const uint8_t c0 = static_cast<uint8_t>(text[index]);
  if ((c0 & 0x80) == 0) {
    index++;
    return c0;
  }

  if ((c0 & 0xe0) == 0xc0 && index + 1 < len) {
    const uint8_t c1 = static_cast<uint8_t>(text[index + 1]);
    index += 2;
    return ((c0 & 0x1f) << 6) | (c1 & 0x3f);
  }

  if ((c0 & 0xf0) == 0xe0 && index + 2 < len) {
    const uint8_t c1 = static_cast<uint8_t>(text[index + 1]);
    const uint8_t c2 = static_cast<uint8_t>(text[index + 2]);
    index += 3;
    return ((c0 & 0x0f) << 12) | ((c1 & 0x3f) << 6) | (c2 & 0x3f);
  }

  if ((c0 & 0xf8) == 0xf0 && index + 3 < len) {
    const uint8_t c1 = static_cast<uint8_t>(text[index + 1]);
    const uint8_t c2 = static_cast<uint8_t>(text[index + 2]);
    const uint8_t c3 = static_cast<uint8_t>(text[index + 3]);
    index += 4;
    return ((c0 & 0x07) << 18) | ((c1 & 0x3f) << 12) | ((c2 & 0x3f) << 6) | (c3 & 0x3f);
  }

  index++;
  return '?';
}

static int glyphAdvance(const lv_font_glyph_dsc_t &glyph) {
  return glyph.adv_w;
}

static int scaleCeil(int value, int scale_num, int scale_den) {
  return (value * scale_num + scale_den - 1) / scale_den;
}

static int measureTextWidthScaled(const String &text, int scale_num, int scale_den) {
  int width = 0;
  size_t index = 0;
  const char *raw = text.c_str();
  const size_t len = text.length();
  while (index < len) {
    const uint32_t cp = decodeUtf8Char(raw, len, index);
    if (cp == '\n') {
      break;
    }

    lv_font_glyph_dsc_t glyph;
    if (lv_font_get_glyph_dsc(&fusion_pixel_10_zh, &glyph, cp, 0)) {
      width += scaleCeil(glyphAdvance(glyph), scale_num, scale_den);
    } else {
      width += scaleCeil(fusion_pixel_10_zh.line_height, scale_num, scale_den) / 2;
    }
  }
  return width;
}

static int measureTextWidth(const String &text, int scale) {
  return measureTextWidthScaled(text, scale, 1);
}

static int measureTextMinYOffset(const String &text) {
  int min_y = 0;
  size_t index = 0;
  const char *raw = text.c_str();
  const size_t len = text.length();
  while (index < len) {
    const uint32_t cp = decodeUtf8Char(raw, len, index);
    if (cp == '\n') {
      break;
    }

    lv_font_glyph_dsc_t glyph;
    if (lv_font_get_glyph_dsc(&fusion_pixel_10_zh, &glyph, cp, 0)) {
      const int glyph_y = fusion_pixel_10_zh.line_height - fusion_pixel_10_zh.base_line -
                          glyph.box_h - glyph.ofs_y;
      min_y = min(min_y, glyph_y);
    }
  }
  return min_y;
}

static void clearCanvas() {
  if (!ui_canvas_buf) {
    return;
  }
  for (int i = 0; i < RlcdDitherDisplay::WIDTH * RlcdDitherDisplay::HEIGHT; i++) {
    ui_canvas_buf[i] = CANVAS_WHITE;
  }
}

static void setCanvasPixel(int x, int y, uint16_t color) {
  if (!ui_canvas_buf || x < 0 || x >= RlcdDitherDisplay::WIDTH || y < 0 || y >= RlcdDitherDisplay::HEIGHT) {
    return;
  }
  ui_canvas_buf[y * RlcdDitherDisplay::WIDTH + x] = color;
}

static void drawPixelBlock(int x, int y, int scale, uint16_t color) {
  for (int yy = 0; yy < scale; yy++) {
    for (int xx = 0; xx < scale; xx++) {
      setCanvasPixel(x + xx, y + yy, color);
    }
  }
}

static void drawHLine(int x1, int x2, int y, int thickness, uint16_t color) {
  if (x2 < x1) {
    return;
  }
  for (int yy = 0; yy < thickness; yy++) {
    for (int x = x1; x <= x2; x++) {
      setCanvasPixel(x, y + yy, color);
    }
  }
}

static void drawVLine(int x, int y1, int y2, int thickness, uint16_t color) {
  if (y2 < y1) {
    return;
  }
  for (int xx = 0; xx < thickness; xx++) {
    for (int y = y1; y <= y2; y++) {
      setCanvasPixel(x + xx, y, color);
    }
  }
}

static void drawRect(int x, int y, int w, int h, uint16_t color) {
  if (w <= 0 || h <= 0) {
    return;
  }
  for (int yy = 0; yy < h; yy++) {
    for (int xx = 0; xx < w; xx++) {
      setCanvasPixel(x + xx, y + yy, color);
    }
  }
}

static void drawRectOutline(int x, int y, int w, int h, int thickness, uint16_t color) {
  drawHLine(x, x + w - 1, y, thickness, color);
  drawHLine(x, x + w - 1, y + h - thickness, thickness, color);
  drawVLine(x, y, y + h - 1, thickness, color);
  drawVLine(x + w - thickness, y, y + h - 1, thickness, color);
}

static void drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
  int dx = abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  for (;;) {
    setCanvasPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

static void drawCircleOutline(int cx, int cy, int radius, uint16_t color) {
  int x = radius;
  int y = 0;
  int err = 0;
  while (x >= y) {
    setCanvasPixel(cx + x, cy + y, color);
    setCanvasPixel(cx + y, cy + x, color);
    setCanvasPixel(cx - y, cy + x, color);
    setCanvasPixel(cx - x, cy + y, color);
    setCanvasPixel(cx - x, cy - y, color);
    setCanvasPixel(cx - y, cy - x, color);
    setCanvasPixel(cx + y, cy - x, color);
    setCanvasPixel(cx + x, cy - y, color);

    y++;
    if (err <= 0) {
      err += 2 * y + 1;
    }
    if (err > 0) {
      x--;
      err -= 2 * x + 1;
    }
  }
}

static void drawTextScaled(const String &text, int x, int y, int scale_num, int scale_den,
                           int max_x = RlcdDitherDisplay::WIDTH - 1) {
  if (!ui_canvas_buf || scale_num <= 0 || scale_den <= 0) {
    return;
  }

  const int min_y = measureTextMinYOffset(text);
  int cursor_x = x;
  size_t index = 0;
  const char *raw = text.c_str();
  const size_t len = text.length();
  while (index < len) {
    const uint32_t cp = decodeUtf8Char(raw, len, index);
    if (cp == '\n') {
      break;
    }

    lv_font_glyph_dsc_t glyph;
    if (!lv_font_get_glyph_dsc(&fusion_pixel_10_zh, &glyph, cp, 0)) {
      cursor_x += scaleCeil(fusion_pixel_10_zh.line_height, scale_num, scale_den) / 2;
      continue;
    }

    glyph.req_raw_bitmap = 1;
    const uint8_t *bitmap = static_cast<const uint8_t *>(glyph.resolved_font->get_glyph_bitmap(&glyph, nullptr));
    if (bitmap && glyph.box_w > 0 && glyph.box_h > 0) {
      const int raw_y = fusion_pixel_10_zh.line_height - fusion_pixel_10_zh.base_line -
                        glyph.box_h - glyph.ofs_y;
      const int glyph_x = cursor_x + scaleCeil(glyph.ofs_x, scale_num, scale_den);
      const int glyph_y = y + scaleCeil(raw_y - min_y, scale_num, scale_den);
      for (int gy = 0; gy < glyph.box_h; gy++) {
        for (int gx = 0; gx < glyph.box_w; gx++) {
          int byte_index;
          int bit_index;
          if (glyph.stride > 0) {
            byte_index = gy * glyph.stride + (gx >> 3);
            bit_index = gx & 0x07;
          } else {
            const int packed_bit = gy * glyph.box_w + gx;
            byte_index = packed_bit >> 3;
            bit_index = packed_bit & 0x07;
          }
          if (bitmap[byte_index] & (0x80 >> bit_index)) {
            const int px = glyph_x + (gx * scale_num) / scale_den;
            const int py = glyph_y + (gy * scale_num) / scale_den;
            const int next_px = glyph_x + ((gx + 1) * scale_num + scale_den - 1) / scale_den;
            const int next_py = glyph_y + ((gy + 1) * scale_num + scale_den - 1) / scale_den;
            if (px <= max_x) {
              drawRect(px, py, max(1, next_px - px), max(1, next_py - py), CANVAS_BLACK);
            }
          }
        }
      }
    }

    cursor_x += scaleCeil(glyphAdvance(glyph), scale_num, scale_den);
    if (cursor_x > max_x) {
      break;
    }
  }
}

static void drawText(const String &text, int x, int y, int scale, int max_x = RlcdDitherDisplay::WIDTH - 1) {
  drawTextScaled(text, x, y, scale, 1, max_x);
}

static void drawRightText(const String &text, int right_x, int y, int scale) {
  const int width = measureTextWidth(text, scale);
  drawText(text, max(0, right_x - width), y, scale, right_x);
}

static void drawRightTextScaled(const String &text, int right_x, int y, int scale_num, int scale_den) {
  const int width = measureTextWidthScaled(text, scale_num, scale_den);
  drawTextScaled(text, max(0, right_x - width), y, scale_num, scale_den, right_x);
}

static String buildEnvironmentStatusText(const WeatherInfo &weather_snapshot, const SensorInfo &sensor_snapshot) {
  String text = weather_snapshot.valid ? weather_snapshot.description : String("...");
  if (weather_snapshot.valid) {
    text += " 室外";
    text += weather_snapshot.temp_c;
    text += "C";
  }
  if (sensor_snapshot.valid) {
    text += " 室内";
    text += String(sensor_snapshot.temp_c, 1);
    text += "C";
  } else {
    text += " 室内...";
  }
  return text;
}

static String buildClockTimeText(const struct tm &now_tm) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
  return String(buffer);
}

static String buildClockDateText(const struct tm &now_tm) {
  static constexpr const char *WEEKDAYS[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
  char buffer[48];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %s",
           now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday, WEEKDAYS[now_tm.tm_wday]);
  return String(buffer);
}

static String buildTopTimeText() {
  struct tm now_tm = {};
  if (getLocalTime(&now_tm, 0)) {
    static constexpr const char *WEEKDAYS[] = {"日", "一", "二", "三", "四", "五", "六"};
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%04d 星期%s %02d:%02d",
             now_tm.tm_year + 1900, WEEKDAYS[now_tm.tm_wday], now_tm.tm_hour, now_tm.tm_min);
    return String(buffer);
  }
  return "时间同步中";
}

static void setAppStatus(const String &status) {
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  app_status = status;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
}

static void setBatteryPercent(int percent) {
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  battery_percent = percent;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
}

static void setRefreshVisualState(RefreshVisualState state) {
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  refresh_visual_state = state;
  refresh_done_until_ms = state == RefreshVisualState::Done ? millis() + REFRESH_DONE_VISIBLE_MS : 0;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
}

static bool isRefreshSpinning() {
  bool spinning = false;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  spinning = refresh_visual_state == RefreshVisualState::Spinning;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  return spinning;
}

static bool isRefreshVisualActive() {
  RefreshVisualState state = RefreshVisualState::Idle;
  uint32_t done_until = 0;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  state = refresh_visual_state;
  done_until = refresh_done_until_ms;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  return state == RefreshVisualState::Spinning || (state == RefreshVisualState::Done && millis() <= done_until);
}

static int wifiSignalLevel() {
  if (WiFi.status() != WL_CONNECTED) {
    return 0;
  }
  const int rssi = WiFi.RSSI();
  if (rssi >= -55) {
    return 4;
  }
  if (rssi >= -65) {
    return 3;
  }
  if (rssi >= -75) {
    return 2;
  }
  return 1;
}

static void drawWifiIcon(int x, int y) {
  const int level = wifiSignalLevel();
  static constexpr int BAR_W = 2;
  static constexpr int GAP = 2;
  for (int i = 0; i < 4; i++) {
    const int h = 3 + i * 3;
    const int bx = x + i * (BAR_W + GAP);
    const int by = y + 12 - h;
    drawRectOutline(bx, by, BAR_W, h, 1, CANVAS_BLACK);
    if (i < level) {
      drawRect(bx, by, BAR_W, h, CANVAS_BLACK);
    }
  }
}

static void drawBluetoothIcon(int x, int y) {
  static constexpr uint8_t MAP[7] = {
    0b00100,
    0b00110,
    0b10101,
    0b01110,
    0b10101,
    0b00110,
    0b00100,
  };
  for (int yy = 0; yy < 7; yy++) {
    for (int xx = 0; xx < 5; xx++) {
      if (MAP[yy] & (0x10 >> xx)) {
        drawRect(x + xx * 2, y + yy * 2, 2, 2, CANVAS_BLACK);
      }
    }
  }
}

static void drawTfCardIcon(int x, int y) {
  static constexpr uint8_t MAP[6] = {
    0b111111,
    0b101010,
    0b111111,
    0b011111,
    0b111111,
    0b111111,
  };
  for (int yy = 0; yy < 6; yy++) {
    for (int xx = 0; xx < 6; xx++) {
      if (MAP[yy] & (0x20 >> xx)) {
        drawRect(x + xx * 2, y + yy * 2, 2, 2, CANVAS_BLACK);
      }
    }
  }
}

static void drawBatteryIcon(int x, int y, int battery) {
  static constexpr int BODY_W = 24;
  static constexpr int BODY_H = 12;
  static constexpr int CAP_W = 3;
  drawRectOutline(x, y - 1, BODY_W, BODY_H, 2, CANVAS_BLACK);
  drawRect(x + BODY_W, y + 2, CAP_W, 6, CANVAS_BLACK);

  if (battery >= 0) {
    const int clamped = min(100, max(0, battery));
    const int fill_w = (BODY_W - 6) * clamped / 100;
    if (fill_w > 0) {
      drawRect(x + 3, y + 2, fill_w, BODY_H - 6, CANVAS_BLACK);
    }
  }
}

static void drawRefreshRing(int x, int y, int gap_start, int gap_len) {
  static constexpr uint16_t RING[10] = {
    0b0011111100,
    0b0110000110,
    0b1100000011,
    0b1000000001,
    0b1000000001,
    0b1000000001,
    0b1000000001,
    0b1100000011,
    0b0110000110,
    0b0011111100,
  };
  static constexpr float REFRESH_TWO_PI = 6.28318530718f;
  static constexpr int SECTORS = 20;
  for (int yy = 0; yy < 10; yy++) {
    for (int xx = 0; xx < 10; xx++) {
      if (!(RING[yy] & (0x0200 >> xx))) {
        continue;
      }

      bool in_gap = false;
      if (gap_len > 0) {
        float angle = atan2f(static_cast<float>(yy) - 4.5f, static_cast<float>(xx) - 4.5f);
        if (angle < 0.0f) {
          angle += REFRESH_TWO_PI;
        }
        const int sector = min(SECTORS - 1, static_cast<int>(angle * SECTORS / REFRESH_TWO_PI));
        in_gap = (sector - gap_start + SECTORS) % SECTORS < gap_len;
      }
      if (!in_gap) {
        drawRect(x + xx * 2, y + yy * 2, 2, 2, CANVAS_BLACK);
      }
    }
  }
}

static void drawRefreshSpinner(int x, int y) {
  static constexpr int SECTORS = 20;
  static constexpr int GAP_SECTORS = 4;
  const int gap_start = (millis() / SPINNER_REFRESH_MS) % SECTORS;
  drawRefreshRing(x, y, gap_start, GAP_SECTORS);
}

static void drawRefreshDoneIcon(int x, int y) {
  static constexpr int CHECK[5][2] = {
    {3, 5}, {4, 6}, {5, 6}, {6, 5}, {7, 4},
  };
  drawRefreshRing(x, y, 0, 0);
  for (int i = 0; i < 5; i++) {
    drawRect(x + CHECK[i][0] * 2, y + CHECK[i][1] * 2, 2, 2, CANVAS_BLACK);
  }
}

static void drawRefreshIcon(int x, int y, RefreshVisualState state) {
  if (state == RefreshVisualState::Spinning) {
    drawRefreshSpinner(x, y);
  } else if (state == RefreshVisualState::Done) {
    drawRefreshDoneIcon(x, y);
  }
}

static int drawRightStatusArea(int battery_snapshot, bool tf_card_snapshot) {
  // String percent = "--%";
  // if (battery_snapshot >= 0) {
  //   percent = String(battery_snapshot) + "%";
  // }

  // const int percent_width = measureTextWidthScaled(percent, SMALL_SCALE_NUM, SMALL_SCALE_DEN);
  // const int percent_x = RlcdDitherDisplay::WIDTH - 4 - percent_width;
  const int percent_x = RlcdDitherDisplay::WIDTH - 4;
  // drawTextScaled(percent, percent_x, 4, SMALL_SCALE_NUM, SMALL_SCALE_DEN);

  const int battery_x = percent_x - 33;
  drawBatteryIcon(battery_x, 13, battery_snapshot);

  static constexpr int ICON_GAP = 7;
  static constexpr int WIFI_W = 14;
  static constexpr int BLUETOOTH_W = 10;
  static constexpr int TF_CARD_W = 12;

  int next_left = battery_x - ICON_GAP;
  if (tf_card_snapshot) {
    const int tf_x = next_left - TF_CARD_W;
    drawTfCardIcon(tf_x, 12);
    next_left = tf_x - ICON_GAP;
  }

  const int wifi_x = next_left - WIFI_W;
  drawWifiIcon(wifi_x, 12);

  int time_right = wifi_x - ICON_GAP;
  if (ble_client_count > 0) {
    const int bluetooth_x = time_right - BLUETOOTH_W;
    drawBluetoothIcon(bluetooth_x, 11);
    time_right = bluetooth_x - ICON_GAP;
  }
  drawRightTextScaled(buildTopTimeText(), time_right, 4, SMALL_SCALE_NUM, SMALL_SCALE_DEN);
  return time_right - measureTextWidthScaled(buildTopTimeText(), SMALL_SCALE_NUM, SMALL_SCALE_DEN);
}

static void renderClockScreen(int battery_snapshot, bool tf_card_snapshot,
                              const WeatherInfo &weather_snapshot, const SensorInfo &sensor_snapshot) {
  struct tm now_tm = {};
  drawRightStatusArea(battery_snapshot, tf_card_snapshot);

  if (!getLocalTime(&now_tm, 0)) {
    const String waiting = "时间同步中";
    const int waiting_width = measureTextWidth(waiting, BODY_SCALE);
    drawText(waiting, (RlcdDitherDisplay::WIDTH - waiting_width) / 2, 128, BODY_SCALE);
    return;
  }

  const String time_text = buildClockTimeText(now_tm);
  const String date_text = buildClockDateText(now_tm);
  const int time_width = measureTextWidth(time_text, CLOCK_SCALE);
  const int date_width = measureTextWidth(date_text, BODY_SCALE);
  drawText(time_text, (RlcdDitherDisplay::WIDTH - time_width) / 2, 62, CLOCK_SCALE);
  drawText(date_text, (RlcdDitherDisplay::WIDTH - date_width) / 2, 158, BODY_SCALE);

  String weather_text = "天气: 未配置";
  if (weather_snapshot.valid) {
    weather_text = weather_snapshot.description;
    weather_text += " ";
    weather_text += weather_snapshot.temp_c;
    weather_text += "C ";
    weather_text += weather_snapshot.humidity;
    weather_text += "%";
  } else if (!isConfigured(OPENWEATHER_API_KEY)) {
    weather_text = "天气: 未配置OpenWeather";
  } else {
    weather_text = "天气: 获取中";
  }

  String sensor_text;
  if (sensor_snapshot.valid) {
    sensor_text = "室内 ";
    sensor_text += String(sensor_snapshot.temp_c, 1);
    sensor_text += "C ";
    sensor_text += String(sensor_snapshot.humidity, 1);
    sensor_text += "%";
  } else {
    sensor_text = "室内温湿度读取中";
  }

  const String env_text = weather_text + " | " + sensor_text;
  const int env_width = measureTextWidth(env_text, BODY_SCALE);
  drawText(env_text, (RlcdDitherDisplay::WIDTH - env_width) / 2, 218, BODY_SCALE);
}

static void renderTaskScreen(const std::vector<TaskDisplayItem> &task_items,
                             int task_count,
                             int page,
                             int page_count,
                             int battery_snapshot,
                             bool tf_card_snapshot,
                             const WeatherInfo &weather_snapshot,
                             const SensorInfo &sensor_snapshot,
                             const String &project_name,
                             RefreshVisualState refresh_state) {
  drawText("Todoist", 6, 0, TITLE_SCALE);
  drawRightStatusArea(battery_snapshot, tf_card_snapshot);
  drawRightTextScaled(truncateUtf8(buildEnvironmentStatusText(weather_snapshot, sensor_snapshot), 33),
                      RlcdDitherDisplay::WIDTH - 8, 25, SMALL_SCALE_NUM, SMALL_SCALE_DEN);
  if (refresh_state != RefreshVisualState::Idle) {
    static constexpr int REFRESH_ICON_SIZE = 20;
    const int title_right = 6 + measureTextWidth("Todoist", TITLE_SCALE);
    const int title_bottom = scaleCeil(fusion_pixel_10_zh.line_height, TITLE_SCALE, 1);
    drawRefreshIcon(title_right + 10, max(0, title_bottom - REFRESH_ICON_SIZE - 24), refresh_state);
  }
  drawText(String("任务: ") + task_count + " active", 8, 52, BODY_SCALE);

  if (task_items.empty()) {
    drawText("暂无任务", 8, 98, BODY_SCALE);
  } else {
    static constexpr int TASK_START_Y = 82;
    static constexpr int TASK_LINE_H = 30;
    int used_lines = 0;
    for (const TaskDisplayItem &item : task_items) {
      const int line_count = min(TASK_MAX_LINES, max(1, static_cast<int>(item.lines.size())));
      const int y = TASK_START_Y + used_lines * TASK_LINE_H;
      for (int line_index = 0; line_index < line_count; line_index++) {
        const int line_x = 8 + (line_index < static_cast<int>(item.x_offsets.size()) ? item.x_offsets[line_index] : 0);
        drawText(item.lines[line_index], line_x, y + line_index * TASK_LINE_H, BODY_SCALE, RlcdDitherDisplay::WIDTH - 8);
      }
      drawHLine(8, RlcdDitherDisplay::WIDTH - 8, y + line_count * TASK_LINE_H - 1, BODY_SCALE, CANVAS_BLACK);
      used_lines += line_count;
    }
  }

  String footer;
  footer += task_count;
  footer += " tasks";
  if (page_count > 1) {
    footer += " | ";
    footer += page + 1;
    footer += "/";
    footer += page_count;
    footer += " KEY下一页";
  }
  const String project_text = truncateUtf8(project_name.length() > 0 ? project_name : String("未加载"), 18);
  const int project_width = measureTextWidthScaled(project_text, SMALL_SCALE_NUM, SMALL_SCALE_DEN);
  drawTextScaled(footer, 8, 270, SMALL_SCALE_NUM, SMALL_SCALE_DEN,
                 max(8, RlcdDitherDisplay::WIDTH - project_width - 20));
  drawRightTextScaled(project_text, RlcdDitherDisplay::WIDTH - 8, 270, SMALL_SCALE_NUM, SMALL_SCALE_DEN);
}

static void renderProjectMenu(const std::vector<TodoistProjectItem> &projects, int selected) {
  static constexpr int BOX_W = 344;
  static constexpr int BOX_H = 224;
  static constexpr int BOX_X = (RlcdDitherDisplay::WIDTH - BOX_W) / 2;
  static constexpr int BOX_Y = (RlcdDitherDisplay::HEIGHT - BOX_H) / 2;
  static constexpr int PAD = 14;
  drawRect(BOX_X, BOX_Y, BOX_W, BOX_H, CANVAS_WHITE);
  drawRectOutline(BOX_X, BOX_Y, BOX_W, BOX_H, 2, CANVAS_BLACK);
  drawText("选择项目", BOX_X + PAD, BOX_Y + 12, BODY_SCALE);
  drawHLine(BOX_X + PAD, BOX_X + BOX_W - PAD, BOX_Y + 44, 2, CANVAS_BLACK);

  const int count = static_cast<int>(projects.size());
  if (count == 0) {
    drawText("项目加载中", BOX_X + PAD, BOX_Y + 82, BODY_SCALE, BOX_X + BOX_W - PAD);
    return;
  }
  if (selected < 0 || selected >= count) {
    selected = 0;
  }
  int start = selected - 2;
  if (start < 0) {
    start = 0;
  }
  const int max_start = max(0, count - 5);
  if (start > max_start) {
    start = max_start;
  }
  for (int i = 0; i < 5 && start + i < count; i++) {
    const int index = start + i;
    String line = index == selected ? "> " : "  ";
    line += truncateUtf8(projects[index].name, 18);
    drawText(line, BOX_X + PAD, BOX_Y + 62 + i * 28, BODY_SCALE, BOX_X + BOX_W - PAD);
  }
  drawTextScaled("KEY上 BOOT下 长按BOOT确定", BOX_X + PAD, BOX_Y + BOX_H - 28,
                 SMALL_SCALE_NUM, SMALL_SCALE_DEN, BOX_X + BOX_W - PAD);
}

static void renderBleOverlay(const BleOverlayInfo &overlay) {
  if (!overlay.visible) {
    return;
  }

  static constexpr int BOX_W = 332;
  static constexpr int BOX_H = 224;
  static constexpr int BOX_X = (RlcdDitherDisplay::WIDTH - BOX_W) / 2;
  static constexpr int BOX_Y = (RlcdDitherDisplay::HEIGHT - BOX_H) / 2;
  static constexpr int PAD = 14;
  drawRect(BOX_X, BOX_Y, BOX_W, BOX_H, CANVAS_WHITE);
  drawRectOutline(BOX_X, BOX_Y, BOX_W, BOX_H, 2, CANVAS_BLACK);

  const uint32_t now_ms = millis();
  const uint32_t remaining_s = static_cast<int32_t>(overlay.visible_until_ms - now_ms) > 0
                                 ? (overlay.visible_until_ms - now_ms + 999) / 1000
                                 : 0;
  const String countdown = String(remaining_s) + "s";
  drawRightText(countdown, BOX_X + BOX_W - PAD, BOX_Y + 14, BODY_SCALE);

  const String title = truncateUtf8(overlay.title, 22);
  drawText(title, BOX_X + PAD, BOX_Y + 14, BODY_SCALE, BOX_X + BOX_W - PAD);
  drawHLine(BOX_X + PAD, BOX_X + BOX_W - PAD, BOX_Y + 46, 2, CANVAS_BLACK);

  const std::vector<String> content_lines = wrapUtf8Lines(overlay.content, 30, 5);
  for (size_t i = 0; i < content_lines.size(); i++) {
    drawText(content_lines[i], BOX_X + PAD, BOX_Y + 60 + static_cast<int>(i) * 28,
             BODY_SCALE, BOX_X + BOX_W - PAD);
  }
}

static void renderTasks() {
  std::vector<TaskDisplayItem> task_items;
  int footer_task_count = 0;
  int footer_page = 0;
  int footer_page_count = 1;
  int battery_snapshot = -1;
  bool tf_card_snapshot = false;
  WeatherInfo weather_snapshot;
  SensorInfo sensor_snapshot;
  BleOverlayInfo overlay_snapshot;
  RefreshVisualState refresh_snapshot = RefreshVisualState::Idle;
  ScreenMode mode_snapshot = ScreenMode::Tasks;
  std::vector<TodoistProjectItem> project_snapshot;
  String project_name_snapshot;
  int project_menu_index_snapshot = 0;
  bool project_menu_open_snapshot = false;
  const uint32_t now_ms = millis();

  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  if (refresh_visual_state == RefreshVisualState::Done && refresh_done_until_ms != 0 &&
      static_cast<int32_t>(now_ms - refresh_done_until_ms) >= 0) {
    refresh_visual_state = RefreshVisualState::Idle;
    refresh_done_until_ms = 0;
  }
  if (ble_overlay.visible && static_cast<int32_t>(now_ms - ble_overlay.visible_until_ms) >= 0) {
    ble_overlay.visible = false;
  }

  const int task_count = static_cast<int>(tasks.size());
  const int page_count = countTaskPages(tasks);
  if (task_page >= page_count) {
    task_page = 0;
  }
  if (task_count > 0) {
    task_items = buildTaskPageItems(tasks, task_page);
  }

  footer_task_count = task_count;
  footer_page = task_page;
  footer_page_count = page_count;
  battery_snapshot = battery_percent;
  tf_card_snapshot = tf_card_ready;
  weather_snapshot = weather_info;
  sensor_snapshot = sensor_info;
  overlay_snapshot = ble_overlay;
  refresh_snapshot = refresh_visual_state;
  mode_snapshot = screen_mode;
  project_snapshot = todoist_projects;
  project_menu_index_snapshot = project_menu_index;
  project_menu_open_snapshot = project_menu_open;
  if (current_project_index >= 0 && current_project_index < static_cast<int>(todoist_projects.size())) {
    project_name_snapshot = todoist_projects[current_project_index].name;
  }

  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }

  clearCanvas();
  if (mode_snapshot == ScreenMode::Clock) {
    renderClockScreen(battery_snapshot, tf_card_snapshot, weather_snapshot, sensor_snapshot);
  } else {
    renderTaskScreen(task_items, footer_task_count, footer_page, footer_page_count,
                     battery_snapshot, tf_card_snapshot, weather_snapshot, sensor_snapshot,
                     project_name_snapshot, refresh_snapshot);
  }
  renderBleOverlay(overlay_snapshot);
  if (project_menu_open_snapshot) {
    renderProjectMenu(project_snapshot, project_menu_index_snapshot);
  }
  if (ui_canvas) {
    lv_obj_invalidate(ui_canvas);
  }
}

static void updateUi() {
  if (!lvglLock(1000)) {
    return;
  }
  renderTasks();
  lvglUnlock();
}

static void createUi() {
  lv_obj_t *screen = lv_screen_active();
  lv_obj_remove_style_all(screen);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(screen, lv_color_white(), 0);

  ui_canvas_buf = static_cast<uint16_t *>(heap_caps_malloc(
    RlcdDitherDisplay::WIDTH * RlcdDitherDisplay::HEIGHT * sizeof(uint16_t),
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!ui_canvas_buf) {
    ui_canvas_buf = static_cast<uint16_t *>(malloc(RlcdDitherDisplay::WIDTH * RlcdDitherDisplay::HEIGHT * sizeof(uint16_t)));
  }
  if (!ui_canvas_buf) {
    Serial.println("[UI] Failed to allocate canvas buffer");
    return;
  }

  ui_canvas = lv_canvas_create(screen);
  lv_obj_remove_style_all(ui_canvas);
  lv_canvas_set_buffer(ui_canvas, ui_canvas_buf, RlcdDitherDisplay::WIDTH, RlcdDitherDisplay::HEIGHT, LV_COLOR_FORMAT_RGB565);
  lv_obj_set_pos(ui_canvas, 0, 0);
  lv_obj_set_size(ui_canvas, RlcdDitherDisplay::WIDTH, RlcdDitherDisplay::HEIGHT);

  renderTasks();
}
