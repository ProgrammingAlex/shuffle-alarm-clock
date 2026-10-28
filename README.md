# Shuffle Alarm Clock

ESP32-S3 alarm clock that plays a different song from its SD card every morning,
going through every song once before repeating. Alarms, music and settings are
managed from a web page served by the clock itself.

**Hardware:** ESP32-S3 N16R8, MAX98357A I2S amp + 4 Ω speaker, microSD module,
2" ST7789V LCD, DS3231 RTC, two buttons. Pins are in `src/config.h`.

## Building

Open this folder in VS Code with PlatformIO, then Build (✓) and Upload (→).

## Releasing an update

Clocks check this repo's latest GitHub Release every night at 3:30 AM and install it
if it's newer (never while an alarm is going off or within an hour of one).

1. Commit and push your changes.
2. Tag a new version and push the tag:
   ```
   git tag v1.0.1
   git push origin v1.0.1
   ```
3. GitHub Actions builds `firmware.bin` and publishes the release. The version number
   comes from the tag, so there's nothing to edit in the code.

To push an update right away, open the clock's web page → Settings → **Check for updates**.

New firmware is only kept once it's back on Wi-Fi. If it crashes or can't connect,
the clock rolls back to the previous version.

`firmware-factory.bin` in each release is a full image for flashing a brand-new board
over USB (offset 0x0).
