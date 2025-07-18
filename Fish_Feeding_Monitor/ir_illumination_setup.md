# IR Illumination Setup Guide

## Overview
This guide covers the setup and configuration of infrared (IR) illumination for the Fish Feeding Monitor system. IR illumination enables 24/7 monitoring of fish feeding behavior without disturbing the aquatic environment.

## Why IR Illumination?

### Advantages
- **Invisible to Fish**: 850nm IR light is outside fish vision spectrum
- **24/7 Monitoring**: Enables night-time feeding analysis
- **Non-Disruptive**: Doesn't affect natural behavior patterns
- **Energy Efficient**: LED technology with long lifespan
- **Weather Independent**: Works in all lighting conditions

### Applications
- **Nocturnal Species**: Monitor night-feeding fish
- **Shaded Environments**: Provide illumination in covered areas
- **Seasonal Monitoring**: Consistent lighting year-round
- **Behavior Studies**: Analyze feeding patterns without influence

## Component Selection

### IR LED Specifications
| Parameter | Recommended Value | Notes |
|-----------|------------------|--------|
| Wavelength | 850nm | Optimal for cameras, invisible to fish |
| Power | 5-10W per LED | Sufficient for 2-3 meter range |
| Beam Angle | 60-120° | Wide coverage for feeding area |
| Operating Voltage | 12V DC | Common power supply voltage |
| Lifespan | 50,000+ hours | Long-term reliability |

### Camera Compatibility
- **Sensor Type**: CMOS sensors work best with IR
- **IR Cut Filter**: Must be removable or switchable
- **Sensitivity**: Check IR sensitivity specs
- **Resolution**: Maintain resolution in IR mode

### Power Supply Requirements
- **Voltage**: 12V DC regulated
- **Current**: 2-5A depending on LED count
- **Regulation**: ±5% voltage stability
- **Protection**: Overcurrent and overvoltage protection

## System Design

### LED Array Configuration
#### 4-LED Square Array (Recommended)
```
    [LED1]     [LED2]
        \       /
         \     /
          Camera
         /     \
        /       \
    [LED3]     [LED4]
```

#### 8-LED Circular Array (High Performance)
```
        [LED1]
    [LED8]   [LED2]
  [LED7]   CAM   [LED3]
    [LED6]   [LED4]
        [LED5]
```

### Mounting Considerations
- **Height**: 0.5-1.5m above water surface
- **Angle**: 30-45° downward for optimal coverage
- **Protection**: IP68 rated for outdoor use
- **Stability**: Secure mounting to prevent vibration

## Installation Procedure

### Step 1: LED Array Assembly
1. **Select LEDs**: Choose 850nm IR LEDs with appropriate power rating
2. **Mount LEDs**: Use aluminum heat sink for thermal management
3. **Wire in Series**: Connect LEDs in series for voltage matching
4. **Add Resistors**: Calculate current limiting resistors
5. **Apply Thermal Paste**: Ensure good thermal contact

### Step 2: Power Supply Setup
1. **Calculate Power**: Total LED power + 20% safety margin
2. **Select Supply**: Choose regulated 12V DC supply
3. **Install Protection**: Add fuses and circuit breakers
4. **Ground System**: Ensure proper grounding for safety
5. **Test Voltage**: Verify output voltage and regulation

### Step 3: Camera Configuration
1. **Remove IR Filter**: Disable IR cut filter in camera settings
2. **Set Exposure**: Adjust for IR illumination levels
3. **Configure Gain**: Set appropriate gain for IR sensitivity
4. **White Balance**: Adjust for IR spectrum
5. **Focus**: Refocus lens for IR wavelength

### Step 4: Control Circuit
1. **GPIO Control**: Connect LED control to Raspberry Pi GPIO
2. **MOSFET Driver**: Use power MOSFET for switching
3. **PWM Control**: Implement PWM for brightness control
4. **Current Sensing**: Monitor LED current for diagnostics
5. **Temperature Monitoring**: Add thermal sensors

## Circuit Diagram

