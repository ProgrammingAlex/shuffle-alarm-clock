#include "webui.h"
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <Update.h>
#include <WiFi.h>
#include "config.h"
#include "app.h"
#include "icon.h"
#include "net.h"
#include "player.h"
#include "storage.h"
#include "timekeeper.h"
#include "web_page.h"

namespace {

AsyncWebServer s_server(80);
volatile uint32_t s_restartAt = 0;

struct UploadState {
  bool failed;
  char error[72];
  char name[112];
  char part[40];
};

// Wall-clock time as if it were UTC, so the page can show the clock's own
// local time no matter what timezone the phone is in.
int64_t localEpoch(time_t t) {
  if (t <= 0) return 0;
  struct tm lt;
  localtime_r(&t, &lt);
  int y = lt.tm_year + 1900;
  unsigned m = lt.tm_mon + 1, d = lt.tm_mday;
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int64_t days = (int64_t)era * 146097 + (int64_t)doe - 719468;
  return days * 86400 + lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
}

void sendJson(AsyncWebServerRequest* r, JsonDocument& doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  r->send(code, "application/json", out);
}

void sendOk(AsyncWebServerRequest* r) { r->send(200, "application/json", "{\"ok\":true}"); }

void sendError(AsyncWebServerRequest* r, const String& msg, int code = 400) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = msg;
  sendJson(r, doc, code);
}

bool fromSetupHotspot(AsyncWebServerRequest* r) {
  return r->client() && Net::isSetupRequest(r->client()->localIP());
}

const char* stateName(AppState s) {
  switch (s) {
    case AppState::Ringing: return "ringing";
    case AppState::Snoozed: return "snoozed";
    case AppState::Preview: return "preview";
    default: return "idle";
  }
}

void sendAlarms(AsyncWebServerRequest* r) {
  JsonDocument doc;
  doc["ok"] = true;
  Storage::alarmsToJson(doc["alarms"].to<JsonArray>());
  sendJson(r, doc);
}

void sendSettings(AsyncWebServerRequest* r) {
  JsonDocument doc;
  doc["ok"] = true;
  Storage::settingsToJson(doc["settings"].to<JsonObject>());
  sendJson(r, doc);
}

void addJsonPost(const char* path, std::function<void(AsyncWebServerRequest*, JsonVariant&)> fn) {
  auto* h = new AsyncCallbackJsonWebHandler(path, [fn](AsyncWebServerRequest* r, JsonVariant& json) { fn(r, json); });
  h->setMethod(HTTP_POST);
  h->setMaxContentLength(8192);
  s_server.addHandler(h);
}

// ---------- song upload (streams straight to the SD card) ----------
void uploadChunk(AsyncWebServerRequest* r, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
  auto* st = static_cast<UploadState*>(r->_tempObject);
  auto fail = [&](const char* msg) {
    if (!st) return;
    st->failed = true;
    strlcpy(st->error, msg, sizeof(st->error));
    if (r->_tempFile) r->_tempFile.close();
    SD.remove(st->part);
  };

  if (index == 0) {
    if (!st) {
      st = static_cast<UploadState*>(calloc(1, sizeof(UploadState)));
      if (!st) return;
      r->_tempObject = st;
    }
    String safe = Player::sanitizeName(filename);
    strlcpy(st->name, safe.c_str(), sizeof(st->name));
    snprintf(st->part, sizeof(st->part), "%s/.up%08lx", MUSIC_DIR, (unsigned long)esp_random());

    String lower = filename;
    lower.toLowerCase();
    uint64_t freeBytes = Player::sdTotalBytes() - Player::sdUsedBytes();
    if (!Player::sdOk()) fail("No SD card in the clock");
    else if (!lower.endsWith(".mp3")) fail("Only MP3 files can be added");
    else if (r->contentLength() + 65536 > freeBytes) fail("The SD card is full");
    else {
      r->_tempFile = SD.open(st->part, FILE_WRITE);
      if (!r->_tempFile) fail("Couldn't write to the SD card");
    }
  }

  if (!st || st->failed || !r->_tempFile) return;
  if (len && r->_tempFile.write(data, len) != len) {
    fail("Writing failed (card full or removed?)");
    return;
  }
  if (final) {
    r->_tempFile.close();
    String dest = String(MUSIC_DIR) + "/" + st->name;
    if (SD.exists(dest)) SD.remove(dest);
    if (!SD.rename(st->part, dest)) {
      fail("Couldn't save the file");
      return;
    }
    Player::rescan();
    Serial.printf("[web] uploaded %s\n", st->name);
  }
}

