#include <Arduino.h>

#include "LvglPort.h"
#include "RlcdDitherDisplay.h"
#include "TodoistConfig.h"

#ifndef ENABLE_TF_CARD_READER
#define ENABLE_TF_CARD_READER 0
#endif

#if ENABLE_TF_CARD_READER
#if !SOC_USB_OTG_SUPPORTED || ARDUINO_USB_MODE
#error TF card reader mode requires USB OTG/TinyUSB mode, not Hardware CDC/JTAG USB mode.
#endif
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
#error TF card reader mode requires USB CDC On Boot disabled.
#endif
#if defined(ARDUINO_USB_MSC_ON_BOOT) && ARDUINO_USB_MSC_ON_BOOT
#error TF card reader mode requires USB Firmware MSC On Boot disabled.
#endif
#if defined(ARDUINO_USB_DFU_ON_BOOT) && ARDUINO_USB_DFU_ON_BOOT
#error TF card reader mode requires USB DFU On Boot disabled.
#endif
#include <USB.h>
#include <USBMSC.h>
#endif

#include <BLEDevice.h>
#include <BLEServer.h>
#include <HTTPClient.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <cJSON.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <string.h>
#include <time.h>
#include <vector>

LV_FONT_DECLARE(fusion_pixel_10_zh);

static constexpr int KEY_BUTTON_PIN = 18;
static constexpr int BOOT_BUTTON_PIN = 0;
static constexpr uint32_t KEY_DEBOUNCE_MS = 40;
static constexpr uint32_t KEY_DOUBLE_CLICK_MS = 450;
static constexpr uint32_t BOOT_DEBOUNCE_MS = 40;
static constexpr uint32_t BOOT_DOUBLE_CLICK_MS = 450;
static constexpr uint32_t STATUS_REFRESH_MS = 1000;
static constexpr uint32_t SPINNER_REFRESH_MS = 180;
static constexpr uint32_t BATTERY_REFRESH_MS = 10000;
static constexpr uint32_t TF_CARD_REFRESH_MS = 10000;
static constexpr uint32_t SENSOR_REFRESH_MS = 5000;
static constexpr uint32_t TODOIST_REFRESH_MS = 10 * 60 * 1000;
static constexpr uint32_t WEATHER_REFRESH_MS = 20 * 60 * 1000;
static constexpr uint32_t REFRESH_DONE_VISIBLE_MS = 8000;
static constexpr bool TF_CARD_READER_MODE = ENABLE_TF_CARD_READER;
static constexpr int TF_SD_CLK_PIN = 38;
static constexpr int TF_SD_CMD_PIN = 21;
static constexpr int TF_SD_D0_PIN = 39;
static constexpr const char *BLE_NOTIFY_DEVICE_NAME = "RLCD-Calender";
static constexpr const char *BLE_NOTIFY_SERVICE_UUID = "8d2f3a70-7c5a-4db8-9e42-8f2d9b5b9a01";
static constexpr const char *BLE_NOTIFY_CHARACTERISTIC_UUID = "8d2f3a71-7c5a-4db8-9e42-8f2d9b5b9a01";
static constexpr int TASKS_PER_PAGE = 6;
static constexpr int TASK_TITLE_COLUMNS = 28;
static constexpr int BODY_SCALE = 2;
static constexpr int TITLE_SCALE = 4;
static constexpr int CLOCK_SCALE = 6;
static constexpr int SMALL_SCALE_NUM = 3;
static constexpr int SMALL_SCALE_DEN = 2;
static constexpr uint16_t CANVAS_WHITE = 0xffff;
static constexpr uint16_t CANVAS_BLACK = 0x0000;
static constexpr const char *TODOIST_BASE_URL = "https://api.todoist.com/api/v1";
static constexpr const char *OPENWEATHER_BASE_URL = "https://api.openweathermap.org/data/2.5/weather";
static constexpr size_t WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);
static constexpr int SHTC3_SDA_PIN = 13;
static constexpr int SHTC3_SCL_PIN = 14;
static constexpr uint8_t SHTC3_ADDR = 0x70;
static constexpr uint8_t SHTC3_TEMP_OFFSET_C = 4;

struct TodoistTaskItem {
  String content;
  String due;
};

struct WeatherInfo {
  String description;
  int temp_c = 0;
  int humidity = 0;
  bool valid = false;
};

struct SensorInfo {
  float temp_c = 0.0f;
  float humidity = 0.0f;
  bool valid = false;
};

struct BleOverlayInfo {
  String title;
  String content;
  uint32_t visible_until_ms = 0;
  bool visible = false;
};

enum class ScreenMode {
  Tasks,
  Clock,
};

enum class RefreshVisualState {
  Idle,
  Spinning,
  Done,
};

static RlcdDitherDisplay display;
static lv_obj_t *ui_canvas = nullptr;
static uint16_t *ui_canvas_buf = nullptr;

#if ENABLE_TF_CARD_READER
static USBMSC tf_card_msc;
#endif

static adc_cali_handle_t battery_cali_handle = nullptr;
static adc_oneshot_unit_handle_t battery_adc_handle = nullptr;
static bool battery_adc_ready = false;
static int battery_percent = -1;
static bool tf_card_ready = false;
static bool tf_card_bus_started = false;

static SemaphoreHandle_t data_mutex = nullptr;
static SemaphoreHandle_t wifi_mutex = nullptr;
static SemaphoreHandle_t radio_mutex = nullptr;
static BLEServer *ble_server = nullptr;
static BLECharacteristic *ble_notify_characteristic = nullptr;
static TaskHandle_t todoist_task_handle = nullptr;
static TaskHandle_t weather_task_handle = nullptr;
static TaskHandle_t ble_task_handle = nullptr;
static TaskHandle_t tf_card_task_handle = nullptr;
static std::vector<TodoistTaskItem> tasks;
static WeatherInfo weather_info;
static SensorInfo sensor_info;
static BleOverlayInfo ble_overlay;
static volatile bool ble_overlay_dirty = false;
static volatile bool ble_status_dirty = false;
static volatile bool network_radio_busy = false;
static volatile bool ble_gatt_allowed = false;
static volatile bool ble_client_connected = false;
static bool ble_initialized = false;
static bool shtc3_ready = false;
static int task_page = 0;
static ScreenMode screen_mode = ScreenMode::Tasks;
static RefreshVisualState refresh_visual_state = RefreshVisualState::Idle;
static uint32_t refresh_done_until_ms = 0;
static String app_status = "Starting";

static void flushCallback(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map) {
  display.flushLvgl(area, color_map);
  lv_display_flush_ready(disp);
}

