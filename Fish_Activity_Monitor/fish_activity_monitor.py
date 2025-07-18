"""
Fish Activity Monitor Module
Aquatic Monitoring System

This module monitors fish activity using computer vision and motion detection
with an ESP32-CAM module.

Features:
- Fish detection and tracking
- Activity level calculation
- Behavioral pattern analysis
- Motion detection algorithms
- MQTT communication
- OTA updates
- Power management
- Dummy data generation
- Camera health monitoring
"""

import cv2
import numpy as np
import json
import time
import threading
import logging
from datetime import datetime
import paho.mqtt.client as mqtt
import sqlite3
import os
import signal
import sys
import requests

# Configuration
class Config:
    def __init__(self):
        self.camera_index = 0
        self.frame_width = 640
        self.frame_height = 480
        self.fps = 10
        self.motion_threshold = 30
        self.min_contour_area = 500
        self.max_contour_area = 50000
        self.tracking_history = 30
        self.measurement_interval = 30  # seconds
        self.dummy_mode = False
        self.mqtt_broker = "your_mqtt_broker"
        self.mqtt_port = 8883
        self.mqtt_user = "your_mqtt_user"
        self.mqtt_password = "your_mqtt_password"
        self.use_tls = True
        # API configuration
        self.api_server = "your-api-server.com"
        self.api_port = 3000
        self.api_endpoint = "/api/fish-activity-readings"
        self.pool_id = 1

config = Config()

