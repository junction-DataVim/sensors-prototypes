# Ammonia Sensor Calibration Guide

## Overview
This guide provides detailed instructions for calibrating the ammonia sensor using ion-selective electrodes (ISE). Proper calibration is essential for accurate measurement of ammonia and ammonium concentrations in aquatic systems.

## Safety Precautions

### Personal Protective Equipment
- **Safety glasses** - Protect eyes from chemical splashes
- **Nitrile gloves** - Prevent skin contact with solutions
- **Lab coat** - Protect clothing from chemical spills
- **Closed-toe shoes** - Protect feet from spills

### Chemical Safety
- **Ammonia solutions**: Use in well-ventilated area
- **Sodium hydroxide**: Highly caustic - handle with care
- **Hydrochloric acid**: Corrosive - avoid skin contact
- **Potassium chloride**: Irritant - avoid inhalation

### Emergency Procedures
- **Skin contact**: Rinse immediately with copious water
- **Eye contact**: Flush for 15 minutes, seek medical attention
- **Inhalation**: Move to fresh air, seek medical help if needed
- **Spills**: Neutralize with appropriate solutions

## Required Materials

### Calibration Standards
| Standard | Concentration | Volume Needed | Storage |
|----------|---------------|---------------|----------|
| NH₄⁺ Standard 1 | 1.0 mg/L NH₄⁺-N | 100 mL | Refrigerate, use within 1 month |
| NH₄⁺ Standard 2 | 10.0 mg/L NH₄⁺-N | 100 mL | Refrigerate, use within 1 month |
| NH₄⁺ Standard 3 | 100.0 mg/L NH₄⁺-N | 100 mL | Refrigerate, use within 1 month |

### Reagents
| Reagent | Purpose | Concentration | Amount |
|---------|---------|---------------|---------|
| ISA Solution | Ionic strength adjustment | 5 M KCl | 50 mL |
| NaOH Solution | pH adjustment | 1 M | 25 mL |
| HCl Solution | pH adjustment | 1 M | 25 mL |
| KCl Solution | Reference electrode | 3 M | 100 mL |

### Equipment
- **Beakers**: 150 mL (4 pieces)
- **Volumetric flasks**: 100 mL (3 pieces)
- **Pipettes**: 1-10 mL variable volume
- **Magnetic stirrer** with stir bars
- **pH meter** (calibrated)
- **Thermometer** (±0.1°C)
- **Wash bottle** with distilled water
- **Timer** for timing measurements

## Standard Preparation

### 1000 mg/L NH₄⁺-N Stock Solution
1. **Weigh** 4.717 g of ammonium chloride (NH₄Cl)
2. **Dissolve** in 800 mL distilled water
3. **Add** 2 mL of 1 M HCl to prevent bacterial growth
4. **Dilute** to 1000 mL with distilled water
5. **Store** in refrigerator (4°C) for up to 6 months

### Working Standards Preparation
#### 100 mg/L Standard
- Dilute 100 mL of stock solution to 1000 mL

#### 10 mg/L Standard
- Dilute 10 mL of 100 mg/L standard to 100 mL

#### 1 mg/L Standard
- Dilute 1 mL of 100 mg/L standard to 100 mL

### Ionic Strength Adjuster (ISA)
1. **Dissolve** 373 g KCl in 800 mL distilled water
2. **Add** 57 mL concentrated ammonia solution
3. **Dilute** to 1000 mL with distilled water
4. **Store** in plastic bottle at room temperature

## Pre-Calibration Procedures

### 1. Electrode Conditioning
- **Soak** electrodes in 10 mg/L NH₄⁺ solution for 2 hours
- **Rinse** with distilled water
- **Dry** gently with tissue
- **Allow** to stabilize for 30 minutes

### 2. System Setup
- **Connect** electrodes to measurement system
- **Verify** electrical connections are secure
- **Check** electrode impedance (should be >10¹² Ω)
- **Calibrate** pH meter with standard buffers

### 3. Environmental Conditions
- **Temperature**: 20-25°C (record exact temperature)
- **Humidity**: 40-60% relative humidity
- **Vibration**: Minimize mechanical vibrations
- **Electrical**: Avoid electrical interference

## Calibration Procedure

### Step 1: Prepare Calibration Solutions
1. **Add ISA** to each standard (2 mL per 100 mL)
2. **Mix** thoroughly by gentle swirling
3. **Allow** to reach room temperature
4. **Arrange** standards in order of increasing concentration

### Step 2: Electrode Preparation
1. **Remove** electrodes from conditioning solution
2. **Rinse** thoroughly with distilled water
3. **Dry** gently with soft tissue
4. **Check** for air bubbles in reference electrode
5. **Verify** KCl solution level in reference electrode

### Step 3: Calibration Measurements
#### Lowest Standard (1 mg/L)
1. **Pour** 100 mL of 1 mg/L standard into beaker
2. **Add** magnetic stir bar
3. **Immerse** electrodes ensuring no air bubbles
4. **Start** gentle stirring (avoid splashing)
5. **Wait** for stable reading (±1 mV for 60 seconds)
6. **Record** voltage and temperature
7. **Note** time required for stabilization

#### Medium Standard (10 mg/L)
1. **Rinse** electrodes with distilled water
2. **Dry** gently with tissue
3. **Immerse** in 10 mg/L standard
4. **Wait** for stable reading
5. **Record** voltage and temperature

