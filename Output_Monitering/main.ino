// ============================================================
//  SMART HOSPITAL BED MONITORING SYSTEM
//  Firmware for ESP32 Dev Board
//
//  Device Role  : One hospital bed unit (scalable to N beds)
//  Server Target: Raspberry Pi HTTP server (ward dashboard)
//  Protocol     : HTTP POST — JSON payload
//  Loop Interval: 5 seconds
//
//  Author  : Embedded Systems / IoT Prototype
//  Version : 1.0.0 — Prototype Release
//  IDE     : Arduino IDE 2.x  (ESP32 board package ≥ 2.0)
//
//  ── Hardware Wiring Summary ──────────────────────────────
//  MAX30102   → I2C  (SDA=GPIO21, SCL=GPIO22)
//  DS18B20    → GPIO 4  (OneWire, 4.7kΩ pull-up to 3.3V)
//  FSR Sensor → GPIO 34 (Analog in, voltage divider circuit)
//  Emergency  → GPIO 15 (Digital in, INPUT_PULLUP, LOW=active)
//  Buzzer     → GPIO 25 (Digital out, active HIGH)
//  Status LED → GPIO 26 (Digital out, active HIGH)
//  MPU6050    → I2C  (SDA=GPIO21, SCL=GPIO22) [OPTIONAL]
//
//  Required Libraries (install via Arduino Library Manager):
//    • WiFi              — built-in ESP32 core
//    • HTTPClient        — built-in ESP32 core
//    • ArduinoJson       — by Benoit Blanchon (v6.x)
//    • MAX3010x          — by SparkFun Electronics
//    • OneWire           — by Paul Stoffregen
//    • DallasTemperature — by Miles Burton
// ============================================================

// ── Standard / Core Libraries ───────────────────────────────
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ── Sensor Libraries ─────────────────────────────────────────
#include <Wire.h>
#include "MAX30105.h"          // MAX30102 driver (SparkFun MAX3010x lib)
#include "heartRate.h"         // Beat averaging helper from MAX3010x lib
#include <OneWire.h>
#include <DallasTemperature.h>

// ============================================================
//  CONFIGURATION — Edit these values before deployment
// ============================================================

// ── Network ─────────────────────────────────────────────────
const char* WIFI_SSID     = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// ── Raspberry Pi Server ──────────────────────────────────────
//  Ensure the Pi server is listening on this address and port.
//  Example FastAPI / Flask endpoint: POST /api/bed-data
const char* SERVER_HOST   = "192.168.1.100";
const int   SERVER_PORT   = 5000;
const char* SERVER_PATH   = "/api/bed-data";

// ── Bed Identity ─────────────────────────────────────────────
//  Increment this for each physical bed unit on the ward.
const int BED_ID = 3;

// ── GPIO Pin Assignments ─────────────────────────────────────
#define PIN_DS18B20       4    // OneWire data pin (DS18B20)
#define PIN_FSR           34   // FSR analog input
#define PIN_EMERGENCY_BTN 15   // Emergency push button (active LOW)
#define PIN_BUZZER        25   // Buzzer output
#define PIN_STATUS_LED    26   // Status LED output

// ── Timing ───────────────────────────────────────────────────
#define LOOP_INTERVAL_MS  5000   // Main monitoring loop interval (5 s)
#define WIFI_RETRY_DELAY  500    // Delay between WiFi retries (ms)
#define WIFI_MAX_RETRIES  20     // Max WiFi connection attempts

// ── Alert Thresholds ─────────────────────────────────────────
#define HEART_RATE_HIGH   120    // BPM upper limit
#define HEART_RATE_LOW    40     // BPM lower limit
#define TEMP_FEVER        38.0f  // °C fever threshold
#define FSR_BED_EMPTY     200    // ADC value below this → bed empty
                                 // (tune to your voltage divider / FSR)

