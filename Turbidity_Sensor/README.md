# Turbidity Sensor Module

## Overview
This module measures water turbidity (clarity) using a TSS (Total Suspended Solids) sensor. Turbidity is a critical parameter for monitoring water quality, fish health, and system efficiency.

## Hardware Requirements
- ESP32 Development Board
- TSS Sensor (e.g., TS-300B or similar)
- Analog output sensor for turbidity measurement
- Waterproof housing
- Power management circuit

## Features
- Real-time turbidity measurement in NTU (Nephelometric Turbidity Units)
- Multi-point calibration with temperature compensation
- Automatic cleaning cycle detection
- Data logging with timestamp
- MQTT and REST API integration
- Alert system for high turbidity levels
- Self-diagnostic capabilities

## Measurement Range
- Turbidity: 0-3000 NTU
- Resolution: 0.1 NTU
- Accuracy: ±5% of reading

## Installation
1. Connect sensor to ESP32 analog input
2. Configure power management
3. Set up waterproof housing
4. Calibrate with standard solutions
5. Configure MQTT and API endpoints

## API Integration
- **POST** `/api/turbidity-readings` - Submit turbidity measurements
- **Payload**: `{timestamp, turbidity_ntu, temperature, sensor_health, pool_id}`

## Calibration
Regular calibration required using formazin standards:
- 0 NTU (distilled water)
- 100 NTU standard
- 1000 NTU standard
- 4000 NTU standard

## Maintenance
- Weekly sensor cleaning
- Monthly calibration verification
- Quarterly full recalibration
- Annual sensor replacement (recommended)
