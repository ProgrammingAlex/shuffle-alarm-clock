#include "updater.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <mutex>
#include "config.h"
#include "app.h"
#include "net.h"
#include "storage.h"
#include "timekeeper.h"

// Tell the ESP32 core not to accept new firmware automatically at boot:
// this file confirms it once the new firmware has shown it can get online.
extern "C" bool verifyRollbackLater() { return true; }

namespace {
  std::mutex s_mutex;
  String s_status = "Not checked yet";
  String s_latest;
  volatile bool s_busy = false;
  volatile bool s_requested = false;
  volatile int s_progress = -1;
  bool s_pendingVerify = false;
  int s_lastNightlyDay = -1;
  Preferences s_prefs;

  void setStatus(const String& st) {
    {
      std::lock_guard<std::mutex> lock(s_mutex);
      s_status = st;
    }
    Serial.printf("[update] %s\n", st.c_str());
  }

  String clean(String v) {
    v.trim();
    if (v.startsWith("v") || v.startsWith("V")) v = v.substring(1);
    return v;
  }

  // "v1.2.3" -> 1002003, so versions compare as numbers
  uint32_t versionNumber(const String& version) {
    String v = clean(version);
    uint32_t parts[3] = {0, 0, 0};
    int idx = 0;
    for (size_t i = 0; i < v.length() && idx < 3; i++) {
      char c = v[i];
      if (c == '.') idx++;
      else if (isdigit((unsigned char)c)) parts[idx] = parts[idx] * 10 + (c - '0');
      else break;
    }
    return parts[0] * 1000000UL + parts[1] * 1000UL + parts[2];
  }

  bool safeToInstall(String& why) {
    if (App::state() == AppState::Ringing || App::state() == AppState::Snoozed) {
      why = "an alarm is going off";
      return false;
    }
    if (TimeKeeper::valid()) {
      time_t now = TimeKeeper::now();
      time_t next = Storage::nextAlarm(now);
      if (next && next - now < 60 * 60) {
        why = "an alarm is due within the hour";
        return false;
      }
    }
    return true;
  }

  // Each secure connection needs about 40 KB of the ESP32's internal memory.
  // These helpers close their connection completely before returning, so
  // only one is ever open at a time. Before, the release-info connection
  // stayed open during the download, which can run the ESP32 out of memory
  // for the second connection ("connection refused").
  int fetchReleaseInfo(String& body) {
    NetworkClientSecure client;
    client.useBuiltinCACertBundle();
    HTTPClient http;
    http.setReuse(false);
    String url = String("https://api.github.com/repos/") + GITHUB_REPO + "/releases/latest";
    if (!http.begin(client, url)) return HTTPC_ERROR_CONNECTION_REFUSED;
    http.setUserAgent("shuffle-alarm-clock/" FIRMWARE_VERSION);
    http.addHeader("Accept", "application/vnd.github+json");
    http.setTimeout(15000);
    int code = http.GET();
    if (code == 200) body = http.getString();
    http.end();
    client.stop();
    return code;
  }

  // Returns where a GitHub download link redirects to (or the link itself).
  String resolveRedirect(const String& url) {
    NetworkClientSecure client;
    client.useBuiltinCACertBundle();
    HTTPClient http;
    http.setReuse(false);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    String result = url;
    if (http.begin(client, url)) {
      const char* keys[] = {"Location"};
      http.collectHeaders(keys, 1);
      http.setUserAgent("shuffle-alarm-clock/" FIRMWARE_VERSION);
      http.setTimeout(15000);
      int code = http.sendRequest("HEAD");
      String loc = http.header("Location");
      if (code >= 300 && code < 400 && loc.startsWith("https://")) result = loc;
      http.end();
    }
    client.stop();
    return result;
  }

