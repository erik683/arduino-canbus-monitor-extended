void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(500000);
  Serial.println("Hello from Arduino!");
}

void loop() {
  digitalWrite(13, HIGH);
  delay(500);
  digitalWrite(13, LOW);
  delay(500);
  
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    Serial.println("Arduino is running!");
    lastPrint = millis();
  }
  
  if (Serial.available()) {
    char c = Serial.read();
    Serial.print("Echo: ");
    Serial.println(c);
  }
}
