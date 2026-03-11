/*
 * =====================================================
 *  Arduino Mega 2560 — v4.1
 *  2 Relay riêng + DHT11 + LCD I2C
 *
 *  Nối dây:
 *    Pin 8  → Relay quạt (kích HIGH = bật)
 *    Pin 9  → Relay bơm  (kích LOW  = bật)
 *    Pin 7  → DHT11 DATA
 *    SDA/SCL → LCD I2C
 *
 *  Tương thích: USB + ESP32 cùng lúc
 * =====================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"

// ── Chân kết nối ──
#define DHTPIN         7
#define RELAY_PIN      8   // Quạt (relay riêng)
#define PUMP_PIN       9   // Bơm  (relay riêng)
#define DHTTYPE        DHT11

// ── Ngưỡng tự động ──
#define TEMP_THRESHOLD 20.0
#define HUM_THRESHOLD  75.0

// ── 2 relay riêng: quạt kích HIGH, bơm kích LOW ──
#define FAN_ON   HIGH
#define FAN_OFF  LOW
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

// Gửi JSON qua cả USB và ESP32
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

// Đọc lệnh Serial (không blocking)
void checkSerial(Stream &port, const char* tag) {
  if (!port.available()) return;
  char buf[20];
  int i = 0;
  unsigned long start = millis();
  while (millis() - start < 50 && i < 19) {
    if (port.available()) {
      char c = port.read();
      if (c == '\n' || c == '\r') { if (i > 0) break; continue; }
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

// Đọc DHT11 (retry 3 lần)
bool readDHT(float &t, float &h) {
  for (int i = 0; i < 3; i++) {
    h = dht.readHumidity();
    t = dht.readTemperature();
    if (!isnan(h) && !isnan(t) && t > -10 && t < 60 && h > 0 && h < 100)
      return true;
    delay(100);
  }
  return false;
}

// =====================================================
void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, FAN_OFF);
  digitalWrite(PUMP_PIN, PUMP_OFF);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Khoi dong...  ");
  lcd.setCursor(0, 1);
  lcd.print("  v4.1  2-Relay ");

  dht.begin();
  delay(3000);
  lcd.clear();

  lastFanChange  = millis();
  lastPumpChange = millis();

  Serial.println(F("Arduino v4.1 - 2 Relay"));
  Serial1.println(F("Arduino v4.1 - 2 Relay"));
}

// =====================================================
void loop() {
  checkSerial(Serial,  "[USB] ");
  checkSerial(Serial1, "[ESP] ");

  unsigned long now = millis();
  if (now - lastSensorTime < SENSOR_INTERVAL) return;
  lastSensorTime = now;

  float t, h;
  if (readDHT(t, h)) {
    lastT = t; lastH = h; dhtErrors = 0;
  } else {
    dhtErrors++;
    if (!isnan(lastT) && !isnan(lastH)) {
      t = lastT; h = lastH;  // Dùng giá trị cũ
    } else {
      t = 25.0; h = 50.0;    // Giá trị mặc định (relay vẫn chạy!)
    }
    if (dhtErrors % 5 == 1) { // Hiển thị lỗi nhưng KHÔNG dừng
      lcd.setCursor(0, 0); lcd.print("DHT11 loi! (#");
      lcd.print(dhtErrors); lcd.print(")  ");
      lcd.setCursor(0, 1); lcd.print("Van dieu khien  ");
    }
  }

  // Warmup 10 giây
  if (!warmupDone) {
    if (now < WARMUP_TIME) {
      lcd.setCursor(0, 0);
      lcd.print("On dinh nguon ");
      lcd.print((WARMUP_TIME - now) / 1000);
      lcd.print("s ");
      lcd.setCursor(0, 1);
      lcd.print("T:"); lcd.print(t, 1);
      lcd.print(" H:"); lcd.print(h, 1); lcd.print("%    ");
      sendStatus(t, h);
      return;
    }
    warmupDone = true;
    lastFanChange = now; lastPumpChange = now;
    Serial.println(F(">> Warmup xong!"));
  }

  // Tự động (chống giật 5s)
  if (!fanManual && (now - lastFanChange > RELAY_DEBOUNCE))
    setFan(t < TEMP_THRESHOLD);
  if (!pumpManual && (now - lastPumpChange > RELAY_DEBOUNCE))
    setPump(h < HUM_THRESHOLD);

  sendStatus(t, h);

  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print(t, 1);
  lcd.print((char)223); lcd.print("C ");
  lcd.print(fanState ? "QUAT:ON " : "QUAT:OFF");

  lcd.setCursor(0, 1);
  lcd.print("H:"); lcd.print(h, 1);
  lcd.print("% ");
  lcd.print(pumpState ? "BOM:ON  " : "BOM:OFF ");
}
