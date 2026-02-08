#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "secrets.h"

// Initialize LCD (I2C address 0x27, 16 chars, 2 lines)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Wi-Fi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

String lastMessage = "";

// Eindeutige Client-ID (verhindert Disconnects durch doppelte Client IDs)
String makeClientId() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[40];
  snprintf(buf, sizeof(buf), "ESP32Power-%04X%08X",
           (uint16_t)(mac >> 32), (uint32_t)mac);
  return String(buf);
}

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void showPowerOnLcd(const String& msg) {
  lcd.setCursor(0, 0);
  lcd.print("Power:           "); // Line "clear" via padding

  lcd.setCursor(0, 1);

  String line = msg;
  line.trim();
  line += " W";

  // Pad to clear leftovers
  while (line.length() < 16) line += " ";
  if (line.length() > 16) line = line.substring(0, 16);

  lcd.print(line);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  message.reserve(length);

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // Nur updaten, wenn sich der Wert ändert -> weniger LCD Flackern
  if (message.length() > 0 && message != lastMessage) {
    lastMessage = message;
    showPowerOnLcd(message);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection... ");

    String clientId = makeClientId();
    Serial.print("ClientId=");
    Serial.println(clientId);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println("MQTT connected.");
      client.subscribe(MQTT_TOPIC_SUB);
      Serial.print("Subscribed to: ");
      Serial.println(MQTT_TOPIC_SUB);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Booting...");
  lcd.setCursor(0, 1);
  lcd.print("WiFi + MQTT");

  setup_wifi();

  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callback);

  delay(500);
  lcd.clear();
  showPowerOnLcd("--");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
