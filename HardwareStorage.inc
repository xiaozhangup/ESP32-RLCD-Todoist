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

static bool updateTfCardStatus(bool *changed_out = nullptr) {
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

  if (!ready && changed) {
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
  if (load_eject && !start) {
    renderTfCardReaderScreen("电脑已弹出TF卡", "拔线或重启可返回普通固件");
  }
  return true;
}

static void tfUsbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base != ARDUINO_USB_EVENTS) {
    return;
  }

  switch (event_id) {
    case ARDUINO_USB_STARTED_EVENT:
      renderTfCardReaderScreen("USB已连接", "电脑应显示TF卡磁盘");
      break;
    case ARDUINO_USB_STOPPED_EVENT:
      renderTfCardReaderScreen("USB已断开", "等待电脑连接");
      break;
    case ARDUINO_USB_SUSPEND_EVENT:
      break;
    case ARDUINO_USB_RESUME_EVENT:
      break;
    default:
      break;
  }
}

static bool startTfCardReaderMode() {
  renderTfCardReaderScreen("正在挂载TF卡", "SDMMC 1-bit");

  if (!updateTfCardStatus()) {
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
