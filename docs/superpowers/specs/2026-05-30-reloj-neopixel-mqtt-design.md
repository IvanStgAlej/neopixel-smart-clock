# Reloj NeoPixel — Diseño MQTT + Home Assistant

**Fecha:** 2026-05-30  
**Estado:** Aprobado  
**Hardware:** ESP8266 NodeMCU ESP-12E + tira NeoPixel 56 LEDs (4 dígitos HH:MM)

---

## 1. Objetivo

Conectar el reloj digital NeoPixel a Home Assistant vía MQTT. HA actúa como cerebro y controlador único — el ESP8266 es un ejecutor que responde comandos. El sistema debe ser escalable: añadir nuevas funciones solo requiere nuevos tipos de comando, sin cambiar la arquitectura.

---

## 2. Hardware

| Componente | Detalle |
|---|---|
| Microcontrolador | ESP8266 NodeMCU ESP-12E |
| LEDs | Tira NeoPixel, 56 LEDs, pin D2 (GPIO4) |
| Configuración | 4 dígitos × 14 LEDs por dígito (formato HH:MM) |
| RTC | Ninguno — hora por NTP vía WiFi |
| Botón | No utilizado |

---

## 3. Arquitectura general

```
┌─────────────────────────────────────────────────────┐
│                   HOME ASSISTANT                     │
│  ┌─────────────┐    ┌──────────────────────────┐    │
│  │ Entidad Light│    │ Automatizaciones / Scripts│    │
│  │ (color, on/off   └──────────┬───────────────┘    │
│  │  brillo)    │               │                     │
│  └──────┬───────┘              │                     │
│         │                      │                     │
│  ┌──────▼──────────────────────▼──────────────┐     │
│  │           Mosquitto MQTT Broker             │     │
│  └──────┬──────────────────────┬──────────────┘     │
└─────────┼──────────────────────┼────────────────────┘
          │                      │
    reloj/light/set          reloj/cmd
    reloj/light/state        reloj/state
          │                      │
┌─────────▼──────────────────────▼───────────────────┐
│                   ESP8266 NodeMCU                    │
│                                                      │
│  WiFiManager → MQTT Client → Command Handler        │
│                                   ↓                  │
│                           Display Engine             │
│                                   ↓                  │
│                        NeoPixels (56 LEDs, D2)       │
│                                                      │
│  NTP Client (sincronización horaria, sin RTC físico) │
└──────────────────────────────────────────────────────┘
```

---

## 4. Comportamiento del sistema

### 4.1 Arranque
1. ESP8266 arranca e intenta conectar al WiFi guardado en flash
2. Si no hay WiFi configurado (o falla): crea AP `"Reloj-Config"` con portal web para configurar SSID + contraseña + IP del broker MQTT
3. WiFi conectado → sincroniza hora por NTP
4. Conecta al broker MQTT
5. Publica payload de MQTT Discovery → HA registra el dispositivo automáticamente
6. Suscribe a `reloj/light/set` y `reloj/cmd`
7. Publica estado inicial en `reloj/light/state` y `reloj/state`
8. Espera comandos de HA

### 4.2 Control de HA
- **HA controla todo** — el ESP no tiene comportamiento por defecto propio
- El ESP ejecuta lo que HA le indique y publica feedback
- Si HA no ha enviado ningún comando desde el arranque: LEDs apagados

### 4.3 Pérdida de conexión
- Si se pierde WiFi o MQTT: **todos los LEDs se apagan**
- El ESP reintenta la conexión en background (sin bloquear)
- Al reconectar: publica estado actual y queda a la espera

### 4.4 Modo reloj (autónomo)
- Cuando HA envía `{"tipo": "hora"}`: el ESP entra en "modo reloj"
- Actualiza los dígitos cada minuto usando la hora interna (NTP)
- Re-sincroniza con NTP cada hora para evitar desvíos
- El modo reloj continúa hasta que HA envíe otro comando

---

## 5. Protocolo MQTT

### 5.1 Topics

| Topic | Dirección | Propósito |
|---|---|---|
| `reloj/light/set` | HA → ESP | Control de luz (encendido, color, brillo) |
| `reloj/light/state` | ESP → HA | Estado de la entidad Light (feedback) |
| `reloj/cmd` | HA → ESP | Comandos de display (qué mostrar) |
| `reloj/state` | ESP → HA | Estado completo del dispositivo |

