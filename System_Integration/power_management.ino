/*
 * Aquatic Monitoring System - Power Management
 * 
 * This module handles:
 * - Deep sleep implementation for ESP32
 * - Solar charging logic with MPPT
 * - Battery monitoring and protection
 * - Power distribution to sensors
 * - Low power mode transitions
 * 
 * Author: Aquatic Monitoring Team
 * Version: 1.0.0
 * Date: 2025-07-18
 */

#include <WiFi.h>
#include <driver/adc.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// Pin definitions for power management
#define SOLAR_VOLTAGE_PIN 35        // ADC pin for solar panel voltage
#define BATTERY_VOLTAGE_PIN 34      // ADC pin for battery voltage
#define CHARGING_CURRENT_PIN 32     // ADC pin for charging current
#define LOAD_SWITCH_PIN 25          // GPIO pin for load switching
#define CHARGE_ENABLE_PIN 26        // GPIO pin for charge controller enable
#define POWER_LED_PIN 2             // Status LED

// Battery and solar panel specifications
#define BATTERY_MIN_VOLTAGE 3.2     // Minimum safe battery voltage (V)
#define BATTERY_MAX_VOLTAGE 4.2     // Maximum battery voltage (V)
#define BATTERY_CAPACITY_MAH 5000   // Battery capacity in mAh
#define SOLAR_MAX_VOLTAGE 6.0       // Maximum solar panel voltage (V)
#define CHARGING_CURRENT_MAX 1000   // Maximum charging current (mA)

// Power management thresholds
#define LOW_POWER_THRESHOLD 3.4     // Enter low power mode below this voltage
#define CRITICAL_POWER_THRESHOLD 3.2 // Critical shutdown voltage
#define WAKE_UP_THRESHOLD 3.6       // Wake up from low power mode above this voltage

// Sleep durations (in seconds)
#define NORMAL_SLEEP_DURATION 300   // 5 minutes normal operation
#define LOW_POWER_SLEEP_DURATION 900 // 15 minutes in low power mode
#define CRITICAL_SLEEP_DURATION 3600 // 1 hour in critical mode

// Global variables for power management
float batteryVoltage = 0.0;
float solarVoltage = 0.0;
float chargingCurrent = 0.0;
float batteryPercentage = 0.0;
bool isCharging = false;
bool lowPowerMode = false;
uint32_t wakeupCount = 0;

// WiFi and MQTT for power status reporting
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// RTC memory structure for persistent data across deep sleep
RTC_DATA_ATTR struct {
  uint32_t bootCount;
  float lastBatteryVoltage;
  uint32_t totalSleepTime;
  bool powerSaveMode;
} rtcData;

void setup() {
  Serial.begin(115200);
  
  // Initialize power management
  initializePowerManagement();
  
  // Read wake up reason and handle accordingly
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  handleWakeupReason(wakeup_reason);
  
  // Read battery and solar status
  readPowerStatus();
  
  // Check if we need to enter low power mode
  if (batteryVoltage < LOW_POWER_THRESHOLD) {
    enterLowPowerMode();
  }
  
  // Initialize WiFi and MQTT if power allows
  if (batteryVoltage > CRITICAL_POWER_THRESHOLD) {
    initializeWiFi();
    initializeMQTT();
    
    // Send power status
    sendPowerStatus();
  }
  
  // Manage charging if solar power is available
  manageCharging();
  
  // Determine sleep duration based on power status
  uint32_t sleepDuration = calculateSleepDuration();
  
  // Prepare for deep sleep
  prepareForSleep(sleepDuration);
}

void loop() {
  // This should never be reached as we enter deep sleep in setup()
  delay(1000);
}

void initializePowerManagement() {
  // Configure GPIO pins
  pinMode(LOAD_SWITCH_PIN, OUTPUT);
  pinMode(CHARGE_ENABLE_PIN, OUTPUT);
  pinMode(POWER_LED_PIN, OUTPUT);
  
  // Configure ADC for battery monitoring
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11); // GPIO35
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); // GPIO34
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11); // GPIO32
  
  // Increment boot count
  rtcData.bootCount++;
  
  // Initialize power status LED
  digitalWrite(POWER_LED_PIN, HIGH);
  
  Serial.println("Power Management System Initialized");
  Serial.printf("Boot count: %d\n", rtcData.bootCount);
}

