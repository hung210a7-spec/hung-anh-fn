// ESP32 RFID SMART ACCESS - PHIÊN BẢN ỔN ĐỊNH 100%
// Sử dụng chân D5, D4 để tránh lỗi Reset 0x8
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// 1. CẤU HÌNH WIFI & FIREBASE (Hùng Anh)
#define WIFI_SSID     "Hung Minh"
#define WIFI_PASSWORD "23456789"
#define API_KEY       "AIzaSyBoVEofW3IdbppeDdP9ksiIoA_zac0zS5U"
#define PROJECT_ID    "hunganh-doam1"

// 2. CẤU HÌNH CHÂN "AN TOÀN" (Dùng chân D5 và D4)
#define SS_PIN    5   // Nối vào SDA trên RC522
#define RST_PIN   4   // Nối vào RST trên RC522
#define SCK_PIN   18  // Nối vào SCK
#define MISO_PIN  19  // Nối vào MISO
#define MOSI_PIN  23  // Nối vào MOSI

MFRC522 rfid(SS_PIN, RST_PIN);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long lastSwipeTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Khởi tạo SPI với chân tường minh
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();
  
  Serial.println("\n[RC522] Kiem tra ket noi...");
  rfid.PCD_DumpVersionToSerial(); // Neu in ra 0x92 la OK

  // Kết nối WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Dang ket noi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Da ket noi! IP: " + WiFi.localIP().toString());

  // Khởi tạo Firebase Firestore (Test Mode)
  config.api_key = API_KEY;
  config.signer.test_mode = true;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("[Firebase] San sang. Xin moi quet the!");
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
  Serial.println(">> THE VUA QUET: " + uid);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  // Gửi LOG lên Firestore
  if (Firebase.ready()) {
    String documentPath = "logs/"; 
    FirebaseJson content;
    content.set("fields/uid/stringValue", uid);
    
    // Tạm thời set tên Admin nếu quẹt trúng thẻ của bạ
    String userName = "Khach La";
    if (uid == "D3BF320A" || uid == "A1B2C3D4") userName = "Hung Anh (Admin)"; 
    
    content.set("fields/name/stringValue", userName);
    content.set("fields/status/stringValue", "Da Quet The");
    content.set("fields/timestamp/stringValue", "SERVER_TIMESTAMP_LATER"); 

    if (Firebase.Firestore.createDocument(&fbdo, PROJECT_ID, "", documentPath.c_str(), content.raw())) {
        Serial.println(">> GOI DU LIEU LEN WEB THANH CONG!");
    } else {
        Serial.println(">> LOI GUI FIREBASE: " + fbdo.errorReason());
    }
  }
}
