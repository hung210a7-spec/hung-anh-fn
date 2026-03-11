/*
 * =====================================================
 *  ESP32 — Cầu nối WiFi/Firebase cho Arduino Mega
 *
 *  Arduino gửi JSON qua Serial1 (Pin18/19)
 *    → ESP32 nhận qua Serial2 (GPIO16/17)
 *    → Đẩy lên Firebase
 *
 *  Web gửi lệnh qua Firebase
 *    → ESP32 nhận
 *    → Gửi cho Arduino qua Serial2
 *
 *  KHÔNG CẦN MÁY TÍNH SAU KHI NẠP CODE!
 * =====================================================
 */

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/RTDBHelper.h>
#include <ArduinoJson.h>

// =====================================================
//  ⚙️ WiFi
// =====================================================
#define WIFI_SSID     "4G-UFI-0486"
#define WIFI_PASSWORD "1234567890"


// =====================================================
//  🔥 Firebase
// =====================================================
#define API_KEY       "AIzaSyBoVEofW3IdbppeDdP9ksiIoA_zac0zS5U"
#define DATABASE_URL  "https://hunganh-doam1-default-rtdb.asia-southeast1.firebasedatabase.app"

// =====================================================
//  📡 Serial2 — Giao tiếp Arduino qua dây nối trực tiếp
//     ESP32 GPIO16 (RX) ← Arduino Pin18 (TX1)
//     ESP32 GPIO17 (TX) → Arduino Pin19 (RX1)
// =====================================================
#define RXD2 16
#define TXD2 17

// =====================================================
FirebaseData fbdo;
FirebaseData streamFbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 2000;

String lastCmdId = "";
bool firebaseOK = false;

// =====================================================
void setup() {
  Serial.begin(115200);                          // Debug USB
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);  // UART Arduino

  Serial.println("\n=============================");
  Serial.println(" ESP32 Firebase Bridge v2");
  Serial.println("=============================");

  // ── WiFi ──
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi: " + WiFi.SSID());
    Serial.println("IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi THAT BAI!");
    return;
  }

  // ── Firebase (Test Mode — No Auth) ──
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.signer.test_mode = true;

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);

  firebaseOK = true;
  Serial.println("Firebase: OK!");

  // ── Lắng nghe lệnh /control ──
  if (Firebase.RTDB.beginStream(&streamFbdo, "/control")) {
    Serial.println("Dang lang nghe lenh tu web...");
  } else {
    Serial.println("Stream loi: " + streamFbdo.errorReason());
  }

  Serial.println("=============================\n");
}

// =====================================================
void loop() {
  if (!firebaseOK) return;

  // ── 1. Nhận JSON từ Arduino → Đẩy lên Firebase ──
  if (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();

    if (line.startsWith("{") && millis() - lastSendTime > SEND_INTERVAL) {
      Serial.println("Arduino: " + line);

      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, line);

      if (!err) {
        FirebaseJson json;
        json.set("t", doc["t"].as<float>());
        json.set("h", doc["h"].as<float>());
        json.set("fan", doc["fan"].as<bool>());
        json.set("pump", doc["pump"].as<bool>());
        json.set("updatedAt", (unsigned long)millis());

        if (Firebase.RTDB.setJSON(&fbdo, "/sensor", &json)) {
          Serial.println("-> Firebase OK");
        } else {
          Serial.println("-> Firebase loi: " + fbdo.errorReason());
        }
        lastSendTime = millis();
      }
    }
  }

  // ── 2. Nhận lệnh từ Firebase → Gửi cho Arduino ──
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
            Serial2.println(cmd);       // Gửi cho Arduino
            Serial.println("Web -> Arduino: " + cmd);
          }
        }
      }
    }
  } else {
    Serial.println("Stream mat, thu lai...");
    Firebase.RTDB.beginStream(&streamFbdo, "/control");
  }

  // ── 3. Kiểm tra WiFi ──
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi mat!");
    WiFi.reconnect();
    delay(3000);
  }
}

