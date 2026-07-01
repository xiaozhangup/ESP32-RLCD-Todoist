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
}

static String jsonEscape(const String &text) {
  String out;
  out.reserve(text.length() + 8);
  for (size_t i = 0; i < text.length(); i++) {
    const char c = text[i];
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

static uint32_t kbUsed(uint32_t total, uint32_t free_value) {
  return total > free_value ? (total - free_value) / 1024 : 0;
}

static String buildDeviceStatusJson() {
  int battery_snapshot = -1;
  uint8_t ble_count_snapshot = 0;
  int task_count_snapshot = 0;
  WeatherInfo weather_snapshot;
  SensorInfo sensor_snapshot;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  battery_snapshot = battery_percent;
  ble_count_snapshot = ble_client_count;
  task_count_snapshot = static_cast<int>(tasks.size());
  weather_snapshot = weather_info;
  sensor_snapshot = sensor_info;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }

  String json;
  json.reserve(320);
  json += "{\"battery\":";
  json += battery_snapshot;
  json += ",\"uptime_ms\":";
  json += millis();
  json += ",\"free_heap\":";
  json += ESP.getFreeHeap();
  json += ",\"free_psram\":";
  json += ESP.getFreePsram();
  json += ",\"ble_clients\":";
  json += static_cast<unsigned>(ble_count_snapshot);
  json += ",\"wifi\":\"";
  json += jsonEscape(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String(""));
  json += "\",\"rssi\":";
  if (WiFi.status() == WL_CONNECTED) {
    json += WiFi.RSSI();
  } else {
    json += 0;
  }
  json += ",\"tasks\":";
  json += task_count_snapshot;
  json += ",\"weather\":\"";
  json += jsonEscape(weather_snapshot.valid ? weather_snapshot.description : String(""));
  json += "\",\"outdoor_temp_c\":";
  if (weather_snapshot.valid) {
    json += weather_snapshot.temp_c;
  } else {
    json += "null";
  }
  json += ",\"sensor_temp_c\":";
  if (sensor_snapshot.valid) {
    json += String(sensor_snapshot.temp_c, 1);
  } else {
    json += "null";
  }
  json += ",\"sensor_humidity\":";
  if (sensor_snapshot.valid) {
    json += String(sensor_snapshot.humidity, 1);
  } else {
    json += "null";
  }
  json += "}";
  return json;
}

static void refreshBleStatusValue() {
  if (ble_notify_characteristic) {
    ble_notify_characteristic->setValue(buildDeviceStatusJson());
  }
}

static void showDeviceStatusOverlay() {
  int battery_snapshot = -1;
  uint8_t ble_count_snapshot = 0;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  battery_snapshot = battery_percent;
  ble_count_snapshot = ble_client_count;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }

  const uint32_t internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  const uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  String content;
  content.reserve(180);
  content += "电量: ";
  if (battery_snapshot >= 0) {
    content += String(battery_snapshot) + "%";
  } else {
    content += "--";
  }
  content += "\n内存: ";
  content += kbUsed(internal_total, internal_free);
  content += "/";
  content += internal_total / 1024;
  content += "KB PS:";
  content += kbUsed(psram_total, psram_free);
  content += "/";
  content += psram_total / 1024;
  content += "KB\n蓝牙: ";
  content += String(static_cast<unsigned>(ble_count_snapshot));
  content += "/";
  content += String(static_cast<unsigned>(BLE_NOTIFY_MAX_CLIENTS));
  content += " 个设备\nWiFi: ";
  if (WiFi.status() == WL_CONNECTED) {
    content += truncateUtf8(WiFi.SSID(), 20);
  } else {
    content += "未连接";
  }
  content += "\n信号: ";
  if (WiFi.status() == WL_CONNECTED) {
    content += WiFi.RSSI();
    content += "dBm ";
    content += wifiSignalLevel();
    content += "/4";
  } else {
    content += "--";
  }

  setBleOverlay("设备状态", content, 12);
  refreshBleStatusValue();
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

static int hexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

static bool parseMediaIconHexBytes(const char *hex, uint8_t *target, int bytes) {
  if (!hex || bytes <= 0 || strlen(hex) < static_cast<size_t>(bytes * 2)) {
    return false;
  }
  for (int i = 0; i < bytes; i++) {
    const int hi = hexValue(hex[i * 2]);
    const int lo = hexValue(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      return false;
    }
    target[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}

static bool parseMediaIconHex(const char *hex, uint8_t *target) {
  return parseMediaIconHexBytes(hex, target, MEDIA_ICON_BYTES);
}

static void clearMediaSourceLocked(const String &source_id) {
  for (MediaInfo &media : media_sources) {
    if (media.source_id == source_id) {
      media = MediaInfo();
    }
  }
}

static bool parseMediaJson(cJSON *media_json, uint16_t conn_id) {
  cJSON *id = cJSON_GetObjectItem(media_json, "id");
  const String source_id = cJSON_IsString(id) && id->valuestring ? String(id->valuestring) : String("ble");
  const bool clear = cJSON_IsTrue(cJSON_GetObjectItem(media_json, "clear"));

  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  if (clear) {
    clearMediaSourceLocked(source_id);
  } else {
    cJSON *title = cJSON_GetObjectItem(media_json, "title");
    cJSON *artist = cJSON_GetObjectItem(media_json, "artist");
    cJSON *status = cJSON_GetObjectItem(media_json, "status");
    cJSON *pos = cJSON_GetObjectItem(media_json, "pos");
    cJSON *dur = cJSON_GetObjectItem(media_json, "dur");
    cJSON *ttl = cJSON_GetObjectItem(media_json, "ttl");
    cJSON *icon = cJSON_GetObjectItem(media_json, "icon");
    cJSON *icon_part = cJSON_GetObjectItem(media_json, "icon_part");
    cJSON *icon_total = cJSON_GetObjectItem(media_json, "icon_total");
    const bool has_info = cJSON_IsString(title) || cJSON_IsString(artist) || cJSON_IsString(status) ||
                          cJSON_IsNumber(pos) || cJSON_IsNumber(dur);

    MediaInfo *slot = nullptr;
    for (MediaInfo &media : media_sources) {
      if (media.source_id == source_id) {
        slot = &media;
        break;
      }
    }
    if (!slot && !has_info && cJSON_IsString(icon) && (!icon->valuestring || !icon->valuestring[0])) {
      if (data_mutex) {
        xSemaphoreGive(data_mutex);
      }
      updateUi();
      return true;
    }
    if (!slot) {
      for (MediaInfo &media : media_sources) {
        if (media.source_id.length() == 0) {
          slot = &media;
          break;
        }
      }
    }
    if (!slot) {
      slot = &media_sources[0];
    }

    slot->source_id = source_id;
    slot->conn_id = conn_id;
    if (has_info) {
      slot->title = cJSON_IsString(title) && title->valuestring ? String(title->valuestring) : String("");
      slot->artist = cJSON_IsString(artist) && artist->valuestring ? String(artist->valuestring) : String("");
      slot->status = cJSON_IsString(status) && status->valuestring ? String(status->valuestring) : String("Stopped");
      slot->position_ms = cJSON_IsNumber(pos) ? static_cast<uint32_t>(max(0, pos->valueint)) : 0;
      slot->duration_ms = cJSON_IsNumber(dur) ? static_cast<uint32_t>(max(0, dur->valueint)) : 0;
      slot->updated_ms = millis();
      slot->expires_ms = slot->updated_ms + (cJSON_IsNumber(ttl) ? max(1000, ttl->valueint) : MEDIA_DEFAULT_TTL_MS);
      slot->valid = slot->title.length() > 0 || slot->artist.length() > 0;
    }
    if (cJSON_IsString(icon)) {
      if (cJSON_IsNumber(icon_part) && cJSON_IsNumber(icon_total)) {
        const int part = icon_part->valueint;
        const int total = icon_total->valueint;
        const int offset = part * MEDIA_ICON_CHUNK_BYTES;
        const int bytes = min(MEDIA_ICON_CHUNK_BYTES, MEDIA_ICON_BYTES - offset);
        if (part >= 0 && total > 0 && total <= 8 && offset >= 0 && offset < MEDIA_ICON_BYTES &&
            icon->valuestring && icon->valuestring[0] &&
            parseMediaIconHexBytes(icon->valuestring, slot->icon + offset, bytes)) {
          if (part == 0) {
            slot->icon_chunk_mask = 0;
            slot->has_icon = false;
          }
          slot->icon_chunk_mask |= 1 << part;
          slot->has_icon = slot->icon_chunk_mask == ((1 << total) - 1);
        }
      } else {
        slot->icon_chunk_mask = 0;
        slot->has_icon = icon->valuestring && icon->valuestring[0] && parseMediaIconHex(icon->valuestring, slot->icon);
      }
    }
  }
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  updateUi();
  return true;
}

static bool handleBlePayloadJson(const char *json, uint16_t conn_id = MEDIA_CONN_NONE) {
  cJSON *root = cJSON_Parse(json);
  if (!root) {
    Serial.printf("[BLE] Invalid JSON near: %s\n", cJSON_GetErrorPtr());
    return false;
  }

  cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
  if (cJSON_IsString(cmd) && cmd->valuestring) {
    const String command = cmd->valuestring;
    cJSON_Delete(root);
    if (command == "key") {
      handleKeyClick();
    } else if (command == "key2") {
      toggleScreenMode();
    } else if (command == "boot") {
      handleBootClick();
    } else if (command == "boot2") {
      showDeviceStatusOverlay();
      updateUi();
    } else if (command == "boot_long") {
      handleBootLongPress();
    } else if (command == "status") {
      refreshBleStatusValue();
    } else {
      Serial.printf("[BLE] Unknown command: %s\n", command.c_str());
      return false;
    }
    refreshBleStatusValue();
    return true;
  }

  cJSON *media = cJSON_GetObjectItem(root, "media");
  if (cJSON_IsObject(media)) {
    const bool ok = parseMediaJson(media, conn_id);
    cJSON_Delete(root);
    return ok;
  }

  cJSON_Delete(root);
  return parseBleOverlayJson(json);
}

static void enqueueBlePayload(const String &payload, uint16_t conn_id = MEDIA_CONN_NONE) {
  if (ble_payload_mutex) {
    xSemaphoreTake(ble_payload_mutex, portMAX_DELAY);
  }
  if (ble_payload_count >= BLE_PAYLOAD_QUEUE_SIZE) {
    ble_payload_tail = (ble_payload_tail + 1) % BLE_PAYLOAD_QUEUE_SIZE;
    ble_payload_count--;
  }
  const size_t copy_len = min(payload.length(), BLE_PAYLOAD_MAX_LEN - 1);
  memcpy(ble_payload_buffer[ble_payload_head], payload.c_str(), copy_len);
  ble_payload_buffer[ble_payload_head][copy_len] = '\0';
  ble_payload_conn_buffer[ble_payload_head] = conn_id;
  ble_payload_head = (ble_payload_head + 1) % BLE_PAYLOAD_QUEUE_SIZE;
  ble_payload_count++;
  ble_payload_pending = ble_payload_count > 0;
  if (ble_payload_mutex) {
    xSemaphoreGive(ble_payload_mutex);
  }
}

static bool takeBlePayload(String &payload, uint16_t &conn_id) {
  bool has_payload = false;
  if (ble_payload_mutex) {
    xSemaphoreTake(ble_payload_mutex, portMAX_DELAY);
  }
  if (ble_payload_count > 0) {
    payload = ble_payload_buffer[ble_payload_tail];
    conn_id = ble_payload_conn_buffer[ble_payload_tail];
    ble_payload_buffer[ble_payload_tail][0] = '\0';
    ble_payload_conn_buffer[ble_payload_tail] = MEDIA_CONN_NONE;
    ble_payload_tail = (ble_payload_tail + 1) % BLE_PAYLOAD_QUEUE_SIZE;
    ble_payload_count--;
    ble_payload_pending = ble_payload_count > 0;
    has_payload = true;
  }
  if (ble_payload_mutex) {
    xSemaphoreGive(ble_payload_mutex);
  }
  return has_payload;
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

static void hideBleOverlay() {
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  ble_overlay.visible = false;
  ble_overlay.visible_until_ms = 0;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  ble_overlay_dirty = false;
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

static void tfCardWorker(void *) {
  for (;;) {
    bool tf_card_changed = false;
    updateTfCardStatus(&tf_card_changed);
    if (tf_card_changed) {
      updateUi();
    }
    vTaskDelay(pdMS_TO_TICKS(TF_CARD_REFRESH_MS));
  }
}

class BleNotifyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    const uint32_t connected = min<uint32_t>(BLE_NOTIFY_MAX_CLIENTS, server->getConnectedCount() + 1);
    ble_client_count = static_cast<uint8_t>(connected);
    ble_status_dirty = true;
    Serial.printf("[BLE] Client connected, count=%u/%u\n",
                  static_cast<unsigned>(ble_client_count), static_cast<unsigned>(BLE_NOTIFY_MAX_CLIENTS));
    if (connected < BLE_NOTIFY_MAX_CLIENTS) {
      server->startAdvertising();
    }
  }

  void onDisconnect(BLEServer *server) override {
    ble_client_count = static_cast<uint8_t>(min<uint32_t>(BLE_NOTIFY_MAX_CLIENTS, server->getConnectedCount()));
    ble_status_dirty = true;
    if (ble_client_count == 0 && data_mutex) {
      xSemaphoreTake(data_mutex, portMAX_DELAY);
      for (MediaInfo &media : media_sources) {
        media = MediaInfo();
      }
      xSemaphoreGive(data_mutex);
    }
    Serial.printf("[BLE] Client disconnected, count=%u/%u\n",
                  static_cast<unsigned>(ble_client_count), static_cast<unsigned>(BLE_NOTIFY_MAX_CLIENTS));
    server->startAdvertising();
  }

  void onDisconnect(BLEServer *, ble_gap_conn_desc *desc) override {
    if (!desc || !data_mutex) {
      return;
    }
    xSemaphoreTake(data_mutex, portMAX_DELAY);
    for (MediaInfo &media : media_sources) {
      if (media.valid && media.conn_id == desc->conn_handle) {
        media = MediaInfo();
      }
    }
    xSemaphoreGive(data_mutex);
    updateUi();
  }
};

class BleNotifyWriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    enqueueBlePayload(characteristic->getValue());
  }

  void onWrite(BLECharacteristic *characteristic, ble_gap_conn_desc *desc) override {
    enqueueBlePayload(characteristic->getValue(), desc ? desc->conn_handle : MEDIA_CONN_NONE);
  }

  void onRead(BLECharacteristic *characteristic) override {
    characteristic->setValue(buildDeviceStatusJson());
  }
};

static BleNotifyServerCallbacks ble_server_callbacks;
static BleNotifyWriteCallbacks ble_write_callbacks;

static bool initBleGattServer() {
  if (ble_initialized && ble_server && ble_notify_characteristic) {
    return true;
  }

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
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  ble_notify_characteristic->setCallbacks(&ble_write_callbacks);
  refreshBleStatusValue();
  service->start();

  BLEAdvertising *advertising = ble_server->getAdvertising();
  advertising->addServiceUUID(BLE_NOTIFY_SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();
  ble_initialized = true;
  return true;
}

static void bleGattWorker(void *) {
  while (!ble_gatt_allowed || WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  for (;;) {
    if (ble_initialized) {
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

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
