/*
ESP32 WebSocket Server with JSON + Broadcast
- Connects to your Wi-Fi router (station mode only)
- HTTP: http://<ip>/
- WS: ws://<ip>:81/
- Requires: arduinoWebSockets, ArduinoJson
*/
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ======= Wi-Fi credentials =======
#define STA_SSID "M3000-5B5C"
#define STA_PASS "bb35aa12"

// ======= Hardware =======
const int LED_PIN = 2; // Built-in LED on many ESP32 boards
bool led_state = false;

// ======= Servers =======
WebServer http(80);
WebSocketsServer ws(81);

// ======= Minimal test page =======
const char* INDEX_HTML = R"HTML(
<!doctype html><html><head><meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP32 JSON WS</title>
<style>
body{font-family:system-ui,sans-serif;margin:2rem}
#state{font-weight:bold}
button{padding:.6rem 1rem;margin-right:.5rem}
#log{border:1px solid #ccc;padding:.75rem;height:220px;overflow:auto;white-space:pre-wrap}
input[type=text]{width:16rem;padding:.5rem}
</style></head><body>
<h1>ESP32 JSON WebSocket</h1>
<p>Status: <span id="st">connecting…</span> | LED: <span id="state">unknown</span></p>
<p>
<button onclick="send({cmd:'led', state:'on'})">LED ON</button>
<button onclick="send({cmd:'led', state:'off'})">LED OFF</button>
</p>
<p>
<input id="msg" type="text" placeholder="say something"/>
<button onclick="send({cmd:'echo', msg:document.getElementById('msg').value})">Send</button>
</p>
<pre id="log"></pre>
<script>
const st=document.getElementById('st');
const led=document.getElementById('state');
const log=m=>{const d=document.getElementById('log');d.textContent+=m+"\\n";d.scrollTop=d.scrollHeight;}
const ws=new WebSocket(`ws://${location.hostname}:81/`);
ws.onopen =()=>{st.textContent="connected";}
ws.onclose =()=>{st.textContent="closed";}
ws.onmessage=e=>{
try{
const msg=JSON.parse(e.data);
if(msg.event==='led'){ led.textContent = (msg.value===true||msg.value==='on')?'ON':'OFF'; }
log(e.data);
}catch(_){ log(e.data); }
};
function send(obj){ if(ws.readyState===1){ ws.send(JSON.stringify(obj)); } }
</script></body></html>
)HTML";

// ======= HTTP handlers =======
void handleRoot() { http.send(200, "text/html; charset=utf-8", INDEX_HTML); }
void handleNotFound() { http.send(404, "text/plain", "Not found"); }

// ======= LED helpers =======
void applyLed(bool on) {
digitalWrite(LED_PIN, on ? HIGH : LOW); // active-HIGH
led_state = on;
}

// ======= JSON helpers =======
void sendJsonTo(uint8_t id, const JsonDocument& doc) {
String s; serializeJson(doc, s);
ws.sendTXT(id, s);
}
void broadcastJson(const JsonDocument& doc) {
String s; serializeJson(doc, s);
ws.broadcastTXT(s);
}

// ======= WebSocket events =======
void onWsEvent(uint8_t id, WStype_t type, uint8_t *payload, size_t len) {
switch (type) {
case WStype_CONNECTED: {
IPAddress ip = ws.remoteIP(id);
Serial.printf("Client %u connected: %s\n", id, ip.toString().c_str());

StaticJsonDocument<128> hello;
hello["event"] = "hello"; hello["who"] = id; hello["msg"] = "welcome";
sendJsonTo(id, hello);

StaticJsonDocument<64> snap;
snap["event"] = "led"; snap["value"] = led_state;
sendJsonTo(id, snap);

StaticJsonDocument<64> joined;
joined["event"] = "presence"; joined["type"] = "join"; joined["who"] = id;
broadcastJson(joined);
} break;

case WStype_TEXT: {
StaticJsonDocument<256> doc;
if (deserializeJson(doc, payload, len)) return;
const char* cmd = doc["cmd"] | "";

if (strcmp(cmd, "led") == 0) {
bool wantOn = false;
if (doc["state"].is<bool>()) wantOn = doc["state"].as<bool>();
else {
const char* st = doc["state"] | "off";
wantOn = (strcasecmp(st, "on") == 0);
}
applyLed(wantOn);

StaticJsonDocument<64> push;
push["event"] = "led"; push["value"] = led_state;
broadcastJson(push);

} else if (strcmp(cmd, "echo") == 0) {
StaticJsonDocument<128> echo;
echo["event"] = "echo"; echo["msg"] = doc["msg"] | "";
sendJsonTo(id, echo);
}
} break;

case WStype_DISCONNECTED: {
Serial.printf("Client %u disconnected\n", id);
StaticJsonDocument<64> left;
left["event"] = "presence"; left["type"] = "leave"; left["who"] = id;
broadcastJson(left);
} break;
}
}

// ======= Wi-Fi setup (station mode only) =======
void startWiFi() {
WiFi.mode(WIFI_STA);
WiFi.begin(STA_SSID, STA_PASS);
Serial.printf("Connecting to %s", STA_SSID);
while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
}
Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
}

// ======= Arduino entry points =======
void setup() {
Serial.begin(115200);
pinMode(LED_PIN, OUTPUT);
digitalWrite(LED_PIN, LOW);

startWiFi();

http.on("/", handleRoot);
http.onNotFound(handleNotFound);
http.begin();

ws.begin();
ws.onEvent(onWsEvent);

Serial.println("HTTP :80 | WS :81");
Serial.println("Open http://<device_ip>/");
}

void loop() {
http.handleClient();
ws.loop();
}
