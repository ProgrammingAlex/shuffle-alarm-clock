#include "net.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <mutex>
#include "config.h"
#include "timekeeper.h"

namespace {
  Preferences s_prefs;
  DNSServer s_dns;
  String s_ssid, s_pass;

  bool s_ap = false;
  bool s_forcedSetup = false;
  bool s_mdns = false;
  bool s_everConnected = false;
  uint32_t s_apStartedAt = 0;
  uint32_t s_disconnectedSince = 0;
  uint32_t s_lastRetry = 0;
  uint32_t s_restartAt = 0;

  std::mutex s_scanMutex;
  String s_scanJson = "[]";
  uint32_t s_lastScan = 0;
  bool s_scanning = false;
  volatile bool s_scanRequested = false;

  void beginSta() {
    WiFi.begin(s_ssid.c_str(), s_pass.c_str());
    s_lastRetry = millis();
  }

  void startAp() {
    if (s_ap) return;
    WiFi.mode(s_ssid.length() ? WIFI_AP_STA : WIFI_AP);
    WiFi.softAP(SETUP_AP_NAME);
    delay(100);
    s_dns.setErrorReplyCode(DNSReplyCode::NoError);
    s_dns.start(53, "*", WiFi.softAPIP());  // every name -> us, so the setup page pops up
    s_ap = true;
    s_apStartedAt = millis();
    s_lastScan = 0;
    Serial.printf("[wifi] setup hotspot \"%s\" on %s\n", SETUP_AP_NAME, WiFi.softAPIP().toString().c_str());
  }

  void stopAp() {
    if (!s_ap) return;
    s_dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    s_ap = false;
    s_forcedSetup = false;
    Serial.println("[wifi] setup hotspot off");
  }

  void handleScan() {
    if (!s_ap) return;
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    if (n >= 0) {
      JsonDocument doc;
      JsonArray arr = doc.to<JsonArray>();
      for (int i = 0; i < n; i++) {
        String name = WiFi.SSID(i);
        if (!name.length()) continue;
        bool dup = false;
        for (JsonObject o : arr) if (name == (const char*)o["ssid"]) { dup = true; break; }
        if (dup) continue;  // results come strongest first
        JsonObject o = arr.add<JsonObject>();
        o["ssid"] = name;
        o["rssi"] = WiFi.RSSI(i);
        o["lock"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      }
      WiFi.scanDelete();
      std::lock_guard<std::mutex> lock(s_scanMutex);
      s_scanJson = "";
      serializeJson(doc, s_scanJson);
      s_scanning = false;
    }
    // Rescan every 20 s while nobody is on the hotspot, or when the page asks
    bool idle = WiFi.softAPgetStationNum() == 0;
    bool due = s_lastScan == 0 || (idle && millis() - s_lastScan > 20000);
    if (!s_scanning && (due || s_scanRequested)) {
      s_scanRequested = false;
      WiFi.scanNetworks(true);
      s_scanning = true;
      s_lastScan = millis();
    }
  }
}

namespace Net {

void begin() {
  s_prefs.begin("wifi", false);
  if (s_prefs.isKey("ssid")) {
    s_ssid = s_prefs.getString("ssid", "");
    s_pass = s_prefs.getString("pass", "");
  } else {
    s_ssid = DEFAULT_WIFI_SSID;
    s_pass = DEFAULT_WIFI_PASS;
  }

  WiFi.persistent(false);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // keeps the web page snappy; it's wall powered
  WiFi.setAutoReconnect(true);
  s_disconnectedSince = millis();

  if (s_ssid.length()) {
    Serial.printf("[wifi] connecting to \"%s\"\n", s_ssid.c_str());
    beginSta();
  } else {
    startAp();
  }
}

void loop() {
  if (s_restartAt && (int32_t)(millis() - s_restartAt) > 0) ESP.restart();
  if (s_ap) s_dns.processNextRequest();

  if (WiFi.status() == WL_CONNECTED) {
    s_disconnectedSince = millis();
    if (!s_everConnected) {
      s_everConnected = true;
      Serial.printf("[wifi] connected, IP %s\n", WiFi.localIP().toString().c_str());
      TimeKeeper::startNtp();
    }
    if (!s_mdns) {
      s_mdns = MDNS.begin(DEVICE_HOSTNAME);
      if (s_mdns) MDNS.addService("http", "tcp", 80);
    }
    // Close the hotspot once we're online (unless someone opened it on purpose)
    if (s_ap && WiFi.softAPgetStationNum() == 0) {
      if (!s_forcedSetup || millis() - s_apStartedAt > 10UL * 60 * 1000) stopAp();
    }
  } else if (s_ssid.length()) {
    if (!s_ap && millis() - s_disconnectedSince > 30000) startAp();
    // Retry every 30 s, but not while a phone is on the hotspot (retrying
    // hops Wi-Fi channels and would kick the phone off the setup page)
    bool phoneOnHotspot = s_ap && WiFi.softAPgetStationNum() > 0;
    if (!s_scanning && !phoneOnHotspot && millis() - s_lastRetry > 30000) beginSta();
  }

  handleScan();
}

bool connected() { return WiFi.status() == WL_CONNECTED; }
bool setupActive() { return s_ap; }
bool hasSavedNetwork() { return s_ssid.length() > 0; }
String ssid() { return s_ssid; }
String ip() { return connected() ? WiFi.localIP().toString() : String(""); }

void startSetup() {
  s_forcedSetup = true;
  startAp();
}

void saveNetwork(const String& ssid, const String& pass) {
  s_prefs.putString("ssid", ssid);
  s_prefs.putString("pass", pass);
  s_restartAt = millis() + 2500;  // give the browser time to get the reply
}

void forgetNetwork() {
  s_prefs.putString("ssid", "");
  s_prefs.putString("pass", "");
  s_restartAt = millis() + 2500;
}

void requestScan() { s_scanRequested = true; }

String scanResultsJson() {
  std::lock_guard<std::mutex> lock(s_scanMutex);
  return s_scanJson;
}

bool isSetupRequest(const IPAddress& localIp) {
  return s_ap && localIp == WiFi.softAPIP();
}

}  // namespace Net
