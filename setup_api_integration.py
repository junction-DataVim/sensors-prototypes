#!/usr/bin/env python3
"""
Aquatic Monitoring System - API Integration Setup Script

This script helps configure all sensor modules to integrate with the 
Aquaculture Pool Monitoring API.

Usage:
    python3 setup_api_integration.py --server <server_url> --port <port> --pool <pool_id>

Example:
    python3 setup_api_integration.py --server 192.168.1.100 --port 3000 --pool 1
"""

import argparse
import os
import re
import sys
from pathlib import Path

# API endpoint mappings
API_ENDPOINTS = {
    "ph_sensor": "/api/ph-readings",
    "ammonia_sensor": "/api/ammonia-readings",
    "nitrite_nitrate_sensor": {
        "nitrite": "/api/nitrite-readings",
        "nitrate": "/api/nitrate-readings"
    },
    "dissolved_oxygen_sensor": "/api/dissolved-oxygen-readings",
    "orp_sensor": "/api/orp-readings",
    "salinity_conductivity_sensor": "/api/salinity-readings",
    "temperature_sensor": "/api/temperature-readings",
    "turbidity_sensor": "/api/turbidity-readings",
    "water_level_flow_sensor": "/api/water-level-readings",
    "toc_bod_sensor": "/api/toc-readings",
    "fish_activity_monitor": "/api/fish-activity-readings",
    "fish_feeding_monitor": "/api/feeding-response-readings"
}

def update_arduino_config(file_path, server, port, pool_id):
    """Update Arduino .ino files with API configuration"""
    if not os.path.exists(file_path):
        print(f"Warning: {file_path} not found")
        return
    
    try:
        with open(file_path, 'r') as f:
            content = f.read()
        
        # Update API server configuration
        content = re.sub(
            r'const char\* api_server = "[^"]*";',
            f'const char* api_server = "{server}";',
            content
        )
        
        content = re.sub(
            r'const int api_port = \d+;',
            f'const int api_port = {port};',
            content
        )
        
        content = re.sub(
            r'const int pool_id = \d+;',
            f'const int pool_id = {pool_id};',
            content
        )
        
        # Alternative patterns for different naming conventions
        content = re.sub(
            r'#define API_SERVER "[^"]*"',
            f'#define API_SERVER "{server}"',
            content
        )
        
        content = re.sub(
            r'#define API_PORT \d+',
            f'#define API_PORT {port}',
            content
        )
        
        content = re.sub(
            r'#define POOL_ID \d+',
            f'#define POOL_ID {pool_id}',
            content
        )
        
        with open(file_path, 'w') as f:
            f.write(content)
        
        print(f"Updated: {file_path}")
        
    except Exception as e:
        print(f"Error updating {file_path}: {e}")

def update_python_config(file_path, server, port, pool_id):
    """Update Python files with API configuration"""
    if not os.path.exists(file_path):
        print(f"Warning: {file_path} not found")
        return
    
    try:
        with open(file_path, 'r') as f:
            content = f.read()
        
        # Update API server configuration
        content = re.sub(
            r'self\.api_server = "[^"]*"',
            f'self.api_server = "{server}"',
            content
        )
        
        content = re.sub(
            r'self\.api_port = \d+',
            f'self.api_port = {port}',
            content
        )
        
        content = re.sub(
            r'self\.pool_id = \d+',
            f'self.pool_id = {pool_id}',
            content
        )
        
        with open(file_path, 'w') as f:
            f.write(content)
        
        print(f"Updated: {file_path}")
        
    except Exception as e:
        print(f"Error updating {file_path}: {e}")

def create_platformio_config(base_path, server, port, pool_id):
    """Create or update platformio.ini with API configuration"""
    platformio_path = os.path.join(base_path, "platformio.ini")
    
    # Check if file exists and read current content
    current_content = ""
    if os.path.exists(platformio_path):
        with open(platformio_path, 'r') as f:
            current_content = f.read()
    
    # Add build flags for API configuration
    api_flags = f'''
; API Configuration
build_flags = 
    -DAPI_SERVER='"{server}"'
    -DAPI_PORT={port}
    -DPOOL_ID={pool_id}
'''
    
    # If the file doesn't contain API configuration, add it
    if "-DAPI_SERVER" not in current_content:
        with open(platformio_path, 'a') as f:
            f.write(api_flags)
        print(f"Added API configuration to: {platformio_path}")

