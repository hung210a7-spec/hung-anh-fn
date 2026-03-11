/*
 * =====================================================
 *  ESP32 ALL-IN-ONE — Thay thế hoàn toàn Arduino Mega
 * 
 *  Chức năng:
 *  1. Đọc DHT11 (nhiệt độ + độ ẩm)
 *  2. Điều khiển 2 relay (quạt + bơm)
 *  3. Hiển thị LCD I2C 16x2
 *  4. Kết nối WiFi + Firebase (không cần USB/PC)
 *  5. Nhận lệnh từ web và điều khiển thiết bị
 *
 *  => CHỈ CẦN 1 BOARD ESP32, KHÔNG CẦN ARDUINO NỮA!
 * =====================================================
 */

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/RTDBHelper.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"

// =====================================================
//  ⚙️ CẤU HÌNH WiFi
// =====================================================
#define WIFI_SSID     "Hung Minh"
#define WIFI_PASSWORD "23456789"

// =====================================================
//  🔥 Firebase
// =====================================================
#define API_KEY       "AIzaSyBoVEofW3IdbppeDdP9ksiIoA_zac0zS5U"
#define DATABASE_URL  "https://hunganh-doam1-default-rtdb.asia-southeast1.firebasedatabase.app"

// =====================================================
//  📌 CHÂN KẾT NỐI ESP32
// =====================================================
//  ESP32 dùng GPIO khác Arduino Mega!
//  Bạn chỉ cần nối dây theo bảng bên dưới:
// 
//  DHT11:
//    VCC → 3.3V,  GND → GND,  DATA → GPIO4
//
//  Relay Quạt:
//    VCC → VIN(5V), GND → GND, IN → GPIO26
//
//  Relay Bơm:
//    VCC → VIN(5V), GND → GND, IN → GPIO27
//
//  LCD I2C:
//    VCC → VIN(5V), GND → GND, SDA → GPIO21, SCL → GPIO22
// =====================================================

#define DHTPIN         4     // DHT11 DATA
#define RELAY_PIN      26    // Quạt
#define PUMP_PIN       27    // Bơm
#define DHTTYPE        DHT11
#define TEMP_THRESHOLD 20.0
#define HUM_THRESHOLD  75.0

#define FAN_ON   HIGH   // Relay quạt: kích HIGH
#define FAN_OFF  LOW
#define PUMP_ON  LOW    // Relay bơm: kích LOW (ngược quạt)
#define PUMP_OFF HIGH

// =====================================================
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Firebase
FirebaseData fbdo;
FirebaseData streamFbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Trạng thái
bool fanState    = false;
bool pumpState   = false;
bool fanManual   = false;
bool pumpManual  = false;
bool firebaseOK  = false;

// Timing
unsigned long lastSensorTime  = 0;
unsigned long lastFirebaseTime = 0;
const unsigned long SENSOR_INTERVAL   = 1000;  // Đọc cảm biến mỗi 1 giây
const unsigned long FIREBASE_INTERVAL = 2000;  // Gửi Firebase mỗi 2 giây

String lastCmdId = "";
float lastT = NAN, lastH = NAN;

// =====================================================
//  Hàm điều khiển quạt / bơm
// =====================================================
void setFan(bool on) {
  fanState = on;
  digitalWrite(RELAY_PIN, on ? FAN_ON : FAN_OFF);
}

void setPump(bool on) {
  pumpState = on;
  digitalWrite(PUMP_PIN, on ? PUMP_ON : PUMP_OFF);
}

// =====================================================
//  Xử lý lệnh từ Firebase
// =====================================================
void handleCommand(String cmd) {
  Serial.println("📥 Lenh: " + cmd);
  
  if (cmd == "FAN:ON")   { fanManual  = true;  setFan(true);   Serial.println("-> Bat quat"); }
  if (cmd == "FAN:OFF")  { fanManual  = true;  setFan(false);  Serial.println("-> Tat quat"); }
  if (cmd == "PUMP:ON")  { pumpManual = true;  setPump(true);  Serial.println("-> Bat bom");  }
  if (cmd == "PUMP:OFF") { pumpManual = true;  setPump(false); Serial.println("-> Tat bom");  }
  if (cmd == "AUTO")     { fanManual  = false; pumpManual = false; Serial.println("-> Auto"); }
}