static bool isConfigured(const char *value) {
  return value && value[0] != '\0';
}

static void pauseBleForNetwork();

static void lockNetworkRadio() {
  if (radio_mutex) {
    xSemaphoreTake(radio_mutex, portMAX_DELAY);
  }
  network_radio_busy = true;
  pauseBleForNetwork();
}

static void unlockNetworkRadio() {
  network_radio_busy = false;
  if (radio_mutex) {
    xSemaphoreGive(radio_mutex);
  }
}

static void lockBleRadio() {
  if (radio_mutex) {
    xSemaphoreTake(radio_mutex, portMAX_DELAY);
  }
  network_radio_busy = true;
}

static void unlockBleRadio() {
  network_radio_busy = false;
  if (radio_mutex) {
    xSemaphoreGive(radio_mutex);
  }
}

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

static void setBleOverlay(const String &title, const String &content, uint32_t seconds) {
  seconds = min<uint32_t>(60, max<uint32_t>(1, seconds));
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  ble_overlay.title = title.length() > 0 ? title : "通知";
  ble_overlay.content = content;
  ble_overlay.visible_until_ms = millis() + seconds * 1000UL;
  ble_overlay.visible = true;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  ble_overlay_dirty = true;
  Serial.printf("[BLE] Overlay received, title=%s seconds=%lu\n",
                title.c_str(), static_cast<unsigned long>(seconds));
}

static bool parseBleOverlayJson(const char *json) {
  cJSON *root = cJSON_Parse(json);
  if (!root) {
    Serial.printf("[BLE] Invalid JSON near: %s\n", cJSON_GetErrorPtr());
    return false;
  }

  cJSON *title = cJSON_GetObjectItem(root, "t");
  cJSON *content = cJSON_GetObjectItem(root, "c");
  cJSON *seconds = cJSON_GetObjectItem(root, "s");
  if (!cJSON_IsString(content) || !content->valuestring) {
    Serial.println("[BLE] Missing content field: c");
    cJSON_Delete(root);
    return false;
  }

  const String title_text = cJSON_IsString(title) && title->valuestring ? String(title->valuestring) : String("通知");
  const String content_text = content->valuestring;
  const uint32_t display_seconds = cJSON_IsNumber(seconds) ? static_cast<uint32_t>(seconds->valueint) : 5;
  cJSON_Delete(root);
  setBleOverlay(title_text, content_text, display_seconds);
  return true;
}

static bool isBleOverlayVisible() {
  bool visible = false;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  visible = ble_overlay.visible;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  return visible;
}

static uint32_t bleOverlayUntilMs() {
  uint32_t until_ms = 0;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  if (ble_overlay.visible) {
    until_ms = ble_overlay.visible_until_ms;
  }
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  return until_ms;
}

static bool shtc3WriteCommand(uint16_t command) {
  Wire.beginTransmission(SHTC3_ADDR);
  Wire.write(static_cast<uint8_t>(command >> 8));
  Wire.write(static_cast<uint8_t>(command & 0xff));
  const uint8_t err = Wire.endTransmission();
  if (err != 0) {
    Serial.printf("[SHTC3] Command 0x%04x failed: %u\n", command, err);
    return false;
  }
  return true;
}

static uint8_t shtc3Crc(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0xff;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31) : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

static bool initShtc3() {
  Wire.begin(SHTC3_SDA_PIN, SHTC3_SCL_PIN, 400000);
  delay(20);

  shtc3WriteCommand(0x3517);  // wakeup
  delay(2);
  shtc3WriteCommand(0x805d);  // soft reset
  delay(20);

  if (!shtc3WriteCommand(0xefc8)) {
    Serial.println("[SHTC3] Read ID command failed");
    return false;
  }
  if (Wire.requestFrom(SHTC3_ADDR, static_cast<uint8_t>(3)) != 3) {
    Serial.println("[SHTC3] Read ID failed");
    return false;
  }
  uint8_t bytes[3];
  for (uint8_t i = 0; i < 3; i++) {
    bytes[i] = Wire.read();
  }
  if (shtc3Crc(bytes, 2) != bytes[2]) {
    Serial.println("[SHTC3] ID CRC failed");
    return false;
  }

  const uint16_t id = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
  Serial.printf("[SHTC3] Ready, ID=0x%04x\n", id);
  shtc3_ready = true;
  return true;
}

static bool readShtc3(float &temp_c, float &humidity) {
  if (!shtc3_ready && !initShtc3()) {
    return false;
  }

  if (!shtc3WriteCommand(0x3517)) {
    shtc3_ready = false;
    return false;
  }
  delay(2);
  if (!shtc3WriteCommand(0x7866)) {
    shtc3_ready = false;
    return false;
  }
  delay(20);

  if (Wire.requestFrom(SHTC3_ADDR, static_cast<uint8_t>(6)) != 6) {
    Serial.println("[SHTC3] Read temp/humidity failed");
    shtc3_ready = false;
    return false;
  }

  uint8_t bytes[6];
  for (uint8_t i = 0; i < 6; i++) {
    bytes[i] = Wire.read();
  }
  if (shtc3Crc(bytes, 2) != bytes[2]) {
    Serial.println("[SHTC3] Temperature CRC failed");
    return false;
  }
  if (shtc3Crc(&bytes[3], 2) != bytes[5]) {
    Serial.println("[SHTC3] Humidity CRC failed");
    return false;
  }

  const uint16_t raw_temp = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
  const uint16_t raw_humi = (static_cast<uint16_t>(bytes[3]) << 8) | bytes[4];
  temp_c = 175.0f * raw_temp / 65536.0f - 45.0f - SHTC3_TEMP_OFFSET_C;
  humidity = 100.0f * raw_humi / 65536.0f;
  shtc3WriteCommand(0xb098);  // sleep
  return true;
}

