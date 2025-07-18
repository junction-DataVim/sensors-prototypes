# Aquatic Monitoring System - Sensor Libraries

This directory contains all required libraries and dependencies for the Aquatic Monitoring System sensors.

## Library Installation

### Arduino Libraries (ESP32)

#### Core Libraries
```bash
# Install via Arduino IDE Library Manager or PlatformIO
- ArduinoJson (v6.21.3)
- PubSubClient (v2.8.0)
- WiFi (v1.0.0)
- ArduinoOTA (v1.0.0)
- EEPROM (v1.0.0)
- Wire (v1.0.0)
- SPI (v1.0.0)
```

#### Sensor-Specific Libraries
```bash
# pH Sensor
- DFRobot_PH (v1.0.0)
- OneWire (v2.3.7)
- DallasTemperature (v3.11.0)

# Dissolved Oxygen
- DFRobot_DO (v1.0.0)
- Atlas Scientific libraries

# Conductivity/TDS
- DFRobot_EC (v1.0.0)

# Ammonia/ISE
- DFRobot_ISE (v1.0.0)

# ADC
- Adafruit_ADS1X15 (v2.4.1)
- Adafruit_CCS811 (v1.1.0)

# Environmental
- Adafruit_Unified_Sensor (v1.1.9)
- DHT_sensor_library (v1.4.4)

# Ultrasonic
- NewPing (v1.9.4)

# Communication
- ESPAsyncWebServer (v1.2.3)
- AsyncTCP (v1.1.1)
- ESP32Time (v2.0.0)
```

### Python Libraries (Raspberry Pi)

#### Computer Vision
```bash
pip install opencv-python==4.8.0.74
pip install numpy==1.24.3
pip install matplotlib==3.7.1
pip install pillow==10.0.0
pip install scikit-learn==1.3.0
pip install scipy==1.11.1
```

#### Camera Libraries
```bash
pip install picamera2==0.3.17
pip install v4l2-python3
```

#### Communication
```bash
pip install paho-mqtt==1.6.1
pip install requests==2.31.0
pip install flask==2.3.2
pip install flask-cors==4.0.0
```

#### Data Processing
```bash
pip install pandas==2.0.3
pip install influxdb-client==1.37.0
pip install psutil==5.9.5
```

#### System Libraries
```bash
pip install RPi.GPIO==0.7.1
pip install pyserial==3.5
pip install schedule==1.2.0
pip install watchdog==3.0.0
```

#### Visualization
```bash
pip install plotly==5.15.0
pip install dash==2.11.2
pip install dash-bootstrap-components==1.4.1
```

#### Security
```bash
pip install cryptography==41.0.3
pip install jsonschema==4.19.0
```

#### Configuration
```bash
pip install pyyaml==6.0.1
pip install python-dotenv==1.0.0
```

## Installation Scripts

### ESP32 Setup Script
```bash
#!/bin/bash
# install_esp32_libraries.sh

echo "Installing ESP32 libraries..."

# Install PlatformIO if not installed
if ! command -v pio &> /dev/null; then
    echo "Installing PlatformIO..."
    curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py
    python3 get-platformio.py
fi

# Install platform and frameworks
pio platform install espressif32
pio platform install native

# Install libraries
pio lib install "arduinojson@^6.21.3"
pio lib install "pubsubclient@^2.8.0"
pio lib install "DFRobot/DFRobot_PH@^1.0.0"
pio lib install "DFRobot/DFRobot_EC@^1.0.0"
pio lib install "DFRobot/DFRobot_ISE@^1.0.0"
pio lib install "milesburton/DallasTemperature@^3.11.0"
pio lib install "OneWire@^2.3.7"
pio lib install "adafruit/Adafruit Unified Sensor@^1.1.9"
pio lib install "adafruit/Adafruit ADS1X15@^2.4.1"
pio lib install "adafruit/Adafruit CCS811 Library@^1.1.0"
pio lib install "NewPing@^1.9.4"
pio lib install "ESPAsyncWebServer@^1.2.3"
pio lib install "AsyncTCP@^1.1.1"
pio lib install "ESP32Time@^2.0.0"
pio lib install "ArduinoOTA@^1.0.0"

echo "ESP32 libraries installed successfully!"
```

### Raspberry Pi Setup Script
```bash
#!/bin/bash
# install_rpi_libraries.sh

echo "Installing Raspberry Pi libraries..."

# Update system
sudo apt update && sudo apt upgrade -y

# Install system dependencies
sudo apt install -y python3-pip python3-venv
sudo apt install -y libopencv-dev python3-opencv
sudo apt install -y libatlas-base-dev gfortran
sudo apt install -y libhdf5-dev libhdf5-serial-dev
sudo apt install -y libqt5gui5 libqt5webkit5 libqt5test5
sudo apt install -y python3-pyqt5

# Install camera libraries
sudo apt install -y python3-picamera2
sudo apt install -y libcamera-apps

# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install Python packages
pip install --upgrade pip
pip install -r requirements.txt

echo "Raspberry Pi libraries installed successfully!"
```

## Library Versions

### Compatibility Matrix
| Library | ESP32 Version | RPi Version | Notes |
|---------|---------------|-------------|-------|
| ArduinoJson | 6.21.3 | N/A | JSON parsing |
| PubSubClient | 2.8.0 | N/A | MQTT client |
| OpenCV | N/A | 4.8.0.74 | Computer vision |
| NumPy | N/A | 1.24.3 | Numerical computing |
| Pandas | N/A | 2.0.3 | Data analysis |
| paho-mqtt | N/A | 1.6.1 | MQTT client |

