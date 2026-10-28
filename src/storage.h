#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <mutex>
#include <vector>
#include <time.h>

// days: bit 0 = Sunday ... bit 6 = Saturday. 0 = rings once, then turns itself off.
struct Alarm {
  uint8_t hour = 7;
  uint8_t minute = 0;
  uint8_t days = 0x3E;  // Mon-Fri
  bool enabled = true;
  String label;
};

struct Settings {
  uint8_t volume = 70;       // 0-100, loudest the alarm gets
  uint8_t rampSec = 30;      // seconds to fade from quiet to full volume (0 = off)
  uint8_t snoozeMin = 9;
  bool use24h = false;
  uint8_t dayBright = 200;   // screen backlight 0-255
  uint8_t nightBright = 15;
  uint8_t nightStart = 22;   // hour the screen dims
  uint8_t nightEnd = 7;      // hour it goes back to normal
};

namespace Storage {
  // Everything below is shared between the main loop and the web server,
  // so lock this mutex while touching alarms() or settings().
  std::recursive_mutex& mutex();

  void begin();
  std::vector<Alarm>& alarms();
  Settings& settings();
  void saveAlarms();
  void saveSettings();

  void alarmsToJson(JsonArray out);
  bool alarmsFromJson(JsonArrayConst in, String& error);
  void settingsToJson(JsonObject out);
  void settingsFromJson(JsonObjectConst in);

  // Next time any enabled alarm will ring after `now` (0 if none). Index via outIndex.
  time_t nextAlarm(time_t now, int* outIndex = nullptr);
}