### 5.2 Entidad Light — `reloj/light/set`

Payload compatible con MQTT Light de HA:
```json
{"state": "ON", "color": {"r": 255, "g": 0, "b": 128}, "brightness": 200}
{"state": "OFF"}
```

Formato de color en comandos de display: **hex HTML** (`#RRGGBB`)

### 5.3 Comandos de display — `reloj/cmd`

Todos los comandos comparten el campo `tipo`. Extensible: añadir nuevos `tipo` no requiere cambios de arquitectura.

```json
{"tipo": "hora"}
{"tipo": "texto",   "valor": "HOLA",        "color": "#FF0000"}
{"tipo": "numero",  "valor": "42",           "color": "#00FF00"}
{"tipo": "scroll",  "valor": "Hola Mundo!",  "color": "#0000FF", "velocidad": 150}
{"tipo": "efecto",  "nombre": "pulso",       "color": "#FFFFFF"}
{"tipo": "apagar"}
```

**Campos comunes:**
- `tipo` (string, requerido): identifica la acción
- `color` (string, opcional): hex `#RRGGBB`; si se omite, usa el color actual de la entidad Light (`reloj/light/set`)
- `velocidad` (int, ms, opcional para scroll): tiempo entre desplazamientos, default 150ms

**Prioridad de color:** el color del campo `color` en `reloj/cmd` sobreescribe temporalmente el color de la entidad Light. El brillo siempre viene de la entidad Light.

**Comportamiento al terminar scroll:** el scroll se reproduce una vez y los LEDs se apagan. Para reproducirlo en bucle, HA debe reenviar el comando.

### 5.4 Estado del dispositivo — `reloj/state`

Publicado al arrancar, tras cada comando recibido, y cada 30s como heartbeat:
```json
{
  "encendido": true,
  "modo": "texto",
  "valor": "HOLA",
  "color": "#FF0000",
  "brillo": 80,
  "ip": "192.168.1.50",
  "rssi": -65
}
```

### 5.5 MQTT Last Will

Al conectarse, el ESP registra este mensaje de "última voluntad":
```json
{"encendido": false}
```
Publicado automáticamente por el broker en `reloj/state` si el ESP se desconecta inesperadamente.

---

## 6. MQTT Discovery (auto-registro en HA)

Al arrancar, el ESP publica en:
`homeassistant/light/reloj_neopixel/config`

Payload que registra el dispositivo como entidad Light nativa con soporte de color y brillo. Tras esto, aparece en HA sin ningún YAML manual.

---

## 7. Estructura del firmware

```
reloj_neopixel/
├── reloj_neopixel.ino      ← Setup y loop principal
├── config.h                ← Constantes: pines, NTP, topics MQTT
├── wifi_manager.h/.cpp     ← Portal WiFiManager para configuración
├── mqtt_client.h/.cpp      ← Conexión, Discovery, pub/sub
├── ntp_client.h/.cpp       ← Sincronización horaria
├── display_engine.h/.cpp   ← Renderizado en NeoPixels (dígitos, scroll, efectos)
└── command_handler.h/.cpp  ← Parsea JSON → llama al módulo correspondiente
```

**Principio de extensibilidad:** para añadir una nueva función, solo se modifica `command_handler` añadiendo un nuevo `tipo` y su lógica de display — el resto de módulos no cambia.

---

## 8. Mapeo de pines

| Función | Pin NodeMCU | GPIO |
|---|---|---|
| NeoPixels (datos) | D2 | GPIO4 |

---

## 9. Dependencias (librerías Arduino)

| Librería | Uso |
|---|---|
| `Adafruit NeoPixel` | Control de la tira LED |
| `WiFiManager` (tzapu) | Portal de configuración WiFi |
| `PubSubClient` | Cliente MQTT |
| `ArduinoJson` | Parseo de comandos JSON |
| `NTPClient` | Sincronización de hora |
| `ESP8266WiFi` | WiFi (incluida con el paquete ESP8266) |

---

## 10. Escalabilidad futura

El diseño está preparado para añadir sin refactorizar:
- Nuevos tipos de comando (nuevo `tipo` en `command_handler`)
- Nuevos efectos (nueva función en `display_engine`)
- Nuevas entidades MQTT Discovery (sensores, botones en HA)
- OTA (actualización de firmware por WiFi)