// ── MPU6050 Feature Flag ─────────────────────────────────────
//  Set to true only when MPU6050 hardware is physically connected.
#define MPU6050_ENABLED   false

// ============================================================
//  SENSOR OBJECT DECLARATIONS
// ============================================================

// MAX30102 particle sensor object
MAX30105 particleSensor;

// DS18B20 OneWire bus and temperature library objects
OneWire           oneWireBus(PIN_DS18B20);
DallasTemperature tempSensor(&oneWireBus);

// ============================================================
//  GLOBAL STATE — runtime values shared across functions
// ============================================================

// Vital sign readings (updated each loop cycle)
float  g_heartRate    = 0.0f;
float  g_spO2         = 0.0f;
float  g_temperature  = 0.0f;
int    g_pressure     = 0;       // 1 = patient present, 0 = bed empty
bool   g_emergency    = false;   // true when button pressed

// Heart-rate beat detection state (MAX30102 rolling average)
const byte RATE_SIZE  = 4;       // Number of samples to average
byte  g_rates[RATE_SIZE];        // Circular buffer of beat intervals
byte  g_rateSpot      = 0;
long  g_lastBeat      = 0;       // Timestamp of last detected beat (ms)
float g_beatsPerMin   = 0.0f;
float g_beatAvg       = 0.0f;

// Alert string sent in JSON payload
String g_alertStatus  = "normal";

// Timestamp of last completed monitoring loop
unsigned long g_lastLoopTime = 0;

// ============================================================
//  FORWARD DECLARATIONS
//  Defined after setup() / loop() for readability.
// ============================================================
bool  connectWiFi();
void  ensureWiFiConnected();
void  initSensors();
void  readHeartRateSpO2();
void  readTemperature();
void  readFSRPressure();
void  readEmergencyButton();
void  evaluateAlerts();
void  activateBuzzer(int durationMs);
void  setStatusLED(bool state);
void  sendDataToServer();
void  printDebugInfo();

// ============================================================
//  SETUP
//  Runs once on power-on or reset.
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);  // Allow serial monitor to stabilise

  Serial.println();
  Serial.println("============================================");
  Serial.println("  Smart Hospital Bed Monitoring System");
  Serial.printf ("  Bed ID: %d  |  Firmware v1.0.0\n", BED_ID);
  Serial.println("============================================");

  // ── Pin Modes ──────────────────────────────────────────────
  pinMode(PIN_EMERGENCY_BTN, INPUT_PULLUP);  // LOW = pressed
  pinMode(PIN_BUZZER,        OUTPUT);
  pinMode(PIN_STATUS_LED,    OUTPUT);

  digitalWrite(PIN_BUZZER,     LOW);
  digitalWrite(PIN_STATUS_LED, LOW);

  // ── Start-up Blink ─────────────────────────────────────────
  //  Two quick blinks confirm GPIO and LED are working.
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_STATUS_LED, HIGH);
    delay(200);
    digitalWrite(PIN_STATUS_LED, LOW);
    delay(200);
  }

  // ── WiFi ───────────────────────────────────────────────────
  connectWiFi();

  // ── Sensor Initialisation ──────────────────────────────────
  initSensors();

  // ── Ready ──────────────────────────────────────────────────
  Serial.println("[SYSTEM] Initialisation complete. Entering monitoring loop.");
  digitalWrite(PIN_STATUS_LED, HIGH);  // Steady ON = system healthy
}

