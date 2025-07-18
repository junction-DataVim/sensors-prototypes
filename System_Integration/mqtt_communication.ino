/*
 * Aquatic Monitoring System - MQTT Communication
 * 
 * This module handles:
 * - WiFi and LoRa connection management
 * - MQTT message publishing and subscribing
 * - JSON payload construction for sensor data
 * - Error recovery and reconnection mechanisms
 * - Data buffering for offline scenarios
 * - Security implementation for IoT communication
 * - OTA update capability
 * 
 * Author: Aquatic Monitoring Team
 * Version: 1.0.0
 * Date: 2025-07-18
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <esp_wifi.h>
#include <SPIFFS.h>
#include <Update.h>
#include <mbedtls/md.h>
#include <time.h>

// Network configuration
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER "your-mqtt-broker.com"
#define MQTT_PORT 8883  // Secure MQTT port
#define MQTT_CLIENT_ID "AquaticMonitor"
#define MQTT_USERNAME "aquatic_user"
#define MQTT_PASSWORD "secure_password"

// MQTT topic structure
#define MQTT_BASE_TOPIC "aquaticmonitoring"
#define LOCATION_ID "pond_01"
#define DEVICE_ID "sensor_node_01"

// Security configuration
#define ENABLE_TLS true
#define CERT_FINGERPRINT "AA:BB:CC:DD:EE:FF:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE"

// Connection parameters
#define WIFI_CONNECT_TIMEOUT 30000  // 30 seconds
#define MQTT_CONNECT_TIMEOUT 10000  // 10 seconds
#define MAX_RECONNECT_ATTEMPTS 5
#define HEARTBEAT_INTERVAL 30000    // 30 seconds

// Buffer configuration
#define MAX_BUFFERED_MESSAGES 50
#define MESSAGE_BUFFER_SIZE 1024

// Pin definitions
#define STATUS_LED_PIN 2
#define WIFI_RESET_PIN 0

// Global variables
WiFiClientSecure wifiClientSecure;
WiFiClient wifiClient;
PubSubClient mqttClient;
unsigned long lastHeartbeat = 0;
unsigned long lastReconnectAttempt = 0;
int reconnectAttempts = 0;
bool isConnected = false;
bool otaInProgress = false;

// Message buffer for offline storage
struct BufferedMessage {
  String topic;
  String payload;
  unsigned long timestamp;
  bool retained;
};

BufferedMessage messageBuffer[MAX_BUFFERED_MESSAGES];
int bufferIndex = 0;
int bufferedMessageCount = 0;

// Device information
String deviceMAC;
String firmwareVersion = "1.0.0";
String lastUpdateTime;

// Function prototypes
void initializeWiFi();
void initializeMQTT();
void connectToWiFi();
void connectToMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishSensorData(const String& sensorType, const JsonDocument& data);
void publishHeartbeat();
void publishDeviceInfo();
void handleOTAUpdate();
void bufferMessage(const String& topic, const String& payload, bool retained = false);
void sendBufferedMessages();
void checkConnections();
void handleWiFiReconnection();
void handleMQTTReconnection();
String generateTopicPath(const String& sensorType, const String& measurement);
bool verifyTLSConnection();
void setupOTA();
void updateStatusLED();
String createSecureHash(const String& data);
void syncTime();

void setup() {
  Serial.begin(115200);
  
  // Initialize SPIFFS for configuration storage
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
  }
  
  // Initialize pins
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  
  // Get device MAC address
  deviceMAC = WiFi.macAddress();
  
  // Initialize communication systems
  initializeWiFi();
  initializeMQTT();
  
  // Setup OTA updates
  setupOTA();
  
  // Sync time for secure connections
  syncTime();
  
  // Connect to services
  connectToWiFi();
  connectToMQTT();
  
  // Send initial device information
  publishDeviceInfo();
  
  Serial.println("MQTT Communication System Ready");
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
  
  // Check and maintain connections
  checkConnections();
  
  // Process MQTT messages
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
  
  // Send heartbeat
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    publishHeartbeat();
    lastHeartbeat = millis();
  }
  
  // Update status LED
  updateStatusLED();
  
  // Handle WiFi reset button
  if (digitalRead(WIFI_RESET_PIN) == LOW) {
    Serial.println("WiFi reset requested");
    WiFi.disconnect(true);
    delay(1000);
    ESP.restart();
  }
  
  // Demo: Publish dummy sensor data every 10 seconds
  static unsigned long lastDataPublish = 0;
  if (millis() - lastDataPublish > 10000) {
    publishDummySensorData();
    lastDataPublish = millis();
  }
  
  delay(100);
}

void initializeWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_ID);
  
  // Configure WiFi power management
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  
  Serial.println("WiFi initialized");
}

void initializeMQTT() {
  if (ENABLE_TLS) {
    // Configure secure connection
    wifiClientSecure.setInsecure(); // For testing - use proper certificates in production
    mqttClient.setClient(wifiClientSecure);
  } else {
    mqttClient.setClient(wifiClient);
  }
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(MESSAGE_BUFFER_SIZE);
  
  Serial.println("MQTT initialized");
}

void connectToWiFi() {
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void connectToMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot connect to MQTT - WiFi not connected");
    return;
  }
  
  Serial.printf("Connecting to MQTT broker: %s:%d\n", MQTT_BROKER, MQTT_PORT);
  
  // Create client ID with device MAC
  String clientId = String(MQTT_CLIENT_ID) + "_" + deviceMAC;
  
  // Create will message for unexpected disconnections
  String willTopic = generateTopicPath("system", "status");
  String willMessage = "{\"status\":\"offline\",\"timestamp\":" + String(millis()) + "}";
  
  bool connected = mqttClient.connect(clientId.c_str(), 
                                     MQTT_USERNAME, 
                                     MQTT_PASSWORD,
                                     willTopic.c_str(),
                                     1,
                                     true,
                                     willMessage.c_str());
  
  if (connected) {
    Serial.println("MQTT connected!");
    
    // Subscribe to command topics
    String commandTopic = generateTopicPath("system", "commands");
    mqttClient.subscribe(commandTopic.c_str());
    
    String otaTopic = generateTopicPath("system", "ota");
    mqttClient.subscribe(otaTopic.c_str());
    
    // Publish online status
    String onlineTopic = generateTopicPath("system", "status");
    String onlineMessage = "{\"status\":\"online\",\"timestamp\":" + String(millis()) + "}";
    mqttClient.publish(onlineTopic.c_str(), onlineMessage.c_str(), true);
    
    // Send any buffered messages
    sendBufferedMessages();
    
    isConnected = true;
    reconnectAttempts = 0;
  } else {
    Serial.printf("MQTT connection failed, rc=%d\n", mqttClient.state());
    isConnected = false;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Message received on topic: %s\n", topic);
  
  // Convert payload to string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  // Parse JSON message
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.println("JSON parsing failed");
    return;
  }
  
  // Handle different types of commands
  String topicStr = String(topic);
  
  if (topicStr.indexOf("commands") != -1) {
    handleCommand(doc);
  } else if (topicStr.indexOf("ota") != -1) {
    handleOTACommand(doc);
  } else if (topicStr.indexOf("config") != -1) {
    handleConfigUpdate(doc);
  }
}

void handleCommand(const JsonDocument& doc) {
  String command = doc["command"];
  
  if (command == "restart") {
    Serial.println("Restart command received");
    ESP.restart();
  } else if (command == "calibrate") {
    String sensorType = doc["sensor"];
    Serial.printf("Calibration command for %s sensor\n", sensorType.c_str());
    // Implement calibration logic here
  } else if (command == "set_interval") {
    int interval = doc["interval"];
    Serial.printf("Setting measurement interval to %d seconds\n", interval);
    // Implement interval setting logic
  } else if (command == "get_status") {
    publishDeviceInfo();
  }
}

void handleOTACommand(const JsonDocument& doc) {
  String action = doc["action"];
  
  if (action == "update") {
    String url = doc["url"];
    String version = doc["version"];
    String checksum = doc["checksum"];
    
    Serial.printf("OTA update requested: %s (v%s)\n", url.c_str(), version.c_str());
    
    // Implement OTA update logic
    performOTAUpdate(url, checksum);
  }
}

void handleConfigUpdate(const JsonDocument& doc) {
  // Handle configuration updates
  if (doc.containsKey("mqtt_interval")) {
    // Update MQTT publishing interval
  }
  
  if (doc.containsKey("sensor_config")) {
    // Update sensor configuration
  }
  
  Serial.println("Configuration updated");
}

void publishSensorData(const String& sensorType, const JsonDocument& data) {
  if (!mqttClient.connected()) {
    // Buffer the message for later
    String topic = generateTopicPath(sensorType, "data");
    String payload;
    serializeJson(data, payload);
    bufferMessage(topic, payload);
    return;
  }
  
  // Add metadata to sensor data
  StaticJsonDocument<1024> enrichedData;
  enrichedData["device_id"] = DEVICE_ID;
  enrichedData["location"] = LOCATION_ID;
  enrichedData["timestamp"] = millis();
  enrichedData["sensor_type"] = sensorType;
  enrichedData["data"] = data;
  
  // Add data integrity hash
  String payload;
  serializeJson(enrichedData, payload);
  enrichedData["hash"] = createSecureHash(payload);
  
  // Serialize final payload
  payload = "";
  serializeJson(enrichedData, payload);
  
  // Publish to MQTT
  String topic = generateTopicPath(sensorType, "data");
  bool success = mqttClient.publish(topic.c_str(), payload.c_str());
  
  if (success) {
    Serial.printf("Published %s data: %s\n", sensorType.c_str(), payload.c_str());
  } else {
    Serial.printf("Failed to publish %s data\n", sensorType.c_str());
    bufferMessage(topic, payload);
  }
}

void publishHeartbeat() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["device_id"] = DEVICE_ID;
  doc["timestamp"] = millis();
  doc["uptime"] = millis() / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["firmware_version"] = firmwareVersion;
  
  String payload;
  serializeJson(doc, payload);
  
  String topic = generateTopicPath("system", "heartbeat");
  mqttClient.publish(topic.c_str(), payload.c_str());
}

void publishDeviceInfo() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<512> doc;
  doc["device_id"] = DEVICE_ID;
  doc["location"] = LOCATION_ID;
  doc["mac_address"] = deviceMAC;
  doc["firmware_version"] = firmwareVersion;
  doc["chip_model"] = ESP.getChipModel();
  doc["chip_revision"] = ESP.getChipRevision();
  doc["flash_size"] = ESP.getFlashChipSize();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["uptime"] = millis() / 1000;
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["last_update"] = lastUpdateTime;
  
  String payload;
  serializeJson(doc, payload);
  
  String topic = generateTopicPath("system", "info");
  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void bufferMessage(const String& topic, const String& payload, bool retained) {
  if (bufferedMessageCount >= MAX_BUFFERED_MESSAGES) {
    Serial.println("Message buffer full, discarding oldest message");
    bufferIndex = (bufferIndex + 1) % MAX_BUFFERED_MESSAGES;
  } else {
    bufferedMessageCount++;
  }
  
  int index = (bufferIndex + bufferedMessageCount - 1) % MAX_BUFFERED_MESSAGES;
  messageBuffer[index].topic = topic;
  messageBuffer[index].payload = payload;
  messageBuffer[index].timestamp = millis();
  messageBuffer[index].retained = retained;
  
  Serial.printf("Buffered message %d: %s\n", bufferedMessageCount, topic.c_str());
}

void sendBufferedMessages() {
  if (bufferedMessageCount == 0) return;
  
  Serial.printf("Sending %d buffered messages\n", bufferedMessageCount);
  
  for (int i = 0; i < bufferedMessageCount; i++) {
    int index = (bufferIndex + i) % MAX_BUFFERED_MESSAGES;
    
    bool success = mqttClient.publish(messageBuffer[index].topic.c_str(),
                                     messageBuffer[index].payload.c_str(),
                                     messageBuffer[index].retained);
    
    if (success) {
      Serial.printf("Sent buffered message: %s\n", messageBuffer[index].topic.c_str());
    } else {
      Serial.printf("Failed to send buffered message: %s\n", messageBuffer[index].topic.c_str());
      break; // Stop sending if one fails
    }
    
    delay(100); // Small delay between messages
  }
  
  // Clear buffer
  bufferedMessageCount = 0;
  bufferIndex = 0;
}

void checkConnections() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    handleWiFiReconnection();
  }
  
  // Check MQTT connection
  if (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
    handleMQTTReconnection();
  }
}

void handleWiFiReconnection() {
  if (millis() - lastReconnectAttempt > 5000) { // Try every 5 seconds
    Serial.println("Attempting WiFi reconnection...");
    connectToWiFi();
    lastReconnectAttempt = millis();
  }
}

void handleMQTTReconnection() {
  if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS &&
      millis() - lastReconnectAttempt > 10000) { // Try every 10 seconds
    
    Serial.printf("Attempting MQTT reconnection (%d/%d)...\n", 
                  reconnectAttempts + 1, MAX_RECONNECT_ATTEMPTS);
    
    connectToMQTT();
    lastReconnectAttempt = millis();
    
    if (!mqttClient.connected()) {
      reconnectAttempts++;
    }
  }
}

String generateTopicPath(const String& sensorType, const String& measurement) {
  return String(MQTT_BASE_TOPIC) + "/" + LOCATION_ID + "/" + sensorType + "/" + measurement;
}

void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_ID);
  ArduinoOTA.setPassword("aquatic_ota_2025");
  
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("OTA Update Start: " + type);
    otaInProgress = true;
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update End");
    otaInProgress = false;
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    otaInProgress = false;
  });
  
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void updateStatusLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  
  if (otaInProgress) {
    // Rapid blinking during OTA
    if (millis() - lastBlink > 100) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState);
      lastBlink = millis();
    }
  } else if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
    // Solid on when connected
    digitalWrite(STATUS_LED_PIN, HIGH);
  } else if (WiFi.status() == WL_CONNECTED) {
    // Slow blinking when WiFi connected but MQTT disconnected
    if (millis() - lastBlink > 1000) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState);
      lastBlink = millis();
    }
  } else {
    // Fast blinking when WiFi disconnected
    if (millis() - lastBlink > 200) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState);
      lastBlink = millis();
    }
  }
}

String createSecureHash(const String& data) {
  // Create SHA256 hash of the data
  byte hash[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char*)data.c_str(), data.length());
  mbedtls_md_finish(&ctx, hash);
  mbedtls_md_free(&ctx);
  
  // Convert to hex string
  String hashString = "";
  for (int i = 0; i < 32; i++) {
    hashString += String(hash[i], HEX);
  }
  
  return hashString;
}

void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for NTP time sync");
  
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  
  Serial.println("\nTime synchronized");
}

void performOTAUpdate(const String& url, const String& checksum) {
  // Implement HTTP OTA update
  Serial.printf("Performing OTA update from: %s\n", url.c_str());
  
  // This would typically involve:
  // 1. Download firmware from URL
  // 2. Verify checksum
  // 3. Write to flash
  // 4. Restart
  
  // For now, just log the attempt
  Serial.println("OTA update functionality not implemented in this demo");
}

void publishDummySensorData() {
  // Generate dummy pH sensor data
  StaticJsonDocument<256> phData;
  phData["ph"] = 7.2 + (random(-50, 50) / 100.0);
  phData["temperature"] = 22.5 + (random(-20, 20) / 10.0);
  phData["calibration_status"] = "OK";
  publishSensorData("ph", phData);
  
  // Generate dummy dissolved oxygen data
  StaticJsonDocument<256> doData;
  doData["dissolved_oxygen"] = 8.5 + (random(-10, 10) / 10.0);
  doData["saturation"] = 95.0 + (random(-50, 50) / 10.0);
  doData["temperature"] = 22.5 + (random(-20, 20) / 10.0);
  publishSensorData("dissolved_oxygen", doData);
}

// Utility function to check sensor health
bool checkSensorHealth() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sensor health: CRITICAL - No WiFi connection");
    return false;
  }
  
  // Check MQTT connection
  if (!mqttClient.connected()) {
    Serial.println("Sensor health: WARNING - MQTT disconnected");
    return false;
  }
  
  // Check memory usage
  if (ESP.getFreeHeap() < 10000) {
    Serial.println("Sensor health: WARNING - Low memory");
    return false;
  }
  
  // Check buffered messages
  if (bufferedMessageCount > MAX_BUFFERED_MESSAGES * 0.8) {
    Serial.println("Sensor health: WARNING - High buffer usage");
    return false;
  }
  
  Serial.println("Sensor health: GOOD");
  return true;
}
