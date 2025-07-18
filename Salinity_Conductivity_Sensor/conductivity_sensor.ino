/*
 * Salinity/Conductivity Sensor Module
 * Aquatic Monitoring System
 * 
 * This module measures electrical conductivity and calculates salinity
 * using a conductivity probe with temperature compensation.
 * 
 * Features:
 * - Electrical conductivity measurement (μS/cm)
 * - Salinity calculation (PSU/ppt)
 * - Temperature compensation
 * - TDS calculation
 * - Automatic calibration
 * - MQTT communication
 * - OTA updates
 * - Power management
 * - Dummy data generation
 * - Sensor health monitoring
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
#define CONDUCTIVITY_PIN 34
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
    float cell_constant = 1.0;         // K = 1.0 cm-1
    float temp_coefficient = 0.02;     // 2% per °C
    float reference_temp = 25.0;       // °C
    float tds_factor = 0.5;            // TDS conversion factor
    unsigned long measurement_interval = 30000; // ms
    unsigned long calibration_interval = 604800000; // 7 days
    bool dummy_mode = false;
    float alarm_threshold_low = 100.0;   // μS/cm
    float alarm_threshold_high = 10000.0; // μS/cm
};

SensorConfig config;

// Measurement data
struct MeasurementData {
    float raw_voltage;
    float conductivity_us_cm;
    float conductivity_ms_cm;
    float salinity_psu;
    float salinity_ppt;
    float tds_mg_l;
    float temperature;
    unsigned long timestamp;
    bool valid;
    int quality_score;
    String water_type;
};

// Calibration data
struct CalibrationData {
    float dry_cal_voltage;           // Dry calibration
    float standard_conductivity[3];  // Standard solutions (μS/cm)
    float measured_voltages[3];      // Measured voltages
    float calibration_temps[3];      // Calibration temperatures
    int calibration_points = 0;
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
void calculateConductivity(MeasurementData& data);
void calculateSalinity(MeasurementData& data);
void calculateTDS(MeasurementData& data);
void applyTemperatureCompensation(MeasurementData& data);
void interpretWaterType(MeasurementData& data);
void publishData(const MeasurementData& data);
void handleCalibration();
void performCalibration();
void saveSensorConfig();
void loadSensorConfig();
void generateDummyData(MeasurementData& data);
void checkSensorHealth();
void handleAlarms(const MeasurementData& data);
void enterSleepMode();
float readConductivityVoltage();
float voltageToResistance(float voltage);
float resistanceToConductivity(float resistance);
int calculateQualityScore(const MeasurementData& data);

void setup() {
    Serial.begin(115200);
    Serial.println("Salinity/Conductivity Sensor Module Starting...");
    
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
    
    Serial.println("Salinity/Conductivity Sensor Module Ready");
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
        String clientId = "ConductivitySensor-" + String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            Serial.println("Connected to MQTT broker");
            client.subscribe("aquatic/conductivity/config");
            client.subscribe("aquatic/conductivity/calibrate");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
        }
    }
}

void setupOTA() {
    ArduinoOTA.setHostname("conductivity-sensor");
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
        data.raw_voltage = readConductivityVoltage();
        
        // Read temperature
        temperatureSensor.requestTemperatures();
        data.temperature = temperatureSensor.getTempCByIndex(0);
        
        // Calculate conductivity
        calculateConductivity(data);
        
        // Apply temperature compensation
        applyTemperatureCompensation(data);
        
        // Calculate derived values
        calculateSalinity(data);
        calculateTDS(data);
        
        // Interpret water type
        interpretWaterType(data);
    }
    
    // Validate measurement
    data.valid = (data.conductivity_us_cm >= 0 && data.conductivity_us_cm <= 200000 &&
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

void calculateConductivity(MeasurementData& data) {
    // Convert voltage to resistance
    float resistance = voltageToResistance(data.raw_voltage);
    
    // Convert resistance to conductivity
    data.conductivity_us_cm = resistanceToConductivity(resistance);
    data.conductivity_ms_cm = data.conductivity_us_cm / 1000.0;
}

void calculateSalinity(MeasurementData& data) {
    // UNESCO practical salinity scale (simplified)
    // For seawater: conductivity ratio at 25°C
    float conductivity_ratio = data.conductivity_ms_cm / 53.087; // Standard seawater at 25°C
    
    if (conductivity_ratio > 0.0002) {
        // Polynomial approximation for practical salinity
        float R = conductivity_ratio;
        data.salinity_psu = -0.08996 + 28.29720 * R + 12.80832 * R * R - 10.67869 * R * R * R;
        data.salinity_ppt = data.salinity_psu; // PSU ≈ ppt for practical purposes
    } else {
        data.salinity_psu = 0.0;
        data.salinity_ppt = 0.0;
    }
}

void calculateTDS(MeasurementData& data) {
    // TDS approximation from conductivity
    data.tds_mg_l = data.conductivity_us_cm * config.tds_factor;
}

void applyTemperatureCompensation(MeasurementData& data) {
    float temp_difference = data.temperature - config.reference_temp;
    float compensation_factor = 1.0 + config.temp_coefficient * temp_difference;
    data.conductivity_us_cm /= compensation_factor;
    data.conductivity_ms_cm = data.conductivity_us_cm / 1000.0;
}

void interpretWaterType(MeasurementData& data) {
    if (data.conductivity_us_cm < 100) {
        data.water_type = "Ultra Pure";
    } else if (data.conductivity_us_cm < 500) {
        data.water_type = "Distilled";
    } else if (data.conductivity_us_cm < 1000) {
        data.water_type = "Deionized";
    } else if (data.conductivity_us_cm < 5000) {
        data.water_type = "Fresh Water";
    } else if (data.conductivity_us_cm < 30000) {
        data.water_type = "Brackish";
    } else {
        data.water_type = "Seawater";
    }
}

void publishData(const MeasurementData& data) {
    DynamicJsonDocument doc(1024);
    
    doc["sensor_id"] = "conductivity_001";
    doc["timestamp"] = data.timestamp;
    doc["conductivity_us_cm"] = data.conductivity_us_cm;
    doc["conductivity_ms_cm"] = data.conductivity_ms_cm;
    doc["salinity_psu"] = data.salinity_psu;
    doc["salinity_ppt"] = data.salinity_ppt;
    doc["tds_mg_l"] = data.tds_mg_l;
    doc["temperature_c"] = data.temperature;
    doc["water_type"] = data.water_type;
    doc["raw_voltage"] = data.raw_voltage;
    doc["quality_score"] = data.quality_score;
    doc["calibrated"] = calibration.calibrated;
    doc["sensor_health"] = sensorHealthy;
    doc["dummy_mode"] = config.dummy_mode;
    
    char buffer[1024];
    serializeJson(doc, buffer);
    
    client.publish("aquatic/conductivity/data", buffer);
    
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
    Serial.println("Conductivity Calibration");
    
    // Dry calibration
    Serial.println("Remove sensor from water for dry calibration. Press button when ready.");
    while (digitalRead(CALIBRATION_BUTTON_PIN) == HIGH) {
        delay(100);
    }
    
    delay(2000);
    calibration.dry_cal_voltage = readConductivityVoltage();
    Serial.print("Dry calibration voltage: ");
    Serial.println(calibration.dry_cal_voltage);
    
    // Wait for button release
    while (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
        delay(100);
    }
    delay(2000);
    
    // Standard solutions calibration
    float standards[] = {1413.0, 12880.0, 111800.0}; // μS/cm
    calibration.calibration_points = 0;
    
    for (int i = 0; i < 3; i++) {
        Serial.print("Place sensor in ");
        Serial.print(standards[i]);
        Serial.println(" μS/cm standard solution. Press button when ready.");
        
        while (digitalRead(CALIBRATION_BUTTON_PIN) == HIGH) {
            delay(100);
        }
        
        delay(2000); // Wait for stabilization
        
        calibration.standard_conductivity[i] = standards[i];
        calibration.measured_voltages[i] = readConductivityVoltage();
        
        temperatureSensor.requestTemperatures();
        calibration.calibration_temps[i] = temperatureSensor.getTempCByIndex(0);
        
        Serial.print("Voltage recorded: ");
        Serial.println(calibration.measured_voltages[i]);
        
        calibration.calibration_points++;
        
        // Wait for button release
        while (digitalRead(CALIBRATION_BUTTON_PIN) == LOW) {
            delay(100);
        }
        delay(2000);
    }
    
    calibration.calibrated = true;
    calibration.last_calibration = millis();
    
    // Save calibration data
    saveSensorConfig();
}

void generateDummyData(MeasurementData& data) {
    // Generate realistic dummy data
    data.raw_voltage = 1.0 + (random(0, 2000) / 1000.0);
    data.conductivity_us_cm = 500.0 + (random(0, 5000));
    data.conductivity_ms_cm = data.conductivity_us_cm / 1000.0;
    data.temperature = 20.0 + (random(-50, 100) / 10.0);
    
    // Calculate derived values
    calculateSalinity(data);
    calculateTDS(data);
    interpretWaterType(data);
    
    data.valid = true;
    data.quality_score = 85 + random(0, 15);
}

void checkSensorHealth() {
    if (consecutiveErrors >= maxConsecutiveErrors) {
        sensorHealthy = false;
        Serial.println("Sensor health degraded - consecutive errors detected");
        
        // Publish health alert
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "conductivity_001";
        doc["alert_type"] = "sensor_health";
        doc["status"] = "degraded";
        doc["consecutive_errors"] = consecutiveErrors;
        doc["timestamp"] = millis();
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/conductivity/alerts", buffer);
    } else {
        sensorHealthy = true;
    }
}

void handleAlarms(const MeasurementData& data) {
    if (data.conductivity_us_cm < config.alarm_threshold_low) {
        Serial.println("ALARM: Low conductivity detected!");
        
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "conductivity_001";
        doc["alert_type"] = "low_conductivity";
        doc["value"] = data.conductivity_us_cm;
        doc["threshold"] = config.alarm_threshold_low;
        doc["timestamp"] = data.timestamp;
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/conductivity/alerts", buffer);
    }
    
    if (data.conductivity_us_cm > config.alarm_threshold_high) {
        Serial.println("ALARM: High conductivity detected!");
        
        DynamicJsonDocument doc(256);
        doc["sensor_id"] = "conductivity_001";
        doc["alert_type"] = "high_conductivity";
        doc["value"] = data.conductivity_us_cm;
        doc["threshold"] = config.alarm_threshold_high;
        doc["timestamp"] = data.timestamp;
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("aquatic/conductivity/alerts", buffer);
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

float readConductivityVoltage() {
    int rawValue = 0;
    
    // Take multiple readings for stability
    for (int i = 0; i < 20; i++) {
        rawValue += analogRead(CONDUCTIVITY_PIN);
        delay(10);
    }
    
    rawValue /= 20;
    
    // Convert to voltage (0-3.3V range)
    return (rawValue * 3.3) / 4095.0;
}

float voltageToResistance(float voltage) {
    // Convert voltage to resistance using voltage divider
    // Assuming 10kΩ reference resistor
    float reference_resistance = 10000.0;
    float supply_voltage = 3.3;
    
    if (voltage <= 0) return 0;
    if (voltage >= supply_voltage) return 1000000; // Very high resistance
    
    return reference_resistance * voltage / (supply_voltage - voltage);
}

float resistanceToConductivity(float resistance) {
    // Convert resistance to conductivity using cell constant
    if (resistance <= 0) return 0;
    
    float conductance = 1.0 / resistance;
    return conductance * config.cell_constant * 1000000; // Convert to μS/cm
}

int calculateQualityScore(const MeasurementData& data) {
    int score = 100;
    
    // Reduce score for extreme values
    if (data.conductivity_us_cm < 50 || data.conductivity_us_cm > 100000) score -= 20;
    if (data.temperature < 10 || data.temperature > 30) score -= 10;
    
    // Reduce score if not calibrated
    if (!calibration.calibrated) score -= 30;
    
    // Reduce score based on age of calibration
    unsigned long calibration_age = millis() - calibration.last_calibration;
    if (calibration_age > 604800000) score -= 20; // 1 week
    
    return max(0, score);
}
