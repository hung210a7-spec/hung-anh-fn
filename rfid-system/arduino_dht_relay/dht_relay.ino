/*
 * =====================================================
 *  Arduino Mega 2560 + DHT11 + Relay x2
 *  GIAO TIẾP VỚI ESP32 QUA Serial1 (Pin18 TX, Pin19 RX)
 *
 *  Serial1 OUT → ESP32: Gửi JSON mỗi 1 giây
 *  {\"t\":28.5,\"h\":65.0,\"fan\":false,\"pump\":true}
 *
 *  Serial1 IN ← ESP32: Nhận lệnh từ web
 *  "FAN:ON" | "FAN:OFF" | "PUMP:ON" | "PUMP:OFF" | "AUTO"
 *
 *  Serial (USB) dùng để debug khi cắm máy tính (tùy chọn)
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

// Gửi JSON qua Serial1 → ESP32
void sendStatus(float t, float h) {
  Serial1.print("{\"t\":");
  Serial1.print(t, 1);
  Serial1.print(",\"h\":");
  Serial1.print(h, 1);
  Serial1.print(",\"fan\":");
  Serial1.print(fanState ? "true" : "false");
  Serial1.print(",\"pump\":");
  Serial1.print(pumpState ? "true" : "false");
  Serial1.println("}");
}

// =====================================================
void setup() {
  Serial.begin(9600);    // USB debug (tùy chọn)
  Serial1.begin(9600);   // Giao tiếp ESP32 (Pin18=TX1, Pin19=RX1)

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

  Serial.println("Arduino Mega san sang!");
  Serial.println("Giao tiep ESP32 qua Serial1 (Pin18/19)");
}

// =====================================================
unsigned long lastSensorTime = 0;
const unsigned long SENSOR_INTERVAL = 1000;
float lastT = NAN, lastH = NAN;

void loop() {
  // ── Đọc lệnh từ ESP32 qua Serial1 ──
  if (Serial1.available() > 0) {
    String cmd = Serial1.readStringUntil('\n');
    cmd.trim();
    Serial.print("ESP32 gui: ["); Serial.print(cmd); Serial.println("]");

    if (cmd == "FAN:ON")   { fanManual  = true;  setFan(true);   }
    if (cmd == "FAN:OFF")  { fanManual  = true;  setFan(false);  }
    if (cmd == "PUMP:ON")  { pumpManual = true;  setPump(true);  }
    if (cmd == "PUMP:OFF") { pumpManual = true;  setPump(false); }
    if (cmd == "AUTO")     { fanManual  = false; pumpManual = false; }
  }

  // ── Đọc lệnh từ USB debug (tùy chọn, khi cắm máy tính) ──
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "FAN:ON")   { fanManual  = true;  setFan(true);   }
    if (cmd == "FAN:OFF")  { fanManual  = true;  setFan(false);  }
    if (cmd == "PUMP:ON")  { pumpManual = true;  setPump(true);  }
    if (cmd == "PUMP:OFF") { pumpManual = true;  setPump(false); }
    if (cmd == "AUTO")     { fanManual  = false; pumpManual = false; }
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

  // Gửi JSON → ESP32
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
