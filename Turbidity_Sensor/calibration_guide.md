# Turbidity Sensor Calibration Guide

## Overview
This guide provides step-by-step instructions for calibrating the turbidity sensor to ensure accurate measurements across the full range of 0-4000 NTU.

## Required Materials
- Formazin turbidity standards (0, 100, 1000, 4000 NTU)
- Distilled water
- Clean sample containers
- Lint-free cleaning cloths
- Calibration worksheet

## Calibration Standards
### Primary Standards (Formazin)
- **0 NTU**: Ultra-pure distilled water
- **100 NTU**: Formazin standard solution
- **1000 NTU**: Formazin standard solution
- **4000 NTU**: Formazin standard solution

### Secondary Standards (Polymer microspheres)
- More stable than formazin
- Longer shelf life
- Less temperature sensitive

## Pre-Calibration Checklist
1. [ ] Sensor cleaned and dried
2. [ ] Standards at room temperature (20±2°C)
3. [ ] Standards well-mixed (gentle inversion)
4. [ ] Sample containers clean and dry
5. [ ] Sensor powered on for 30+ minutes
6. [ ] WiFi connection established

## Calibration Procedure

### Step 1: Initial Setup
1. Connect sensor to ESP32 development board
2. Open serial monitor (115200 baud)
3. Wait for "Sensor ready" message
4. Type `STATUS` to verify sensor health

### Step 2: Zero Point Calibration (0 NTU)
1. Fill clean container with distilled water
2. Immerse sensor completely (avoid air bubbles)
3. Wait 2 minutes for thermal equilibrium
4. Type `CAL` in serial monitor
5. When prompted for "Standard 1 (0.0 NTU)": press Enter
6. System will average 10 readings and store voltage

### Step 3: Low Range Calibration (100 NTU)
1. Remove sensor and clean with distilled water
2. Gently invert 100 NTU standard 10 times
3. Pour standard into clean container
4. Immerse sensor completely
5. Wait 2 minutes for stabilization
6. When prompted for "Standard 2 (100.0 NTU)": press Enter
7. System will store voltage reading

### Step 4: Mid Range Calibration (1000 NTU)
1. Clean sensor with distilled water
2. Prepare 1000 NTU standard (mix gently)
3. Pour into clean container
4. Immerse sensor completely
5. Wait 2 minutes for stabilization
6. When prompted for "Standard 3 (1000.0 NTU)": press Enter
7. System will store voltage reading

### Step 5: High Range Calibration (4000 NTU)
1. Clean sensor thoroughly
2. Prepare 4000 NTU standard (mix very gently)
3. Pour into clean container
4. Immerse sensor completely
5. Wait 3 minutes for stabilization
6. When prompted for "Standard 4 (4000.0 NTU)": press Enter
7. System will store voltage reading

### Step 6: Verification
1. Type `STATUS` to view calibration points
2. Test with known standards to verify accuracy
3. Expected accuracy: ±5% of reading
4. If accuracy is poor, repeat calibration

## Calibration Validation

### Acceptance Criteria
- **0 NTU**: Reading should be -0.5 to +0.5 NTU
- **100 NTU**: Reading should be 95-105 NTU
- **1000 NTU**: Reading should be 950-1050 NTU
- **4000 NTU**: Reading should be 3800-4200 NTU

### Troubleshooting
| Problem | Cause | Solution |
|---------|-------|----------|
| Negative readings | Sensor dirty | Clean sensor thoroughly |
| Unstable readings | Air bubbles | Ensure complete immersion |
| Poor linearity | Old standards | Use fresh standards |
| High noise | Electrical interference | Check grounding |

## Maintenance Schedule

### Daily
- Visual inspection of sensor
- Check for algae growth or debris

### Weekly
- Clean sensor with soft brush
- Rinse with distilled water
- Check sample verification

### Monthly
- Single-point verification (100 NTU)
- Clean sensor housing
- Check cable connections

### Quarterly
- Full 4-point calibration
- Replace standards if expired
- Document calibration results

### Annually
- Professional calibration service
- Sensor replacement evaluation
- System performance review

## Temperature Compensation
The sensor automatically compensates for temperature variations:
- Reference temperature: 20°C
- Compensation factor: 2%/°C
- Valid range: 0-50°C

## Environmental Considerations
- Avoid direct sunlight during calibration
- Maintain stable temperature (±2°C)
- Minimize vibrations
- Use consistent sample handling

## Record Keeping
Document each calibration with:
- Date and time
- Operator name
- Standard lot numbers
- Calibration results
- Environmental conditions
- Any observations

## Quality Control
- Use certified reference materials
- Perform duplicate measurements
- Maintain calibration traceability
- Regular proficiency testing

## Safety Notes
- Handle standards with care
- Avoid skin contact with formazin
- Dispose of waste properly
- Follow laboratory safety protocols

## Troubleshooting Common Issues

### Calibration Drift
- Check standard expiration dates
- Verify temperature stability
- Inspect sensor for contamination
- Review handling procedures

### Poor Repeatability
- Ensure proper mixing of standards
- Check for air bubbles
- Verify sensor immersion depth
- Minimize vibrations

### Offset Errors
- Verify zero-point calibration
- Check for sensor fouling
- Ensure proper cleaning
- Review calibration procedure

## Advanced Calibration Options

### Custom Calibration Points
For specific applications, additional calibration points can be programmed:
```
// Add custom calibration point
calibration[4] = {voltage_reading, ntu_value};
```

### Linearization Correction
For improved accuracy, polynomial correction can be applied:
```
corrected_ntu = a*ntu^2 + b*ntu + c
```

### Multi-Range Calibration
Different calibration curves for different ranges:
- Low range: 0-100 NTU
- Mid range: 100-1000 NTU
- High range: 1000-4000 NTU