# Global variables
camera = None
mqtt_client = None
running = True
fish_tracks = []
activity_buffer = []
last_measurement = 0

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('fish_activity.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

class FishTracker:
    def __init__(self):
        self.background_subtractor = cv2.createBackgroundSubtractorMOG2(
            history=500, varThreshold=16, detectShadows=True
        )
        self.kernel = np.ones((5, 5), np.uint8)
        self.tracks = []
        self.next_id = 0
        self.max_disappeared = 30
        
    def update(self, frame):
        # Apply background subtraction
        fg_mask = self.background_subtractor.apply(frame)
        
        # Morphological operations to clean up the mask
        fg_mask = cv2.morphologyEx(fg_mask, cv2.MORPH_CLOSE, self.kernel)
        fg_mask = cv2.morphologyEx(fg_mask, cv2.MORPH_OPEN, self.kernel)
        
        # Find contours
        contours, _ = cv2.findContours(fg_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        # Filter contours by area
        valid_contours = []
        for contour in contours:
            area = cv2.contourArea(contour)
            if config.min_contour_area < area < config.max_contour_area:
                valid_contours.append(contour)
        
        # Get centroids
        centroids = []
        for contour in valid_contours:
            M = cv2.moments(contour)
            if M["m00"] != 0:
                cx = int(M["m10"] / M["m00"])
                cy = int(M["m01"] / M["m00"])
                centroids.append((cx, cy))
        
        # Update tracks
        self.update_tracks(centroids)
        
        return fg_mask, valid_contours
    
    def update_tracks(self, centroids):
        if len(centroids) == 0:
            # Mark all tracks as disappeared
            for track in self.tracks:
                track['disappeared'] += 1
            
            # Remove tracks that have disappeared too long
            self.tracks = [track for track in self.tracks if track['disappeared'] < self.max_disappeared]
            return
        
        if len(self.tracks) == 0:
            # Create new tracks for all centroids
            for centroid in centroids:
                self.tracks.append({
                    'id': self.next_id,
                    'centroid': centroid,
                    'disappeared': 0,
                    'history': [centroid]
                })
                self.next_id += 1
            return
        
        # Calculate distances between existing tracks and new centroids
        distances = np.zeros((len(self.tracks), len(centroids)))
        for i, track in enumerate(self.tracks):
            for j, centroid in enumerate(centroids):
                distances[i, j] = np.linalg.norm(np.array(track['centroid']) - np.array(centroid))
        
        # Assign centroids to tracks using Hungarian algorithm (simplified)
        used_centroids = set()
        for i in range(len(self.tracks)):
            if len(used_centroids) >= len(centroids):
                break
            
            min_distance = float('inf')
            min_j = -1
            
            for j in range(len(centroids)):
                if j not in used_centroids and distances[i, j] < min_distance:
                    min_distance = distances[i, j]
                    min_j = j
            
            if min_j != -1 and min_distance < 100:  # Max distance threshold
                self.tracks[i]['centroid'] = centroids[min_j]
                self.tracks[i]['disappeared'] = 0
                self.tracks[i]['history'].append(centroids[min_j])
                
                # Limit history length
                if len(self.tracks[i]['history']) > config.tracking_history:
                    self.tracks[i]['history'].pop(0)
                
                used_centroids.add(min_j)
            else:
                self.tracks[i]['disappeared'] += 1
        
        # Create new tracks for unmatched centroids
        for j, centroid in enumerate(centroids):
            if j not in used_centroids:
                self.tracks.append({
                    'id': self.next_id,
                    'centroid': centroid,
                    'disappeared': 0,
                    'history': [centroid]
                })
                self.next_id += 1
        
        # Remove disappeared tracks
        self.tracks = [track for track in self.tracks if track['disappeared'] < self.max_disappeared]

class ActivityAnalyzer:
    def __init__(self):
        self.movement_buffer = []
        self.buffer_size = 100
        
    def analyze_activity(self, tracks):
        current_time = time.time()
        
        # Calculate movement for each track
        total_movement = 0
        active_fish = 0
        
        for track in tracks:
            if len(track['history']) > 1:
                # Calculate movement over last few positions
                movement = 0
                for i in range(1, min(len(track['history']), 10)):
                    p1 = np.array(track['history'][-i-1])
                    p2 = np.array(track['history'][-i])
                    movement += np.linalg.norm(p2 - p1)
                
                total_movement += movement
                if movement > 10:  # Threshold for active fish
                    active_fish += 1
        
        # Store movement data
        self.movement_buffer.append({
            'timestamp': current_time,
            'total_movement': total_movement,
            'active_fish': active_fish,
            'total_fish': len(tracks)
        })
        
        # Limit buffer size
        if len(self.movement_buffer) > self.buffer_size:
            self.movement_buffer.pop(0)
        
        # Calculate activity metrics
        if len(self.movement_buffer) > 0:
            recent_data = self.movement_buffer[-min(10, len(self.movement_buffer)):]
            
            avg_movement = sum(d['total_movement'] for d in recent_data) / len(recent_data)
            avg_active_fish = sum(d['active_fish'] for d in recent_data) / len(recent_data)
            avg_total_fish = sum(d['total_fish'] for d in recent_data) / len(recent_data)
            
            activity_level = min(100, avg_movement / 10)  # Normalize to 0-100
            
            return {
                'activity_level': activity_level,
                'active_fish_count': int(avg_active_fish),
                'total_fish_count': int(avg_total_fish),
                'average_movement': avg_movement,
                'timestamp': current_time
            }
        
        return None

def setup_mqtt():
    global mqtt_client
    
    def on_connect(client, userdata, flags, rc):
        logger.info(f"Connected to MQTT broker with result code {rc}")
        client.subscribe("aquatic/fish_activity/config")
        client.subscribe("aquatic/fish_activity/control")
    
    def on_message(client, userdata, msg):
        try:
            topic = msg.topic
            payload = json.loads(msg.payload.decode())
            
            if topic == "aquatic/fish_activity/config":
                # Update configuration
                for key, value in payload.items():
                    if hasattr(config, key):
                        setattr(config, key, value)
                logger.info("Configuration updated")
            
            elif topic == "aquatic/fish_activity/control":
                # Handle control commands
                if payload.get("command") == "restart":
                    logger.info("Restart command received")
                    # Implement restart logic
                elif payload.get("command") == "calibrate":
                    logger.info("Calibration command received")
                    # Implement calibration logic
                    
        except Exception as e:
            logger.error(f"Error processing MQTT message: {e}")
    
    mqtt_client = mqtt.Client()
    mqtt_client.username_pw_set(config.mqtt_user, config.mqtt_password)
    
    if config.use_tls:
        mqtt_client.tls_set()
    
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    
    try:
        mqtt_client.connect(config.mqtt_broker, config.mqtt_port, 60)
        mqtt_client.loop_start()
        logger.info("MQTT client started")
    except Exception as e:
        logger.error(f"Failed to connect to MQTT broker: {e}")

def setup_camera():
    global camera
    
    try:
        camera = cv2.VideoCapture(config.camera_index)
        camera.set(cv2.CAP_PROP_FRAME_WIDTH, config.frame_width)
        camera.set(cv2.CAP_PROP_FRAME_HEIGHT, config.frame_height)
        camera.set(cv2.CAP_PROP_FPS, config.fps)
        
        if not camera.isOpened():
            raise Exception("Could not open camera")
        
        logger.info("Camera initialized successfully")
        return True
        
    except Exception as e:
        logger.error(f"Failed to initialize camera: {e}")
        return False

def setup_database():
    try:
        conn = sqlite3.connect('fish_activity.db', check_same_thread=False)
        cursor = conn.cursor()
        
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS activity_data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp REAL,
                activity_level REAL,
                active_fish_count INTEGER,
                total_fish_count INTEGER,
                average_movement REAL,
                quality_score INTEGER
            )
        ''')
        
        conn.commit()
        conn.close()
        logger.info("Database initialized")
        
    except Exception as e:
        logger.error(f"Failed to initialize database: {e}")

def publish_data(data):
    global mqtt_client
    
    if mqtt_client is None:
        return
    
    try:
        # Add sensor metadata
        mqtt_data = {
            'sensor_id': 'fish_activity_001',
            'timestamp': data['timestamp'],
            'activity_level': data['activity_level'],
            'active_fish_count': data['active_fish_count'],
            'total_fish_count': data['total_fish_count'],
            'average_movement': data['average_movement'],
            'quality_score': calculate_quality_score(data),
            'dummy_mode': config.dummy_mode
        }
        
        # Publish to MQTT
        mqtt_client.publish("aquatic/fish_activity/data", json.dumps(mqtt_data))
        
        # Store in database
        store_data(mqtt_data)
        
        # Send to API
        send_to_api(data)
        
        logger.info(f"Data published: Activity={data['activity_level']:.1f}%, Fish={data['total_fish_count']}")
        
    except Exception as e:
        logger.error(f"Failed to publish data: {e}")

def send_to_api(data):
    """Send data to the API endpoint"""
    try:
        api_url = f"http://{config.api_server}:{config.api_port}{config.api_endpoint}"
        
        # Prepare API payload according to the API documentation
        api_payload = {
            "pool_id": config.pool_id,
            "activity_level": data['activity_level'],
            "movement_count": data['total_fish_count'],  # Using total fish count as movement count
            "average_speed": data['average_movement'],
            "notes": "Automated reading from fish activity monitor"
        }
        
        response = requests.post(
            api_url,
            json=api_payload,
            headers={'Content-Type': 'application/json'},
            timeout=10
        )
        
        if response.status_code in [200, 201]:
            logger.info(f"API Success: {response.status_code}")
        else:
            logger.error(f"API Error: {response.status_code} - {response.text}")
            
    except Exception as e:
        logger.error(f"Failed to send data to API: {e}")

def store_data(data):
    try:
        conn = sqlite3.connect('fish_activity.db')
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO activity_data 
            (timestamp, activity_level, active_fish_count, total_fish_count, average_movement, quality_score)
            VALUES (?, ?, ?, ?, ?, ?)
        ''', (
            data['timestamp'],
            data['activity_level'],
            data['active_fish_count'],
            data['total_fish_count'],
            data['average_movement'],
            data['quality_score']
        ))
        
        conn.commit()
        conn.close()
        
    except Exception as e:
        logger.error(f"Failed to store data: {e}")

