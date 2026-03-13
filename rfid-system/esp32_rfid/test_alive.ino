void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- TEST HELLO WORLD ---");
  Serial.println("Neu ban thay dong nay, ESP32 van song tot!");
}

void loop() {
  Serial.println("ESP32 dang chay...");
  delay(1000);
}
