/*
 * Aquatic Monitoring System - pH Sensor Module
 * 
 * This module measures water pH with temperature compensation
 * and transmits data via MQTT with power management features.
 * 
 * Features:
 * - Temperature compensated pH measurement
 * - Automatic calibration routines
 * - Data validation and quality control
 * - MQTT data transmission
 * - Deep sleep power management
 * - OTA firmware updates
 * 
 * Hardware:
 * - ESP32 microcontroller
 * - Atlas Scientific pH EZO circuit
 * - DS18B20 temperature sensor
 * - pH electrode with BNC connector
 * 
 * Author: Aquatic Monitoring Team
 * Version: 1.0.0
 * Date: 2025-07-18
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <esp_sleep.h>
#include <ArduinoOTA.h>

// Pin definitions
#define PH_RX_PIN 16              // ESP32 RX pin for pH sensor
#define PH_TX_PIN 17              // ESP32 TX pin for pH sensor
#define TEMP_SENSOR_PIN 4         // DS18B20 temperature sensor pin
#define CALIBRATION_BUTTON_PIN 0  // Button for calibration mode
#define STATUS_LED_PIN 2          // Status LED
#define POWER_ENABLE_PIN 25       // Power control for sensors

// Measurement parameters
#define MEASUREMENT_INTERVAL 30000  // 30 seconds between measurements
#define CALIBRATION_TIMEOUT 300000  // 5 minutes calibration timeout
#define STABILIZATION_TIME 10000    // 10 seconds for reading stabilization
#define MAX_READINGS_BUFFER 10      // Buffer for measurement averaging

// pH sensor specifications
#define PH_MIN_RANGE 0.001          // Minimum pH reading
#define PH_MAX_RANGE 14.000         // Maximum pH reading
#define PH_RESOLUTION 0.001         // pH resolution
#define PH_ACCURACY 0.02            // pH accuracy
#define MIN_ELECTRODE_SLOPE 50.0    // Minimum acceptable slope (mV/pH)

// Temperature compensation
#define TEMP_COMPENSATION_COEFF -0.0003  // pH/°C
#define REFERENCE_TEMP 25.0             // Reference temperature for compensation

// Power management
#define SLEEP_DURATION 300000000ULL  // 5 minutes in microseconds
#define LOW_POWER_THRESHOLD 3.4      // Battery voltage threshold for sleep

// MQTT configuration
#define MQTT_BROKER "your-mqtt-broker.com"
#define MQTT_PORT 1883
#define MQTT_USERNAME "aquatic_user"
#define MQTT_PASSWORD "secure_password"

// API configuration
#define API_SERVER "your-api-server.com"
#define API_PORT 3000
#define API_ENDPOINT "/api/ph-readings"
#define POOL_ID 1  // Configure this for each installation
#define DEVICE_ID "ph_sensor_01"
#define LOCATION_ID "pond_01"

// WiFi credentials
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// EEPROM addresses for calibration data
#define EEPROM_SIZE 512
#define CALIB_VALID_ADDR 0
#define CALIB_MID_ADDR 4
#define CALIB_LOW_ADDR 8
#define CALIB_HIGH_ADDR 12
#define CALIB_SLOPE_ADDR 16
#define CALIB_DATE_ADDR 20

// Global variables
SoftwareSerial phSerial(PH_RX_PIN, PH_TX_PIN);
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Sensor data structure
struct SensorData {
  float pH;
  float temperature;
  float rawVoltage;
  float slope;
  unsigned long timestamp;
  String calibrationStatus;
  bool dataValid;
};

// Calibration data structure
struct CalibrationData {
  bool isValid;
  float midPoint;
  float lowPoint;
  float highPoint;
  float slope;
  unsigned long calibrationDate;
};

// Global variables for sensor operation
SensorData currentReading;
CalibrationData calibration;
float phBuffer[MAX_READINGS_BUFFER];
int bufferIndex = 0;
bool calibrationMode = false;
unsigned long lastMeasurement = 0;
unsigned long bootCount = 0;

// RTC memory for persistent data
RTC_DATA_ATTR struct {
  uint32_t bootCount;
  float lastpH;
  float lastTemp;
  bool calibrationValid;
} rtcData;

void setup() {
  Serial.begin(115200);
  
  // Initialize hardware
  initializeHardware();
  
  // Load calibration data from EEPROM
  loadCalibrationData();
  
  // Initialize sensors
  initializeSensors();
  
  // Check if calibration button is pressed
  if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    enterCalibrationMode();
  }
  
  // Initialize WiFi and MQTT
  initializeWiFi();
  initializeMQTT();
  
  // Setup OTA updates
  setupOTA();
  
  // Increment boot count
  rtcData.bootCount++;
  
  // Publish device status
  publishDeviceStatus();
  
  Serial.println("pH Sensor Module Initialized");
  Serial.printf("Boot count: %d\n", rtcData.bootCount);
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
  
  // Handle MQTT communication
  if (mqttClient.connected()) {
    mqttClient.loop();
  } else {
    reconnectMQTT();
  }
  
  // Take pH measurement
  if (millis() - lastMeasurement >= MEASUREMENT_INTERVAL) {
    takePHMeasurement();
    lastMeasurement = millis();
  }
  
  // Check for calibration mode
  if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
      enterCalibrationMode();
    }
  }
  
  // Enter deep sleep if low power
  if (checkLowPowerCondition()) {
    enterDeepSleep();
  }
  
  delay(100);
}

void initializeHardware() {
  // Initialize pins
  pinMode(CALIBRATION_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(POWER_ENABLE_PIN, OUTPUT);
  
  // Power on sensors
  digitalWrite(POWER_ENABLE_PIN, HIGH);
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Initialize serial communication with pH sensor
  phSerial.begin(9600);
  
  // Initialize temperature sensor
  tempSensor.begin();
  
  // Status LED indication
  digitalWrite(STATUS_LED_PIN, HIGH);
  
  Serial.println("Hardware initialized");
}

void initializeSensors() {
  // Send initialization commands to pH sensor
  phSerial.print("C,0\r");  // Turn off continuous mode
  delay(300);
  
  // Set response mode
  phSerial.print("RESPONSE,1\r");
  delay(300);
  
  // Check sensor status
  phSerial.print("Status\r");
  delay(300);
  
  String response = readPhSensorResponse();
  Serial.println("pH Sensor Status: " + response);
  
  // Set temperature compensation
  float currentTemp = readTemperature();
  setTemperatureCompensation(currentTemp);
  
  // Initialize buffer
  for (int i = 0; i < MAX_READINGS_BUFFER; i++) {
    phBuffer[i] = 7.0; // Initialize with neutral pH
  }
  
  Serial.println("Sensors initialized");
}

void initializeWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.println("IP address: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection failed");
  }
}

void initializeMQTT() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  
  reconnectMQTT();
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = String(DEVICE_ID) + "_" + String(WiFi.macAddress());
    
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
      
      // Subscribe to command topic
      String commandTopic = "aquaticmonitoring/" + String(LOCATION_ID) + "/ph/commands";
      mqttClient.subscribe(commandTopic.c_str());
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
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
  
  Serial.println("Message received: " + message);
  
  // Parse JSON command
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (!error) {
    String command = doc["command"];
    
    if (command == "calibrate") {
      enterCalibrationMode();
    } else if (command == "measure") {
      takePHMeasurement();
    } else if (command == "status") {
      publishDeviceStatus();
    } else if (command == "sleep") {
      enterDeepSleep();
    }
  }
}

void takePHMeasurement() {
  Serial.println("Taking pH measurement...");
  
  // Read current temperature
  float currentTemp = readTemperature();
  
  // Set temperature compensation
  setTemperatureCompensation(currentTemp);
  
  // Take pH reading
  phSerial.print("R\r");
  delay(1000); // Wait for measurement
  
  String response = readPhSensorResponse();
  
  // Parse pH value
  float phValue = response.toFloat();
  
  // Validate reading
  if (phValue >= PH_MIN_RANGE && phValue <= PH_MAX_RANGE) {
    // Add to buffer for averaging
    phBuffer[bufferIndex] = phValue;
    bufferIndex = (bufferIndex + 1) % MAX_READINGS_BUFFER;
    
    // Calculate average
    float avgPH = 0;
    for (int i = 0; i < MAX_READINGS_BUFFER; i++) {
      avgPH += phBuffer[i];
    }
    avgPH /= MAX_READINGS_BUFFER;
    
    // Update sensor data
    currentReading.pH = avgPH;
    currentReading.temperature = currentTemp;
    currentReading.timestamp = millis();
    currentReading.dataValid = true;
    currentReading.calibrationStatus = getCalibrationStatus();
    
    // Store in RTC memory
    rtcData.lastpH = avgPH;
    rtcData.lastTemp = currentTemp;
    
    // Publish data
    publishSensorData();
    
    Serial.printf("pH: %.3f, Temperature: %.2f°C\n", avgPH, currentTemp);
  } else {
    Serial.println("Invalid pH reading: " + String(phValue));
    currentReading.dataValid = false;
  }
}

float readTemperature() {
  tempSensor.requestTemperatures();
  float temp = tempSensor.getTempCByIndex(0);
  
  // Validate temperature reading
  if (temp > -50.0 && temp < 125.0) {
    return temp;
  } else {
    Serial.println("Invalid temperature reading");
    return REFERENCE_TEMP; // Return reference temperature if invalid
  }
}

void setTemperatureCompensation(float temperature) {
  String command = "T," + String(temperature, 2) + "\r";
  phSerial.print(command);
  delay(300);
}

String readPhSensorResponse() {
  String response = "";
  unsigned long startTime = millis();
  
  while (millis() - startTime < 2000) { // 2 second timeout
    if (phSerial.available()) {
      char c = phSerial.read();
      if (c == '\r') {
        break;
      }
      response += c;
    }
  }
  
  return response;
}

void enterCalibrationMode() {
  Serial.println("Entering calibration mode...");
  calibrationMode = true;
  
  // Flash LED to indicate calibration mode
  for (int i = 0; i < 5; i++) {
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(200);
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(200);
  }
  
  // Calibration sequence
  performCalibration();
  
  calibrationMode = false;
  Serial.println("Calibration mode completed");
}

void performCalibration() {
  Serial.println("Starting pH calibration sequence...");
  
  // Clear previous calibration
  phSerial.print("Cal,clear\r");
  delay(1000);
  
  // Mid-point calibration (pH 7.00)
  Serial.println("Place electrode in pH 7.00 buffer and press button...");
  waitForButtonPress();
  
  phSerial.print("Cal,mid,7.00\r");
  delay(2000);
  
  String response = readPhSensorResponse();
  Serial.println("Mid calibration: " + response);
  
  // Low-point calibration (pH 4.01)
  Serial.println("Place electrode in pH 4.01 buffer and press button...");
  waitForButtonPress();
  
  phSerial.print("Cal,low,4.01\r");
  delay(2000);
  
  response = readPhSensorResponse();
  Serial.println("Low calibration: " + response);
  
  // High-point calibration (pH 10.01)
  Serial.println("Place electrode in pH 10.01 buffer and press button...");
  waitForButtonPress();
  
  phSerial.print("Cal,high,10.01\r");
  delay(2000);
  
  response = readPhSensorResponse();
  Serial.println("High calibration: " + response);
  
  // Query calibration status
  phSerial.print("Cal,?\r");
  delay(1000);
  
  String calibStatus = readPhSensorResponse();
  Serial.println("Calibration status: " + calibStatus);
  
  // Query slope
  phSerial.print("Slope,?\r");
  delay(1000);
  
  String slopeResponse = readPhSensorResponse();
  float slope = slopeResponse.toFloat();
  
  // Validate calibration
  if (slope >= MIN_ELECTRODE_SLOPE) {
    // Save calibration data
    calibration.isValid = true;
    calibration.midPoint = 7.00;
    calibration.lowPoint = 4.01;
    calibration.highPoint = 10.01;
    calibration.slope = slope;
    calibration.calibrationDate = millis();
    
    saveCalibrationData();
    
    Serial.println("Calibration successful!");
    Serial.printf("Electrode slope: %.2f mV/pH\n", slope);
  } else {
    Serial.println("Calibration failed - poor electrode slope");
    calibration.isValid = false;
  }
}

void waitForButtonPress() {
  Serial.println("Press calibration button to continue...");
  
  while (digitalRead(CALIBRATION_BUTTON_PIN) == HIGH) {
    delay(100);
  }
  
  // Wait for button release
  while (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    delay(100);
  }
  
  delay(500); // Debounce
}

void saveCalibrationData() {
  EEPROM.put(CALIB_VALID_ADDR, calibration.isValid);
  EEPROM.put(CALIB_MID_ADDR, calibration.midPoint);
  EEPROM.put(CALIB_LOW_ADDR, calibration.lowPoint);
  EEPROM.put(CALIB_HIGH_ADDR, calibration.highPoint);
  EEPROM.put(CALIB_SLOPE_ADDR, calibration.slope);
  EEPROM.put(CALIB_DATE_ADDR, calibration.calibrationDate);
  EEPROM.commit();
  
  Serial.println("Calibration data saved to EEPROM");
}

void loadCalibrationData() {
  EEPROM.get(CALIB_VALID_ADDR, calibration.isValid);
  EEPROM.get(CALIB_MID_ADDR, calibration.midPoint);
  EEPROM.get(CALIB_LOW_ADDR, calibration.lowPoint);
  EEPROM.get(CALIB_HIGH_ADDR, calibration.highPoint);
  EEPROM.get(CALIB_SLOPE_ADDR, calibration.slope);
  EEPROM.get(CALIB_DATE_ADDR, calibration.calibrationDate);
  
  if (calibration.isValid) {
    Serial.println("Calibration data loaded from EEPROM");
    Serial.printf("Last calibration: %lu\n", calibration.calibrationDate);
  } else {
    Serial.println("No valid calibration data found");
  }
}

String getCalibrationStatus() {
  if (calibration.isValid) {
    unsigned long daysSinceCalibration = (millis() - calibration.calibrationDate) / (24 * 60 * 60 * 1000);
    
    if (daysSinceCalibration < 7) {
      return "Good";
    } else if (daysSinceCalibration < 14) {
      return "Warning";
    } else {
      return "Expired";
    }
  } else {
    return "Invalid";
  }
}

void publishSensorData() {
  if (!mqttClient.connected()) {
    return;
  }
  
  // Publish to MQTT
  StaticJsonDocument<512> doc;
  doc["device_id"] = DEVICE_ID;
  doc["location"] = LOCATION_ID;
  doc["timestamp"] = currentReading.timestamp;
  doc["pH"] = currentReading.pH;
  doc["temperature"] = currentReading.temperature;
  doc["calibration_status"] = currentReading.calibrationStatus;
  doc["data_valid"] = currentReading.dataValid;
  doc["boot_count"] = rtcData.bootCount;
  doc["slope"] = calibration.slope;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  String topic = "aquaticmonitoring/" + String(LOCATION_ID) + "/ph/data";
  mqttClient.publish(topic.c_str(), jsonString.c_str());
  
  Serial.println("Data published to MQTT: " + jsonString);
  
  // Send to API endpoint
  sendToAPI();
}

void sendToAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - skipping API call");
    return;
  }
  
  HTTPClient http;
  WiFiClient client;
  
  // Configure API endpoint
  String apiUrl = "http://" + String(API_SERVER) + ":" + String(API_PORT) + String(API_ENDPOINT);
  http.begin(client, apiUrl);
  http.addHeader("Content-Type", "application/json");
  
  // Prepare API payload according to the API documentation
  StaticJsonDocument<256> apiDoc;
  apiDoc["pool_id"] = POOL_ID;
  apiDoc["ph_value"] = currentReading.pH;
  apiDoc["notes"] = "Automated reading from pH sensor " + String(DEVICE_ID);
  
  String apiPayload;
  serializeJson(apiDoc, apiPayload);
  
  // Send POST request
  int httpResponseCode = http.POST(apiPayload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("API Response Code: " + String(httpResponseCode));
    Serial.println("API Response: " + response);
  } else {
    Serial.println("API Error: " + String(httpResponseCode));
  }
  
  http.end();
}

void publishDeviceStatus() {
  if (!mqttClient.connected()) {
    return;
  }
  
  StaticJsonDocument<512> doc;
  doc["device_id"] = DEVICE_ID;
  doc["location"] = LOCATION_ID;
  doc["timestamp"] = millis();
  doc["uptime"] = millis() / 1000;
  doc["firmware_version"] = "1.0.0";
  doc["calibration_valid"] = calibration.isValid;
  doc["calibration_date"] = calibration.calibrationDate;
  doc["electrode_slope"] = calibration.slope;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["sensor_health"] = checkSensorHealth();
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  String topic = "aquaticmonitoring/" + String(LOCATION_ID) + "/ph/status";
  mqttClient.publish(topic.c_str(), jsonString.c_str(), true);
  
  Serial.println("Status published: " + jsonString);
}

void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_ID);
  ArduinoOTA.setPassword("ph_sensor_ota_2025");
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

bool checkLowPowerCondition() {
  // Check battery voltage (assuming voltage divider on A0)
  float batteryVoltage = analogRead(A0) * 3.3 / 4095.0 * 2.0;
  
  if (batteryVoltage < LOW_POWER_THRESHOLD) {
    Serial.println("Low power condition detected");
    return true;
  }
  
  return false;
}

void enterDeepSleep() {
  Serial.println("Entering deep sleep mode...");
  
  // Publish sleep status
  if (mqttClient.connected()) {
    StaticJsonDocument<128> doc;
    doc["device_id"] = DEVICE_ID;
    doc["status"] = "sleeping";
    doc["wake_time"] = SLEEP_DURATION / 1000000;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    String topic = "aquaticmonitoring/" + String(LOCATION_ID) + "/ph/status";
    mqttClient.publish(topic.c_str(), jsonString.c_str());
    
    delay(100); // Allow message to be sent
  }
  
  // Disconnect WiFi
  WiFi.disconnect(true);
  
  // Power down sensors
  digitalWrite(POWER_ENABLE_PIN, LOW);
  
  // Configure wake up
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
  
  // Enter deep sleep
  esp_deep_sleep_start();
}

// Utility function to check sensor health
bool checkSensorHealth() {
  // Check calibration validity
  if (!calibration.isValid) {
    Serial.println("Sensor health: CRITICAL - No calibration");
    return false;
  }
  
  // Check electrode slope
  if (calibration.slope < MIN_ELECTRODE_SLOPE) {
    Serial.println("Sensor health: CRITICAL - Poor electrode slope");
    return false;
  }
  
  // Check calibration age
  unsigned long daysSinceCalibration = (millis() - calibration.calibrationDate) / (24 * 60 * 60 * 1000);
  if (daysSinceCalibration > 14) {
    Serial.println("Sensor health: WARNING - Calibration expired");
    return false;
  }
  
  // Check temperature sensor
  float temp = readTemperature();
  if (temp == REFERENCE_TEMP) {
    Serial.println("Sensor health: WARNING - Temperature sensor issue");
    return false;
  }
  
  Serial.println("Sensor health: GOOD");
  return true;
}

// Function to generate dummy pH data for testing
void generateDummyPHData() {
  // Simulate pH fluctuation based on time of day
  unsigned long currentTime = millis();
  float baseValue = 7.2; // Slightly alkaline
  
  // Simulate daily pH cycle (photosynthesis effect)
  float dailyCycle = sin(currentTime / 3600000.0 * 2 * PI / 24) * 0.5;
  
  // Add some random noise
  float noise = (random(-20, 20) / 100.0);
  
  currentReading.pH = baseValue + dailyCycle + noise;
  currentReading.temperature = 22.0 + (random(-50, 50) / 100.0);
  currentReading.timestamp = currentTime;
  currentReading.dataValid = true;
  currentReading.calibrationStatus = "Good";
  
  publishSensorData();
  
  Serial.println("Published dummy pH data for testing");
}
