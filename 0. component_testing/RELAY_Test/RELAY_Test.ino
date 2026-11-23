#define RELAY_PIN 32  // pin sinyal relay

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);

  // Untuk relay low-level trigger:
  digitalWrite(RELAY_PIN, HIGH);  // relay OFF awal
}

void loop() {
  Serial.println("Relay ON");
  digitalWrite(RELAY_PIN, LOW);  // aktif (low trigger)
  delay(2000);

  Serial.println("Relay OFF");
  digitalWrite(RELAY_PIN, HIGH); // nonaktif
  delay(2000);
}
