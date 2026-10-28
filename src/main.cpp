// =====================================================================
//  Shuffle Alarm Clock
//  ESP32-S3 N16R8 + MAX98357A + microSD + ST7789V 2" LCD + DS3231
//
//  Buttons
//    Ringing:  SNOOZE = snooze,  OFF = turn alarm off
//    Snoozed:  OFF = cancel the snooze
//    Idle:     SNOOZE = show the QR code for the web page
//              OFF    = screen off / on
//    Hold both for 5 seconds = open the Wi-Fi setup hotspot
// =====================================================================
#include <Arduino.h>
#include <mutex>
#include "config.h"
#include "app.h"
#include "display.h"
#include "net.h"
#include "player.h"
#include "storage.h"
#include "timekeeper.h"
#include "webui.h"

namespace {

// ---------- state shared with the web server ----------
struct Msg {
  AppAction action;
  char arg[112];
};
QueueHandle_t s_queue;
std::mutex s_appMutex;
volatile AppState s_state = AppState::Idle;
String s_label;
volatile time_t s_snoozeUntil = 0;

// ---------- ringing ----------
uint32_t s_ringStartMs = 0;
uint32_t s_trackStartMs = 0;
uint32_t s_lastVolMs = 0;
uint8_t s_quickFails = 0;
uint8_t s_currentVol = 0;
bool s_beepMode = false;

// ---------- screen ----------
uint32_t s_infoUntil = 0;
bool s_screenOff = false;
uint32_t s_lastDraw = 0;
long s_lastMinute = -1;

// ---------- buttons ----------
struct Button {
  uint8_t pin;
  bool raw = false, down = false, pressed = false, released = false, partOfCombo = false;
  uint32_t changedAt = 0, downAt = 0;

