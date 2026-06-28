#include "RlcdDitherDisplay.h"

#include <esp_heap_caps.h>
#include <string.h>

static constexpr uint32_t SPI_CLK = 24000000;

static_assert(RlcdDitherDisplay::DITHER_BLOCK_SIZE == 5, "DITHER_5X5 must match DITHER_BLOCK_SIZE");
static_assert(RlcdDitherDisplay::BAYER_BLOCK_SIZE == 8, "BAYER_8X8 must match BAYER_BLOCK_SIZE");
static_assert(RlcdDitherDisplay::TEMPORAL_PHASES == 8, "TEMPORAL_ORDER_8 must match TEMPORAL_PHASES");

// Low values turn black first, so darker tones spread dots evenly in a 5x5 cell.
static constexpr uint8_t DITHER_5X5[25] = {
  0, 13, 3, 16, 6,
  18, 8, 21, 11, 24,
  4, 17, 1, 14, 7,
  22, 12, 19, 9, 23,
  2, 15, 5, 20, 10,
};

static constexpr uint8_t BAYER_8X8[64] = {
  0, 48, 12, 60, 3, 51, 15, 63,
  32, 16, 44, 28, 35, 19, 47, 31,
  8, 56, 4, 52, 11, 59, 7, 55,
  40, 24, 36, 20, 43, 27, 39, 23,
  2, 50, 14, 62, 1, 49, 13, 61,
  34, 18, 46, 30, 33, 17, 45, 29,
  10, 58, 6, 54, 9, 57, 5, 53,
  42, 26, 38, 22, 41, 25, 37, 21,
};

static constexpr uint8_t TEMPORAL_ORDER_8[RlcdDitherDisplay::TEMPORAL_PHASES] = {
  0, 4, 2, 6, 1, 5, 3, 7,
};

RlcdDitherDisplay::RlcdDitherDisplay(int sck, int mosi, int dc, int cs, int rst)
  : _sck(sck), _mosi(mosi), _dc(dc), _cs(cs), _rst(rst), _spi(HSPI) {
}

RlcdDitherDisplay::~RlcdDitherDisplay() {
  if (_lcd_buffer) {
    free(_lcd_buffer);
  }
  if (_gray_buffer) {
    free(_gray_buffer);
  }
  if (_tx_buffer) {
    free(_tx_buffer);
  }
  _spi.end();
}

