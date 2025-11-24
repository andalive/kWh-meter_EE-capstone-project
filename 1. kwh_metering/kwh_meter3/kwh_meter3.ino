#include <PZEM004Tv30.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ----- PIN DEFINITION -----
/*
  SD Card Interface code for ESP32
  SPI Pins of ESP32 SD card as follows:
  CS    = 5;
  MOSI  = 23;
  MISO  = 19;
  SCK   = 18; 
*/
#define RX_PIN 16      //ESP32 RX
#define TX_PIN 17      //ESP32 TX
#define PB_LEFT 14     //left button
#define PB_RIGHT 12    //right button
#define RELAY_PIN 32
#define RED_LED 25
#define YELLOW_LED 26
#define GREEN_LED 27
#define BUZZER_PIN 33

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
float sumVoltage = 0, sumCurrent = 0, sumPower = 0, sumEnergy = 0, sumFreq = 0, sumPF = 0;
unsigned long countSamples = 0, lastFiveMin = 0;
int pageCtr = 1, pageMax = 3;                              //navigation page
bool alarmMuted = false;                                   //alarm settings

// ----- Treshold parameter -----
float V_HH = 242, V_H = 231, V_LL = 198, V_L = 200;
float I_HH = 8, I_H = 6.8;

// ----- TIMER NON BLOCKING -----
unsigned long lastSensorRead = 0;
unsigned long lastTimeUpdate = 0;

// ----- INTERUPT FALGS -----
volatile bool rightPressed = false, leftPressed = false;
unsigned long lastIRight = 0, lastILeft = 0;
const unsigned long debounceI = 150; // ms debounce


// ==============================================================
// ISR HANDLERS BUTTON
// ==============================================================
void IRAM_ATTR onRightPress() {
  if (millis() - lastIRight > debounceI) {
    rightPressed = true;
    lastIRight = millis();
  }
}

void IRAM_ATTR onLeftPress() {
  if (millis() - lastILeft > debounceI) {
    leftPressed = true;
    lastILeft = millis();
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

  return WiFi.status() == WL_CONNECTED;
}


// ============================================================== 
// FUNGSI WAKTU-TANGGAL
// ==============================================================
String getDateTime() {
  timeClient.update();
  formattedDate = timeClient.getFormattedDate();
  Serial.println("\n" + formattedDate);

  int splitT = formattedDate.indexOf("T");
  
  dateStamp = formattedDate.substring(0, splitT);                                // Extract date
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);   // Extract time

  dd   = dateStamp.substring(8, 10);
  mm   = dateStamp.substring(5, 7);
  yyyy = dateStamp.substring(0, 4);

  return timeStamp + "  " + dd + "-" + mm + "-" + yyyy;
}


// ==============================================================
// CLEAR CONTENT AREA ONLY (Baris 1, 2, 3 saja)
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
// FUNGSI LOGIKA ALARM
// ==============================================================
void alarmTrigger(float v, float i) {
  bool isCritical = (v >= V_HH || v <= V_LL || i >= I_HH);
  bool isWarning  = ((v >= V_H && v < V_HH) || 
                     (v > V_LL && v <= V_L) ||
                     (i >= I_H && i < I_HH));
  bool isNormal   = (!isCritical && !isWarning);

  // 1. Reset mute ketika kondisi normal
  if (isNormal) {
    alarmMuted = false;   // boleh bunyi lagi nanti
  }

  // 2. KRITIS → LED merah + relay off
  if (isCritical) {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RELAY_PIN, LOW);

    if (!alarmMuted) {
      digitalWrite(BUZZER_PIN, HIGH);    // bunyi jika belum mute
    } else {
      digitalWrite(BUZZER_PIN, LOW);     // mati kalau dimute user
    }
    return;
  }

  // 3. WARNING → LED kuning
  if (isWarning) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RELAY_PIN, HIGH);

    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  // 4. NORMAL → LED hijau
  if (isNormal) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RELAY_PIN, HIGH);

    digitalWrite(BUZZER_PIN, LOW);
  }
}


// ==============================================================
// SETUP
// ==============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PB_LEFT, INPUT_PULLDOWN);
  pinMode(PB_RIGHT, INPUT_PULLDOWN);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(PB_RIGHT), onRightPress, RISING);
  attachInterrupt(digitalPinToInterrupt(PB_LEFT), onLeftPress, RISING);
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  
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

  lastFiveMin = millis();

  // ----- Reset energi -----
  //pzem.resetEnergy();
}


// ==============================================================
// LOOP
// ==============================================================
void loop() {
  unsigned long nowTime = millis();

  // ----- TANGGAL DAN WAKTU -----
  if (nowTime - lastTimeUpdate >= 1000) {
    lastTimeUpdate += 1000;
    //lastTimeUpdate = nowTime;
    lcd.setCursor(0, 0);
    lcd.print(getDateTime());
  }

  // ----- KONFIGURASI TOMBOL NAVIGASI -----
  // Handle tombol untuk matikan alarm
  if (rightPressed || leftPressed) {
    alarmMuted = true;
    digitalWrite(BUZZER_PIN, LOW);
  }

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
    lastSensorRead += 1000;

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

    // ----- Print the values to the Serial terminal -----
    Serial.print("Voltage  : ");    Serial.print(voltage);      Serial.println("V");
    Serial.print("Current  : ");    Serial.print(current);      Serial.println("A");
    Serial.print("Power    : ");    Serial.print(power);        Serial.println("W");
    Serial.print("Energy   : ");    Serial.print(energy);       Serial.println("kWh");
    Serial.print("Frequency: ");    Serial.print(frequency);    Serial.println("Hz");
    Serial.print("PF       : ");    Serial.println(pf);

    // ----- Print the values to the LCD -----
    drawPage();

    // ----- AKUMULASI DATA LOGGING -----
    sumVoltage += voltage;
    sumCurrent += current;
    sumPower += power;
    sumEnergy += energy;
    sumFreq += frequency;
    sumPF += pf;

    countSamples++;

    if (nowTime - lastFiveMin >= 300000) {
      lastFiveMin += 300000;

      //Hitung rata-rata
      float avgVoltage = sumVoltage/countSamples;
      float avgCurrent = sumCurrent/countSamples;
      float avgPower = sumPower/countSamples;
      float avgFreq = sumFreq/countSamples;
      float avgEnergy = sumEnergy/countSamples;
      float avgPF = sumPF/countSamples;

      Serial.println("\n5 min avg data: =================");
      Serial.println("Avg Voltage   : " + String(avgVoltage));
      Serial.println("Avg Current   : " + String(avgCurrent));
      Serial.println("Avg Power     : " + String(avgPower));
      Serial.println("Avg Energy    : " + String(avgEnergy));
      Serial.println("Avg Frequency : " + String(avgFreq));
      Serial.println("Avg PF        : " + String(avgPF));
      Serial.println("=================================\n");

      // Reset akumulasi
      sumVoltage   = 0;
      sumCurrent   = 0;
      sumPower     = 0;
      sumEnergy    = 0;
      sumFreq      = 0;
      sumPF        = 0;
      countSamples = 0;
    }

    alarmTrigger(voltage, current);
  }
}