  void update() {
    pressed = released = false;
    bool r = digitalRead(pin) == LOW;
    if (r != raw) { raw = r; changedAt = millis(); }
    if (millis() - changedAt > 30 && raw != down) {
      down = raw;
      if (down) { pressed = true; downAt = millis(); }
      else released = true;
    }
  }
};
Button s_snoozeBtn{PIN_BTN_SNOOZE};
Button s_offBtn{PIN_BTN_OFF};
bool s_comboFired = false;

void setState(AppState st, const String& label = "") {
  std::lock_guard<std::mutex> lock(s_appMutex);
  s_state = st;
  s_label = label;
}

Settings settingsCopy() {
  std::lock_guard<std::recursive_mutex> lock(Storage::mutex());
  return Storage::settings();
}

uint8_t rampVolume(const Settings& s) {
  uint8_t maxV = s.volume;
  if (s.rampSec == 0) return maxV;
  uint8_t minV = max(3, maxV * 15 / 100);
  uint32_t elapsed = millis() - s_ringStartMs;
  uint32_t ramp = s.rampSec * 1000UL;
  if (elapsed >= ramp) return maxV;
  return minV + (uint32_t)(maxV - minV) * elapsed / ramp;
}

void startRinging(const String& label) {
  Settings s = settingsCopy();
  bool wasSnoozed = s_state == AppState::Snoozed;
  setState(AppState::Ringing, label);
  s_snoozeUntil = 0;
  s_ringStartMs = millis();
  s_quickFails = 0;
  s_currentVol = rampVolume(s);
  String song = Player::playNextShuffled(s_currentVol);
  s_beepMode = song == "Beep";
  s_trackStartMs = millis();
  s_screenOff = false;
  s_infoUntil = 0;
  Serial.printf("[alarm] %s \"%s\" -> %s\n", wasSnoozed ? "snooze over" : "ringing", label.c_str(), song.c_str());
}

void stopEverything() {
  Player::stop();
  s_snoozeUntil = 0;
  setState(AppState::Idle);
}

void snooze() {
  if (s_state != AppState::Ringing) return;
  Settings s = settingsCopy();
  Player::stop();
  String label;
  {
    std::lock_guard<std::mutex> lock(s_appMutex);
    label = s_label;
  }
  s_snoozeUntil = TimeKeeper::now() + s.snoozeMin * 60;
  setState(AppState::Snoozed, label);
  Serial.printf("[alarm] snoozed for %d min\n", s.snoozeMin);
}

void updateRinging() {
  Settings s = settingsCopy();
  if (millis() - s_lastVolMs > 150) {
    s_lastVolMs = millis();
    uint8_t v = rampVolume(s);
    if (v != s_currentVol) {
      s_currentVol = v;
      Player::setVolume(v);
    }
  }

  if (Player::takeTrackEnded()) {
    // A song that "ends" within 3 s is probably broken; after 3 in a row, fall back to the beep
    bool quick = millis() - s_trackStartMs < 3000;
    s_quickFails = (quick && !s_beepMode) ? s_quickFails + 1 : 0;
    if (s_beepMode || s_quickFails >= 3) {
      Player::playBeep(s_currentVol);
      s_beepMode = true;
    } else {
      s_beepMode = Player::playNextShuffled(s_currentVol) == "Beep";
    }
    s_trackStartMs = millis();
  }

  if (millis() - s_ringStartMs > RING_TIMEOUT_MIN * 60000UL) {
    Serial.println("[alarm] nobody answered, stopping");
    stopEverything();
  }
}

void startPreview(const String& name) {
  if (s_state == AppState::Ringing || s_state == AppState::Snoozed) return;
  Settings s = settingsCopy();
  bool ok = name.length() ? Player::playSong(name, s.volume) : false;
  if (!ok) Player::playBeep(s.volume);
  setState(AppState::Preview);
}

void checkAlarms() {
  if (!TimeKeeper::valid()) return;
  time_t now = TimeKeeper::now();
  long minute = now / 60;
  if (minute == s_lastMinute) return;
  s_lastMinute = minute;

  struct tm lt;
  localtime_r(&now, &lt);
  String label;
  bool fire = false;
  {
    std::lock_guard<std::recursive_mutex> lock(Storage::mutex());
    auto& alarms = Storage::alarms();
    for (Alarm& a : alarms) {
      if (!a.enabled || a.hour != lt.tm_hour || a.minute != lt.tm_min) continue;
      if (a.days != 0 && !(a.days & (1 << lt.tm_wday))) continue;
      if (!fire) {
        fire = true;
        label = a.label;
      }
      if (a.days == 0) a.enabled = false;  // one-time alarm
    }
    if (fire) Storage::saveAlarms();
  }
  if (!fire || s_state == AppState::Ringing) return;
  if (s_state == AppState::Preview) Player::stop();
  startRinging(label);
}

void handleMessages() {
  Msg m;
  while (xQueueReceive(s_queue, &m, 0) == pdTRUE) {
    switch (m.action) {
      case AppAction::Snooze: snooze(); break;
      case AppAction::StopAlarm:
        if (s_state == AppState::Ringing || s_state == AppState::Snoozed) stopEverything();
        break;
      case AppAction::Preview: startPreview(m.arg); break;
      case AppAction::StopPreview:
        if (s_state == AppState::Preview) stopEverything();
        break;
      case AppAction::TestAlarm:
        if (s_state == AppState::Preview) Player::stop();
        startRinging("Test alarm");
        break;
      case AppAction::AlarmsChanged: break;  // screen picks up the new "next alarm" on its own
    }
  }
}

void handleButtons() {
  s_snoozeBtn.update();
  s_offBtn.update();

  // Both held for 5 s -> Wi-Fi setup
  if (s_snoozeBtn.down && s_offBtn.down) {
    s_snoozeBtn.partOfCombo = s_offBtn.partOfCombo = true;
    uint32_t since = max(s_snoozeBtn.downAt, s_offBtn.downAt);
    if (!s_comboFired && millis() - since > 5000) {
      s_comboFired = true;
      s_screenOff = false;
      s_infoUntil = 0;
      Net::startSetup();
    }
  }

  AppState st = s_state;
  // While ringing, react the moment a button goes down
  if (st == AppState::Ringing) {
    if (s_offBtn.pressed) stopEverything();
    else if (s_snoozeBtn.pressed) snooze();
  } else if (st == AppState::Snoozed) {
    if (s_offBtn.pressed) stopEverything();
  } else if (st == AppState::Preview) {
    if (s_offBtn.pressed || s_snoozeBtn.pressed) stopEverything();
  } else {
    // Idle actions happen on release, so holding both for setup doesn't trigger them
    if (s_snoozeBtn.released && !s_snoozeBtn.partOfCombo) {
      if (s_screenOff) s_screenOff = false;
      else s_infoUntil = s_infoUntil ? 0 : millis() + 60000;
    }
    if (s_offBtn.released && !s_offBtn.partOfCombo) {
      s_screenOff = !s_screenOff;
      s_infoUntil = 0;
    }
  }

  if (!s_snoozeBtn.down && !s_offBtn.down) {
    s_snoozeBtn.partOfCombo = s_offBtn.partOfCombo = false;
    s_comboFired = false;
  }
}

bool isNight(const Settings& s, int hour) {
  if (s.nightStart == s.nightEnd) return false;
  if (s.nightStart > s.nightEnd) return hour >= s.nightStart || hour < s.nightEnd;
  return hour >= s.nightStart && hour < s.nightEnd;
}

void drawScreen() {
  if (millis() - s_lastDraw < 100) return;
  s_lastDraw = millis();
  if (s_infoUntil && (int32_t)(millis() - s_infoUntil) > 0) s_infoUntil = 0;

  Settings s = settingsCopy();
  ScreenInfo info;
  info.timeValid = TimeKeeper::valid();
  info.now = TimeKeeper::now();
  info.use24h = s.use24h;
  info.nextAlarm = info.timeValid ? Storage::nextAlarm(info.now) : 0;
  info.snoozeUntil = s_snoozeUntil;
  info.song = Player::nowPlaying();
  info.wifiConnected = Net::connected();
  info.wifiName = Net::ssid();
  info.ip = Net::ip();
  info.sdOk = Player::sdOk();
  info.songCount = Player::songCount();
  info.previewing = s_state == AppState::Preview;
  {
    std::lock_guard<std::mutex> lock(s_appMutex);
    info.alarmLabel = s_label;
  }

  AppState st = s_state;
  if (st == AppState::Ringing) info.mode = ScreenInfo::RINGING;
  else if (st == AppState::Snoozed) info.mode = ScreenInfo::SNOOZED;
  else if (s_infoUntil) info.mode = ScreenInfo::INFO;
  else if (Net::setupActive()) info.mode = ScreenInfo::WIFI_SETUP;
  else info.mode = ScreenInfo::NORMAL;

  // In setup mode, only show "can't reach X" when there is a saved network
  if (info.mode == ScreenInfo::WIFI_SETUP && (info.wifiConnected || !Net::hasSavedNetwork())) info.wifiName = "";

  struct tm lt;
  localtime_r(&info.now, &lt);
  bool night = info.timeValid && isNight(s, lt.tm_hour);
  if (st == AppState::Ringing) info.brightness = 255;
  else if (s_screenOff) info.brightness = 0;
  else if (info.mode == ScreenInfo::INFO || info.mode == ScreenInfo::WIFI_SETUP) info.brightness = 130;  // not too bright, so phone cameras don't wash out the QR code
  else info.brightness = night ? s.nightBright : s.dayBright;

  Display::update(info);
}

}  // namespace

