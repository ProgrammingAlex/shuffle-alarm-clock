#pragma once
#include <Arduino.h>

// What the screen should show. main.cpp fills this in every loop.
struct ScreenInfo {
  enum Mode { NORMAL, RINGING, SNOOZED, INFO, WIFI_SETUP } mode = NORMAL;
  bool timeValid = false;
  time_t now = 0;
  bool use24h = false;
  time_t nextAlarm = 0;       // 0 = none
  String alarmLabel;          // ringing / snoozed alarm name
  time_t snoozeUntil = 0;
  String song;                // now playing
  bool wifiConnected = false;
  String wifiName;            // network we're on / trying
  String ip;
  bool sdOk = true;
  size_t songCount = 0;
  bool previewing = false;
  String updateMsg;           // "Updating 45%" while new firmware installs
  uint8_t brightness = 200;
};

namespace Display {
  void begin();
  void update(const ScreenInfo& info);  // cheap to call often; only redraws on change
}