// =====================================================
void setup() {
  Serial.begin(115200);
  
  // ── GPIO ──
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  setFan(false);
  setPump(false);

  // ── LCD ──
  Wire.begin(21, 22);  // SDA=21, SCL=22 (mặc định ESP32)
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  ESP32 v1.0    ");
  lcd.setCursor(0, 1);
  lcd.print("  Khoi dong...  ");

  // ── DHT11 ──
  dht.begin();
  delay(2000);

  Serial.println("\n=============================");
  Serial.println(" ESP32 All-in-One");
  Serial.println("=============================");

  // ── WiFi ──
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ket noi WiFi...");
  
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 30) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi: " + WiFi.SSID());
    Serial.println("📶 IP: " + WiFi.localIP().toString());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi: OK!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());
    delay(1500);
  } else {
    Serial.println("\n❌ WiFi THAT BAI!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi: LOI!");
    lcd.setCursor(0, 1);
    lcd.print("Kiem tra mk WiFi");
    // Vẫn chạy offline (chỉ cảm biến + relay + LCD)
  }

  // ── Firebase ──
  if (WiFi.status() == WL_CONNECTED) {
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.signer.test_mode = true;

    Firebase.begin(&config, &auth);
    Firebase.reconnectNetwork(true);
    
    firebaseOK = true;
    Serial.println("✅ Firebase: OK!");

    // Lắng nghe lệnh từ web (/control)
    if (!Firebase.RTDB.beginStream(&streamFbdo, "/control")) {
      Serial.println("❌ Stream loi: " + streamFbdo.errorReason());
    } else {
      Serial.println("📡 Dang lang nghe lenh tu web...");
    }
  }

  lcd.clear();
  Serial.println("=============================\n");
}

// =====================================================
void loop() {
  unsigned long now = millis();

  // ── 1. Nhận lệnh từ Firebase ──
  if (firebaseOK) {
    if (Firebase.RTDB.readStream(&streamFbdo)) {
      if (streamFbdo.streamAvailable()) {
        if (streamFbdo.dataType() == "json") {
          FirebaseJson json = streamFbdo.jsonObject();
          FirebaseJsonData cmdData, idData;
          
          json.get(cmdData, "cmd");
          json.get(idData, "id");
          
          if (cmdData.success) {
            String cmd = cmdData.stringValue;
            String cmdId = idData.success ? idData.stringValue : String(millis());
            
            if (cmdId != lastCmdId && cmd.length() > 0) {
              lastCmdId = cmdId;
              handleCommand(cmd);
            }
          }
        }
      }
    } else {
      Serial.println("⚠️ Stream mat ket noi, dang thu lai...");
      Firebase.RTDB.beginStream(&streamFbdo, "/control");
    }
  }

  // ── 2. Đọc cảm biến mỗi 1 giây ──
  if (now - lastSensorTime >= SENSOR_INTERVAL) {
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
    if (!fanManual)  setFan(t < TEMP_THRESHOLD);
    if (!pumpManual) setPump(h < HUM_THRESHOLD);

    // LCD
    lcd.setCursor(0, 0);
    lcd.print("T:"); lcd.print(t, 1);
    lcd.print((char)223); lcd.print("C ");
    lcd.print(fanState ? "QUAT:ON " : "QUAT:OFF");

    lcd.setCursor(0, 1);
    lcd.print("H:"); lcd.print(h, 1);
    lcd.print("% ");
    lcd.print(pumpState ? "BOM:ON  " : "BOM:OFF ");

    // ── 3. Gửi lên Firebase mỗi 2 giây ──
    if (firebaseOK && now - lastFirebaseTime >= FIREBASE_INTERVAL) {
      lastFirebaseTime = now;

      FirebaseJson json;
      json.set("t", t);
      json.set("h", h);
      json.set("fan", fanState);
      json.set("pump", pumpState);
      json.set("updatedAt", (unsigned long)now);

      if (Firebase.RTDB.setJSON(&fbdo, "/sensor", &json)) {
        Serial.print("📤 T:"); Serial.print(t);
        Serial.print(" H:"); Serial.print(h);
        Serial.print(" Q:"); Serial.print(fanState);
        Serial.print(" B:"); Serial.println(pumpState);
      } else {
        Serial.println("❌ Firebase loi: " + fbdo.errorReason());
      }
    }
  }

  // ── 4. Kiểm tra WiFi ──
  if (WiFi.status() != WL_CONNECTED && firebaseOK) {
    Serial.println("⚠️ WiFi mat ket noi!");
    lcd.setCursor(0, 0);
    lcd.print("WiFi: MAT!      ");
    WiFi.reconnect();
    delay(3000);
  }
}
