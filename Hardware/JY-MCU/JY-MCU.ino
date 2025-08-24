void setup() {
  Serial.begin(115200);    // PC↔Mega USB (COM8)
  Serial1.begin(115200);   // Mega Serial1↔HC-06
}
void loop() {
  if (Serial.available())  Serial1.write(Serial.read());
  if (Serial1.available()) Serial.write(Serial1.read());
}
