#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "secrets.h"   
// expects:
// WIFI_SSID, WIFI_PASSWORD, MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS
// TOPIC_SOC, TOPIC_BATT, TOPIC_CUR, TOPIC_PV

LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiClient espClient;
PubSubClient client(espClient);

// ===== Bar chars (0..5) =====
byte bar0[8] = {0,0,0,0,0,0,0,0};
byte bar1[8] = {16,16,16,16,16,16,16,16};
byte bar2[8] = {24,24,24,24,24,24,24,24};
byte bar3[8] = {28,28,28,28,28,28,28,28};
byte bar4[8] = {30,30,30,30,30,30,30,30};
byte bar5[8] = {31,31,31,31,31,31,31,31};

// ===== Live values =====
int  soc = -1;       // 0..100
long battW = 0;      // + charge / - discharge
long curW  = 0;      // + import / - export
long pvW   = 0;      // PV power (W)

// ===== Rule: "grid stable around 0" =====
const int GRID_STABLE_W = 50;   // abs(curW) <= 50 => treat as stable around 0

// Optional: reduce flicker by rounding to 10W
long round10(long v) {
  if (v >= 0) return ((v + 5) / 10) * 10;
  return -(((-v + 5) / 10) * 10);
}

// MQTT reconnect pacing
unsigned long lastMqttTry = 0;

// ===== Helpers =====
String makeClientId() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[40];
  snprintf(buf, sizeof(buf), "ESP32-Batt-%04X%08X",
           (uint16_t)(mac >> 32), (uint32_t)mac);
  return String(buf);
}

String pad16(String s) {
  if (s.length() > 16) s = s.substring(0, 16);
  while (s.length() < 16) s += " ";
  return s;
}

void drawBarN(int percent, int col, int row, int widthChars) {
  int maxUnits = widthChars * 5;
  int totalUnits = (percent * maxUnits + 50) / 100;
  int fullChars = totalUnits / 5;
  int remainder = totalUnits % 5;

  lcd.setCursor(col, row);
  for (int i = 0; i < widthChars; i++) {
    if (i < fullChars) lcd.write(byte(5));
    else if (i == fullChars) {
      switch (remainder) {
        case 0: lcd.write(byte(0)); break;
        case 1: lcd.write(byte(1)); break;
        case 2: lcd.write(byte(2)); break;
        case 3: lcd.write(byte(3)); break;
        case 4: lcd.write(byte(4)); break;
      }
    } else {
      lcd.write(byte(0));
    }
  }
}

// Line2 formats (compact, always 16 chars)
String line2GridPv(long gridW, long pvW_) {
  // Example: "G+ 243W PV 114"
  long g = round10(gridW);
  long p = round10(pvW_);

  char sign = (g > 0) ? '+' : (g < 0) ? '-' : ' ';
  long gv = labs(g);
  long pv = labs(p);

  if (gv > 9999) gv = 9999;
  if (pv > 9999) pv = 9999;

  char buf[17];
  // "G+ 243W PV 114" => 14 chars, padded to 16
  snprintf(buf, sizeof(buf), "G%c%4ldW PV%4ld", sign, gv, pv);
  return pad16(String(buf));
}

String line2BattPv(long battW_, long pvW_) {
  // Example: "B- 180W PV 114"
  long b = round10(battW_);
  long p = round10(pvW_);

  char sign = (b > 0) ? '+' : (b < 0) ? '-' : ' ';
  long bv = labs(b);
  long pv = labs(p);

  if (bv > 9999) bv = 9999;
  if (pv > 9999) pv = 9999;

  char buf[17];
  snprintf(buf, sizeof(buf), "B%c%4ldW PV%4ld", sign, bv, pv);
  return pad16(String(buf));
}

void updateDisplay() {
  // Line 1: "SOC:75% " + 8-char bar
  if (soc < 0) {
    lcd.setCursor(0, 0);
    lcd.print("SOC:--% ");
    lcd.setCursor(8, 0);
    lcd.print("        ");
  } else {
    char left[9];
    snprintf(left, sizeof(left), "SOC:%2d%% ", soc); // 8 chars
    lcd.setCursor(0, 0);
    lcd.print(left);
    drawBarN(soc, 8, 0, 8);
  }

  // Line 2: your requested logic
  lcd.setCursor(0, 1);

  if (labs(curW) > GRID_STABLE_W) {
    // Not stable => show Grid import/export + PV
    lcd.print(line2GridPv(curW, pvW));
  } else {
    // Stable around 0 => show Battery charge/discharge + PV
    lcd.print(line2BattPv(battW, pvW));
  }
}

void setup_wifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - start > 20000) {
      Serial.println("\nWiFi timeout -> restart");
      ESP.restart();
    }
  }

  Serial.println("\nWi-Fi connected.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void mqttConnectNonBlocking() {
  if (client.connected()) return;

  unsigned long now = millis();
  if (now - lastMqttTry < 3000) return;
  lastMqttTry = now;

  String clientId = makeClientId();
  Serial.print("MQTT connect as ");
  Serial.println(clientId);

  if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
    Serial.println("MQTT connected.");
    client.subscribe(TOPIC_SOC);
    client.subscribe(TOPIC_BATT);
    client.subscribe(TOPIC_CUR);
    client.subscribe(TOPIC_PV);
    Serial.println("Subscribed: SOC + BATT + CUR + PV");
  } else {
    Serial.print("MQTT failed rc=");
    Serial.println(client.state());
  }
}

void onMessage(const String& t, String msg) {
  msg.trim();
  if (msg.length() == 0) return;

  if (t == String(TOPIC_SOC)) {
    int v = msg.toInt(); // tolerant: "21" or "21 %"
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    soc = v;
    updateDisplay();
    return;
  }

  if (t == String(TOPIC_BATT)) {
    battW = msg.toInt();
    // Only redraw if we are in "stable" mode (otherwise line2 uses grid+pv)
    if (labs(curW) <= GRID_STABLE_W) updateDisplay();
    return;
  }

  if (t == String(TOPIC_CUR)) {
    curW = msg.toInt();
    updateDisplay(); // changes which mode we are in
    return;
  }

  if (t == String(TOPIC_PV)) {
    pvW = msg.toInt();
    updateDisplay();
    return;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String t(topic);
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("Message arrived [");
  Serial.print(t);
  Serial.print("]: ");
  Serial.println(msg);

  onMessage(t, msg);
}

void setup() {
  Serial.begin(115200);

  // ESP32 I2C stable defaults
  Wire.begin(21, 22);
  Wire.setClock(100000);

  lcd.init();
  lcd.backlight();

  lcd.createChar(0, bar0);
  lcd.createChar(1, bar1);
  lcd.createChar(2, bar2);
  lcd.createChar(3, bar3);
  lcd.createChar(4, bar4);
  lcd.createChar(5, bar5);

  lcd.setCursor(0, 0);
  lcd.print("Battery Monitor ");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...   ");

  setup_wifi();

  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callback);

  delay(200);
  updateDisplay();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) setup_wifi();

  mqttConnectNonBlocking();
  client.loop();

  // periodic refresh keeps screen consistent
  static unsigned long lastUi = 0;
  if (millis() - lastUi >= 1000) {
    lastUi = millis();
    updateDisplay();
  }
}
