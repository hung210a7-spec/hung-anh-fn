/*
 * =====================================================
 *  Arduino Mega 2560 — v4.2 Realtime Sync
 *  2 Relay riêng + DHT11 + LCD I2C
 *
 *  Nối dây:
 *    Pin 8  → Relay quạt (kích HIGH = bật)
 *    Pin 9  → Relay bơm  (kích HIGH = bật)
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

// ── 2 relay riêng: cả 2 đều kích HIGH = bật ──
#define FAN_ON   HIGH
#define FAN_OFF  LOW
#define PUMP_ON  HIGH
#define PUMP_OFF LOW

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

// Đọc lệnh Serial (dùng String cho an toàn & dễ xử lý lệnh ngắn)
void checkSerial(Stream &port, const char* tag) {
  if (!port.available()) return;
  
  String cmd = port.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  Serial.print(tag); Serial.println(cmd);

  // Không cho phép điều khiển nếu đang trong thời gian warmup (để chống reset)
  if (!warmupDone) {
    Serial.println(F("Dang warmup, tu choi lenh!"));
    return;
  }

  bool changed = false;
  if (cmd == "FAN:ON")   { fanManual  = true;  setFan(true);   changed = true; }
  if (cmd == "FAN:OFF")  { fanManual  = true;  setFan(false);  changed = true; }
  if (cmd == "PUMP:ON")  { pumpManual = true;  setPump(true);  changed = true; }
  if (cmd == "PUMP:OFF") { pumpManual = true;  setPump(false); changed = true; }
  if (cmd == "AUTO")     { fanManual  = false; pumpManual = false; changed = true; }

  // Gửi trạng thái ngay lập tức để web cập nhật tức thời (thời gian thực)!
  if (changed) {
    sendStatus(lastT, lastH);
  }
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
  lcd.print("  v4.2  2-Relay ");

  dht.begin();
  delay(3000);
  lcd.clear();

  lastFanChange  = millis();
  lastPumpChange = millis();

  Serial.println(F("Arduino v4.2 - Thay doi tuc thi"));
  Serial1.println(F("Arduino v4.2 - Thay doi tuc thi"));
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
