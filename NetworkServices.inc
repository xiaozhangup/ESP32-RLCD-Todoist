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
  setCoexPreference(ESP_COEX_PREFER_WIFI, "wifi connect");
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);

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
        delay(300);
      }

      if (WiFi.status() == WL_CONNECTED) {
        WiFi.setSleep(false);
        esp_wifi_set_ps(WIFI_PS_NONE);
        Serial.printf("[WiFi] Connected, SSID=%s IP=%s RSSI=%d dBm\n",
                      ssid, WiFi.localIP().toString().c_str(), WiFi.RSSI());
        refreshNetworkClockIfDue();
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

static void refreshNetworkClockIfDue() {
  const uint32_t now_ms = millis();
  if (last_time_sync_ms != 0 && now_ms - last_time_sync_ms < TIME_SYNC_REFRESH_MS) {
    return;
  }
  configTzTime(LOCAL_TIMEZONE, "pool.ntp.org", "time.nist.gov");
  last_time_sync_ms = now_ms;
  Serial.printf("[Time] NTP configured silently, timezone=%s\n", LOCAL_TIMEZONE);
}

class PsramHttpBody : public Stream {
public:
  ~PsramHttpBody() override {
    reset();
  }

  bool reserve(size_t requested_capacity) {
    if (requested_capacity <= capacity_) {
      return true;
    }

    char *next = static_cast<char *>(heap_caps_realloc(data_, requested_capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!next) {
      next = static_cast<char *>(heap_caps_malloc(requested_capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
      if (next && data_) {
        memcpy(next, data_, length_ + 1);
        heap_caps_free(data_);
      }
    }
    if (!next) {
      return false;
    }

    data_ = next;
    capacity_ = requested_capacity;
    if (length_ == 0) {
      data_[0] = '\0';
    }
    return true;
  }

  void reset() {
    if (data_) {
      heap_caps_free(data_);
    }
    data_ = nullptr;
    length_ = 0;
    capacity_ = 0;
  }

  void swap(PsramHttpBody &other) {
    char *data = data_;
    const size_t length = length_;
    const size_t capacity = capacity_;
    data_ = other.data_;
    length_ = other.length_;
    capacity_ = other.capacity_;
    other.data_ = data;
    other.length_ = length;
    other.capacity_ = capacity;
  }

  size_t write(uint8_t value) override {
    return write(&value, 1);
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    if (!buffer || size == 0) {
      return 0;
    }

    const size_t needed = length_ + size + 1;
    if (needed > capacity_) {
      size_t next_capacity = capacity_ > 0 ? capacity_ : 512;
      while (next_capacity < needed) {
        next_capacity *= 2;
      }
      if (!reserve(next_capacity)) {
        Serial.printf("[HTTP] PSRAM body allocation failed, need=%u current=%u psram_free=%u\n",
                      static_cast<unsigned>(needed), static_cast<unsigned>(capacity_), ESP.getFreePsram());
        return 0;
      }
    }

    memcpy(data_ + length_, buffer, size);
    length_ += size;
    data_[length_] = '\0';
    return size;
  }

  int available() override {
    return 0;
  }

  int read() override {
    return -1;
  }

  int peek() override {
    return -1;
  }

  void flush() override {}

  const char *c_str() const {
    return data_ ? data_ : "";
  }

  size_t length() const {
    return length_;
  }

  bool empty() const {
    return length_ == 0;
  }

  String preview(size_t max_len) const {
    if (!data_) {
      return "";
    }
    if (length_ <= max_len) {
      return String(data_);
    }
    String text;
    text.reserve(max_len + 3);
    for (size_t i = 0; i < max_len; i++) {
      text += data_[i];
    }
    text += "...";
    return text;
  }

private:
  char *data_ = nullptr;
  size_t length_ = 0;
  size_t capacity_ = 0;
};

static bool readHttpBodyToPsram(HTTPClient &http, PsramHttpBody &body, const char *tag) {
  body.reset();
  const int content_length = http.getSize();
  if (content_length > 0 && !body.reserve(static_cast<size_t>(content_length) + 1)) {
    Serial.printf("[%s] Cannot reserve PSRAM response body, size=%d psram=%u\n",
                  tag, content_length, ESP.getFreePsram());
    return false;
  }

  const int bytes_written = http.writeToStream(&body);
  if (bytes_written < 0) {
    Serial.printf("[%s] Body read failed: %d (%s)\n",
                  tag, bytes_written, HTTPClient::errorToString(bytes_written).c_str());
    return false;
  }

  return true;
}

static int readTlsError(WiFiClientSecure &client, char *buffer, size_t buffer_size) {
  return client.lastError(buffer, buffer_size);
}

static int readTlsError(WiFiClient &, char *buffer, size_t buffer_size) {
  if (buffer_size > 0) {
    buffer[0] = '\0';
  }
  return 0;
}

template <typename ClientT>
static bool performHttpGetWithClient(const char *tag, const String &url, PsramHttpBody &body,
                                     const char *bearer_token, ClientT &client) {
  body.reset();
  lockNetworkRadio();
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[%s] HTTP skipped because WiFi is %s(%d)\n", tag, wifiStatusName(WiFi.status()), WiFi.status());
    unlockNetworkRadio();
    return false;
  }

  bool success = false;
  String last_http_error;
  int last_tls_error = 0;
  char last_tls_text[128] = {};

  for (int attempt = 1; attempt <= 2; attempt++) {
    client.setTimeout(15000);

    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    http.setReuse(false);
    http.useHTTP10(true);

    if (!http.begin(client, url)) {
      Serial.printf("[%s] HTTP begin failed\n", tag);
      client.stop();
      last_http_error = HTTPClient::errorToString(HTTPC_ERROR_CONNECTION_REFUSED);
      if (attempt < 2) {
        delay(300);
        continue;
      }
      break;
    }

    if (bearer_token && bearer_token[0] != '\0') {
      http.addHeader("Authorization", String("Bearer ") + bearer_token);
    }
    http.addHeader("Content-Type", "application/json");

    const int code = http.GET();
    last_http_error = code < 0 ? HTTPClient::errorToString(code) : String();
    last_tls_text[0] = '\0';
    last_tls_error = readTlsError(client, last_tls_text, sizeof(last_tls_text));

    PsramHttpBody response_body;
    bool body_read = true;
    if (code > 0) {
      body_read = readHttpBodyToPsram(http, response_body, tag);
    }

    http.end();
    client.stop();

    if (code >= 200 && code < 300 && body_read) {
      body.swap(response_body);
      success = true;
      break;
    }

    if (code > 0) {
      Serial.printf("[%s] HTTP %d, %u bytes: %s\n",
                    tag, code, static_cast<unsigned>(response_body.length()), response_body.preview(160).c_str());
      break;
    }

    Serial.printf("[%s] HTTP %d (%s), tls=%d %s\n",
                  tag, code, last_http_error.c_str(), last_tls_error, last_tls_text);
    if (attempt < 2) {
      delay(300);
    }
  }
  unlockNetworkRadio();
  return success;
}

static bool httpGet(const String &url, PsramHttpBody &body) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(20);
  return performHttpGetWithClient("Todoist", url, body, TODOIST_API_TOKEN, client);
}

static bool httpGetPlain(const String &url, PsramHttpBody &body, const char *tag) {
  if (url.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(20);
    return performHttpGetWithClient(tag, url, body, nullptr, client);
  }

  WiFiClient client;
  return performHttpGetWithClient(tag, url, body, nullptr, client);
}

static bool parseWeather(const char *body, WeatherInfo &target) {
  cJSON *root = cJSON_Parse(body);
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
  PsramHttpBody body;
  WeatherInfo fetched;
  if (!httpGetPlain(url, body, "Weather") || !parseWeather(body.c_str(), fetched)) {
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

static bool parseProjects(const char *body, String &next_cursor,
                          std::vector<TodoistProjectItem> &target,
                          String &inbox_project_id) {
  cJSON *root = cJSON_Parse(body);
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
      if (!cJSON_IsString(id) || !id->valuestring) {
        continue;
      }

      TodoistProjectItem item;
      item.id = id->valuestring;
      item.name = cJSON_IsString(name) && name->valuestring && name->valuestring[0] != '\0'
                    ? String(name->valuestring)
                    : String("未命名项目");
      target.push_back(item);

      const bool is_inbox_by_name = cJSON_IsString(name) && name->valuestring && strcmp(name->valuestring, "Inbox") == 0;
      if (inbox_project_id.length() == 0 && (cJSON_IsTrue(inbox) || cJSON_IsTrue(is_inbox) || is_inbox_by_name)) {
        inbox_project_id = id->valuestring;
      }
    }
  }

  cJSON_Delete(root);
  return true;
}

static bool parseTasks(const char *body, String &next_cursor, std::vector<TodoistTaskItem> &target) {
  cJSON *root = cJSON_Parse(body);
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
      target.insert(target.begin(), item);
    }
  }

  cJSON_Delete(root);
  return true;
}

