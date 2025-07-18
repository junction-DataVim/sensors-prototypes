/*
 * ORP (Oxidation-Reduction Potential) Sensor Module
 * Aquatic Monitoring System
 * 
 * This module measures the oxidation-reduction potential (ORP/Redox) of water
 * using a platinum electrode and reference electrode.
 * 
 * Features:
 * - ORP measurement in mV
 * - Temperature compensation
 * - Automatic calibration
 * - MQTT communication
 * - OTA updates
 * - Power management
 * - Dummy data generation
 * - Sensor health monitoring
 * - Fail-safe mechanisms
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <esp_sleep.h>
#include <driver/adc.h>

// Pin definitions
#define ORP_SENSOR_PIN 34
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

WiFiClientSecure espClient;
PubSubClient client(espClient);

// Sensor configuration
struct SensorConfig {
    float calibration_offset = 0.0;    // mV offset
    float temp_coefficient = 2.0;      // mV/°C
    float reference_temp = 25.0;       // °C
    unsigned long measurement_interval = 30000; // ms
    unsigned long calibration_interval = 2592000000; // 30 days
    bool dummy_mode = false;
    float alarm_threshold_low = -200.0;  // mV
    float alarm_threshold_high = 800.0;  // mV
};

SensorConfig config;

// Measurement data
struct MeasurementData {
    float orp_voltage_raw;
    float orp_voltage_compensated;
    float temperature;
    unsigned long timestamp;
    bool valid;
    int quality_score;
    String water_condition;
};

// Calibration data
struct CalibrationData {
    float standard_orp_value = 468.0;  // mV (Quinhydrone standard)
    float measured_voltage;
    float calibration_temp;
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
void calculateORP(MeasurementData& data);
void applyTemperatureCompensation(MeasurementData& data);
void interpretWaterCondition(MeasurementData& data);
void publishData(const MeasurementData& data);
void handleCalibration();
void performCalibration();
void saveSensorConfig();
void loadSensorConfig();
void generateDummyData(MeasurementData& data);
void checkSensorHealth();
void handleAlarms(const MeasurementData& data);
void enterSleepMode();
float readORPVoltage();
int calculateQualityScore(const MeasurementData& data);

void setup() {
    Serial.begin(115200);
    Serial.println("ORP Sensor Module Starting...");
    
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
    
    Serial.println("ORP Sensor Module Ready");
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
        String clientId = "ORPSensor-" + String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            Serial.println("Connected to MQTT broker");
            client.subscribe("aquatic/orp/config");
            client.subscribe("aquatic/orp/calibrate");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
        }
    }
}

void setupOTA() {
    ArduinoOTA.setHostname("orp-sensor");
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
    
    Serial.println("Sensors initialized");
}

void performMeasurement() {
    MeasurementData data;
    data.timestamp = millis();
    
    if (config.dummy_mode) {
        generateDummyData(data);
    } else {
        // Read raw sensor values
        data.orp_voltage_raw = readORPVoltage();
        
        // Read temperature
        temperatureSensor.requestTemperatures();
        data.temperature = temperatureSensor.getTempCByIndex(0);
        
        // Calculate compensated ORP
        calculateORP(data);
        
        // Apply temperature compensation
        applyTemperatureCompensation(data);
        
        // Interpret water condition
        interpretWaterCondition(data);
    }
    
    // Validate measurement
    data.valid = (data.orp_voltage_compensated >= -500 && data.orp_voltage_compensated <= 1000 &&
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

void calculateORP(MeasurementData& data) {
    if (calibration.calibrated) {
        // Apply calibration offset
        data.orp_voltage_compensated = data.orp_voltage_raw + config.calibration_offset;
    } else {
        // Use raw measurement
        data.orp_voltage_compensated = data.orp_voltage_raw;
    }
}

void applyTemperatureCompensation(MeasurementData& data) {
    float temp_difference = data.temperature - config.reference_temp;
    float temp_compensation = config.temp_coefficient * temp_difference;
    data.orp_voltage_compensated += temp_compensation;
}

void interpretWaterCondition(MeasurementData& data) {
    if (data.orp_voltage_compensated < 200) {
        data.water_condition = "Highly Reducing";
    } else if (data.orp_voltage_compensated < 400) {
        data.water_condition = "Reducing";
    } else if (data.orp_voltage_compensated < 600) {
        data.water_condition = "Neutral";
    } else if (data.orp_voltage_compensated < 800) {
        data.water_condition = "Oxidizing";
    } else {
        data.water_condition = "Highly Oxidizing";
    }
}

void publishData(const MeasurementData& data) {
    DynamicJsonDocument doc(1024);
    
    doc["sensor_id"] = "orp_001";
    doc["timestamp"] = data.timestamp;
    doc["orp_mv"] = data.orp_voltage_compensated;
    doc["orp_raw_mv"] = data.orp_voltage_raw;
    doc["temperature_c"] = data.temperature;
    doc["water_condition"] = data.water_condition;
    doc["quality_score"] = data.quality_score;
    doc["calibrated"] = calibration.calibrated;
    doc["sensor_health"] = sensorHealthy;
    doc["dummy_mode"] = config.dummy_mode;
    
    char buffer[1024];
    serializeJson(doc, buffer);
    
    client.publish("aquatic/orp/data", buffer);
    
    Serial.println("Data published to MQTT");
}

void handleCalibration() {
    Serial.println("Starting calibration procedure...");
    digitalWrite(STATUS_LED_PIN, HIGH);
    
    performCalibration();
    
    digitalWrite(STATUS_LED_PIN, LOW);
    Serial.println("Calibration completed");
}

void performCalibration() {
    Serial.println("ORP Calibration with Standard Solution");
    Serial.print("Place sensor in ");
    Serial.print(calibration.standard_orp_value);
    Serial.println(" mV standard solution. Press button when ready.");
    
    while (digitalRead(CALIBRATION_BUTTON_PIN) == HIGH) {
        delay(100);
    }
    
    delay(2000); // Wait for stabilization
    
    // Read calibration values
    calibration.measured_voltage = readORPVoltage();
    temperatureSensor.requestTemperatures();
    calibration.calibration_temp = temperatureSensor.getTempCByIndex(0);
    
    // Calculate offset
    config.calibration_offset = calibration.standard_orp_value - calibration.measured_voltage;
    
    calibration.calibrated = true;
    calibration.last_calibration = millis();
    
    Serial.print("Measured voltage: ");
    Serial.println(calibration.measured_voltage);
    Serial.print("Calibration offset: ");
    Serial.println(config.calibration_offset);
    
    // Save calibration data
    saveSensorConfig();
    
    // Wait for button release
    while (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
        delay(100);
    }
}

void generateDummyData(MeasurementData& data) {
    // Generate realistic dummy data
    data.orp_voltage_raw = 400.0 + (random(-2000, 2000) / 10.0);
    data.orp_voltage_compensated = data.orp_voltage_raw + config.calibration_offset;
    data.temperature = 20.0 + (random(-50, 100) / 10.0);
    data.valid = true;
    data.quality_score = 85 + random(0, 15);
    
    // Apply temperature compensation
    applyTemperatureCompensation(data);
    
    // Interpret water condition
    interpretWaterCondition(data);
}

void checkSensorHealth() {
    if (consecutiveErrors >= maxConsecutiveErrors) {
        sensorHealthy = false;
        Serial.println("Sensor health degraded - consecutive errors detected");
        
        // Publish health alert
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "orp_001";
        doc["alert_type"] = "sensor_health";
        doc["status"] = "degraded";
        doc["consecutive_errors"] = consecutiveErrors;
        doc["timestamp"] = millis();
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/orp/alerts", buffer);
    } else {
        sensorHealthy = true;
    }
}

void handleAlarms(const MeasurementData& data) {
    if (data.orp_voltage_compensated < config.alarm_threshold_low) {
        Serial.println("ALARM: Low ORP detected - highly reducing conditions!");
        
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "orp_001";
        doc["alert_type"] = "low_orp";
        doc["value"] = data.orp_voltage_compensated;
        doc["threshold"] = config.alarm_threshold_low;
        doc["timestamp"] = data.timestamp;
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/orp/alerts", buffer);
    }
    
    if (data.orp_voltage_compensated > config.alarm_threshold_high) {
        Serial.println("ALARM: High ORP detected - highly oxidizing conditions!");
        
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "orp_001";
        doc["alert_type"] = "high_orp";
        doc["value"] = data.orp_voltage_compensated;
        doc["threshold"] = config.alarm_threshold_high;
        doc["timestamp"] = data.timestamp;
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/orp/alerts", buffer);
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

float readORPVoltage() {
    int rawValue = 0;
    
    // Take multiple readings for stability
    for (int i = 0; i < 20; i++) {
        rawValue += analogRead(ORP_SENSOR_PIN);
        delay(10);
    }
    
    rawValue /= 20;
    
    // Convert to voltage (0-3.3V range, then to mV)
    float voltage = (rawValue * 3.3) / 4095.0;
    
    // Convert to ORP voltage (may need amplification circuit)
    // Assuming amplification factor and offset
    return (voltage - 1.65) * 1000.0; // Convert to mV with 1.65V offset
}

int calculateQualityScore(const MeasurementData& data) {
    int score = 100;
    
    // Reduce score for extreme values
    if (data.orp_voltage_compensated < -300 || data.orp_voltage_compensated > 900) score -= 20;
    if (data.temperature < 10 || data.temperature > 30) score -= 10;
    
    // Reduce score if not calibrated
    if (!calibration.calibrated) score -= 30;
    
    // Reduce score based on age of calibration
    unsigned long calibration_age = millis() - calibration.last_calibration;
    if (calibration_age > 2592000000) score -= 20; // 30 days
    
    return max(0, score);
}
