/*
  RAK2305 Meshtastic BLE ↔ MQTT Gateway + Web UI (configurable topic, single-topic mode)
  --------------------------------------------------------------------------------------
  - Eén MQTT-topic (geen /fromMesh of /toMesh)
  - Topic instelbaar via webinterface
  - Topic wordt opgeslagen in flash (Preferences)
  - Realtime webinterface met WebSocket
  - Ontworpen voor RAK2305 (ESP32) + RAK4631 (Meshtastic via BLE)
  - Vereist libraries: PubSubClient, arduinoWebSockets, ESP32 BLE Arduino

  Upload tips:
  - Board: ESP32 Dev Module
  - Partition Scheme: No OTA (Large APP)   <-- belangrijk (grote sketch)
  - Upload Speed: 921600 (of 115200)

  Vul je eigen Wi‑Fi en (optioneel) MQTT in hieronder.
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "BLEDevice.h"
#include <WebServer.h>
#include <WebSocketsServer.h>   // Library: WebSockets by Markus Sattler

// ===== Wi-Fi instellingen =====
// VUL DIT IN VOOR UPLOADEN (plaatsvervangers – veilig voor GitHub)
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ===== MQTT server instellingen =====
// Gebruik de standaard meshnet MQTT-server of je eigen broker
const char* mqtt_server = "mqtt.meshnet.nl";
const int   mqtt_port   = 1883;
// Laat placeholders in de code staan (veilig voor GitHub); README bevat voorbeeld-credentials.
const char* mqtt_user   = "YOUR_MQTT_USERNAME";
const char* mqtt_pass   = "YOUR_MQTT_PASSWORD";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Preferences prefs;

// ===== BLE UUIDs (Meshtastic UART service) =====
static BLEUUID serviceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID charUUID_TX("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID charUUID_RX("6e400003-b5a3-f393-e0a9-e50e24dcca9e");

static BLERemoteCharacteristic* pTXChar = nullptr;
static BLERemoteCharacteristic* pRXChar = nullptr;
static BLEAdvertisedDevice* myDevice = nullptr;
static bool bleConnected = false;
static bool doConnect    = false;

// ===== Webserver + WebSocket =====
WebServer server(80);
WebSocketsServer webSocket(81);

// ===== Variabelen =====
String mqtt_topic = "msh/EU_868/Zuid-Holland";   // default; via web instelbaar
String lastReceived = "";
String lastSent     = "";

// ---------- BLE ----------
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {}
  void onDisconnect(BLEClient* pclient) { bleConnected = false; }
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
    }
  }
};

bool connectToServer() {
  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  if (!pClient->connect(myDevice)) return false;

  BLERemoteService* pService = pClient->getService(serviceUUID);
  if (!pService) return false;

  pTXChar = pService->getCharacteristic(charUUID_TX);
  pRXChar = pService->getCharacteristic(charUUID_RX);
  if (!pTXChar || !pRXChar) return false;

  // Notify vanuit Meshtastic → publish naar MQTT + push naar web UI
  pRXChar->registerForNotify([](BLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
    String msg; for (size_t i=0;i<len;i++) msg += (char)data[i];
    msg.trim();
    if (msg.length()) {
      lastReceived = msg;
      mqttClient.publish(mqtt_topic.c_str(), msg.c_str());
      webSocket.broadcastTXT(String("{\"type\":\"recv\",\"data\":\"") + msg + "\"}");
    }
  });

  bleConnected = true;
  return true;
}

// ---------- Wi‑Fi ----------
void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("🌐 Verbinden met Wi‑Fi: %s", ssid);
  int tries=0;
  while (WiFi.status() != WL_CONNECTED && tries<40) { delay(250); Serial.print("."); tries++; }
  Serial.println();
  if (WiFi.status()==WL_CONNECTED) {
    Serial.printf("✅ Wi‑Fi verbonden! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("❌ Wi‑Fi niet verbonden (controleer SSID/wachtwoord)");
  }
}

// ---------- MQTT ----------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (!bleConnected || !pTXChar) return;
  String msg; for (unsigned int i=0;i<length;i++) msg += (char)payload[i];
  msg.trim();
  if (msg.length()) {
    lastSent = msg;
    pTXChar->writeValue((uint8_t*)msg.c_str(), msg.length());
    webSocket.broadcastTXT(String("{\"type\":\"sent\",\"data\":\"") + msg + "\"}");
  }
}

void reconnect_mqtt() {
  while (!mqttClient.connected()) {
    Serial.print("🔄 Verbinden met MQTT...");
    // Let op: vul eigen user/pass in de code in vóór uploaden of laat leeg voor anonieme broker
    if (mqttClient.connect("RAK2305_Gateway", mqtt_user, mqtt_pass)) {
      Serial.println("✅ MQTT verbonden");
      mqttClient.subscribe(mqtt_topic.c_str());
    } else {
      Serial.print("❌ rc="); Serial.println(mqttClient.state());
      delay(3000);
    }
  }
}

// ---------- WebSocket ----------
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg; for (size_t i=0;i<length;i++) msg += (char)payload[i];
    if (msg.startsWith("SEND:")) {
      String data = msg.substring(5);
      mqttClient.publish(mqtt_topic.c_str(), data.c_str());
      if (bleConnected && pTXChar) pTXChar->writeValue((uint8_t*)data.c_str(), data.length());
      lastSent = data;
      webSocket.broadcastTXT(String("{\"type\":\"sent\",\"data\":\"") + data + "\"}");
    }
    if (msg.startsWith("TOPIC:")) {
      mqtt_topic = msg.substring(6);
      prefs.putString("mqttTopic", mqtt_topic);
      mqttClient.subscribe(mqtt_topic.c_str());
      webSocket.broadcastTXT(String("{\"type\":\"topic\",\"data\":\"") + mqtt_topic + "\"}");
      Serial.printf("🔧 Nieuw topic: %s\n", mqtt_topic.c_str());
    }
  }
}

// ---------- Web UI ----------
const char HTML_PAGE[] PROGMEM = R"html(
<!doctype html><html><head>
<meta charset='utf-8'><title>RAK2305 Gateway</title>
<style>
body{font-family:sans-serif;margin:20px}input[type=text]{width:70%;padding:8px;margin:5px}
button{padding:8px 12px;margin:5px}pre{background:#eee;padding:10px;border-radius:8px}
</style></head><body>
<h1>📡 RAK2305 Gateway</h1>
<p><b>MQTT Topic:</b><br><input id='topic' value=''/>
<button onclick='saveTopic()'>Opslaan</button></p>
<hr><p><b>Verstuur bericht:</b><br>
<input id='msg' placeholder='Typ bericht...'/>
<button onclick='sendMsg()'>Verzend</button></p>
<hr><pre id='log'></pre>
<script>
let ws=new WebSocket('ws://'+location.hostname+':81/');
ws.onmessage=e=>{const d=JSON.parse(e.data);
  if(d.type==='recv') log('⬅ '+d.data);
  if(d.type==='sent') log('➡ '+d.data);
  if(d.type==='topic') document.getElementById('topic').value=d.data;};
function log(t){let l=document.getElementById('log');l.textContent+=t+'\\n';l.scrollTop=l.scrollHeight;}
function sendMsg(){ws.send('SEND:'+document.getElementById('msg').value);document.getElementById('msg').value='';}
function saveTopic(){ws.send('TOPIC:'+document.getElementById('topic').value);}
</script></body></html>
)html";

void handleRoot(){ server.send(200, "text/html", HTML_PAGE); }

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  BLEDevice::init("");

  // Topic uit flash lezen (of default)
  prefs.begin("meshgw", false);
  mqtt_topic = prefs.getString("mqttTopic", mqtt_topic);
  Serial.printf("📦 Topic geladen: %s\n", mqtt_topic.c_str());

  setup_wifi();

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  server.on("/", handleRoot);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("🌍 Webserver actief (HTTP:80 / WS:81)");

  // Start BLE scan
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  scan->setActiveScan(true);
  scan->start(10, false);
}

// ---------- Loop ----------
void loop() {
  if (!mqttClient.connected()) reconnect_mqtt();
  mqttClient.loop();

  if (doConnect) { connectToServer(); doConnect = false; }
  if (!bleConnected) BLEDevice::getScan()->start(5, false);

  server.handleClient();
  webSocket.loop();
}