static bool fetchTodoistProjects(std::vector<TodoistProjectItem> &target, String &inbox_project_id) {
  String cursor;
  do {
    String url = String(TODOIST_BASE_URL) + "/projects";
    if (cursor.length() > 0) {
      url += "?cursor=" + urlEncode(cursor);
    }

    PsramHttpBody body;
    if (!httpGet(url, body) || !parseProjects(body.c_str(), cursor, target, inbox_project_id)) {
      Serial.println("[Todoist] Failed while fetching projects");
      return false;
    }
  } while (cursor.length() > 0);

  if (target.empty()) {
    Serial.println("[Todoist] No projects found");
    return false;
  }
  return true;
}

static bool fetchTodoistTasks() {
  if (!isConfigured(TODOIST_API_TOKEN)) {
    Serial.println("[Todoist] Missing TODOIST_API_TOKEN in TodoistConfig.h");
    setAppStatus("Missing Todoist token");
    return false;
  }

  setRefreshVisualState(RefreshVisualState::Spinning);
  updateUi();

  std::vector<TodoistProjectItem> fetched_projects;
  String inbox_project_id;
  setAppStatus("Loading projects");
  updateUi();
  if (!fetchTodoistProjects(fetched_projects, inbox_project_id)) {
    Serial.println("[Todoist] Cannot load projects");
    setAppStatus("Projects not found");
    setRefreshVisualState(RefreshVisualState::Idle);
    updateUi();
    return false;
  }

  String previous_project_id;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  if (current_project_index >= 0 && current_project_index < static_cast<int>(todoist_projects.size())) {
    previous_project_id = todoist_projects[current_project_index].id;
  }
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }

  int selected_project_index = 0;
  for (int i = 0; i < static_cast<int>(fetched_projects.size()); i++) {
    if ((previous_project_id.length() > 0 && fetched_projects[i].id == previous_project_id) ||
        (previous_project_id.length() == 0 && inbox_project_id.length() > 0 && fetched_projects[i].id == inbox_project_id)) {
      selected_project_index = i;
      break;
    }
  }
  const String selected_project_id = fetched_projects[selected_project_index].id;

  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  todoist_projects = fetched_projects;
  current_project_index = selected_project_index;
  if (project_menu_index < 0 || project_menu_index >= static_cast<int>(todoist_projects.size())) {
    project_menu_index = current_project_index;
  }
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }

  std::vector<TodoistTaskItem> fetched;
  fetched.reserve(16);
  String cursor;
  do {
    setAppStatus("Loading tasks");
    updateUi();
    String url = String(TODOIST_BASE_URL) + "/tasks?project_id=" + urlEncode(selected_project_id);
    if (cursor.length() > 0) {
      url += "&cursor=" + urlEncode(cursor);
    }

    PsramHttpBody body;
    if (!httpGet(url, body) || !parseTasks(body.c_str(), cursor, fetched)) {
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
  if (current_project_index >= 0 && current_project_index < static_cast<int>(todoist_projects.size()) &&
      todoist_projects[current_project_index].id == selected_project_id) {
    tasks = fetched;
    task_page = 0;
    app_status = "";
  }
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  setRefreshVisualState(RefreshVisualState::Done);
  return true;
}

static void todoistWorker(void *) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED && !connectWifi()) {
      Serial.println("[Todoist] WiFi unavailable, waiting before retry");
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000));
      continue;
    }

    fetchTodoistTasks();
    if (!ble_gatt_allowed) {
      ble_gatt_allowed = true;
    }
    updateUi();
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TODOIST_REFRESH_MS));
  }
}

static void weatherWorker(void *) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED && !connectWifi()) {
      Serial.println("[Weather] WiFi unavailable, waiting before retry");
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000));
      continue;
    }

    refreshNetworkClockIfDue();
    fetchWeather();
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WEATHER_REFRESH_MS));
  }
}
