# Water Level and Flow Sensor Calibration Guide

## Overview
This guide provides comprehensive instructions for calibrating both water level and flow sensors to ensure accurate measurements and reliable system operation.

## Required Equipment

### For Level Calibration
- Measuring tape or ruler (metric)
- Water-filled calibration tank
- Reference level markers
- Stable mounting platform

### For Flow Calibration
- Calibrated flow meter (reference)
- Graduated measuring cylinder (1-10L)
- Stopwatch
- Constant flow source
- Flow control valve

## Pre-Calibration Setup

### Level Sensor Preparation
1. Mount ultrasonic sensor at fixed height above water surface
2. Ensure sensor is perfectly vertical (use spirit level)
3. Remove any obstacles in sensor beam path
4. Measure exact distance from sensor to tank bottom
5. Record ambient temperature and humidity

### Flow Sensor Preparation
1. Install flow sensor in straight pipe section
2. Ensure 10× pipe diameter upstream/downstream clearance
3. Remove air bubbles from system
4. Verify sensor wiring and connections
5. Check for pipe leaks or restrictions

## Level Sensor Calibration

### Step 1: Zero Point Calibration
1. Empty tank completely
2. Measure exact distance from sensor to tank bottom
3. Record this as `SENSOR_HEIGHT` in code
4. Verify reading shows maximum distance

### Step 2: Multi-Point Calibration
1. Fill tank to 25% capacity
   - Measure actual water level with ruler
   - Record sensor reading
   - Compare and note difference
2. Repeat for 50%, 75%, and 100% capacity
3. Calculate average error across all points
4. Adjust `SOUND_SPEED` constant if needed

### Step 3: Temperature Compensation
1. Measure at different temperatures (15°C, 25°C, 35°C)
2. Note speed of sound variation
3. Implement temperature correction:
   ```
   adjusted_speed = 0.034 * (1 + 0.00174 * (temp - 20))
   ```

### Level Calibration Data Sheet
| Fill Level | Actual (cm) | Sensor (cm) | Error (cm) | Error (%) |
|------------|-------------|-------------|------------|-----------|
| 25%        |             |             |            |           |
| 50%        |             |             |            |           |
| 75%        |             |             |            |           |
| 100%       |             |             |            |           |

## Flow Sensor Calibration

### Step 1: Pulse Count Verification
1. Connect flow sensor to oscilloscope
2. Pass known volume through sensor
3. Count pulses manually
4. Calculate pulses per liter
5. Compare with manufacturer specification

### Step 2: Low Flow Calibration (1-5 L/min)
1. Set up controlled flow at 1 L/min
2. Measure actual flow with graduated cylinder
3. Record sensor pulse count over 60 seconds
4. Calculate calibration factor:
   ```
   factor = pulses_per_minute / actual_flow_rate
   ```
5. Repeat for 2, 3, 4, and 5 L/min

### Step 3: High Flow Calibration (10-25 L/min)
1. Use larger flow rates with reference meter
2. Record pulse counts over 30-second intervals
3. Calculate average calibration factor
4. Check for linearity across range

### Step 4: Bidirectional Flow Testing
1. Test reverse flow if applicable
2. Verify pulse counting works in both directions
3. Check for flow direction sensitivity

### Flow Calibration Data Sheet
| Flow Rate (L/min) | Pulses/min | Actual (L/min) | Factor | Error (%) |
|-------------------|------------|----------------|---------|-----------|
| 1.0               |            |                |         |           |
| 2.0               |            |                |         |           |
| 5.0               |            |                |         |           |
| 10.0              |            |                |         |           |
| 20.0              |            |                |         |           |

## System Integration Calibration

### Step 1: Combined Operation Test
1. Fill tank to known level
2. Start pump and measure flow
3. Monitor level change over time
4. Verify volume balance:
   ```
   level_change × tank_area = flow_rate × time
   ```

### Step 2: Leak Detection Calibration
1. Fill tank to 75% capacity
2. Turn off all pumps and inflows
3. Monitor level over 24 hours
4. Calculate natural evaporation rate
5. Set leak detection threshold above evaporation

### Step 3: Pump Control Calibration
1. Set low level alarm at 25% capacity
2. Set high level alarm at 90% capacity
3. Test automatic pump start/stop
4. Verify hysteresis prevents rapid cycling
5. Test manual override functions

## Validation Procedures

### Accuracy Verification
- Level accuracy: ±1 cm or ±2% of reading
- Flow accuracy: ±3% of reading
- Response time: <2 seconds for both sensors

### Repeatability Testing
1. Perform 10 consecutive measurements
2. Calculate standard deviation
3. Verify repeatability within ±0.5% of range

### Long-term Stability
1. Monitor sensors over 1 week
2. Check for drift or degradation
3. Verify calibration remains stable

## Environmental Considerations

### Temperature Effects
- Ultrasonic: Speed of sound varies with temperature
- Flow sensor: Viscosity changes affect calibration
- Compensation: Use temperature sensor for corrections

### Humidity Effects
- High humidity can affect ultrasonic propagation
- Condensation on sensors can cause errors
- Use humidity compensation if needed

### Installation Effects
- Pipe roughness affects flow measurement
- Tank shape affects level-to-volume conversion
- Mounting vibration can cause noise

## Troubleshooting

### Level Sensor Issues
| Problem | Cause | Solution |
|---------|-------|----------|
| Erratic readings | Obstacles in beam path | Clear obstacles, check mounting |
| No reading | Sensor failure | Check wiring, replace sensor |
| Offset error | Wrong zero point | Recalibrate zero point |
| Temperature drift | No compensation | Implement temperature correction |

### Flow Sensor Issues
| Problem | Cause | Solution |
|---------|-------|----------|
| No pulses | Sensor failure | Check power, replace sensor |
| Low readings | Partial blockage | Clean sensor, check installation |
| High readings | Air in system | Remove air bubbles |
| Inconsistent | Turbulent flow | Improve pipe installation |

## Maintenance Schedule

### Weekly
- Visual inspection of sensors
- Check for debris or fouling
- Verify connections are secure

### Monthly
- Single-point verification
- Clean sensor surfaces
- Check calibration with known values

### Quarterly
- Full recalibration
- Inspect mounting hardware
- Update calibration records

### Annually
- Replace sensors if needed
- Professional calibration service
- System performance review

## Quality Assurance

### Documentation Requirements
- Calibration certificates
- Measurement uncertainty analysis
- Traceability to national standards
- Operator training records

### Audit Trail
- Date and time of calibration
- Operator identification
- Environmental conditions
- Before/after readings
- Adjustments made

## Advanced Calibration

### Multi-Point Curve Fitting
For improved accuracy, use polynomial fitting:
```
corrected_value = a₀ + a₁×x + a₂×x² + a₃×x³
```

### Statistical Analysis
- Calculate measurement uncertainty
- Determine confidence intervals
- Perform regression analysis
- Validate calibration model

### Automated Calibration
- Use programmable flow/level sources
- Implement automatic calibration routines
- Remote calibration verification
- Scheduled calibration reminders

## Safety Considerations
- Use proper electrical safety procedures
- Ensure waterproof connections
- Test emergency shutdown systems
- Follow lockout/tagout procedures
- Maintain safety equipment calibration
