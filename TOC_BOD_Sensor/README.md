# TOC/BOD Sensor Module

## Overview
This module measures Total Organic Carbon (TOC) and Biochemical Oxygen Demand (BOD) using advanced electrochemical and optical sensors. These parameters are crucial for monitoring organic pollution, biological activity, and overall water quality.

## Hardware Requirements
- ESP32 Development Board
- TOC Sensor (UV-Persulfate or High-Temperature Combustion)
- BOD Sensor (Optical or Electrochemical)
- Temperature compensation sensor
- Sample preparation system
- Data logging capabilities

## Features
- Real-time TOC measurement (mg/L)
- BOD estimation and trending
- Temperature compensation
- Automatic calibration routines
- Data logging with timestamp
- MQTT and REST API integration
- Alert system for high organic loads
- Self-diagnostic capabilities

## Measurement Specifications
- **TOC Range**: 0-1000 mg/L
- **TOC Accuracy**: ±2% of reading or ±0.5 mg/L
- **BOD Range**: 0-500 mg/L
- **BOD Accuracy**: ±5% of reading
- **Response Time**: <5 minutes

## Installation
1. Install sensors in representative sampling location
2. Connect sample preparation system
3. Configure calibration standards
4. Set up data communication
5. Program measurement intervals

## API Integration
- **POST** `/api/toc-bod-readings` - Submit TOC and BOD measurements
- **Payload**: `{timestamp, toc_mgl, bod_mgl, temperature, sensor_health, pool_id}`

## Calibration
- Standard solutions for TOC calibration
- BOD seed samples for biological calibration
- Temperature compensation verification
- Quality control samples

## Applications
- Organic pollution monitoring
- Biological activity assessment
- Treatment efficiency evaluation
- Environmental compliance
