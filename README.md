# NeoPixel Smart Clock

[![CI](https://github.com/IvanStgAlej/neopixel-smart-clock/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/IvanStgAlej/neopixel-smart-clock/actions/workflows/ci.yml)

A WiFi-connected clock built around an ESP8266 and 56 NeoPixel LEDs arranged as four 7-segment digits. The enclosure is 3D-printed and the whole thing is hand-soldered.

- Time from NTP, with a daylight-saving switch
- WiFiManager captive portal for WiFi and MQTT broker setup
- MQTT control with Home Assistant discovery (light, DST switch, text, scrolling text, number, clock-mode button)
- Over-the-air updates (ArduinoOTA and a web uploader at `/update`)
- Countdown timer that can be started from an Elgato Stream Deck through a Home Assistant script (see [docs/timer-streamdeck.md](docs/timer-streamdeck.md))
- Hardware-independent logic lives in `lib/clock_core` and is unit-tested on the PC

## Hardware

- ESP8266 NodeMCU 1.0 (ESP-12E)
- 56 WS2812-style NeoPixel LEDs on pin `D2` (4 digits x 14 LEDs, 2 LEDs per segment, no colon)
- 3D-printed enclosure (see Credits)

## Architecture

```mermaid
flowchart LR
    SD[Stream Deck] --> HA[Home Assistant]
    HA --> B[MQTT broker]
    B <--> ESP[ESP8266]
    ESP --> LED[NeoPixels]
```

## Build and flash

Requires [PlatformIO](https://platformio.org/). Library and core versions are pinned in `platformio.ini`. Do not change the core or the `board_build.ldscript` memory layout, or the saved LittleFS configuration is erased.

1. Create your secrets file (it is git-ignored):

   ```
   cp include/secrets.example.h include/secrets.h
   ```

   Then edit `include/secrets.h` with your MQTT broker login and OTA passwords. Without it the project still compiles, using the placeholder values and a compiler warning.

2. Build and flash over USB:

   ```
   pio run -e nodemcuv2 -t upload
   ```

3. Or flash over the air (`nodemcuv2_ota`, target `reloj-neopixel.local`). The OTA password is read from the `RELOJ_OTA_PASS` environment variable. If it is not set, the upload is attempted with an empty password and fails against firmware that has one.

   ```
   RELOJ_OTA_PASS=your-ota-password pio run -e nodemcuv2_ota -t upload
   ```

On first boot the clock opens a WiFi access point called `Reloj-Config`; connect to it to enter your WiFi credentials and the MQTT broker address.

## Tests

```
pio test -e native
```

Runs the Unity tests for glyph rendering, timer formatting, the countdown state machine and MQTT command parsing on the PC.

## MQTT API

The API keys and values are in Spanish and are intentionally unchanged, so existing Home Assistant automations keep working.

| Topic | Direction | Payload |
|---|---|---|
| `reloj/cmd` | to clock | `{"tipo": "hora" \| "texto" \| "numero" \| "scroll" \| "apagar" \| "temporizador", ...}` |
| `reloj/light/set` | to clock | Home Assistant JSON light: `state`, `brightness`, `color` |
| `reloj/light/state` | from clock | Light state (JSON schema) |
| `reloj/state` | from clock | `encendido`, `modo`, `valor`, `color`, `brillo`, `ip`, `rssi`, `version` (every 30 s and on change) |
| `reloj/available` | from clock | `online` / `offline` (last will) |
| `reloj/dst/set`, `reloj/dst/state` | both | `ON` = summer time (UTC+2), `OFF` = winter time (UTC+1) |

Command fields for `reloj/cmd`:

| `tipo` | Fields |
|---|---|
| `hora` | none; shows the NTP time |
| `texto` | `valor` (text), optional `color` (`#RRGGBB`) |
| `numero` | `valor` (number), optional `color` |
| `scroll` | `valor` (text), optional `velocidad` (ms per step), optional `color` |
| `apagar` | none; turns the display off |
| `temporizador` | `segundos` (0 cancels), optional `color` |
| `tiempo` | `temperatura` (°C), `lluvia` (%), optional `segundos` (default 5, max 60). Temporary overlay: temperature on the left (blue ≤10 °C → white 20-25 °C → orange 30 °C → red ≥35 °C), rain 00-99 on the right in blue; then the previous content returns |
| `aviso` | `valor` (text, first 4 characters shown), optional `color`, optional `segundos` (default 5, max 60). Temporary overlay like `tiempo`: the color is not kept and the previous content returns |

## Versioning

Releases are git tags of the form `vX.Y.Z`. `scripts/version.py` derives `FW_VERSION` from `git describe --tags` at build time (`0.0.0-dev+<short-hash>`, with `-dirty` appended when the tree has uncommitted changes, when there is no tag; plain `0.0.0-dev` only when git is unavailable). The version is published in `reloj/state` (`version`) and as `sw_version` in the Home Assistant device info.

## Credits

Enclosure based on the design by matej_f (Thingiverse, CC license). The enclosure model files are not included in this repository.

## License

[MIT](LICENSE)

**Release binaries and credentials.** Release binaries are built in CI without `include/secrets.h`, so they contain the placeholder credentials from `include/secrets.example.h` (MQTT and OTA password `change-me`, `/update` login `admin`/`change-me`). They are CI artifacts for reference only: do not flash them onto a real clock (the MQTT login would fail and the OTA passwords would be guessable). To flash your own clock, build from source with your own `include/secrets.h`.
