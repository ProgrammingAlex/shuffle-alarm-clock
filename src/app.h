#pragma once
#include <Arduino.h>

// Lets the web server ask the main loop to do things, so the alarm state
// machine is only ever changed from one place (main.cpp).
enum class AppAction : uint8_t {
  Snooze,
  StopAlarm,
  Preview,       // arg = song file name, or empty for "next shuffled song"
  StopPreview,
  TestAlarm,     // ring right now, like a real alarm
  AlarmsChanged,
  VolumeChanged, // volume setting changed: apply it to whatever is playing now
};

enum class AppState : uint8_t { Idle, Ringing, Snoozed, Preview };

namespace App {
  void post(AppAction action, const String& arg = "");
  AppState state();
  String ringingLabel();
  time_t snoozeUntil();
}
