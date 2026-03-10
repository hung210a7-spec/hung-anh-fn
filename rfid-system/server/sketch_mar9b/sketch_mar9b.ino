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
  digitalWrite(PUMP_PIN, on ? FAN_ON : FAN_OFF);
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
  delay(2000);
  lcd.clear();
}

// =====================================================
void loop() {
  // ── Đọc lệnh từ Serial (từ Node.js) ──
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "FAN:ON")   { fanManual  = true;  setFan(true);   }
    if (cmd == "FAN:OFF")  { fanManual  = true;  setFan(false);  }
    if (cmd == "PUMP:ON")  { pumpManual = true;  setPump(true);  }
    if (cmd == "PUMP:OFF") { pumpManual = true;  setPump(false); }
    if (cmd == "AUTO")     { fanManual  = false; pumpManual = false; }
  }

  // ── Đọc cảm biến ──
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    lcd.setCursor(0, 0);
    lcd.print("LOI cam bien!   ");
    lcd.setCursor(0, 1);
    lcd.print("Kiem tra DHT11  ");
    delay(2000);
    return;
  }

  // ── Điều khiển tự động (chỉ khi không ở chế độ tay) ──
  if (!fanManual) {
    setFan(t < TEMP_THRESHOLD);
  }
  if (!pumpManual) {
    setPump(h < HUM_THRESHOLD);
  }

  // ── Gửi JSON lên Node.js ──
  sendStatus(t, h);

  // ── LCD ──
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(t, 1);
  lcd.print((char)223);
  lcd.print("C ");
  lcd.print(fanState ? "QUAT:ON " : "QUAT:OFF");

  lcd.setCursor(0, 1);
  lcd.print("H:");
  lcd.print(h, 1);
  lcd.print("% ");
  lcd.print(pumpState ? "BOM:ON  " : "BOM:OFF ");

  delay(3000);
}