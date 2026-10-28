#include "display.h"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.h"

namespace {

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

 public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI3_HOST;  // SPI2 is used by the SD card
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = PIN_TFT_SCK;
      cfg.pin_mosi = PIN_TFT_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = PIN_TFT_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = PIN_TFT_CS;
      cfg.pin_rst = PIN_TFT_RST;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;
      cfg.invert = TFT_INVERT;
      cfg.rgb_order = TFT_RGB_ORDER;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = PIN_TFT_BL;
      cfg.invert = false;
      cfg.freq = 12000;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};

LGFX s_lcd;
LGFX_Sprite s_canvas(&s_lcd);
bool s_canvasOk = false;
String s_lastSig;
int s_lastBrightness = -1;

// Colours
constexpr uint32_t C_BG = 0x000000;
constexpr uint32_t C_TIME = 0xF2EEE4;
constexpr uint32_t C_DIM = 0x8C8C8C;
constexpr uint32_t C_ACCENT = 0xFF3B30;   // red theme
constexpr uint32_t C_WARN = 0xFFB020;     // warnings in amber so they stand apart from the red
constexpr uint32_t C_OK = 0x50D080;
constexpr uint32_t C_RING_BG = 0x2E0606;

const char* DAY_NAMES[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* MONTH_NAMES[] = {"January", "February", "March", "April", "May", "June",
                             "July", "August", "September", "October", "November", "December"};

void splitTime(time_t t, bool use24h, String& hm, String& ampm) {
  struct tm lt;
  localtime_r(&t, &lt);
  char buf[8];
  if (use24h) {
    snprintf(buf, sizeof(buf), "%02d:%02d", lt.tm_hour, lt.tm_min);
    ampm = "";
  } else {
    int h = lt.tm_hour % 12;
    if (h == 0) h = 12;
    snprintf(buf, sizeof(buf), "%d:%02d", h, lt.tm_min);
    ampm = lt.tm_hour < 12 ? "AM" : "PM";
  }
  hm = buf;
}

String shortTime(time_t t, bool use24h) {
  String hm, ampm;
  splitTime(t, use24h, hm, ampm);
  return ampm.length() ? hm + " " + ampm : hm;
}

String dateLine(time_t t) {
  struct tm lt;
  localtime_r(&t, &lt);
  return String(DAY_NAMES[lt.tm_wday]) + ", " + MONTH_NAMES[lt.tm_mon] + " " + lt.tm_mday;
}

String alarmLine(time_t next, time_t now, bool use24h) {
  struct tm a, n;
  localtime_r(&next, &a);
  localtime_r(&now, &n);
  String when;
  if (a.tm_yday == n.tm_yday && a.tm_year == n.tm_year) {
    when = "Today";
  } else {
    time_t tomorrow = now + 86400;
    struct tm tm2;
    localtime_r(&tomorrow, &tm2);
    when = (a.tm_yday == tm2.tm_yday) ? "Tomorrow" : String(DAY_NAMES[a.tm_wday]).substring(0, 3);
  }
  return when + " " + shortTime(next, use24h);
}

// Draws a simple bell icon centred on (x, y)
void drawBell(LGFX_Sprite& c, int x, int y, uint32_t color) {
  c.fillCircle(x, y - 3, 6, color);
  c.fillRect(x - 6, y - 3, 13, 7, color);
  c.fillRect(x - 8, y + 3, 17, 3, color);
  c.fillCircle(x, y + 8, 2, color);
}

// Big clock digits, scaled to fit `maxW`, centred at (cx, cy)
void drawBigTime(LGFX_Sprite& c, const ScreenInfo& s, int cx, int cy, int maxW, uint32_t color) {
  String hm = "--:--", ampm;
  if (s.timeValid) splitTime(s.now, s.use24h, hm, ampm);

  c.setFont(&fonts::FreeSansBold12pt7b);
  c.setTextSize(1);
  int ampmW = ampm.length() ? c.textWidth(ampm) + 6 : 0;

  c.setFont(&fonts::Font7);
  c.setTextSize(1);
  int baseW = c.textWidth(hm);
  float scale = min(2.0f, (float)(maxW - ampmW) / baseW);
  c.setTextSize(scale);
  int w = c.textWidth(hm);
  int h = c.fontHeight();
  int left = cx - (w + ampmW) / 2;
  c.setTextColor(color);
  c.setTextDatum(textdatum_t::middle_left);
  c.drawString(hm, left, cy);

  if (ampmW) {
    c.setFont(&fonts::FreeSansBold12pt7b);
    c.setTextSize(1);
    c.setTextDatum(textdatum_t::top_left);
    c.drawString(ampm, left + w + 6, cy - h / 2 + 2);
  }
}

void drawCentered(LGFX_Sprite& c, const String& text, int y, const lgfx::IFont* font, uint32_t color) {
  c.setFont(font);
  c.setTextSize(1);
  c.setTextColor(color);
  c.setTextDatum(textdatum_t::middle_center);
  String t = text;
  while (t.length() > 3 && c.textWidth(t) > 304) t = t.substring(0, t.length() - 4) + "...";
  c.drawString(t, 160, y);
}

void drawLeft(LGFX_Sprite& c, const String& text, int x, int y, const lgfx::IFont* font, uint32_t color, int maxW) {
  c.setFont(font);
  c.setTextSize(1);
  c.setTextColor(color);
  c.setTextDatum(textdatum_t::top_left);
  String t = text;
  while (t.length() > 3 && c.textWidth(t) > maxW) t = t.substring(0, t.length() - 4) + "...";
  c.drawString(t, x, y);
}

void drawStatusRow(LGFX_Sprite& c, const ScreenInfo& s) {
  String warn;
  if (!s.wifiConnected) warn = "No Wi-Fi";
  if (!s.sdOk) warn += String(warn.length() ? "  -  " : "") + "No SD card";
  else if (s.songCount == 0) warn += String(warn.length() ? "  -  " : "") + "No songs (beep)";
  if (warn.length()) drawCentered(c, warn, 14, &fonts::FreeSans9pt7b, C_WARN);
}

void drawNormal(LGFX_Sprite& c, const ScreenInfo& s) {
  drawStatusRow(c, s);
  drawBigTime(c, s, 160, 100, 304, C_TIME);
  drawCentered(c, s.timeValid ? dateLine(s.now) : String("Waiting for the time..."), 168,
               &fonts::FreeSans12pt7b, C_DIM);

  if (s.previewing) {
    drawCentered(c, "Playing: " + s.song, 212, &fonts::FreeSans9pt7b, C_ACCENT);
  } else if (s.nextAlarm && s.timeValid) {
    String line = alarmLine(s.nextAlarm, s.now, s.use24h);
    c.setFont(&fonts::FreeSansBold12pt7b);
    int w = c.textWidth(line) + 26;
    int x = 160 - w / 2;
    drawBell(c, x + 8, 211, C_ACCENT);
    c.setTextColor(C_ACCENT);
    c.setTextDatum(textdatum_t::middle_left);
    c.drawString(line, x + 26, 212);
  } else {
    drawCentered(c, "No alarms on", 212, &fonts::FreeSans12pt7b, C_DIM);
  }
}

void drawRinging(LGFX_Sprite& c, const ScreenInfo& s) {
  c.fillScreen(C_RING_BG);
  drawCentered(c, s.alarmLabel.length() ? s.alarmLabel : String("Wake up!"), 22, &fonts::FreeSansBold18pt7b, C_ACCENT);
  drawBigTime(c, s, 160, 105, 304, C_TIME);
  if (s.song.length()) drawCentered(c, s.song, 168, &fonts::FreeSans12pt7b, C_TIME);
  c.drawFastHLine(20, 196, 280, C_DIM);
  c.setFont(&fonts::FreeSansBold12pt7b);
  c.setTextColor(C_ACCENT);
  c.setTextDatum(textdatum_t::middle_center);
  c.drawString("SNOOZE", 85, 218);
  c.drawString("OFF", 235, 218);
  c.drawFastVLine(160, 204, 28, C_DIM);
}

void drawSnoozed(LGFX_Sprite& c, const ScreenInfo& s) {
  drawStatusRow(c, s);
  drawBigTime(c, s, 160, 100, 304, C_TIME);
  drawCentered(c, "Snoozing until " + shortTime(s.snoozeUntil, s.use24h), 168, &fonts::FreeSansBold12pt7b, C_ACCENT);
  drawCentered(c, "Press OFF to cancel", 212, &fonts::FreeSans9pt7b, C_DIM);
}

// Big QR code on the left: about 22 mm wide on the 2" screen, with a white
// border at least 2 modules wide so phone cameras lock on easily.
void drawQrBox(LGFX_Sprite& c, const String& text) {
  const int bx = 4, by = 4, bs = 212;
  c.fillRoundRect(bx, by, bs, bs, 6, 0xFFFFFF);
  c.qrcode(text.c_str(), bx + 12, by + 12, bs - 24, 1, false);
}

void drawInfo(LGFX_Sprite& c, const ScreenInfo& s) {
  // Link straight to the IP address: works on every phone. (alarmclock.local
  // works on iPhones and computers, but many Android phones can't open it.)
  String url = s.wifiConnected && s.ip.length() ? "http://" + s.ip + "/"
                                                : String("http://") + DEVICE_HOSTNAME + ".local/";
  drawQrBox(c, url);
  int x = 224, w = 92;
  drawLeft(c, "Scan to", x, 8, &fonts::FreeSansBold9pt7b, C_ACCENT, w);
  drawLeft(c, "set alarms", x, 28, &fonts::FreeSansBold9pt7b, C_ACCENT, w);
  drawLeft(c, "& music", x, 48, &fonts::FreeSansBold9pt7b, C_ACCENT, w);
  drawLeft(c, s.wifiConnected ? s.wifiName : String("No Wi-Fi"), x, 90, &fonts::Font2, C_DIM, w);
  drawLeft(c, "v" FIRMWARE_VERSION, x, 112, &fonts::Font2, C_DIM, w);
  drawLeft(c, "SNOOZE:", x, 172, &fonts::Font2, C_DIM, w);
  drawLeft(c, "go back", x, 190, &fonts::Font2, C_DIM, w);
  // full-width line so the whole address always fits
  drawCentered(c, s.wifiConnected ? "Or type  " + s.ip : String("Or type  ") + DEVICE_HOSTNAME + ".local", 229,
               &fonts::FreeSans9pt7b, C_TIME);
}

void drawWifiSetup(LGFX_Sprite& c, const ScreenInfo& s) {
  // Standard Wi-Fi join code: iPhone and Android cameras offer to join the network
  drawQrBox(c, String("WIFI:T:nopass;S:") + SETUP_AP_NAME + ";;");
  int x = 224, w = 92;
  drawLeft(c, "Wi-Fi", x, 8, &fonts::FreeSansBold12pt7b, C_ACCENT, w);
  drawLeft(c, "setup", x, 34, &fonts::FreeSansBold12pt7b, C_ACCENT, w);
  const char* lines[] = {"Scan with", "your phone", "camera,", "then pick", "your home", "Wi-Fi."};
  for (int i = 0; i < 6; i++) drawLeft(c, lines[i], x, 76 + i * 20, &fonts::FreeSans9pt7b, C_TIME, w);
  if (s.wifiName.length()) {
    drawCentered(c, "Can't reach " + s.wifiName + ", still trying", 229, &fonts::FreeSans9pt7b, C_WARN);
  } else {
    drawCentered(c, SETUP_AP_NAME, 229, &fonts::FreeSans9pt7b, C_DIM);
  }
}

String signature(const ScreenInfo& s) {
  String sig;
  sig.reserve(160);
  sig += (int)s.mode;
  sig += '|';
  if (s.timeValid) sig += (long)(s.now / 60);  // redraw once a minute
  sig += '|';
  sig += (long)s.nextAlarm; sig += '|';
  sig += s.alarmLabel; sig += '|';
  sig += (long)s.snoozeUntil; sig += '|';
  sig += s.song; sig += '|';
  sig += s.wifiConnected; sig += s.sdOk; sig += s.previewing; sig += s.use24h; sig += '|';
  sig += (int)s.songCount; sig += '|';
  sig += s.wifiName; sig += '|';
  sig += s.ip;
  return sig;
}

}  // namespace

