# Temporizador desde el Stream Deck

Flujo: botón del Stream Deck → plugin Home Assistant → `script.reloj_temporizador` → MQTT `reloj/cmd` → reloj.

## Comando MQTT (firmware)

Topic `reloj/cmd`:

```json
{"tipo": "temporizador", "segundos": 300}
{"tipo": "temporizador", "segundos": 300, "color": "#00FF88"}
{"tipo": "temporizador", "segundos": 0}
```

- Cuenta atrás en **MM:SS**; a partir de 100 minutos, **HH:MM**. Tope: 99h59m.
- Sin `color`, usa el color actual de la luz.
- Al llegar a 0 parpadea `0000` en rojo 10 s y vuelve a lo que mostraba antes (hora, texto, scroll o apagado).
- `segundos: 0` cancela y vuelve a lo anterior.
- Cualquier otro comando (`hora`, `texto`, `apagar`…) sustituye al temporizador. Apagar la luz desde HA lo cancela.
- La cuenta la lleva el propio reloj con `millis()`; un corte de WiFi no la para.
- `reloj/state` publica `modo: "temporizador"` y en `valor` los segundos restantes (en el heartbeat de 30 s).

## Script de Home Assistant

Instalado en `/config/scripts.yaml` como `script.reloj_temporizador` (editable desde Ajustes → Scripts).
Acepta `minutos` **o** `segundos` (si llegan los dos, manda `segundos`; si no llega ninguno, cancela):

```yaml
reloj_temporizador:
  alias: 'Reloj: temporizador'
  icon: mdi:timer-outline
  mode: restart
  fields:
    minutos:
      name: Minutos
      description: Duración en minutos (0 cancela)
      required: false
      selector:
        number: {min: 0, max: 600, step: 0.5, unit_of_measurement: min}
    segundos:
      name: Segundos
      description: 'Alternativa a minutos: duración en segundos (0 cancela)'
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

**Prueba sin Stream Deck:** Herramientas para desarrolladores → Acciones → `script.reloj_temporizador` con `minutos: 1` (o `segundos: 30`) → Realizar acción. Debe verse la cuenta atrás.

**Depurar:** Ajustes → Scripts → Reloj: temporizador → ⋮ → Trazas. Cada pulsación deja una traza con el payload publicado o el error.

## Botones del Stream Deck (plugin Home Assistant)

Plugin `de.perdoctus.streamdeck.homeassistant`, conectado a `http://192.168.1.4:8123` con un token de larga duración.
Cada botón es una acción **Entity (generic)** con Entity = `script.reloj_temporizador` y, en *Short Press*,
Domain = `script`, Service = `reloj_temporizador` y este *Service data*:

| Botón | Service data |
|---|---|
| 5 min | `{"minutos": 5}` |
| 10 min | `{"minutos": 10}` |
| 15 min | `{"minutos": 15}` |
| 25 min | `{"minutos": 25}` |
| Cancelar | `{"minutos": 0}` |
| 10 seg | `{"segundos": 10}` |

Para crear uno nuevo, copia un botón existente (Ctrl+C / Ctrl+V en la app de Stream Deck) y cambia título y service data.
