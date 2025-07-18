# TOC/BOD Sensor Calibration Guide

## Overview
This guide provides detailed instructions for calibrating Total Organic Carbon (TOC) and Biochemical Oxygen Demand (BOD) sensors to ensure accurate measurements of organic pollution and biological activity.

## Required Materials

### TOC Calibration Standards
- **0 mg/L**: Ultra-pure water (TOC-free)
- **10 mg/L**: Potassium hydrogen phthalate (KHP) solution
- **50 mg/L**: KHP solution
- **100 mg/L**: KHP solution
- **500 mg/L**: KHP solution (for high-range applications)

### BOD Calibration Standards
- **0 mg/L**: BOD-free dilution water
- **20 mg/L**: Glucose-glutamic acid solution
- **50 mg/L**: Glucose-glutamic acid solution
- **100 mg/L**: Glucose-glutamic acid solution
- **BOD seed solution**: Activated sludge or commercial seed

### Equipment
- Certified reference materials
- Volumetric flasks (100 mL, 1000 mL)
- Pipettes (1 mL, 10 mL, 100 mL)
- Incubator (20°C ± 1°C)
- pH meter
- Dissolved oxygen meter
- Temperature-controlled water bath

## Pre-Calibration Preparation

### System Setup
1. Ensure sensors are clean and dry
2. Check all connections and power supplies
3. Verify sample preparation system operation
4. Calibrate temperature sensor first
5. Allow system to reach thermal equilibrium

### Standard Preparation
1. Prepare fresh standards daily
2. Use analytical grade chemicals
3. Maintain standards at 20°C ± 2°C
4. Protect from light and contamination
5. Document preparation details

## TOC Sensor Calibration

### Step 1: Zero Point Calibration
1. Prepare TOC-free water:
   - Use ultra-pure water (>18 MΩ·cm)
   - Alternatively, use UV-irradiated distilled water
   - Verify TOC < 0.1 mg/L with reference method
2. Fill sample container with TOC-free water
3. Start measurement cycle
4. Allow 3 measurement cycles for stabilization
5. Record zero-point voltage reading

### Step 2: Span Calibration
1. Prepare 100 mg/L KHP standard:
   - Dissolve 0.2125 g KHP in 1000 mL ultra-pure water
   - KHP has theoretical TOC of 204.2 mg C/g
   - Actual TOC = 0.2125 × 204.2 = 43.4 mg/L
   - Dilute to 100 mg/L with ultra-pure water
2. Fill sample container with standard
3. Start measurement cycle
4. Allow 3 measurement cycles for stabilization
5. Record span-point voltage reading

### Step 3: Multi-Point Calibration
1. Prepare intermediate standards (10, 50 mg/L)
2. Measure each standard in ascending order
3. Record voltage readings for each standard
4. Calculate calibration curve parameters
5. Verify linearity (R² > 0.995)

### TOC Calibration Calculations
```
TOC (mg/L) = (Voltage - Intercept) / Slope

Where:
Slope = (V_span - V_zero) / (C_span - C_zero)
Intercept = V_zero - Slope × C_zero
```

## BOD Sensor Calibration

### Step 1: Prepare BOD-Free Water
1. Prepare phosphate buffer solution:
   - KH₂PO₄: 8.5 g/L
   - K₂HPO₄: 21.75 g/L
   - Na₂HPO₄·7H₂O: 33.4 g/L
   - NH₄Cl: 1.7 g/L
   - pH = 7.2 ± 0.1
2. Aerate buffer solution for 30 minutes
3. Verify dissolved oxygen > 8 mg/L
4. Use as zero-point standard

### Step 2: Prepare Glucose-Glutamic Acid Standard
1. Stock solution:
   - Glucose: 150 mg/L
   - Glutamic acid: 150 mg/L
   - Theoretical BOD₅ = 220 mg/L
2. Prepare working standards by dilution
3. Add BOD seed solution (1-2 mL/L)
4. Incubate duplicate samples at 20°C

