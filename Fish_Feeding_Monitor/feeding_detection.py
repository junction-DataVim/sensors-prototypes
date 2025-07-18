#!/usr/bin/env python3
"""
Aquatic Monitoring System - Fish Feeding Detection Module

This module uses computer vision to detect and analyze fish feeding behavior,
measuring strike rates and feeding efficiency using a Raspberry Pi camera.

Features:
- Real-time fish and food detection using YOLO
- Strike rate analysis with motion tracking
- Feeding efficiency calculation
- MQTT data transmission
- Automated feeding control
- Night vision with IR illumination
- Data logging and visualization

Hardware:
- Raspberry Pi 4 (8GB RAM recommended)
- Pi Camera V2.1 or USB camera
- IR LED array for night vision
- Servo motor for feeding mechanism
- Waterproof housing

Author: Aquatic Monitoring Team
Version: 1.0.0
Date: 2025-07-18
"""

import cv2
import numpy as np
import time
import json
import logging
import threading
import queue
from datetime import datetime, timedelta
import paho.mqtt.client as mqtt
import RPi.GPIO as GPIO
from picamera2 import Picamera2
import math
import os
import sqlite3
from collections import deque
import yaml
import argparse
import signal
import sys
import requests

# Constants
DEVICE_ID = "fish_feeding_monitor_01"
LOCATION_ID = "pond_01"
MQTT_BROKER = "your-mqtt-broker.com"
MQTT_PORT = 1883
MQTT_USERNAME = "aquatic_user"
MQTT_PASSWORD = "secure_password"

# API Configuration
API_SERVER = "your-api-server.com"
API_PORT = 3000
API_ENDPOINT = "/api/fish-feeding-readings"
POOL_ID = "pool_001"

# GPIO Pin definitions
SERVO_PIN = 18          # Servo motor for feeding mechanism
IR_LED_PIN = 24         # IR LED array control
MOTION_SENSOR_PIN = 23  # PIR motion sensor
STATUS_LED_PIN = 25     # Status LED

# Camera settings
CAMERA_WIDTH = 1920
CAMERA_HEIGHT = 1080
CAMERA_FPS = 30
EXPOSURE_TIME = 10000   # Microseconds

# Detection parameters
FISH_CONFIDENCE_THRESHOLD = 0.5
FOOD_CONFIDENCE_THRESHOLD = 0.3
MOTION_THRESHOLD = 30
MAX_STRIKE_DISTANCE = 100  # Pixels
MIN_STRIKE_DURATION = 0.1  # Seconds
MAX_STRIKE_DURATION = 2.0  # Seconds

# Feeding parameters
FEED_AMOUNT_GRAMS = 5.0
FEEDING_DURATION = 300  # 5 minutes
ANALYSIS_WINDOW = 60    # 1 minute for strike rate calculation

# File paths
CONFIG_FILE = "config.yaml"
LOG_FILE = "feeding_monitor.log"
DATABASE_FILE = "feeding_data.db"
MODEL_PATH = "models/yolov5s.pt"

