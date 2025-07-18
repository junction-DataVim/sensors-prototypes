# Aquatic Monitoring System - Calibration Procedures

## Overview
This document outlines the standardized calibration procedures for all sensors in the Aquatic Monitoring System. Proper calibration is essential for accurate water quality measurements and reliable monitoring data.

## General Calibration Requirements

### Required Equipment
- **Digital multimeter** (for electrical measurements)
- **Calibrated thermometer** (±0.1°C accuracy)
- **Certified reference standards** (specific to each sensor type)
- **Clean distilled water** (for rinsing)
- **Calibration logbook** (for record keeping)
- **Timer/stopwatch** (for timing measurements)
- **Protective equipment** (gloves, safety glasses)

### Environmental Conditions
- **Temperature**: 20-25°C (room temperature)
- **Humidity**: 40-60% relative humidity
- **Pressure**: Standard atmospheric pressure
- **Lighting**: Consistent lighting for optical sensors
- **Stability**: No vibrations or electrical interference

### Pre-Calibration Checklist
1. ✅ Verify sensor is clean and free from contamination
2. ✅ Check all electrical connections are secure
3. ✅ Ensure reference standards are within expiration date
4. ✅ Record ambient temperature and humidity
5. ✅ Allow sensors to stabilize for 15-30 minutes
6. ✅ Verify data logging system is functioning

---

## Sensor-Specific Calibration Procedures

### 1. pH Sensor Calibration

**Required Standards:**
- pH 4.01 buffer solution
- pH 7.00 buffer solution  
- pH 10.01 buffer solution
- Temperature compensation probe

**Procedure:**
1. **Preparation**
   - Rinse electrode with distilled water
   - Gently dry with soft tissue
   - Allow to reach room temperature (20-25°C)

2. **Two-Point Calibration**
   - Immerse electrode in pH 7.00 buffer
   - Wait for stable reading (±0.01 pH units)
   - Record reading and adjust offset if needed
   - Rinse electrode thoroughly
   - Immerse in pH 4.01 buffer
   - Wait for stable reading
   - Record reading and adjust slope if needed

3. **Three-Point Calibration (Recommended)**
   - Perform two-point calibration first
   - Rinse electrode thoroughly
   - Immerse in pH 10.01 buffer
   - Wait for stable reading
   - Verify linearity and adjust if needed

4. **Verification**
   - Test with pH 7.00 buffer again
   - Acceptable range: 7.00 ± 0.05 pH units
   - Record calibration date and constants

**Calibration Frequency:** Every 2 weeks or after 100 measurements

### 2. Dissolved Oxygen (DO) Sensor Calibration

**Required Standards:**
- 0% oxygen solution (sodium sulfite)
- 100% air-saturated water
- Barometric pressure reading
- Temperature measurement

**Procedure:**
1. **Zero Calibration**
   - Prepare 0% oxygen solution using sodium sulfite
   - Immerse sensor in solution
   - Wait 10-15 minutes for stabilization
   - Set zero point reading

2. **Span Calibration**
   - Prepare air-saturated water at known temperature
   - Record barometric pressure
   - Calculate theoretical DO concentration
   - Immerse sensor in solution
   - Wait for stable reading
   - Adjust span to match theoretical value

3. **Verification**
   - Test with fresh air-saturated water
   - Acceptable range: ±0.2 mg/L

**Calibration Frequency:** Weekly or after 50 measurements

### 3. Conductivity/TDS Sensor Calibration

**Required Standards:**
- 1413 µS/cm standard solution
- 12.88 mS/cm standard solution
- Cell constant verification solution

**Procedure:**
1. **Cell Constant Determination**
   - Use certified KCl solution
   - Measure resistance and calculate cell constant
   - Verify against manufacturer specifications

2. **Two-Point Calibration**
   - Immerse sensor in 1413 µS/cm solution
   - Wait for stable reading
   - Adjust for low-range calibration
   - Rinse thoroughly with distilled water
   - Immerse in 12.88 mS/cm solution
   - Adjust for high-range calibration

3. **Temperature Compensation**
   - Verify temperature coefficient setting
   - Test at different temperatures if possible

**Calibration Frequency:** Monthly or after 200 measurements

### 4. Turbidity Sensor Calibration

**Required Standards:**
- 0 NTU (distilled water)
- 100 NTU formazin standard
- 1000 NTU formazin standard
- Black light trap

**Procedure:**
1. **Zero Calibration**
   - Fill sample chamber with distilled water
   - Ensure no air bubbles
   - Set zero reading

2. **Span Calibration**
   - Use 100 NTU formazin standard
   - Ensure standard is well-mixed
   - Adjust span calibration

3. **Linearity Check**
   - Test with 1000 NTU standard
   - Verify linear response
   - Adjust if necessary

**Calibration Frequency:** Weekly or after 100 measurements

### 5. Ammonia Sensor Calibration

**Required Standards:**
- 1 mg/L NH₃-N standard
- 10 mg/L NH₃-N standard
- 100 mg/L NH₃-N standard
- pH adjustment solutions

**Procedure:**
1. **pH Adjustment**
   - Adjust standards to pH 11-12 for NH₃ measurement
   - Allow time for equilibration

