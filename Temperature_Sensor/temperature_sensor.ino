/*
 * Temperature Sensor Module
 * Aquatic Monitoring System
 * 
 * This module measures water temperature using multiple DS18B20 sensors
 * for redundancy and accuracy.
 * 
 * Features:
 * - Multiple temperature sensors (redundancy)
 * - High precision measurement
 * - Sensor failure detection
 * - Data averaging and validation
 * - MQTT communication
 * - OTA updates
 * - Power management
 * - Dummy data generation
 * - Sensor health monitoring
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

// Pin definitions
#define TEMP_SENSOR_PIN 4
#define POWER_CONTROL_PIN 2
#define STATUS_LED_PIN 5
#define CALIBRATION_BUTTON_PIN 0

// Temperature sensor setup
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature temperatureSensors(&oneWire);

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
const char* api_endpoint = "/api/temperature-readings";
const int pool_id = 1;  // Configure this for each installation

WiFiClientSecure espClient;
PubSubClient client(espClient);

// Sensor configuration
struct SensorConfig {
    float calibration_offset = 0.0;    // °C offset
    float alarm_threshold_low = 5.0;   // °C
    float alarm_threshold_high = 35.0; // °C
    unsigned long measurement_interval = 30000; // ms
    unsigned long calibration_interval = 2592000000; // 30 days
    bool dummy_mode = false;
    int resolution = 12; // 9-12 bits
};

SensorConfig config;

// Measurement data
struct MeasurementData {
    float temperatures[8];    // Up to 8 sensors
    float average_temperature;
    float min_temperature;
    float max_temperature;
    float temperature_range;
    int active_sensors;
    int failed_sensors;
    unsigned long timestamp;
    bool valid;
    int quality_score;
    DeviceAddress sensor_addresses[8];
};

// Calibration data
struct CalibrationData {
    float reference_temp = 25.0;   // °C
    float measured_temp;
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
void scanSensors();
void calculateStatistics(MeasurementData& data);
void publishData(const MeasurementData& data);
void handleCalibration();
void performCalibration();
void saveSensorConfig();
void loadSensorConfig();
void generateDummyData(MeasurementData& data);
void checkSensorHealth();
void handleAlarms(const MeasurementData& data);
void enterSleepMode();
int calculateQualityScore(const MeasurementData& data);

void setup() {
    Serial.begin(115200);
    Serial.println("Temperature Sensor Module Starting...");
    
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
    
    Serial.println("Temperature Sensor Module Ready");
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
        String clientId = "TemperatureSensor-" + String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            Serial.println("Connected to MQTT broker");
            client.subscribe("aquatic/temperature/config");
            client.subscribe("aquatic/temperature/calibrate");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
        }
    }
}

void setupOTA() {
    ArduinoOTA.setHostname("temperature-sensor");
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
    // Initialize temperature sensors
    temperatureSensors.begin();
    temperatureSensors.setResolution(config.resolution);
    
    // Scan for connected sensors
    scanSensors();
    
    Serial.println("Temperature sensors initialized");
}

void scanSensors() {
    int deviceCount = temperatureSensors.getDeviceCount();
    Serial.print("Found ");
    Serial.print(deviceCount);
    Serial.println(" temperature sensors");
    
    for (int i = 0; i < deviceCount && i < 8; i++) {
        DeviceAddress address;
        if (temperatureSensors.getAddress(address, i)) {
            Serial.print("Sensor ");
            Serial.print(i);
            Serial.print(" Address: ");
            for (int j = 0; j < 8; j++) {
                Serial.print(address[j], HEX);
                Serial.print(" ");
            }
            Serial.println();
        }
    }
}

void performMeasurement() {
    MeasurementData data;
    data.timestamp = millis();
    data.active_sensors = 0;
    data.failed_sensors = 0;
    
    if (config.dummy_mode) {
        generateDummyData(data);
    } else {
        // Request temperature measurements
        temperatureSensors.requestTemperatures();
        
        // Read from all sensors
        int deviceCount = temperatureSensors.getDeviceCount();
        for (int i = 0; i < deviceCount && i < 8; i++) {
            float temp = temperatureSensors.getTempCByIndex(i);
            
            if (temp != DEVICE_DISCONNECTED_C) {
                data.temperatures[data.active_sensors] = temp + config.calibration_offset;
                data.active_sensors++;
            } else {
                data.failed_sensors++;
            }
        }
        
        // Calculate statistics
        calculateStatistics(data);
    }
    
    // Validate measurement
    data.valid = (data.active_sensors > 0 && 
                  data.average_temperature > -50 && 
                  data.average_temperature < 100);
    
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

void calculateStatistics(MeasurementData& data) {
    if (data.active_sensors == 0) {
        data.average_temperature = 0;
        data.min_temperature = 0;
        data.max_temperature = 0;
        data.temperature_range = 0;
        return;
    }
    
    // Calculate average
    float sum = 0;
    data.min_temperature = data.temperatures[0];
    data.max_temperature = data.temperatures[0];
    
    for (int i = 0; i < data.active_sensors; i++) {
        sum += data.temperatures[i];
        
        if (data.temperatures[i] < data.min_temperature) {
            data.min_temperature = data.temperatures[i];
        }
        
        if (data.temperatures[i] > data.max_temperature) {
            data.max_temperature = data.temperatures[i];
        }
    }
    
    data.average_temperature = sum / data.active_sensors;
    data.temperature_range = data.max_temperature - data.min_temperature;
}

void publishData(const MeasurementData& data) {
    // Publish to MQTT
    DynamicJsonDocument doc(1024);
    
    doc["sensor_id"] = "temperature_001";
    doc["timestamp"] = data.timestamp;
    doc["temperature_c"] = data.average_temperature;
    doc["min_temperature_c"] = data.min_temperature;
    doc["max_temperature_c"] = data.max_temperature;
    doc["temperature_range_c"] = data.temperature_range;
    doc["active_sensors"] = data.active_sensors;
    doc["failed_sensors"] = data.failed_sensors;
    doc["quality_score"] = data.quality_score;
    doc["calibrated"] = calibration.calibrated;
    doc["sensor_health"] = sensorHealthy;
    doc["dummy_mode"] = config.dummy_mode;
    
    // Add individual sensor readings
    JsonArray sensors = doc.createNestedArray("individual_sensors");
    for (int i = 0; i < data.active_sensors; i++) {
        sensors.add(data.temperatures[i]);
    }
    
    char buffer[1024];
    serializeJson(doc, buffer);
    
    client.publish("aquatic/temperature/data", buffer);
    
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
    apiDoc["temperature_celsius"] = data.average_temperature;
    apiDoc["notes"] = "Automated reading from temperature sensor array";
    
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
    Serial.println("Temperature Calibration");
    Serial.print("Place sensors in ");
    Serial.print(calibration.reference_temp);
    Serial.println("°C reference bath. Press button when ready.");
    
    while (digitalRead(CALIBRATION_BUTTON_PIN) == HIGH) {
        delay(100);
    }
    
    delay(5000); // Wait for thermal equilibrium
    
    // Read calibration temperature
    temperatureSensors.requestTemperatures();
    calibration.measured_temp = temperatureSensors.getTempCByIndex(0);
    
    // Calculate offset
    config.calibration_offset = calibration.reference_temp - calibration.measured_temp;
    
    calibration.calibrated = true;
    calibration.last_calibration = millis();
    
    Serial.print("Measured temperature: ");
    Serial.println(calibration.measured_temp);
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
    data.active_sensors = 3;
    data.failed_sensors = 0;
    
    float base_temp = 20.0 + (random(-50, 100) / 10.0);
    
    for (int i = 0; i < data.active_sensors; i++) {
        data.temperatures[i] = base_temp + (random(-20, 20) / 10.0);
    }
    
    calculateStatistics(data);
    
    data.valid = true;
    data.quality_score = 90 + random(0, 10);
}

void checkSensorHealth() {
    if (consecutiveErrors >= maxConsecutiveErrors) {
        sensorHealthy = false;
        Serial.println("Sensor health degraded - consecutive errors detected");
        
        // Publish health alert
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "temperature_001";
        doc["alert_type"] = "sensor_health";
        doc["status"] = "degraded";
        doc["consecutive_errors"] = consecutiveErrors;
        doc["timestamp"] = millis();
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/temperature/alerts", buffer);
    } else {
        sensorHealthy = true;
    }
}

void handleAlarms(const MeasurementData& data) {
    if (data.average_temperature < config.alarm_threshold_low) {
        Serial.println("ALARM: Low temperature detected!");
        
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "temperature_001";
        doc["alert_type"] = "low_temperature";
        doc["value"] = data.average_temperature;
        doc["threshold"] = config.alarm_threshold_low;
        doc["timestamp"] = data.timestamp;
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/temperature/alerts", buffer);
    }
    
    if (data.average_temperature > config.alarm_threshold_high) {
        Serial.println("ALARM: High temperature detected!");
        
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "temperature_001";
        doc["alert_type"] = "high_temperature";
        doc["value"] = data.average_temperature;
        doc["threshold"] = config.alarm_threshold_high;
        doc["timestamp"] = data.timestamp;
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/temperature/alerts", buffer);
    }
    
    if (data.temperature_range > 2.0) {
        Serial.println("WARNING: Large temperature variation between sensors!");
        
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "temperature_001";
        doc["alert_type"] = "temperature_variation";
        doc["value"] = data.temperature_range;
        doc["threshold"] = 2.0;
        doc["timestamp"] = data.timestamp;
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/temperature/alerts", buffer);
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

int calculateQualityScore(const MeasurementData& data) {
    int score = 100;
    
    // Reduce score for failed sensors
    score -= data.failed_sensors * 20;
    
    // Reduce score for temperature variation
    if (data.temperature_range > 1.0) score -= 10;
    if (data.temperature_range > 2.0) score -= 20;
    
    // Reduce score for extreme temperatures
    if (data.average_temperature < 10 || data.average_temperature > 30) score -= 10;
    
    // Reduce score if not calibrated
    if (!calibration.calibrated) score -= 20;
    
    // Reduce score based on age of calibration
    unsigned long calibration_age = millis() - calibration.last_calibration;
    if (calibration_age > 2592000000) score -= 15; // 30 days
    
    return max(0, score);
}
