#pragma once

// Fill these before uploading to the ESP32-S3 board.
struct WifiCredential {
  const char *ssid;
  const char *password;
};

static constexpr WifiCredential WIFI_NETWORKS[] = {
  // {"Another WiFi", "password"},
};

static constexpr const char *TODOIST_API_TOKEN = "";
static constexpr const char *OPENWEATHER_API_KEY = "";
static constexpr const char *OPENWEATHER_CITY = "Beijing";

// POSIX timezone string for China Standard Time.
static constexpr const char *LOCAL_TIMEZONE = "CST-8";