### Basic LED Control Circuit
```
+12V ─────┬─────────────────────────────────┐
          │                                 │
          R1 (Current Limiting)             │
          │                                 │
        ┌─┴─┐                             │
        │LED│ (850nm IR)                  │
        └─┬─┘                             │
          │                                 │
          │    ┌─────────────────────────┐   │
          └────┤ N-Channel MOSFET        │   │
               │ (IRF540N or similar)    │   │
               └─────────────────────────┘   │
                         │                  │
                         │                  │
                    ┌────┴────┐              │
                    │ GPIO Pin│              │
                    │(RPi)    │              │
                    └─────────┘              │
                                             │
Ground ─────────────────────────────────────┘
```

### PWM Control Circuit
```
RPi GPIO ──[1kΩ]──┬── Gate (MOSFET)
                  │
                  └── [10kΩ] ── GND
                  
Drain (MOSFET) ── LED Array ── +12V
Source (MOSFET) ── GND
```

## Software Configuration

### GPIO Control (Python)
```python
import RPi.GPIO as GPIO
import time

# Setup GPIO
IR_LED_PIN = 24
GPIO.setmode(GPIO.BCM)
GPIO.setup(IR_LED_PIN, GPIO.OUT)

# Create PWM instance
pwm = GPIO.PWM(IR_LED_PIN, 1000)  # 1kHz frequency
pwm.start(0)  # Start with 0% duty cycle

def set_ir_brightness(brightness):
    """Set IR LED brightness (0-100%)"""
    pwm.ChangeDutyCycle(brightness)

def enable_ir_illumination():
    """Enable IR illumination"""
    GPIO.output(IR_LED_PIN, GPIO.HIGH)

def disable_ir_illumination():
    """Disable IR illumination"""
    GPIO.output(IR_LED_PIN, GPIO.LOW)

# Automatic night detection
def auto_ir_control():
    """Automatically control IR based on ambient light"""
    import datetime
    
    current_hour = datetime.datetime.now().hour
    
    if current_hour < 6 or current_hour > 18:
        # Night time - enable IR
        set_ir_brightness(80)  # 80% brightness
        print("IR illumination enabled (night mode)")
    else:
        # Day time - disable IR
        set_ir_brightness(0)
        print("IR illumination disabled (day mode)")
```

### Camera Configuration
```python
from picamera2 import Picamera2
import cv2

def setup_ir_camera():
    """Configure camera for IR operation"""
    camera = Picamera2()
    
    # Configure for IR
    config = camera.create_preview_configuration(
        main={"size": (1920, 1080)},
        controls={
            "ExposureTime": 20000,  # 20ms exposure
            "AnalogueGain": 2.0,    # Increase gain for IR
            "ColourGains": (1.0, 1.0),  # Disable auto white balance
            "AeEnable": False,      # Disable auto exposure
            "AwbEnable": False      # Disable auto white balance
        }
    )
    
    camera.configure(config)
    return camera

def capture_ir_frame(camera):
    """Capture frame optimized for IR"""
    frame = camera.capture_array()
    
    # Convert to grayscale for IR processing
    if len(frame.shape) == 3:
        frame = cv2.cvtColor(frame, cv2.COLOR_RGB2GRAY)
    
    # Apply contrast enhancement
    frame = cv2.equalizeHist(frame)
    
    return frame
```

## Performance Optimization

### LED Efficiency
- **Heat Management**: Use adequate heat sinks
- **Current Control**: Operate at optimal current
- **Beam Focusing**: Use lenses for directed illumination
- **Reflectors**: Add reflectors to increase efficiency

### Camera Optimization
- **Exposure Time**: Balance between noise and motion blur
- **Gain Settings**: Minimize gain to reduce noise
- **Frame Rate**: Adjust for processing capability
- **Resolution**: Use appropriate resolution for detection

### Power Management
- **Dimming Control**: Reduce brightness when not needed
- **Thermal Protection**: Monitor temperature and reduce power
- **Battery Backup**: Include UPS for power outages
- **Energy Monitoring**: Track power consumption

## Maintenance and Troubleshooting

### Regular Maintenance
#### Weekly
- **Visual Inspection**: Check for damaged LEDs
- **Clean Lenses**: Remove water spots and algae
- **Check Connections**: Verify all electrical connections
- **Test Brightness**: Ensure consistent illumination