static void updateSensorInfo() {
  float temp_c = 0.0f;
  float humidity = 0.0f;
  SensorInfo next;
  if (readShtc3(temp_c, humidity)) {
    next.temp_c = temp_c;
    next.humidity = humidity;
    next.valid = true;
    Serial.printf("[SHTC3] temp=%.1fC humidity=%.1f%%\n", temp_c, humidity);
  } else {
    Serial.println("[SHTC3] Sensor read failed");
  }

  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  sensor_info = next;
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
  if (ble_client_connected) {
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

static void renderTaskScreen(const std::vector<String> &task_lines,
                             int task_count,
                             int page,
                             int page_count,
                             int battery_snapshot,
                             bool tf_card_snapshot,
                             RefreshVisualState refresh_state) {
  drawText("Todoist", 6, 0, TITLE_SCALE);
  drawRightStatusArea(battery_snapshot, tf_card_snapshot);
  if (refresh_state != RefreshVisualState::Idle) {
    static constexpr int REFRESH_ICON_SIZE = 20;
    const int title_right = 6 + measureTextWidth("Todoist", TITLE_SCALE);
    const int title_bottom = scaleCeil(fusion_pixel_10_zh.line_height, TITLE_SCALE, 1);
    drawRefreshIcon(title_right + 10, max(0, title_bottom - REFRESH_ICON_SIZE - 24), refresh_state);
  }
  drawText(String("Inbox: ") + task_count + " active tasks", 8, 52, BODY_SCALE);

  if (task_lines.empty()) {
    drawText("Inbox 为空", 8, 98, BODY_SCALE);
  } else {
    static constexpr int TASK_START_Y = 82;
    static constexpr int TASK_ROW_H = 30;
    for (size_t i = 0; i < task_lines.size(); i++) {
      const int y = TASK_START_Y + static_cast<int>(i) * TASK_ROW_H;
      drawText(task_lines[i], 8, y, BODY_SCALE, RlcdDitherDisplay::WIDTH - 8);
      drawHLine(8, RlcdDitherDisplay::WIDTH - 8, y + 29, BODY_SCALE, CANVAS_BLACK);
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
  footer += " 双击KEY时钟";
  drawTextScaled(footer, 8, 270, SMALL_SCALE_NUM, SMALL_SCALE_DEN, RlcdDitherDisplay::WIDTH - 8);
}

static void renderBleOverlay(const BleOverlayInfo &overlay) {
  if (!overlay.visible) {
    return;
  }

  static constexpr int BOX_W = 332;
  static constexpr int BOX_H = 190;
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

  const std::vector<String> content_lines = wrapUtf8Lines(overlay.content, 30, 4);
  for (size_t i = 0; i < content_lines.size(); i++) {
    drawText(content_lines[i], BOX_X + PAD, BOX_Y + 60 + static_cast<int>(i) * 30,
             BODY_SCALE, BOX_X + BOX_W - PAD);
  }
}

static void renderTasks() {
  std::vector<String> task_lines;
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
  const int page_count = task_count == 0 ? 1 : (task_count + TASKS_PER_PAGE - 1) / TASKS_PER_PAGE;
  if (task_page >= page_count) {
    task_page = 0;
  }
  if (task_count > 0) {
    const int start = task_page * TASKS_PER_PAGE;
    const int end = min(task_count, start + TASKS_PER_PAGE);
    task_lines.reserve(end - start);
    for (int i = start; i < end; i++) {
      String line;
      line += String(i + 1);
      line += ". ";
      line += truncateUtf8(tasks[i].content, TASK_TITLE_COLUMNS);
      if (tasks[i].due.length() > 0) {
        line += " 截止:";
        line += tasks[i].due;
      }
      task_lines.push_back(line);
    }
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

  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }

  clearCanvas();
  if (mode_snapshot == ScreenMode::Clock) {
    renderClockScreen(battery_snapshot, tf_card_snapshot, weather_snapshot, sensor_snapshot);
  } else {
    renderTaskScreen(task_lines, footer_task_count, footer_page, footer_page_count,
                     battery_snapshot, tf_card_snapshot, refresh_snapshot);
  }
  renderBleOverlay(overlay_snapshot);
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

static bool updateTfCardStatus(bool log_success, bool *changed_out = nullptr) {
  if (!tf_card_bus_started) {
    if (!SD_MMC.setPins(TF_SD_CLK_PIN, TF_SD_CMD_PIN, TF_SD_D0_PIN)) {
      Serial.printf("[TF] SD_MMC pin setup failed, clk=%d cmd=%d d0=%d\n",
                    TF_SD_CLK_PIN, TF_SD_CMD_PIN, TF_SD_D0_PIN);
      return false;
    }
    tf_card_bus_started = true;
  }

  bool ready = SD_MMC.cardType() != CARD_NONE;
  if (!ready) {
    ready = SD_MMC.begin("/sdcard", true, false);
  }

  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  const bool changed = tf_card_ready != ready;
  tf_card_ready = ready;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  if (changed_out) {
    *changed_out = changed;
  }

  if (ready && (changed || log_success)) {
    Serial.printf("[TF] Card detected, size=%lluMB\n",
                  static_cast<unsigned long long>(SD_MMC.cardSize() / 1024ULL / 1024ULL));
  } else if (!ready && changed) {
    Serial.println("[TF] Card not detected");
  }
  return ready;
}

#if ENABLE_TF_CARD_READER
static void renderTfCardReaderScreen(const String &status, const String &detail) {
  if (!lvglLock(1000)) {
    return;
  }

  clearCanvas();
  drawText("TF读卡器", 22, 28, TITLE_SCALE, RlcdDitherDisplay::WIDTH - 22);
  drawHLine(22, RlcdDitherDisplay::WIDTH - 22, 88, 2, CANVAS_BLACK);
  drawText(status, 28, 112, BODY_SCALE, RlcdDitherDisplay::WIDTH - 28);
  if (detail.length() > 0) {
    drawText(detail, 28, 156, BODY_SCALE, RlcdDitherDisplay::WIDTH - 28);
  }
  drawTextScaled("USB MSC | 连接电脑后会显示为U盘", 28, 246, SMALL_SCALE_NUM, SMALL_SCALE_DEN,
                 RlcdDitherDisplay::WIDTH - 28);

  if (ui_canvas) {
    lv_obj_invalidate(ui_canvas);
  }
  lvglUnlock();
}

static uint8_t *allocTfSectorBuffer(uint32_t sector_size) {
  uint8_t *buffer = static_cast<uint8_t *>(heap_caps_malloc(sector_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
  if (!buffer) {
    buffer = static_cast<uint8_t *>(malloc(sector_size));
  }
  return buffer;
}

static int32_t tfMscRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  const uint32_t sector_size = SD_MMC.sectorSize();
  if (!sector_size || !buffer) {
    Serial.printf("[TF] Read failed, invalid sector size=%lu buffer=%p\n",
                  static_cast<unsigned long>(sector_size), buffer);
    return -1;
  }

  uint32_t done = 0;
  uint8_t *partial_sector = nullptr;
  while (done < bufsize) {
    const uint32_t absolute_offset = offset + done;
    const uint32_t sector = lba + absolute_offset / sector_size;
    const uint32_t sector_offset = absolute_offset % sector_size;
    const uint32_t chunk = min(sector_size - sector_offset, bufsize - done);
    uint8_t *dst = static_cast<uint8_t *>(buffer) + done;

    if (sector_offset == 0 && chunk == sector_size) {
      if (!SD_MMC.readRAW(dst, sector)) {
        Serial.printf("[TF] Read sector failed, lba=%lu\n", static_cast<unsigned long>(sector));
        free(partial_sector);
        return -1;
      }
    } else {
      if (!partial_sector) {
        partial_sector = allocTfSectorBuffer(sector_size);
        if (!partial_sector) {
          Serial.printf("[TF] Read failed, cannot allocate %lu bytes\n", static_cast<unsigned long>(sector_size));
          return -1;
        }
      }
      if (!SD_MMC.readRAW(partial_sector, sector)) {
        Serial.printf("[TF] Read partial sector failed, lba=%lu\n", static_cast<unsigned long>(sector));
        free(partial_sector);
        return -1;
      }
      memcpy(dst, partial_sector + sector_offset, chunk);
    }

    done += chunk;
  }

  free(partial_sector);
  return bufsize;
}

static int32_t tfMscWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  const uint32_t sector_size = SD_MMC.sectorSize();
  if (!sector_size || !buffer) {
    Serial.printf("[TF] Write failed, invalid sector size=%lu buffer=%p\n",
                  static_cast<unsigned long>(sector_size), buffer);
    return -1;
  }

  uint32_t done = 0;
  uint8_t *partial_sector = nullptr;
  while (done < bufsize) {
    const uint32_t absolute_offset = offset + done;
    const uint32_t sector = lba + absolute_offset / sector_size;
    const uint32_t sector_offset = absolute_offset % sector_size;
    const uint32_t chunk = min(sector_size - sector_offset, bufsize - done);
    uint8_t *src = buffer + done;

    if (sector_offset == 0 && chunk == sector_size) {
      if (!SD_MMC.writeRAW(src, sector)) {
        Serial.printf("[TF] Write sector failed, lba=%lu\n", static_cast<unsigned long>(sector));
        free(partial_sector);
        return -1;
      }
    } else {
      if (!partial_sector) {
        partial_sector = allocTfSectorBuffer(sector_size);
        if (!partial_sector) {
          Serial.printf("[TF] Write failed, cannot allocate %lu bytes\n", static_cast<unsigned long>(sector_size));
          return -1;
        }
      }
      if (!SD_MMC.readRAW(partial_sector, sector)) {
        Serial.printf("[TF] Write partial read failed, lba=%lu\n", static_cast<unsigned long>(sector));
        free(partial_sector);
        return -1;
      }
      memcpy(partial_sector + sector_offset, src, chunk);
      if (!SD_MMC.writeRAW(partial_sector, sector)) {
        Serial.printf("[TF] Write partial sector failed, lba=%lu\n", static_cast<unsigned long>(sector));
        free(partial_sector);
        return -1;
      }
    }

    done += chunk;
  }

  free(partial_sector);
  return bufsize;
}

static bool tfMscStartStop(uint8_t power_condition, bool start, bool load_eject) {
  Serial.printf("[TF] StartStop power=%u start=%d eject=%d\n", power_condition, start, load_eject);
  if (load_eject && !start) {
    renderTfCardReaderScreen("电脑已弹出TF卡", "拔线或重启可返回普通固件");
  }
  return true;
}

static void tfUsbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base != ARDUINO_USB_EVENTS) {
    return;
  }

  arduino_usb_event_data_t *data = static_cast<arduino_usb_event_data_t *>(event_data);
  switch (event_id) {
    case ARDUINO_USB_STARTED_EVENT:
      Serial.println("[TF] USB plugged");
      renderTfCardReaderScreen("USB已连接", "电脑应显示TF卡磁盘");
      break;
    case ARDUINO_USB_STOPPED_EVENT:
      Serial.println("[TF] USB unplugged");
      renderTfCardReaderScreen("USB已断开", "等待电脑连接");
      break;
    case ARDUINO_USB_SUSPEND_EVENT:
      Serial.printf("[TF] USB suspended remote_wakeup=%u\n", data ? data->suspend.remote_wakeup_en : 0);
      break;
    case ARDUINO_USB_RESUME_EVENT:
      Serial.println("[TF] USB resumed");
      break;
    default:
      break;
  }
}

static bool startTfCardReaderMode() {
  Serial.println("[TF] TF card reader mode enabled");
  renderTfCardReaderScreen("正在挂载TF卡", "SDMMC 1-bit");

  if (!updateTfCardStatus(true)) {
    Serial.println("[TF] SD_MMC mount failed");
    renderTfCardReaderScreen("TF卡挂载失败", "确认卡已插入且格式正常");
    return false;
  }

  const uint32_t sector_size = SD_MMC.sectorSize();
  const uint64_t card_bytes = SD_MMC.cardSize();
  const uint64_t sector_count = sector_size > 0 ? card_bytes / sector_size : 0;
  if (!sector_size || !sector_count) {
    Serial.printf("[TF] Invalid card geometry, sector_size=%lu card_bytes=%llu sectors=%llu\n",
                  static_cast<unsigned long>(sector_size),
                  static_cast<unsigned long long>(card_bytes),
                  static_cast<unsigned long long>(sector_count));
    renderTfCardReaderScreen("TF卡参数无效", "查看串口日志");
    return false;
  }

  tf_card_msc.vendorID("ESP32S3");
  tf_card_msc.productID("RLCD_TF");
  tf_card_msc.productRevision("1.0");
  tf_card_msc.onRead(tfMscRead);
  tf_card_msc.onWrite(tfMscWrite);
  tf_card_msc.onStartStop(tfMscStartStop);
  tf_card_msc.isWritable(true);
  tf_card_msc.mediaPresent(true);
  if (!tf_card_msc.begin(static_cast<uint32_t>(sector_count), sector_size)) {
    Serial.printf("[TF] USB MSC init failed, sector_size=%lu sectors=%llu\n",
                  static_cast<unsigned long>(sector_size),
                  static_cast<unsigned long long>(sector_count));
    renderTfCardReaderScreen("USB MSC初始化失败", "查看串口日志");
    return false;
  }

  USB.onEvent(tfUsbEventCallback);
  if (!USB.begin()) {
    Serial.println("[TF] USB begin failed");
    renderTfCardReaderScreen("USB启动失败", "确认USB-OTG模式");
    return false;
  }

  const uint64_t card_mb = card_bytes / 1024ULL / 1024ULL;
  Serial.printf("[TF] Ready, size=%lluMB sector_size=%lu sectors=%llu\n",
                static_cast<unsigned long long>(card_mb),
                static_cast<unsigned long>(sector_size),
                static_cast<unsigned long long>(sector_count));
  renderTfCardReaderScreen("TF卡已就绪", String(card_mb) + "MB | 等待电脑枚举");
  return true;
}
#endif

static void initBatteryAdc() {
  adc_cali_curve_fitting_config_t cali_config = {};
  cali_config.unit_id = ADC_UNIT_1;
  cali_config.atten = ADC_ATTEN_DB_12;
  cali_config.bitwidth = ADC_BITWIDTH_12;
  esp_err_t err = adc_cali_create_scheme_curve_fitting(&cali_config, &battery_cali_handle);
  if (err != ESP_OK) {
    Serial.printf("[Battery] ADC calibration init failed: %d\n", err);
    return;
  }

  adc_oneshot_unit_init_cfg_t init_config = {};
  init_config.unit_id = ADC_UNIT_1;
  err = adc_oneshot_new_unit(&init_config, &battery_adc_handle);
  if (err != ESP_OK) {
    Serial.printf("[Battery] ADC unit init failed: %d\n", err);
    return;
  }

  adc_oneshot_chan_cfg_t channel_config = {};
  channel_config.bitwidth = ADC_BITWIDTH_12;
  channel_config.atten = ADC_ATTEN_DB_12;
  err = adc_oneshot_config_channel(battery_adc_handle, ADC_CHANNEL_3, &channel_config);
  if (err != ESP_OK) {
    Serial.printf("[Battery] ADC channel config failed: %d\n", err);
    return;
  }
  battery_adc_ready = true;
  Serial.println("[Battery] ADC ready");
}

static int readBatteryPercent() {
  if (!battery_adc_ready) {
    Serial.println("[Battery] ADC not ready");
    return -1;
  }

  int raw = 0;
  int millivolts = 0;
  esp_err_t err = adc_oneshot_read(battery_adc_handle, ADC_CHANNEL_3, &raw);
  if (err != ESP_OK) {
    Serial.printf("[Battery] ADC read failed: %d\n", err);
    return -1;
  }
  err = adc_cali_raw_to_voltage(battery_cali_handle, raw, &millivolts);
  if (err != ESP_OK) {
    Serial.printf("[Battery] ADC calibration failed: %d raw=%d\n", err, raw);
    return -1;
  }

  const float voltage = millivolts * 0.001f * 3.0f;
  if (voltage <= 3.0f) {
    return 0;
  }
  if (voltage >= 4.12f) {
    return 100;
  }
  return static_cast<int>(((voltage - 3.0f) / 1.12f) * 100.0f + 0.5f);
}

static bool connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  lockNetworkRadio();
  if (wifi_mutex) {
    xSemaphoreTake(wifi_mutex, portMAX_DELAY);
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (wifi_mutex) {
      xSemaphoreGive(wifi_mutex);
    }
    unlockNetworkRadio();
    return true;
  }

  bool has_config = false;
  for (size_t i = 0; i < WIFI_NETWORK_COUNT; i++) {
    if (isConfigured(WIFI_NETWORKS[i].ssid) && isConfigured(WIFI_NETWORKS[i].password)) {
      has_config = true;
      break;
    }
  }

  if (!has_config) {
    Serial.println("[WiFi] Missing WIFI_NETWORKS entries in TodoistConfig.h");
    setAppStatus("Missing WiFi config");
    updateUi();
    if (wifi_mutex) {
      xSemaphoreGive(wifi_mutex);
    }
    unlockNetworkRadio();
    return false;
  }

  setAppStatus("Connecting WiFi");
  updateUi();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  while (WiFi.status() != WL_CONNECTED) {
    for (size_t i = 0; i < WIFI_NETWORK_COUNT; i++) {
      const char *ssid = WIFI_NETWORKS[i].ssid;
      const char *password = WIFI_NETWORKS[i].password;
      if (!isConfigured(ssid) || !isConfigured(password)) {
        continue;
      }

      Serial.printf("[WiFi] Connecting to SSID[%u]: %s\n", static_cast<unsigned>(i), ssid);
      WiFi.disconnect();
      delay(100);
      WiFi.begin(ssid, password);

      const uint32_t start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        Serial.printf("[WiFi] Waiting... ssid=%s status=%s(%d)\n", ssid, wifiStatusName(WiFi.status()), WiFi.status());
        delay(300);
      }

      if (WiFi.status() == WL_CONNECTED) {
        WiFi.setSleep(true);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        Serial.printf("[WiFi] Connected, SSID=%s IP=%s RSSI=%d dBm\n",
                      ssid, WiFi.localIP().toString().c_str(), WiFi.RSSI());
        setAppStatus(WiFi.localIP().toString());
        configTzTime(LOCAL_TIMEZONE, "pool.ntp.org", "time.nist.gov");
        Serial.printf("[Time] NTP configured, timezone=%s\n", LOCAL_TIMEZONE);
        updateUi();
        if (wifi_mutex) {
          xSemaphoreGive(wifi_mutex);
        }
        unlockNetworkRadio();
        return true;
      }

      Serial.printf("[WiFi] Connect failed for SSID=%s after %lu ms, status=%s(%d)\n",
                    ssid, millis() - start, wifiStatusName(WiFi.status()), WiFi.status());
    }

    Serial.println("[WiFi] All configured networks failed, retrying from first entry");
    setAppStatus("WiFi retrying");
    updateUi();
    delay(1000);
  }

  if (wifi_mutex) {
    xSemaphoreGive(wifi_mutex);
  }
  unlockNetworkRadio();
  return WiFi.status() == WL_CONNECTED;
}

static bool httpGet(const String &url, String &body) {
  lockNetworkRadio();
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(client, url)) {
    Serial.printf("[Todoist] HTTP begin failed: %s\n", url.c_str());
    unlockNetworkRadio();
    return false;
  }
  http.addHeader("Authorization", String("Bearer ") + TODOIST_API_TOKEN);
  http.addHeader("Content-Type", "application/json");

  Serial.printf("[Todoist] GET %s\n", url.c_str());
  const int code = http.GET();
  body = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    Serial.printf("[Todoist] HTTP %d: %s\n", code, body.c_str());
    unlockNetworkRadio();
    return false;
  }
  Serial.printf("[Todoist] HTTP %d, %d bytes\n", code, body.length());
  unlockNetworkRadio();
  return true;
}

