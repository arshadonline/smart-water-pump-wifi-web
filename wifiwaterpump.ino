/***************************************************************
 *  ESP32 Smart Water-Tank Controller  –  Watni Digital Labs
 *
 *  • Ultrasonic level sensor (HC-SR04 / JSN-SR04T)
 *  • Relay-controlled water pump
 *  • Auto / Manual web interface with multi-colour UI
 *  • Footer links to https://watnidigital.com/
 ****************************************************************/

#include <WiFi.h>
#include <WebServer.h>

/* ---------- Wi-Fi credentials ---------- */
const char* SSID     = "wilink-HeadOffice";
const char* PASSWORD = "12345678";

/* ---------- GPIO assignment ---------- */
#define TRIG_PIN   17          // Ultrasonic TRIG
#define ECHO_PIN   16          // Ultrasonic ECHO  (use level-shifter or 1 kΩ / 2 kΩ divider!)
#define RELAY_PIN   2          // Relay IN (HIGH = pump ON)
#define LED_PIN     2          // On-board LED (heartbeat)

/* ---------- Tank geometry & logic ---------- */
const float  SOUND_SPEED      = 0.0343;   // cm / µs at 25 °C
const uint16_t TANK_DEPTH_CM  = 200;      // Distance sensor→bottom when EMPTY
const uint16_t FULL_LEVEL_CM  = 180;      // ≥ this: stop pump
const uint16_t LOW_LEVEL_CM   =  50;      // ≤ this: start pump
const uint32_t MEAS_INTERVAL  = 5000;     // ms between level checks

WebServer server(80);

/* ---------- State variables ---------- */
bool  autoMode   = true;
bool  pumpState  = false;
uint16_t levelCM = 0;
uint32_t lastMeas = 0;

/* ---------- Helper: turn pump ON/OFF ---------- */
void turnPump(bool on)
{
  pumpState = on;
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
}

/* ---------- Helper: read water level ---------- */
uint16_t readLevelCm()
{
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 3; ++i) {                    // average 3 pings
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    uint32_t dur = pulseIn(ECHO_PIN, HIGH, 30000);     // 30 ms timeout
    if (dur == 0) dur = 30000;                         // out of range guard
    sum += dur;
    delay(60);
  }
  float distance = (sum / 3.0) * SOUND_SPEED / 2.0;    // cm
  if (distance > TANK_DEPTH_CM) distance = TANK_DEPTH_CM;
  return TANK_DEPTH_CM - static_cast<uint16_t>(distance);
}

/* ---------- Dynamic HTML page ---------- */
String htmlPage()
{
  String h = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
  body{font-family:'Segoe UI',sans-serif;text-align:center;
       background:linear-gradient(135deg,#e0f7fa,#e3f2fd);color:#333;padding:20px;}
  h2{color:#006064} p{font-size:1.2em;margin:10px}
  button{padding:12px 24px;font-size:1em;border:0;border-radius:6px;margin:8px;cursor:pointer;
         box-shadow:0 4px 6px rgba(0,0,0,0.1);transition:.3s}
  .auto{background:#81d4fa}.on{background:#aed581}.off{background:#ef9a9a}
  .auto:hover{background:#4fc3f7}.on:hover{background:#9ccc65}.off:hover{background:#e57373}
  .green{color:green}.red{color:red}
  .footer{margin-top:40px;font-size:.95em;color:#444}
  .footer a{color:#00695c;text-decoration:none;font-weight:bold}
  .footer a:hover{text-decoration:underline}
</style></head><body>
  <h2>Watni Smart Water Tank</h2>)rawliteral";

  h += "<p>Mode: <b>" + String(autoMode ? "AUTO" : "MANUAL") + "</b></p>";
  h += "<p>Water level: <b>" + String(levelCM) + " cm</b></p>";
  h += "<p>Pump is: <b class='" + String(pumpState ? "green" : "red") + "'>" +
       String(pumpState ? "ON" : "OFF") + "</b></p>";

  h += R"rawliteral(
  <a href="/auto"><button class="auto">Auto&nbsp;Mode</button></a>
  <a href="/on"><button class="on">Manual&nbsp;ON</button></a>
  <a href="/off"><button class="off">Manual&nbsp;OFF</button></a>

  <div class="footer">
    Smart Water Pump Solution developed by
    <a href="https://watnidigital.com/" target="_blank">Watni Digital Labs</a>
  </div>
</body></html>)rawliteral";

  return h;
}

/* ---------- Web route handlers ---------- */
void handleRoot()           { server.send(200, "text/html", htmlPage()); }
void handleAuto()           { autoMode = true;  server.sendHeader("Location","/"); server.send(303); }
void handleManualOn()       { autoMode = false; turnPump(true);  server.sendHeader("Location","/"); server.send(303); }
void handleManualOff()      { autoMode = false; turnPump(false); server.sendHeader("Location","/"); server.send(303); }

void setupWeb()
{
  server.on("/",     handleRoot);
  server.on("/auto", handleAuto);
  server.on("/on",   handleManualOn);
  server.on("/off",  handleManualOff);
  server.begin();
}

/* ---------- Arduino setup ---------- */
void setup()
{
  Serial.begin(115200);

  pinMode(TRIG_PIN,  OUTPUT);
  pinMode(ECHO_PIN,  INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN,   OUTPUT);
  turnPump(false);

  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print('.'); }
  Serial.println("\nWi-Fi connected  IP: " + WiFi.localIP().toString());

  setupWeb();
}

/* ---------- Main loop ---------- */
void loop()
{
  server.handleClient();

  uint32_t now = millis();
  if (now - lastMeas >= MEAS_INTERVAL) {
    lastMeas = now;

    levelCM = readLevelCm();
    Serial.printf("Level: %u cm  |  Pump: %s  |  Mode: %s\n",
                  levelCM, pumpState ? "ON" : "OFF", autoMode ? "AUTO" : "MAN");

    if (autoMode) {
      if (!pumpState && levelCM <= LOW_LEVEL_CM)  turnPump(true);
      if ( pumpState && levelCM >= FULL_LEVEL_CM) turnPump(false);
    }
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));   // heartbeat blink
  }
}
