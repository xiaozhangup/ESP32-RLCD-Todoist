#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>

class RlcdDitherDisplay {
public:
  static constexpr int WIDTH = 400;
  static constexpr int HEIGHT = 300;
  static constexpr int DITHER_BLOCK_SIZE = 5;
  static constexpr int BUFFER_SIZE = WIDTH * HEIGHT / 8;

  RlcdDitherDisplay(int sck = 11, int mosi = 12, int dc = 5, int cs = 40, int rst = 41);
  ~RlcdDitherDisplay();

  bool begin();
  void clear(bool white = true);
  void flushLvgl(const lv_area_t *area, const uint8_t *color_map);
  void display();

private:
  int _sck;
  int _mosi;
  int _dc;
  int _cs;
  int _rst;
  SPIClass _spi;
  uint8_t *_lcd_buffer = nullptr;
  uint8_t *_gray_buffer = nullptr;

  void reset();
  void command(uint8_t cmd);
  void data(const uint8_t *data, size_t len);
  void commandData(uint8_t cmd, const uint8_t *data, size_t len);
  void initPanel();
  void setPixel(int x, int y, bool white);
  void ditherBlocks(int x1, int y1, int x2, int y2);
  void ditherBlock(int block_x, int block_y);
  static uint8_t rgb565ToLuma(uint16_t color);
};
