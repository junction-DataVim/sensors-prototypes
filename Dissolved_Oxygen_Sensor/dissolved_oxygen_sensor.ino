/*
 * Dissolved Oxygen Sensor Module
 * Aquatic Monitoring System
 * 
 * This module measures dissolved oxygen (DO) levels in water using
 * an optical or electrochemical sensor.
 * 
 * Features:
 * - Optical/electrochemical DO measurement
 * - Temperature compensation
 * - Salinity compensation
 * - Pressure compensation
 * - Calibration routines
 * - MQTT communication
 * - OTA updates
 * - Power management
 * - Dummy data generation
 * - Sensor health monitoring
 * - Fail-safe mechanisms
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <esp_sleep.h>
#include <Wire.h>

// Pin definitions
#define DO_SENSOR_PIN 34
#define TEMP_SENSOR_PIN 4
#define PRESSURE_SENSOR_SDA 21
#define PRESSURE_SENSOR_SCL 22
#define POWER_CONTROL_PIN 2
#define STATUS_LED_PIN 5
#define CALIBRATION_BUTTON_PIN 0

// Temperature sensor setup
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature temperatureSensor(&oneWire);

// WiFi and MQTT configuration
const char* ssid = "your_wifi_ssid";
const char* password = "your_wifi_password";
const char* mqtt_server = "your_mqtt_broker";
const int mqtt_port = 8883;
const char* mqtt_user = "your_mqtt_user";
const char* mqtt_password = "your_mqtt_password";

// API configuration
const char* api_server = "your-api-server.com";
const int api_port = 3000;
const char* api_endpoint = "/api/dissolved-oxygen-readings";
const int pool_id = 1;  // Configure this for each installation

WiFiClientSecure espClient;
PubSubClient client(espClient);

// Sensor configuration
struct SensorConfig {
    float calibration_slope = 1.0;
    float calibration_intercept = 0.0;
    float temp_coefficient = 0.024;    // %/°C
    float salinity_coefficient = 0.08; // %/ppt
    float pressure_coefficient = 0.0004; // %/mbar
    float reference_temp = 25.0;       // °C
    float reference_salinity = 0.0;    // ppt
    float reference_pressure = 1013.25; // mbar
    unsigned long measurement_interval = 30000; // ms
    unsigned long calibration_interval = 604800000; // 7 days
    bool dummy_mode = false;
    float alarm_threshold_low = 3.0;   // mg/L
    float alarm_threshold_high = 15.0; // mg/L
};

SensorConfig config;

// Measurement data
struct MeasurementData {
    float do_voltage;
    float do_concentration_mg_l;
    float do_saturation_percent;
    float temperature;
    float salinity;
    float pressure;
    unsigned long timestamp;
    bool valid;
    int quality_score;
};

// Calibration data
struct CalibrationData {
    float zero_do_voltage;      // 0% saturation
    float span_do_voltage;      // 100% saturation
    float span_temperature;     // Temperature during span cal
    bool calibrated = false;
    unsigned long last_calibration = 0;
};

CalibrationData calibration;

// System state
unsigned long lastMeasurement = 0;
unsigned long lastCalibration = 0;
bool sensorHealthy = true;
int consecutiveErrors = 0;
const int maxConsecutiveErrors = 5;

// Function declarations
void setupWiFi();
void setupMQTT();
void setupOTA();
void setupSensors();
void performMeasurement();
void calculateDO(MeasurementData& data);
void applyTemperatureCompensation(MeasurementData& data);
void applySalinityCompensation(MeasurementData& data);
void applyPressureCompensation(MeasurementData& data);
void publishData(const MeasurementData& data);
void handleCalibration();
void performCalibration();
void saveSensorConfig();
void loadSensorConfig();
void generateDummyData(MeasurementData& data);
void checkSensorHealth();
void handleAlarms(const MeasurementData& data);
void enterSleepMode();
float readDOVoltage();
float readPressure();
float calculateSaturation(float do_mg_l, float temp, float salinity);
float getOxygenSolubility(float temp, float salinity);
int calculateQualityScore(const MeasurementData& data);

void setup() {
    Serial.begin(115200);
    Serial.println("Dissolved Oxygen Sensor Module Starting...");
    
    // Initialize pins
    pinMode(POWER_CONTROL_PIN, OUTPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    pinMode(CALIBRATION_BUTTON_PIN, INPUT_PULLUP);
    
    // Power on sensors
    digitalWrite(POWER_CONTROL_PIN, HIGH);
    digitalWrite(STATUS_LED_PIN, HIGH);
    
    // Initialize EEPROM
    EEPROM.begin(512);
    
    // Load configuration
    loadSensorConfig();
    
    // Setup components
    setupSensors();
    setupWiFi();
    setupMQTT();
    setupOTA();
    
    Serial.println("Dissolved Oxygen Sensor Module Ready");
    digitalWrite(STATUS_LED_PIN, LOW);
}

void loop() {
    // Handle OTA updates
    ArduinoOTA.handle();
    
    // Maintain MQTT connection
    if (!client.connected()) {
        if (WiFi.status() == WL_CONNECTED) {
            setupMQTT();
        }
    }
    client.loop();
    
    // Check for calibration request
    if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
        delay(50); // Debounce
        if (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
            handleCalibration();
        }
    }
    
    // Perform measurement
    if (millis() - lastMeasurement >= config.measurement_interval) {
        performMeasurement();
        lastMeasurement = millis();
    }
    
    // Check sensor health
    checkSensorHealth();
    
    // Auto-calibration check
    if (millis() - lastCalibration >= config.calibration_interval) {
        if (calibration.calibrated) {
            Serial.println("Auto-calibration reminder - manual calibration recommended");
        }
        lastCalibration = millis();
    }
    
    // Power management
    if (config.measurement_interval > 60000) { // If interval > 1 minute
        enterSleepMode();
    }
    
    delay(1000);
}

void setupWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("WiFi connected: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi connection failed");
    }
}

void setupMQTT() {
    espClient.setInsecure(); // For testing - use proper certificates in production
    client.setServer(mqtt_server, mqtt_port);
    
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "DissolvedOxygenSensor-" + String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            Serial.println("Connected to MQTT broker");
            client.subscribe("aquatic/dissolved_oxygen/config");
            client.subscribe("aquatic/dissolved_oxygen/calibrate");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
        }
    }
}

void setupOTA() {
    ArduinoOTA.setHostname("dissolved-oxygen-sensor");
    ArduinoOTA.setPassword("your_ota_password");
    
    ArduinoOTA.onStart([]() {
        Serial.println("Starting OTA update");
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println("OTA update completed");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA Progress: %u%%\n", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    
    ArduinoOTA.begin();
}

void setupSensors() {
    // Initialize temperature sensor
    temperatureSensor.begin();
    temperatureSensor.setResolution(12);
    
    // Initialize I2C for pressure sensor
    Wire.begin(PRESSURE_SENSOR_SDA, PRESSURE_SENSOR_SCL);
    
    // Initialize ADC for DO sensor
    analogReadResolution(12);
    
    Serial.println("Sensors initialized");
}

void performMeasurement() {
    MeasurementData data;
    data.timestamp = millis();
    
    if (config.dummy_mode) {
        generateDummyData(data);
    } else {
        // Read raw sensor values
        data.do_voltage = readDOVoltage();
        
        // Read temperature
        temperatureSensor.requestTemperatures();
        data.temperature = temperatureSensor.getTempCByIndex(0);
        
        // Read pressure
        data.pressure = readPressure();
        
        // Get salinity (should be read from conductivity sensor)
        data.salinity = 0.0; // Freshwater assumption
        
        // Calculate DO concentration
        calculateDO(data);
        
        // Apply compensations
        applyTemperatureCompensation(data);
        applySalinityCompensation(data);
        applyPressureCompensation(data);
        
        // Calculate saturation percentage
        data.do_saturation_percent = calculateSaturation(data.do_concentration_mg_l, data.temperature, data.salinity);
    }
    
    // Validate measurement
    data.valid = (data.do_concentration_mg_l >= 0 && data.do_concentration_mg_l <= 20 &&
                  data.temperature > -10 && data.temperature < 50 &&
                  data.do_saturation_percent >= 0 && data.do_saturation_percent <= 200);
    
    if (data.valid) {
        consecutiveErrors = 0;
        data.quality_score = calculateQualityScore(data);
        
        // Publish data
        publishData(data);
        
        // Check for alarms
        handleAlarms(data);
        
        Serial.println("Measurement completed successfully");
    } else {
        consecutiveErrors++;
        Serial.println("Invalid measurement detected");
    }
}

void calculateDO(MeasurementData& data) {
    if (calibration.calibrated) {
        // Use calibration data
        float span_voltage = calibration.span_do_voltage - calibration.zero_do_voltage;
        float measured_voltage = data.do_voltage - calibration.zero_do_voltage;
        
        // Calculate concentration based on calibration
        float saturation_at_cal = getOxygenSolubility(calibration.span_temperature, 0.0);
        data.do_concentration_mg_l = (measured_voltage / span_voltage) * saturation_at_cal;
    } else {
        // Use default calibration
        data.do_concentration_mg_l = (data.do_voltage - config.calibration_intercept) * config.calibration_slope;
    }
}

void applyTemperatureCompensation(MeasurementData& data) {
    float temp_factor = 1.0 + config.temp_coefficient * (data.temperature - config.reference_temp);
    data.do_concentration_mg_l *= temp_factor;
}

void applySalinityCompensation(MeasurementData& data) {
    float salinity_factor = 1.0 - config.salinity_coefficient * (data.salinity - config.reference_salinity);
    data.do_concentration_mg_l *= salinity_factor;
}

void applyPressureCompensation(MeasurementData& data) {
    float pressure_factor = 1.0 + config.pressure_coefficient * (data.pressure - config.reference_pressure);
    data.do_concentration_mg_l *= pressure_factor;
}

void publishData(const MeasurementData& data) {
    // Publish to MQTT
    DynamicJsonDocument doc(1024);
    
    doc["sensor_id"] = "dissolved_oxygen_001";
    doc["timestamp"] = data.timestamp;
    doc["do_mg_l"] = data.do_concentration_mg_l;
    doc["do_saturation_percent"] = data.do_saturation_percent;
    doc["temperature_c"] = data.temperature;
    doc["salinity_ppt"] = data.salinity;
    doc["pressure_mbar"] = data.pressure;
    doc["do_voltage_mv"] = data.do_voltage;
    doc["quality_score"] = data.quality_score;
    doc["calibrated"] = calibration.calibrated;
    doc["sensor_health"] = sensorHealthy;
    doc["dummy_mode"] = config.dummy_mode;
    
    char buffer[1024];
    serializeJson(doc, buffer);
    
    client.publish("aquatic/dissolved_oxygen/data", buffer);
    
    Serial.println("Data published to MQTT");
    
    // Send to API endpoint
    sendToAPI(data);
}

void sendToAPI(const MeasurementData& data) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected - skipping API call");
        return;
    }
    
    HTTPClient http;
    WiFiClient apiClient;
    
    // Configure API endpoint
    String apiUrl = "http://" + String(api_server) + ":" + String(api_port) + String(api_endpoint);
    http.begin(apiClient, apiUrl);
    http.addHeader("Content-Type", "application/json");
    
    // Prepare API payload according to the API documentation
    DynamicJsonDocument apiDoc(256);
    apiDoc["pool_id"] = pool_id;
    apiDoc["do_ppm"] = data.do_concentration_mg_l;
    apiDoc["do_percent_saturation"] = data.do_saturation_percent;
    apiDoc["notes"] = "Automated reading from dissolved oxygen sensor";
    
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

void handleCalibration() {
    Serial.println("Starting calibration procedure...");
    digitalWrite(STATUS_LED_PIN, HIGH);
    
    performCalibration();
    
    digitalWrite(STATUS_LED_PIN, LOW);
    Serial.println("Calibration completed");
}

void performCalibration() {
    Serial.println("Dissolved Oxygen Calibration");
    
    // Zero calibration (0% saturation)
    Serial.println("Place sensor in sodium sulfite solution (0% DO). Press button when ready.");
    while (digitalRead(CALIBRATION_BUTTON_PIN) == HIGH) {
        delay(100);
    }
    
    delay(2000); // Wait for stabilization
    calibration.zero_do_voltage = readDOVoltage();
    Serial.print("Zero voltage recorded: ");
    Serial.println(calibration.zero_do_voltage);
    
    // Wait for button release
    while (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
        delay(100);
    }
    delay(1000);
    
    // Span calibration (100% saturation)
    Serial.println("Place sensor in air-saturated water. Press button when ready.");
    while (digitalRead(CALIBRATION_BUTTON_PIN) == HIGH) {
        delay(100);
    }
    
    delay(2000); // Wait for stabilization
    calibration.span_do_voltage = readDOVoltage();
    temperatureSensor.requestTemperatures();
    calibration.span_temperature = temperatureSensor.getTempCByIndex(0);
    
    Serial.print("Span voltage recorded: ");
    Serial.println(calibration.span_do_voltage);
    Serial.print("Span temperature: ");
    Serial.println(calibration.span_temperature);
    
    calibration.calibrated = true;
    calibration.last_calibration = millis();
    
    // Save calibration data
    saveSensorConfig();
}

void generateDummyData(MeasurementData& data) {
    // Generate realistic dummy data
    data.do_concentration_mg_l = 6.0 + (random(-200, 400) / 100.0);
    data.temperature = 20.0 + (random(-50, 100) / 10.0);
    data.salinity = random(0, 50) / 10.0;
    data.pressure = 1013.25 + random(-100, 100);
    data.do_voltage = 500.0 + random(-200, 200);
    data.do_saturation_percent = 80.0 + (random(-300, 400) / 10.0);
    data.valid = true;
    data.quality_score = 85 + random(0, 15);
}

void checkSensorHealth() {
    if (consecutiveErrors >= maxConsecutiveErrors) {
        sensorHealthy = false;
        Serial.println("Sensor health degraded - consecutive errors detected");
        
        // Publish health alert
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "dissolved_oxygen_001";
        doc["alert_type"] = "sensor_health";
        doc["status"] = "degraded";
        doc["consecutive_errors"] = consecutiveErrors;
        doc["timestamp"] = millis();
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/dissolved_oxygen/alerts", buffer);
    } else {
        sensorHealthy = true;
    }
}

void handleAlarms(const MeasurementData& data) {
    if (data.do_concentration_mg_l < config.alarm_threshold_low) {
        Serial.println("ALARM: Low dissolved oxygen detected!");
        
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "dissolved_oxygen_001";
        doc["alert_type"] = "low_oxygen";
        doc["value"] = data.do_concentration_mg_l;
        doc["threshold"] = config.alarm_threshold_low;
        doc["timestamp"] = data.timestamp;
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/dissolved_oxygen/alerts", buffer);
    }
    
    if (data.do_concentration_mg_l > config.alarm_threshold_high) {
        Serial.println("ALARM: High dissolved oxygen detected!");
        
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "dissolved_oxygen_001";
        doc["alert_type"] = "high_oxygen";
        doc["value"] = data.do_concentration_mg_l;
        doc["threshold"] = config.alarm_threshold_high;
        doc["timestamp"] = data.timestamp;
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/dissolved_oxygen/alerts", buffer);
    }
}

void saveSensorConfig() {
    EEPROM.put(0, config);
    EEPROM.put(sizeof(config), calibration);
    EEPROM.commit();
    Serial.println("Configuration saved to EEPROM");
}

void loadSensorConfig() {
    EEPROM.get(0, config);
    EEPROM.get(sizeof(config), calibration);
    Serial.println("Configuration loaded from EEPROM");
}

void enterSleepMode() {
    Serial.println("Entering sleep mode...");
    
    // Turn off sensors
    digitalWrite(POWER_CONTROL_PIN, LOW);
    digitalWrite(STATUS_LED_PIN, LOW);
    
    // Configure wake-up
    esp_sleep_enable_timer_wakeup(config.measurement_interval * 1000);
    
    // Enter deep sleep
    esp_deep_sleep_start();
}

float readDOVoltage() {
    int rawValue = 0;
    
    // Take multiple readings for stability
    for (int i = 0; i < 10; i++) {
        rawValue += analogRead(DO_SENSOR_PIN);
        delay(10);
    }
    
    rawValue /= 10;
    
    // Convert to voltage (0-3.3V range)
    return (rawValue * 3.3) / 4095.0 * 1000.0; // Convert to mV
}

float readPressure() {
    // Simulate pressure reading - replace with actual sensor code
    return 1013.25 + (random(-50, 50) / 10.0);
}

float calculateSaturation(float do_mg_l, float temp, float salinity) {
    float solubility = getOxygenSolubility(temp, salinity);
    return (do_mg_l / solubility) * 100.0;
}

float getOxygenSolubility(float temp, float salinity) {
    // Simplified calculation - replace with accurate formula
    float base_solubility = 14.652 - 0.41022 * temp + 0.007991 * temp * temp - 0.000077774 * temp * temp * temp;
    float salinity_correction = 1.0 - 0.000033 * salinity;
    return base_solubility * salinity_correction;
}

int calculateQualityScore(const MeasurementData& data) {
    int score = 100;
    
    // Reduce score for extreme values
    if (data.do_concentration_mg_l < 2.0 || data.do_concentration_mg_l > 12.0) score -= 20;
    if (data.temperature < 5 || data.temperature > 35) score -= 10;
    if (data.do_saturation_percent < 50 || data.do_saturation_percent > 120) score -= 15;
    
    // Reduce score if not calibrated
    if (!calibration.calibrated) score -= 30;
    
    // Reduce score based on age of calibration
    unsigned long calibration_age = millis() - calibration.last_calibration;
    if (calibration_age > 604800000) score -= 20; // 1 week
    
    return max(0, score);
}
