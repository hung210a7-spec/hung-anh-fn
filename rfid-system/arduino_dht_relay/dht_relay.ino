/*
 * =====================================================
 *  Arduino Mega 2560 + DHT11 + Relay x2
 *  PHIÊN BẢN ỔN ĐỊNH — CHỐNG RESET
 *
 *  Sửa lỗi:
 *  1. DHT11 đọc mỗi 3 giây (tối thiểu 2s theo datasheet)
 *  2. Retry 3 lần khi đọc DHT thất bại
 *  3. Không dùng String (tránh rò rỉ RAM)
 *  4. Relay chống giật (debounce 5 giây)
 *  5. Tương thích USB + ESP32 cùng lúc
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

float lastT = NAN, lastH = NAN;
int dhtErrorCount = 0;

unsigned long lastSensorTime = 0;
unsigned long lastFanChange  = 0;
unsigned long lastPumpChange = 0;

// Thời gian (ms)
const unsigned long SENSOR_INTERVAL  = 3000;  // DHT11 cần tối thiểu 2 giây
const unsigned long RELAY_DEBOUNCE   = 5000;  // Chống giật relay 5 giây
const unsigned long SERIAL_TIMEOUT   = 50;    // Timeout đọc serial

// Buffer gửi JSON (không dùng String)
char jsonBuf[100];

// =====================================================
void setFan(bool on) {
  if (fanState == on) return;  // Không làm gì nếu trạng thái không đổi
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

// Gửi JSON bằng char[] (không dùng String, tiết kiệm RAM)
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

// Đọc 1 lệnh từ Serial (có timeout, không blocking)
bool readCommand(Stream &port, char *buf, int maxLen) {
  if (!port.available()) return false;
  
  int i = 0;
  unsigned long start = millis();
  while (millis() - start < SERIAL_TIMEOUT && i < maxLen - 1) {
    if (port.available()) {
      char c = port.read();
      if (c == '\n' || c == '\r') {
        if (i > 0) break;  // Kết thúc lệnh
        continue;          // Bỏ qua ký tự rỗng đầu
      }
      buf[i++] = c;
    }
  }
  buf[i] = '\0';
  return i > 0;
}

// Xử lý lệnh
void handleCommand(const char *cmd) {
  if (strcmp(cmd, "FAN:ON")   == 0) { fanManual  = true;  setFan(true);   }
  if (strcmp(cmd, "FAN:OFF")  == 0) { fanManual  = true;  setFan(false);  }
  if (strcmp(cmd, "PUMP:ON")  == 0) { pumpManual = true;  setPump(true);  }
  if (strcmp(cmd, "PUMP:OFF") == 0) { pumpManual = true;  setPump(false); }
  if (strcmp(cmd, "AUTO")     == 0) { fanManual  = false; pumpManual = false; }
}

// Đọc DHT11 với retry
bool readDHT(float &t, float &h) {
  for (int retry = 0; retry < 3; retry++) {
    h = dht.readHumidity();
    t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      // Kiểm tra giá trị hợp lệ
      if (t > -10 && t < 60 && h > 0 && h < 100) {
        return true;
      }
    }
    delay(100);  // Chờ 100ms rồi thử lại
  }
  return false;
}

// =====================================================
void setup() {
  // Khởi tạo Serial
  Serial.begin(9600);
  Serial1.begin(9600);
  Serial.setTimeout(SERIAL_TIMEOUT);
  Serial1.setTimeout(SERIAL_TIMEOUT);

  // Khởi tạo relay ở trạng thái TẮT
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, FAN_OFF);
  digitalWrite(PUMP_PIN, PUMP_OFF);

  // Khởi tạo LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Khoi dong...  ");
  lcd.setCursor(0, 1);
  lcd.print("   v3.0 Stable  ");

  // Khởi tạo DHT11
  dht.begin();
  delay(3000);  // DHT11 cần 2-3 giây sau cấp nguồn
  lcd.clear();

  Serial.println(F("Arduino Mega v3.0 - On dinh"));
  Serial1.println(F("Arduino Mega v3.0 - On dinh"));
}

// =====================================================
void loop() {
  char cmdBuf[20];

  // ── Nhận lệnh từ USB ──
  if (readCommand(Serial, cmdBuf, sizeof(cmdBuf))) {
    Serial.print(F("[USB] ")); Serial.println(cmdBuf);
    handleCommand(cmdBuf);
  }

  // ── Nhận lệnh từ ESP32 ──
  if (readCommand(Serial1, cmdBuf, sizeof(cmdBuf))) {
    Serial.print(F("[ESP] ")); Serial.println(cmdBuf);
    handleCommand(cmdBuf);
  }

  // ── Đọc cảm biến mỗi 3 giây ──
  unsigned long now = millis();
  if (now - lastSensorTime < SENSOR_INTERVAL) return;
  lastSensorTime = now;

  float t, h;
  if (readDHT(t, h)) {
    // Đọc thành công
    lastT = t;
    lastH = h;
    dhtErrorCount = 0;
  } else {
    // Đọc thất bại
    dhtErrorCount++;
    if (!isnan(lastT) && !isnan(lastH) && dhtErrorCount < 10) {
      // Dùng giá trị cũ (tối đa 10 lần liên tiếp)
      t = lastT;
      h = lastH;
    } else {
      // Lỗi quá nhiều lần
      lcd.setCursor(0, 0); lcd.print("LOI DHT11! (#");
      lcd.print(dhtErrorCount); lcd.print(")  ");
      lcd.setCursor(0, 1); lcd.print("Kiem tra day    ");
      return;
    }
  }

  // ── Điều khiển tự động (có chống giật) ──
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
