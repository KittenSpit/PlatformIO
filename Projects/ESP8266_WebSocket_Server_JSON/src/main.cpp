#include <Arduino.h>

/*
ESP8266 WebSocket Server with JSON + Broadcast + OLED IP display
- HTTP: http://<ip>/
- WS : ws://<ip>:81/
- OLED: Shows SSID while connecting, then IP + LED state
- Requires: arduinoWebSockets, ArduinoJson, Adafruit SSD1306, Adafruit GFX
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Hash.h> // needed by arduinoWebSockets on ESP8266
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ======= NEW: OLED =======
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // Reset not used on many ESP8266 boards
#define OLED_ADDR 0x3C // Most 0.96" displays use 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ======= Wi-Fi credentials =======
#define STA_SSID "M3000-5B5C"
#define STA_PASS "bb35aa12"

// ======= Hardware =======
const int LED_PIN = LED_BUILTIN; // Built-in LED on ESP8266 (usually GPIO2), ACTIVE-LOW
bool led_state = false; // logical state (true = ON)

// ======= Servers =======
ESP8266WebServer http(80);
WebSocketsServer ws(81);

// ======= Minimal test page (store in flash) =======
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP8266 JSON WS</title>
<style>
body{font-family:system-ui,sans-serif;margin:2rem}
#state{font-weight:bold}
button{padding:.6rem 1rem;margin-right:.5rem}
#log{border:1px solid #ccc;padding:.75rem;height:220px;overflow:auto;white-space:pre-wrap}
input[type=text]{width:16rem;padding:.5rem}
</style></head><body>
<h1>ESP8266 JSON WebSocket</h1>
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
void handleRoot() { http.send_P(200, "text/html; charset=utf-8", INDEX_HTML); }
void handleNotFound() { http.send(404, "text/plain", "Not found"); }

// ======= NEW: OLED helpers =======
void oledShowConnecting() {
display.clearDisplay();
display.setTextColor(SSD1306_WHITE);
display.setTextSize(1);
display.setCursor(0, 0);
display.println(F("ESP8266 JSON WS"));
display.println(F("-------------------"));
display.println(F("Wi-Fi: connecting"));
display.print(F("SSID : "));
display.println(F(STA_SSID));
display.display();
}

void oledShowState() {
display.clearDisplay();
display.setTextColor(SSD1306_WHITE);
display.setTextSize(1);
display.setCursor(0, 0);
display.println(F("ESP8266 JSON WS"));
display.println(F("-------------------"));
display.print(F("IP : "));
display.println(WiFi.localIP());
//display.print(F("HTTP: http://"));
//display.print(WiFi.localIP());
//display.println(F("/"));
//display.print(F("WS : ws://"));
//display.print(WiFi.localIP());
//display.println(F(":81/"));
display.println();
display.print(F("LED : "));
display.println(led_state ? F("ON") : F("OFF"));
display.display();
}

// ======= LED helpers (ESP8266 LED is ACTIVE-LOW) =======
void applyLed(bool on) {
digitalWrite(LED_PIN, on ? LOW : HIGH); // LOW = ON, HIGH = OFF
led_state = on;
oledShowState(); // update OLED whenever LED changes
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

JsonDocument hello;
hello["event"] = "hello"; hello["who"] = id; hello["msg"] = "welcome";
sendJsonTo(id, hello);

JsonDocument snap;
snap["event"] = "led"; snap["value"] = led_state;
sendJsonTo(id, snap);

JsonDocument joined;
joined["event"] = "presence"; joined["type"] = "join"; joined["who"] = id;
broadcastJson(joined);
} break;

case WStype_TEXT: {
JsonDocument doc;
if (deserializeJson(doc, payload, len)) return;
const char* cmd = doc["cmd"] | "";

if (strcmp(cmd, "led") == 0) {
bool wantOn = false;
if (doc["state"].is<bool>()) {
wantOn = doc["state"].as<bool>();
} else {
String st = doc["state"] | "off";
wantOn = st.equalsIgnoreCase("on");
}
applyLed(wantOn);

JsonDocument push;
push["event"] = "led"; push["value"] = led_state;
broadcastJson(push);

} else if (strcmp(cmd, "echo") == 0) {
JsonDocument echo;
echo["event"] = "echo"; echo["msg"] = doc["msg"] | "";
sendJsonTo(id, echo);
}
} break;

case WStype_DISCONNECTED: {
Serial.printf("Client %u disconnected\n", id);
JsonDocument left;
left["event"] = "presence"; left["type"] = "leave"; left["who"] = id;
broadcastJson(left);
} break;

default:
break;
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
Serial.begin(9600);
pinMode(LED_PIN, OUTPUT);
digitalWrite(LED_PIN, HIGH); // OFF (active-LOW)

// ---- NEW: OLED init ----
Wire.begin(); // SDA=D2(GPIO4), SCL=D1(GPIO5) on NodeMCU by default
if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
Serial.println(F("SSD1306 allocation failed"));
} else {
display.clearDisplay();
oledShowConnecting();
}

startWiFi();

// Show IP + state on OLED after Wi-Fi up
oledShowState();

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