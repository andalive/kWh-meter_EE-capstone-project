#include <PZEM004Tv30.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>


#define RX_PIN 16      //ESP32 RX
#define TX_PIN 17      //ESP32 TX
#define PB_LEFT 18     //left button
#define PB_RIGHT 19    //right button
// #define RELAY_SIGNAL 23
// #define RED_LED 25
// #define YELLOW_LED 26
// #define GREEN_LED 27
// #define BUZZER 33

PZEM004Tv30 pzem(Serial2, RX_PIN, TX_PIN);
LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- NETWORK CONFIG ---
const char* ssid = "NNA 2700";
const char* password = "807<8Bo4";

// --- DEFINE NPT CLIENT TO GET TIME ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);  //offset = GMT+7

String formattedDate, dateStamp, timeStamp, dd, mm, yyyy;  //variabel waktu
float voltage, current, power, energy, frequency, pf;      //variabel kWh
int pageCtr = 1, pageMax = 3;                              //navigation page
bool currentR = LOW, lastR = LOW;                          //debounce button right
bool currentL = LOW, lastL = LOW;                          //debounce button left  

// --- TIMER NON BLOCKING ---
unsigned long lastSensorRead = 0;
unsigned long lastTimeUpdate = 0;
unsigned long lastLCDUpdate = 0;


// =============================== 
// FUNGSI KONEKSI WIFI
// ===============================
bool connectWiFi() {
  Serial.println("\n*******************************************");
  Serial.print("Connection to: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);

  int attempts = 0;

  lcd.setCursor(0, 2);
  lcd.print("Connecting WiFi ...");

  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  //delay(2000);
  //lcd.clear();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("*******************************************");

    lcd.setCursor(0, 3);
    lcd.print("[CONNECTED]");
    return true;
  } else {
    Serial.println("\nWiFi failed.");
    Serial.println("*******************************************");

    lcd.setCursor(0, 3);
    lcd.print("[FAILED]");
    return false;
  }
}


// =============================== 
// FUNGSI WAKTU-TANGGAL
// ===============================
String getDateTime() {
  timeClient.update();

  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  int splitT = formattedDate.indexOf("T");
  
  dateStamp = formattedDate.substring(0, splitT);                                // Extract date
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);   // Extract time

  dd   = dateStamp.substring(8, 10);
  mm   = dateStamp.substring(5, 7);
  yyyy = dateStamp.substring(0, 4);

  return timeStamp + "  " + dd + "-" + mm + "-" + yyyy;
}


// =============================== 
// FUNGSI DEBOUNCING TOMBOL
// ===============================
bool debounce(bool lastState, int pinOut) {
  bool currentState = digitalRead(pinOut);

  if (currentState != lastState) {
    delay(5);
    currentState = digitalRead(pinOut);
  }
  return currentState;
}


// ===============================
// SETUP
// ===============================
void setup() {
  Serial.begin(115200);

  pinMode(PB_LEFT, INPUT_PULLDOWN);
  pinMode(PB_RIGHT, INPUT_PULLDOWN);
  
  // --- INITIALIZE LCD ---
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("POWER MONITOR"); 

  // --- CEK KONEKSI WIFI ---
  if (connectWiFi()) {
    // --- Initialize NTP client ---
    timeClient.begin();
  }

  delay(2000);
  lcd.clear();

  // --- Reset energi ---
  pzem.resetEnergy();
}


// ===============================
// LOOP
// ===============================
void loop() {
  unsigned long nowTime = millis();

  // --- KONFIGURASI TOMBOL NAVIGASI ---
  currentL = debounce(lastL, PB_LEFT);       //debounce for left button
  currentR = debounce(lastR, PB_RIGHT);      //debounce for right button

  // --- PAGE NEXT (RIGHT BUTTON) ---
  if (lastR == LOW && currentR == HIGH) {
    lcd.clear();

    if (pageCtr < pageMax) {
      pageCtr++;
    } else {
      pageCtr = pageMax;
    }
  }

  lastR = currentR;

  // --- PAGE PREV (LEFT BUTTON) ---
  if (lastL == LOW && currentL == HIGH) {
    lcd.clear();

    if (pageCtr > 1) {
      pageCtr--;
    } else {
      pageCtr = 1;
    }
  }

  lastL = currentL;

  // --- Read the data from the PZEM sensor ---
  Serial.print("\nCustom Address:");
  Serial.println(pzem.readAddress(), HEX);

  if (nowTime - lastSensorRead >= 1000) {
    lastSensorRead = nowTime;

    voltage   = pzem.voltage();
    current   = pzem.current();
    power     = pzem.power();
    energy    = pzem.energy();
    frequency = pzem.frequency();
    pf        = pzem.pf();

    if (isnan(voltage))   voltage   = 0;
    if (isnan(current))   current   = 0;
    if (isnan(power))     power     = 0;
    if (isnan(energy))    energy    = 0;
    if (isnan(frequency)) frequency = 0;
    if (isnan(pf))        pf        = 0;
  }

  // --- Update Date & Time tiap 1 s ---
  if (nowTime - lastTimeUpdate >= 1000) {
    lastTimeUpdate = nowTime;

    lcd.setCursor(0, 0);
    lcd.print(getDateTime());
  }

  // --- Print the values to the Serial terminal ---
  Serial.print("Voltage: ");      Serial.print(voltage);      Serial.println("V");
  Serial.print("Current: ");      Serial.print(current);      Serial.println("A");
  Serial.print("Power: ");        Serial.print(power);        Serial.println("W");
  Serial.print("Energy: ");       Serial.print(energy);       Serial.println("kWh");
  Serial.print("Frequency: ");    Serial.print(frequency);    Serial.println("Hz");
  Serial.print("PF: ");           Serial.println(pf);

  // --- Update LCD only if Sensor read ---
  if (nowTime - lastLCDUpdate >= 1000) {
    lastLCDUpdate = nowTime;

    // --- Print the values to the LCD ---
    switch(pageCtr) {
    case 1:
      lcd.setCursor(0, 2);
      lcd.print("V:" + String(voltage, 1) + "V");
      lcd.setCursor(10,2);
      lcd.print("I:" + String(current, 1) + "A");
      lcd.setCursor(0, 3);
      lcd.print("P:" + String(power, 1) + "W");
      lcd.setCursor(10,3);
      lcd.print("E:" + String(energy, 1) + "kWh");
      break;
    case 2:
      lcd.setCursor(0, 2);
      lcd.print("F:" + String(frequency, 1) + "Hz");
      lcd.setCursor(10,2);
      lcd.print("PF:" + String(pf, 2));
      break;
    case 3:
      lcd.setCursor(0, 2);
      lcd.print("END");
      break;
    }
  }
}