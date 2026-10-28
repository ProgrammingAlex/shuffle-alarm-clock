#include "timekeeper.h"
#include <Wire.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include "config.h"

namespace {
  constexpr uint8_t DS3231_ADDR = 0x68;
  constexpr time_t VALID_AFTER = 1735689600;  // 2025-01-01

  bool s_rtc = false;
  bool s_ntpStarted = false;
  volatile bool s_ntpSynced = false;
  volatile bool s_syncPending = false;
  uint32_t s_lastRtcWrite = 0;

  uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
  uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

  // Days since 1970-01-01 for a UTC date (no timezone involved)
  int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
  }

  bool rtcRead(time_t& out) {
    // Status register: bit 7 (OSF) set means the RTC lost power and its time is junk
    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(0x0F);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(DS3231_ADDR, (uint8_t)1) != 1) return false;
    if (Wire.read() & 0x80) return false;

    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(DS3231_ADDR, (uint8_t)7) != 7) return false;
    uint8_t ss = bcd2dec(Wire.read() & 0x7F);
    uint8_t mm = bcd2dec(Wire.read() & 0x7F);
    uint8_t hh = bcd2dec(Wire.read() & 0x3F);
    Wire.read();  // day of week
    uint8_t d = bcd2dec(Wire.read() & 0x3F);
    uint8_t mo = bcd2dec(Wire.read() & 0x1F);
    int y = 2000 + bcd2dec(Wire.read());
    if (mo < 1 || mo > 12 || d < 1 || d > 31) return false;
    out = (time_t)(daysFromCivil(y, mo, d) * 86400LL + hh * 3600 + mm * 60 + ss);
    return out > VALID_AFTER;
  }

  void rtcWrite(time_t t) {
    struct tm u;
    gmtime_r(&t, &u);  // RTC stores UTC; timezone is applied in software
    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(0x00);
    Wire.write(dec2bcd(u.tm_sec));
    Wire.write(dec2bcd(u.tm_min));
    Wire.write(dec2bcd(u.tm_hour));  // 24-hour mode
    Wire.write(dec2bcd(u.tm_wday + 1));
    Wire.write(dec2bcd(u.tm_mday));
    Wire.write(dec2bcd(u.tm_mon + 1));
    Wire.write(dec2bcd(u.tm_year - 100));
    Wire.endTransmission();

    // Clear the "oscillator stopped" flag so future reads are trusted
    Wire.beginTransmission(DS3231_ADDR);
    Wire.write(0x0F);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom(DS3231_ADDR, (uint8_t)1) == 1) {
      uint8_t status = Wire.read() & ~0x80;
      Wire.beginTransmission(DS3231_ADDR);
      Wire.write(0x0F);
      Wire.write(status);
      Wire.endTransmission();
    }
  }

  void onNtpSync(struct timeval*) {
    s_ntpSynced = true;
    s_syncPending = true;  // write to the RTC from loop(), not this callback
  }
}

namespace TimeKeeper {

void begin() {
  setenv("TZ", TZ_INFO, 1);
  tzset();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  Wire.beginTransmission(DS3231_ADDR);
  s_rtc = (Wire.endTransmission() == 0);

  if (s_rtc) {
    time_t t;
    if (rtcRead(t)) {
      struct timeval tv = {t, 0};
      settimeofday(&tv, nullptr);
      Serial.println("[time] loaded from DS3231");
    } else {
      Serial.println("[time] DS3231 found but time not set yet");
    }
  } else {
    Serial.println("[time] no DS3231 found, waiting for NTP");
  }
}

void startNtp() {
  if (s_ntpStarted) return;
  s_ntpStarted = true;
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
}

void loop() {
  if (s_syncPending) {
    s_syncPending = false;
    if (s_rtc && (s_lastRtcWrite == 0 || millis() - s_lastRtcWrite > 60000)) {
      rtcWrite(time(nullptr));
      s_lastRtcWrite = millis();
      Serial.println("[time] NTP sync, DS3231 updated");
    }
  }
}

bool valid() { return time(nullptr) > VALID_AFTER; }
bool rtcPresent() { return s_rtc; }
bool ntpSynced() { return s_ntpSynced; }
time_t now() { return time(nullptr); }

}  // namespace TimeKeeper
