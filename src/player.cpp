#include "player.h"
#include <SPI.h>
#include <SD.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Audio.h>
#include <algorithm>
#include <mutex>
#include "config.h"

namespace {
  enum CmdType : uint8_t { CMD_PLAY, CMD_STOP, CMD_VOLUME };
  struct Cmd {
    CmdType type;
    uint8_t volume;
    bool fromSd;
    uint32_t gen;
    char path[192];
  };

  constexpr const char* BEEP_PATH = "/beep.wav";

  QueueHandle_t s_queue = nullptr;
  volatile bool s_running = false;
  volatile uint32_t s_gen = 0;       // bumped for every new track
  volatile uint32_t s_endedGen = 0;  // gen of the last track that finished
  bool s_sdOk = false;

  std::mutex s_mutex;  // guards everything below
  std::vector<SongInfo> s_songs;
  String s_nowPlaying;
  Preferences s_prefs;
  uint32_t s_seed = 1;
  uint16_t s_pos = 0;
  uint32_t s_hash = 0;
  String s_lastName;

  // ---------- audio task: the only code that touches the Audio object ----------
  void audioTask(void*) {
    // Print the audio library's messages (decode errors, file problems) to
    // the serial monitor, so a song that stutters or cuts out shows why.
    Audio::audio_info_callback = [](Audio::msg_t m) {
      if (!m.msg) return;
      if (m.e != Audio::evt_info && m.e != Audio::evt_log && m.e != Audio::evt_eof) return;
      Serial.printf("[audio] %s\n", m.msg);
    };
    Audio* audio = new Audio();
    // Bigger safety cushion of decoded sound (about 0.28 s instead of 0.19 s)
    // so a short hiccup elsewhere doesn't turn into a gap you can hear.
    audio->settings.DMA_DESC_NUM = 48;
    // Skip the tone controls and level meter (not used) to save CPU time.
    audio->settings.IIR_FILTER = false;
    audio->settings.VU_LEVEL = false;
    audio->setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
    // The library decodes in its own task, by default on core 0, which it
    // shares with Wi-Fi. Wi-Fi outranks it, so web page traffic (and song
    // uploads especially) could starve it and make the music stutter.
    // Core 1 only has this clock's own lower-priority code on it.
    audio->setAudioTaskCore(1);
    audio->setVolumeSteps(100);
    // The amp's SD pin is driven HIGH to switch it on, which also makes it
    // play only the LEFT channel. Mix stereo songs into both channels first
    // so nothing from the right channel is lost.
    audio->forceMono(true);
    audio->setVolume(0);
    bool wasRunning = false;
    uint32_t playingGen = 0;
    Cmd c;
    for (;;) {
      while (xQueueReceive(s_queue, &c, 0) == pdTRUE) {
        switch (c.type) {
          case CMD_PLAY: {
            audio->stopSong();
            audio->setVolume(c.volume);
            fs::FS& fs = c.fromSd ? static_cast<fs::FS&>(SD) : static_cast<fs::FS&>(LittleFS);
            playingGen = c.gen;
            wasRunning = audio->connecttoFS(fs, c.path);
            if (!wasRunning) s_endedGen = playingGen;  // couldn't open: treat as finished
            break;
          }
          case CMD_STOP:
            audio->stopSong();
            wasRunning = false;
            break;
          case CMD_VOLUME:
            audio->setVolume(c.volume);
            break;
        }
      }
      audio->loop();
      bool r = audio->isRunning();
      if (wasRunning && !r) {
        wasRunning = false;
        s_endedGen = playingGen;
      }
      s_running = wasRunning;
      vTaskDelay(1);
    }
  }

  void send(CmdType type, uint8_t volume = 0, const String& path = "", bool fromSd = true) {
    Cmd c{};
    c.gen = s_gen;
    c.type = type;
    c.volume = volume;
    c.fromSd = fromSd;
    strlcpy(c.path, path.c_str(), sizeof(c.path));
    xQueueSend(s_queue, &c, pdMS_TO_TICKS(100));
  }

  void ampOn(bool on) { digitalWrite(PIN_AMP_SD, on ? HIGH : LOW); }

  // ---------- built-in fallback sound ----------
  void writeLE16(File& f, uint16_t v) { f.write((uint8_t*)&v, 2); }
  void writeLE32(File& f, uint32_t v) { f.write((uint8_t*)&v, 4); }

