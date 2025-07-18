# Water Level and Flow Sensor Module

## Overview
This module monitors water level and flow rate in aquatic systems using ultrasonic level sensors and flow meters. Critical for maintaining proper water circulation and preventing overflow/underflow conditions.

## Hardware Requirements
- ESP32 Development Board
- HC-SR04 Ultrasonic Distance Sensor (for level measurement)
- YF-S201 Hall Effect Flow Sensor (for flow rate)
- Waterproof housing for level sensor
- Pipe mounting hardware for flow sensor
- Power management circuit

## Features
- Real-time water level monitoring (cm/inches)
- Flow rate measurement (L/min, GPM)
- Volume calculation and totalizer
- Leak detection algorithms
- Pump control integration
- Data logging with timestamp
- MQTT and REST API integration
- Alert system for low/high levels and flow issues
- Self-diagnostic capabilities

## Measurement Specifications
- **Level Range**: 5-400 cm (2-157 inches)
- **Level Accuracy**: ±1 cm (±0.4 inches)
- **Flow Range**: 1-30 L/min (0.26-7.93 GPM)
- **Flow Accuracy**: ±3% of reading
- **Response Time**: <2 seconds

## Installation
1. Mount ultrasonic sensor above water surface
2. Install flow sensor in main circulation line
3. Configure sensor heights and pipe diameter
4. Calibrate level zero point
5. Set up MQTT and API endpoints

## API Integration
- **POST** `/api/water-level-readings` - Submit water level and flow measurements
- **Payload**: `{timestamp, water_level_cm, flow_rate_lpm, total_volume_l, pump_status, sensor_health, pool_id}`

## Configuration
- Tank dimensions and shape
- Pipe diameter and flow coefficient
- Alarm thresholds
- Pump control parameters
- Data logging intervals

## Maintenance
- Monthly sensor cleaning
- Quarterly calibration check
- Annual sensor replacement
- Flow sensor filter cleaning