bool RlcdDitherDisplay::begin() {
  pinMode(_dc, OUTPUT);
  pinMode(_cs, OUTPUT);
  pinMode(_rst, OUTPUT);
  digitalWrite(_cs, HIGH);
  digitalWrite(_dc, HIGH);
  digitalWrite(_rst, HIGH);

  _lcd_buffer = static_cast<uint8_t *>(heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  _gray_buffer = static_cast<uint8_t *>(heap_caps_malloc(WIDTH * HEIGHT, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  _tx_buffer = static_cast<uint8_t *>(heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!_lcd_buffer) {
    _lcd_buffer = static_cast<uint8_t *>(malloc(BUFFER_SIZE));
  }
  if (!_gray_buffer) {
    _gray_buffer = static_cast<uint8_t *>(malloc(WIDTH * HEIGHT));
  }
  if (!_tx_buffer) {
    _tx_buffer = static_cast<uint8_t *>(malloc(BUFFER_SIZE));
  }
  if (!_lcd_buffer || !_gray_buffer || !_tx_buffer) {
    return false;
  }

  _spi.begin(_sck, -1, _mosi, _cs);
  _spi.beginTransaction(SPISettings(SPI_CLK, MSBFIRST, SPI_MODE0));

  initPanel();
  clear(true);
  display();
  _last_temporal_frame_ms = millis();
  return true;
}

void RlcdDitherDisplay::clear(bool white) {
  memset(_gray_buffer, white ? 0xff : 0x00, WIDTH * HEIGHT);
  memset(_lcd_buffer, white ? 0xff : 0x00, BUFFER_SIZE);
}

void RlcdDitherDisplay::flushLvgl(const lv_area_t *area, const uint8_t *color_map) {
  const int x1 = area->x1 < 0 ? 0 : area->x1;
  const int y1 = area->y1 < 0 ? 0 : area->y1;
  const int x2 = area->x2 >= WIDTH ? WIDTH - 1 : area->x2;
  const int y2 = area->y2 >= HEIGHT ? HEIGHT - 1 : area->y2;
  const int area_w = area->x2 - area->x1 + 1;
  const uint16_t *src = reinterpret_cast<const uint16_t *>(color_map);
  int dirty_x1 = WIDTH;
  int dirty_y1 = HEIGHT;
  int dirty_x2 = -1;
  int dirty_y2 = -1;

  for (int y = y1; y <= y2; y++) {
    const int src_y = y - area->y1;
    for (int x = x1; x <= x2; x++) {
      const int src_x = x - area->x1;
      const uint8_t luma = rgb565ToLuma(src[src_y * area_w + src_x]);
      uint8_t &old_luma = _gray_buffer[y * WIDTH + x];
      if (old_luma != luma) {
        old_luma = luma;
        if (x < dirty_x1) dirty_x1 = x;
        if (x > dirty_x2) dirty_x2 = x;
        if (y < dirty_y1) dirty_y1 = y;
        if (y > dirty_y2) dirty_y2 = y;
      }
    }
  }

  if (dirty_x2 < dirty_x1 || dirty_y2 < dirty_y1) {
    return;
  }

  const int tx_x1 = dirty_x1 & ~1;
  const int tx_x2 = dirty_x2 | 1;
  const int block_y_min = (HEIGHT - 1 - dirty_y2) >> 2;
  const int block_y_max = (HEIGHT - 1 - dirty_y1) >> 2;
  const int block_start = (block_y_min / 3) * 3;
  int block_end = (block_y_max / 3) * 3 + 2;
  if (block_end >= HEIGHT / 4) {
    block_end = HEIGHT / 4 - 1;
  }
  const int tx_y1 = max(0, HEIGHT - 1 - ((block_end << 2) + 3));
  const int tx_y2 = min(HEIGHT - 1, HEIGHT - 1 - (block_start << 2));

  renderArea(tx_x1, tx_y1, tx_x2, tx_y2);
  displayArea(tx_x1, tx_y1, tx_x2, tx_y2);
}

void RlcdDitherDisplay::display() {
  displayArea(0, 0, WIDTH - 1, HEIGHT - 1);
}

void RlcdDitherDisplay::setDitherMode(DitherMode mode) {
  _mode = mode;
  _temporal_phase = 0;
  renderArea(0, 0, WIDTH - 1, HEIGHT - 1);
  display();
}

bool RlcdDitherDisplay::updateTemporalFrame(uint32_t now_ms) {
  if (_mode != DitherMode::Hybrid5x5Temporal8) {
    return false;
  }
  if (now_ms - _last_temporal_frame_ms < HYBRID_FRAME_INTERVAL_MS) {
    return false;
  }

  _last_temporal_frame_ms = now_ms;
  _temporal_phase = (_temporal_phase + 1) % TEMPORAL_PHASES;
  renderArea(0, 0, WIDTH - 1, HEIGHT - 1);
  display();
  return true;
}

void RlcdDitherDisplay::showDiagonalGradient() {
  static constexpr uint32_t max_sum = (WIDTH - 1) + (HEIGHT - 1);

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      _gray_buffer[y * WIDTH + x] = ((x + y) * 255U) / max_sum;
    }
  }

  renderArea(0, 0, WIDTH - 1, HEIGHT - 1);
  display();
}

void RlcdDitherDisplay::reset() {
  digitalWrite(_rst, HIGH);
  delay(50);
  digitalWrite(_rst, LOW);
  delay(20);
  digitalWrite(_rst, HIGH);
  delay(50);
}

void RlcdDitherDisplay::command(uint8_t cmd) {
  digitalWrite(_dc, LOW);
  digitalWrite(_cs, LOW);
  _spi.transfer(cmd);
  digitalWrite(_cs, HIGH);
}

void RlcdDitherDisplay::data(const uint8_t *data_ptr, size_t len) {
  digitalWrite(_dc, HIGH);
  digitalWrite(_cs, LOW);
  _spi.transferBytes(const_cast<uint8_t *>(data_ptr), nullptr, len);
  digitalWrite(_cs, HIGH);
}

void RlcdDitherDisplay::commandData(uint8_t cmd, const uint8_t *data_ptr, size_t len) {
  digitalWrite(_dc, LOW);
  digitalWrite(_cs, LOW);
  _spi.transfer(cmd);
  if (len > 0) {
    digitalWrite(_dc, HIGH);
    _spi.transferBytes(const_cast<uint8_t *>(data_ptr), nullptr, len);
  }
  digitalWrite(_cs, HIGH);
}

void RlcdDitherDisplay::initPanel() {
  reset();

  const uint8_t d6[] = {0x17, 0x02};
  const uint8_t d1[] = {0x01};
  const uint8_t c0[] = {0x11, 0x04};
  const uint8_t c1[] = {0x69, 0x69, 0x69, 0x69};
  const uint8_t c2[] = {0x19, 0x19, 0x19, 0x19};
  const uint8_t c4[] = {0x4B, 0x4B, 0x4B, 0x4B};
  const uint8_t d8[] = {0x80, 0xE9};
  const uint8_t b2[] = {0x02};
  const uint8_t b3[] = {0xE5, 0xF6, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45};
  const uint8_t b4[] = {0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45};
  const uint8_t g_timing[] = {0x32, 0x03, 0x1F};
  const uint8_t b7[] = {0x13};
  const uint8_t b0[] = {0x64};
  const uint8_t c9[] = {0x00};
  const uint8_t m36[] = {0x48};
  const uint8_t m3a[] = {0x11};
  const uint8_t b9[] = {0x20};
  const uint8_t b8[] = {0x29};
  const uint8_t win_a[] = {0x12, 0x2A};
  const uint8_t win_b[] = {0x00, 0xC7};
  const uint8_t m35[] = {0x00};
  const uint8_t d0[] = {0xFF};

  commandData(0xD6, d6, sizeof(d6));
  commandData(0xD1, d1, sizeof(d1));
  commandData(0xC0, c0, sizeof(c0));
  commandData(0xC1, c1, sizeof(c1));
  commandData(0xC2, c2, sizeof(c2));
  commandData(0xC4, c4, sizeof(c4));
  commandData(0xC5, c2, sizeof(c2));
  commandData(0xD8, d8, sizeof(d8));
  commandData(0xB2, b2, sizeof(b2));
  commandData(0xB3, b3, sizeof(b3));
  commandData(0xB4, b4, sizeof(b4));
  commandData(0x62, g_timing, sizeof(g_timing));
  commandData(0xB7, b7, sizeof(b7));
  commandData(0xB0, b0, sizeof(b0));
  command(0x11);
  delay(120);
  commandData(0xC9, c9, sizeof(c9));
  commandData(0x36, m36, sizeof(m36));
  commandData(0x3A, m3a, sizeof(m3a));
  commandData(0xB9, b9, sizeof(b9));
  commandData(0xB8, b8, sizeof(b8));
  command(0x21);
  commandData(0x2A, win_a, sizeof(win_a));
  commandData(0x2B, win_b, sizeof(win_b));
  commandData(0x35, m35, sizeof(m35));
  commandData(0xD0, d0, sizeof(d0));
  command(0x38);
  command(0x29);
}

void RlcdDitherDisplay::setPixel(int x, int y, bool white) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
    return;
  }

  const uint16_t inv_y = HEIGHT - 1 - y;
  const uint16_t byte_x = x >> 1;
  const uint16_t block_y = inv_y >> 2;
  const uint32_t index = byte_x * (HEIGHT >> 2) + block_y;
  const uint8_t local_x = x & 1;
  const uint8_t local_y = inv_y & 3;
  const uint8_t mask = 1 << (7 - ((local_y << 1) | local_x));

  if (white) {
    _lcd_buffer[index] |= mask;
  } else {
    _lcd_buffer[index] &= ~mask;
  }
}