// ---------- App interface used by the web server ----------
namespace App {
void post(AppAction action, const String& arg) {
  Msg m{};
  m.action = action;
  strlcpy(m.arg, arg.c_str(), sizeof(m.arg));
  xQueueSend(s_queue, &m, pdMS_TO_TICKS(50));
}
AppState state() { return s_state; }
String ringingLabel() {
  std::lock_guard<std::mutex> lock(s_appMutex);
  return s_label;
}
time_t snoozeUntil() { return s_snoozeUntil; }
}  // namespace App

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n[boot] Shuffle Alarm Clock v%s, PSRAM %u KB\n", FIRMWARE_VERSION, (unsigned)(ESP.getPsramSize() / 1024));

  s_queue = xQueueCreate(10, sizeof(Msg));
  pinMode(PIN_BTN_SNOOZE, INPUT_PULLUP);
  pinMode(PIN_BTN_OFF, INPUT_PULLUP);

  Storage::begin();
  Display::begin();
  TimeKeeper::begin();
  Player::begin();
  Net::begin();
  WebUI::begin();
}

void loop() {
  Net::loop();
  TimeKeeper::loop();
  Player::loop();
  WebUI::loop();
  handleMessages();
  handleButtons();
  checkAlarms();

  if (s_state == AppState::Ringing) {
    updateRinging();
  } else if (s_state == AppState::Snoozed) {
    if (TimeKeeper::now() >= s_snoozeUntil) {
      String label;
      {
        std::lock_guard<std::mutex> lock(s_appMutex);
        label = s_label;
      }
      startRinging(label);
    }
  } else if (s_state == AppState::Preview) {
    if (Player::takeTrackEnded()) stopEverything();
  }

  drawScreen();
  delay(5);
}
