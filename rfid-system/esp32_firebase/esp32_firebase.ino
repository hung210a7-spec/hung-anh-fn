/*
 * =====================================================
 *  ESP32 — Cầu nối WiFi/Firebase cho Arduino Mega
 * 
 *  Chức năng:
 *  1. Nhận JSON từ Arduino qua Serial2 (GPIO16/17)
 *  2. Đẩy dữ liệu cảm biến lên Firebase Realtime DB
 *  3. Nhận lệnh điều khiển từ Firebase → gửi cho Arduino
 *
 *  Sau khi nạp code, ESP32 hoạt động độc lập không cần PC
 * =====================================================
 */

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/RTDBHelper.h>
#include <ArduinoJson.h>

// =====================================================
//  ⚙️ CẤU HÌNH WiFi — SỬA THÔNG TIN WiFi CỦA BẠN
// =====================================================
#define WIFI_SSID     "Hung Minh"
#define WIFI_PASSWORD "23456789"

// =====================================================
//  🔥 Firebase — Lấy từ Firebase Console
// =====================================================
#define API_KEY       "AIzaSyBoVEofW3IdbppeDdP9ksiIoA_zac0zS5U"
#define DATABASE_URL  "https://hunganh-doam1-default-rtdb.asia-southeast1.firebasedatabase.app"

// =====================================================
//  📡 Serial2 — Giao tiếp với Arduino qua TXS0108E
// =====================================================
#define RXD2 16  // ESP32 RX ← TXS A1 ← Arduino TX1
#define TXD2 17  // ESP32 TX → TXS A2 → Arduino RX1

// =====================================================
FirebaseData fbdo;
FirebaseData streamFbdo;  // Dùng riêng cho stream lệnh
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1000; // Gửi Firebase mỗi 1 giây

String lastCmdId = "";
bool firebaseReady = false;

// =====================================================
void setup() {
  Serial.begin(115200);   // Debug qua USB
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // UART với Arduino

  Serial.println("\n=============================");
  Serial.println(" ESP32 Firebase Bridge");
  Serial.println("=============================");

  // ── Kết nối WiFi ──
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Dang ket noi WiFi");
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 30) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi: " + WiFi.SSID());
    Serial.println("📶 IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n❌ WiFi THAT BAI! Kiem tra ten/mat khau WiFi");
    return;
  }

  // ── Kết nối Firebase ──
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Đăng nhập ẩn danh (bắt buộc với thư viện này dù Rule là public)
  Serial.print("Dang dang nhap Firebase... ");
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("OK!");
    firebaseReady = true;
  } else {
    Serial.print("Loi: ");
    Serial.println(config.signer.signupError.message.c_str());
    return;
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);

  // ── Lắng nghe lệnh từ Firebase /control ──
  if (!Firebase.RTDB.beginStream(&streamFbdo, "/control")) {
    Serial.println("❌ Stream loi: " + streamFbdo.errorReason());
  } else {
    Serial.println("📡 Dang lang nghe lenh tu web...");
  }

  Serial.println("=============================\n");
}

// =====================================================
void loop() {
  if (!firebaseReady) return;

  // ── 1. Đọc JSON từ Arduino (qua Serial2) ──
  if (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();
    
    if (line.startsWith("{")) {
      Serial.println("📥 Arduino: " + line);
      
      // Parse JSON
      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, line);
      
      if (!err && millis() - lastSendTime > SEND_INTERVAL) {
        // Đẩy lên Firebase /sensor
        FirebaseJson json;
        json.set("t", doc["t"].as<float>());
        json.set("h", doc["h"].as<float>());
        json.set("fan", doc["fan"].as<bool>());
        json.set("pump", doc["pump"].as<bool>());
        json.set("updatedAt", (unsigned long)millis());

        if (Firebase.RTDB.setJSON(&fbdo, "/sensor", &json)) {
          Serial.println("✅ Firebase: Da gui du lieu");
        } else {
          Serial.println("❌ Firebase gui loi: " + fbdo.errorReason());
        }
        lastSendTime = millis();
      }
    }
  }

  // ── 2. Nhận lệnh từ Firebase → Gửi cho Arduino ──
  if (Firebase.RTDB.readStream(&streamFbdo)) {
    if (streamFbdo.streamAvailable()) {
      // Đọc dữ liệu từ /control
      FirebaseJson json;
      FirebaseJsonData cmdData, tsData, idData;
      
      if (streamFbdo.dataType() == "json") {
        json = streamFbdo.jsonObject();
        
        json.get(cmdData, "cmd");
        json.get(tsData, "ts");
        json.get(idData, "id");
        
        if (cmdData.success) {
          String cmd = cmdData.stringValue;
          String cmdId = idData.success ? idData.stringValue : tsData.stringValue;
          
          // Chỉ xử lý lệnh mới
          if (cmdId != lastCmdId && cmd.length() > 0) {
            lastCmdId = cmdId;
            
            // Gửi lệnh xuống Arduino qua Serial2
            Serial2.println(cmd);
            Serial.println("📤 Gui Arduino: " + cmd);
          }
        }
      }
    }
  } else {
    Serial.println("❌ Stream loi: " + streamFbdo.errorReason());
    // Thử kết nối lại stream
    if (!Firebase.RTDB.beginStream(&streamFbdo, "/control")) {
      Serial.println("❌ Reconnect stream loi");
    }
  }

  // ── 3. Kiểm tra WiFi ──
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi mat ket noi! Dang ket noi lai...");
    WiFi.reconnect();
    delay(3000);
  }
}