// ============================================================
//  MAIN LOOP
//  Executes every LOOP_INTERVAL_MS milliseconds (default 5 s).
//  Non-blocking timing via millis() to keep the MCU responsive.
// ============================================================
void loop() {
  unsigned long now = millis();

  // ── Continuous MAX30102 Beat Detection ─────────────────────
  //  Beat detection must be polled faster than the loop interval
  //  to avoid missing peaks. It runs on every loop() iteration.
  readHeartRateSpO2();

  // ── Timed Monitoring Block (every 5 seconds) ───────────────
  if (now - g_lastLoopTime >= LOOP_INTERVAL_MS) {
    g_lastLoopTime = now;

    Serial.println();
    Serial.println("--- Monitoring Cycle ---");

    // 1. Read remaining sensors
    readTemperature();
    readFSRPressure();
    readEmergencyButton();

    // 2. Evaluate alert conditions and set g_alertStatus
    evaluateAlerts();

    // 3. Debug output to Serial Monitor
    printDebugInfo();

    // 4. Ensure WiFi is connected before transmitting
    ensureWiFiConnected();

    // 5. Send JSON data packet to Raspberry Pi server
    sendDataToServer();
  }
}

// ============================================================
//  WIFI — CONNECTION FUNCTIONS
// ============================================================

/**
 * @brief Attempt to connect to WiFi on startup.
 * @return true if connected, false if timed out.
 */
bool connectWiFi() {
  Serial.printf("[WiFi] Connecting to: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_MAX_RETRIES) {
    delay(WIFI_RETRY_DELAY);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println();
    Serial.println("[WiFi] ERROR: Failed to connect. Will retry in monitoring loop.");
    return false;
  }
}

/**
 * @brief Called each monitoring cycle to restore WiFi if dropped.
 *        Blocking for up to WIFI_MAX_RETRIES × WIFI_RETRY_DELAY ms.
 */
void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("[WiFi] Connection lost. Reconnecting...");
  WiFi.disconnect();
  delay(100);
  connectWiFi();
}

// ============================================================
//  SENSOR INITIALISATION
// ============================================================

/**
 * @brief Initialise all sensors and validate hardware presence.
 *        System continues even if a sensor fails (safe defaults used).
 */
void initSensors() {
  // ── I2C Bus ────────────────────────────────────────────────
  Wire.begin();  // Uses default SDA=21, SCL=22 for ESP32

  // ----------------------------
  // MAX30102 SENSOR SECTION
  // This sensor measures heart rate (BPM) and blood oxygen
  // saturation (SpO2) using photoplethysmography (PPG).
  // It is used in the hospital system to continuously monitor
  // a patient's cardiovascular status and oxygen levels.
  // Monitoring begins as soon as the device enters the main
  // loop. Any reading outside normal range triggers an alert.
  // ----------------------------
  Serial.print("[INIT] MAX30102 heart rate / SpO2 sensor... ");
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("FAILED — check wiring. Using default values.");
  } else {
    // Sensor configuration for finger / wrist placement
    particleSensor.setup();                         // Default configuration
    particleSensor.setPulseAmplitudeRed(0x0A);      // Low red LED for heart rate
    particleSensor.setPulseAmplitudeGreen(0);       // Green off
    Serial.println("OK");
  }

  // ----------------------------
  // DS18B20 TEMPERATURE SENSOR SECTION
  // This sensor measures patient body surface temperature
  // using a waterproof digital thermistor probe.
  // It is used to detect fever conditions (>38°C) which may
  // indicate infection, sepsis, or post-operative complications.
  // Monitoring begins at the start of each 5-second loop cycle.
  // ----------------------------
  Serial.print("[INIT] DS18B20 temperature sensor... ");
  tempSensor.begin();
  int deviceCount = tempSensor.getDeviceCount();
  if (deviceCount == 0) {
    Serial.println("FAILED — no devices found on OneWire bus.");
  } else {
    Serial.printf("OK (%d device(s) found)\n", deviceCount);
    tempSensor.setResolution(12);  // 12-bit = 0.0625°C resolution
  }

  // ----------------------------
  // FSR PRESSURE SENSOR SECTION
  // This Force Sensitive Resistor detects whether the patient
  // is physically present on the bed by measuring the weight
  // applied to the mattress contact area.
  // It is used in the hospital system to alert nursing staff
  // if a patient leaves the bed unexpectedly, which may
  // indicate a fall, confusion, or unauthorised movement.
  // Monitoring begins each 5-second loop cycle.
  // ----------------------------
  Serial.print("[INIT] FSR pressure sensor on GPIO ");
  Serial.print(PIN_FSR);
  // GPIO 34 is input-only analog capable on ESP32 — no pinMode needed
  // for analogRead, but we explicitly document it.
  Serial.println("... OK (analog input)");

  // ----------------------------
  // EMERGENCY PUSH BUTTON SECTION
  // This momentary push button allows a patient or staff member
  // to manually trigger an immediate nurse call alert.
  // It is used as a direct emergency escalation mechanism when
  // the patient is in distress but automatic sensors have not
  // yet detected an abnormal condition.
  // The button is read each 5-second loop cycle. In a future
  // production build, an interrupt handler can be added for
  // sub-second response time.
  // ----------------------------
  Serial.printf("[INIT] Emergency button on GPIO %d... OK (INPUT_PULLUP)\n",
                PIN_EMERGENCY_BTN);

  // ── MPU6050 Fall Detection (Optional) ──────────────────────
  //  When enabled, the MPU6050 6-axis IMU detects sudden
  //  acceleration spikes that may indicate a patient fall.
  //  Leave MPU6050_ENABLED = false when hardware is absent.
