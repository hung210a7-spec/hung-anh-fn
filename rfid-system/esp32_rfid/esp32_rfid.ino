/*
 * ================================================================
 *  HỆ THỐNG KIỂM SOÁT RA VÀO RFID
 *  ESP32 + RC522
 *  
 *  ⚙️ CẤU HÌNH: Chỉnh sửa các giá trị trong phần CONFIG bên dưới
 * ================================================================
 */

#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ================================================================
//  ⚙️ CONFIG — SỬA CÁC GIÁ TRỊ NÀY CHO PHÙ HỢP
// ================================================================
const char* WIFI_SSID     = "TEN_WIFI_CUA_BAN";      // Tên WiFi
const char* WIFI_PASSWORD = "MAT_KHAU_WIFI";          // Mật khẩu WiFi
const char* SERVER_IP     = "192.168.1.100";          // IP máy tính (gõ ipconfig trong CMD)
const int   SERVER_PORT   = 3000;
const char* LOCATION      = "Tang 1 - Cua Chinh";    // Vị trí đầu đọc này
// ================================================================

// Chân kết nối RC522 → ESP32
#define SS_PIN    5    // SDA/SS
#define RST_PIN   22   // RST
#define LED_GREEN 2    // LED xanh (hợp lệ)
#define LED_RED   4    // LED đỏ (không hợp lệ)
#define BUZZER    15   // Còi

MFRC522 rfid(SS_PIN, RST_PIN);

String serverURL;

void setup() {
    Serial.begin(115200);
    SPI.begin();
    rfid.PCD_Init();

    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(BUZZER, OUTPUT);

    // Kết nối WiFi
    Serial.print("Đang kết nối WiFi: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 30) {
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi kết nối thành công!");
        Serial.print("IP ESP32: ");
        Serial.println(WiFi.localIP());
        beep(1, 200); // 1 tiếng bíp = sẵn sàng
    } else {
        Serial.println("\n❌ WiFi thất bại! Kiểm tra SSID/Password");
        beepError();
    }

    serverURL = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/api/scan";
    Serial.println("Server URL: " + serverURL);
    Serial.println("🔄 Sẵn sàng đọc thẻ RFID...");
}

void loop() {
    // Chờ thẻ mới
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        return;
    }

    // Đọc UID thẻ
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(rfid.uid.uidByte[i], HEX);
        if (i < rfid.uid.size - 1) uid += ":";
    }
    uid.toUpperCase();

    Serial.println("📡 Đọc thẻ: " + uid);

    // Gửi lên server
    bool success = sendToServer(uid);

    if (success) {
        digitalWrite(LED_GREEN, HIGH);
        beep(1, 300);
        delay(1000);
        digitalWrite(LED_GREEN, LOW);
    } else {
        digitalWrite(LED_RED, HIGH);
        beepError();
        delay(1000);
        digitalWrite(LED_RED, LOW);
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    delay(1500); // Chống đọc trùng
}

bool sendToServer(String uid) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ Mất kết nối WiFi!");
        return false;
    }

    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    // Tạo JSON payload
    StaticJsonDocument<200> doc;
    doc["uid"]      = uid;
    doc["location"] = LOCATION;

    String payload;
    serializeJson(doc, payload);

    Serial.println("Gửi: " + payload);
    int statusCode = http.POST(payload);

    if (statusCode > 0) {
        String response = http.getString();
        Serial.println("Server trả lời (" + String(statusCode) + "): " + response);
        http.end();

        // Parse response để biết hợp lệ hay không
        StaticJsonDocument<200> res;
        deserializeJson(res, response);
        bool valid = res["valid"] | true;
        return valid;
    } else {
        Serial.println("❌ Lỗi HTTP: " + http.errorToString(statusCode));
        http.end();
        return false;
    }
}

void beep(int times, int duration) {
    for (int i = 0; i < times; i++) {
        digitalWrite(BUZZER, HIGH);
        delay(duration);
        digitalWrite(BUZZER, LOW);
        if (i < times - 1) delay(100);
    }
}

void beepError() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZER, HIGH);
        delay(150);
        digitalWrite(BUZZER, LOW);
        delay(100);
    }
}
