/*
 * =====================================================
 *  Arduino Mega 2560 — v4.0 Anti-Reset
 *  Module 4 Relay (Active LOW) + DHT11 + LCD I2C
 *
 *  Nối dây:
 *    Pin 8  → IN3 relay (Quạt)   | COM3 ← (+)Nguồn | NO3 → (+)Quạt
 *    Pin 9  → IN2 relay (Bơm)    | COM2 ← (+)Nguồn | NO2 → (+)Bơm
 *    Pin 7  → DHT11 DATA
 *    SDA/SCL → LCD I2C
 *    (-)Quạt → (-)Nguồn
 *    (-)Bơm  → (-)Nguồn
 *
 *  Tương thích: USB + ESP32 cùng lúc
 * =====================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"

// ── Chân kết nối ──
#define DHTPIN         7
#define RELAY_PIN      8   // Quạt → IN3
#define PUMP_PIN       9   // Bơm  → IN2
#define DHTTYPE        DHT11

// ── Ngưỡng tự động ──
#define TEMP_THRESHOLD 20.0
#define HUM_THRESHOLD  75.0

// ── Module 4 relay: Active LOW (IN=LOW → bật) ──
#define FAN_ON   LOW
#define FAN_OFF  HIGH
#define PUMP_ON  LOW
#define PUMP_OFF HIGH

// ── Thời gian (ms) ──
#define SENSOR_INTERVAL  3000   // Đọc DHT mỗi 3 giây
#define RELAY_DEBOUNCE   5000   // Chống giật relay 5 giây
#define WARMUP_TIME      10000  // 10 giây chờ ổn định nguồn

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

bool fanState   = false;
bool pumpState  = false;
bool fanManual  = false;
bool pumpManual = false;
bool warmupDone = false;

float lastT = NAN, lastH = NAN;
int   dhtErrors = 0;

unsigned long lastSensorTime = 0;
unsigned long lastFanChange  = 0;
unsigned long lastPumpChange = 0;

char jsonBuf[100];

// =====================================================
//  Điều khiển relay
// =====================================================
void setFan(bool on) {
  if (fanState == on) return;
  fanState = on;
  digitalWrite(RELAY_PIN, on ? FAN_ON : FAN_OFF);
  lastFanChange = millis();
}

void setPump(bool on) {
  if (pumpState == on) return;
  pumpState = on;
  digitalWrite(PUMP_PIN, on ? PUMP_ON : PUMP_OFF);
  lastPumpChange = millis();
}

// =====================================================
//  Gửi JSON qua cả USB và ESP32
// =====================================================
void sendStatus(float t, float h) {
  snprintf(jsonBuf, sizeof(jsonBuf),
    "{\"t\":%.1f,\"h\":%.1f,\"fan\":%s,\"pump\":%s}",
    t, h,
    fanState  ? "true" : "false",
    pumpState ? "true" : "false"
  );
  Serial.println(jsonBuf);
  Serial1.println(jsonBuf);
}

// =====================================================
//  Đọc lệnh Serial (không blocking)
// =====================================================
void checkSerial(Stream &port, const char* tag) {
  if (!port.available()) return;
  
  char buf[20];
  int i = 0;
  unsigned long start = millis();
  
  while (millis() - start < 50 && i < 19) {
    if (port.available()) {
      char c = port.read();
      if (c == '\n' || c == '\r') {
        if (i > 0) break;
        continue;
      }
      buf[i++] = c;
    }
  }
  buf[i] = '\0';
  if (i == 0) return;

  Serial.print(tag); Serial.println(buf);

  if (strcmp(buf, "FAN:ON")   == 0) { fanManual  = true;  setFan(true);   }
  if (strcmp(buf, "FAN:OFF")  == 0) { fanManual  = true;  setFan(false);  }
  if (strcmp(buf, "PUMP:ON")  == 0) { pumpManual = true;  setPump(true);  }
  if (strcmp(buf, "PUMP:OFF") == 0) { pumpManual = true;  setPump(false); }
  if (strcmp(buf, "AUTO")     == 0) { fanManual  = false; pumpManual = false; }
}

// =====================================================
//  Đọc DHT11 (retry 3 lần)
// =====================================================
bool readDHT(float &t, float &h) {
  for (int i = 0; i < 3; i++) {
    h = dht.readHumidity();
    t = dht.readTemperature();
    if (!isnan(h) && !isnan(t) && t > -10 && t < 60 && h > 0 && h < 100) {
      return true;
    }
    delay(100);
  }
  return false;
}

// =====================================================
//  SETUP
// =====================================================
void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);

  // Relay TẮT ngay lập tức
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, FAN_OFF);   // HIGH = tắt
  digitalWrite(PUMP_PIN, PUMP_OFF);   // HIGH = tắt

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Khoi dong...  ");
  lcd.setCursor(0, 1);
  lcd.print(" v4.0 Anti-Reset");

  // DHT
  dht.begin();
  delay(3000);
  lcd.clear();

  // Đặt thời điểm relay = bây giờ (chống bật ngay)
  lastFanChange  = millis();
  lastPumpChange = millis();

  Serial.println(F("Arduino v4.0 Anti-Reset"));
  Serial1.println(F("Arduino v4.0 Anti-Reset"));
}

// =====================================================
//  LOOP
// =====================================================
void loop() {
  // ── Nhận lệnh ──
  checkSerial(Serial,  "[USB] ");
  checkSerial(Serial1, "[ESP] ");

  // ── Đọc cảm biến mỗi 3 giây ──
  unsigned long now = millis();
  if (now - lastSensorTime < SENSOR_INTERVAL) return;
  lastSensorTime = now;

  float t, h;
  if (readDHT(t, h)) {
    lastT = t;
    lastH = h;
    dhtErrors = 0;
  } else {
    dhtErrors++;
    if (!isnan(lastT) && !isnan(lastH) && dhtErrors < 10) {
      t = lastT;
      h = lastH;
    } else {
      lcd.setCursor(0, 0); lcd.print("LOI DHT11! (#");
      lcd.print(dhtErrors); lcd.print(")  ");
      lcd.setCursor(0, 1); lcd.print("Kiem tra day    ");
      return;
    }
  }

  // ── Warmup 10 giây (không bật relay) ──
  if (!warmupDone) {
    if (now < WARMUP_TIME) {
      lcd.setCursor(0, 0);
      lcd.print("On dinh nguon ");
      lcd.print((WARMUP_TIME - now) / 1000);
      lcd.print("s ");
      lcd.setCursor(0, 1);
      lcd.print("T:"); lcd.print(t, 1);
      lcd.print(" H:"); lcd.print(h, 1);
      lcd.print("%    ");
      sendStatus(t, h);
      return;
    }
    warmupDone = true;
    lastFanChange  = now;
    lastPumpChange = now;
    Serial.println(F(">> Warmup xong!"));
  }

  // ── Điều khiển tự động (chống giật 5s) ──
  if (!fanManual && (now - lastFanChange > RELAY_DEBOUNCE)) {
    setFan(t < TEMP_THRESHOLD);
  }
  if (!pumpManual && (now - lastPumpChange > RELAY_DEBOUNCE)) {
    setPump(h < HUM_THRESHOLD);
  }

  // ── Gửi JSON ──
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