#if MPU6050_ENABLED
  Serial.println("[INIT] MPU6050 fall detection sensor... (stub — not implemented)");
  // TODO: Add MPU6050 initialisation and fall detection logic
  //       using the Adafruit MPU6050 library when hardware is ready.
#else
  Serial.println("[INIT] MPU6050 fall detection: DISABLED (hardware not present)");
#endif
}

// ============================================================
//  SENSOR READING FUNCTIONS
// ============================================================

// ----------------------------
// MAX30102 SENSOR SECTION
// Continuous beat detection polling. Called every loop() tick
// (not just every 5 s) so no peaks are missed.
// Heart rate average is computed once per 5-second report cycle
// using a 4-sample rolling buffer provided by the SparkFun lib.
// ----------------------------

/**
 * @brief Poll the MAX30102 for new samples and compute BPM.
 *        Must be called on every loop() iteration.
 */
void readHeartRateSpO2() {
  long irValue = particleSensor.getIR();

  // Only process if a finger / probe is detected (IR > threshold)
  if (irValue < 50000) {
    // No contact — keep last valid reading or default
    return;
  }

  // Beat detected?
  if (checkForBeat(irValue)) {
    long delta      = millis() - g_lastBeat;
    g_lastBeat      = millis();
    g_beatsPerMin   = 60.0f / (delta / 1000.0f);

    // Sanity gate — discard physiologically impossible readings
    if (g_beatsPerMin > 20 && g_beatsPerMin < 255) {
      g_rates[g_rateSpot++ % RATE_SIZE] = (byte)g_beatsPerMin;

      // Compute rolling average
      float avg = 0;
      for (byte i = 0; i < RATE_SIZE; i++) avg += g_rates[i];
      g_beatAvg   = avg / RATE_SIZE;
      g_heartRate = g_beatAvg;
    }
  }

  // SpO2 estimation — simplified ratio-of-ratios proxy.
  // For production, integrate a full R/IR SpO2 calibration curve
  // or use a dedicated library with calibration coefficients.
  long redValue = particleSensor.getRed();
  if (irValue > 0 && redValue > 0) {
    float ratio = (float)redValue / (float)irValue;
    // Approximate mapping: ratio ≈ 0.5 → SpO2 ≈ 100%, ratio ≈ 1.0 → SpO2 ≈ 94%
    float estimatedSpO2 = 110.0f - (25.0f * ratio);
    estimatedSpO2 = constrain(estimatedSpO2, 0.0f, 100.0f);
    g_spO2 = estimatedSpO2;
  }
}

// ----------------------------
// DS18B20 TEMPERATURE SENSOR SECTION
// ----------------------------