void RlcdDitherDisplay::displayArea(int x1, int y1, int x2, int y2) {
  if (!_tx_buffer || x2 < x1 || y2 < y1) {
    return;
  }

  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 >= WIDTH) x2 = WIDTH - 1;
  if (y2 >= HEIGHT) y2 = HEIGHT - 1;

  const int byte_x_start = x1 >> 1;
  const int byte_x_end = x2 >> 1;

  const int block_y_min = (HEIGHT - 1 - y2) >> 2;
  const int block_y_max = (HEIGHT - 1 - y1) >> 2;
  const int block_start = (block_y_min / 3) * 3;
  int block_end = (block_y_max / 3) * 3 + 2;
  if (block_end >= HEIGHT / 4) {
    block_end = HEIGHT / 4 - 1;
  }

  const uint8_t raw_col_start = 0x12 + block_start / 3;
  const uint8_t raw_col_end = 0x12 + block_end / 3;
  const uint8_t col_start = 0x3C - raw_col_end;
  const uint8_t col_end = 0x3C - raw_col_start;
  const uint8_t row_start = static_cast<uint8_t>(byte_x_start);
  const uint8_t row_end = static_cast<uint8_t>(byte_x_end);
  const int bytes_per_row = block_end - block_start + 1;
  const int rows = byte_x_end - byte_x_start + 1;

  size_t out = 0;
  for (int bx = byte_x_start; bx <= byte_x_end; bx++) {
    const uint8_t *src = _lcd_buffer + bx * (HEIGHT >> 2) + block_start;
    memcpy(_tx_buffer + out, src, bytes_per_row);
    out += bytes_per_row;
  }

  const uint8_t col_bounds[] = {col_start, col_end};
  const uint8_t row_bounds[] = {row_start, row_end};
  commandData(0x2A, col_bounds, sizeof(col_bounds));
  commandData(0x2B, row_bounds, sizeof(row_bounds));
  commandData(0x2C, _tx_buffer, static_cast<size_t>(rows) * bytes_per_row);
}