static bool httpGetPlain(const String &url, String &body, const char *tag) {
  lockNetworkRadio();
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(client, url)) {
    Serial.printf("[%s] HTTP begin failed: %s\n", tag, url.c_str());
    unlockNetworkRadio();
    return false;
  }

  Serial.printf("[%s] GET %s\n", tag, url.c_str());
  const int code = http.GET();
  body = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    Serial.printf("[%s] HTTP %d: %s\n", tag, code, body.c_str());
    unlockNetworkRadio();
    return false;
  }
  Serial.printf("[%s] HTTP %d, %d bytes\n", tag, code, body.length());
  unlockNetworkRadio();
  return true;
}

static bool parseWeather(const String &body, WeatherInfo &target) {
  cJSON *root = cJSON_Parse(body.c_str());
  if (!root) {
    Serial.printf("[Weather] Failed to parse JSON near: %s\n", cJSON_GetErrorPtr());
    return false;
  }

  cJSON *weather_array = cJSON_GetObjectItem(root, "weather");
  cJSON *first_weather = cJSON_IsArray(weather_array) ? cJSON_GetArrayItem(weather_array, 0) : nullptr;
  cJSON *description = first_weather ? cJSON_GetObjectItem(first_weather, "description") : nullptr;
  cJSON *main = cJSON_GetObjectItem(root, "main");
  cJSON *temp = main ? cJSON_GetObjectItem(main, "temp") : nullptr;
  cJSON *humidity = main ? cJSON_GetObjectItem(main, "humidity") : nullptr;

  if (!cJSON_IsString(description) || !description->valuestring || !cJSON_IsNumber(temp) || !cJSON_IsNumber(humidity)) {
    Serial.println("[Weather] Missing description/temp/humidity in response");
    cJSON_Delete(root);
    return false;
  }

  target.description = description->valuestring;
  const double temp_value = temp->valuedouble;
  target.temp_c = static_cast<int>(temp_value + (temp_value >= 0 ? 0.5 : -0.5));
  target.humidity = humidity->valueint;
  target.valid = true;

  cJSON_Delete(root);
  Serial.printf("[Weather] Parsed weather: %s %dC humidity=%d%%\n",
                target.description.c_str(), target.temp_c, target.humidity);
  return true;
}

