// ESP32 RFID Access Control - Core V1 (Firebase Firestore)
// Tính năng: Đọc thẻ RC522 -> Hiện UID -> Gửi Log lên Firestore

#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ---------------- 1. CẤU HÌNH WIFI & FIREBASE ----------------
#define WIFI_SSID     "Hung Minh"
#define WIFI_PASSWORD "23456789"

#define API_KEY       "AIzaSyBoVEofW3IdbppeDdP9ksiIoA_zac0zS5U"
#define PROJECT_ID    "hunganh-doam1" // Trích xuất từ URL Database cũ
// Dùng Test Mode nên mượn tạm Account ẩn danh hoặc bỏ qua auth
#define USER_EMAIL    ""
#define USER_PASSWORD ""

// ---------------- 2. CẤU HÌNH CHÂN RC522 ----------------
#define SS_PIN  5   // SDA
#define RST_PIN 4   // RST
// Chân SPI mặc định (SCK=18, MISO=19, MOSI=23) đã nối theo sơ đồ 38-pin

MFRC522 rfid(SS_PIN, RST_PIN);

// ---------------- 3. KHỞI TẠO FIREBASE ----------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Biến lưu thời gian
unsigned long lastSwipeTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // 1. Khởi tạo SPI và RC522
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("\n[RC522] Kiem tra phien ban Firmware:");
  rfid.PCD_DumpVersionToSerial();
  Serial.println("[RC522] Dã san sang doc the...");

  // 2. Kết nối WiFi
  Serial.print("[WiFi] Dang ket noi mang: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Da ket noi! IP: " + WiFi.localIP().toString());

  // 3. Khởi tạo Firebase Firestore
  config.api_key = API_KEY;
  // Bật Test Mode để bỏ qua đăng nhập Auth (giống bên Realtime DB)
  config.signer.test_mode = true;

  // Cấu trúc Firestore Payload
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("[Firebase] Da khoi tao. Xin moi quet the!");
}

void loop() {
  if (millis() - lastSwipeTime < 3000) return;

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  lastSwipeTime = millis();

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if(rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  Serial.println("\n============================");
  Serial.print(">> The vua quet UID: ");
  Serial.println(uid);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  sendLogToFirestore(uid);
}

void sendLogToFirestore(String uidStr) {
  if (Firebase.ready()) {
    Serial.println("[Firestore] Dang gui log le mang...");
    String documentPath = "logs/"; 
    FirebaseJson content;
    
    content.set("fields/uid/stringValue", uidStr);
    
    String userName = "Khách Lạ";
    if (uidStr == "D3BF320A" || uidStr == "A1B2C3D4") { 
      userName = "Hung Anh";
    }
    content.set("fields/name/stringValue", userName);
    content.set("fields/status/stringValue", "Da Quet The");
    content.set("fields/timestamp/stringValue", "SERVER_TIMESTAMP_LATER"); 

    if (Firebase.Firestore.createDocument(&fbdo, PROJECT_ID, "", documentPath.c_str(), content.raw())) {
        Serial.printf(">> GHI THANH CONG! Nguoi quet: %s\n", userName.c_str());
    } else {
        Serial.println(">> LAI DOC ERROR: " + fbdo.errorReason());
    }
  } else {
    Serial.println("[Lỗi] Firebase chua san sang, ko the gui log.");
  }
}