def test_api_connectivity(server, port):
    """Test if the API server is reachable"""
    try:
        import requests
        response = requests.get(f"http://{server}:{port}/api/pools", timeout=5)
        if response.status_code == 200:
            print(f"✓ API server is reachable at {server}:{port}")
            return True
        else:
            print(f"⚠ API server responded with status {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Cannot reach API server at {server}:{port}: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Setup API integration for Aquatic Monitoring System')
    parser.add_argument('--server', required=True, help='API server hostname or IP address')
    parser.add_argument('--port', type=int, default=3000, help='API server port (default: 3000)')
    parser.add_argument('--pool', type=int, default=1, help='Pool ID for this installation (default: 1)')
    parser.add_argument('--test', action='store_true', help='Test API connectivity')
    parser.add_argument('--dry-run', action='store_true', help='Show what would be updated without making changes')
    
    args = parser.parse_args()
    
    # Find the base directory (should contain sensor folders)
    base_dir = Path(__file__).parent
    
    if not base_dir.exists():
        print(f"Error: Base directory not found: {base_dir}")
        sys.exit(1)
    
    print(f"Configuring API integration...")
    print(f"Server: {args.server}")
    print(f"Port: {args.port}")
    print(f"Pool ID: {args.pool}")
    print(f"Base directory: {base_dir}")
    print()
    
    # Test API connectivity if requested
    if args.test:
        if not test_api_connectivity(args.server, args.port):
            response = input("API server is not reachable. Continue anyway? (y/N): ")
            if response.lower() != 'y':
                sys.exit(1)
    
    # Update sensor configurations
    sensor_configs = [
        ("pH_Sensor/ph_sensor.ino", "arduino"),
        ("Ammonia_Sensor/ammonia_sensor.ino", "arduino"),
        ("Nitrite_Nitrate_Sensor/nitrite_nitrate_sensor.ino", "arduino"),
        ("Dissolved_Oxygen_Sensor/dissolved_oxygen_sensor.ino", "arduino"),
        ("ORP_Sensor/orp_sensor.ino", "arduino"),
        ("Salinity_Conductivity_Sensor/conductivity_sensor.ino", "arduino"),
        ("Temperature_Sensor/temperature_sensor.ino", "arduino"),
        ("Turbidity_Sensor/turbidity_sensor.ino", "arduino"),
        ("Water_Level_Flow_Sensor/water_level_sensor.ino", "arduino"),
        ("TOC_BOD_Sensor/toc_sensor.ino", "arduino"),
        ("Fish_Activity_Monitor/fish_activity_monitor.py", "python"),
        ("Fish_Feeding_Monitor/feeding_detection.py", "python"),
    ]
    
    for config_file, file_type in sensor_configs:
        file_path = base_dir / config_file
        
        if args.dry_run:
            print(f"Would update: {file_path}")
            continue
        
        if file_type == "arduino":
            update_arduino_config(file_path, args.server, args.port, args.pool)
        elif file_type == "python":
            update_python_config(file_path, args.server, args.port, args.pool)
    
    if not args.dry_run:
        # Create/update platformio.ini
        create_platformio_config(base_dir, args.server, args.port, args.pool)
        
        # Create a configuration summary file
        config_summary = f"""# Aquatic Monitoring System - API Integration Configuration
# Generated on: {__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

API_SERVER = "{args.server}"
API_PORT = {args.port}
POOL_ID = {args.pool}

# All sensor modules have been configured to send data to:
# {args.server}:{args.port}

# API Endpoints:
{chr(10).join(f"# {sensor}: {endpoint}" for sensor, endpoint in API_ENDPOINTS.items())}

# Next Steps:
# 1. Verify API server is running and accessible
# 2. Upload firmware to ESP32 modules
# 3. Run Python scripts for fish monitoring
# 4. Monitor logs for successful API calls
# 5. Check API dashboard for incoming data
"""
        
        with open(base_dir / "api_config_summary.txt", 'w') as f:
            f.write(config_summary)
        
        print(f"\nConfiguration complete!")
        print(f"Summary saved to: {base_dir / 'api_config_summary.txt'}")
        print(f"\nNext steps:")
        print(f"1. Upload firmware to your ESP32 modules")
        print(f"2. Run Python scripts for fish monitoring")
        print(f"3. Monitor sensor logs for successful API calls")
        print(f"4. Check your API dashboard at http://{args.server}:{args.port}")

if __name__ == "__main__":
    main()