  void makeBeepWav() {
    if (LittleFS.exists(BEEP_PATH)) return;
    const uint32_t rate = 16000;
    const float seconds = 1.4f;
    const uint32_t samples = (uint32_t)(rate * seconds);
    File f = LittleFS.open(BEEP_PATH, "w");
    if (!f) return;
    f.write((const uint8_t*)"RIFF", 4);
    writeLE32(f, 36 + samples * 2);
    f.write((const uint8_t*)"WAVEfmt ", 8);
    writeLE32(f, 16);
    writeLE16(f, 1);          // PCM
    writeLE16(f, 1);          // mono
    writeLE32(f, rate);
    writeLE32(f, rate * 2);
    writeLE16(f, 2);
    writeLE16(f, 16);
    f.write((const uint8_t*)"data", 4);
    writeLE32(f, samples * 2);
    // Four short beeps, then a pause (the clock loops it)
    int16_t buf[256];
    size_t n = 0;
    for (uint32_t i = 0; i < samples; i++) {
      float t = (float)i / rate;
      float slot = fmodf(t, 0.2f);
      bool on = t < 0.8f && slot < 0.1f;
      buf[n++] = on ? (int16_t)(sinf(2.0f * PI * 2000.0f * t) * 14000) : 0;
      if (n == 256) { f.write((uint8_t*)buf, sizeof(buf)); n = 0; }
    }
    if (n) f.write((uint8_t*)buf, n * 2);
    f.close();
  }

  // ---------- shuffle ----------
  uint32_t xorshift(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }

  std::vector<uint16_t> makeOrder(size_t n, uint32_t seed) {
    std::vector<uint16_t> order(n);
    for (size_t i = 0; i < n; i++) order[i] = i;
    uint32_t s = seed ? seed : 1;
    for (size_t i = n; i > 1; i--) {
      size_t j = xorshift(s) % i;
      std::swap(order[i - 1], order[j]);
    }
    return order;
  }

  uint32_t hashSongs() {
    uint32_t h = 2166136261u;  // FNV-1a over all names
    for (const SongInfo& s : s_songs) {
      for (size_t i = 0; i < s.name.length(); i++) {
        h ^= (uint8_t)s.name[i];
        h *= 16777619u;
      }
      h ^= 0xFF;
      h *= 16777619u;
    }
    return h;
  }

  void saveShuffle() {
    s_prefs.putUInt("seed", s_seed);
    s_prefs.putUShort("pos", s_pos);
    s_prefs.putUInt("hash", s_hash);
    s_prefs.putString("last", s_lastName);
  }

  bool isMp3(const String& name) {
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".mp3");
  }
  uint32_t s_lastMountTry = 0;

  // Try to mount the SD card. Safe to call again later (card inserted after boot).
  bool mountCard() {
    s_lastMountTry = millis();
    SD.end();
    bool ok = SD.begin(PIN_SD_CS, SPI, 20000000);
    if (!ok) { SD.end(); ok = SD.begin(PIN_SD_CS, SPI, 4000000); }  // slower retry for picky cards
    if (!ok) {
      SD.end();
      Serial.println("[sd] NOT found. Check: card is FAT32, SD module VCC has 5V, wiring CS=10 SCK=12 MOSI=11 MISO=13");
      return false;
    }
    const char* types[] = {"none", "MMC", "SD", "SDHC/SDXC", "unknown"};
    uint8_t t = SD.cardType();
    Serial.printf("[sd] mounted (%s), %llu MB used of %llu MB\n", types[t > 4 ? 4 : t],
                  SD.usedBytes() >> 20, SD.totalBytes() >> 20);
    if (!SD.exists(MUSIC_DIR)) SD.mkdir(MUSIC_DIR);
    // Remove half-finished uploads left over from a power cut
    std::vector<String> leftovers;
    File dir = SD.open(MUSIC_DIR);
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      String n = f.name();
      if (n.startsWith(".up")) leftovers.push_back(String(MUSIC_DIR) + "/" + n);
      f.close();
    }
    dir.close();
    for (const String& p : leftovers) SD.remove(p);
    s_sdOk = true;
    return true;
  }
}

namespace Player {

void begin() {
  pinMode(PIN_AMP_SD, OUTPUT);
  ampOn(false);

  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  mountCard();

  if (LittleFS.begin(true)) makeBeepWav();

  s_prefs.begin("shuffle", false);
  s_seed = s_prefs.getUInt("seed", 1);
  s_pos = s_prefs.getUShort("pos", 0);
  s_hash = s_prefs.getUInt("hash", 0);
  s_lastName = s_prefs.getString("last", "");

  rescan();

  s_queue = xQueueCreate(8, sizeof(Cmd));
  // Priority 1: this task only refills the big file buffer (several seconds of
  // music), so it must never hold up the library's decoder (priority 2).
  xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 1, nullptr, 1);
}

bool sdOk() { return s_sdOk; }

