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
#include <FFat.h>
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
#include <esp_bt.h>
#include <esp_coexist.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <string.h>
#include <time.h>
#include <vector>

#ifndef CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 1
#endif

static constexpr int KEY_BUTTON_PIN = 18;
static constexpr int BOOT_BUTTON_PIN = 0;
static constexpr uint32_t KEY_DEBOUNCE_MS = 40;
static constexpr uint32_t KEY_DOUBLE_CLICK_MS = 650;
static constexpr uint32_t BOOT_DEBOUNCE_MS = 40;
static constexpr uint32_t BOOT_DOUBLE_CLICK_MS = 450;
static constexpr uint32_t BOOT_LONG_PRESS_MS = 900;
static constexpr uint32_t STATUS_REFRESH_MS = 1000;
static constexpr uint32_t SPINNER_REFRESH_MS = 180;
static constexpr uint32_t BATTERY_REFRESH_MS = 10000;
static constexpr uint32_t TF_CARD_REFRESH_MS = 10000;
static constexpr uint32_t SENSOR_REFRESH_MS = 40000;
static constexpr uint32_t TIME_SYNC_REFRESH_MS = 24UL * 60UL * 60UL * 1000UL;
static constexpr uint32_t TODOIST_REFRESH_MS = 10 * 60 * 1000;
static constexpr uint32_t WEATHER_REFRESH_MS = 10 * 60 * 1000;
static constexpr uint32_t REFRESH_DONE_VISIBLE_MS = 8000;
static constexpr bool TF_CARD_READER_MODE = ENABLE_TF_CARD_READER;
static constexpr int TF_SD_CLK_PIN = 38;
static constexpr int TF_SD_CMD_PIN = 21;
static constexpr int TF_SD_D0_PIN = 39;
static constexpr const char *BLE_NOTIFY_DEVICE_NAME = "RLCD-Calender";
static constexpr const char *BLE_NOTIFY_SERVICE_UUID = "8d2f3a70-7c5a-4db8-9e42-8f2d9b5b9a01";
static constexpr const char *BLE_NOTIFY_CHARACTERISTIC_UUID = "8d2f3a71-7c5a-4db8-9e42-8f2d9b5b9a01";
static constexpr uint8_t BLE_NOTIFY_MAX_CLIENTS = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;
static constexpr uint8_t BLE_PAYLOAD_QUEUE_SIZE = 6;
static constexpr size_t BLE_PAYLOAD_MAX_LEN = 1536;
static constexpr int MEDIA_SOURCE_MAX = 4;
static constexpr int MEDIA_ICON_SIZE = 48;
static constexpr int MEDIA_ICON_BYTES = MEDIA_ICON_SIZE * MEDIA_ICON_SIZE / 8;
static constexpr int MEDIA_ICON_CHUNK_BYTES = 192;
static constexpr uint16_t MEDIA_CONN_NONE = 0xffff;
static constexpr uint32_t MEDIA_DEFAULT_TTL_MS = 5000;
static constexpr int TASK_LINE_SLOTS_PER_PAGE = 6;
static constexpr int TASK_TEXT_COLUMNS = 38;
static constexpr int TASK_MAX_LINES = 2;
static constexpr int BODY_SCALE = 2;
static constexpr int TITLE_SCALE = 4;
static constexpr int CLOCK_SCALE = 6;
static constexpr int SMALL_SCALE_NUM = 3;
static constexpr int SMALL_SCALE_DEN = 2;
static constexpr uint16_t CANVAS_WHITE = 0xffff;
static constexpr uint16_t CANVAS_BLACK = 0x0000;
static constexpr const char *TODOIST_BASE_URL = "https://api.todoist.com/api/v1";
static constexpr const char *OPENWEATHER_BASE_URL = "http://api.openweathermap.org/data/2.5/weather";
static constexpr size_t WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);
static constexpr int SHTC3_SDA_PIN = 13;
static constexpr int SHTC3_SCL_PIN = 14;
static constexpr uint8_t SHTC3_ADDR = 0x70;
static constexpr uint8_t SHTC3_TEMP_OFFSET_C = 4;

struct TodoistTaskItem {
  String content;
  String due;
};

struct TodoistProjectItem {
  String id;
  String name;
};

