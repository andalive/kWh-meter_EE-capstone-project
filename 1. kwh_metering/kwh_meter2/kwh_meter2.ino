#include <PZEM004Tv30.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ----- PIN DEFINITION -----
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

// ----- NETWORK CONFIG -----
const char* ssid = "NNA 2700";
const char* password = "807<8Bo4";

// ----- NTP CLIENT TO GET TIME -----
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);  //offset = GMT+7

String formattedDate, dateStamp, timeStamp, dd, mm, yyyy;  //variabel waktu
float voltage, current, power, energy, frequency, pf;      //variabel kWh
int pageCtr = 1, pageMax = 3;                              //navigation page

// ----- TIMER NON BLOCKING -----
unsigned long lastSensorRead = 0;
unsigned long lastTimeUpdate = 0;

// ----- INTERUPT FALGS -----
volatile bool rightPressed = false, leftPressed = false;
unsigned long lastIRight = 0, lastILeft = 0;
const unsigned long debounceI = 150; // ms debounce

// ----- ISR HANDLERS -----
void IRAM_ATTR onRightPress() {
  unsigned long now = millis();

  if (now - lastIRight > debounceI) {
    rightPressed = true;
    lastIRight = now;
  }
}

void IRAM_ATTR onLeftPress() {
  unsigned long now = millis();

  if (now - lastILeft > debounceI) {
    leftPressed = true;
    lastILeft = now;
  }
}


// ==============================================================
// FUNGSI KONEKSI WIFI
// ==============================================================
bool connectWiFi() {
  Serial.print("\nConnection to: ");
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

  return WiFi.status() == WL_CONNECTED;
}


// ============================================================== 
// FUNGSI WAKTU-TANGGAL
// ==============================================================
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


// ==============================================================
// CLEAR CONTENT AREA ONLY
// (Baris 1, 2, 3 saja)
// ==============================================================
void clearContentArea() {
  lcd.setCursor(0, 1); lcd.print("                    ");
  lcd.setCursor(0, 2); lcd.print("                    ");
  lcd.setCursor(0, 3); lcd.print("                    ");
}


// ============================================================== 
// FUNGSI NAVIGASI HALAMAN
// ==============================================================
void drawPage() {
  clearContentArea();

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


// ==============================================================
// SETUP
// ==============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PB_LEFT, INPUT_PULLDOWN);
  pinMode(PB_RIGHT, INPUT_PULLDOWN);

  attachInterrupt(digitalPinToInterrupt(PB_RIGHT), onRightPress, RISING);
  attachInterrupt(digitalPinToInterrupt(PB_LEFT), onLeftPress, RISING);
  
  // ----- INITIALIZE LCD -----
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("POWER MONITOR"); 

  // ----- CEK KONEKSI WIFI -----
  if (connectWiFi()) {
    // ----- Initialize NTP client -----
    timeClient.begin();

    Serial.println("[CONNECTED]\n");
    lcd.setCursor(0, 3);
    lcd.print("[CONNECTED]");
  } else {
    Serial.println("[FAILED]\n");
    lcd.setCursor(0, 3);
    lcd.print("[FAILED]");
  }

  delay(2000);
  lcd.clear();

  // ----- Reset energi -----
  pzem.resetEnergy();
}


// ==============================================================
// LOOP
// ==============================================================
void loop() {
  unsigned long nowTime = millis();

  // ----- TANGGAL DAN WAKTU -----
  if (nowTime - lastTimeUpdate >= 1000) {
    lastTimeUpdate = nowTime;
    lcd.setCursor(0, 0);
    lcd.print(getDateTime());
  }

  // ----- KONFIGURASI TOMBOL NAVIGASI -----
  // tombol kanan
  if (rightPressed) {
    rightPressed = false;

    if (pageCtr < pageMax) {
      pageCtr++;
    }
    
    drawPage();
  }

  //tombol kiri
  if (leftPressed) {
    leftPressed = false;

    if (pageCtr > 1) {
      pageCtr--;
    }

    drawPage();
  }
  

  // ----- Read the data from the PZEM sensor -----
  // Serial.print("\nCustom Address:");
  // Serial.println(pzem.readAddress(), HEX);

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

    drawPage();
  }
}
