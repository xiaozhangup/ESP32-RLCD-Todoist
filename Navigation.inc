static bool isProjectMenuOpen() {
  bool open = false;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  open = project_menu_open;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  return open;
}

static void moveProjectMenu(int delta) {
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  const int count = static_cast<int>(todoist_projects.size());
  if (project_menu_open && count > 0) {
    project_menu_index = (project_menu_index + delta + count) % count;
  }
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  updateUi();
}

static void openProjectMenu() {
  bool should_refresh_projects = false;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  const int count = static_cast<int>(todoist_projects.size());
  if (current_project_index < 0 || current_project_index >= count) {
    current_project_index = 0;
  }
  project_menu_index = current_project_index;
  project_menu_open = true;
  ble_overlay.visible = false;
  ble_overlay.visible_until_ms = 0;
  ble_overlay_dirty = false;
  should_refresh_projects = count == 0;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  if (should_refresh_projects && todoist_task_handle) {
    xTaskNotifyGive(todoist_task_handle);
  }
  updateUi();
}

static void selectProjectMenu() {
  bool changed = false;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  const int count = static_cast<int>(todoist_projects.size());
  if (project_menu_open && count > 0) {
    if (project_menu_index < 0 || project_menu_index >= count) {
      project_menu_index = 0;
    }
    changed = current_project_index != project_menu_index;
    current_project_index = project_menu_index;
    task_page = 0;
    app_status = changed ? "Loading tasks" : "";
  }
  project_menu_open = false;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  if (changed && todoist_task_handle) {
    xTaskNotifyGive(todoist_task_handle);
  }
  updateUi();
}

static void advanceTaskPage() {
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  if (screen_mode == ScreenMode::Tasks) {
    const int task_count = static_cast<int>(tasks.size());
    const int page_count = countTaskPages(tasks);
    task_page = (task_page + 1) % page_count;
  }
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  updateUi();
}

static void handleKeyClick() {
  if (isProjectMenuOpen()) {
    moveProjectMenu(-1);
  } else if (isBleOverlayVisible()) {
    hideBleOverlay();
    updateUi();
  } else {
    advanceTaskPage();
  }
}

static void handleBootClick() {
  if (isProjectMenuOpen()) {
    moveProjectMenu(1);
    return;
  }
  setAppStatus("Manual refresh");
  updateUi();
  if (todoist_task_handle) {
    xTaskNotifyGive(todoist_task_handle);
  } else {
    Serial.println("[Button] Todoist task is not running");
  }
}

static void handleBootLongPress() {
  if (isProjectMenuOpen()) {
    selectProjectMenu();
  } else {
    openProjectMenu();
  }
}

static void toggleScreenMode() {
  ScreenMode new_mode = ScreenMode::Tasks;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  screen_mode = screen_mode == ScreenMode::Tasks ? ScreenMode::Clock : ScreenMode::Tasks;
  ble_overlay.visible = false;
  ble_overlay.visible_until_ms = 0;
  ble_overlay_dirty = false;
  project_menu_open = false;
  new_mode = screen_mode;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  if (new_mode == ScreenMode::Clock) {
    updateSensorInfo();
    if (weather_task_handle) {
      xTaskNotifyGive(weather_task_handle);
    }
  }
  updateUi();
}

static ScreenMode currentScreenMode() {
  ScreenMode mode = ScreenMode::Tasks;
  if (data_mutex) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
  }
  mode = screen_mode;
  if (data_mutex) {
    xSemaphoreGive(data_mutex);
  }
  return mode;
}