struct TaskDisplayItem {
  std::vector<String> lines;
  std::vector<int> x_offsets;
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

struct MediaInfo {
  String source_id;
  String title;
  String artist;
  String status;
  uint32_t position_ms = 0;
  uint32_t duration_ms = 0;
  uint32_t updated_ms = 0;
  uint32_t expires_ms = 0;
  uint16_t conn_id = MEDIA_CONN_NONE;
  bool valid = false;
  bool has_icon = false;
  uint8_t icon_chunk_mask = 0;
  uint8_t icon[MEDIA_ICON_BYTES] = {};
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
static SemaphoreHandle_t ble_payload_mutex = nullptr;
static BLEServer *ble_server = nullptr;
static BLECharacteristic *ble_notify_characteristic = nullptr;
static TaskHandle_t todoist_task_handle = nullptr;
static TaskHandle_t weather_task_handle = nullptr;
static TaskHandle_t ble_task_handle = nullptr;
static TaskHandle_t tf_card_task_handle = nullptr;
static std::vector<TodoistProjectItem> todoist_projects;
static std::vector<TodoistTaskItem> tasks;
static WeatherInfo weather_info;
static SensorInfo sensor_info;
static BleOverlayInfo ble_overlay;
static MediaInfo media_sources[MEDIA_SOURCE_MAX];
static volatile bool ble_overlay_dirty = false;
static volatile bool ble_status_dirty = false;
static volatile bool ble_payload_pending = false;
static volatile bool network_radio_busy = false;
static volatile bool ble_gatt_allowed = false;
static volatile uint8_t ble_client_count = 0;
static volatile uint8_t ble_payload_head = 0;
static volatile uint8_t ble_payload_tail = 0;
static volatile uint8_t ble_payload_count = 0;
static bool ble_initialized = false;
static bool shtc3_ready = false;
static int task_page = 0;
static int current_project_index = 0;
static int project_menu_index = 0;
static ScreenMode screen_mode = ScreenMode::Tasks;
static bool project_menu_open = false;
static RefreshVisualState refresh_visual_state = RefreshVisualState::Idle;
static uint32_t refresh_done_until_ms = 0;
static uint32_t last_time_sync_ms = 0;
static String app_status = "Starting";
static char ble_payload_buffer[BLE_PAYLOAD_QUEUE_SIZE][BLE_PAYLOAD_MAX_LEN] = {};
static uint16_t ble_payload_conn_buffer[BLE_PAYLOAD_QUEUE_SIZE] = {};

static void flushCallback(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map) {
  display.flushLvgl(area, color_map);
  lv_display_flush_ready(disp);
}

static bool isConfigured(const char *value) {
  return value && value[0] != '\0';
}

static void *psramJsonMalloc(size_t size) {
  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void psramJsonFree(void *ptr) {
  heap_caps_free(ptr);
}

static void initJsonAllocator() {
  cJSON_Hooks hooks = {};
  hooks.malloc_fn = psramJsonMalloc;
  hooks.free_fn = psramJsonFree;
  cJSON_InitHooks(&hooks);
}

static void setCoexPreference(esp_coex_prefer_t preference, const char *context) {
  const esp_err_t err = esp_coex_preference_set(preference);
  if (err != ESP_OK) {
    Serial.printf("[Radio] Failed to set coexist preference in %s: %d\n", context, static_cast<int>(err));
  }
}

static void lockNetworkRadio() {
  if (radio_mutex) {
    xSemaphoreTake(radio_mutex, portMAX_DELAY);
  }
  setCoexPreference(ESP_COEX_PREFER_WIFI, "network");
  network_radio_busy = true;
}

static void unlockNetworkRadio() {
  network_radio_busy = false;
  setCoexPreference(ESP_COEX_PREFER_BALANCE, "network");
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
  setCoexPreference(ESP_COEX_PREFER_BALANCE, "ble");
  if (radio_mutex) {
    xSemaphoreGive(radio_mutex);
  }
}

static BaseType_t createPinnedTaskPsram(TaskFunction_t task_function, const char *name, uint32_t stack_bytes,
                                        void *parameters, UBaseType_t priority,
                                        TaskHandle_t *task_handle, BaseType_t core_id) {
  BaseType_t result = xTaskCreatePinnedToCoreWithCaps(task_function, name, stack_bytes, parameters, priority,
                                                      task_handle, core_id, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (result == pdPASS) {
    return result;
  }

  Serial.printf("[Task] %s PSRAM stack allocation failed (%d), falling back to internal RAM\n",
                name, static_cast<int>(result));
  return xTaskCreatePinnedToCore(task_function, name, stack_bytes, parameters, priority, task_handle, core_id);
}

static int measureTextWidth(const String &text, int scale);
static void refreshNetworkClockIfDue();
static void handleKeyClick();
static void handleBootClick();
static void handleBootLongPress();
static bool isProjectMenuOpen();
static void toggleScreenMode();
static ScreenMode currentScreenMode();
static void startBootLoading(int total);
static void showBootLoading(int loaded, int total);
static void stopBootLoading();

#include "FontStorage.inc"

#include "UiTextPages.inc"

#include "HardwareStorage.inc"

#include "NetworkServices.inc"

#include "BackgroundBluetooth.inc"

#include "Navigation.inc"

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
  ble_payload_mutex = xSemaphoreCreateMutex();
  initJsonAllocator();
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  initBatteryAdc();
  setBatteryPercent(readBatteryPercent());
  initShtc3();
  updateSensorInfo();

  if (!lvglPortInit(RlcdDitherDisplay::WIDTH, RlcdDitherDisplay::HEIGHT, flushCallback)) {
    Serial.println("LVGL init failed");
    return;
  }
  if (lvglLock(-1)) {
    createUi();
    lvglUnlock();
  }
  startBootLoading(displayFontCount());
  if (!initFontStorage()) {
    Serial.println("[Font] No FATFS fonts loaded");
  }
  stopBootLoading();
  updateUi();

#if ENABLE_TF_CARD_READER
  if (TF_CARD_READER_MODE) {
    startTfCardReaderMode();
    return;
  }
#endif

  if (createPinnedTaskPsram(todoistWorker, "Todoist", 12288, nullptr, 2, &todoist_task_handle, 1) != pdPASS) {
    Serial.println("[Todoist] Failed to create Todoist task");
  }
  if (createPinnedTaskPsram(weatherWorker, "Weather", 12288, nullptr, 1, &weather_task_handle, 1) != pdPASS) {
    Serial.println("[Weather] Failed to create Weather task");
  }
  if (createPinnedTaskPsram(tfCardWorker, "TFCard", 4096, nullptr, 1, &tf_card_task_handle, 1) != pdPASS) {
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
  static uint32_t boot_press_start_ms = 0;
  static bool boot_long_handled = false;
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
      if (isProjectMenuOpen()) {
        key_click_count = 0;
        handleKeyClick();
      } else {
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
  }
  if (key_click_count == 1 && now - last_key_release_ms > KEY_DOUBLE_CLICK_MS) {
    key_click_count = 0;
    handleKeyClick();
  }

  const bool boot_pressed = digitalRead(BOOT_BUTTON_PIN) == LOW;
  if (boot_pressed != last_boot_pressed && now - last_boot_change_ms >= BOOT_DEBOUNCE_MS) {
    last_boot_change_ms = now;
    last_boot_pressed = boot_pressed;

    if (boot_pressed) {
      boot_press_start_ms = now;
      boot_long_handled = false;
    } else if (!boot_long_handled) {
      if (isProjectMenuOpen()) {
        boot_click_count = 0;
        handleBootClick();
      } else {
        if (boot_click_count == 0 || now - last_boot_release_ms <= BOOT_DOUBLE_CLICK_MS) {
          boot_click_count++;
        } else {
          boot_click_count = 1;
        }
        last_boot_release_ms = now;

        if (boot_click_count >= 2) {
          boot_click_count = 0;
          showDeviceStatusOverlay();
          updateUi();
        }
      }
    } else {
      boot_long_handled = false;
    }
  }
  if (boot_pressed && !boot_long_handled && now - boot_press_start_ms >= BOOT_LONG_PRESS_MS) {
    boot_click_count = 0;
    boot_long_handled = true;
    handleBootLongPress();
  }
  if (boot_click_count == 1 && now - last_boot_release_ms > BOOT_DOUBLE_CLICK_MS) {
    boot_click_count = 0;
    handleBootClick();
  } else if (boot_click_count > 1 && now - last_boot_release_ms > BOOT_DOUBLE_CLICK_MS) {
    boot_click_count = 0;
  }

  const ScreenMode mode_snapshot = currentScreenMode();
  if (now - last_battery_ms >= BATTERY_REFRESH_MS) {
    last_battery_ms = now;
    setBatteryPercent(readBatteryPercent());
  }
  if (now - last_sensor_ms >= SENSOR_REFRESH_MS) {
    last_sensor_ms = now;
    updateSensorInfo();
    updateUi();
  }
  String ble_payload;
  uint16_t ble_payload_conn_id = MEDIA_CONN_NONE;
  if (takeBlePayload(ble_payload, ble_payload_conn_id)) {
    handleBlePayloadJson(ble_payload.c_str(), ble_payload_conn_id);
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