#### Highest Standard (100 mg/L)
1. **Rinse** electrodes with distilled water
2. **Dry** gently with tissue
3. **Immerse** in 100 mg/L standard
4. **Wait** for stable reading
5. **Record** voltage and temperature

### Step 4: Data Analysis
1. **Plot** log concentration vs. voltage
2. **Calculate** slope using linear regression
3. **Determine** intercept and correlation coefficient
4. **Verify** slope is within acceptable range (50-65 mV/decade)

## Slope Calculation

### Nernst Equation
The ISE follows the Nernst equation:
```
E = E₀ + (RT/nF) × ln(a)
```

Where:
- E = measured potential (mV)
- E₀ = standard electrode potential (mV)
- R = gas constant (8.314 J/mol·K)
- T = absolute temperature (K)
- n = charge number (1 for NH₄⁺)
- F = Faraday constant (96,485 C/mol)
- a = activity of NH₄⁺ ion

### Practical Slope Calculation
At 25°C, the theoretical slope is:
```
Slope = (RT/nF) × ln(10) = 59.16 mV/decade
```

Temperature correction:
```
Slope(T) = 59.16 × (T + 273.15) / 298.15
```

## Acceptable Performance Criteria

### Slope Requirements
- **Ideal**: 59.16 mV/decade at 25°C
- **Acceptable**: 50-65 mV/decade
- **Poor**: <50 or >65 mV/decade (replace electrode)

### Correlation Coefficient
- **Excellent**: R² > 0.999
- **Good**: R² > 0.995
- **Acceptable**: R² > 0.990
- **Poor**: R² < 0.990 (investigate issues)

### Response Time
- **Fast**: <2 minutes to 90% of final reading
- **Acceptable**: 2-5 minutes
- **Slow**: >5 minutes (clean/replace electrode)

### Reproducibility
- **Excellent**: <2% RSD for duplicate measurements
- **Good**: 2-5% RSD
- **Acceptable**: 5-10% RSD
- **Poor**: >10% RSD (investigate problems)

## Troubleshooting

### Poor Slope Issues
| Symptom | Cause | Solution |
|---------|-------|----------|
| Slope < 50 mV/decade | Aged electrode membrane | Replace electrode |
| Slope > 65 mV/decade | Contaminated electrode | Clean and recondition |
| Irregular slope | Air bubbles | Remove bubbles, re-measure |
| No slope | Broken electrode | Check continuity, replace |

### Slow Response Issues
| Symptom | Cause | Solution |
|---------|-------|----------|
| >5 min stabilization | Dirty electrode | Clean with appropriate solution |
| Baseline drift | Reference electrode | Replace reference solution |
| Noisy readings | Electrical interference | Check grounding, shielding |
| Erratic behavior | Poor connections | Verify all connections |

### Calibration Failures
| Problem | Likely Cause | Corrective Action |
|---------|--------------|-------------------|
| Non-linear response | Electrode damage | Replace electrode |
| Poor correlation | Contaminated standards | Prepare fresh standards |
| Inconsistent readings | Temperature variation | Control temperature |
| Baseline shift | Reference electrode | Service reference electrode |

## Quality Control

### Calibration Verification
1. **Independent standard**: Use different lot number
2. **Duplicate measurements**: Measure each standard twice
3. **Blank measurement**: Measure distilled water
4. **Cross-check**: Compare with colorimetric method

### Documentation Requirements
- **Date and time** of calibration
- **Operator name** and signature
- **Standard lot numbers** and expiration dates
- **Temperature** during calibration
- **Voltage readings** for each standard
- **Calculated slope** and intercept
- **Correlation coefficient**
- **Any deviations** from procedure

### Acceptance Criteria
- **Slope**: Within 50-65 mV/decade
- **Correlation**: R² > 0.995
- **Reproducibility**: <5% RSD
- **Verification**: Within ±10% of independent standard

## Maintenance Schedule

### Daily
- **Visual inspection** of electrodes
- **Check** reference electrode solution level
- **Verify** stable baseline readings

### Weekly
- **Performance check** with single standard
- **Clean** electrodes with appropriate solution
- **Check** electrical connections

### Monthly
- **Full calibration** with all standards
- **Replace** reference electrode solution
- **Document** performance trends

### Quarterly
- **Replace** electrodes if performance degrades
- **Prepare** fresh calibration standards
- **Audit** calibration procedures

## Storage and Handling

### Electrode Storage
- **Short-term**: Store in conditioning solution
- **Long-term**: Store dry in protective cap
- **Never**: Store in distilled water
- **Temperature**: Room temperature (20-25°C)

### Standard Storage
- **Refrigeration**: 4°C for all standards
- **Expiration**: 1 month for working standards
- **Labeling**: Date prepared and expiration
- **Containers**: Use only glass or PTFE

### Reference Electrode Maintenance
- **Monthly**: Replace KCl solution
- **Inspect**: Check for crystal formation
- **Clean**: Remove salt deposits carefully
- **Condition**: Soak in fresh KCl before use

## Environmental Considerations

### Waste Disposal
- **Neutralize** acidic and basic solutions
- **Collect** ammonia solutions for proper disposal
- **Recycle** glass containers when possible
- **Follow** local environmental regulations

### Sustainability
- **Minimize** solution volumes
- **Reuse** standards when possible
- **Optimize** calibration frequency
- **Document** chemical usage

This calibration guide ensures accurate and reliable ammonia measurements while maintaining safety and environmental responsibility. Regular calibration following these procedures will provide high-quality data for aquatic monitoring applications.
