/*
 * Water Level and Flow Sensor Module for Aquatic Monitoring System
 * 
 * This module monitors water level using ultrasonic sensor and flow rate using hall effect sensor
 * Features: Level monitoring, flow measurement, leak detection, pump control integration
 * 
 * Hardware: ESP32 + HC-SR04 + YF-S201 Flow Sensor
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

// Pin definitions
#define TRIG_PIN 5              // Ultrasonic trigger pin
#define ECHO_PIN 18             // Ultrasonic echo pin
#define FLOW_SENSOR_PIN 2       // Flow sensor input pin
#define PUMP_CONTROL_PIN 4      // Pump control relay
#define STATUS_LED_PIN 19       // Status LED
#define TEMP_SENSOR_PIN 21      // Temperature sensor

// Sensor constants
#define SOUND_SPEED 0.034       // cm/µs (speed of sound)
#define LEVEL_SAMPLES 5         // Number of level samples to average
#define FLOW_SAMPLES 10         // Number of flow samples
#define MEASUREMENT_INTERVAL 10000  // 10 seconds between measurements
#define FLOW_PULSE_TIMEOUT 2000000  // 2 seconds timeout for flow pulses

// Tank configuration
#define TANK_HEIGHT 200.0       // Total tank height in cm
#define SENSOR_HEIGHT 210.0     // Sensor height from bottom in cm
#define TANK_DIAMETER 100.0     // Tank diameter in cm (for volume calculation)
#define PIPE_DIAMETER 2.54      // Pipe diameter in cm (1 inch)

// Flow sensor calibration (pulses per liter)
#define FLOW_CALIBRATION_FACTOR 450.0  // Pulses per liter for YF-S201

// Temperature sensor setup
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature temperatureSensor(&oneWire);

// WiFi and MQTT configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_BROKER";
const int mqtt_port = 1883;
const char* mqtt_topic = "aquatic/water_level";

// API configuration
const char* api_server = "YOUR_API_SERVER";
const int api_port = 3000;
const char* api_endpoint = "/api/water-level-readings";
const char* pool_id = "pool_001";

WiFiClient espClient;
PubSubClient client(espClient);
HTTPClient http;

// Global variables
volatile unsigned long flowPulseCount = 0;
volatile unsigned long lastFlowPulse = 0;
float waterLevel = 0.0;
float flowRate = 0.0;
float totalVolume = 0.0;
float lastWaterLevel = 0.0;
bool pumpStatus = false;
bool sensorHealthy = true;
int consecutiveErrors = 0;
unsigned long lastMeasurement = 0;
unsigned long lastVolumeReset = 0;

// Dummy data generation
bool dummyMode = false;
float dummyLevel = 75.0;
float dummyFlow = 5.0;
float dummyTrend = 0.1;

// Alarm thresholds
float lowLevelAlarm = 20.0;     // cm
float highLevelAlarm = 180.0;   // cm
float lowFlowAlarm = 1.0;       // L/min
float highFlowAlarm = 25.0;     // L/min

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  pinMode(PUMP_CONTROL_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  
  // Initialize flow sensor interrupt
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulseISR, FALLING);
  
  // Initialize temperature sensor
  temperatureSensor.begin();
  
  // Initialize EEPROM
  EEPROM.begin(512);
  loadSettings();
  
  // Connect to WiFi
  connectWiFi();
  
  // Initialize MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  
  Serial.println("Water Level and Flow Sensor Module Initialized");
  Serial.println("Commands: DUMMY, RESET, PUMP_ON, PUMP_OFF, STATUS, CALIBRATE");
  
  // Initial readings
  delay(2000);
  takeMeasurement();
}

void loop() {
  // Maintain connections
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  
  // Check for serial commands
  handleSerialCommands();
  
  // Take measurements at regular intervals
  if (millis() - lastMeasurement > MEASUREMENT_INTERVAL) {
    takeMeasurement();
    lastMeasurement = millis();
  }
  
  // Update status LED
  updateStatusLED();
  
  // Check for leaks and anomalies
  checkForLeaks();
  
  delay(1000);
}

void takeMeasurement() {
  float level, flow, temperature;
  
  if (dummyMode) {
    // Generate dummy data
    level = generateDummyLevel();
    flow = generateDummyFlow();
    temperature = 22.0 + sin(millis() / 300000.0) * 3.0;
  } else {
    // Read actual sensor values
    level = readWaterLevel();
    flow = readFlowRate();
    temperature = readTemperature();
  }
  
  // Validate readings
  if (isValidReading(level, flow, temperature)) {
    // Update global variables
    waterLevel = level;
    flowRate = flow;
    
    // Calculate volume based on level
    float currentVolume = calculateVolume(level);
    
    // Update total flow volume
    updateTotalVolume(flow);
    
    sensorHealthy = true;
    consecutiveErrors = 0;
    
    // Create measurement data
    StaticJsonDocument<300> doc;
    doc["timestamp"] = millis();
    doc["water_level_cm"] = level;
    doc["flow_rate_lpm"] = flow;
    doc["total_volume_l"] = totalVolume;
    doc["current_volume_l"] = currentVolume;
    doc["pump_status"] = pumpStatus;
    doc["temperature"] = temperature;
    doc["sensor_health"] = sensorHealthy;
    doc["pool_id"] = pool_id;
    
    // Send to MQTT
    sendMQTTData(doc);
    
    // Send to API
    sendAPIData(doc);
    
    // Log to serial
    Serial.printf("Level: %.1f cm, Flow: %.2f L/min, Volume: %.1f L, Temp: %.1f°C\\n", 
                  level, flow, currentVolume, temperature);
    
    // Check for alerts
    checkAlerts(level, flow);
    
    // Automatic pump control
    automaticPumpControl(level, flow);
    
    lastWaterLevel = level;
    
  } else {
    handleSensorError();
  }
}

float readWaterLevel() {
  float sum = 0.0;
  int validReadings = 0;
  
  for (int i = 0; i < LEVEL_SAMPLES; i++) {
    // Clear trigger
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    
    // Send trigger pulse
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    // Read echo duration
    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
    
    if (duration > 0) {
      // Calculate distance
      float distance = duration * SOUND_SPEED / 2.0;
      float level = SENSOR_HEIGHT - distance;
      
      if (level >= 0 && level <= TANK_HEIGHT) {
        sum += level;
        validReadings++;
      }
    }
    
    delay(100);
  }
  
  return (validReadings > 0) ? (sum / validReadings) : -1.0;
}

float readFlowRate() {
  // Reset pulse counter
  noInterrupts();
  unsigned long pulses = flowPulseCount;
  flowPulseCount = 0;
  interrupts();
  
  // Calculate flow rate (L/min)
  float flowRate = (pulses * 60.0) / (FLOW_CALIBRATION_FACTOR * (MEASUREMENT_INTERVAL / 1000.0));
  
  return flowRate;
}

float readTemperature() {
  temperatureSensor.requestTemperatures();
  return temperatureSensor.getTempCByIndex(0);
}

void IRAM_ATTR flowPulseISR() {
  flowPulseCount++;
  lastFlowPulse = millis();
}

float calculateVolume(float level) {
  // Volume calculation for cylindrical tank
  float radius = TANK_DIAMETER / 2.0;
  float volume = PI * radius * radius * level / 1000.0; // Convert to liters
  return volume;
}

void updateTotalVolume(float currentFlowRate) {
  // Add flow volume since last measurement
  float timeDelta = (millis() - lastMeasurement) / 60000.0; // Convert to minutes
  totalVolume += currentFlowRate * timeDelta;
  
  // Reset total volume daily
  if (millis() - lastVolumeReset > 86400000) { // 24 hours
    totalVolume = 0.0;
    lastVolumeReset = millis();
  }
}

bool isValidReading(float level, float flow, float temperature) {
  return (level >= 0 && level <= TANK_HEIGHT &&
          flow >= 0 && flow <= 50.0 &&
          temperature >= 0 && temperature <= 50.0);
}

void checkAlerts(float level, float flow) {
  // Water level alerts
  if (level < lowLevelAlarm) {
    sendAlert("LOW_WATER_LEVEL", level);
  } else if (level > highLevelAlarm) {
    sendAlert("HIGH_WATER_LEVEL", level);
  }
  
  // Flow rate alerts
  if (flow < lowFlowAlarm && pumpStatus) {
    sendAlert("LOW_FLOW_RATE", flow);
  } else if (flow > highFlowAlarm) {
    sendAlert("HIGH_FLOW_RATE", flow);
  }
  
  // Rapid level change
  if (abs(level - lastWaterLevel) > 5.0) {
    sendAlert("RAPID_LEVEL_CHANGE", level);
  }
  
  // No flow detected when pump is on
  if (pumpStatus && flow < 0.5) {
    sendAlert("PUMP_NO_FLOW", flow);
  }
}

void checkForLeaks() {
  // Leak detection logic
  static unsigned long lastLeakCheck = 0;
  static float lastLeakLevel = 0.0;
  
  if (millis() - lastLeakCheck > 300000) { // Check every 5 minutes
    if (!pumpStatus && (lastLeakLevel - waterLevel) > 2.0) {
      // Water level dropped significantly without pump running
      sendAlert("POSSIBLE_LEAK", waterLevel);
    }
    lastLeakLevel = waterLevel;
    lastLeakCheck = millis();
  }
}

void automaticPumpControl(float level, float flow) {
  // Automatic pump control logic
  if (level < 30.0 && !pumpStatus) {
    // Start pump when level is low
    controlPump(true);
    sendAlert("PUMP_AUTO_START", level);
  } else if (level > 170.0 && pumpStatus) {
    // Stop pump when level is high
    controlPump(false);
    sendAlert("PUMP_AUTO_STOP", level);
  }
}

void controlPump(bool enable) {
  pumpStatus = enable;
  digitalWrite(PUMP_CONTROL_PIN, enable ? HIGH : LOW);
  
  // Log pump state change
  StaticJsonDocument<150> pumpLog;
  pumpLog["timestamp"] = millis();
  pumpLog["pump_status"] = enable;
  pumpLog["water_level"] = waterLevel;
  pumpLog["flow_rate"] = flowRate;
  
  String logMsg;
  serializeJson(pumpLog, logMsg);
  client.publish("aquatic/pump_control", logMsg.c_str());
}

void sendAlert(const char* alertType, float value) {
  StaticJsonDocument<200> alert;
  alert["type"] = alertType;
  alert["value"] = value;
  alert["timestamp"] = millis();
  alert["sensor"] = "water_level_flow";
  alert["water_level"] = waterLevel;
  alert["flow_rate"] = flowRate;
  
  String alertMsg;
  serializeJson(alert, alertMsg);
  client.publish("aquatic/alerts", alertMsg.c_str());
  
  Serial.printf("ALERT: %s - Value: %.2f\\n", alertType, value);
}

float generateDummyLevel() {
  // Simulate realistic water level variations
  dummyLevel += dummyTrend * (random(-5, 6) / 10.0);
  
  // Add pump effects
  if (pumpStatus) {
    dummyLevel += 0.5; // Pump increases level
  } else {
    dummyLevel -= 0.1; // Natural evaporation/consumption
  }
  
  // Add random variations
  dummyLevel += random(-20, 21) / 100.0; // ±0.2 cm noise
  
  // Keep within realistic bounds
  dummyLevel = constrain(dummyLevel, 10.0, 190.0);
  
  return dummyLevel;
}

float generateDummyFlow() {
  // Simulate realistic flow variations
  if (pumpStatus) {
    dummyFlow = 8.0 + random(-100, 101) / 100.0; // 7-9 L/min when pump on
  } else {
    dummyFlow = 0.0 + random(0, 21) / 100.0; // 0-0.2 L/min when pump off
  }
  
  return dummyFlow;
}

void loadSettings() {
  int addr = 0;
  EEPROM.get(addr, lowLevelAlarm);
  addr += sizeof(float);
  EEPROM.get(addr, highLevelAlarm);
  addr += sizeof(float);
  EEPROM.get(addr, lowFlowAlarm);
  addr += sizeof(float);
  EEPROM.get(addr, highFlowAlarm);
  addr += sizeof(float);
  EEPROM.get(addr, totalVolume);
}

void saveSettings() {
  int addr = 0;
  EEPROM.put(addr, lowLevelAlarm);
  addr += sizeof(float);
  EEPROM.put(addr, highLevelAlarm);
  addr += sizeof(float);
  EEPROM.put(addr, lowFlowAlarm);
  addr += sizeof(float);
  EEPROM.put(addr, highFlowAlarm);
  addr += sizeof(float);
  EEPROM.put(addr, totalVolume);
  EEPROM.commit();
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\\n');
    command.trim();
    command.toUpperCase();
    
    if (command == "DUMMY") {
      dummyMode = !dummyMode;
      Serial.printf("Dummy mode: %s\\n", dummyMode ? "ON" : "OFF");
    } else if (command == "PUMP_ON") {
      controlPump(true);
      Serial.println("Pump turned ON");
    } else if (command == "PUMP_OFF") {
      controlPump(false);
      Serial.println("Pump turned OFF");
    } else if (command == "RESET") {
      totalVolume = 0.0;
      flowPulseCount = 0;
      Serial.println("Flow totals reset");
    } else if (command == "STATUS") {
      printStatus();
    } else if (command == "CALIBRATE") {
      performCalibration();
    }
  }
}

void performCalibration() {
  Serial.println("Starting calibration...");
  Serial.println("1. Level calibration - place sensor at known height");
  Serial.println("2. Flow calibration - use known flow rate");
  Serial.println("Enter calibration mode (Y/N)?");
  
  while (!Serial.available()) delay(100);
  String response = Serial.readStringUntil('\\n');
  
  if (response.charAt(0) == 'Y' || response.charAt(0) == 'y') {
    // Perform calibration procedures
    Serial.println("Calibration mode activated");
    // Implementation would go here
  }
}

void printStatus() {
  Serial.println("\\n=== Water Level & Flow Status ===");
  Serial.printf("Water Level: %.1f cm\\n", waterLevel);
  Serial.printf("Flow Rate: %.2f L/min\\n", flowRate);
  Serial.printf("Total Volume: %.1f L\\n", totalVolume);
  Serial.printf("Pump Status: %s\\n", pumpStatus ? "ON" : "OFF");
  Serial.printf("Sensor Health: %s\\n", sensorHealthy ? "OK" : "ERROR");
  Serial.printf("Consecutive Errors: %d\\n", consecutiveErrors);
  Serial.printf("Dummy Mode: %s\\n", dummyMode ? "ON" : "OFF");
  Serial.println("\\nAlarm Thresholds:");
  Serial.printf("  Low Level: %.1f cm\\n", lowLevelAlarm);
  Serial.printf("  High Level: %.1f cm\\n", highLevelAlarm);
  Serial.printf("  Low Flow: %.1f L/min\\n", lowFlowAlarm);
  Serial.printf("  High Flow: %.1f L/min\\n", highFlowAlarm);
  Serial.println("================================\\n");
}

void handleSensorError() {
  consecutiveErrors++;
  sensorHealthy = (consecutiveErrors < 3);
  
  Serial.printf("Sensor error #%d\\n", consecutiveErrors);
  
  if (consecutiveErrors >= 5) {
    // Emergency pump control
    controlPump(false);
    Serial.println("Emergency pump shutdown due to sensor failure");
  }
}

void updateStatusLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  
  if (sensorHealthy) {
    digitalWrite(STATUS_LED_PIN, HIGH);
  } else {
    if (millis() - lastBlink > 500) {
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
    
    if (client.connect("WaterLevelFlowSensor")) {
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
  
  if (message == "pump_on") {
    controlPump(true);
  } else if (message == "pump_off") {
    controlPump(false);
  } else if (message == "dummy_on") {
    dummyMode = true;
  } else if (message == "dummy_off") {
    dummyMode = false;
  } else if (message == "reset_volume") {
    totalVolume = 0.0;
    flowPulseCount = 0;
  }
}

void sendMQTTData(const StaticJsonDocument<300>& doc) {
  String message;
  serializeJson(doc, message);
  client.publish(mqtt_topic, message.c_str());
}

void sendAPIData(const StaticJsonDocument<300>& doc) {
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
