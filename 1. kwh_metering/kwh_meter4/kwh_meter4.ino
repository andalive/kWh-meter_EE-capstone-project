/*
  KWH METER
  VER. 1.3 -> adapted to FreeRTOS tasks & queues
*/

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
bool alarmMuted = false, lastCritical = false;             //alarm settings
unsigned long bootMillis = 0;                              //variabel runtime

// ----- Treshold parameter -----
float V_HH = 242, V_H = 231, V_LL = 198, V_L = 200;
float I_HH = 8, I_H = 6.8;

// ----- INTERUPT FLAGS -----
volatile bool rightPressed = false, leftPressed = false;
unsigned long lastIRight = 0, lastILeft = 0;
const unsigned long debounceI = 150; // ms debounce

// ----- FreeRTOS objects -----
QueueHandle_t buttonQueue;    // queue for button ISR events (char 'R' or 'L')
SemaphoreHandle_t lcdMutex;   // mutex to protect LCD access

// ==============================================================
// ISR HANDLERS BUTTON (send event to queue)
// ==============================================================
// We still keep debounce check here as original
void IRAM_ATTR onRightPress() {
  if (millis() - lastIRight > debounceI) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    char ev = 'R';
    xQueueSendFromISR(buttonQueue, &ev, &xHigherPriorityTaskWoken);
    lastIRight = millis();
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
  }
}