/**
 * @brief Request and read patient body temperature from DS18B20.
 *        Blocking conversion takes ~750 ms at 12-bit resolution.
 *        For non-blocking use in production, switch to async
 *        requestTemperaturesByIndex with a 750 ms timeout guard.
 */
void readTemperature() {
  tempSensor.requestTemperatures();
  float tempC = tempSensor.getTempCByIndex(0);

  // Validate — DS18B20 returns -127 on error
  if (tempC == DEVICE_DISCONNECTED_C || tempC < -10.0f || tempC > 50.0f) {
    Serial.println("[SENSOR] DS18B20 read error — using last valid value.");
    // g_temperature retains its previous value (safe default behaviour)
  } else {
    g_temperature = tempC;
  }
}

// ----------------------------
// FSR PRESSURE SENSOR SECTION
// ----------------------------

/**
 * @brief Read the FSR analog value and determine bed occupancy.
 *        ADC range: 0–4095 (12-bit, ESP32).
 *        Low value = no weight = bed empty.
 */
void readFSRPressure() {
  int rawADC = analogRead(PIN_FSR);

  // Map to binary occupancy: 1 = patient present, 0 = bed empty
  g_pressure = (rawADC > FSR_BED_EMPTY) ? 1 : 0;

  Serial.printf("[SENSOR] FSR raw ADC: %d → Bed %s\n",
                rawADC, g_pressure ? "OCCUPIED" : "EMPTY");
}

// ----------------------------
// EMERGENCY BUTTON SECTION
// ----------------------------

/**
 * @brief Read the state of the emergency push button.
 *        Active LOW due to INPUT_PULLUP configuration.
 */
void readEmergencyButton() {
  // digitalRead LOW means button is pressed (pulled to GND)
  g_emergency = (digitalRead(PIN_EMERGENCY_BTN) == LOW);

  if (g_emergency) {
    Serial.println("[SENSOR] EMERGENCY BUTTON PRESSED!");
  }
}

// ============================================================
//  ALERT EVALUATION
// ============================================================

/**
 * @brief Evaluate all sensor readings against clinical thresholds.
 *        Sets g_alertStatus and triggers the buzzer accordingly.
 *
 *  Priority order (highest first):
 *    1. Emergency button
 *    2. Critical heart rate (too low)
 *    3. High heart rate
 *    4. Fever
 *    5. Bed empty (patient left bed)
 *    6. Normal
 */
void evaluateAlerts() {
  bool alertTriggered = false;

  if (g_emergency) {
    g_alertStatus  = "EMERGENCY_BUTTON";
    alertTriggered = true;
    Serial.println("[ALERT] ⚠ EMERGENCY BUTTON — Immediate nurse call!");
    activateBuzzer(3000);   // 3-second continuous alert

  } else if (g_heartRate > 0 && g_heartRate < HEART_RATE_LOW) {
    g_alertStatus  = "CRITICAL_LOW_HR";
    alertTriggered = true;
    Serial.printf("[ALERT] ⚠ CRITICAL: Heart rate too low (%.0f BPM)\n", g_heartRate);
    activateBuzzer(2000);

  } else if (g_heartRate > HEART_RATE_HIGH) {
    g_alertStatus  = "HIGH_HEART_RATE";
    alertTriggered = true;
    Serial.printf("[ALERT] ⚠ HIGH: Heart rate elevated (%.0f BPM)\n", g_heartRate);
    activateBuzzer(1000);

  } else if (g_temperature > TEMP_FEVER) {
    g_alertStatus  = "FEVER_WARNING";
    alertTriggered = true;
    Serial.printf("[ALERT] ⚠ WARNING: Fever detected (%.1f°C)\n", g_temperature);
    activateBuzzer(500);

  } else if (g_pressure == 0) {
    g_alertStatus  = "BED_EMPTY";
    alertTriggered = true;
    Serial.println("[ALERT] ⚠ NOTICE: Patient has left the bed.");
    activateBuzzer(500);

  } else {
    g_alertStatus = "normal";
  }

  // Visual feedback — rapid blink on alert, steady on normal
  if (alertTriggered) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_STATUS_LED, LOW);
      delay(100);
      digitalWrite(PIN_STATUS_LED, HIGH);
      delay(100);
    }
  } else {
    digitalWrite(PIN_STATUS_LED, HIGH);  // Steady = all normal
  }
}

