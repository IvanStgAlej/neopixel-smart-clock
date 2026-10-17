# Phase 1 design: project foundation

Summary of the approved design for turning the original Arduino sketch into a reproducible, tested and versioned PlatformIO project. The original working notes (in Spanish) are kept in [docs/history](../history).

## Goal

Make the clock a project that anyone can build with one command, with no secrets in the repository, automated tests, and CI that builds, tests and publishes each version.

## Objectives

1. No credentials in the repository, now or in its history.
2. Reproducible PlatformIO build: pinned platform, core and library versions; one command to build and flash over USB or OTA.
3. Unit tests for all logic that does not depend on hardware, run on the PC.
4. GitHub Actions CI: build and test on every push; publish a release with the `.bin` on a `vX.Y.Z` tag.
5. The firmware knows its version and publishes it over MQTT and in Home Assistant.
6. English repository: code, comments, logs, commit messages and documentation.

Out of scope: new user-facing features, update-from-GitHub (phase 2), and a polished README with photos and GIFs (phase 3).

## Constraints

- Board: ESP8266 NodeMCU 1.0. ESP8266 core 3.1.2 (`espressif8266@4.2.1`) and the 4 MB / 2 MB FS memory layout (`eagle.flash.4m2m.ld`) are kept, so the LittleFS data (MQTT and DST settings) survives.
- Libraries stay at the versions that work today: Adafruit NeoPixel 1.15.5, ArduinoJson 7.4.3, PubSubClient 2.8, WiFiManager 2.0.17, NTPClient 3.2.1.
- Wireless updates (ArduinoOTA and the `/update` web page) keep working.
- The refactoring must not noticeably increase RAM use.
- The MQTT API is not translated: topics, JSON keys and Home Assistant entity names stay in Spanish, otherwise the Home Assistant script and the Stream Deck buttons would break.

## Design

### Secrets

Credentials move to `include/secrets.h` (git-ignored). `include/secrets.example.h` holds `"change-me"` placeholders. `config.h` includes `secrets.h` if present and otherwise falls back to the example with a `#warning`, so CI builds without real credentials. `scripts/check_secrets.sh` searches the files staged for commit (and optionally the whole history) for the real values without printing them.

### PlatformIO environments

| Environment | Purpose |
|---|---|
| `nodemcuv2` | Build and flash over USB, with pinned platform and libraries. |
| `nodemcuv2_ota` | Same, uploading over OTA to `reloj-neopixel.local`; the password comes from the `RELOJ_OTA_PASS` environment variable. |
| `native` | Run the Unity tests on the PC. |

### Testable core

Hardware-independent logic is extracted into `lib/clock_core/` and does not include `Arduino.h`. Time is passed in as a parameter (the `millis()` value) so the logic can be tested without the board. It does use ArduinoJson, which also runs on the PC, so command parsing is tested with the same library as the firmware.

| Module | Tested behaviour |
|---|---|
| Timer formatting | `MMSS` below 100 minutes, `HHMM` above, cap at 99h59m |
| Countdown | start, round-up display, reaching zero, blink phases, end after 10 s, cancel, restart while running, `millis()` wrap-around |
| Command parsing | each `tipo`, missing fields, invalid JSON, unknown type, zero/negative/fractional/textual `segundos`, optional color, truncation of long text |
| Glyphs | digits, letters, unknown characters, UTF-8 accent and tilde normalization, truncated multibyte input |

### Firmware version

`scripts/version.py` defines `FW_VERSION` from `git describe --tags` (untagged builds get `0.0.0-dev+<short-hash>`, with `-dirty` when the tree is dirty; plain `0.0.0-dev` only when git is unavailable). It is published in `reloj/state` as `version` and in the Discovery `device` block as `sw_version`.

### CI

- `ci.yml` (push and pull request): install PlatformIO with caching, run `pio test -e native`, run `pio run -e nodemcuv2` using the example secrets, upload the `.bin` as an artifact.
- `release.yml` (tag `v*`): build, test, and create a GitHub Release with `firmware-vX.Y.Z.bin` and its `sha256`. These binaries are built without `include/secrets.h`, so they carry placeholder credentials; they are reference artifacts and must not be flashed (directly or through `/update`) onto a real clock. Flash a build from source with your own `include/secrets.h`.

## Verification

1. `pio test -e native` passes.
2. `pio run -e nodemcuv2` builds with flash and RAM use close to the original.
3. No value from `secrets.h` appears in tracked files or history.
4. On the real clock: time, text, scroll, timer start and cancel, light/brightness/color from Home Assistant, DST switch, `version` in `reloj/state`, the `/update` page, and the saved MQTT configuration surviving the update.
5. CI green and a `v1.0.0` release with its `.bin` once the repository is on GitHub.