void handleWakeupReason(esp_sleep_wakeup_cause_t wakeup_reason) {
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.println("Wakeup was not caused by deep sleep");
      break;
  }
}

void readPowerStatus() {
  // Read battery voltage
  int batteryRaw = adc1_get_raw(ADC1_CHANNEL_6);
  batteryVoltage = (batteryRaw * 3.3 / 4095.0) * 2.0; // Voltage divider compensation
  
  // Read solar voltage
  int solarRaw = adc1_get_raw(ADC1_CHANNEL_7);
  solarVoltage = (solarRaw * 3.3 / 4095.0) * 2.5; // Voltage divider compensation
  
  // Read charging current
  int currentRaw = adc1_get_raw(ADC1_CHANNEL_4);
  chargingCurrent = (currentRaw * 3.3 / 4095.0) * 1000; // Convert to mA
  
  // Calculate battery percentage
  batteryPercentage = calculateBatteryPercentage(batteryVoltage);
  
  // Determine if charging
  isCharging = (solarVoltage > batteryVoltage + 0.5) && (chargingCurrent > 50);
  
  // Print power status
  Serial.printf("Battery: %.2fV (%.1f%%)\n", batteryVoltage, batteryPercentage);
  Serial.printf("Solar: %.2fV\n", solarVoltage);
  Serial.printf("Charging: %s (%.0fmA)\n", isCharging ? "Yes" : "No", chargingCurrent);
}

float calculateBatteryPercentage(float voltage) {
  // Li-ion battery discharge curve approximation
  if (voltage >= 4.2) return 100.0;
  if (voltage >= 4.0) return 80.0 + (voltage - 4.0) * 100.0;
  if (voltage >= 3.8) return 60.0 + (voltage - 3.8) * 100.0;
  if (voltage >= 3.6) return 40.0 + (voltage - 3.6) * 100.0;
  if (voltage >= 3.4) return 20.0 + (voltage - 3.4) * 100.0;
  if (voltage >= 3.2) return 5.0 + (voltage - 3.2) * 75.0;
  return 0.0;
}

void manageCharging() {
  if (solarVoltage > batteryVoltage + 0.5 && batteryVoltage < BATTERY_MAX_VOLTAGE) {
    // Enable charging
    digitalWrite(CHARGE_ENABLE_PIN, HIGH);
    Serial.println("Charging enabled");
    
    // MPPT algorithm - simple implementation
    static float lastPower = 0.0;
    float currentPower = solarVoltage * chargingCurrent;
    
    if (currentPower > lastPower) {
      // Increase charging current slightly
      // This would control a PWM signal to the charge controller
    } else {
      // Decrease charging current slightly
      // This would control a PWM signal to the charge controller
    }
    
    lastPower = currentPower;
  } else {
    // Disable charging
    digitalWrite(CHARGE_ENABLE_PIN, LOW);
    Serial.println("Charging disabled");
  }
}

void enterLowPowerMode() {
  Serial.println("Entering low power mode");
  
  // Disable non-essential systems
  digitalWrite(LOAD_SWITCH_PIN, LOW);
  
  // Set low power mode flag
  lowPowerMode = true;
  rtcData.powerSaveMode = true;
  
  // Reduce CPU frequency
  setCpuFrequencyMhz(80);
  
  // Status LED pattern for low power mode
  for (int i = 0; i < 3; i++) {
    digitalWrite(POWER_LED_PIN, LOW);
    delay(100);
    digitalWrite(POWER_LED_PIN, HIGH);
    delay(100);
  }
}

uint32_t calculateSleepDuration() {
  if (batteryVoltage < CRITICAL_POWER_THRESHOLD) {
    Serial.println("Critical power - extended sleep");
    return CRITICAL_SLEEP_DURATION;
  } else if (batteryVoltage < LOW_POWER_THRESHOLD) {
    Serial.println("Low power - extended sleep");
    return LOW_POWER_SLEEP_DURATION;
  } else {
    Serial.println("Normal power - regular sleep");
    return NORMAL_SLEEP_DURATION;
  }
}

void initializeWiFi() {
  if (batteryVoltage < LOW_POWER_THRESHOLD) {
    Serial.println("Skipping WiFi due to low power");
    return;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi connection failed");
  }
}

void initializeMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  mqttClient.setServer("your-mqtt-broker.com", 1883);
  
  int attempts = 0;
  while (!mqttClient.connected() && attempts < 3) {
    Serial.print("Attempting MQTT connection...");
    
    if (mqttClient.connect("AquaticPowerManager")) {
      Serial.println("connected");
    } else {
      Serial.printf("failed, rc=%d\n", mqttClient.state());
      delay(2000);
      attempts++;
    }
  }
}

void sendPowerStatus() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<400> doc;
  doc["device_id"] = "aquatic_power_manager";
  doc["timestamp"] = millis();
  doc["battery_voltage"] = batteryVoltage;
  doc["battery_percentage"] = batteryPercentage;
  doc["solar_voltage"] = solarVoltage;
  doc["charging_current"] = chargingCurrent;
  doc["is_charging"] = isCharging;
  doc["low_power_mode"] = lowPowerMode;
  doc["boot_count"] = rtcData.bootCount;
  
  // Add power consumption estimate
  doc["estimated_runtime_hours"] = estimateRuntimeHours();
  
  char jsonBuffer[500];
  serializeJson(doc, jsonBuffer);
  
  mqttClient.publish("aquaticmonitoring/power/status", jsonBuffer);
  Serial.println("Power status sent via MQTT");
}

float estimateRuntimeHours() {
  // Simple runtime estimation based on current battery level and consumption
  float currentConsumption = 150; // mA average consumption
  float remainingCapacity = (batteryPercentage / 100.0) * BATTERY_CAPACITY_MAH;
  
  if (isCharging) {
    // Factor in charging current
    float netConsumption = currentConsumption - chargingCurrent;
    if (netConsumption <= 0) return 999.0; // Indefinite runtime
    remainingCapacity += chargingCurrent * 8; // Assume 8 hours of daylight
  }
  
  return remainingCapacity / currentConsumption;
}

void prepareForSleep(uint32_t sleepDuration) {
  Serial.printf("Preparing for sleep: %d seconds\n", sleepDuration);
  
  // Disconnect WiFi to save power
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  // Disable Bluetooth
  esp_bt_controller_disable();
  
  // Configure timer wakeup
  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  
  // Configure external wakeup if needed (e.g., for emergency conditions)
  // esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 0);
  
  // Save current state to RTC memory
  rtcData.lastBatteryVoltage = batteryVoltage;
  rtcData.totalSleepTime += sleepDuration;
  
  // Turn off status LED
  digitalWrite(POWER_LED_PIN, LOW);
  
  // Final power status
  Serial.println("Entering deep sleep...");
  Serial.flush();
  
  // Enter deep sleep
  esp_deep_sleep_start();
}

// Utility function to check sensor health
bool checkSensorHealth() {
  // Check if power levels are sufficient for sensor operation
  if (batteryVoltage < CRITICAL_POWER_THRESHOLD) {
    Serial.println("Sensor health: CRITICAL - Low power");
    return false;
  }
  
  if (batteryVoltage < LOW_POWER_THRESHOLD) {
    Serial.println("Sensor health: WARNING - Low power");
    return false;
  }
  
  // Check if charging system is working
  if (solarVoltage > 2.0 && !isCharging && batteryVoltage < BATTERY_MAX_VOLTAGE) {
    Serial.println("Sensor health: WARNING - Charging system issue");
    return false;
  }
  
  Serial.println("Sensor health: GOOD");
  return true;
}

// Function to generate dummy power data for testing
void generateDummyPowerData() {
  // Simulate battery discharge
  static float simulatedBattery = 4.0;
  static bool simulatedCharging = false;
  
  // Simulate solar charging during day hours
  int currentHour = (millis() / 3600000) % 24;
  if (currentHour >= 6 && currentHour <= 18) {
    solarVoltage = 5.5 + random(-100, 100) / 1000.0;
    if (simulatedBattery < 4.2) {
      simulatedCharging = true;
      simulatedBattery += 0.01;
    }
  } else {
    solarVoltage = 0.1;
    simulatedCharging = false;
    simulatedBattery -= 0.005;
  }
  
  batteryVoltage = simulatedBattery;
  isCharging = simulatedCharging;
  chargingCurrent = isCharging ? 500 : 0;
  batteryPercentage = calculateBatteryPercentage(batteryVoltage);
  
  Serial.println("Using dummy power data for testing");
}
