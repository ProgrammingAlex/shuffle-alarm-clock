#pragma once
#include <Arduino.h>
#include <vector>

struct SongInfo {
  String name;
  uint32_t size;
};

// Audio playback + the shuffle order.
// Songs are MP3 files in MUSIC_DIR on the SD card. Shuffle plays every song
// once before any repeats, and remembers its place across reboots.
// If the SD card is missing or empty, a built-in beep is used instead.
namespace Player {
  void begin();
  void loop();                         // keeps looking for the SD card if it was missing

  bool sdOk();
  void rescan();                       // re-read the song folder
  std::vector<SongInfo> songs();
  size_t songCount();
  uint64_t sdTotalBytes();
  uint64_t sdUsedBytes();

  String playNextShuffled(uint8_t volume);   // returns the song name (or "Beep")
  bool playSong(const String& name, uint8_t volume);
  void playBeep(uint8_t volume);
  void stop();
  void setVolume(uint8_t volume);            // 0-100

  bool isPlaying();
  bool takeTrackEnded();                     // true once each time a track finishes
  String nowPlaying();

  bool deleteSong(const String& name);
  String sanitizeName(const String& name);   // safe file name for uploads
}