static bool fetchWeather() {
  if (!isConfigured(OPENWEATHER_API_KEY) || !isConfigured(OPENWEATHER_CITY)) {
    Serial.println("[Weather] Missing OPENWEATHER_API_KEY or OPENWEATHER_CITY in TodoistConfig.h");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED && !connectWifi()) {
    Serial.println("[Weather] WiFi unavailable");
    return false;
  }

  String url = String(OPENWEATHER_BASE_URL) + "?q=" + urlEncode(OPENWEATHER_CITY) +
               "&appid=" + urlEncode(OPENWEATHER_API_KEY) + "&units=metric&lang=zh_cn";
  String body;
  WeatherInfo fetched;
  if (!httpGetPlain(url, body, "Weather") || !parseWeather(body, fetched)) {
    Serial.println("[Weather] Failed while fetching weather");
    return false;
  }

  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  weather_info = fetched;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  updateUi();
  return true;
}

static bool readNextCursor(cJSON *root, String &cursor) {
  cursor = "";
  cJSON *next = cJSON_GetObjectItem(root, "next_cursor");
  if (cJSON_IsString(next) && next->valuestring && next->valuestring[0] != '\0') {
    cursor = next->valuestring;
    return true;
  }
  return false;
}

static bool parseInboxProject(const String &body, String &inbox_project_id, String &next_cursor) {
  cJSON *root = cJSON_Parse(body.c_str());
  if (!root) {
    Serial.printf("[Todoist] Failed to parse projects JSON near: %s\n", cJSON_GetErrorPtr());
    return false;
  }

  readNextCursor(root, next_cursor);
  cJSON *results = cJSON_GetObjectItem(root, "results");
  if (cJSON_IsArray(results)) {
    cJSON *project = nullptr;
    cJSON_ArrayForEach(project, results) {
      cJSON *inbox = cJSON_GetObjectItem(project, "inbox_project");
      cJSON *is_inbox = cJSON_GetObjectItem(project, "is_inbox_project");
      cJSON *id = cJSON_GetObjectItem(project, "id");
      cJSON *name = cJSON_GetObjectItem(project, "name");
      const bool is_inbox_by_name = cJSON_IsString(name) && name->valuestring && strcmp(name->valuestring, "Inbox") == 0;
      if ((cJSON_IsTrue(inbox) || cJSON_IsTrue(is_inbox) || is_inbox_by_name) &&
          cJSON_IsString(id) && id->valuestring) {
        inbox_project_id = id->valuestring;
        Serial.printf("[Todoist] Inbox project id=%s\n", inbox_project_id.c_str());
        cJSON_Delete(root);
        return true;
      }
    }
  }

  cJSON_Delete(root);
  return true;
}

