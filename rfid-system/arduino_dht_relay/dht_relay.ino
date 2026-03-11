/*
 * =====================================================
 *  Arduino Mega 2560 + DHT11 + Relay x2
 *  TƯƠNG THÍCH CẢ 2 CHẾ ĐỘ:
 *
 *  Chế độ 1: Cắm USB → Node.js server (Serial)
 *  Chế độ 2: Nối ESP32 → WiFi/Firebase (Serial1)
 *  Chế độ 3: Cả hai cùng lúc!
 *
 *  Arduino gửi JSON trên CẢ Serial + Serial1
 *  Arduino nhận lệnh từ CẢ Serial + Serial1
 * =====================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"

#define DHTPIN         7
#define RELAY_PIN      8   // Quạt
#define PUMP_PIN       9   // Bơm
#define DHTTYPE        DHT11
#define TEMP_THRESHOLD 20.0
#define HUM_THRESHOLD  75.0

#define FAN_ON   HIGH
#define FAN_OFF  LOW
#define PUMP_ON  LOW
#define PUMP_OFF HIGH

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

bool fanState   = false;
bool pumpState  = false;
bool fanManual  = false;
bool pumpManual = false;

// =====================================================
void setFan(bool on) {
  fanState = on;
  digitalWrite(RELAY_PIN, on ? FAN_ON : FAN_OFF);
}

void setPump(bool on) {
  pumpState = on;
  digitalWrite(PUMP_PIN, on ? PUMP_ON : PUMP_OFF);
}

// Gửi JSON trên CẢ 2 cổng Serial
void sendStatus(float t, float h) {
  String json = "{\"t\":";
  json += String(t, 1);
  json += ",\"h\":";
  json += String(h, 1);
  json += ",\"fan\":";
  json += (fanState ? "true" : "false");
  json += ",\"pump\":";
  json += (pumpState ? "true" : "false");
  json += "}";

  Serial.println(json);   // USB → Node.js server
  Serial1.println(json);  // Pin18/19 → ESP32
}

// Xử lý lệnh (dùng chung cho cả 2 nguồn)
void handleCommand(String cmd) {
  if (cmd == "FAN:ON")   { fanManual  = true;  setFan(true);   }
  if (cmd == "FAN:OFF")  { fanManual  = true;  setFan(false);  }
  if (cmd == "PUMP:ON")  { pumpManual = true;  setPump(true);  }
  if (cmd == "PUMP:OFF") { pumpManual = true;  setPump(false); }
  if (cmd == "AUTO")     { fanManual  = false; pumpManual = false; }
}

// =====================================================
void setup() {
  Serial.begin(9600);    // USB (Node.js server hoặc Serial Monitor)
  Serial1.begin(9600);   // Pin18/19 (ESP32)

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  setFan(false);
  setPump(false);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Khoi dong...  ");
  dht.begin();
  delay(3000);
  lcd.clear();

  Serial.println("Arduino san sang! (USB)");
  Serial1.println("Arduino san sang! (ESP32)");
}

// =====================================================
unsigned long lastSensorTime = 0;
const unsigned long SENSOR_INTERVAL = 1000;
float lastT = NAN, lastH = NAN;

void loop() {
  // ── Nhận lệnh từ USB (Node.js / Serial Monitor) ──
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      Serial.print("[USB] ");  Serial.println(cmd);
      handleCommand(cmd);
    }
  }

  // ── Nhận lệnh từ ESP32 (Serial1) ──
  if (Serial1.available() > 0) {
    String cmd = Serial1.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      Serial.print("[ESP32] "); Serial.println(cmd);
      handleCommand(cmd);
    }
  }

  // ── Cập nhật cảm biến mỗi 1 giây ──
  unsigned long now = millis();
  if (now - lastSensorTime < SENSOR_INTERVAL) return;
  lastSensorTime = now;

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    if (!isnan(lastT) && !isnan(lastH)) {
      h = lastH; t = lastT;
    } else {
      lcd.setCursor(0, 0); lcd.print("LOI DHT11!      ");
      lcd.setCursor(0, 1); lcd.print("Kiem tra day    ");
      return;
    }
  } else {
    lastT = t; lastH = h;
  }

  // Điều khiển tự động
  if (!fanManual)  setFan(t  < TEMP_THRESHOLD);
  if (!pumpManual) setPump(h < HUM_THRESHOLD);

  // Gửi JSON → cả USB và ESP32
  sendStatus(t, h);

  // LCD
  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print(t, 1);
  lcd.print((char)223); lcd.print("C ");
  lcd.print(fanState ? "QUAT:ON " : "QUAT:OFF");

  lcd.setCursor(0, 1);
  lcd.print("H:"); lcd.print(h, 1);
  lcd.print("% ");
  lcd.print(pumpState ? "BOM:ON  " : "BOM:OFF ");
}
