#pragma once
#include <Arduino.h>
#include <time.h>

// Keeps the ESP32's system clock correct.
//  - At boot it loads the time from the DS3231 (if one is connected).
//  - Once Wi-Fi is up it syncs from the internet (NTP) and writes the
//    result back to the DS3231, so the time survives power cuts.
namespace TimeKeeper {
  void begin();       // call early in setup()
  void startNtp();    // call once Wi-Fi has connected
  void loop();

  bool valid();       // true once we know the real time
  bool rtcPresent();
  bool ntpSynced();
  time_t now();
}