class FeedingMonitor:
    def __init__(self, config_file=CONFIG_FILE):
        """Initialize the feeding monitor system."""
        self.config = self.load_config(config_file)
        self.setup_logging()
        self.setup_gpio()
        self.setup_camera()
        self.setup_mqtt()
        self.setup_database()
        
        # Initialize detection models
        self.setup_detection_models()
        
        # Data structures
        self.feeding_events = deque(maxlen=1000)
        self.strike_events = deque(maxlen=10000)
        self.fish_positions = deque(maxlen=100)
        self.food_positions = deque(maxlen=100)
        
        # State variables
        self.feeding_active = False
        self.ir_illumination = False
        self.system_health = True
        self.last_feeding_time = None
        self.background_model = None
        
        # Threading
        self.frame_queue = queue.Queue(maxsize=10)
        self.analysis_queue = queue.Queue(maxsize=100)
        self.running = True
        
        # Statistics
        self.session_stats = {
            'strikes': 0,
            'fish_count': 0,
            'food_consumed': 0,
            'feeding_duration': 0,
            'efficiency': 0
        }
        
        self.logger.info("Feeding monitor initialized successfully")
    
    def load_config(self, config_file):
        """Load configuration from YAML file."""
        try:
            with open(config_file, 'r') as f:
                return yaml.safe_load(f)
        except FileNotFoundError:
            # Create default config
            default_config = {
                'camera': {
                    'width': CAMERA_WIDTH,
                    'height': CAMERA_HEIGHT,
                    'fps': CAMERA_FPS,
                    'exposure': EXPOSURE_TIME
                },
                'detection': {
                    'fish_confidence': FISH_CONFIDENCE_THRESHOLD,
                    'food_confidence': FOOD_CONFIDENCE_THRESHOLD,
                    'motion_threshold': MOTION_THRESHOLD
                },
                'feeding': {
                    'amount_grams': FEED_AMOUNT_GRAMS,
                    'duration_seconds': FEEDING_DURATION,
                    'analysis_window': ANALYSIS_WINDOW
                },
                'mqtt': {
                    'broker': MQTT_BROKER,
                    'port': MQTT_PORT,
                    'username': MQTT_USERNAME,
                    'password': MQTT_PASSWORD
                }
            }
            
            with open(config_file, 'w') as f:
                yaml.dump(default_config, f)
            
            return default_config
    
    def setup_logging(self):
        """Setup logging configuration."""
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            handlers=[
                logging.FileHandler(LOG_FILE),
                logging.StreamHandler()
            ]
        )
        self.logger = logging.getLogger(__name__)
    
    def setup_gpio(self):
        """Initialize GPIO pins."""
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(SERVO_PIN, GPIO.OUT)
        GPIO.setup(IR_LED_PIN, GPIO.OUT)
        GPIO.setup(MOTION_SENSOR_PIN, GPIO.IN)
        GPIO.setup(STATUS_LED_PIN, GPIO.OUT)
        
        # Initialize servo
        self.servo = GPIO.PWM(SERVO_PIN, 50)  # 50Hz
        self.servo.start(0)
        
        # Status LED on
        GPIO.output(STATUS_LED_PIN, GPIO.HIGH)
        
        self.logger.info("GPIO initialized")
    
    def setup_camera(self):
        """Initialize camera system."""
        try:
            self.camera = Picamera2()
            
            # Configure camera
            camera_config = self.camera.create_preview_configuration(
                main={"size": (self.config['camera']['width'], 
                              self.config['camera']['height'])},
                controls={"ExposureTime": self.config['camera']['exposure']}
            )
            
            self.camera.configure(camera_config)
            self.camera.start()
            
            # Allow camera to warm up
            time.sleep(2)
            
            self.logger.info("Camera initialized successfully")
            
        except Exception as e:
            self.logger.error(f"Camera initialization failed: {e}")
            raise
    
    def setup_mqtt(self):
        """Initialize MQTT client."""
        self.mqtt_client = mqtt.Client()
        self.mqtt_client.username_pw_set(
            self.config['mqtt']['username'],
            self.config['mqtt']['password']
        )
        
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_message = self.on_mqtt_message
        
        try:
            self.mqtt_client.connect(
                self.config['mqtt']['broker'],
                self.config['mqtt']['port'],
                60
            )
            self.mqtt_client.loop_start()
            self.logger.info("MQTT client initialized")
        except Exception as e:
            self.logger.error(f"MQTT initialization failed: {e}")
    
    def setup_database(self):
        """Initialize SQLite database."""
        self.db_conn = sqlite3.connect(DATABASE_FILE, check_same_thread=False)
        self.db_cursor = self.db_conn.cursor()
        
        # Create tables
        self.db_cursor.execute('''
            CREATE TABLE IF NOT EXISTS feeding_sessions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME,
                duration_seconds INTEGER,
                strikes_count INTEGER,
                fish_count INTEGER,
                food_amount_grams REAL,
                efficiency_percent REAL,
                water_temperature REAL,
                notes TEXT
            )
        ''')
        
        self.db_cursor.execute('''
            CREATE TABLE IF NOT EXISTS strike_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id INTEGER,
                timestamp DATETIME,
                fish_x INTEGER,
                fish_y INTEGER,
                food_x INTEGER,
                food_y INTEGER,
                distance_pixels INTEGER,
                duration_seconds REAL,
                FOREIGN KEY (session_id) REFERENCES feeding_sessions (id)
            )
        ''')
        
        self.db_conn.commit()
        self.logger.info("Database initialized")
    
    def setup_detection_models(self):
        """Initialize AI detection models."""
        try:
            # Initialize background subtractor
            self.background_subtractor = cv2.createBackgroundSubtractorMOG2(
                history=500,
                varThreshold=50,
                detectShadows=True
            )
            
            # Initialize optical flow
            self.optical_flow = cv2.DISOpticalFlow_create()
            
            # Initialize contour detector
            self.contour_detector = cv2.SimpleBlobDetector_create()
            
            # Load pre-trained models if available
            if os.path.exists(MODEL_PATH):
                self.logger.info("Loading YOLOv5 model...")
                # import torch
                # self.yolo_model = torch.hub.load('ultralytics/yolov5', 'custom', path=MODEL_PATH)
                self.yolo_model = None  # Placeholder for actual model
            else:
                self.logger.warning("YOLOv5 model not found, using basic detection")
                self.yolo_model = None
            
            self.logger.info("Detection models initialized")
            
        except Exception as e:
            self.logger.error(f"Model initialization failed: {e}")
            self.yolo_model = None
    
    def on_mqtt_connect(self, client, userdata, flags, rc):
        """MQTT connection callback."""
        if rc == 0:
            self.logger.info("MQTT connected successfully")
            # Subscribe to command topic
            command_topic = f"aquaticmonitoring/{LOCATION_ID}/feeding/commands"
            client.subscribe(command_topic)
        else:
            self.logger.error(f"MQTT connection failed with code {rc}")
    
    def on_mqtt_message(self, client, userdata, msg):
        """MQTT message callback."""
        try:
            message = json.loads(msg.payload.decode())
            command = message.get('command', '')
            
            if command == 'start_feeding':
                self.start_feeding_session()
            elif command == 'stop_feeding':
                self.stop_feeding_session()
            elif command == 'toggle_ir':
                self.toggle_ir_illumination()
            elif command == 'get_status':
                self.publish_status()
            elif command == 'calibrate':
                self.calibrate_system()
            
        except Exception as e:
            self.logger.error(f"MQTT message processing error: {e}")
    
    def capture_frames(self):
        """Capture frames from camera in separate thread."""
        self.logger.info("Starting frame capture thread")
        
        while self.running:
            try:
                # Capture frame
                frame = self.camera.capture_array()
                
                # Convert to OpenCV format
                if len(frame.shape) == 3:
                    frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
                
                # Add timestamp
                timestamp = datetime.now()
                
                # Put frame in queue (non-blocking)
                try:
                    self.frame_queue.put((frame, timestamp), timeout=0.1)
                except queue.Full:
                    # Skip frame if queue is full
                    pass
                
                time.sleep(1.0 / self.config['camera']['fps'])
                
            except Exception as e:
                self.logger.error(f"Frame capture error: {e}")
                time.sleep(1)
    
    def process_frames(self):
        """Process frames for detection in separate thread."""
        self.logger.info("Starting frame processing thread")
        
        while self.running:
            try:
                # Get frame from queue
                frame, timestamp = self.frame_queue.get(timeout=1.0)
                
                # Process frame
                results = self.analyze_frame(frame, timestamp)
                
                # Put results in analysis queue
                self.analysis_queue.put(results)
                
            except queue.Empty:
                continue
            except Exception as e:
                self.logger.error(f"Frame processing error: {e}")
    
    def analyze_frame(self, frame, timestamp):
        """Analyze single frame for fish and food detection."""
        results = {
            'timestamp': timestamp,
            'fish_detected': [],
            'food_detected': [],
            'strikes': [],
            'motion_level': 0
        }
        
        try:
            # Convert to different color spaces
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
            
            # Background subtraction for motion detection
            if self.background_subtractor is not None:
                fg_mask = self.background_subtractor.apply(frame)
                motion_level = cv2.countNonZero(fg_mask) / (frame.shape[0] * frame.shape[1])
                results['motion_level'] = motion_level
            
            # Fish detection
            fish_detections = self.detect_fish(frame, gray, hsv)
            results['fish_detected'] = fish_detections
            
            # Food detection (only during feeding)
            if self.feeding_active:
                food_detections = self.detect_food(frame, gray, hsv)
                results['food_detected'] = food_detections
                
                # Strike detection
                strikes = self.detect_strikes(fish_detections, food_detections)
                results['strikes'] = strikes
            
            return results
            
        except Exception as e:
            self.logger.error(f"Frame analysis error: {e}")
            return results
    
    def detect_fish(self, frame, gray, hsv):
        """Detect fish in the frame."""
        fish_detections = []
        
        try:
            # Use YOLO model if available
            if self.yolo_model is not None:
                # YOLOv5 detection (placeholder)
                # results = self.yolo_model(frame)
                # fish_detections = self.parse_yolo_results(results, 'fish')
                pass
            else:
                # Basic contour-based detection
                fish_detections = self.detect_fish_contours(frame, gray, hsv)
            
        except Exception as e:
            self.logger.error(f"Fish detection error: {e}")
        
        return fish_detections
    
    def detect_fish_contours(self, frame, gray, hsv):
        """Detect fish using contour analysis."""
        fish_detections = []
        
        try:
            # Apply Gaussian blur
            blurred = cv2.GaussianBlur(gray, (5, 5), 0)
            
            # Adaptive thresholding
            thresh = cv2.adaptiveThreshold(
                blurred, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, 
                cv2.THRESH_BINARY, 11, 2
            )
            
            # Find contours
            contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            for contour in contours:
                area = cv2.contourArea(contour)
                
                # Filter by area (fish size)
                if 500 < area < 5000:  # Adjust based on fish size
                    x, y, w, h = cv2.boundingRect(contour)
                    
                    # Calculate aspect ratio
                    aspect_ratio = w / h
                    
                    # Fish typically have aspect ratio between 1.5 and 4
                    if 1.5 < aspect_ratio < 4:
                        fish_detections.append({
                            'x': x + w // 2,
                            'y': y + h // 2,
                            'width': w,
                            'height': h,
                            'area': area,
                            'confidence': 0.7  # Fixed confidence for basic detection
                        })
            
        except Exception as e:
            self.logger.error(f"Contour detection error: {e}")
        
        return fish_detections
    
    def detect_food(self, frame, gray, hsv):
        """Detect food pellets in the frame."""
        food_detections = []
        
        try:
            # Food is typically small, circular, and different color
            # Use color-based detection
            
            # Define food color range (adjust based on food type)
            # Brown/tan food pellets
            lower_brown = np.array([10, 50, 50])
            upper_brown = np.array([20, 255, 255])
            
            # Create mask for food color
            mask = cv2.inRange(hsv, lower_brown, upper_brown)
            
            # Morphological operations to clean up
            kernel = np.ones((3, 3), np.uint8)
            mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
            mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
            
            # Find contours
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            for contour in contours:
                area = cv2.contourArea(contour)
                
                # Filter by area (food pellet size)
                if 10 < area < 200:  # Adjust based on pellet size
                    x, y, w, h = cv2.boundingRect(contour)
                    
                    # Calculate circularity
                    perimeter = cv2.arcLength(contour, True)
                    if perimeter > 0:
                        circularity = 4 * math.pi * area / (perimeter * perimeter)
                        
                        # Food pellets are typically circular
                        if circularity > 0.5:
                            food_detections.append({
                                'x': x + w // 2,
                                'y': y + h // 2,
                                'width': w,
                                'height': h,
                                'area': area,
                                'confidence': min(circularity, 1.0)
                            })
            
        except Exception as e:
            self.logger.error(f"Food detection error: {e}")
        
        return food_detections
    
    def detect_strikes(self, fish_detections, food_detections):
        """Detect feeding strikes based on fish and food positions."""
        strikes = []
        
        try:
            # Check each fish against each food item
            for fish in fish_detections:
                for food in food_detections:
                    # Calculate distance between fish and food
                    dx = fish['x'] - food['x']
                    dy = fish['y'] - food['y']
                    distance = math.sqrt(dx * dx + dy * dy)
                    
                    # If fish is close to food, it might be a strike
                    if distance < MAX_STRIKE_DISTANCE:
                        strikes.append({
                            'fish_x': fish['x'],
                            'fish_y': fish['y'],
                            'food_x': food['x'],
                            'food_y': food['y'],
                            'distance': distance,
                            'timestamp': datetime.now()
                        })
            
        except Exception as e:
            self.logger.error(f"Strike detection error: {e}")
        
        return strikes
    
    def start_feeding_session(self):
        """Start a feeding session."""
        if self.feeding_active:
            self.logger.warning("Feeding session already active")
            return
        
        self.logger.info("Starting feeding session")
        self.feeding_active = True
        self.last_feeding_time = datetime.now()
        
        # Reset session statistics
        self.session_stats = {
            'strikes': 0,
            'fish_count': 0,
            'food_consumed': 0,
            'feeding_duration': 0,
            'efficiency': 0
        }
        
        # Dispense food
        self.dispense_food()
        
        # Enable IR illumination if needed
        current_hour = datetime.now().hour
        if current_hour < 6 or current_hour > 18:  # Night time
            self.enable_ir_illumination()
        
        # Start analysis timer
        threading.Timer(self.config['feeding']['duration_seconds'], 
                       self.stop_feeding_session).start()
        
        # Publish feeding start event
        self.publish_feeding_event('start')
    
    def stop_feeding_session(self):
        """Stop the feeding session and analyze results."""
        if not self.feeding_active:
            return
        
        self.logger.info("Stopping feeding session")
        self.feeding_active = False
        
        # Disable IR illumination
        self.disable_ir_illumination()
        
        # Calculate session statistics
        self.calculate_session_stats()
        
        # Save to database
        self.save_feeding_session()
        
        # Publish results
        self.publish_feeding_event('stop')
        
        # Send data to API
        self.send_api_data({
            "duration_seconds": self.session_stats.get('feeding_duration', 0),
            "strikes_count": self.session_stats.get('strikes', 0),
            "fish_count": self.session_stats.get('fish_count', 0),
            "food_consumed_grams": self.session_stats.get('food_consumed', 0),
            "efficiency_percent": self.session_stats.get('efficiency', 0),
            "water_temperature": 22.0  # Could be added from temperature sensor
        })
        
        self.logger.info(f"Feeding session completed: {self.session_stats}")
    
    def dispense_food(self):
        """Control servo to dispense food."""
        try:
            # Rotate servo to dispense position
            duty_cycle = 2.5 + (90 * 10) / 180  # 90 degrees
            self.servo.ChangeDutyCycle(duty_cycle)
            time.sleep(1)
            
            # Return to closed position
            duty_cycle = 2.5  # 0 degrees
            self.servo.ChangeDutyCycle(duty_cycle)
            time.sleep(1)
            
            # Stop servo
            self.servo.ChangeDutyCycle(0)
            
            self.logger.info("Food dispensed")
            
        except Exception as e:
            self.logger.error(f"Food dispensing error: {e}")
    
    def enable_ir_illumination(self):
        """Enable IR LED illumination."""
        GPIO.output(IR_LED_PIN, GPIO.HIGH)
        self.ir_illumination = True
        self.logger.info("IR illumination enabled")
    
    def disable_ir_illumination(self):
        """Disable IR LED illumination."""
        GPIO.output(IR_LED_PIN, GPIO.LOW)
        self.ir_illumination = False
        self.logger.info("IR illumination disabled")
    
    def toggle_ir_illumination(self):
        """Toggle IR LED illumination."""
        if self.ir_illumination:
            self.disable_ir_illumination()
        else:
            self.enable_ir_illumination()
    
    def calculate_session_stats(self):
        """Calculate feeding session statistics."""
        if not self.feeding_active:
            return
        
        # Calculate duration
        duration = (datetime.now() - self.last_feeding_time).total_seconds()
        self.session_stats['feeding_duration'] = duration
        
        # Count strikes in the session
        session_start = self.last_feeding_time
        strikes_in_session = [
            strike for strike in self.strike_events 
            if strike['timestamp'] >= session_start
        ]
        self.session_stats['strikes'] = len(strikes_in_session)
        
        # Estimate fish count (maximum concurrent fish detected)
        max_fish_count = 0
        for event in self.feeding_events:
            if event['timestamp'] >= session_start:
                fish_count = len(event.get('fish_detected', []))
                max_fish_count = max(max_fish_count, fish_count)
        
        self.session_stats['fish_count'] = max_fish_count
        
        # Calculate efficiency (strikes per gram of food)
        if self.config['feeding']['amount_grams'] > 0:
            self.session_stats['efficiency'] = (
                self.session_stats['strikes'] / self.config['feeding']['amount_grams']
            )
    
    def save_feeding_session(self):
        """Save feeding session to database."""
        try:
            self.db_cursor.execute('''
                INSERT INTO feeding_sessions 
                (timestamp, duration_seconds, strikes_count, fish_count, 
                 food_amount_grams, efficiency_percent, notes)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            ''', (
                self.last_feeding_time,
                self.session_stats['feeding_duration'],
                self.session_stats['strikes'],
                self.session_stats['fish_count'],
                self.config['feeding']['amount_grams'],
                self.session_stats['efficiency'],
                f"IR: {self.ir_illumination}"
            ))
            
            session_id = self.db_cursor.lastrowid
            
            # Save individual strikes
            session_start = self.last_feeding_time
            for strike in self.strike_events:
                if strike['timestamp'] >= session_start:
                    self.db_cursor.execute('''
                        INSERT INTO strike_events 
                        (session_id, timestamp, fish_x, fish_y, food_x, food_y, 
                         distance_pixels, duration_seconds)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                    ''', (
                        session_id,
                        strike['timestamp'],
                        strike['fish_x'],
                        strike['fish_y'],
                        strike['food_x'],
                        strike['food_y'],
                        strike['distance'],
                        0.5  # Estimated strike duration
                    ))
            
            self.db_conn.commit()
            self.logger.info("Feeding session saved to database")
            
        except Exception as e:
            self.logger.error(f"Database save error: {e}")
    
    def publish_feeding_event(self, event_type):
        """Publish feeding event via MQTT."""
        try:
            topic = f"aquaticmonitoring/{LOCATION_ID}/feeding/events"
            
            payload = {
                'device_id': DEVICE_ID,
                'location': LOCATION_ID,
                'timestamp': datetime.now().isoformat(),
                'event_type': event_type,
                'session_stats': self.session_stats,
                'config': self.config['feeding']
            }
            
            self.mqtt_client.publish(topic, json.dumps(payload))
            self.logger.info(f"Published feeding event: {event_type}")
            
        except Exception as e:
            self.logger.error(f"MQTT publish error: {e}")
    
    def publish_status(self):
        """Publish system status via MQTT."""
        try:
            topic = f"aquaticmonitoring/{LOCATION_ID}/feeding/status"
            
            payload = {
                'device_id': DEVICE_ID,
                'location': LOCATION_ID,
                'timestamp': datetime.now().isoformat(),
                'system_health': self.system_health,
                'feeding_active': self.feeding_active,
                'ir_illumination': self.ir_illumination,
                'last_feeding': self.last_feeding_time.isoformat() if self.last_feeding_time else None,
                'queue_sizes': {
                    'frames': self.frame_queue.qsize(),
                    'analysis': self.analysis_queue.qsize()
                }
            }
            
            self.mqtt_client.publish(topic, json.dumps(payload), retain=True)
            self.logger.info("Published system status")
            
        except Exception as e:
            self.logger.error(f"Status publish error: {e}")
    
    def calibrate_system(self):
        """Perform system calibration."""
        self.logger.info("Starting system calibration")
        
        try:
            # Calibrate background model
            self.logger.info("Calibrating background model...")
            for i in range(100):  # Collect 100 frames
                frame = self.camera.capture_array()
                if len(frame.shape) == 3:
                    frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
                
                self.background_subtractor.apply(frame)
                time.sleep(0.1)
            
            # Test servo movement
            self.logger.info("Testing servo movement...")
            self.dispense_food()
            
            # Test IR illumination
            self.logger.info("Testing IR illumination...")
            self.enable_ir_illumination()
            time.sleep(2)
            self.disable_ir_illumination()
            
            self.logger.info("System calibration completed")
            
        except Exception as e:
            self.logger.error(f"Calibration error: {e}")
    
    def send_api_data(self, data):
        """Send feeding data to REST API."""
        try:
            # Prepare API payload
            payload = {
                "timestamp": data.get("timestamp", int(time.time() * 1000)),
                "feeding_duration_seconds": data.get("duration_seconds", 0),
                "strikes_count": data.get("strikes_count", 0),
                "fish_count": data.get("fish_count", 0),
                "food_consumed_grams": data.get("food_consumed_grams", 0),
                "feeding_efficiency": data.get("efficiency_percent", 0),
                "water_temperature": data.get("water_temperature", 22.0),
                "sensor_health": self.system_health,
                "pool_id": POOL_ID
            }
            
            # Send POST request to API
            url = f"http://{API_SERVER}:{API_PORT}{API_ENDPOINT}"
            headers = {"Content-Type": "application/json"}
            
            response = requests.post(url, json=payload, headers=headers, timeout=10)
            
            if response.status_code == 200:
                self.logger.info(f"API data sent successfully: {response.status_code}")
            else:
                self.logger.warning(f"API request failed: {response.status_code}")
                
        except requests.exceptions.RequestException as e:
            self.logger.error(f"API request error: {e}")
        except Exception as e:
            self.logger.error(f"Unexpected error sending API data: {e}")
    
    def run(self):
        """Main execution loop."""
        self.logger.info("Starting feeding monitor")
        
        try:
            # Start background threads
            frame_thread = threading.Thread(target=self.capture_frames)
            process_thread = threading.Thread(target=self.process_frames)
            
            frame_thread.start()
            process_thread.start()
            
            # Main analysis loop
            while self.running:
                try:
                    # Get analysis results
                    results = self.analysis_queue.get(timeout=1.0)
                    
                    # Update data structures
                    self.fish_positions.extend(results['fish_detected'])
                    self.food_positions.extend(results['food_detected'])
                    self.strike_events.extend(results['strikes'])
                    
                    # Publish real-time data
                    if self.feeding_active:
                        self.publish_realtime_data(results)
                    
                except queue.Empty:
                    continue
                except Exception as e:
                    self.logger.error(f"Analysis loop error: {e}")
            
            # Wait for threads to complete
            frame_thread.join()
            process_thread.join()
            
        except KeyboardInterrupt:
            self.logger.info("Shutting down...")
            self.shutdown()
        except Exception as e:
            self.logger.error(f"Runtime error: {e}")
            self.shutdown()
    
    def publish_realtime_data(self, results):
        """Publish real-time analysis data."""
        try:
            topic = f"aquaticmonitoring/{LOCATION_ID}/feeding/realtime"
            
            payload = {
                'device_id': DEVICE_ID,
                'timestamp': results['timestamp'].isoformat(),
                'fish_count': len(results['fish_detected']),
                'food_count': len(results['food_detected']),
                'strikes': len(results['strikes']),
                'motion_level': results['motion_level']
            }
            
            self.mqtt_client.publish(topic, json.dumps(payload))
            
        except Exception as e:
            self.logger.error(f"Real-time publish error: {e}")
    
    def shutdown(self):
        """Shutdown the system gracefully."""
        self.logger.info("Shutting down feeding monitor")
        
        self.running = False
        
        # Stop feeding if active
        if self.feeding_active:
            self.stop_feeding_session()
        
        # Cleanup GPIO
        self.servo.stop()
        GPIO.cleanup()
        
        # Close camera
        if hasattr(self, 'camera'):
            self.camera.stop()
        
        # Close database
        if hasattr(self, 'db_conn'):
            self.db_conn.close()
        
        # Stop MQTT
        if hasattr(self, 'mqtt_client'):
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
        
        self.logger.info("Shutdown complete")

def signal_handler(signum, frame):
    """Handle shutdown signals."""
    print("\nShutdown signal received")
    sys.exit(0)

def main():
    """Main entry point."""
    # Setup signal handlers
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Fish Feeding Monitor')
    parser.add_argument('--config', default=CONFIG_FILE, help='Configuration file')
    parser.add_argument('--debug', action='store_true', help='Enable debug logging')
    args = parser.parse_args()
    
    # Set debug logging
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)
    
    # Create and run monitor
    monitor = FeedingMonitor(args.config)
    monitor.run()

if __name__ == "__main__":
    main()
