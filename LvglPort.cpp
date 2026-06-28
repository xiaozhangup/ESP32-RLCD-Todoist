#include "LvglPort.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static constexpr uint32_t LVGL_TICK_PERIOD_MS = 20;
static constexpr uint32_t LVGL_TASK_MAX_DELAY_MS = 50;
static constexpr uint32_t LVGL_TASK_MIN_DELAY_MS = 20;

static SemaphoreHandle_t lvgl_mutex = nullptr;

static void lvglTick(void *) {
  lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

bool lvglLock(int timeout_ms) {
  if (!lvgl_mutex) {
    return false;
  }

  const TickType_t timeout_ticks = timeout_ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTake(lvgl_mutex, timeout_ticks) == pdTRUE;
}

void lvglUnlock() {
  if (lvgl_mutex) {
    xSemaphoreGive(lvgl_mutex);
  }
}

static void lvglTask(void *) {
  uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;

  for (;;) {
    if (lvglLock(-1)) {
      task_delay_ms = lv_timer_handler();
      lvglUnlock();
    }

    if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
      task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
      task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
    }

    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}

bool lvglPortInit(int width, int height, LvglFlushCb flush_cb) {
  lvgl_mutex = xSemaphoreCreateMutex();
  if (!lvgl_mutex) {
    return false;
  }

  lv_init();

  lv_display_t *disp = lv_display_create(width, height);
  if (!disp) {
    return false;
  }

  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, flush_cb);

  const size_t buffer_size = width * height * LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565);
  uint8_t *buffer_1 = static_cast<uint8_t *>(heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  uint8_t *buffer_2 = static_cast<uint8_t *>(heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!buffer_1) {
    buffer_1 = static_cast<uint8_t *>(malloc(buffer_size));
  }
  if (!buffer_2) {
    buffer_2 = static_cast<uint8_t *>(malloc(buffer_size));
  }
  if (!buffer_1 || !buffer_2) {
    return false;
  }

  lv_display_set_buffers(disp, buffer_1, buffer_2, buffer_size, LV_DISPLAY_RENDER_MODE_FULL);

  esp_timer_create_args_t tick_args = {};
  tick_args.callback = lvglTick;
  tick_args.name = "lvgl_tick";
  esp_timer_handle_t tick_timer = nullptr;
  if (esp_timer_create(&tick_args, &tick_timer) != ESP_OK) {
    return false;
  }
  if (esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000) != ESP_OK) {
    return false;
  }

  return xTaskCreatePinnedToCore(lvglTask, "LVGL", 8192, nullptr, 5, nullptr, 0) == pdPASS;
}