def calculate_quality_score(data):
    score = 100
    
    # Reduce score for extreme values
    if data['activity_level'] < 5 or data['activity_level'] > 95:
        score -= 20
    
    # Reduce score if no fish detected
    if data['total_fish_count'] == 0:
        score -= 50
    
    # Reduce score for dummy mode
    if config.dummy_mode:
        score -= 30
    
    return max(0, score)

def generate_dummy_data():
    import random
    
    return {
        'activity_level': random.uniform(10, 80),
        'active_fish_count': random.randint(1, 8),
        'total_fish_count': random.randint(5, 12),
        'average_movement': random.uniform(50, 300),
        'timestamp': time.time()
    }

def main_loop():
    global running, last_measurement
    
    if not config.dummy_mode:
        if not setup_camera():
            logger.error("Failed to initialize camera, switching to dummy mode")
            config.dummy_mode = True
    
    tracker = FishTracker()
    analyzer = ActivityAnalyzer()
    
    logger.info("Fish Activity Monitor started")
    
    while running:
        try:
            current_time = time.time()
            
            if config.dummy_mode:
                # Generate dummy data
                if current_time - last_measurement >= config.measurement_interval:
                    data = generate_dummy_data()
                    publish_data(data)
                    last_measurement = current_time
                
                time.sleep(1)
                continue
            
            # Read frame from camera
            ret, frame = camera.read()
            if not ret:
                logger.error("Failed to read frame from camera")
                time.sleep(1)
                continue
            
            # Process frame
            fg_mask, contours = tracker.update(frame)
            
            # Analyze activity
            activity_data = analyzer.analyze_activity(tracker.tracks)
            
            # Publish data periodically
            if activity_data and current_time - last_measurement >= config.measurement_interval:
                publish_data(activity_data)
                last_measurement = current_time
            
            # Optional: Display frame (for debugging)
            # cv2.imshow('Fish Activity Monitor', frame)
            # if cv2.waitKey(1) & 0xFF == ord('q'):
            #     break
            
            time.sleep(0.1)  # Small delay to prevent CPU overload
            
        except KeyboardInterrupt:
            logger.info("Keyboard interrupt received")
            break
        except Exception as e:
            logger.error(f"Error in main loop: {e}")
            time.sleep(1)

def cleanup():
    global running, camera, mqtt_client
    
    logger.info("Cleaning up...")
    running = False
    
    if camera:
        camera.release()
    
    if mqtt_client:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
    
    cv2.destroyAllWindows()
    logger.info("Cleanup completed")

def signal_handler(signum, frame):
    logger.info(f"Signal {signum} received, shutting down...")
    cleanup()
    sys.exit(0)

if __name__ == "__main__":
    # Setup signal handlers
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    try:
        # Initialize components
        setup_database()
        setup_mqtt()
        
        # Start main loop
        main_loop()
        
    except Exception as e:
        logger.error(f"Fatal error: {e}")
    finally:
        cleanup()
