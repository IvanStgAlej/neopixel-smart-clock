# Timer from the Stream Deck

Flow: Stream Deck button → Home Assistant plugin → `script.reloj_temporizador` → MQTT `reloj/cmd` → clock.

The MQTT API (topics, JSON keys and values) is kept in Spanish on purpose, so existing Home Assistant scripts and Stream Deck buttons keep working.

## MQTT command (firmware)

Topic `reloj/cmd`:

```json
{"tipo": "temporizador", "segundos": 300}
{"tipo": "temporizador", "segundos": 300, "color": "#00FF88"}
{"tipo": "temporizador", "segundos": 0}
```

- Counts down in **MM:SS**; from 100 minutes up it shows **HH:MM**. Cap: 99h59m.
- Without `color`, the light's current color is used.
- At 0 it blinks `0000` in red for 10 s, then goes back to what it was showing before (time, text, scroll or off).
- `segundos: 0` cancels the timer and goes back to the previous display.
- Any other command (`hora`, `texto`, `apagar`, ...) replaces the timer. Turning the light off from HA cancels it.
- The countdown runs on the clock itself using `millis()`, so a WiFi outage does not stop it.
- `reloj/state` publishes `modo: "temporizador"` and the remaining seconds in `valor` (on the 30 s heartbeat).

## Home Assistant script

Installed in `/config/scripts.yaml` as `script.reloj_temporizador` (editable from Settings → Scripts).
It accepts `minutos` **or** `segundos` (if both arrive, `segundos` wins; if neither arrives, it cancels):

```yaml
reloj_temporizador:
  alias: 'Reloj: temporizador'
  icon: mdi:timer-outline
  mode: restart
  fields:
    minutos:
      name: Minutos
      description: Duration in minutes (0 cancels)
      required: false
      selector:
        number: {min: 0, max: 600, step: 0.5, unit_of_measurement: min}
    segundos:
      name: Segundos
      description: 'Alternative to minutos: duration in seconds (0 cancels)'
      required: false
      selector:
        number: {min: 0, max: 36000, unit_of_measurement: s}
  sequence:
  - action: mqtt.publish
    data:
      topic: reloj/cmd
      payload: >-
        {"tipo":"temporizador","segundos":{{ (segundos | float(0)) | int if segundos is defined
        else ((minutos | float(0)) * 60) | int if minutos is defined else 0 }}}
```

**Testing without a Stream Deck:** Developer tools → Actions → `script.reloj_temporizador` with `minutos: 1` (or `segundos: 30`) → Perform action. The countdown should appear on the clock.

**Debugging:** Settings → Scripts → Reloj: temporizador → ⋮ → Traces. Every button press leaves a trace with the published payload or the error.

## Stream Deck buttons (Home Assistant plugin)

Plugin `de.perdoctus.streamdeck.homeassistant`, connected to `http://<home-assistant-host>:8123` with a long-lived access token.
Each button is an **Entity (generic)** action with Entity = `script.reloj_temporizador` and, under *Short Press*,
Domain = `script`, Service = `reloj_temporizador` and this *Service data*:

| Button | Service data |
|---|---|
| 5 min | `{"minutos": 5}` |
| 10 min | `{"minutos": 10}` |
| 15 min | `{"minutos": 15}` |
| 25 min | `{"minutos": 25}` |
| Cancel | `{"minutos": 0}` |
| 10 sec | `{"segundos": 10}` |

To create a new one, copy an existing button (Ctrl+C / Ctrl+V in the Stream Deck app) and change the title and service data.
