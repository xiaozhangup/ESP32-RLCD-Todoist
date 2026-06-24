#include "RlcdDitherDisplay.h"

#include <esp_heap_caps.h>
#include <string.h>

static constexpr uint32_t SPI_CLK = 24000000;

static_assert(RlcdDitherDisplay::DITHER_BLOCK_SIZE == 5, "DITHER_5X5 must match DITHER_BLOCK_SIZE");

// Low values are written first, so darker blocks get the most visually even dots.
static constexpr uint8_t DITHER_5X5[25] = {
  0, 13, 3, 16, 6,
  18, 8, 21, 11, 24,
  4, 17, 1, 14, 7,
  22, 12, 19, 9, 23,
  2, 15, 5, 20, 10,
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
  if (!_lcd_buffer) {
    _lcd_buffer = static_cast<uint8_t *>(malloc(BUFFER_SIZE));
  }
  if (!_gray_buffer) {
    _gray_buffer = static_cast<uint8_t *>(malloc(WIDTH * HEIGHT));
  }
  if (!_lcd_buffer || !_gray_buffer) {
    return false;
  }

  _spi.begin(_sck, -1, _mosi, _cs);
  _spi.beginTransaction(SPISettings(SPI_CLK, MSBFIRST, SPI_MODE0));

  initPanel();
  clear(true);
  display();
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

  for (int y = y1; y <= y2; y++) {
    const int src_y = y - area->y1;
    for (int x = x1; x <= x2; x++) {
      const int src_x = x - area->x1;
      _gray_buffer[y * WIDTH + x] = rgb565ToLuma(src[src_y * area_w + src_x]);
    }
  }

  ditherBlocks(x1, y1, x2, y2);
  display();
}

void RlcdDitherDisplay::display() {
  const uint8_t col_bounds[] = {0x12, 0x2A};
  const uint8_t row_bounds[] = {0x00, 0xC7};
  commandData(0x2A, col_bounds, sizeof(col_bounds));
  commandData(0x2B, row_bounds, sizeof(row_bounds));
  commandData(0x2C, _lcd_buffer, BUFFER_SIZE);
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

void RlcdDitherDisplay::ditherBlocks(int x1, int y1, int x2, int y2) {
  const int bx1 = x1 / DITHER_BLOCK_SIZE;
  const int by1 = y1 / DITHER_BLOCK_SIZE;
  const int bx2 = x2 / DITHER_BLOCK_SIZE;
  const int by2 = y2 / DITHER_BLOCK_SIZE;

  for (int by = by1; by <= by2; by++) {
    for (int bx = bx1; bx <= bx2; bx++) {
      ditherBlock(bx, by);
    }
  }
}

void RlcdDitherDisplay::ditherBlock(int block_x, int block_y) {
  const int x0 = block_x * DITHER_BLOCK_SIZE;
  const int y0 = block_y * DITHER_BLOCK_SIZE;
  const int x_end = (x0 + DITHER_BLOCK_SIZE) > WIDTH ? WIDTH : x0 + DITHER_BLOCK_SIZE;
  const int y_end = (y0 + DITHER_BLOCK_SIZE) > HEIGHT ? HEIGHT : y0 + DITHER_BLOCK_SIZE;

  for (int y = y0; y < y_end; y++) {
    for (int x = x0; x < x_end; x++) {
      const uint8_t luma = _gray_buffer[y * WIDTH + x];
      const uint8_t black_level = ((255 - luma) * 25 + 127) / 255;
      const uint8_t threshold_index = (y - y0) * DITHER_BLOCK_SIZE + (x - x0);
      setPixel(x, y, DITHER_5X5[threshold_index] >= black_level);
    }
  }
}

uint8_t RlcdDitherDisplay::rgb565ToLuma(uint16_t color) {
  const uint8_t r = ((color >> 11) & 0x1f) << 3;
  const uint8_t g = ((color >> 5) & 0x3f) << 2;
  const uint8_t b = (color & 0x1f) << 3;
  return (77 * r + 150 * g + 29 * b) >> 8;
}
