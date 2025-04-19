from crop_ai import get_crop_info_from_groq
from flask import Flask, request, jsonify, render_template, send_file
from datetime import datetime
import random
import os
import io
import csv
import re

app = Flask(__name__)

# In-memory storage for now (replace with Firebase or DB later)
sensor_data = {
    "soil_moisture": None,
    "temperature": None,
    "humidity": None,
}

irrigation_status = {"drip": False, "mist": False}
crop_selection = {"crop": "None"}

SENSOR_CSV_PATH = "data/sensor_log.csv"
os.makedirs("data", exist_ok=True)

# Create the CSV file if it doesn't exist already
if not os.path.exists(SENSOR_CSV_PATH):
    with open(SENSOR_CSV_PATH, mode='w', newline='') as file:
        writer = csv.DictWriter(file, fieldnames=['timestamp', 'temperature', 'humidity', 'soil_moisture'])
        writer.writeheader()

@app.route('/download-csv')
def download_csv():
    if not os.path.exists(SENSOR_CSV_PATH) or os.path.getsize(SENSOR_CSV_PATH) == 0:
        return "No data available to export.", 404
    
    return send_file(SENSOR_CSV_PATH, mimetype='text/csv', as_attachment=True, download_name='sensor_data.csv')

@app.route('/crop-info', methods=['GET'])
def crop_info():
    crop = request.args.get('crop')
    if not crop:
        return jsonify({"error": "Crop name is required"}), 400
    
    result = get_crop_info_from_groq(crop)
    if result:
        return jsonify({"crop": crop, "info": result})
    return jsonify({"error": "Failed to fetch crop info"}), 500

# Confirmed crop info
confirmed_crop_info = {
    "temperature": "20-30°C",
    "soil_moisture": "Moderate",
    "humidity": "50-70%"
}

# Endpoint to control irrigation
@app.route('/control', methods=['POST'])
def control_irrigation():
    data = request.json
    irrigation_status.update(data)
    return jsonify({"message": "Irrigation control updated", "status": irrigation_status}), 200

# Endpoint for crop selection
@app.route('/crop-selection', methods=['POST'])
def select_crop():
    data = request.json
    crop_selection.update(data)
    return jsonify({"message": "Crop selection updated", "crop": crop_selection}), 200

# Serve the website
@app.route("/")
def index():
    return render_template("index.html")

# Sensor data (including millis)
sensor_data = {
    "soil_moisture": None,
    "temperature": None,
    "humidity": None,
    "millis": 0
}

@app.route('/sensor-data', methods=['POST'])
def receive_sensor_data():
    data = request.json
    sensor_data.update(data)

    # Add timestamped row to CSV file
    timestamp = datetime.now().isoformat()
    row = {
        "timestamp": timestamp,
        "temperature": data.get("temperature"),
        "humidity": data.get("humidity"),
        "soil_moisture": data.get("soil_moisture")
    }
    with open(SENSOR_CSV_PATH, mode='a', newline='') as file:
        writer = csv.DictWriter(file, fieldnames=['timestamp', 'temperature', 'humidity', 'soil_moisture'])
        writer.writerow(row)

    return jsonify({"message": "Sensor data received", "data": sensor_data}), 200

@app.route('/sensor-data', methods=['GET'])
def get_sensor_data():
    return jsonify(sensor_data)

# Simulate pump state (replace with actual control logic)
pump_state = {"status": "off"}
manual_pump_active = False

@app.route('/control-pump', methods=['POST'])
def control_pump():
    global pump_state, manual_pump_active
    data = request.json
    action = data.get("action")
    duration = data.get("duration", 0)

    if action in ["on", "off"]:
        pump_state["status"] = action
        manual_pump_active = (action == "on")

        if action == "on" and duration > 0:
            from threading import Timer
            Timer(duration * 60, deactivate_manual_pump).start()

        return jsonify({"status": "success", "pump": pump_state["status"]})
    
    return jsonify({"status": "error", "message": "Invalid action"}), 400

def deactivate_manual_pump():
    global pump_state, manual_pump_active
    pump_state["status"] = "off"
    manual_pump_active = False

@app.route('/confirm_crop', methods=['POST'])
def confirm_crop():
    data = request.json
    # Check if any required value is "N/A"
    if any(data.get(k) == "N/A" for k in ["temperature", "soil_moisture", "humidity"]):
        return jsonify({"success": False, "message": "Parameters not valid"}), 400

    # Update confirmed info
    global confirmed_crop_info
    confirmed_crop_info = {
        "temperature": data["temperature"],
        "soil_moisture": data["soil_moisture"],
        "humidity": data["humidity"]
    }
    return jsonify({"success": True})

@app.route('/thresh_info')
def thresh_info():
    try:
        temp_range = confirmed_crop_info.get("temperature", "")
        humidity_range = confirmed_crop_info.get("humidity", "")
        soil_moisture_level = confirmed_crop_info.get("soil_moisture", "")

        # Extract temperature midpoint
        temp_nums = list(map(int, re.findall(r'\d+', temp_range)))
        temp_avg = sum(temp_nums) / len(temp_nums) if temp_nums else 25

        # Extract humidity midpoint
        humidity_nums = list(map(int, re.findall(r'\d+', humidity_range)))
        humidity_avg = sum(humidity_nums) / len(humidity_nums) if humidity_nums else 60

        # Base moisture levels
        moisture_base = {
            "Low": 35,
            "Moderate": 50,
            "High": 70
        }.get(soil_moisture_level, 50)

        # Calculate target moisture
        target = moisture_base + (temp_avg - 25) * 0.4 + (humidity_avg - 60) * 0.3
        target = round(max(30, min(target, 100)), 1)  # Clamp to 30–100%

        return jsonify({
            "target_moisture": moisture_base,
            "manual_pump_active": manual_pump_active
        })

    except Exception as e:
        print("Error in /thresh_info:", e)
        return jsonify({"error": "Failed to calculate moisture threshold"}), 500

if __name__ == "__main__":
    app.run(debug=True)
