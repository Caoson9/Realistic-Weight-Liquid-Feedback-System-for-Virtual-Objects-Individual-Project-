void setup() {
  Serial.begin(57600);   // Unity is also 57600
  while (!Serial);       // Mega2560 serial ready
}

void loop() {
  if (Serial.available()) {
    int c = Serial.read();
    Serial.write(c);     // Read and write back
  }
}
