#pragma once
#include <Arduino.h>

// Automatic firmware updates from GitHub Releases.
//  - Every night at UPDATE_HOUR:UPDATE_MINUTE the clock asks GitHub for the
//    latest release of GITHUB_REPO. If it's newer than FIRMWARE_VERSION and no
//    alarm is ringing or due within the hour, it downloads FIRMWARE_ASSET
//    (over HTTPS, certificate checked) and installs it.
//  - New firmware has to prove itself: it is only kept once it's back on
//    Wi-Fi. If it crashes or can't get online, the clock rolls back to the
//    previous version and won't try that version again.
namespace Updater {
  void begin();
  void loop();

  void requestCheck();       // "Check for updates" on the web page
  bool busy();
  int progress();            // 0-100 while installing, -1 otherwise
  String status();           // shown on the web page
  String latestVersion();    // newest version seen on GitHub ("" if unknown)
}
