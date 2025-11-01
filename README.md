# 📡 RAK2305 Meshtastic BLE ↔ MQTT Gateway

Firmware voor **RAK2305 (ESP32)** die koppelt met een **RAK4631 (Meshtastic)** via BLE.
Verbindt met Wi‑Fi en publiceert/ontvangt berichten via MQTT met een **realtime webinterface**.

## ✨ Functies
- BLE ↔ MQTT bridge via **één enkel topic** (geen `/fromMesh` of `/toMesh`)
- Webinterface (HTTP) + WebSocket voor live logging en verzenden
- Topic **instelbaar via de browser** en opgeslagen in flash (Preferences)
- Compatibel met RAK19007 baseboard + RAK4631 (Meshtastic)

## ⚙️ Instellingen (vóór uploaden)
In het `.ino`-bestand:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

const char* mqtt_server = "mqtt.meshnet.nl";
const char* mqtt_user = "YOUR_MQTT_USERNAME";
const char* mqtt_pass = "YOUR_MQTT_PASSWORD";
```
> Voorbeeld broker (zoals gebruikt in tests):
> - Server: `mqtt.meshnet.nl`
> - Port: `1883`
> - User: `boreft`
> - Password: `meshboreft`

Standaard topic (instelbaar via web): `msh/EU_868/Zuid-Holland`

## 🧰 Vereiste libraries
- **PubSubClient** (Nick O’Leary)
- **arduinoWebSockets** (Markus Sattler)
- **ESP32 BLE Arduino** (nkolban)

## 🚀 Upload
- Board: **ESP32 Dev Module**
- Partition Scheme: **No OTA (Large APP)**
- Upload Speed: 921600 (of 115200)
- Open na upload: `http://<IP-van-de-ESP32>` (UI) – WebSocket op poort 81

## 🔧 Gebruik
- In de webpagina kun je het MQTT-topic aanpassen en opslaan.
- Berichten die via BLE binnenkomen worden op het topic gepubliceerd.
- Berichten die je naar het topic publiceert (of via de webpagina verzendt) gaan het mesh in via BLE.

## 🪴 Licentie
MIT License
