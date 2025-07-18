/*
 * Aquatic Monitoring System - Ammonia Sensor Module
 * 
 * This module measures dissolved ammonia (NH₃) and ammonium (NH₄⁺) concentrations
 * using ion-selective electrodes with temperature and pH compensation.
 * 
 * Features:
 * - Ion-selective electrode measurement
 * - NH₃/NH₄⁺ equilibrium calculations
 * - Temperature and pH compensation
 * - Multi-point calibration
 * - Data validation and quality control
 * - MQTT data transmission
 * - Deep sleep power management
 * - Toxic threshold monitoring
 * 
 * Hardware:
 * - ESP32 microcontroller
 * - NH₄⁺ Ion Selective Electrode
 * - Reference electrode (Ag/AgCl)
 * - High impedance amplifier
 * - pH sensor for equilibrium calculations
 * - DS18B20 temperature sensor
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
#include <math.h>

// Pin definitions
#define AMMONIA_ADC_PIN 36        // ADC pin for ammonia ISE
#define TEMP_SENSOR_PIN 4         // DS18B20 temperature sensor
#define PH_RX_PIN 16              // pH sensor RX
#define PH_TX_PIN 17              // pH sensor TX
#define REFERENCE_PIN 39          // Reference electrode ADC
#define CALIBRATION_BUTTON_PIN 0  // Calibration button
#define STATUS_LED_PIN 2          // Status LED
#define POWER_ENABLE_PIN 25       // Power control

// Measurement parameters
#define MEASUREMENT_INTERVAL 60000    // 1 minute between measurements
#define STABILIZATION_TIME 30000      // 30 seconds for ISE stabilization
#define CALIBRATION_POINTS 3          // Number of calibration points
#define MAX_READINGS_BUFFER 10        // Buffer for averaging

// Ammonia sensor specifications
#define AMMONIA_MIN_RANGE 0.01        // Minimum ammonia reading (mg/L)
#define AMMONIA_MAX_RANGE 100.0       // Maximum ammonia reading (mg/L)
#define THEORETICAL_SLOPE 59.16       // mV/decade at 25°C
#define MIN_ACCEPTABLE_SLOPE 50.0     // Minimum acceptable slope
#define MAX_ACCEPTABLE_SLOPE 65.0     // Maximum acceptable slope

// Calibration standards (mg/L NH₄⁺-N)
#define CALIB_STANDARD_1 1.0
#define CALIB_STANDARD_2 10.0
#define CALIB_STANDARD_3 100.0

// NH₃/NH₄⁺ equilibrium constants
#define PKA_25C 9.25                  // pKa at 25°C
#define TEMP_COEFF 0.0324             // Temperature coefficient for pKa
#define REFERENCE_TEMP 25.0           // Reference temperature

// Toxic thresholds (mg/L NH₃-N)
#define TOXIC_WARNING_THRESHOLD 0.1
#define TOXIC_CRITICAL_THRESHOLD 0.5
#define TOXIC_ACUTE_THRESHOLD 2.0

// Power management
#define SLEEP_DURATION 600000000ULL   // 10 minutes in microseconds
#define LOW_POWER_THRESHOLD 3.4       // Battery voltage threshold

// MQTT configuration
#define MQTT_BROKER "your-mqtt-broker.com"
#define MQTT_PORT 1883
#define MQTT_USERNAME "aquatic_user"
#define MQTT_PASSWORD "secure_password"
#define DEVICE_ID "ammonia_sensor_01"
#define LOCATION_ID "pond_01"

// WiFi credentials
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// API configuration
#define API_SERVER "your-api-server.com"
#define API_PORT 3000
#define API_ENDPOINT "/api/ammonia-readings"
#define POOL_ID 1  // Configure this for each installation

// EEPROM addresses
#define EEPROM_SIZE 512
#define CALIB_VALID_ADDR 0
#define CALIB_SLOPE_ADDR 4
#define CALIB_INTERCEPT_ADDR 8
#define CALIB_DATE_ADDR 12
#define CALIB_POINTS_ADDR 16

// Global variables
SoftwareSerial phSerial(PH_RX_PIN, PH_TX_PIN);
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Sensor data structure
struct AmmoniaData {
  float totalAmmonia;      // Total NH₃ + NH₄⁺
  float freeAmmonia;       // NH₃ (toxic form)
  float ammonium;          // NH₄⁺ (less toxic)
  float pH;                // pH for equilibrium calculation
  float temperature;       // Temperature for compensation
  float rawVoltage;        // Raw ISE voltage
  float slope;             // Electrode slope
  unsigned long timestamp; // Measurement timestamp
  String toxicityLevel;    // Toxicity assessment
  bool dataValid;          // Data quality flag
};

// Calibration data structure
struct CalibrationData {
  bool isValid;
  float slope;
  float intercept;
  float standards[CALIBRATION_POINTS];
  float voltages[CALIBRATION_POINTS];
  unsigned long calibrationDate;
  float correlationCoeff;
};

// Global variables
AmmoniaData currentReading;
CalibrationData calibration;
float ammoniaBuffer[MAX_READINGS_BUFFER];
int bufferIndex = 0;
bool calibrationMode = false;
unsigned long lastMeasurement = 0;

// RTC memory for persistent data
RTC_DATA_ATTR struct {
  uint32_t bootCount;
  float lastTotalAmmonia;
  float lastFreeAmmonia;
  float lastpH;
  float lastTemp;
  bool toxicAlert;
} rtcData;

void setup() {
  Serial.begin(115200);
  
  // Initialize hardware
  initializeHardware();
  
  // Load calibration data
  loadCalibrationData();
  
  // Initialize sensors
  initializeSensors();
  
  // Check calibration mode
  if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    enterCalibrationMode();
  }
  
  // Initialize communication
  initializeWiFi();
  initializeMQTT();
  
  // Setup OTA
  setupOTA();
  
  // Increment boot count
  rtcData.bootCount++;
  
  // Publish device status
  publishDeviceStatus();
  
  Serial.println("Ammonia Sensor Module Initialized");
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
  
  // Handle MQTT
  if (mqttClient.connected()) {
    mqttClient.loop();
  } else {
    reconnectMQTT();
  }
  
  // Take measurement
  if (millis() - lastMeasurement >= MEASUREMENT_INTERVAL) {
    takeAmmoniaMeasurement();
    lastMeasurement = millis();
  }
  
  // Check calibration button
  if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
      enterCalibrationMode();
    }
  }
  
  // Check power management
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
  digitalWrite(STATUS_LED_PIN, HIGH);
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Initialize pH sensor communication
  phSerial.begin(9600);
  
  // Initialize temperature sensor
  tempSensor.begin();
  
  // Configure ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  Serial.println("Hardware initialized");
}

void initializeSensors() {
  // Initialize pH sensor
  phSerial.print("C,0\r");  // Turn off continuous mode
  delay(300);
  phSerial.print("RESPONSE,1\r");
  delay(300);
  
  // Initialize buffer
  for (int i = 0; i < MAX_READINGS_BUFFER; i++) {
    ammoniaBuffer[i] = 0.0;
  }
  
  // Set initial temperature compensation
  float currentTemp = readTemperature();
  updateTemperatureCompensation(currentTemp);
  
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
    Serial.println("IP: " + WiFi.localIP().toString());
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
      
      // Subscribe to commands
      String commandTopic = "aquaticmonitoring/" + String(LOCATION_ID) + "/ammonia/commands";
      mqttClient.subscribe(commandTopic.c_str());
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  // Parse command
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (!error) {
    String command = doc["command"];
    
    if (command == "calibrate") {
      enterCalibrationMode();
    } else if (command == "measure") {
      takeAmmoniaMeasurement();
    } else if (command == "status") {
      publishDeviceStatus();
    } else if (command == "sleep") {
      enterDeepSleep();
    }
  }
}

void takeAmmoniaMeasurement() {
  Serial.println("Taking ammonia measurement...");
  
  // Read supporting parameters
  float temperature = readTemperature();
  float pH = readPH();
  
  // Read ISE voltage
  float iseVoltage = readISEVoltage();
  
  // Calculate total ammonia concentration
  float totalAmmonia = calculateAmmoniaConcentration(iseVoltage, temperature);
  
  // Calculate NH₃/NH₄⁺ equilibrium
  float freeAmmoniaFraction = calculateFreeAmmoniaFraction(pH, temperature);
  float freeAmmonia = totalAmmonia * freeAmmoniaFraction;
  float ammonium = totalAmmonia * (1.0 - freeAmmoniaFraction);
  
  // Validate measurement
  if (totalAmmonia >= AMMONIA_MIN_RANGE && totalAmmonia <= AMMONIA_MAX_RANGE) {
    // Add to buffer
    ammoniaBuffer[bufferIndex] = totalAmmonia;
    bufferIndex = (bufferIndex + 1) % MAX_READINGS_BUFFER;
    
    // Calculate average
    float avgAmmonia = 0;
    for (int i = 0; i < MAX_READINGS_BUFFER; i++) {
      avgAmmonia += ammoniaBuffer[i];
    }
    avgAmmonia /= MAX_READINGS_BUFFER;
    
    // Update sensor data
    currentReading.totalAmmonia = avgAmmonia;
    currentReading.freeAmmonia = freeAmmonia;
    currentReading.ammonium = ammonium;
    currentReading.pH = pH;
    currentReading.temperature = temperature;
    currentReading.rawVoltage = iseVoltage;
    currentReading.slope = calibration.slope;
    currentReading.timestamp = millis();
    currentReading.toxicityLevel = assessToxicity(freeAmmonia);
    currentReading.dataValid = true;
    
    // Store in RTC memory
    rtcData.lastTotalAmmonia = totalAmmonia;
    rtcData.lastFreeAmmonia = freeAmmonia;
    rtcData.lastpH = pH;
    rtcData.lastTemp = temperature;
    
    // Check for toxic levels
    if (freeAmmonia >= TOXIC_WARNING_THRESHOLD) {
      rtcData.toxicAlert = true;
      publishToxicAlert();
    } else {
      rtcData.toxicAlert = false;
    }
    
    // Publish data
    publishSensorData();
    
    Serial.printf("Total NH₃: %.3f mg/L, Free NH₃: %.3f mg/L, NH₄⁺: %.3f mg/L\n", 
                  totalAmmonia, freeAmmonia, ammonium);
    Serial.printf("pH: %.2f, Temperature: %.2f°C, Toxicity: %s\n", 
                  pH, temperature, currentReading.toxicityLevel.c_str());
  } else {
    Serial.println("Invalid ammonia reading: " + String(totalAmmonia));
    currentReading.dataValid = false;
  }
}

float readTemperature() {
  tempSensor.requestTemperatures();
  float temp = tempSensor.getTempCByIndex(0);
  
  if (temp > -50.0 && temp < 125.0) {
    return temp;
  } else {
    Serial.println("Invalid temperature reading");
    return REFERENCE_TEMP;
  }
}

float readPH() {
  phSerial.print("R\r");
  delay(1000);
  
  String response = "";
  unsigned long startTime = millis();
  
  while (millis() - startTime < 2000) {
    if (phSerial.available()) {
      char c = phSerial.read();
      if (c == '\r') break;
      response += c;
    }
  }
  
  float pH = response.toFloat();
  
  // Validate pH reading
  if (pH >= 0.0 && pH <= 14.0) {
    return pH;
  } else {
    Serial.println("Invalid pH reading");
    return 7.0; // Return neutral pH if invalid
  }
}

float readISEVoltage() {
  // Take multiple readings for averaging
  float voltageSum = 0;
  int validReadings = 0;
  
  for (int i = 0; i < 10; i++) {
    int rawReading = analogRead(AMMONIA_ADC_PIN);
    float voltage = (rawReading * 3.3) / 4095.0;
    
    // Basic range check
    if (voltage >= 0.0 && voltage <= 3.3) {
      voltageSum += voltage;
      validReadings++;
    }
    
    delay(100);
  }
  
  if (validReadings > 0) {
    return voltageSum / validReadings;
  } else {
    Serial.println("No valid ISE readings");
    return 0.0;
  }
}

float calculateAmmoniaConcentration(float voltage, float temperature) {
  if (!calibration.isValid) {
    Serial.println("No valid calibration data");
    return 0.0;
  }
  
  // Temperature compensation for slope
  float tempCompensatedSlope = calibration.slope * (temperature + 273.15) / (REFERENCE_TEMP + 273.15);
  
  // Apply Nernst equation (with temperature compensation)
  // C = 10^((V - intercept) / slope)
  float logConcentration = (voltage - calibration.intercept) / tempCompensatedSlope;
  float concentration = pow(10, logConcentration);
  
  return concentration;
}

float calculateFreeAmmoniaFraction(float pH, float temperature) {
  // Calculate temperature-dependent pKa
  float pKa = PKA_25C + (TEMP_COEFF * (temperature - REFERENCE_TEMP));
  
  // Calculate ionization fraction using Henderson-Hasselbalch equation
  // fNH₃ = 1 / (1 + 10^(pKa - pH))
  float powerTerm = pKa - pH;
  float fraction = 1.0 / (1.0 + pow(10, powerTerm));
  
  return fraction;
}

String assessToxicity(float freeAmmonia) {
  if (freeAmmonia >= TOXIC_ACUTE_THRESHOLD) {
    return "ACUTE";
  } else if (freeAmmonia >= TOXIC_CRITICAL_THRESHOLD) {
    return "CRITICAL";
  } else if (freeAmmonia >= TOXIC_WARNING_THRESHOLD) {
    return "WARNING";
  } else {
    return "SAFE";
  }
}

void enterCalibrationMode() {
  Serial.println("Entering calibration mode...");
  calibrationMode = true;
  
  // Flash LED
  for (int i = 0; i < 5; i++) {
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(200);
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(200);
  }
  
  performCalibration();
  
  calibrationMode = false;
  Serial.println("Calibration completed");
}

void performCalibration() {
  float standards[CALIBRATION_POINTS] = {CALIB_STANDARD_1, CALIB_STANDARD_2, CALIB_STANDARD_3};
  float voltages[CALIBRATION_POINTS];
  
  Serial.println("Starting ammonia calibration...");
  
  for (int i = 0; i < CALIBRATION_POINTS; i++) {
    Serial.printf("Place electrode in %.1f mg/L NH₄⁺ standard and press button...\n", standards[i]);
    waitForButtonPress();
    
    // Allow stabilization
    delay(STABILIZATION_TIME);
    
    // Read voltage
    voltages[i] = readISEVoltage();
    
    Serial.printf("Standard: %.1f mg/L, Voltage: %.3f V\n", standards[i], voltages[i]);
  }
  
  // Calculate calibration curve
  calculateCalibrationCurve(standards, voltages);
  
  // Validate calibration
  if (abs(calibration.slope) >= MIN_ACCEPTABLE_SLOPE && 
      abs(calibration.slope) <= MAX_ACCEPTABLE_SLOPE) {
    
    calibration.isValid = true;
    calibration.calibrationDate = millis();
    
    // Store calibration points
    for (int i = 0; i < CALIBRATION_POINTS; i++) {
      calibration.standards[i] = standards[i];
      calibration.voltages[i] = voltages[i];
    }
    
    saveCalibrationData();
    
    Serial.println("Calibration successful!");
    Serial.printf("Slope: %.2f mV/decade\n", calibration.slope * 1000);
    Serial.printf("Intercept: %.3f V\n", calibration.intercept);
    Serial.printf("Correlation: %.4f\n", calibration.correlationCoeff);
  } else {
    Serial.println("Calibration failed - poor electrode slope");
    calibration.isValid = false;
  }
}

void calculateCalibrationCurve(float* standards, float* voltages) {
  // Convert to log scale for linear regression
  float logStandards[CALIBRATION_POINTS];
  for (int i = 0; i < CALIBRATION_POINTS; i++) {
    logStandards[i] = log10(standards[i]);
  }
  
  // Calculate linear regression
  float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
  int n = CALIBRATION_POINTS;
  
  for (int i = 0; i < n; i++) {
    sumX += logStandards[i];
    sumY += voltages[i];
    sumXY += logStandards[i] * voltages[i];
    sumX2 += logStandards[i] * logStandards[i];
  }
  
  // Calculate slope and intercept
  calibration.slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
  calibration.intercept = (sumY - calibration.slope * sumX) / n;
  
  // Calculate correlation coefficient
  float sumY2 = 0;
  for (int i = 0; i < n; i++) {
    sumY2 += voltages[i] * voltages[i];
  }
  
  float numerator = n * sumXY - sumX * sumY;
  float denominator = sqrt((n * sumX2 - sumX * sumX) * (n * sumY2 - sumY * sumY));
  
  if (denominator != 0) {
    calibration.correlationCoeff = numerator / denominator;
  } else {
    calibration.correlationCoeff = 0.0;
  }
}

void waitForButtonPress() {
  Serial.println("Press button to continue...");
  
  while (digitalRead(CALIBRATION_BUTTON_PIN) == HIGH) {
    delay(100);
  }
  
  while (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
    delay(100);
  }
  
  delay(500);
}

void saveCalibrationData() {
  EEPROM.put(CALIB_VALID_ADDR, calibration.isValid);
  EEPROM.put(CALIB_SLOPE_ADDR, calibration.slope);
  EEPROM.put(CALIB_INTERCEPT_ADDR, calibration.intercept);
  EEPROM.put(CALIB_DATE_ADDR, calibration.calibrationDate);
  EEPROM.commit();
  
  Serial.println("Calibration data saved");
}

void loadCalibrationData() {
  EEPROM.get(CALIB_VALID_ADDR, calibration.isValid);
  EEPROM.get(CALIB_SLOPE_ADDR, calibration.slope);
  EEPROM.get(CALIB_INTERCEPT_ADDR, calibration.intercept);
  EEPROM.get(CALIB_DATE_ADDR, calibration.calibrationDate);
  
  if (calibration.isValid) {
    Serial.println("Calibration data loaded");
  } else {
    Serial.println("No valid calibration data");
  }
}

void updateTemperatureCompensation(float temperature) {
  // Update pH sensor temperature
  String command = "T," + String(temperature, 2) + "\r";
  phSerial.print(command);
  delay(300);
}

void publishSensorData() {
  if (!mqttClient.connected()) return;
  
  // Publish to MQTT
  StaticJsonDocument<768> doc;
  doc["device_id"] = DEVICE_ID;
  doc["location"] = LOCATION_ID;
  doc["timestamp"] = currentReading.timestamp;
  doc["total_ammonia"] = currentReading.totalAmmonia;
  doc["free_ammonia"] = currentReading.freeAmmonia;
  doc["ammonium"] = currentReading.ammonium;
  doc["pH"] = currentReading.pH;
  doc["temperature"] = currentReading.temperature;
  doc["raw_voltage"] = currentReading.rawVoltage;
  doc["slope"] = currentReading.slope;
  doc["toxicity_level"] = currentReading.toxicityLevel;
  doc["data_valid"] = currentReading.dataValid;
  doc["boot_count"] = rtcData.bootCount;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  String topic = "aquaticmonitoring/" + String(LOCATION_ID) + "/ammonia/data";
  mqttClient.publish(topic.c_str(), jsonString.c_str());
  
  Serial.println("Data published to MQTT");
  
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
  apiDoc["ammonia_ppm"] = currentReading.totalAmmonia;
  apiDoc["notes"] = "Automated reading from ammonia sensor " + String(DEVICE_ID);
  
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

void publishToxicAlert() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["device_id"] = DEVICE_ID;
  doc["location"] = LOCATION_ID;
  doc["timestamp"] = millis();
  doc["alert_type"] = "TOXIC_AMMONIA";
  doc["free_ammonia"] = currentReading.freeAmmonia;
  doc["toxicity_level"] = currentReading.toxicityLevel;
  doc["threshold"] = TOXIC_WARNING_THRESHOLD;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  String topic = "aquaticmonitoring/" + String(LOCATION_ID) + "/ammonia/alerts";
  mqttClient.publish(topic.c_str(), jsonString.c_str());
  
  Serial.println("Toxic alert published");
}

void publishDeviceStatus() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<512> doc;
  doc["device_id"] = DEVICE_ID;
  doc["location"] = LOCATION_ID;
  doc["timestamp"] = millis();
  doc["uptime"] = millis() / 1000;
  doc["firmware_version"] = "1.0.0";
  doc["calibration_valid"] = calibration.isValid;
  doc["calibration_date"] = calibration.calibrationDate;
  doc["electrode_slope"] = calibration.slope;
  doc["correlation_coeff"] = calibration.correlationCoeff;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["sensor_health"] = checkSensorHealth();
  doc["toxic_alert"] = rtcData.toxicAlert;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  String topic = "aquaticmonitoring/" + String(LOCATION_ID) + "/ammonia/status";
  mqttClient.publish(topic.c_str(), jsonString.c_str(), true);
  
  Serial.println("Status published");
}

void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_ID);
  ArduinoOTA.setPassword("ammonia_ota_2025");
  
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA End");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
  });
  
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

bool checkLowPowerCondition() {
  float batteryVoltage = analogRead(A0) * 3.3 / 4095.0 * 2.0;
  return batteryVoltage < LOW_POWER_THRESHOLD;
}

void enterDeepSleep() {
  Serial.println("Entering deep sleep...");
  
  // Disconnect WiFi
  WiFi.disconnect(true);
  
  // Power down sensors
  digitalWrite(POWER_ENABLE_PIN, LOW);
  
  // Configure wake up
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
  
  // Enter deep sleep
  esp_deep_sleep_start();
}

bool checkSensorHealth() {
  if (!calibration.isValid) {
    Serial.println("Sensor health: CRITICAL - No calibration");
    return false;
  }
  
  if (abs(calibration.slope) < MIN_ACCEPTABLE_SLOPE) {
    Serial.println("Sensor health: CRITICAL - Poor slope");
    return false;
  }
  
  unsigned long daysSinceCalibration = (millis() - calibration.calibrationDate) / (24 * 60 * 60 * 1000);
  if (daysSinceCalibration > 7) {
    Serial.println("Sensor health: WARNING - Calibration expired");
    return false;
  }
  
  return true;
}

void generateDummyAmmoniaData() {
  // Generate realistic ammonia data
  unsigned long currentTime = millis();
  float baseAmmonia = 0.05; // Base level
  
  // Simulate diurnal variation
  float diurnalCycle = sin(currentTime / 3600000.0 * 2 * PI / 24) * 0.02;
  
  // Add random variation
  float noise = (random(-10, 10) / 1000.0);
  
  currentReading.totalAmmonia = baseAmmonia + diurnalCycle + noise;
  currentReading.pH = 7.8 + (random(-20, 20) / 100.0);
  currentReading.temperature = 22.0 + (random(-30, 30) / 100.0);
  
  // Calculate equilibrium
  float freeAmmoniaFraction = calculateFreeAmmoniaFraction(currentReading.pH, currentReading.temperature);
  currentReading.freeAmmonia = currentReading.totalAmmonia * freeAmmoniaFraction;
  currentReading.ammonium = currentReading.totalAmmonia * (1.0 - freeAmmoniaFraction);
  
  currentReading.timestamp = currentTime;
  currentReading.dataValid = true;
  currentReading.toxicityLevel = assessToxicity(currentReading.freeAmmonia);
  
  publishSensorData();
  
  Serial.println("Published dummy ammonia data");
}