void loop() {
  if (!s_sdOk && !s_running && millis() - s_lastMountTry > 5000) {
    if (mountCard()) rescan();
  }
}

void rescan() {
  std::vector<SongInfo> found;
  if (s_sdOk) {
    File dir = SD.open(MUSIC_DIR);
    if (dir && dir.isDirectory()) {
      File f = dir.openNextFile();
      while (f) {
        String name = f.name();
        int slash = name.lastIndexOf('/');
        if (slash >= 0) name = name.substring(slash + 1);
        if (!f.isDirectory() && !name.startsWith(".") && isMp3(name)) {
          found.push_back({name, (uint32_t)f.size()});
        }
        f.close();
        f = dir.openNextFile();
      }
      dir.close();
    }
  }
  std::sort(found.begin(), found.end(), [](const SongInfo& a, const SongInfo& b) {
    return a.name.compareTo(b.name) < 0;
  });
  std::lock_guard<std::mutex> lock(s_mutex);
  s_songs = found;
}

std::vector<SongInfo> songs() {
  std::lock_guard<std::mutex> lock(s_mutex);
  return s_songs;
}

size_t songCount() {
  std::lock_guard<std::mutex> lock(s_mutex);
  return s_songs.size();
}

uint64_t sdTotalBytes() { return s_sdOk ? SD.totalBytes() : 0; }
uint64_t sdUsedBytes() { return s_sdOk ? SD.usedBytes() : 0; }

String playNextShuffled(uint8_t volume) {
  String name;
  {
    std::lock_guard<std::mutex> lock(s_mutex);
    size_t n = s_songs.size();
    if (n > 0) {
      uint32_t h = hashSongs();
      bool newCycle = (h != s_hash) || (s_pos >= n);
      if (newCycle) {
        s_hash = h;
        s_pos = 0;
        // New random order; avoid starting with the song that just played
        for (int tries = 0; tries < 10; tries++) {
          s_seed = esp_random() | 1;
          if (n < 2 || s_songs[makeOrder(n, s_seed)[0]].name != s_lastName) break;
        }
      }
      std::vector<uint16_t> order = makeOrder(n, s_seed);
      name = s_songs[order[s_pos]].name;
      s_pos++;
      s_lastName = name;
      saveShuffle();
    }
  }
  if (name.length() && playSong(name, volume)) return name;
  playBeep(volume);
  return "Beep";
}

bool playSong(const String& name, uint8_t volume) {
  if (!s_sdOk) return false;
  String path = String(MUSIC_DIR) + "/" + name;
  if (!SD.exists(path)) return false;
  {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_nowPlaying = name.substring(0, name.length() - 4);  // drop ".mp3"
  }
  s_gen = s_gen + 1;
  s_running = true;
  ampOn(true);
  send(CMD_PLAY, volume, path, true);
  return true;
}

void playBeep(uint8_t volume) {
  {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_nowPlaying = "Beep";
  }
  s_gen = s_gen + 1;
  s_running = true;
  ampOn(true);
  send(CMD_PLAY, volume, BEEP_PATH, false);
}

void stop() {
  s_gen = s_gen + 1;  // anything still finishing is no longer "ours"
  send(CMD_STOP);
  s_running = false;
  {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_nowPlaying = "";
  }
  delay(20);
  ampOn(false);
}

void setVolume(uint8_t volume) { send(CMD_VOLUME, volume); }

bool isPlaying() { return s_running; }

bool takeTrackEnded() {
  if (s_endedGen != s_gen || s_gen == 0) return false;
  s_endedGen = 0;
  return true;
}

String nowPlaying() {
  std::lock_guard<std::mutex> lock(s_mutex);
  return s_nowPlaying;
}

bool deleteSong(const String& name) {
  if (!s_sdOk) return false;
  String safe = sanitizeName(name);
  if (safe != name) return false;  // refuse anything that isn't a plain file name
  bool ok = SD.remove(String(MUSIC_DIR) + "/" + name);
  rescan();
  return ok;
}

String sanitizeName(const String& input) {
  String name = input;
  int slash = max(name.lastIndexOf('/'), name.lastIndexOf('\\'));
  if (slash >= 0) name = name.substring(slash + 1);
  String out;
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    bool ok = isalnum((unsigned char)c) || strchr(" -_.()&,'!", c);
    out += ok ? c : '_';
  }
  out.trim();
  while (out.startsWith(".")) out = out.substring(1);
  if (!isMp3(out)) out += ".mp3";
  if (out.length() > 100) out = out.substring(0, 96) + ".mp3";
  if (out.length() <= 4) out = "song.mp3";
  return out;
}

}  // namespace Player