void uploadDone(AsyncWebServerRequest* r) {
  auto* st = static_cast<UploadState*>(r->_tempObject);
  if (!st) return sendError(r, "No file received");
  if (st->failed) return sendError(r, st->error);
  JsonDocument doc;
  doc["ok"] = true;
  doc["name"] = st->name;
  sendJson(r, doc);
}

// ---------- firmware update ----------
void firmwareChunk(AsyncWebServerRequest* r, const String&, size_t index, uint8_t* data, size_t len, bool final) {
  if (index == 0) {
    Serial.println("[web] firmware update started");
    App::post(AppAction::StopPreview);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) Update.printError(Serial);
  }
  if (!Update.hasError() && len && Update.write(data, len) != len) Update.printError(Serial);
  if (final && !Update.end(true)) Update.printError(Serial);
}

void firmwareDone(AsyncWebServerRequest* r) {
  if (Update.hasError() || !Update.isFinished()) {
    return sendError(r, String("Update failed: ") + Update.errorString());
  }
  sendOk(r);
  s_restartAt = millis() + 1500;
}

void setupRoutes() {
  // Pages
  s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (fromSetupHotspot(r)) r->send(200, "text/html", (const uint8_t*)SETUP_HTML, strlen(SETUP_HTML));
    else r->send(200, "text/html", (const uint8_t*)INDEX_HTML, strlen(INDEX_HTML));
  });
  s_server.on("/setup", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send(200, "text/html", (const uint8_t*)SETUP_HTML, strlen(SETUP_HTML));
  });
  s_server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send(200, "application/manifest+json", (const uint8_t*)MANIFEST_JSON, strlen(MANIFEST_JSON));
  });
  auto sendIcon = [](AsyncWebServerRequest* r) {
    AsyncWebServerResponse* res = r->beginResponse(200, "image/png", ICON_PNG, ICON_PNG_LEN);
    res->addHeader("Cache-Control", "max-age=604800");
    r->send(res);
  };
  s_server.on("/icon.png", HTTP_GET, sendIcon);
  s_server.on("/apple-touch-icon.png", HTTP_GET, sendIcon);
  s_server.on("/apple-touch-icon-precomposed.png", HTTP_GET, sendIcon);

  // Status
  s_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r) {
    time_t now = TimeKeeper::now();
    bool valid = TimeKeeper::valid();
    JsonDocument doc;
    doc["ok"] = true;
    doc["valid"] = valid;
    doc["local"] = valid ? localEpoch(now) : 0;
    doc["state"] = stateName(App::state());
    doc["label"] = App::ringingLabel();
    doc["snoozeLocal"] = localEpoch(App::snoozeUntil());
    doc["nowPlaying"] = Player::nowPlaying();
    doc["nextLocal"] = valid ? localEpoch(Storage::nextAlarm(now)) : 0;
    {
      std::lock_guard<std::recursive_mutex> lock(Storage::mutex());
      doc["use24h"] = Storage::settings().use24h;
    }
    doc["ssid"] = Net::ssid();
    doc["ip"] = Net::ip();
    doc["rssi"] = Net::connected() ? WiFi.RSSI() : 0;
    doc["sdOk"] = Player::sdOk();
    doc["songCount"] = Player::songCount();
    doc["rtc"] = TimeKeeper::rtcPresent();
    doc["ntp"] = TimeKeeper::ntpSynced();
    doc["version"] = FIRMWARE_VERSION;
    sendJson(r, doc);
  });

  // Alarms
  s_server.on("/api/alarms", HTTP_GET, sendAlarms);
  addJsonPost("/api/alarms", [](AsyncWebServerRequest* r, JsonVariant& json) {
    String err;
    if (!Storage::alarmsFromJson(json.as<JsonArrayConst>(), err)) return sendError(r, err);
    Storage::saveAlarms();
    App::post(AppAction::AlarmsChanged);
    sendAlarms(r);
  });

  // Settings
  s_server.on("/api/settings", HTTP_GET, sendSettings);
  addJsonPost("/api/settings", [](AsyncWebServerRequest* r, JsonVariant& json) {
    Storage::settingsFromJson(json.as<JsonObjectConst>());
    Storage::saveSettings();
    sendSettings(r);
  });

  // Songs
  s_server.on("/api/songs", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument doc;
    doc["ok"] = true;
    doc["sdOk"] = Player::sdOk();
    doc["total"] = Player::sdTotalBytes();
    doc["used"] = Player::sdUsedBytes();
    JsonArray arr = doc["songs"].to<JsonArray>();
    for (const SongInfo& s : Player::songs()) {
      JsonObject o = arr.add<JsonObject>();
      o["name"] = s.name;
      o["size"] = s.size;
    }
    sendJson(r, doc);
  });
  s_server.on("/api/upload", HTTP_POST, uploadDone, uploadChunk);
  addJsonPost("/api/song-delete", [](AsyncWebServerRequest* r, JsonVariant& json) {
    String name = json["name"] | "";
    if (!name.length() || !Player::deleteSong(name)) return sendError(r, "Couldn't delete that song");
    sendOk(r);
  });
  addJsonPost("/api/preview", [](AsyncWebServerRequest* r, JsonVariant& json) {
    if (App::state() == AppState::Ringing || App::state() == AppState::Snoozed)
      return sendError(r, "Turn the alarm off first");
    App::post(AppAction::Preview, json["name"] | "");
    sendOk(r);
  });
  addJsonPost("/api/preview-stop", [](AsyncWebServerRequest* r, JsonVariant&) {
    App::post(AppAction::StopPreview);
    sendOk(r);
  });

  // Alarm control
  addJsonPost("/api/snooze", [](AsyncWebServerRequest* r, JsonVariant&) {
    App::post(AppAction::Snooze);
    sendOk(r);
  });
  addJsonPost("/api/stop", [](AsyncWebServerRequest* r, JsonVariant&) {
    App::post(AppAction::StopAlarm);
    sendOk(r);
  });
  addJsonPost("/api/test-alarm", [](AsyncWebServerRequest* r, JsonVariant&) {
    App::post(AppAction::TestAlarm);
    sendOk(r);
  });

  // Wi-Fi
  s_server.on("/api/wifi-scan", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (r->hasParam("refresh")) Net::requestScan();
    r->send(200, "application/json", Net::scanResultsJson());
  });
  addJsonPost("/api/wifi-save", [](AsyncWebServerRequest* r, JsonVariant& json) {
    String ssid = json["ssid"] | "";
    String pass = json["pass"] | "";
    ssid.trim();
    if (!ssid.length() || ssid.length() > 32) return sendError(r, "Enter the network name");
    if (pass.length() > 64 || (pass.length() > 0 && pass.length() < 8))
      return sendError(r, "Wi-Fi passwords are 8 to 64 characters");
    Net::saveNetwork(ssid, pass);
    sendOk(r);
  });
  addJsonPost("/api/wifi-forget", [](AsyncWebServerRequest* r, JsonVariant&) {
    Net::forgetNetwork();
    sendOk(r);
  });

  // Firmware
  s_server.on("/api/update", HTTP_POST, firmwareDone, firmwareChunk);

  // Anything else: on the setup hotspot, send phones to the setup page
  // (this is what makes the iPhone pop the page up by itself)
  s_server.onNotFound([](AsyncWebServerRequest* r) {
    if (fromSetupHotspot(r)) {
      r->redirect("http://" + WiFi.softAPIP().toString() + "/");
      return;
    }
    r->send(404, "text/plain", "Not found");
  });
}

}  // namespace

namespace WebUI {

void begin() {
  setupRoutes();
  s_server.begin();
}

void loop() {
  if (s_restartAt && (int32_t)(millis() - s_restartAt) > 0) ESP.restart();
}

}  // namespace WebUI