static bool parseTasks(const String &body, String &next_cursor, std::vector<TodoistTaskItem> &target) {
  cJSON *root = cJSON_Parse(body.c_str());
  if (!root) {
    Serial.printf("[Todoist] Failed to parse tasks JSON near: %s\n", cJSON_GetErrorPtr());
    return false;
  }

  readNextCursor(root, next_cursor);
  cJSON *results = cJSON_GetObjectItem(root, "results");
  if (cJSON_IsArray(results)) {
    cJSON *task = nullptr;
    cJSON_ArrayForEach(task, results) {
      cJSON *content = cJSON_GetObjectItem(task, "content");
      if (!cJSON_IsString(content) || !content->valuestring) {
        continue;
      }

      TodoistTaskItem item;
      item.content = content->valuestring;
      cJSON *due = cJSON_GetObjectItem(task, "due");
      if (cJSON_IsObject(due)) {
        cJSON *due_string = cJSON_GetObjectItem(due, "string");
        cJSON *due_date = cJSON_GetObjectItem(due, "date");
        if (cJSON_IsString(due_string) && due_string->valuestring) {
          item.due = due_string->valuestring;
        } else if (cJSON_IsString(due_date) && due_date->valuestring) {
          item.due = due_date->valuestring;
        }
      }
      target.push_back(item);
    }
  }

  cJSON_Delete(root);
  Serial.printf("[Todoist] Parsed tasks total so far=%u\n", static_cast<unsigned>(target.size()));
  return true;
}

static bool findInboxProject(String &inbox_project_id) {
  String cursor;
  do {
    String url = String(TODOIST_BASE_URL) + "/projects";
    if (cursor.length() > 0) {
      url += "?cursor=" + urlEncode(cursor);
    }

    String body;
    if (!httpGet(url, body) || !parseInboxProject(body, inbox_project_id, cursor)) {
      Serial.println("[Todoist] Failed while fetching projects");
      return false;
    }
    if (inbox_project_id.length() > 0) {
      return true;
    }
  } while (cursor.length() > 0);

  Serial.println("[Todoist] Inbox project not found");
  return false;
}

