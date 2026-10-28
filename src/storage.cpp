#include "storage.h"
#include <Preferences.h>
#include "config.h"

namespace {
  std::recursive_mutex s_mutex;
  std::vector<Alarm> s_alarms;
  Settings s_settings;
  Preferences s_prefs;

  uint8_t clampU8(int v, int lo, int hi) { return (uint8_t)constrain(v, lo, hi); }

  time_t occurrence(time_t now, int dayOffset, uint8_t hour, uint8_t minute, int* wday) {
    struct tm t;
    localtime_r(&now, &t);
    t.tm_mday += dayOffset;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = 0;
    t.tm_isdst = -1;  // let mktime work out daylight saving
    time_t result = mktime(&t);
    if (wday) *wday = t.tm_wday;
    return result;
  }
}

namespace Storage {

std::recursive_mutex& mutex() { return s_mutex; }
std::vector<Alarm>& alarms() { return s_alarms; }
Settings& settings() { return s_settings; }

void begin() {
  std::lock_guard<std::recursive_mutex> lock(s_mutex);
  s_prefs.begin("clock", false);

  String a = s_prefs.getString("alarms", "");
  if (a.length()) {
    JsonDocument doc;
    if (!deserializeJson(doc, a)) {
      String err;
      alarmsFromJson(doc.as<JsonArrayConst>(), err);
    }
  } else {
    // First boot: one weekday alarm, switched off, so the page isn't empty
    Alarm def;
    def.enabled = false;
    def.label = "Wake up";
    s_alarms.push_back(def);
  }

  String s = s_prefs.getString("settings", "");
  if (s.length()) {
    JsonDocument doc;
    if (!deserializeJson(doc, s)) settingsFromJson(doc.as<JsonObjectConst>());
  }
}

void saveAlarms() {
  std::lock_guard<std::recursive_mutex> lock(s_mutex);
  JsonDocument doc;
  alarmsToJson(doc.to<JsonArray>());
  String out;
  serializeJson(doc, out);
  s_prefs.putString("alarms", out);
}

void saveSettings() {
  std::lock_guard<std::recursive_mutex> lock(s_mutex);
  JsonDocument doc;
  settingsToJson(doc.to<JsonObject>());
  String out;
  serializeJson(doc, out);
  s_prefs.putString("settings", out);
}

void alarmsToJson(JsonArray out) {
  std::lock_guard<std::recursive_mutex> lock(s_mutex);
  for (const Alarm& a : s_alarms) {
    JsonObject o = out.add<JsonObject>();
    o["hour"] = a.hour;
    o["minute"] = a.minute;
    o["days"] = a.days;
    o["enabled"] = a.enabled;
    o["label"] = a.label;
  }
}

bool alarmsFromJson(JsonArrayConst in, String& error) {
  if (in.isNull()) { error = "Expected a list of alarms"; return false; }
  if (in.size() > MAX_ALARMS) { error = "Too many alarms (max " + String(MAX_ALARMS) + ")"; return false; }
  std::vector<Alarm> parsed;
  for (JsonObjectConst o : in) {
    Alarm a;
    a.hour = clampU8(o["hour"] | 7, 0, 23);
    a.minute = clampU8(o["minute"] | 0, 0, 59);
    a.days = (o["days"] | 0) & 0x7F;
    a.enabled = o["enabled"] | true;
    a.label = String((const char*)(o["label"] | ""));
    if (a.label.length() > 30) a.label = a.label.substring(0, 30);
    parsed.push_back(a);
  }
  std::lock_guard<std::recursive_mutex> lock(s_mutex);
  s_alarms = parsed;
  return true;
}

void settingsToJson(JsonObject out) {
  std::lock_guard<std::recursive_mutex> lock(s_mutex);
  out["volume"] = s_settings.volume;
  out["rampSec"] = s_settings.rampSec;
  out["snoozeMin"] = s_settings.snoozeMin;
  out["use24h"] = s_settings.use24h;
  out["dayBright"] = s_settings.dayBright;
  out["nightBright"] = s_settings.nightBright;
  out["nightStart"] = s_settings.nightStart;
  out["nightEnd"] = s_settings.nightEnd;
}

void settingsFromJson(JsonObjectConst in) {
  std::lock_guard<std::recursive_mutex> lock(s_mutex);
  Settings& s = s_settings;
  s.volume = clampU8(in["volume"] | s.volume, 5, 100);
  s.rampSec = clampU8(in["rampSec"] | s.rampSec, 0, 120);
  s.snoozeMin = clampU8(in["snoozeMin"] | s.snoozeMin, 1, 30);
  s.use24h = in["use24h"] | s.use24h;
  s.dayBright = clampU8(in["dayBright"] | s.dayBright, 10, 255);
  s.nightBright = clampU8(in["nightBright"] | s.nightBright, 0, 255);
  s.nightStart = clampU8(in["nightStart"] | s.nightStart, 0, 23);
  s.nightEnd = clampU8(in["nightEnd"] | s.nightEnd, 0, 23);
}

time_t nextAlarm(time_t now, int* outIndex) {
  std::lock_guard<std::recursive_mutex> lock(s_mutex);
  time_t best = 0;
  int bestIdx = -1;
  for (size_t i = 0; i < s_alarms.size(); i++) {
    const Alarm& a = s_alarms[i];
    if (!a.enabled) continue;
    for (int d = 0; d <= 7; d++) {
      int wday;
      time_t t = occurrence(now, d, a.hour, a.minute, &wday);
      if (t <= now) continue;
      if (a.days != 0 && !(a.days & (1 << wday))) continue;
      if (best == 0 || t < best) { best = t; bestIdx = (int)i; }
      break;
    }
  }
  if (outIndex) *outIndex = bestIdx;
  return best;
}

}  // namespace Storage
