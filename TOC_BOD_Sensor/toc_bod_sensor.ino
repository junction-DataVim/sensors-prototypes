/*
 * TOC/BOD Sensor Module for Aquatic Monitoring System
 * 
 * This module measures Total Organic Carbon (TOC) and Biochemical Oxygen Demand (BOD)
 * Features: Advanced electrochemical sensors, temperature compensation, data logging
 * 
 * Hardware: ESP32 + TOC Sensor + BOD Sensor + Temperature Sensor
 * Communication: MQTT + REST API
 * 
 * Author: Aquatic Monitoring System
 * Version: 1.0
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

// Pin definitions
#define TOC_SENSOR_PIN 36       // Analog pin for TOC sensor
#define BOD_SENSOR_PIN 39       // Analog pin for BOD sensor
#define TEMP_SENSOR_PIN 4       // Temperature sensor pin
#define SAMPLE_VALVE_PIN 2      // Sample inlet control valve
#define REAGENT_PUMP_PIN 5      // Reagent pump control
#define STATUS_LED_PIN 19       // Status LED
#define HEATER_PIN 18           // Sample heater control

// Sensor constants
#define TOC_SAMPLES 10          // Number of TOC samples to average
#define BOD_SAMPLES 5           // Number of BOD samples
#define MEASUREMENT_INTERVAL 300000  // 5 minutes between measurements
#define CALIBRATION_INTERVAL 86400000  // 24 hours between auto-calibrations
#define SAMPLE_PREPARATION_TIME 120000  // 2 minutes sample prep

// TOC measurement parameters
#define TOC_REFERENCE_VOLTAGE 3.3
#define TOC_RESOLUTION 4095
#define TOC_ZERO_OFFSET 0.1     // Zero-point offset in mg/L
#define TOC_SPAN_FACTOR 1.25    // Span adjustment factor

// BOD measurement parameters
#define BOD_REFERENCE_VOLTAGE 3.3
#define BOD_RESOLUTION 4095
#define BOD_CALIBRATION_FACTOR 0.85
#define BOD_TEMPERATURE_COEFFICIENT 0.02  // 2% per °C

// Temperature sensor setup
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature temperatureSensor(&oneWire);

// WiFi and MQTT configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_BROKER";
const int mqtt_port = 1883;
const char* mqtt_topic = "aquatic/toc_bod";

// API configuration
const char* api_server = "YOUR_API_SERVER";
const int api_port = 3000;
const char* api_endpoint = "/api/toc-bod-readings";
const char* pool_id = "pool_001";

WiFiClient espClient;
PubSubClient client(espClient);
HTTPClient http;

// Calibration structure
struct CalibrationData {
  float toc_slope;
  float toc_intercept;
  float bod_slope;
  float bod_intercept;
  float temperature_coefficient;
  unsigned long last_calibration;
};

// Global variables
CalibrationData calibration;
float lastTOC = 0.0;
float lastBOD = 0.0;
float lastTemperature = 0.0;
bool sensorHealthy = true;
bool calibrationNeeded = false;
int consecutiveErrors = 0;
unsigned long lastMeasurement = 0;
unsigned long lastCalibration = 0;

// Sample preparation state
enum SampleState {
  IDLE,
  SAMPLING,
  HEATING,
  MEASURING,
  CLEANING
};

SampleState currentState = IDLE;
unsigned long stateStartTime = 0;

// Dummy data generation
bool dummyMode = false;
float dummyTOC = 25.0;
float dummyBOD = 15.0;
float dummyTrend = 0.1;

// Alert thresholds
float tocHighAlarm = 50.0;      // mg/L
float bodHighAlarm = 30.0;      // mg/L
float tocCriticalAlarm = 100.0; // mg/L
float bodCriticalAlarm = 60.0;  // mg/L

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(SAMPLE_VALVE_PIN, OUTPUT);
  pinMode(REAGENT_PUMP_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(HEATER_PIN, OUTPUT);
  
  // Initialize all outputs to OFF
  digitalWrite(SAMPLE_VALVE_PIN, LOW);
  digitalWrite(REAGENT_PUMP_PIN, LOW);
  digitalWrite(HEATER_PIN, LOW);
  
  // Initialize temperature sensor
  temperatureSensor.begin();
  
  // Initialize EEPROM and load calibration
  EEPROM.begin(512);
  loadCalibration();
  
  // Initialize default calibration if needed
  if (calibration.toc_slope == 0.0) {
    initializeDefaultCalibration();
  }
  
  // Connect to WiFi
  connectWiFi();
  
  // Initialize MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  
  Serial.println("TOC/BOD Sensor Module Initialized");
  Serial.println("Commands: CAL, DUMMY, RESET, STATUS, SAMPLE");
  
  // Initial sensor warm-up
  warmUpSensors();
}

void loop() {
  // Maintain connections
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  
  // Handle sample preparation state machine
  handleSampleStateMachine();
  
  // Check for serial commands
  handleSerialCommands();
  
  // Take measurements at regular intervals
  if (millis() - lastMeasurement > MEASUREMENT_INTERVAL) {
    startMeasurementCycle();
    lastMeasurement = millis();
  }
  
  // Check for automatic calibration
  if (millis() - lastCalibration > CALIBRATION_INTERVAL) {
    calibrationNeeded = true;
  }
  
  // Update status LED
  updateStatusLED();
  
  delay(1000);
}

void startMeasurementCycle() {
  if (currentState == IDLE) {
    currentState = SAMPLING;
    stateStartTime = millis();
    Serial.println("Starting measurement cycle...");
  }
}

void handleSampleStateMachine() {
  unsigned long stateTime = millis() - stateStartTime;
  
  switch (currentState) {
    case IDLE:
      // Waiting for next measurement
      break;
      
    case SAMPLING:
      // Open sample valve
      digitalWrite(SAMPLE_VALVE_PIN, HIGH);
      
      if (stateTime > 30000) { // 30 seconds sampling
        digitalWrite(SAMPLE_VALVE_PIN, LOW);
        currentState = HEATING;
        stateStartTime = millis();
        Serial.println("Sample collected, heating...");
      }
      break;
      
    case HEATING:
      // Heat sample for TOC analysis
      digitalWrite(HEATER_PIN, HIGH);
      
      if (stateTime > 60000) { // 1 minute heating
        digitalWrite(HEATER_PIN, LOW);
        currentState = MEASURING;
        stateStartTime = millis();
        Serial.println("Sample heated, measuring...");
      }
      break;
      
    case MEASURING:
      // Perform actual measurements
      if (stateTime > 5000) { // 5 seconds for measurement
        takeMeasurement();
        currentState = CLEANING;
        stateStartTime = millis();
      }
      break;
      
    case CLEANING:
      // Clean sample lines
      digitalWrite(REAGENT_PUMP_PIN, HIGH);
      
      if (stateTime > 15000) { // 15 seconds cleaning
        digitalWrite(REAGENT_PUMP_PIN, LOW);
        currentState = IDLE;
        Serial.println("Measurement cycle complete");
      }
      break;
  }
}

void takeMeasurement() {
  float toc, bod, temperature;
  
  if (dummyMode) {
    // Generate dummy data
    toc = generateDummyTOC();
    bod = generateDummyBOD();
    temperature = 22.0 + sin(millis() / 300000.0) * 3.0;
  } else {
    // Read actual sensor values
    toc = readTOC();
    bod = readBOD();
    temperature = readTemperature();
  }
  
  // Apply temperature compensation
  toc = applyTemperatureCompensation(toc, temperature, true);
  bod = applyTemperatureCompensation(bod, temperature, false);
  
  // Validate readings
  if (isValidReading(toc, bod, temperature)) {
    // Update global variables
    lastTOC = toc;
    lastBOD = bod;
    lastTemperature = temperature;
    sensorHealthy = true;
    consecutiveErrors = 0;
    
    // Create measurement data
    StaticJsonDocument<250> doc;
    doc["timestamp"] = millis();
    doc["toc_mgl"] = toc;
    doc["bod_mgl"] = bod;
    doc["temperature"] = temperature;
    doc["sensor_health"] = sensorHealthy;
    doc["pool_id"] = pool_id;
    
    // Send to MQTT
    sendMQTTData(doc);
    
    // Send to API
    sendAPIData(doc);
    
    // Log to serial
    Serial.printf("TOC: %.2f mg/L, BOD: %.2f mg/L, Temp: %.2f°C\\n", toc, bod, temperature);
    
    // Check for alerts
    checkAlerts(toc, bod);
    
  } else {
    handleSensorError();
  }
}

float readTOC() {
  float sum = 0.0;
  int validReadings = 0;
  
  // Take multiple samples
  for (int i = 0; i < TOC_SAMPLES; i++) {
    int rawValue = analogRead(TOC_SENSOR_PIN);
    float voltage = (rawValue * TOC_REFERENCE_VOLTAGE) / TOC_RESOLUTION;
    
    // Apply calibration
    float toc = (voltage - calibration.toc_intercept) / calibration.toc_slope;
    
    if (toc >= 0.0 && toc <= 1000.0) { // Valid range
      sum += toc;
      validReadings++;
    }
    
    delay(100);
  }
  
  float avgTOC = (validReadings > 0) ? (sum / validReadings) : -1.0;
  
  // Apply zero offset and span correction
  avgTOC = (avgTOC - TOC_ZERO_OFFSET) * TOC_SPAN_FACTOR;
  
  return avgTOC;
}

float readBOD() {
  float sum = 0.0;
  int validReadings = 0;
  
  // Take multiple samples
  for (int i = 0; i < BOD_SAMPLES; i++) {
    int rawValue = analogRead(BOD_SENSOR_PIN);
    float voltage = (rawValue * BOD_REFERENCE_VOLTAGE) / BOD_RESOLUTION;
    
    // Apply calibration
    float bod = (voltage - calibration.bod_intercept) / calibration.bod_slope;
    
    if (bod >= 0.0 && bod <= 500.0) { // Valid range
      sum += bod;
      validReadings++;
    }
    
    delay(200);
  }
  
  float avgBOD = (validReadings > 0) ? (sum / validReadings) : -1.0;
  
  // Apply calibration factor
  avgBOD *= BOD_CALIBRATION_FACTOR;
  
  return avgBOD;
}

float readTemperature() {
  temperatureSensor.requestTemperatures();
  return temperatureSensor.getTempCByIndex(0);
}

float applyTemperatureCompensation(float value, float temperature, bool isTOC) {
  float referenceTemp = 20.0; // Reference temperature
  float coefficient = isTOC ? 0.01 : BOD_TEMPERATURE_COEFFICIENT; // 1% per °C for TOC, 2% for BOD
  
  float compensation = 1.0 + coefficient * (temperature - referenceTemp);
  return value * compensation;
}

bool isValidReading(float toc, float bod, float temperature) {
  return (toc >= 0.0 && toc <= 1000.0 && 
          bod >= 0.0 && bod <= 500.0 && 
          temperature >= 0.0 && temperature <= 50.0);
}

void checkAlerts(float toc, float bod) {
  // TOC alerts
  if (toc > tocCriticalAlarm) {
    sendAlert("CRITICAL_TOC", toc);
  } else if (toc > tocHighAlarm) {
    sendAlert("HIGH_TOC", toc);
  }
  
  // BOD alerts
  if (bod > bodCriticalAlarm) {
    sendAlert("CRITICAL_BOD", bod);
  } else if (bod > bodHighAlarm) {
    sendAlert("HIGH_BOD", bod);
  }
  
  // Rapid change alerts
  if (abs(toc - lastTOC) > 20.0) {
    sendAlert("RAPID_TOC_CHANGE", toc);
  }
  
  if (abs(bod - lastBOD) > 10.0) {
    sendAlert("RAPID_BOD_CHANGE", bod);
  }
  
  // High organic load correlation
  if (toc > 30.0 && bod > 20.0) {
    sendAlert("HIGH_ORGANIC_LOAD", toc + bod);
  }
}

void sendAlert(const char* alertType, float value) {
  StaticJsonDocument<200> alert;
  alert["type"] = alertType;
  alert["value"] = value;
  alert["timestamp"] = millis();
  alert["sensor"] = "toc_bod";
  alert["toc"] = lastTOC;
  alert["bod"] = lastBOD;
  
  String alertMsg;
  serializeJson(alert, alertMsg);
  client.publish("aquatic/alerts", alertMsg.c_str());
  
  Serial.printf("ALERT: %s - Value: %.2f\\n", alertType, value);
}

float generateDummyTOC() {
  // Simulate realistic TOC variations
  dummyTOC += dummyTrend * (random(-10, 11) / 10.0);
  
  // Add daily variation (higher during feeding)
  float dailyVariation = sin((millis() / 3600000.0) * 2 * PI) * 5.0;
  dummyTOC += dailyVariation;
  
  // Add noise
  dummyTOC += random(-100, 101) / 100.0; // ±1.0 mg/L noise
  
  // Simulate organic load events
  if (random(0, 1000) < 3) { // 0.3% chance per measurement
    dummyTOC += random(10, 30); // 10-30 mg/L increase
  }
  
  // Keep within realistic bounds
  dummyTOC = constrain(dummyTOC, 5.0, 80.0);
  
  return dummyTOC;
}

float generateDummyBOD() {
  // BOD typically correlates with TOC
  dummyBOD = dummyTOC * 0.6 + random(-300, 301) / 100.0; // ±3.0 mg/L variation
  
  // Add biological activity variation
  float bioVariation = sin((millis() / 7200000.0) * 2 * PI) * 3.0;
  dummyBOD += bioVariation;
  
  // Keep within realistic bounds
  dummyBOD = constrain(dummyBOD, 2.0, 50.0);
  
  return dummyBOD;
}

void initializeDefaultCalibration() {
  calibration.toc_slope = 0.05;    // V/(mg/L)
  calibration.toc_intercept = 0.1; // V
  calibration.bod_slope = 0.04;    // V/(mg/L)
  calibration.bod_intercept = 0.08; // V
  calibration.temperature_coefficient = 0.02; // 2%/°C
  calibration.last_calibration = millis();
  
  saveCalibration();
}

void loadCalibration() {
  EEPROM.get(0, calibration);
}

void saveCalibration() {
  EEPROM.put(0, calibration);
  EEPROM.commit();
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\\n');
    command.trim();
    command.toUpperCase();
    
    if (command == "CAL") {
      performCalibration();
    } else if (command == "DUMMY") {
      dummyMode = !dummyMode;
      Serial.printf("Dummy mode: %s\\n", dummyMode ? "ON" : "OFF");
    } else if (command == "RESET") {
      initializeDefaultCalibration();
      Serial.println("Calibration reset to defaults");
    } else if (command == "STATUS") {
      printStatus();
    } else if (command == "SAMPLE") {
      if (currentState == IDLE) {
        startMeasurementCycle();
      }
    }
  }
}

void performCalibration() {
  Serial.println("Starting TOC/BOD calibration...");
  Serial.println("Prepare standard solutions and press Enter:");
  
  // TOC calibration
  Serial.println("\\n--- TOC Calibration ---");
  Serial.println("Standard 1 (0 mg/L): ");
  while (!Serial.available()) delay(100);
  Serial.readStringUntil('\\n');
  
  float toc_zero = readTOCRaw();
  Serial.printf("Zero reading: %.3f V\\n", toc_zero);
  
  Serial.println("Standard 2 (100 mg/L): ");
  while (!Serial.available()) delay(100);
  Serial.readStringUntil('\\n');
  
  float toc_span = readTOCRaw();
  Serial.printf("Span reading: %.3f V\\n", toc_span);
  
  calibration.toc_intercept = toc_zero;
  calibration.toc_slope = (toc_span - toc_zero) / 100.0;
  
  // BOD calibration
  Serial.println("\\n--- BOD Calibration ---");
  Serial.println("Standard 1 (0 mg/L): ");
  while (!Serial.available()) delay(100);
  Serial.readStringUntil('\\n');
  
  float bod_zero = readBODRaw();
  Serial.printf("Zero reading: %.3f V\\n", bod_zero);
  
  Serial.println("Standard 2 (50 mg/L): ");
  while (!Serial.available()) delay(100);
  Serial.readStringUntil('\\n');
  
  float bod_span = readBODRaw();
  Serial.printf("Span reading: %.3f V\\n", bod_span);
  
  calibration.bod_intercept = bod_zero;
  calibration.bod_slope = (bod_span - bod_zero) / 50.0;
  
  calibration.last_calibration = millis();
  saveCalibration();
  
  Serial.println("Calibration completed and saved!");
}

float readTOCRaw() {
  float sum = 0.0;
  for (int i = 0; i < 10; i++) {
    int rawValue = analogRead(TOC_SENSOR_PIN);
    sum += (rawValue * TOC_REFERENCE_VOLTAGE) / TOC_RESOLUTION;
    delay(100);
  }
  return sum / 10.0;
}

float readBODRaw() {
  float sum = 0.0;
  for (int i = 0; i < 10; i++) {
    int rawValue = analogRead(BOD_SENSOR_PIN);
    sum += (rawValue * BOD_REFERENCE_VOLTAGE) / BOD_RESOLUTION;
    delay(100);
  }
  return sum / 10.0;
}

void printStatus() {
  Serial.println("\\n=== TOC/BOD Sensor Status ===");
  Serial.printf("Last TOC: %.2f mg/L\\n", lastTOC);
  Serial.printf("Last BOD: %.2f mg/L\\n", lastBOD);
  Serial.printf("Last Temperature: %.2f°C\\n", lastTemperature);
  Serial.printf("Sensor Health: %s\\n", sensorHealthy ? "OK" : "ERROR");
  Serial.printf("Sample State: %s\\n", getStateString(currentState));
  Serial.printf("Consecutive Errors: %d\\n", consecutiveErrors);
  Serial.printf("Dummy Mode: %s\\n", dummyMode ? "ON" : "OFF");
  Serial.printf("Calibration Needed: %s\\n", calibrationNeeded ? "YES" : "NO");
  Serial.println("\\nCalibration Parameters:");
  Serial.printf("  TOC Slope: %.4f V/(mg/L)\\n", calibration.toc_slope);
  Serial.printf("  TOC Intercept: %.4f V\\n", calibration.toc_intercept);
  Serial.printf("  BOD Slope: %.4f V/(mg/L)\\n", calibration.bod_slope);
  Serial.printf("  BOD Intercept: %.4f V\\n", calibration.bod_intercept);
  Serial.println("\\nAlert Thresholds:");
  Serial.printf("  TOC High: %.1f mg/L\\n", tocHighAlarm);
  Serial.printf("  TOC Critical: %.1f mg/L\\n", tocCriticalAlarm);
  Serial.printf("  BOD High: %.1f mg/L\\n", bodHighAlarm);
  Serial.printf("  BOD Critical: %.1f mg/L\\n", bodCriticalAlarm);
  Serial.println("============================\\n");
}

const char* getStateString(SampleState state) {
  switch (state) {
    case IDLE: return "IDLE";
    case SAMPLING: return "SAMPLING";
    case HEATING: return "HEATING";
    case MEASURING: return "MEASURING";
    case CLEANING: return "CLEANING";
    default: return "UNKNOWN";
  }
}

void handleSensorError() {
  consecutiveErrors++;
  sensorHealthy = (consecutiveErrors < 3);
  
  Serial.printf("Sensor error #%d\\n", consecutiveErrors);
  
  if (consecutiveErrors >= 5) {
    // Reset to idle state
    currentState = IDLE;
    digitalWrite(SAMPLE_VALVE_PIN, LOW);
    digitalWrite(REAGENT_PUMP_PIN, LOW);
    digitalWrite(HEATER_PIN, LOW);
    consecutiveErrors = 0;
  }
}

void warmUpSensors() {
  Serial.println("Warming up sensors...");
  for (int i = 0; i < 20; i++) {
    analogRead(TOC_SENSOR_PIN);
    analogRead(BOD_SENSOR_PIN);
    delay(250);
  }
  Serial.println("Sensors ready");
}

void updateStatusLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  
  if (sensorHealthy) {
    if (currentState == IDLE) {
      digitalWrite(STATUS_LED_PIN, HIGH);
    } else {
      // Blink during measurement
      if (millis() - lastBlink > 250) {
        ledState = !ledState;
        digitalWrite(STATUS_LED_PIN, ledState);
        lastBlink = millis();
      }
    }
  } else {
    // Fast blink for error
    if (millis() - lastBlink > 100) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState);
      lastBlink = millis();
    }
  }
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect("TOCBODSensor")) {
      Serial.println("connected");
      client.subscribe("aquatic/commands");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  if (message == "calibrate") {
    calibrationNeeded = true;
  } else if (message == "sample") {
    if (currentState == IDLE) {
      startMeasurementCycle();
    }
  } else if (message == "dummy_on") {
    dummyMode = true;
  } else if (message == "dummy_off") {
    dummyMode = false;
  }
}

void sendMQTTData(const StaticJsonDocument<250>& doc) {
  String message;
  serializeJson(doc, message);
  client.publish(mqtt_topic, message.c_str());
}

void sendAPIData(const StaticJsonDocument<250>& doc) {
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(String("http://") + api_server + ":" + api_port + api_endpoint);
    http.addHeader("Content-Type", "application/json");
    
    String payload;
    serializeJson(doc, payload);
    
    int httpResponseCode = http.POST(payload);
    
    if (httpResponseCode > 0) {
      Serial.printf("API Response: %d\\n", httpResponseCode);
    } else {
      Serial.printf("API Error: %d\\n", httpResponseCode);
    }
    
    http.end();
  }
}