### Step 3: BOD Calibration Procedure
1. Measure initial dissolved oxygen
2. Incubate samples for 5 days at 20°C
3. Measure final dissolved oxygen
4. Calculate BOD₅ = DO_initial - DO_final
5. Correlate with sensor voltage readings

### BOD Calibration Validation
- Use certified reference materials
- Perform duplicate measurements
- Verify precision within ±10%
- Check against standard methods

## Temperature Compensation

### TOC Temperature Correction
```
TOC_corrected = TOC_measured × (1 + 0.01 × (T - 20))
```

### BOD Temperature Correction
```
BOD_corrected = BOD_measured × (1 + 0.02 × (T - 20))
```

## Quality Control

### Daily Checks
- Zero and span verification
- Standard reference measurement
- System blank analysis
- Control chart monitoring

### Weekly Checks
- Multi-point calibration verification
- Precision and accuracy assessment
- System maintenance
- Documentation review

### Monthly Checks
- Full recalibration
- Sensor cleaning and inspection
- Replacement of consumables
- Performance evaluation

## Troubleshooting

### TOC Sensor Issues
| Problem | Possible Cause | Solution |
|---------|---------------|----------|
| Drift in zero point | Contamination | Clean sensor, replace reagents |
| Poor linearity | Sensor degradation | Replace sensor element |
| High noise | Electrical interference | Check grounding, shielding |
| Slow response | Fouling | Clean sensor, check flow rate |

### BOD Sensor Issues
| Problem | Possible Cause | Solution |
|---------|---------------|----------|
| Low readings | Insufficient seed | Increase seed concentration |
| High variability | Temperature fluctuation | Improve temperature control |
| Negative values | Nitrification | Add nitrification inhibitor |
| Slow stabilization | Poor mixing | Improve sample mixing |

## Method Validation

### Accuracy Assessment
- Use certified reference materials
- Compare with reference methods
- Calculate bias and precision
- Verify measurement uncertainty

### Precision Testing
- Perform replicate measurements
- Calculate standard deviation
- Verify repeatability < 5%
- Check reproducibility < 10%

### Linearity Verification
- Prepare calibration series
- Perform linear regression
- Verify R² > 0.995
- Check residual distribution

## Maintenance Schedule

### Daily Tasks
- [ ] Check sensor operation
- [ ] Verify calibration stability
- [ ] Clean sample lines
- [ ] Record performance data

### Weekly Tasks
- [ ] Single-point calibration check
- [ ] Clean sensor surfaces
- [ ] Check reagent levels
- [ ] Inspect connections

### Monthly Tasks
- [ ] Full recalibration
- [ ] Replace consumables
- [ ] Performance evaluation
- [ ] Documentation update

### Quarterly Tasks
- [ ] Sensor inspection
- [ ] Method validation
- [ ] Preventive maintenance
- [ ] Training review

## Advanced Calibration Techniques

### Matrix Matching
- Use standards in sample matrix
- Account for matrix effects
- Validate with real samples
- Document matrix influences

### Multi-Range Calibration
- Low range: 0-50 mg/L
- Mid range: 50-200 mg/L
- High range: 200-1000 mg/L
- Optimize for application range

### Automated Calibration
- Programmable standard delivery
- Automated calibration sequences
- Remote calibration verification
- Scheduled maintenance alerts

## Data Management

### Record Keeping
- Calibration certificates
- Standard preparation logs
- Measurement uncertainty data
- Maintenance records

### Documentation
- Standard operating procedures
- Calibration history
- Performance trends
- Corrective actions

## Safety Considerations
- Handle chemicals safely
- Use appropriate PPE
- Follow waste disposal procedures
- Maintain MSDS documentation
- Train personnel properly

## Regulatory Compliance
- Follow EPA methods
- Maintain traceability
- Document procedures
- Perform regular audits
- Keep certification current
