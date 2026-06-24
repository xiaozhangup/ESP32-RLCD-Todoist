#include "LvglPort.h"
#include "RlcdDitherDisplay.h"

LV_FONT_DECLARE(fusion_pixel_30_ascii);

static constexpr int GRAY_PATCH_COUNT = 16;
static constexpr int GRAY_PATCH_WIDTH = 20;
static constexpr int GRAY_PATCH_HEIGHT = 90;
static constexpr int GRAY_PATCH_START_X = 40;
static constexpr int GRAY_PATCH_START_Y = 95;
static constexpr int TITLE_Y = 12;
static constexpr int BOTTOM_TEXT_Y = -38;
static constexpr int NOTE_Y = -10;

static RlcdDitherDisplay display;

static void flushCallback(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map) {
  display.flushLvgl(area, color_map);
  lv_display_flush_ready(disp);
}

static void createDemoUi() {
  lv_obj_t *screen = lv_screen_active();
  lv_obj_remove_style_all(screen);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_bg_grad_color(screen, lv_color_white(), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_HOR, 0);

  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "RLCD 5x5 gray");
  lv_obj_set_style_text_font(title, &fusion_pixel_30_ascii, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, TITLE_Y);

  for (int i = 0; i < GRAY_PATCH_COUNT; i++) {
    lv_obj_t *box = lv_obj_create(screen);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, GRAY_PATCH_WIDTH, GRAY_PATCH_HEIGHT);
    lv_obj_set_pos(box, GRAY_PATCH_START_X + i * GRAY_PATCH_WIDTH, GRAY_PATCH_START_Y);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    const uint8_t gray = (255 * i) / (GRAY_PATCH_COUNT - 1);
    lv_obj_set_style_bg_color(box, lv_color_make(gray, gray, gray), 0);
  }

  lv_obj_t *black_text = lv_label_create(screen);
  lv_label_set_text(black_text, "BLACK");
  lv_obj_set_style_text_font(black_text, &fusion_pixel_30_ascii, 0);
  lv_obj_set_style_text_color(black_text, lv_color_black(), 0);
  lv_obj_align(black_text, LV_ALIGN_BOTTOM_RIGHT, -20, BOTTOM_TEXT_Y);

  lv_obj_t *white_text = lv_label_create(screen);
  lv_label_set_text(white_text, "WHITE");
  lv_obj_set_style_text_font(white_text, &fusion_pixel_30_ascii, 0);
  lv_obj_set_style_text_color(white_text, lv_color_white(), 0);
  lv_obj_align(white_text, LV_ALIGN_BOTTOM_LEFT, 20, BOTTOM_TEXT_Y);

  lv_obj_t *note = lv_label_create(screen);
  lv_label_set_text(note, "16 GRAY PATCHES");
  lv_obj_set_style_text_font(note, &fusion_pixel_30_ascii, 0);
  lv_obj_set_style_text_color(note, lv_color_black(), 0);
  lv_obj_align(note, LV_ALIGN_BOTTOM_MID, 0, NOTE_Y);
}

void setup() {
  Serial.begin(115200);

  if (!display.begin()) {
    Serial.println("RLCD init failed");
    return;
  }

  if (!lvglPortInit(RlcdDitherDisplay::WIDTH, RlcdDitherDisplay::HEIGHT, flushCallback)) {
    Serial.println("LVGL init failed");
    return;
  }

  if (lvglLock(-1)) {
    createDemoUi();
    lvglUnlock();
  }
}

void loop() {
  delay(1000);
}