void IRAM_ATTR onLeftPress() {
  if (millis() - lastILeft > debounceI) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    char ev = 'L';
    xQueueSendFromISR(buttonQueue, &ev, &xHigherPriorityTaskWoken);
    lastILeft = millis();
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
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

  // Protect lcd with mutex
  if (xSemaphoreTake(lcdMutex, pdMS_TO_TICKS(200))) {
    lcd.setCursor(0, 1); lcd.print("Connecting WiFi ...");
    xSemaphoreGive(lcdMutex);
  }

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
// FUNGSI LOGIKA + STATUS ALARM
// ==============================================================
String getAlarmState(float v, float i) {
  bool isCritical = (v >= V_HH || v <= V_LL || i >= I_HH);
  bool isWarning  = ((v >= V_H && v < V_HH) || 
                     (v > V_LL && v <= V_L) ||
                     (i >= I_H && i < I_HH));
  
  if (isCritical) {
    return "CRITICAL";
  }

  if (isWarning) {
    return "WARNING";
  }

  return "NORMAL";
}

void alarmTrigger(float v, float i) {
  bool isCritical = (v >= V_HH || v <= V_LL || i >= I_HH);
  bool isWarning  = ((v >= V_H && v < V_HH) || 
                     (v > V_LL && v <= V_L) ||
                     (i >= I_H && i < I_HH));
  bool isNormal   = (!isCritical && !isWarning);

  // 1. Reset mute ketika kondisi kritis terjadi lagi
  if (isCritical && !lastCritical) {
    alarmMuted = false;   // boleh bunyi lagi nanti
  }

  lastCritical = isCritical;

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
// FUNGSI NAVIGASI HALAMAN (drawPage)
// ==============================================================
void drawPage(float v, float i) {
  // We'll update only content area as original
  if (xSemaphoreTake(lcdMutex, pdMS_TO_TICKS(200))) {
    // clear content (only lines 1..3)
    lcd.setCursor(0, 1); lcd.print("                    ");
    lcd.setCursor(0, 2); lcd.print("                    ");
    lcd.setCursor(0, 3); lcd.print("                    ");

    String alarmState = getAlarmState(v, i);

    switch(pageCtr) {
      case 1:
        lcd.setCursor(0,1);  lcd.print("V:" + String(voltage, 1) + "V");
        lcd.setCursor(10,1); lcd.print("I:" + String(current, 1) + "A");
        lcd.setCursor(0, 2); lcd.print("P:" + String(power, 1) + "W");
        lcd.setCursor(10,2); lcd.print("E:" + String(energy, 1) + "kWh");
        lcd.setCursor(0,3);  lcd.print("[STATUS: " + alarmState + "]");
        break;
      case 2:
        lcd.setCursor(0,1);  lcd.print("F:" + String(frequency, 1) + "Hz");
        lcd.setCursor(10,1); lcd.print("PF:" + String(pf, 2));
        lcd.setCursor(0,3);  lcd.print("[STATUS: " + alarmState + "]");
        break;
      case 3: {
        unsigned long rt = (millis() - bootMillis) / 1000;
        int h = rt/3600;
        int m = (rt % 3600)/60;
        int s = rt % 60;

        lcd.setCursor(0,1); lcd.print("===SYSTEM  STATUS===");
        lcd.setCursor(0,2); lcd.print("RUN  : " + String(h) + "h " + String(m) + "m " + String(s) + "s");
        lcd.setCursor(0,3); lcd.print("ALARM: " + alarmState);
        break;
      }
    }

    xSemaphoreGive(lcdMutex);
  }
}


// ==============================================================
// FreeRTOS Task: Update time display every 1 second (replaces original loop timing for time)
// ==============================================================
void TaskTimeUpdate(void *pvParameters) {
  const TickType_t xDelay = pdMS_TO_TICKS(1000);

  for (;;) {
    // Update LCD time (same call as before)
    String dt = getDateTime();

    if (xSemaphoreTake(lcdMutex, pdMS_TO_TICKS(200))) {
      lcd.setCursor(0, 0);
      lcd.print(dt);
      xSemaphoreGive(lcdMutex);
    }

    vTaskDelay(xDelay);
  }
}


// ==============================================================
// FreeRTOS Task: Read PZEM data every 1 second, serial print, accumulate & compute 5-min averages
// ==============================================================
void TaskSensorRead(void *pvParameters) {
  const TickType_t xDelay = pdMS_TO_TICKS(1000);
  // local copy for timing of 5-min accumulate
  unsigned long localLastFiveMin = millis();
  unsigned long localCountSamples = 0;
  float localSumV = 0, localSumI = 0, localSumP = 0, localSumE = 0, localSumF = 0, localSumPF = 0;

  for (;;) {
    float rV = pzem.voltage();
    float rI = pzem.current();
    float rP = pzem.power();
    float rE = pzem.energy();
    float rF = pzem.frequency();
    float rPF = pzem.pf();

    if (isnan(rV))   rV   = 0;
    if (isnan(rI))   rI   = 0;
    if (isnan(rP))   rP   = 0;
    if (isnan(rE))   rE   = 0;
    if (isnan(rF))   rF   = 0;
    if (isnan(rPF))  rPF  = 0;

    // Update globals
    voltage = rV;
    current = rI;
    power = rP;
    energy = rE;
    frequency = rF;
    pf = rPF;

    // ----- Print the values to the Serial terminal (same format) -----
    Serial.print("Voltage  : ");    Serial.print(voltage);      Serial.println("V");
    Serial.print("Current  : ");    Serial.print(current);      Serial.println("A");
    Serial.print("Power    : ");    Serial.print(power);        Serial.println("W");
    Serial.print("Energy   : ");    Serial.print(energy);       Serial.println("kWh");
    Serial.print("Frequency: ");    Serial.print(frequency);    Serial.println("Hz");
    Serial.print("PF       : ");    Serial.println(pf);

    // ----- Print the values to the LCD -----
    drawPage(voltage, current);

    // ----- AKUMULASI DATA LOGGING -----
    sumVoltage += voltage;
    sumCurrent += current;
    sumPower += power;
    sumEnergy += energy;
    sumFreq += frequency;
    sumPF += pf;

    localSumV += voltage;
    localSumI += current;
    localSumP += power;
    localSumE += energy;
    localSumF += frequency;
    localSumPF += pf;

    countSamples++;
    localCountSamples++;

    // Check 5-min window (300000 ms)
    if (millis() - localLastFiveMin >= 300000UL) {
      localLastFiveMin += 300000UL;

      //Hitung rata-rata
      float avgVoltage = localSumV / (localCountSamples == 0 ? 1 : localCountSamples);
      float avgCurrent = localSumI / (localCountSamples == 0 ? 1 : localCountSamples);
      float avgPower = localSumP / (localCountSamples == 0 ? 1 : localCountSamples);
      float avgFreq = localSumF / (localCountSamples == 0 ? 1 : localCountSamples);
      float avgEnergy = localSumE / (localCountSamples == 0 ? 1 : localCountSamples);
      float avgPF = localSumPF / (localCountSamples == 0 ? 1 : localCountSamples);

      Serial.println("\n5 min avg data: =================");
      Serial.println("Avg Voltage   : " + String(avgVoltage));
      Serial.println("Avg Current   : " + String(avgCurrent));
      Serial.println("Avg Power     : " + String(avgPower));
      Serial.println("Avg Energy    : " + String(avgEnergy));
      Serial.println("Avg Frequency : " + String(avgFreq));
      Serial.println("Avg PF        : " + String(avgPF));
      Serial.println("=================================\n");

      // Reset akumulasi lokal (global sums are also reset here)
      localSumV = localSumI = localSumP = localSumE = localSumF = localSumPF = 0;
      localCountSamples = 0;

      sumVoltage   = 0;
      sumCurrent   = 0;
      sumPower     = 0;
      sumEnergy    = 0;
      sumFreq      = 0;
      sumPF        = 0;
      countSamples = 0;
    }

    // Call alarm logic
    alarmTrigger(voltage, current);

    vTaskDelay(xDelay);
  }
}


// ==============================================================
// FreeRTOS Task: Button handler - reads events from buttonQueue
// Handles mute and page navigation (keeps same behavior with drawPage())
// ==============================================================
void TaskButtonHandler(void *pvParameters) {
  char ev;
  for (;;) {
    if (xQueueReceive(buttonQueue, &ev, portMAX_DELAY) == pdTRUE) {
      if(lastCritical) {
        alarmMuted = true;
        digitalWrite(BUZZER_PIN, LOW);
      }

      if (ev == 'R') {
        // tombol kanan
        if (pageCtr < pageMax) {
          pageCtr++;
        }
        drawPage(voltage, current);
      } else if (ev == 'L') {
        // tombol kiri
        if (pageCtr > 1) {
          pageCtr--;
        }
        drawPage(voltage, current);
      }
    }
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

  // Create queue and mutex BEFORE attaching interrupts/tasks to ensure safe use
  buttonQueue = xQueueCreate(10, sizeof(char));    // hold events 'R' or 'L'
  lcdMutex = xSemaphoreCreateMutex();

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

  // Print INTRO
  if (xSemaphoreTake(lcdMutex, pdMS_TO_TICKS(500))) {
    lcd.setCursor(0, 0);
    lcd.print("POWER MONITOR");
    xSemaphoreGive(lcdMutex);
  }

  // ----- CEK KONEKSI WIFI -----
  if (connectWiFi()) {
    // ----- Initialize NTP client -----
    timeClient.begin();

    Serial.println("[CONNECTED]\n");
    if (xSemaphoreTake(lcdMutex, pdMS_TO_TICKS(200))) {
      lcd.setCursor(0, 2); lcd.print("[CONNECTED]");
      lcd.setCursor(0, 3); lcd.print("IP: " + WiFi.localIP().toString());
      xSemaphoreGive(lcdMutex);
    }
  } else {
    Serial.println("[FAILED]\n");
    if (xSemaphoreTake(lcdMutex, pdMS_TO_TICKS(200))) {
      lcd.setCursor(0, 2); lcd.print("[FAILED]");
      lcd.setCursor(0, 3); lcd.print("IP: None");
      xSemaphoreGive(lcdMutex);
    }
  }

  delay(2000);
  if (xSemaphoreTake(lcdMutex, pdMS_TO_TICKS(200))) {
    lcd.clear();
    xSemaphoreGive(lcdMutex);
  }

  lastFiveMin = millis();

  // ----- Reset energi -----
  //pzem.resetEnergy();

  // Create FreeRTOS tasks
  // Task priorities: time low, sensor medium, button high
  xTaskCreatePinnedToCore(TaskTimeUpdate, "TimeTask", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskSensorRead, "SensorTask", 8192, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskButtonHandler, "ButtonTask", 4096, NULL, 3, NULL, 1);
}


// ==============================================================
// loop()
// ==============================================================
void loop() {
  // Nothing here — everything handled by FreeRTOS tasks
  vTaskDelay(pdMS_TO_TICKS(1000)); // keep loop minimal (not necessary, but safe)
}
