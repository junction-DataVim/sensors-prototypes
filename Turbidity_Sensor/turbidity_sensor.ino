/*
 * Turbidity Sensor Module for Aquatic Monitoring System
 * 
 * This module measures water turbidity using a TSS sensor with ESP32
 * Features: Multi-point calibration, temperature compensation, API integration
 * 
 * Hardware: ESP32 + TSS-300B Turbidity Sensor
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
#define TURBIDITY_PIN 36        // Analog pin for turbidity sensor
#define TEMP_SENSOR_PIN 4       // Temperature sensor pin
#define POWER_ENABLE_PIN 2      // Power control for sensor
#define STATUS_LED_PIN 5        // Status LED

// Sensor constants
#define TURBIDITY_SAMPLES 10    // Number of samples to average
#define CALIBRATION_POINTS 4    // Number of calibration points
#define MEASUREMENT_INTERVAL 30000  // 30 seconds between measurements
#define CLEANING_CYCLE_DETECT 300000  // 5 minutes for cleaning detection

// Temperature sensor setup
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature temperatureSensor(&oneWire);

// WiFi and MQTT configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_BROKER";
const int mqtt_port = 1883;
const char* mqtt_topic = "aquatic/turbidity";

// API configuration
const char* api_server = "YOUR_API_SERVER";
const int api_port = 3000;
const char* api_endpoint = "/api/turbidity-readings";
const char* pool_id = "pool_001";

WiFiClient espClient;
PubSubClient client(espClient);
HTTPClient http;

// Calibration structure
struct CalibrationPoint {
  float voltage;
  float ntu;
};

// Global variables
CalibrationPoint calibration[CALIBRATION_POINTS];
float lastTurbidity = 0.0;
float lastTemperature = 0.0;
unsigned long lastMeasurement = 0;
unsigned long lastCleaning = 0;
bool sensorHealthy = true;
int consecutiveErrors = 0;

// Dummy data generation for testing
bool dummyMode = false;
float dummyTurbidity = 15.0;
float dummyTrend = 0.1;

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(POWER_ENABLE_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(POWER_ENABLE_PIN, HIGH);
  
  // Initialize EEPROM
  EEPROM.begin(512);
  
  // Initialize temperature sensor
  temperatureSensor.begin();
  
  // Load calibration data
  loadCalibration();
  
  // Initialize default calibration if not present
  if (calibration[0].voltage == 0.0) {
    initializeDefaultCalibration();
  }
  
  // Connect to WiFi
  connectWiFi();
  
  // Initialize MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  
  Serial.println("Turbidity Sensor Module Initialized");
  Serial.println("Commands: CAL, DUMMY, RESET, STATUS");
  
  // Initial sensor warm-up
  warmUpSensor();
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
  
  delay(1000);
}

void takeMeasurement() {
  float turbidity, temperature;
  
  if (dummyMode) {
    // Generate dummy data with realistic variations
    turbidity = generateDummyTurbidity();
    temperature = 22.0 + sin(millis() / 300000.0) * 3.0; // 19-25°C variation
  } else {
    // Read actual sensor values
    turbidity = readTurbidity();
    temperature = readTemperature();
  }
  
  // Validate readings
  if (isValidReading(turbidity, temperature)) {
    // Apply temperature compensation
    turbidity = applyTemperatureCompensation(turbidity, temperature);
    
    // Update global variables
    lastTurbidity = turbidity;
    lastTemperature = temperature;
    sensorHealthy = true;
    consecutiveErrors = 0;
    
    // Create measurement data
    StaticJsonDocument<200> doc;
    doc["timestamp"] = millis();
    doc["turbidity_ntu"] = turbidity;
    doc["temperature"] = temperature;
    doc["sensor_health"] = sensorHealthy;
    doc["pool_id"] = pool_id;
    
    // Send to MQTT
    sendMQTTData(doc);
    
    // Send to API
    sendAPIData(doc);
    
    // Log to serial
    Serial.printf("Turbidity: %.2f NTU, Temp: %.2f°C\\n", turbidity, temperature);
    
    // Check for alerts
    checkAlerts(turbidity);
    
  } else {
    handleSensorError();
  }
}

float readTurbidity() {
  float sum = 0.0;
  int validReadings = 0;
  
  // Enable sensor power
  digitalWrite(POWER_ENABLE_PIN, HIGH);
  delay(100); // Power stabilization
  
  // Take multiple samples
  for (int i = 0; i < TURBIDITY_SAMPLES; i++) {
    int rawValue = analogRead(TURBIDITY_PIN);
    float voltage = (rawValue * 3.3) / 4095.0;
    
    // Convert voltage to NTU using calibration
    float ntu = voltageToNTU(voltage);
    
    if (ntu >= 0.0 && ntu <= 4000.0) { // Valid range
      sum += ntu;
      validReadings++;
    }
    
    delay(50);
  }
  
  return (validReadings > 0) ? (sum / validReadings) : -1.0;
}

float readTemperature() {
  temperatureSensor.requestTemperatures();
  return temperatureSensor.getTempCByIndex(0);
}

float voltageToNTU(float voltage) {
  // Linear interpolation between calibration points
  for (int i = 0; i < CALIBRATION_POINTS - 1; i++) {
    if (voltage >= calibration[i].voltage && voltage <= calibration[i + 1].voltage) {
      float slope = (calibration[i + 1].ntu - calibration[i].ntu) / 
                    (calibration[i + 1].voltage - calibration[i].voltage);
      return calibration[i].ntu + slope * (voltage - calibration[i].voltage);
    }
  }
  
  // Extrapolation for out-of-range values
  if (voltage < calibration[0].voltage) {
    return calibration[0].ntu;
  } else {
    return calibration[CALIBRATION_POINTS - 1].ntu;
  }
}

float applyTemperatureCompensation(float turbidity, float temperature) {
  // Temperature compensation factor (NTU varies ~2% per °C)
  float referenceTemp = 20.0; // Reference temperature
  float compensation = 1.0 + 0.02 * (temperature - referenceTemp);
  return turbidity * compensation;
}

bool isValidReading(float turbidity, float temperature) {
  return (turbidity >= 0.0 && turbidity <= 4000.0 && 
          temperature >= 0.0 && temperature <= 50.0);
}

void checkAlerts(float turbidity) {
  // High turbidity alert
  if (turbidity > 100.0) {
    sendAlert("HIGH_TURBIDITY", turbidity);
  }
  
  // Rapid change alert
  if (abs(turbidity - lastTurbidity) > 50.0) {
    sendAlert("RAPID_CHANGE", turbidity);
  }
  
  // Cleaning cycle detection
  if (turbidity < 5.0 && lastTurbidity > 50.0) {
    lastCleaning = millis();
    sendAlert("CLEANING_DETECTED", turbidity);
  }
}

void sendAlert(const char* alertType, float value) {
  StaticJsonDocument<150> alert;
  alert["type"] = alertType;
  alert["value"] = value;
  alert["timestamp"] = millis();
  alert["sensor"] = "turbidity";
  
  String alertMsg;
  serializeJson(alert, alertMsg);
  client.publish("aquatic/alerts", alertMsg.c_str());
}

float generateDummyTurbidity() {
  // Simulate realistic turbidity variations
  dummyTurbidity += dummyTrend * (random(-10, 11) / 10.0);
  
  // Add noise and trends
  dummyTurbidity += random(-50, 51) / 100.0; // ±0.5 NTU noise
  
  // Simulate feeding events (turbidity spike)
  if (random(0, 1000) < 5) { // 0.5% chance per measurement
    dummyTurbidity += random(20, 50); // 20-50 NTU increase
  }
  
  // Simulate cleaning events
  if (random(0, 1000) < 2) { // 0.2% chance per measurement
    dummyTurbidity = random(5, 15); // Reset to low values
  }
  
  // Keep within realistic bounds
  dummyTurbidity = constrain(dummyTurbidity, 5.0, 200.0);
  
  return dummyTurbidity;
}

void initializeDefaultCalibration() {
  // Default calibration points (voltage -> NTU)
  calibration[0] = {0.5, 0.0};     // 0 NTU
  calibration[1] = {1.2, 100.0};   // 100 NTU
  calibration[2] = {2.1, 1000.0};  // 1000 NTU
  calibration[3] = {2.8, 4000.0};  // 4000 NTU
  
  saveCalibration();
}

void loadCalibration() {
  int addr = 0;
  for (int i = 0; i < CALIBRATION_POINTS; i++) {
    EEPROM.get(addr, calibration[i]);
    addr += sizeof(CalibrationPoint);
  }
}

void saveCalibration() {
  int addr = 0;
  for (int i = 0; i < CALIBRATION_POINTS; i++) {
    EEPROM.put(addr, calibration[i]);
    addr += sizeof(CalibrationPoint);
  }
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
    }
  }
}

void performCalibration() {
  Serial.println("Starting calibration procedure...");
  Serial.println("Place sensor in standards and press Enter:");
  
  for (int i = 0; i < CALIBRATION_POINTS; i++) {
    Serial.printf("Standard %d (%.1f NTU): ", i + 1, calibration[i].ntu);
    
    while (!Serial.available()) delay(100);
    Serial.readStringUntil('\\n');
    
    float voltage = 0.0;
    for (int j = 0; j < 10; j++) {
      voltage += (analogRead(TURBIDITY_PIN) * 3.3) / 4095.0;
      delay(100);
    }
    voltage /= 10.0;
    
    calibration[i].voltage = voltage;
    Serial.printf("Voltage: %.3fV\\n", voltage);
  }
  
  saveCalibration();
  Serial.println("Calibration completed and saved!");
}

void printStatus() {
  Serial.println("\\n=== Turbidity Sensor Status ===");
  Serial.printf("Last Turbidity: %.2f NTU\\n", lastTurbidity);
  Serial.printf("Last Temperature: %.2f°C\\n", lastTemperature);
  Serial.printf("Sensor Health: %s\\n", sensorHealthy ? "OK" : "ERROR");
  Serial.printf("Consecutive Errors: %d\\n", consecutiveErrors);
  Serial.printf("Dummy Mode: %s\\n", dummyMode ? "ON" : "OFF");
  Serial.printf("Last Cleaning: %lu ms ago\\n", millis() - lastCleaning);
  Serial.println("\\nCalibration Points:");
  for (int i = 0; i < CALIBRATION_POINTS; i++) {
    Serial.printf("  %.1f NTU -> %.3fV\\n", calibration[i].ntu, calibration[i].voltage);
  }
  Serial.println("==============================\\n");
}

void handleSensorError() {
  consecutiveErrors++;
  sensorHealthy = (consecutiveErrors < 3);
  
  Serial.printf("Sensor error #%d\\n", consecutiveErrors);
  
  if (consecutiveErrors >= 5) {
    // Reset sensor
    digitalWrite(POWER_ENABLE_PIN, LOW);
    delay(1000);
    digitalWrite(POWER_ENABLE_PIN, HIGH);
    delay(2000);
    consecutiveErrors = 0;
  }
}

void warmUpSensor() {
  Serial.println("Warming up sensor...");
  for (int i = 0; i < 10; i++) {
    analogRead(TURBIDITY_PIN);
    delay(500);
  }
  Serial.println("Sensor ready");
}

void updateStatusLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  
  if (sensorHealthy) {
    // Solid on when healthy
    digitalWrite(STATUS_LED_PIN, HIGH);
  } else {
    // Blink when error
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
    
    if (client.connect("TurbiditySensor")) {
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
    performCalibration();
  } else if (message == "dummy_on") {
    dummyMode = true;
  } else if (message == "dummy_off") {
    dummyMode = false;
  }
}

void sendMQTTData(const StaticJsonDocument<200>& doc) {
  String message;
  serializeJson(doc, message);
  client.publish(mqtt_topic, message.c_str());
}

void sendAPIData(const StaticJsonDocument<200>& doc) {
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