2. **Multi-Point Calibration**
   - Start with lowest concentration
   - Wait for stable reading (may take 5-10 minutes)
   - Record reading for each standard
   - Generate calibration curve

3. **Verification**
   - Test with independent standard
   - Acceptable range: ±10% of true value

**Calibration Frequency:** Bi-weekly or after 50 measurements

---

## Quality Control Procedures

### Calibration Verification
1. **Independent Standards**
   - Use different lot numbers for verification
   - Source from different suppliers when possible
   - Verify against NIST-traceable standards

2. **Duplicate Measurements**
   - Perform calibration in duplicate
   - Calculate relative percent difference
   - Acceptable RPD: <5% for most parameters

3. **Blank Checks**
   - Analyze method blanks
   - Verify no contamination or drift
   - Document any anomalies

### Data Management
1. **Documentation Requirements**
   - Record all calibration data
   - Note environmental conditions
   - Document any deviations from procedure
   - Store records for minimum 2 years

2. **Calibration Certificates**
   - Maintain certificates for all standards
   - Track expiration dates
   - Verify traceability to national standards

3. **Trend Analysis**
   - Plot calibration data over time
   - Look for systematic drift
   - Identify when maintenance is needed

---

## Troubleshooting Common Issues

### pH Sensor Issues
| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| Slow response | Dirty electrode | Clean with appropriate solution |
| Erratic readings | Cracked bulb | Replace electrode |
| Poor slope | Old electrode | Replace electrode |
| Drift | Temperature effects | Verify temperature compensation |

### DO Sensor Issues
| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| Low readings | Membrane fouling | Clean or replace membrane |
| No response | Electrolyte depletion | Refill electrolyte |
| Slow response | Air bubbles | Remove bubbles, ensure proper flow |

### Conductivity Issues
| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| Unstable readings | Electrode fouling | Clean electrodes thoroughly |
| Poor linearity | Cell constant error | Recalibrate cell constant |
| Temperature errors | Compensation failure | Check temperature probe |

---

## Maintenance Schedule

### Daily
- Visual inspection of sensors
- Check for obvious damage or fouling
- Verify data logging is functioning

### Weekly
- Clean sensors as needed
- Check reference standards
- Verify calibration within specifications

### Monthly
- Complete calibration of all sensors
- Replace consumables (membranes, electrolytes)
- Update calibration records

### Quarterly
- Comprehensive system check
- Replace aging sensors
- Audit calibration procedures

### Annually
- Replace all reference standards
- Comprehensive training review
- Equipment calibration verification

---

## Validation Techniques

### Statistical Methods
1. **Regression Analysis**
   - Calculate R² for calibration curves
   - Acceptable R² > 0.995 for most sensors
   - Identify outliers and investigate

2. **Method Detection Limit (MDL)**
   - Analyze blank samples repeatedly
   - Calculate standard deviation
   - MDL = 3 × standard deviation

3. **Precision and Accuracy**
   - Use certified reference materials
   - Calculate bias and precision
   - Document method performance

### Inter-laboratory Comparisons
1. **Split Sample Analysis**
   - Send samples to certified laboratories
   - Compare results statistically
   - Investigate significant differences

2. **Proficiency Testing**
   - Participate in external QC programs
   - Analyze blind samples
   - Maintain certification status

---

## Emergency Procedures

### Calibration Failure
1. **Immediate Actions**
   - Stop measurements immediately
   - Document the failure
   - Notify system administrator

2. **Investigation**
   - Check all connections
   - Verify standard solutions
   - Review recent maintenance

3. **Corrective Action**
   - Recalibrate with fresh standards
   - Replace faulty components
   - Verify correction with independent method

### Standard Contamination
1. **Identification**
   - Check for visible contamination
   - Verify with independent standard
   - Document contamination source

2. **Response**
   - Discard contaminated standard
   - Prepare fresh standard
   - Recalibrate affected sensors

---

## Record Keeping

### Required Documentation
- Calibration date and time
- Environmental conditions
- Standard lot numbers and expiration dates
- Measured values and adjustments
- Operator name and signature
- Equipment serial numbers

### Electronic Records
- Automated data logging where possible
- Backup calibration data regularly
- Maintain audit trail
- Password protection for critical data

### Reporting
- Monthly calibration summary
- Quarterly trend analysis
- Annual method validation report
- Deviation investigations

---

## Training Requirements

### Initial Training
- 8-hour calibration workshop
- Hands-on practice with all sensors
- Written examination (80% minimum)
- Supervised calibration period

### Ongoing Training
- Annual refresher training
- New procedure training as needed
- Proficiency testing
- Documentation of training records

### Competency Assessment
- Practical calibration demonstration
- Problem-solving scenarios
- Record keeping evaluation
- Continuous improvement feedback

---

## Conclusion

Proper calibration is the foundation of reliable water quality monitoring. Following these standardized procedures ensures:

- **Accurate measurements** for environmental assessment
- **Consistent data quality** across all monitoring locations
- **Regulatory compliance** with environmental standards
- **Early detection** of system problems
- **Confidence** in monitoring results

Regular calibration, combined with proper maintenance and documentation, provides the data quality needed for effective aquatic ecosystem management.

---

*Document Version: 1.0*  
*Last Updated: 2025-07-18*  
*Next Review: 2026-01-18*