static bool fetchInboxTasks() {
  if (!isConfigured(TODOIST_API_TOKEN)) {
    Serial.println("[Todoist] Missing TODOIST_API_TOKEN in TodoistConfig.h");
    setAppStatus("Missing Todoist token");
    return false;
  }

  setRefreshVisualState(RefreshVisualState::Spinning);
  updateUi();

  String inbox_project_id;
  setAppStatus("Finding Inbox");
  updateUi();
  if (!findInboxProject(inbox_project_id)) {
    Serial.println("[Todoist] Cannot find Inbox project");
    setAppStatus("Inbox not found");
    setRefreshVisualState(RefreshVisualState::Idle);
    updateUi();
    return false;
  }

  std::vector<TodoistTaskItem> fetched;
  fetched.reserve(16);
  String cursor;
  do {
    setAppStatus("Loading tasks");
    updateUi();
    String url = String(TODOIST_BASE_URL) + "/tasks?project_id=" + urlEncode(inbox_project_id);
    if (cursor.length() > 0) {
      url += "&cursor=" + urlEncode(cursor);
    }

    String body;
    if (!httpGet(url, body) || !parseTasks(body, cursor, fetched)) {
      Serial.println("[Todoist] Failed while fetching tasks");
      setAppStatus("Todoist failed");
      setRefreshVisualState(RefreshVisualState::Idle);
      updateUi();
      return false;
    }
  } while (cursor.length() > 0);

  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  tasks = fetched;
  task_page = 0;
  app_status = "";
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  setRefreshVisualState(RefreshVisualState::Done);
  Serial.printf("[Todoist] Inbox updated, tasks=%u\n", static_cast<unsigned>(fetched.size()));
  return true;
}

static void todoistWorker(void *) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED && !connectWifi()) {
      Serial.println("[Todoist] WiFi unavailable, waiting before retry");
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000));
      continue;
    }

    fetchInboxTasks();
    if (!ble_gatt_allowed) {
      ble_gatt_allowed = true;
      Serial.println("[BLE] GATT server enabled after first Todoist attempt");
    }
    updateUi();
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TODOIST_REFRESH_MS));
  }
}

static void weatherWorker(void *) {
  for (;;) {
    if (currentScreenMode() != ScreenMode::Clock) {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WEATHER_REFRESH_MS));
      continue;
    }

    if (WiFi.status() != WL_CONNECTED && !connectWifi()) {
      Serial.println("[Weather] WiFi unavailable, waiting before retry");
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000));
      continue;
    }

    fetchWeather();
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WEATHER_REFRESH_MS));
  }
}

static void tfCardWorker(void *) {
  for (;;) {
    bool tf_card_changed = false;
    updateTfCardStatus(false, &tf_card_changed);
    if (tf_card_changed) {
      updateUi();
    }
    vTaskDelay(pdMS_TO_TICKS(TF_CARD_REFRESH_MS));
  }
}

class BleNotifyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    ble_client_connected = true;
    ble_status_dirty = true;
    Serial.println("[BLE] Client connected");
  }

  void onDisconnect(BLEServer *server) override {
    ble_client_connected = false;
    ble_status_dirty = true;
    Serial.println("[BLE] Client disconnected, restarting advertising");
    server->getAdvertising()->start();
  }
};

class BleNotifyWriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    const String value = characteristic->getValue();
    Serial.printf("[BLE] GATT write, %u bytes: %s\n",
                  static_cast<unsigned>(value.length()), value.c_str());
    parseBleOverlayJson(value.c_str());
  }
};

static BleNotifyServerCallbacks ble_server_callbacks;
static BleNotifyWriteCallbacks ble_write_callbacks;