#### Monthly
- **Thermal Check**: Monitor LED temperatures
- **Current Measurement**: Verify LED current draw
- **Mounting Inspection**: Check for loose hardware
- **Performance Test**: Validate illumination coverage

### Common Issues and Solutions

| Problem | Symptoms | Cause | Solution |
|---------|----------|-------|----------|
| Dim Illumination | Poor image quality | LED degradation | Replace LEDs |
| Uneven Lighting | Bright/dark spots | Misaligned LEDs | Adjust LED positions |
| Overheating | LEDs shutting down | Inadequate cooling | Add heat sinks |
| Power Issues | Intermittent operation | Power supply problems | Check voltage/current |
| Image Quality | Grainy/noisy images | Improper camera settings | Adjust exposure/gain |

### Troubleshooting Steps
1. **Check Power Supply**: Verify voltage and current
2. **Measure LED Current**: Ensure proper operation
3. **Test Individual LEDs**: Identify failed components
4. **Check Connections**: Verify all wiring
5. **Monitor Temperature**: Check for overheating
6. **Validate Software**: Ensure proper GPIO control

## Safety Considerations

### Electrical Safety
- **Proper Grounding**: All equipment must be grounded
- **GFCI Protection**: Use ground fault circuit interrupters
- **Waterproof Connections**: IP68 rated for wet environments
- **Fusing**: Appropriate fuses for overcurrent protection

### Eye Safety
- **IR Radiation**: 850nm IR is invisible but can cause eye damage
- **Warning Labels**: Mark all IR equipment clearly
- **Protective Equipment**: Use IR viewing cards for alignment
- **Power Limits**: Keep power within safe levels

### Environmental Safety
- **Heat Dissipation**: Ensure adequate cooling
- **Waterproofing**: Protect from moisture ingress
- **UV Protection**: Use UV-resistant materials
- **Wildlife Protection**: Minimize impact on non-target species

## Performance Validation

### Test Procedures
1. **Illumination Coverage**: Map light distribution
2. **Intensity Measurement**: Use IR power meter
3. **Camera Performance**: Test image quality
4. **Detection Accuracy**: Validate fish detection
5. **Reliability Testing**: Long-term operation test

### Success Metrics
- **Coverage**: 90%+ of feeding area illuminated
- **Uniformity**: <20% variation across coverage area
- **Reliability**: >99% uptime over 30 days
- **Detection Rate**: >95% fish detection accuracy
- **Power Efficiency**: <50W total system consumption

## Advanced Features

### Adaptive Illumination
- **Ambient Light Sensor**: Automatically adjust brightness
- **Motion Detection**: Increase brightness when fish detected
- **Time-based Control**: Vary intensity throughout night
- **Weather Compensation**: Adjust for cloud cover

### Multi-Spectrum Illumination
- **Dual Wavelength**: 850nm and 940nm for different conditions
- **Visible Light**: Emergency visible illumination
- **UV-A**: Special applications for fluorescent marking
- **Polarized Light**: Reduce reflections from water surface

### Smart Control
- **MQTT Integration**: Remote monitoring and control
- **Scheduled Operation**: Automated on/off timing
- **Fault Detection**: Automatic problem identification
- **Energy Optimization**: Minimize power consumption

## Cost Analysis

### Initial Investment
- **LEDs**: $50-100 for 4-8 LED array
- **Power Supply**: $30-50 for regulated 12V supply
- **Control Circuit**: $20-30 for MOSFET driver
- **Housing**: $40-60 for weatherproof enclosure
- **Installation**: $100-200 for labor

### Operating Costs
- **Power Consumption**: ~$20-40/year at $0.10/kWh
- **Maintenance**: ~$50/year for replacement parts
- **Upgrades**: ~$100/year for improvements

### Return on Investment
- **24/7 Monitoring**: Priceless for research applications
- **Reduced Labor**: Automated monitoring saves time
- **Better Data**: Improved feeding behavior analysis
- **Early Detection**: Prevent fish health issues

This comprehensive IR illumination setup will provide reliable, efficient lighting for continuous fish feeding monitoring while maintaining the natural behavior of aquatic species.
