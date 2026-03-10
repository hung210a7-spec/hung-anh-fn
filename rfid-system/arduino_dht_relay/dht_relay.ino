/*
 * =====================================================
 *  Arduino Mega 2560 + DHT11 + Relay x2
 *  CÓ GIAO TIẾP WEB QUA SERIAL
 *
 *  SERIAL OUT: Gửi JSON mỗi 3 giây
 *  {"t":28.5,"h":65.0,"fan":false,"pump":true}
 *
 *  SERIAL IN: Nhận lệnh từ Node.js
 *  "FAN:ON" | "FAN:OFF" | "PUMP:ON" | "PUMP:OFF"
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

#define FAN_ON   HIGH   // Relay quạt: kích HIGH
#define FAN_OFF  LOW
#define PUMP_ON  LOW    // Relay bơm: kích LOW (ngược quạt)
#define PUMP_OFF HIGH

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

bool fanState  = false;
bool pumpState = false;
bool fanManual  = false; // true = điều khiển tay, false = tự động
bool pumpManual = false;

// =====================================================
void setFan(bool on) {
  fanState = on;
  digitalWrite(RELAY_PIN, on ? FAN_ON : FAN_OFF);
}

void setPump(bool on) {
  pumpState = on;
  digitalWrite(PUMP_PIN, on ? PUMP_ON : PUMP_OFF); // LOW=bật, HIGH=tắt
}

void sendStatus(float t, float h) {
  Serial.print("{\"t\":");
  Serial.print(t, 1);
  Serial.print(",\"h\":");
  Serial.print(h, 1);
  Serial.print(",\"fan\":");
  Serial.print(fanState ? "true" : "false");
  Serial.print(",\"pump\":");
  Serial.print(pumpState ? "true" : "false");
  Serial.println("}");
}

// =====================================================
void setup() {
  Serial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  setFan(false);
  setPump(false);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Khoi dong...  ");
  dht.begin();
  delay(3000); // DHT11 cần ít nhất 2-3 giây sau khi cấp nguồn
  lcd.clear();
}

// =====================================================
// Biến đếm thời gian bằng millis (không blocking)
unsigned long lastSensorTime = 0;
const unsigned long SENSOR_INTERVAL = 1000; // 1 giây
float lastT = NAN, lastH = NAN;

void loop() {
  // ── Đọc lệnh Serial liên tục (không bị chặn) ──
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    Serial.print("LENH NHAN: ["); Serial.print(cmd); Serial.println("]");
    if (cmd == "FAN:ON")   { fanManual  = true;  setFan(true);   Serial.println("-> Bat quat"); }
    if (cmd == "FAN:OFF")  { fanManual  = true;  setFan(false);  Serial.println("-> Tat quat"); }
    if (cmd == "PUMP:ON")  { pumpManual = true;  setPump(true);  Serial.println("-> Bat bom");  }
    if (cmd == "PUMP:OFF") { pumpManual = true;  setPump(false); Serial.println("-> Tat bom");  }
    if (cmd == "AUTO")     { fanManual  = false; pumpManual = false; Serial.println("-> Auto"); }
  }

  // ── Cập nhật cảm biến mỗi 1 giây ──
  unsigned long now = millis();
  if (now - lastSensorTime < SENSOR_INTERVAL) return;
  lastSensorTime = now;

  // Đọc DHT11 (không dùng delay trong retry)
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    // Dùng giá trị cũ nếu có, không vẽ lỗi lên LCD liên tục
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

  // ── Điều khiển tự động ──
  if (!fanManual)  setFan(t  < TEMP_THRESHOLD);
  if (!pumpManual) setPump(h < HUM_THRESHOLD);

  // ── Gửi JSON lên Node.js ──
  sendStatus(t, h);

  // ── LCD ──
  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print(t, 1);
  lcd.print((char)223); lcd.print("C ");
  lcd.print(fanState ? "QUAT:ON " : "QUAT:OFF");

  lcd.setCursor(0, 1);
  lcd.print("H:"); lcd.print(h, 1);
  lcd.print("% ");
  lcd.print(pumpState ? "BOM:ON  " : "BOM:OFF ");
}

