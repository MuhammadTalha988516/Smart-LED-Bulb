// ============================================================
//  NODE 1 — SMART LED ACTUATOR NODE
//  Hardware : ESP32-S3 + LED (GPIO2) + Button (GPIO4) + LCD I2C
//  Role     : Subscribes to MQTT topic "home/led/control"
//             Publishes LED state to   "home/led/status"
//             Button also toggles LED locally
//  Broker   : LAPTOP (connected to Pi hotspot — IP usually 192.168.4.2)
//
//  ⚠️  IMPORTANT: Check your laptop's IP on the Pi hotspot network
//      Run on laptop:  ipconfig (Windows)  OR  ip a (Linux/Mac)
//      Look for the IP under the WiFi adapter connected to RaspberryPI_IDS
//      It will be something like 192.168.4.2 or 192.168.4.3
//      Set that IP below in MQTT_SERVER
// ============================================================

#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PubSubClient.h>

// ================= WIFI — Pi Hotspot =================
const char* ssid     = "fyp";   // Pi hotspot SSID
const char* password = "12345678";        // Pi hotspot password

// ================= MQTT — Mosquitto on LAPTOP =================
const char* MQTT_SERVER   = "192.168.137.1";  // ← YOUR LAPTOP'S IP on Pi hotspot
const int   MQTT_PORT     = 1883;
const char* MQTT_CLIENT   = "ESP32_LED_Node";
const char* TOPIC_CONTROL = "home/led/control"; // subscribe  (laptop → node)
const char* TOPIC_STATUS  = "home/led/status";  // publish    (node → laptop)

// ================= PINS =================
#define BUTTON_PIN 4
#define LED_PIN    2

// ================= LCD =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= STATE =================
bool ledState       = false;
bool lastButtonState = HIGH;

// ================= OBJECTS =================
WiFiClient   espClient;
PubSubClient mqtt(espClient);

// ============================================================
//  MQTT CALLBACK — called when a message arrives on subscribed topic
// ============================================================
void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  msg.toUpperCase();

  Serial.print("[MQTT] Received on ");
  Serial.print(topic);
  Serial.print(" → ");
  Serial.println(msg);

  if (String(topic) == TOPIC_CONTROL) {
    if (msg == "ON") {
      ledState = true;
    } else if (msg == "OFF") {
      ledState = false;
    }
    applyLED();
  }
}

// ============================================================
//  APPLY LED STATE + update LCD + publish status back
// ============================================================
void applyLED()
{
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);

  String statusMsg = ledState ? "ON" : "OFF";

  // Publish status so IDS and laptop know the real state
  mqtt.publish(TOPIC_STATUS, statusMsg.c_str(), true);

  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bulb Status:");
  lcd.setCursor(0, 1);
  lcd.print(statusMsg);

  Serial.print("[LED] State → ");
  Serial.println(statusMsg);
}

// ============================================================
//  CONNECT / RECONNECT TO MQTT BROKER
// ============================================================
void connectMQTT()
{
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Connecting to broker...");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MQTT Connecting");

    if (mqtt.connect(MQTT_CLIENT)) {
      Serial.println(" Connected!");
      mqtt.subscribe(TOPIC_CONTROL);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT OK");
      lcd.setCursor(0, 1);
      lcd.print(MQTT_SERVER);
      delay(1500);

      // Publish current state so broker knows node is alive
      applyLED();

    } else {
      Serial.print(" Failed. RC=");
      Serial.print(mqtt.state());
      Serial.println(" — retrying in 3s");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT Failed");
      lcd.setCursor(0, 1);
      lcd.print("Retry in 3s...");
      delay(3000);
    }
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup()
{
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);

  // LCD
  Wire.begin(18, 17);        // SDA=18, SCL=17 (ESP32-S3)
  Wire.setClock(100000);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart LED Node");
  delay(1500);

  // WiFi — connect to Pi hotspot
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connecting");
  WiFi.begin(ssid, password);

  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(dots % 16, 1);
    lcd.print(".");
    dots++;
  }

  Serial.println("\n[WiFi] Connected → " + WiFi.localIP().toString());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(2000);

  // MQTT
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  connectMQTT();
}

// ============================================================
//  LOOP
// ============================================================
void loop()
{
  // Keep MQTT alive
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  // ---- Physical button toggle ----
  bool currentButton = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && currentButton == LOW) {
    ledState = !ledState;
    applyLED();
    delay(200); // debounce
  }
  lastButtonState = currentButton;
}
