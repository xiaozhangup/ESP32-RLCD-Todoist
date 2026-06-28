#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>

class RlcdDitherDisplay {
public:
  enum class DitherMode {
    BinaryThreshold,
    Spatial5x5,
    Spatial8x8,
    Hybrid5x5Temporal8,
  };

  static constexpr int WIDTH = 400;
  static constexpr int HEIGHT = 300;
  static constexpr int DITHER_BLOCK_SIZE = 5;
  static constexpr int BAYER_BLOCK_SIZE = 8;
  static constexpr int TEMPORAL_PHASES = 8;
  static constexpr uint32_t HYBRID_FRAME_INTERVAL_MS = 45;
  static constexpr int BUFFER_SIZE = WIDTH * HEIGHT / 8;

  RlcdDitherDisplay(int sck = 11, int mosi = 12, int dc = 5, int cs = 40, int rst = 41);
  ~RlcdDitherDisplay();

  bool begin();
  void clear(bool white = true);
  void flushLvgl(const lv_area_t *area, const uint8_t *color_map);
  void display();
  void setDitherMode(DitherMode mode);
  bool updateTemporalFrame(uint32_t now_ms);
  void showDiagonalGradient();

private:
  int _sck;
  int _mosi;
  int _dc;
  int _cs;
  int _rst;
  SPIClass _spi;
  uint8_t *_lcd_buffer = nullptr;
  uint8_t *_gray_buffer = nullptr;
  uint8_t *_tx_buffer = nullptr;
  DitherMode _mode = DitherMode::Spatial8x8;
  uint8_t _temporal_phase = 0;
  uint32_t _last_temporal_frame_ms = 0;

  void reset();
  void command(uint8_t cmd);
  void data(const uint8_t *data, size_t len);
  void commandData(uint8_t cmd, const uint8_t *data, size_t len);
  void initPanel();
  void setPixel(int x, int y, bool white);
  void displayArea(int x1, int y1, int x2, int y2);
  void renderArea(int x1, int y1, int x2, int y2);
  bool renderPixel(int x, int y, uint8_t luma) const;
  bool renderPixelSpatial5x5(int x, int y, uint8_t luma) const;
  bool renderPixelSpatial8x8(int x, int y, uint8_t luma) const;
  bool renderPixelHybrid5x5Temporal8(int x, int y, uint8_t luma) const;
  static uint8_t rgb565ToLuma(uint16_t color);
};