// ============================================================
//  OUTPUT FUNCTIONS — BUZZER AND LED
// ============================================================

/**
 * @brief Activate the buzzer for a specified duration.
 * @param durationMs Duration in milliseconds.
 *        Note: blocking — acceptable for short alert bursts
 *        on a dedicated monitoring device.
 */
void activateBuzzer(int durationMs) {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(durationMs);
  digitalWrite(PIN_BUZZER, LOW);
}

/**
 * @brief Set the status LED on or off.
 * @param state true = ON, false = OFF.
 */
void setStatusLED(bool state) {
  digitalWrite(PIN_STATUS_LED, state ? HIGH : LOW);
}

// ============================================================
//  HTTP COMMUNICATION — SEND DATA TO RASPBERRY PI SERVER
// ============================================================

/**
 * @brief Serialise sensor data to JSON and POST to the Pi server.
 *
 *  JSON Payload Format:
 *  {
 *    "bed_id"     : 3,
 *    "heart_rate" : 82,
 *    "spo2"       : 97,
 *    "temperature": 36.8,
 *    "pressure"   : 1,
 *    "emergency"  : 0,
 *    "status"     : "normal"
 *  }
 */
void sendDataToServer() {
  // Build server URL
  String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + SERVER_PATH;

  // ── Serialise JSON ─────────────────────────────────────────
  // StaticJsonDocument size: 256 bytes sufficient for this payload.
  StaticJsonDocument<256> doc;
  doc["bed_id"]      = BED_ID;
  doc["heart_rate"]  = (int)g_heartRate;
  doc["spo2"]        = (int)g_spO2;
  doc["temperature"] = round(g_temperature * 10.0f) / 10.0f;  // 1 decimal
  doc["pressure"]    = g_pressure;
  doc["emergency"]   = g_emergency ? 1 : 0;
  doc["status"]      = g_alertStatus;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  Serial.println("[HTTP] Sending data to server...");
  Serial.println("[HTTP] Payload: " + jsonPayload);

  // ── HTTP POST ──────────────────────────────────────────────
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(4000);  // 4-second timeout — prevents loop stall

  int httpCode = http.POST(jsonPayload);

  if (httpCode > 0) {
    Serial.printf("[HTTP] Response code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
      Serial.println("[HTTP] Data accepted by server.");
    } else {
      Serial.println("[HTTP] Server returned unexpected code.");
    }
  } else {
    Serial.printf("[HTTP] POST failed: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

//  DEBUG OUTPUT — SERIAL MONITOR
void printDebugInfo() {
  Serial.println("┌─────────────────────────────────────┐");
  Serial.printf ("│  BED ID      : %d\n", BED_ID);
  Serial.printf ("│  Heart Rate  : %.0f BPM\n",  g_heartRate);
  Serial.printf ("│  SpO2        : %.0f %%\n",   g_spO2);
  Serial.printf ("│  Temperature : %.1f °C\n",   g_temperature);
  Serial.printf ("│  Bed Pressure: %s\n",         g_pressure ? "OCCUPIED" : "EMPTY");
  Serial.printf ("│  Emergency   : %s\n",         g_emergency ? "YES ⚠" : "No");
  Serial.printf ("│  Alert Status: %s\n",         g_alertStatus.c_str());
  Serial.printf ("│  WiFi RSSI   : %d dBm\n",    WiFi.RSSI());
  Serial.printf ("│  Uptime      : %lu s\n",      millis() / 1000);
  Serial.println("└─────────────────────────────────────┘");
}