namespace Display {

void begin() {
  s_lcd.init();
  s_lcd.setRotation(TFT_ROTATION);
  s_lcd.setBrightness(0);
  s_lcd.fillScreen(TFT_BLACK);

  s_canvas.setPsram(true);
  s_canvas.setColorDepth(16);
  s_canvasOk = s_canvas.createSprite(s_lcd.width(), s_lcd.height()) != nullptr;

  s_lcd.setTextColor(C_DIM);
  s_lcd.setFont(&fonts::FreeSans12pt7b);
  s_lcd.setTextDatum(textdatum_t::middle_center);
  s_lcd.drawString("Starting...", s_lcd.width() / 2, s_lcd.height() / 2);
  s_lcd.setBrightness(150);
}

void update(const ScreenInfo& s) {
  if (s.brightness != s_lastBrightness) {
    s_lcd.setBrightness(s.brightness);
    s_lastBrightness = s.brightness;
  }
  if (!s_canvasOk) return;

  String sig = signature(s);
  if (sig == s_lastSig) return;
  s_lastSig = sig;

  LGFX_Sprite& c = s_canvas;
  c.fillScreen(C_BG);
  switch (s.mode) {
    case ScreenInfo::RINGING: drawRinging(c, s); break;
    case ScreenInfo::SNOOZED: drawSnoozed(c, s); break;
    case ScreenInfo::INFO: drawInfo(c, s); break;
    case ScreenInfo::WIFI_SETUP: drawWifiSetup(c, s); break;
    default: drawNormal(c, s); break;
  }
  c.pushSprite(0, 0);
}

}  // namespace Display
