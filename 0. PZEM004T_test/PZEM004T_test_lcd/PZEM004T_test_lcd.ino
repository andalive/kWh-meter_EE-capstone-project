#include <PZEM004Tv30.h>
#include <LiquidCrystal_I2C.h>

#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#define PZEM_SERIAL Serial2

PZEM004Tv30 pzem(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN);
LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {
  /* Debugging serial */
  Serial.begin(115200);

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  // Display a welcome message on the LCD
  lcd.setCursor(0, 0);
  lcd.print("Power Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();

  pzem.resetEnergy();
}

void loop() {
  Serial.print("\nCustom Address: ");
  Serial.println(pzem.readAddress(), HEX);

  // Read data from the PZEM sensor
  float voltage   = pzem.voltage();
  float current   = pzem.current();
  float power     = pzem.power();
  float energy    = pzem.energy();
  float frequency = pzem.frequency();
  float pf        = pzem.pf();

  // Check if the data is valid
  if (isnan(voltage)) {
    lcd.setCursor(0, 2);
    lcd.print("Error reading V");
    Serial.println("Error reading V");
  } else if (isnan(current)) {
    lcd.setCursor(0, 2);
    lcd.print("Error reading I");
    Serial.println("Error reading I");
  } else if (isnan(power)) {
    lcd.setCursor(0, 2);
    lcd.print("Error reading P");
    Serial.println("Error reading P");
  } else if (isnan(energy)) {
    lcd.setCursor(0, 2);
    lcd.print("Error reading E");
    Serial.println("Error reading E");
  } else if (isnan(frequency)) {
    lcd.setCursor(0, 2);
    lcd.print("Error reading F");
    Serial.println("Error reading F");
  } else if (isnan(pf)) {
    lcd.setCursor(0, 2);
    lcd.print("Error reading pf");
    Serial.println("Error reading pf");
  } else {
    // Print the values to the Serial console
    Serial.print("Voltage: ");      Serial.print(voltage);      Serial.println("V");
    Serial.print("Current: ");      Serial.print(current);      Serial.println("A");
    Serial.print("Power: ");        Serial.print(power);        Serial.println("W");
    Serial.print("Energy: ");       Serial.print(energy, 3);    Serial.println("kWh");
    Serial.print("Frequency: ");    Serial.print(frequency, 1); Serial.println("Hz");
    Serial.print("PF: ");           Serial.println(pf);
    
    // Print the values to the LCD
    lcd.setCursor(0, 0);
    lcd.print("Voltage:" + String(voltage) + "V   ");
    lcd.setCursor(0, 1);
    lcd.print("Current:" + String(current) + "A   ");
    lcd.setCursor(0, 2);
    lcd.print("Power  :" + String(power) + "W   ");
    lcd.setCursor(0, 3);
    lcd.print("Energy :" + String(energy, 3) + "kwh  ");
  }

  // Delay for better readability
  delay(1000);
}