  void runCheck() {
    if (!Net::connected()) {
      setStatus("No Wi-Fi, couldn't check for updates");
      return;
    }
    setStatus("Checking for updates...");

    String body;
    int code = fetchReleaseInfo(body);
    if (code != 200) {
      if (code == 404) setStatus("No releases published yet");
      else if (code < 0) setStatus("Couldn't reach GitHub (" + HTTPClient::errorToString(code) + ")");
      else setStatus("GitHub replied with error " + String(code));
      return;
    }

    JsonDocument filter;
    filter["tag_name"] = true;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["browser_download_url"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) {
      setStatus("Couldn't read the release info");
      return;
    }
    String tag = clean(doc["tag_name"] | "");
    String binUrl;
    for (JsonObject a : doc["assets"].as<JsonArray>()) {
      if (String((const char*)(a["name"] | "")) == FIRMWARE_ASSET) binUrl = (const char*)(a["browser_download_url"] | "");
    }
    {
      std::lock_guard<std::mutex> lock(s_mutex);
      s_latest = tag;
    }
    if (!tag.length() || !binUrl.length()) {
      setStatus("The latest release has no " FIRMWARE_ASSET);
      return;
    }
    if (versionNumber(tag) <= versionNumber(FIRMWARE_VERSION)) {
      setStatus("Up to date (v" FIRMWARE_VERSION ")");
      return;
    }
    if (clean(s_prefs.getString("badver", "")) == tag) {
      setStatus("v" + tag + " failed to start before and was rolled back, skipping it");
      return;
    }
    String why;
    if (!safeToInstall(why)) {
      setStatus("v" + tag + " is available, waiting because " + why);
      return;
    }

    setStatus("Installing v" + tag + "...");
    App::post(AppAction::StopPreview);
    s_prefs.putString("trying", tag);

    // GitHub answers the download link with a redirect to its file server.
    // Look that up first, so the download itself is one plain connection.
    String fileUrl = resolveRedirect(binUrl);
    Serial.printf("[update] downloading from %s\n", fileUrl.substring(0, 60).c_str());

    NetworkClientSecure dl;
    dl.useBuiltinCACertBundle();
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // just in case there's another hop
    httpUpdate.rebootOnUpdate(false);
    httpUpdate.onProgress([](int cur, int total) {
      if (total > 0) s_progress = (int)((int64_t)cur * 100 / total);
    });
    s_progress = 0;
    t_httpUpdate_return result = httpUpdate.update(dl, fileUrl, FIRMWARE_VERSION);
    if (result == HTTP_UPDATE_OK) {
      s_progress = 100;
      setStatus("Installed v" + tag + ", restarting");
      delay(1500);
      ESP.restart();
    }
    s_progress = -1;
    s_prefs.remove("trying");
    // Add the secure-connection error and free memory, so a failure says why
    char tlsErr[100] = "";
    dl.lastError(tlsErr, sizeof(tlsErr));
    dl.stop();
    String msg = "Update failed: " + httpUpdate.getLastErrorString();
    if (tlsErr[0]) msg += String(" (") + tlsErr + ")";
    msg += " [mem " + String(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024) + "k]";
    setStatus(msg);
  }

  void updateTask(void*) {
    runCheck();
    s_busy = false;
    vTaskDelete(nullptr);
  }

  void startTask() {
    if (s_busy) return;
    s_busy = true;
    if (xTaskCreatePinnedToCore(updateTask, "update", 16384, nullptr, 1, nullptr, 0) != pdPASS) s_busy = false;
  }
}

namespace Updater {

void begin() {
  s_prefs.begin("update", false);

  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
    s_pendingVerify = true;
    Serial.println("[update] running new firmware, will keep it once it's back on Wi-Fi");
  }

  // An update that crashed and was rolled back by the bootloader: don't retry it
  String trying = s_prefs.getString("trying", "");
  if (trying.length() && clean(trying) != clean(FIRMWARE_VERSION)) {
    if (esp_ota_get_last_invalid_partition() != nullptr) {
      s_prefs.putString("badver", trying);
      setStatus("v" + trying + " didn't start properly and was rolled back");
    }
    s_prefs.remove("trying");
  }
}

void loop() {
  // Confirm new firmware once it has been running for a minute and is online
  if (s_pendingVerify) {
    if (Net::connected() && millis() > 60000) {
      esp_ota_mark_app_valid_cancel_rollback();
      s_pendingVerify = false;
      s_prefs.remove("trying");
      setStatus("Updated to v" FIRMWARE_VERSION);
    } else if (millis() > 10UL * 60 * 1000) {
      // Never got online: go back to the old version. Clear "trying" so the old
      // version doesn't blacklist this one (the router may just have been off).
      Serial.println("[update] new firmware never got online, rolling back");
      s_prefs.remove("trying");
      delay(100);
      esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    return;
  }

  if (s_requested && !s_busy) {
    s_requested = false;
    startTask();
  }

  // Nightly check
  if (!s_busy && TimeKeeper::valid()) {
    time_t now = TimeKeeper::now();
    struct tm lt;
    localtime_r(&now, &lt);
    if (lt.tm_hour == UPDATE_HOUR && lt.tm_min == UPDATE_MINUTE && lt.tm_yday != s_lastNightlyDay) {
      s_lastNightlyDay = lt.tm_yday;
      startTask();
    }
  }
}

void requestCheck() { s_requested = true; }
bool busy() { return s_busy || s_requested; }
int progress() { return s_progress; }

String status() {
  std::lock_guard<std::mutex> lock(s_mutex);
  return s_status;
}

String latestVersion() {
  std::lock_guard<std::mutex> lock(s_mutex);
  return s_latest;
}

}  // namespace Updater
