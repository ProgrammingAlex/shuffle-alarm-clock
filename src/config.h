#pragma once
// =====================================================================
//  Shuffle Alarm Clock - settings you might want to change
// =====================================================================

// ---------- Pins (ESP32-S3 N16R8) ----------
// Avoid GPIO 26-37 on N16R8 boards (used by flash + octal PSRAM),
// and 0 / 3 / 45 / 46 (boot strapping pins).

// MAX98357A I2S amplifier
#define PIN_I2S_BCLK   4
#define PIN_I2S_LRC    5
#define PIN_I2S_DOUT   6    // -> DIN on the amp
#define PIN_AMP_SD     9    // -> SD on the amp (HIGH = on, LOW = off, kills idle hiss)

// microSD card module (SPI)
#define PIN_SD_SCK     12
#define PIN_SD_MISO    13
#define PIN_SD_MOSI    11
#define PIN_SD_CS      10

// ST7789V 2" 240x320 LCD (SPI, separate bus from the SD card)
#define PIN_TFT_SCK    18   // SCL / SCK on the module
#define PIN_TFT_MOSI   17   // SDA / MOSI / DIN on the module
#define PIN_TFT_DC     16
#define PIN_TFT_CS     15
#define PIN_TFT_RST    7
#define PIN_TFT_BL     8    // BL / LED on the module (PWM dimming)

// DS3231 RTC (I2C) - optional, the clock works without it
#define PIN_I2C_SDA    1
#define PIN_I2C_SCL    2

// Buttons: wire each between the pin and GND (internal pull-ups used)
#define PIN_BTN_SNOOZE 39
#define PIN_BTN_OFF    40

// ---------- Display ----------
// If colours look inverted (black shows white), flip TFT_INVERT.
// If red and blue are swapped, flip TFT_RGB_ORDER.
// If the picture is upside down, change TFT_ROTATION to 3.
#define TFT_INVERT     true
#define TFT_RGB_ORDER  false
#define TFT_ROTATION   1

// ---------- Network ----------
#define DEVICE_HOSTNAME  "alarmclock"          // -> http://alarmclock.local
#define SETUP_AP_NAME    "AlarmClock-Setup"    // hotspot shown during Wi-Fi setup
// Optional: pre-load the recipient's Wi-Fi so it connects out of the box.
// Leave empty to use the setup hotspot instead. Wi-Fi saved through the
// setup page always takes priority over these.
#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASS ""

// ---------- Time ----------
// Eastern time with automatic daylight saving changes
#define TZ_INFO        "EST5EDT,M3.2.0,M11.1.0"
#define NTP_SERVER_1   "pool.ntp.org"
#define NTP_SERVER_2   "time.nrc.ca"

// ---------- Alarm behaviour ----------
#define MUSIC_DIR        "/music"   // songs live in this folder on the SD card
#define MAX_ALARMS       8
#define RING_TIMEOUT_MIN 30         // stop ringing on its own after this long

// ---------- Updates ----------
// Release builds get their version from the git tag (see .github/workflows).
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0.0"
#endif
#define GITHUB_REPO     "ProgrammingAlex/shuffle-alarm-clock"  // public repo with the releases
#define FIRMWARE_ASSET  "firmware.bin"   // file attached to each GitHub release
#define UPDATE_HOUR     3                // nightly update check, local time
#define UPDATE_MINUTE   30
