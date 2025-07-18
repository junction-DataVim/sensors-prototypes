# API Integration Configuration
# Aquatic Monitoring System
#
# This file contains configuration for integrating all sensor modules 
# with the Aquaculture Pool Monitoring API

## API Server Configuration
API_SERVER = "your-api-server.com"
API_PORT = 3000
API_BASE_URL = f"http://{API_SERVER}:{API_PORT}/api"

## Pool Configuration
POOL_ID = 1  # Configure this for each installation

## API Endpoints for Each Sensor Type
API_ENDPOINTS = {
    "ph": "/api/ph-readings",
    "ammonia": "/api/ammonia-readings", 
    "nitrite": "/api/nitrite-readings",
    "nitrate": "/api/nitrate-readings",
    "dissolved_oxygen": "/api/dissolved-oxygen-readings",
    "orp": "/api/orp-readings",
    "salinity": "/api/salinity-readings",
    "temperature": "/api/temperature-readings",
    "turbidity": "/api/turbidity-readings",
    "water_level": "/api/water-level-readings",
    "toc": "/api/toc-readings",
    "fish_activity": "/api/fish-activity-readings",
    "feeding_response": "/api/feeding-response-readings",
    "water_purity": "/api/water-purity-readings"
}

## Expected API Payload Formats
# Based on the API documentation, here are the expected payload formats for each sensor:

API_PAYLOAD_FORMATS = {
    "ph": {
        "pool_id": "int",
        "ph_value": "decimal(4,2)",
        "notes": "string (optional)"
    },
    
    "ammonia": {
        "pool_id": "int", 
        "ammonia_ppm": "decimal(6,3)",
        "notes": "string (optional)"
    },
    
    "nitrite": {
        "pool_id": "int",
        "nitrite_ppm": "decimal(6,3)", 
        "notes": "string (optional)"
    },
    
    "nitrate": {
        "pool_id": "int",
        "nitrate_ppm": "decimal(6,2)",
        "notes": "string (optional)"
    },
    
    "dissolved_oxygen": {
        "pool_id": "int",
        "do_ppm": "decimal(5,2)",
        "do_percent_saturation": "decimal(5,1)",
        "notes": "string (optional)"
    },
    
    "orp": {
        "pool_id": "int",
        "orp_mv": "decimal(6,1)",
        "notes": "string (optional)"
    },
    
    "salinity": {
        "pool_id": "int",
        "salinity_ppt": "decimal(5,2)",
        "conductivity_us_cm": "decimal(8,1)",
        "tds_ppm": "decimal(8,1)",
        "notes": "string (optional)"
    },
    
    "temperature": {
        "pool_id": "int",
        "temperature_celsius": "decimal(4,1)",
        "notes": "string (optional)"
    },
    
    "turbidity": {
        "pool_id": "int",
        "turbidity_ntu": "decimal(5,2)",
        "notes": "string (optional)"
    },
    
    "water_level": {
        "pool_id": "int",
        "water_level_cm": "decimal(6,1)",
        "flow_rate_lpm": "decimal(8,2)",
        "notes": "string (optional)"
    },
    
    "toc": {
        "pool_id": "int",
        "toc_ppm": "decimal(6,2)",
        "bod_ppm": "decimal(6,2)",
        "notes": "string (optional)"
    },
    
    "fish_activity": {
        "pool_id": "int",
        "activity_level": "decimal(5,2)",
        "movement_count": "int",
        "average_speed": "decimal(5,2)",
        "notes": "string (optional)"
    },
    
    "feeding_response": {
        "pool_id": "int",
        "strike_rate_percent": "decimal(5,2)",
        "feeding_attempts": "int",
        "successful_strikes": "int",
        "response_time_seconds": "decimal(5,2)",
        "notes": "string (optional)"
    },
    
    "water_purity": {
        "pool_id": "int",
        "quality": "string",
        "good": "decimal(5,2)",
        "excellent": "decimal(5,2)",
        "poor": "decimal(5,2)",
        "notes": "string (optional)"
    }
}

## HTTP Response Codes
# 200 - Success (data retrieved)
# 201 - Created (new resource created)
# 400 - Bad Request (validation errors, missing required fields)
# 404 - Not Found (resource doesn't exist)
# 500 - Internal Server Error (database or server errors)

## SMS Alert Integration
# The API includes automated SMS alerting when sensor values exceed safe limits
# Alert thresholds are configured in the API server's config/sensor-limits.js

## Usage Examples in Arduino Code:
"""
// Example for pH sensor
HTTPClient http;
WiFiClient client;
String apiUrl = "http://your-api-server.com:3000/api/ph-readings";
http.begin(client, apiUrl);
http.addHeader("Content-Type", "application/json");

DynamicJsonDocument doc(256);
doc["pool_id"] = 1;
doc["ph_value"] = 7.2;
doc["notes"] = "Automated reading from pH sensor";

String payload;
serializeJson(doc, payload);
int httpResponseCode = http.POST(payload);
"""

## Usage Examples in Python Code:
"""
import requests

def send_to_api(sensor_data):
    api_url = "http://your-api-server.com:3000/api/fish-activity-readings"
    
    payload = {
        "pool_id": 1,
        "activity_level": sensor_data['activity_level'],
        "movement_count": sensor_data['movement_count'],
        "average_speed": sensor_data['average_speed'],
        "notes": "Automated reading from fish activity monitor"
    }
    
    response = requests.post(api_url, json=payload)
    
    if response.status_code in [200, 201]:
        print("Data sent successfully")
    else:
        print(f"API Error: {response.status_code}")
"""

## Configuration Steps:
# 1. Update API_SERVER and API_PORT with your actual server details
# 2. Set POOL_ID to match your pool configuration in the API database
# 3. Update each sensor module's configuration with the correct API endpoint
# 4. Ensure your API server is running and accessible from the sensor network
# 5. Configure SMS alerts in the API server if needed
# 6. Test API connectivity from each sensor module

## Network Security:
# - Use HTTPS in production environments
# - Implement proper authentication if required
# - Consider using API keys for additional security
# - Ensure proper firewall configuration

## Error Handling:
# - Implement retry logic for failed API calls
# - Log all API interactions for debugging
# - Provide fallback behavior when API is unavailable
# - Monitor API response times and success rates
