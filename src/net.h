#pragma once
#include <Arduino.h>
#include <IPAddress.h>

// Wi-Fi connection + setup hotspot.
//  - Connects to the saved network (or DEFAULT_WIFI_SSID from config.h).
//  - With no saved network, or if it can't connect for 30 s, it also opens
//    the "AlarmClock-Setup" hotspot with a captive setup page, while it
//    keeps retrying the saved network in the background (so a router that
//    boots slower than the clock after a power cut doesn't break anything).
namespace Net {
  void begin();
  void loop();

  bool connected();
  bool setupActive();          // hotspot is on
  bool hasSavedNetwork();
  String ssid();               // network we're on / trying
  String ip();

  void startSetup();           // open the hotspot on demand (both buttons held)
  void saveNetwork(const String& ssid, const String& pass);  // saves + restarts
  void forgetNetwork();        // clears + restarts into setup
  String scanResultsJson();    // for the setup page
  void requestScan();
  bool isSetupRequest(const IPAddress& localIp);  // request came in over the hotspot?
}