### Version Control
- All library versions are pinned for reproducibility
- Use semantic versioning for updates
- Test compatibility before upgrading
- Maintain backward compatibility

## Custom Libraries

### Sensor Abstraction Layer
```cpp
// SensorBase.h
#ifndef SENSOR_BASE_H
#define SENSOR_BASE_H

#include <Arduino.h>
#include <ArduinoJson.h>

class SensorBase {
public:
    virtual bool initialize() = 0;
    virtual bool calibrate() = 0;
    virtual bool takeMeasurement() = 0;
    virtual JsonDocument getData() = 0;
    virtual bool checkHealth() = 0;
    virtual String getStatus() = 0;
    
protected:
    bool isInitialized = false;
    bool isCalibrated = false;
    unsigned long lastMeasurement = 0;
    String deviceId;
    String sensorType;
};

#endif
```

### Communication Layer
```cpp
// CommBase.h
#ifndef COMM_BASE_H
#define COMM_BASE_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

class CommBase {
public:
    virtual bool connectWiFi() = 0;
    virtual bool connectMQTT() = 0;
    virtual bool publishData(const String& topic, const JsonDocument& data) = 0;
    virtual bool subscribeToCommands() = 0;
    virtual void handleCommand(const String& command) = 0;
    
protected:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    String ssid;
    String password;
    String mqttBroker;
    String deviceId;
};

#endif
```

### Power Management
```cpp
// PowerManager.h
#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <esp_sleep.h>

class PowerManager {
public:
    static void enterDeepSleep(uint64_t sleepTimeUs);
    static void enterLightSleep(uint64_t sleepTimeUs);
    static float getBatteryVoltage();
    static bool isLowPower();
    static void enableSensor(int pin);
    static void disableSensor(int pin);
    
private:
    static const float LOW_POWER_THRESHOLD;
    static const int BATTERY_PIN;
};

#endif
```

## Usage Examples

### Basic Sensor Implementation
```cpp
#include "SensorBase.h"
#include "CommBase.h"
#include "PowerManager.h"

class pHSensor : public SensorBase {
private:
    float pHValue;
    float temperature;
    
public:
    bool initialize() override {
        // Initialize pH sensor
        Serial.println("Initializing pH sensor...");
        // ... implementation
        isInitialized = true;
        return true;
    }
    
    bool takeMeasurement() override {
        if (!isInitialized) return false;
        
        // Take pH measurement
        pHValue = readPH();
        temperature = readTemperature();
        
        lastMeasurement = millis();
        return true;
    }
    
    JsonDocument getData() override {
        JsonDocument doc;
        doc["pH"] = pHValue;
        doc["temperature"] = temperature;
        doc["timestamp"] = lastMeasurement;
        return doc;
    }
    
    bool checkHealth() override {
        return isInitialized && (millis() - lastMeasurement < 300000);
    }
};
```

### Communication Implementation
```cpp
#include "CommBase.h"

class AquaticComm : public CommBase {
public:
    AquaticComm(const String& deviceId, const String& ssid, 
                const String& password, const String& broker) {
        this->deviceId = deviceId;
        this->ssid = ssid;
        this->password = password;
        this->mqttBroker = broker;
        
        mqttClient.setClient(wifiClient);
        mqttClient.setServer(broker.c_str(), 1883);
        mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            String message = "";
            for (int i = 0; i < length; i++) {
                message += (char)payload[i];
            }
            handleCommand(message);
        });
    }
    
    bool publishData(const String& topic, const JsonDocument& data) override {
        if (!mqttClient.connected()) return false;
        
        String payload;
        serializeJson(data, payload);
        
        return mqttClient.publish(topic.c_str(), payload.c_str());
    }
};
```

## Debugging Tools

### Serial Monitor Helper
```cpp
// Debug.h
#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(x, ...) Serial.printf(x, __VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(x, ...)
#endif

#endif
```

### Memory Monitor
```cpp
// MemoryMonitor.h
#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

class MemoryMonitor {
public:
    static void printMemoryUsage();
    static size_t getFreeHeap();
    static size_t getMinFreeHeap();
    static bool isMemoryLow();
    
private:
    static const size_t LOW_MEMORY_THRESHOLD = 10000;
};

#endif
```

## Documentation

### API Documentation
Each library includes comprehensive API documentation with:
- Function descriptions
- Parameter specifications
- Return value documentation
- Usage examples
- Error handling guidelines

### Integration Guide
Step-by-step instructions for integrating libraries:
1. Install required dependencies
2. Include header files
3. Initialize library instances
4. Configure parameters
5. Implement callbacks
6. Handle errors gracefully

### Best Practices
- Always check return values
- Implement proper error handling
- Use watchdog timers for reliability
- Monitor memory usage
- Validate input parameters
- Log important events

## Support

### Issue Tracking
- GitHub Issues for bug reports
- Feature requests via discussions
- Documentation improvements
- Performance optimization suggestions

### Community Resources
- Wiki with tutorials
- Example projects
- User forums
- Developer chat

### Professional Support
- Commercial support available
- Custom development services
- Training and consulting
- System integration assistance

This sensor library collection provides a robust foundation for the Aquatic Monitoring System, ensuring reliable operation and easy maintenance.
