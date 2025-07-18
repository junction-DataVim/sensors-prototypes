/*
 * Nitrite/Nitrate Sensor Module
 * Aquatic Monitoring System
 * 
 * This module measures nitrite (NO2-) and nitrate (NO3-) levels in water using
 * ion-selective electrodes (ISE) or colorimetric methods.
 * 
 * Features:
 * - Dual measurement (NO2- and NO3-)
 * - Temperature compensation
 * - Cross-sensor compensation (pH, conductivity)
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
#include <driver/adc.h>

// Pin definitions
#define NITRITE_SENSOR_PIN 34
#define NITRATE_SENSOR_PIN 35
#define TEMP_SENSOR_PIN 4
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
const char* api_endpoint_nitrite = "/api/nitrite-readings";
const char* api_endpoint_nitrate = "/api/nitrate-readings";
const int pool_id = 1;  // Configure this for each installation

WiFiClientSecure espClient;
PubSubClient client(espClient);

// Sensor configuration
struct SensorConfig {
    float nitrite_slope = -59.16;      // mV/decade
    float nitrite_intercept = 0.0;     // mV
    float nitrate_slope = -59.16;      // mV/decade
    float nitrate_intercept = 0.0;     // mV
    float temp_coefficient = 0.02;     // %/°C
    float ph_compensation = 0.1;       // compensation factor
    float conductivity_compensation = 0.05; // compensation factor
    unsigned long measurement_interval = 30000; // ms
    unsigned long calibration_interval = 86400000; // 24 hours
    bool dummy_mode = false;
    float alarm_threshold_no2 = 0.5;   // mg/L
    float alarm_threshold_no3 = 50.0;  // mg/L
};

SensorConfig config;

// Measurement data
struct MeasurementData {
    float nitrite_voltage;
    float nitrate_voltage;
    float nitrite_concentration;
    float nitrate_concentration;
    float temperature;
    float ph_value;
    float conductivity;
    unsigned long timestamp;
    bool valid;
    int quality_score;
};

// Calibration data
struct CalibrationData {
    float nitrite_standards[3] = {0.1, 1.0, 10.0}; // mg/L
    float nitrate_standards[3] = {1.0, 10.0, 100.0}; // mg/L
    float nitrite_voltages[3];
    float nitrate_voltages[3];
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
void calculateConcentrations(MeasurementData& data);
void applyTemperatureCompensation(MeasurementData& data);
void applyCrossSensorCompensation(MeasurementData& data);
void publishData(const MeasurementData& data);
void handleCalibration();
void performCalibration();
void saveSensorConfig();
void loadSensorConfig();
void generateDummyData(MeasurementData& data);
void checkSensorHealth();
void handleAlarms(const MeasurementData& data);
void enterSleepMode();
float readAnalogVoltage(int pin);
int calculateQualityScore(const MeasurementData& data);

void setup() {
    Serial.begin(115200);
    Serial.println("Nitrite/Nitrate Sensor Module Starting...");
    
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
    
    Serial.println("Nitrite/Nitrate Sensor Module Ready");
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
        String clientId = "NitriteNitrateSensor-" + String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            Serial.println("Connected to MQTT broker");
            client.subscribe("aquatic/nitrite_nitrate/config");
            client.subscribe("aquatic/nitrite_nitrate/calibrate");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
        }
    }
}

void setupOTA() {
    ArduinoOTA.setHostname("nitrite-nitrate-sensor");
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
    
    // Initialize ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); // Pin 34
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11); // Pin 35
    
    Serial.println("Sensors initialized");
}

void performMeasurement() {
    MeasurementData data;
    data.timestamp = millis();
    
    if (config.dummy_mode) {
        generateDummyData(data);
    } else {
        // Read raw sensor values
        data.nitrite_voltage = readAnalogVoltage(NITRITE_SENSOR_PIN);
        data.nitrate_voltage = readAnalogVoltage(NITRATE_SENSOR_PIN);
        
        // Read temperature
        temperatureSensor.requestTemperatures();
        data.temperature = temperatureSensor.getTempCByIndex(0);
        
        // Get cross-sensor data (simulated for now)
        data.ph_value = 7.0; // Should be read from pH sensor
        data.conductivity = 500.0; // Should be read from conductivity sensor
        
        // Calculate concentrations
        calculateConcentrations(data);
        
        // Apply compensations
        applyTemperatureCompensation(data);
        applyCrossSensorCompensation(data);
    }
    
    // Validate measurement
    data.valid = (data.nitrite_concentration >= 0 && data.nitrite_concentration <= 100 &&
                  data.nitrate_concentration >= 0 && data.nitrate_concentration <= 1000 &&
                  data.temperature > -10 && data.temperature < 50);
    
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

void calculateConcentrations(MeasurementData& data) {
    if (calibration.calibrated) {
        // Use calibration data
        data.nitrite_concentration = pow(10, (data.nitrite_voltage - calibration.nitrite_voltages[1]) / config.nitrite_slope);
        data.nitrate_concentration = pow(10, (data.nitrate_voltage - calibration.nitrate_voltages[1]) / config.nitrate_slope);
    } else {
        // Use default calibration
        data.nitrite_concentration = pow(10, (data.nitrite_voltage - config.nitrite_intercept) / config.nitrite_slope);
        data.nitrate_concentration = pow(10, (data.nitrate_voltage - config.nitrate_intercept) / config.nitrate_slope);
    }
}

void applyTemperatureCompensation(MeasurementData& data) {
    float temp_factor = 1.0 + config.temp_coefficient * (data.temperature - 25.0);
    data.nitrite_concentration *= temp_factor;
    data.nitrate_concentration *= temp_factor;
}

void applyCrossSensorCompensation(MeasurementData& data) {
    // pH compensation
    float ph_factor = 1.0 + config.ph_compensation * (data.ph_value - 7.0);
    data.nitrite_concentration *= ph_factor;
    
    // Conductivity compensation
    float cond_factor = 1.0 + config.conductivity_compensation * (data.conductivity - 500.0) / 500.0;
    data.nitrite_concentration *= cond_factor;
    data.nitrate_concentration *= cond_factor;
}

void publishData(const MeasurementData& data) {
    // Publish to MQTT
    DynamicJsonDocument doc(1024);
    
    doc["sensor_id"] = "nitrite_nitrate_001";
    doc["timestamp"] = data.timestamp;
    doc["nitrite_mg_l"] = data.nitrite_concentration;
    doc["nitrate_mg_l"] = data.nitrate_concentration;
    doc["temperature_c"] = data.temperature;
    doc["nitrite_voltage_mv"] = data.nitrite_voltage;
    doc["nitrate_voltage_mv"] = data.nitrate_voltage;
    doc["quality_score"] = data.quality_score;
    doc["calibrated"] = calibration.calibrated;
    doc["sensor_health"] = sensorHealthy;
    doc["dummy_mode"] = config.dummy_mode;
    
    char buffer[1024];
    serializeJson(doc, buffer);
    
    client.publish("aquatic/nitrite_nitrate/data", buffer);
    
    Serial.println("Data published to MQTT");
    
    // Send to API endpoints
    sendToAPI(data);
}

void sendToAPI(const MeasurementData& data) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected - skipping API call");
        return;
    }
    
    HTTPClient http;
    WiFiClient apiClient;
    
    // Send nitrite reading
    String nitriteUrl = "http://" + String(api_server) + ":" + String(api_port) + String(api_endpoint_nitrite);
    http.begin(apiClient, nitriteUrl);
    http.addHeader("Content-Type", "application/json");
    
    DynamicJsonDocument nitriteDoc(256);
    nitriteDoc["pool_id"] = pool_id;
    nitriteDoc["nitrite_ppm"] = data.nitrite_concentration;
    nitriteDoc["notes"] = "Automated reading from nitrite sensor";
    
    String nitritePayload;
    serializeJson(nitriteDoc, nitritePayload);
    
    int nitriteResponse = http.POST(nitritePayload);
    if (nitriteResponse > 0) {
        Serial.println("Nitrite API Response: " + String(nitriteResponse));
    } else {
        Serial.println("Nitrite API Error: " + String(nitriteResponse));
    }
    http.end();
    
    // Send nitrate reading
    String nitrateUrl = "http://" + String(api_server) + ":" + String(api_port) + String(api_endpoint_nitrate);
    http.begin(apiClient, nitrateUrl);
    http.addHeader("Content-Type", "application/json");
    
    DynamicJsonDocument nitrateDoc(256);
    nitrateDoc["pool_id"] = pool_id;
    nitrateDoc["nitrate_ppm"] = data.nitrate_concentration;
    nitrateDoc["notes"] = "Automated reading from nitrate sensor";
    
    String nitratePayload;
    serializeJson(nitrateDoc, nitratePayload);
    
    int nitrateResponse = http.POST(nitratePayload);
    if (nitrateResponse > 0) {
        Serial.println("Nitrate API Response: " + String(nitrateResponse));
    } else {
        Serial.println("Nitrate API Error: " + String(nitrateResponse));
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
    Serial.println("Calibration mode - Follow prompts for standard solutions");
    
    // Calibrate nitrite sensor
    for (int i = 0; i < 3; i++) {
        Serial.print("Place sensor in ");
        Serial.print(calibration.nitrite_standards[i]);
        Serial.println(" mg/L nitrite standard. Press button when ready.");
        
        while (digitalRead(CALIBRATION_BUTTON_PIN) == HIGH) {
            delay(100);
        }
        
        delay(1000); // Wait for stabilization
        calibration.nitrite_voltages[i] = readAnalogVoltage(NITRITE_SENSOR_PIN);
        
        Serial.print("Voltage recorded: ");
        Serial.println(calibration.nitrite_voltages[i]);
        
        delay(2000);
        while (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
            delay(100);
        }
    }
    
    // Calibrate nitrate sensor
    for (int i = 0; i < 3; i++) {
        Serial.print("Place sensor in ");
        Serial.print(calibration.nitrate_standards[i]);
        Serial.println(" mg/L nitrate standard. Press button when ready.");
        
        while (digitalRead(CALIBRATION_BUTTON_PIN) == HIGH) {
            delay(100);
        }
        
        delay(1000); // Wait for stabilization
        calibration.nitrate_voltages[i] = readAnalogVoltage(NITRATE_SENSOR_PIN);
        
        Serial.print("Voltage recorded: ");
        Serial.println(calibration.nitrate_voltages[i]);
        
        delay(2000);
        while (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
            delay(100);
        }
    }
    
    // Calculate calibration parameters
    // Simple linear regression for now
    calibration.calibrated = true;
    calibration.last_calibration = millis();
    
    // Save calibration data
    saveSensorConfig();
}

void generateDummyData(MeasurementData& data) {
    // Generate realistic dummy data
    data.nitrite_concentration = 0.1 + (random(0, 50) / 100.0);
    data.nitrate_concentration = 5.0 + (random(0, 200) / 10.0);
    data.temperature = 20.0 + (random(-30, 150) / 10.0);
    data.nitrite_voltage = 100.0 + random(-500, 500);
    data.nitrate_voltage = 200.0 + random(-500, 500);
    data.ph_value = 7.0 + (random(-20, 20) / 10.0);
    data.conductivity = 500.0 + random(-200, 200);
    data.valid = true;
    data.quality_score = 85 + random(0, 15);
}

void checkSensorHealth() {
    if (consecutiveErrors >= maxConsecutiveErrors) {
        sensorHealthy = false;
        Serial.println("Sensor health degraded - consecutive errors detected");
        
        // Publish health alert
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "nitrite_nitrate_001";
        doc["alert_type"] = "sensor_health";
        doc["status"] = "degraded";
        doc["consecutive_errors"] = consecutiveErrors;
        doc["timestamp"] = millis();
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/nitrite_nitrate/alerts", buffer);
    } else {
        sensorHealthy = true;
    }
}

void handleAlarms(const MeasurementData& data) {
    if (data.nitrite_concentration > config.alarm_threshold_no2) {
        Serial.println("ALARM: High nitrite concentration detected!");
        
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "nitrite_nitrate_001";
        doc["alert_type"] = "high_nitrite";
        doc["value"] = data.nitrite_concentration;
        doc["threshold"] = config.alarm_threshold_no2;
        doc["timestamp"] = data.timestamp;
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/nitrite_nitrate/alerts", buffer);
    }
    
    if (data.nitrate_concentration > config.alarm_threshold_no3) {
        Serial.println("ALARM: High nitrate concentration detected!");
        
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "nitrite_nitrate_001";
        doc["alert_type"] = "high_nitrate";
        doc["value"] = data.nitrate_concentration;
        doc["threshold"] = config.alarm_threshold_no3;
        doc["timestamp"] = data.timestamp;
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/nitrite_nitrate/alerts", buffer);
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

float readAnalogVoltage(int pin) {
    int rawValue = 0;
    
    // Take multiple readings for stability
    for (int i = 0; i < 10; i++) {
        rawValue += analogRead(pin);
        delay(10);
    }
    
    rawValue /= 10;
    
    // Convert to voltage (0-3.3V range)
    return (rawValue * 3.3) / 4095.0 * 1000.0; // Convert to mV
}

int calculateQualityScore(const MeasurementData& data) {
    int score = 100;
    
    // Reduce score for extreme values
    if (data.nitrite_concentration > 5.0) score -= 20;
    if (data.nitrate_concentration > 200.0) score -= 20;
    if (data.temperature < 10 || data.temperature > 30) score -= 10;
    
    // Reduce score if not calibrated
    if (!calibration.calibrated) score -= 30;
    
    // Reduce score based on age of calibration
    unsigned long calibration_age = millis() - calibration.last_calibration;
    if (calibration_age > 604800000) score -= 20; // 1 week
    
    return max(0, score);
}