void RlcdDitherDisplay::renderArea(int x1, int y1, int x2, int y2) {
  for (int y = y1; y <= y2; y++) {
    for (int x = x1; x <= x2; x++) {
      setPixel(x, y, renderPixel(x, y, _gray_buffer[y * WIDTH + x]));
    }
  }
}

bool RlcdDitherDisplay::renderPixel(int x, int y, uint8_t luma) const {
  switch (_mode) {
    case DitherMode::BinaryThreshold:
      return luma >= 128;
    case DitherMode::Spatial5x5:
      return renderPixelSpatial5x5(x, y, luma);
    case DitherMode::Spatial8x8:
      return renderPixelSpatial8x8(x, y, luma);
    case DitherMode::Hybrid5x5Temporal8:
      return renderPixelHybrid5x5Temporal8(x, y, luma);
  }

  return renderPixelSpatial8x8(x, y, luma);
}

bool RlcdDitherDisplay::renderPixelSpatial5x5(int x, int y, uint8_t luma) const {
  const uint8_t threshold_index = (y % DITHER_BLOCK_SIZE) * DITHER_BLOCK_SIZE + (x % DITHER_BLOCK_SIZE);
  const uint8_t black = 255 - luma;
  const uint8_t black_level = (black * 25 + 127) / 255;
  return DITHER_5X5[threshold_index] >= black_level;
}

bool RlcdDitherDisplay::renderPixelSpatial8x8(int x, int y, uint8_t luma) const {
  const uint8_t threshold_index = (y % BAYER_BLOCK_SIZE) * BAYER_BLOCK_SIZE + (x % BAYER_BLOCK_SIZE);
  const uint8_t black = 255 - luma;
  const uint8_t black_level = (black * 64 + 127) / 255;
  return BAYER_8X8[threshold_index] >= black_level;
}

bool RlcdDitherDisplay::renderPixelHybrid5x5Temporal8(int x, int y, uint8_t luma) const {
  const uint8_t threshold_index = (y % DITHER_BLOCK_SIZE) * DITHER_BLOCK_SIZE + (x % DITHER_BLOCK_SIZE);
  const uint8_t spatial_order = DITHER_5X5[threshold_index];
  const uint8_t black = 255 - luma;
  const uint16_t black_slots = (black * (25 * TEMPORAL_PHASES) + 127) / 255;
  const uint16_t threshold = spatial_order * TEMPORAL_PHASES + TEMPORAL_ORDER_8[_temporal_phase];
  return threshold >= black_slots;
}

uint8_t RlcdDitherDisplay::rgb565ToLuma(uint16_t color) {
  const uint8_t r = ((color >> 11) & 0x1f) << 3;
  const uint8_t g = ((color >> 5) & 0x3f) << 2;
  const uint8_t b = (color & 0x1f) << 3;
  return (77 * r + 150 * g + 29 * b) >> 8;
}