static bool initBleGattServer() {
  if (ble_initialized && ble_server && ble_notify_characteristic) {
    return true;
  }

  Serial.println("[BLE] Starting GATT notification server");
  BLEDevice::init(BLE_NOTIFY_DEVICE_NAME);
  ble_server = BLEDevice::createServer();
  if (!ble_server) {
    Serial.println("[BLE] Failed to create server");
    ble_initialized = false;
    return false;
  }
  ble_server->setCallbacks(&ble_server_callbacks);

  BLEService *service = ble_server->createService(BLE_NOTIFY_SERVICE_UUID);
  ble_notify_characteristic = service->createCharacteristic(
    BLE_NOTIFY_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  ble_notify_characteristic->setCallbacks(&ble_write_callbacks);
  service->start();

  BLEAdvertising *advertising = ble_server->getAdvertising();
  advertising->addServiceUUID(BLE_NOTIFY_SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();
  ble_initialized = true;
  Serial.printf("[BLE] Advertising as %s service=%s characteristic=%s\n",
                BLE_NOTIFY_DEVICE_NAME, BLE_NOTIFY_SERVICE_UUID, BLE_NOTIFY_CHARACTERISTIC_UUID);
  return true;
}

static void pauseBleForNetwork() {
  if (!ble_initialized) {
    return;
  }

  Serial.println("[BLE] Pausing GATT server for WiFi/HTTPS");
  BLEDevice::deinit(false);
  ble_server = nullptr;
  ble_notify_characteristic = nullptr;
  ble_client_connected = false;
  ble_initialized = false;
  delay(100);
}

static void bleGattWorker(void *) {
  Serial.println("[BLE] GATT server waiting for first Todoist attempt");
  while (!ble_gatt_allowed || WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  for (;;) {
    if (WiFi.status() != WL_CONNECTED || network_radio_busy) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    lockBleRadio();
    initBleGattServer();
    unlockBleRadio();
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

static void advanceTaskPage() {
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  if (screen_mode == ScreenMode::Tasks) {
    const int task_count = static_cast<int>(tasks.size());
    const int page_count = task_count == 0 ? 1 : (task_count + TASKS_PER_PAGE - 1) / TASKS_PER_PAGE;
    task_page = (task_page + 1) % page_count;
    Serial.printf("[Button] KEY single click, task page=%d/%d\n", task_page + 1, page_count);
  } else {
    Serial.println("[Button] KEY single click ignored on clock screen");
  }
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  updateUi();
}

static void toggleScreenMode() {
  ScreenMode new_mode = ScreenMode::Tasks;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  screen_mode = screen_mode == ScreenMode::Tasks ? ScreenMode::Clock : ScreenMode::Tasks;
  new_mode = screen_mode;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  Serial.printf("[Button] KEY double click, screen=%s\n", new_mode == ScreenMode::Clock ? "Clock" : "Tasks");
  if (new_mode == ScreenMode::Clock) {
    updateSensorInfo();
    if (weather_task_handle) {
      xTaskNotifyGive(weather_task_handle);
    }
  }
  updateUi();
}

static ScreenMode currentScreenMode() {
  ScreenMode mode = ScreenMode::Tasks;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  mode = screen_mode;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  return mode;
}

void setup() {
  Serial.begin(115200);

  if (!display.begin()) {
    Serial.println("RLCD init failed");
    return;
  }
  display.setDitherMode(RlcdDitherDisplay::DitherMode::BinaryThreshold);
  pinMode(KEY_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  data_mutex = xSemaphoreCreateMutex();
  wifi_mutex = xSemaphoreCreateMutex();
  radio_mutex = xSemaphoreCreateMutex();
  initBatteryAdc();
  setBatteryPercent(readBatteryPercent());
  initShtc3();

  if (!lvglPortInit(RlcdDitherDisplay::WIDTH, RlcdDitherDisplay::HEIGHT, flushCallback)) {
    Serial.println("LVGL init failed");
    return;
  }

  if (lvglLock(-1)) {
    createUi();
    lvglUnlock();
  }

#if ENABLE_TF_CARD_READER
  if (TF_CARD_READER_MODE) {
    startTfCardReaderMode();
    return;
  }
#endif

  if (xTaskCreatePinnedToCore(todoistWorker, "Todoist", 12288, nullptr, 2, &todoist_task_handle, 1) != pdPASS) {
    Serial.println("[Todoist] Failed to create Todoist task");
  }
  if (xTaskCreatePinnedToCore(weatherWorker, "Weather", 8192, nullptr, 1, &weather_task_handle, 1) != pdPASS) {
    Serial.println("[Weather] Failed to create Weather task");
  }
  if (xTaskCreatePinnedToCore(tfCardWorker, "TFCard", 4096, nullptr, 1, &tf_card_task_handle, 1) != pdPASS) {
    Serial.println("[TF] Failed to create TF card task");
  }
  if (xTaskCreatePinnedToCore(bleGattWorker, "BLEGatt", 8192, nullptr, 1, &ble_task_handle, 0) != pdPASS) {
    Serial.println("[BLE] Failed to create BLE GATT task");
  }
}

void loop() {
  if (TF_CARD_READER_MODE) {
    delay(1000);
    return;
  }

  static bool last_pressed = false;
  static bool last_boot_pressed = false;
  static uint8_t key_click_count = 0;
  static uint8_t boot_click_count = 0;
  static uint32_t last_key_change_ms = 0;
  static uint32_t last_key_release_ms = 0;
  static uint32_t last_boot_change_ms = 0;
  static uint32_t last_boot_release_ms = 0;
  static uint32_t last_status_ms = 0;
  static uint32_t last_spinner_ms = 0;
  static uint32_t last_battery_ms = 0;
  static uint32_t last_sensor_ms = 0;
  static uint32_t last_overlay_ms = 0;
  static time_t last_rendered_second = 0;
  static long last_rendered_task_minute = -1;

  const uint32_t now = millis();
  const bool pressed = digitalRead(KEY_BUTTON_PIN) == LOW;
  if (pressed != last_pressed && now - last_key_change_ms >= KEY_DEBOUNCE_MS) {
    last_key_change_ms = now;
    last_pressed = pressed;

    if (!pressed) {
      if (key_click_count == 0 || now - last_key_release_ms <= KEY_DOUBLE_CLICK_MS) {
        key_click_count++;
      } else {
        key_click_count = 1;
      }
      last_key_release_ms = now;

      if (key_click_count >= 2) {
        key_click_count = 0;
        toggleScreenMode();
      }
    }
  }
  if (key_click_count == 1 && now - last_key_release_ms > KEY_DOUBLE_CLICK_MS) {
    key_click_count = 0;
    advanceTaskPage();
  }

  const bool boot_pressed = digitalRead(BOOT_BUTTON_PIN) == LOW;
  if (boot_pressed != last_boot_pressed && now - last_boot_change_ms >= BOOT_DEBOUNCE_MS) {
    last_boot_change_ms = now;
    last_boot_pressed = boot_pressed;

    if (!boot_pressed) {
      if (boot_click_count == 0 || now - last_boot_release_ms <= BOOT_DOUBLE_CLICK_MS) {
        boot_click_count++;
      } else {
        boot_click_count = 1;
      }
      last_boot_release_ms = now;

      if (boot_click_count >= 2) {
        boot_click_count = 0;
        Serial.println("[Button] BOOT double click, refreshing Todoist tasks");
        setAppStatus("Manual refresh");
        updateUi();
        if (todoist_task_handle) {
          xTaskNotifyGive(todoist_task_handle);
        } else {
          Serial.println("[Button] Todoist task is not running");
        }
      }
    }
  }
  if (boot_click_count > 0 && now - last_boot_release_ms > BOOT_DOUBLE_CLICK_MS) {
    boot_click_count = 0;
  }

  const ScreenMode mode_snapshot = currentScreenMode();
  if (now - last_battery_ms >= BATTERY_REFRESH_MS) {
    last_battery_ms = now;
    setBatteryPercent(readBatteryPercent());
  }
  if (mode_snapshot == ScreenMode::Clock && now - last_sensor_ms >= SENSOR_REFRESH_MS) {
    last_sensor_ms = now;
    updateSensorInfo();
  }
  if (isRefreshSpinning() && now - last_spinner_ms >= SPINNER_REFRESH_MS) {
    last_spinner_ms = now;
    updateUi();
  }
  if (ble_status_dirty) {
    ble_status_dirty = false;
    updateUi();
  }
  const uint32_t overlay_until_ms = bleOverlayUntilMs();
  if (ble_overlay_dirty ||
      (overlay_until_ms != 0 && static_cast<int32_t>(now - overlay_until_ms) >= 0)) {
    ble_overlay_dirty = false;
    last_overlay_ms = now;
    updateUi();
  } else if (overlay_until_ms != 0 && now - last_overlay_ms >= STATUS_REFRESH_MS) {
    last_overlay_ms = now;
    updateUi();
  }
  const time_t current_second = time(nullptr);
  const long current_minute = current_second > 0 ? static_cast<long>(current_second / 60) : -1;
  if (mode_snapshot == ScreenMode::Clock && current_second > 0 && current_second != last_rendered_second) {
    last_rendered_second = current_second;
    last_status_ms = now;
    updateUi();
  } else if (mode_snapshot == ScreenMode::Tasks && current_minute >= 0 && current_minute != last_rendered_task_minute) {
    last_rendered_task_minute = current_minute;
    last_status_ms = now;
    updateUi();
  } else if (isRefreshVisualActive() && now - last_status_ms >= STATUS_REFRESH_MS) {
    last_status_ms = now;
    updateUi();
  }

  delay(20);
}
