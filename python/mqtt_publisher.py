"""
CO326 - Smart Humidity Monitoring Digital Twin
MQTT Publisher — Edge Device Simulator

Simulates an ESP32-S3 edge device that:
  1. Reads sensor data from CSV (simulating DHT22 sensor)
  2. Runs a trained Linear Regression model for humidity prediction
  3. Performs anomaly detection and trend analysis
  4. Publishes structured JSON telemetry to MQTT broker using Sparkplug B UNS topics
  5. Listens for bidirectional commands (e.g., Dehumidifier ON/OFF)

MQTT Topics (Unified Namespace - Sparkplug B style):
  spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01        → telemetry data
  spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01/anomaly → anomaly alerts
  spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01/status  → device status (LWT)
  spBv1.0/SmartFactory/DCMD/HumidityMonitoring/humidityTwin01          → commands TO device
"""

import json
import time
import sys
import os
import signal
import threading
from datetime import datetime, timezone

import pandas as pd
import numpy as np
import joblib
import paho.mqtt.client as mqtt

# =============================================================================
# Configuration
# =============================================================================
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_CLIENT_ID = "humidityTwin01_publisher"

# Sparkplug B - Unified Namespace (UNS) Topics
TOPIC_DATA    = "spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01"
TOPIC_ANOMALY = "spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01/anomaly"
TOPIC_STATUS  = "spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01/status"
TOPIC_RUL     = "spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01/rul"
TOPIC_CMD     = "spBv1.0/SmartFactory/DCMD/HumidityMonitoring/humidityTwin01"

DEVICE_ID = "humidityTwin01"
CSV_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Project", "data_3hrs.csv")
MODEL_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Project", "humidity_model.joblib")

PUBLISH_INTERVAL = 2  # seconds between readings (simulating real sensor)
WINDOW_SIZE = 5
ANOMALY_THRESHOLD = 5.0  # RH% deviation to flag CRITICAL
QOS = 1  # MQTT Quality of Service

# =============================================================================
# Global State
# =============================================================================
humidity_history = []
temperature_history = []
anomaly_score_history = []  # For RUL estimation
dehumidifier_state = "OFF"  # Bidirectional control state
running = True

# Model coefficients (fallback if joblib model not found)
INTERCEPT = 46.125165
COEFFICIENTS = {
    'h_lag_1': -0.000474,
    'h_lag_2':  0.009150,
    'h_lag_3': -0.009703,
    'h_lag_4': -0.022380,
    'h_lag_5': -0.006806,
    't_lag_1':  0.009196,
}


# =============================================================================
# Edge AI Functions (mirrors ai_model.h logic)
# =============================================================================
def predict_humidity(h_history, t_history):
    """Linear regression prediction using hardcoded coefficients."""
    prediction = INTERCEPT
    for i, key in enumerate(['h_lag_1', 'h_lag_2', 'h_lag_3', 'h_lag_4', 'h_lag_5']):
        prediction += h_history[i] * COEFFICIENTS[key]
    prediction += t_history[0] * COEFFICIENTS['t_lag_1']
    return prediction


def analyze_data(humidity, temperature):
    """
    Edge AI analysis — mirrors the C ai_model.h logic.
    Returns a dict with prediction, anomaly score, status, and trend.
    """
    global humidity_history, temperature_history

    result = {
        "prediction": 0.0,
        "anomaly_score": 0.0,
        "anomaly_status": "NORMAL",
        "is_anomaly": False,
        "trend": "STABLE",
    }

    # Initialization phase — fill the history buffer
    if len(humidity_history) < WINDOW_SIZE:
        result["anomaly_status"] = "INITIALIZING"
        humidity_history.insert(0, humidity)
        temperature_history.insert(0, temperature)
        return result

    # 1. Prediction
    result["prediction"] = predict_humidity(humidity_history, temperature_history)

    # 2. Anomaly Detection
    error = abs(humidity - result["prediction"])
    result["anomaly_score"] = round(error, 4)

    if error > ANOMALY_THRESHOLD:
        result["anomaly_status"] = "CRITICAL"
        result["is_anomaly"] = True
    elif error > ANOMALY_THRESHOLD * 0.6:
        result["anomaly_status"] = "WARNING"
    else:
        result["anomaly_status"] = "NORMAL"

    # 3. Trend Analysis
    diff = humidity - humidity_history[0]
    if diff > 0.2:
        result["trend"] = "INCREASING"
    elif diff < -0.2:
        result["trend"] = "DECREASING"
    else:
        result["trend"] = "STABLE"

    # Update history (shift and insert new reading at front)
    humidity_history.insert(0, humidity)
    temperature_history.insert(0, temperature)
    if len(humidity_history) > WINDOW_SIZE:
        humidity_history.pop()
        temperature_history.pop()

    return result


def estimate_rul(anomaly_scores, threshold=ANOMALY_THRESHOLD, interval_sec=PUBLISH_INTERVAL):
    """
    Simple RUL (Remaining Useful Life) estimation using linear trend analysis.
    Estimates how many seconds until the anomaly score reaches CRITICAL threshold.
    """
    if len(anomaly_scores) < 10:
        return None  # Not enough data yet

    recent = anomaly_scores[-20:]  # Use last 20 readings
    x = np.arange(len(recent))
    coeffs = np.polyfit(x, recent, 1)  # Linear fit: slope, intercept
    slope = coeffs[0]
    current_level = recent[-1]

    if slope <= 0:
        return -1  # Score is decreasing or stable — no degradation

    # Estimate steps to reach threshold
    steps_to_threshold = (threshold - current_level) / slope
    if steps_to_threshold < 0:
        return 0  # Already past threshold

    rul_seconds = steps_to_threshold * interval_sec
    return round(rul_seconds, 1)


# =============================================================================
# MQTT Callbacks
# =============================================================================
def on_connect(client, userdata, flags, reason_code, properties):
    """Called when the client connects to the broker."""
    if reason_code == 0:
        print(f"[MQTT] Connected to broker at {MQTT_BROKER}:{MQTT_PORT}")
        # Subscribe to command topic for bidirectional control
        client.subscribe(TOPIC_CMD, qos=QOS)
        print(f"[MQTT] Subscribed to command topic: {TOPIC_CMD}")
        # Publish ONLINE status
        status_payload = json.dumps({
            "device_id": DEVICE_ID,
            "status": "ONLINE",
            "timestamp": datetime.now(timezone.utc).isoformat()
        })
        client.publish(TOPIC_STATUS, status_payload, qos=QOS, retain=True)
        print(f"[MQTT] Published ONLINE status")
    else:
        print(f"[MQTT] Connection failed with code: {reason_code}")


def on_disconnect(client, userdata, flags, reason_code, properties):
    """Called when disconnected from broker."""
    print(f"[MQTT] Disconnected (reason: {reason_code})")


def on_message(client, userdata, msg):
    """Handle incoming commands (bidirectional control)."""
    global dehumidifier_state
    try:
        payload = json.loads(msg.payload.decode())
        print(f"\n[CMD] Received command: {payload}")

        if "command" in payload:
            cmd = payload["command"].upper()
            if cmd in ("DEHUMIDIFIER_ON", "ACTIVATE_DEHUMIDIFIER"):
                dehumidifier_state = "ON"
                print(f"[CMD] Dehumidifier ACTIVATED")
            elif cmd in ("DEHUMIDIFIER_OFF", "DEACTIVATE_DEHUMIDIFIER"):
                dehumidifier_state = "OFF"
                print(f"[CMD] Dehumidifier DEACTIVATED")
            else:
                print(f"[CMD] Unknown command: {cmd}")

            # Acknowledge command
            ack = json.dumps({
                "device_id": DEVICE_ID,
                "command_ack": cmd,
                "dehumidifier_state": dehumidifier_state,
                "timestamp": datetime.now(timezone.utc).isoformat()
            })
            client.publish(TOPIC_STATUS, ack, qos=QOS)
    except Exception as e:
        print(f"[CMD] Error processing command: {e}")


# =============================================================================
# Signal Handler for Graceful Shutdown
# =============================================================================
def signal_handler(sig, frame):
    global running
    print("\n[SYSTEM] Shutting down gracefully...")
    running = False


# =============================================================================
# Main Publisher Loop
# =============================================================================
def main():
    global running, anomaly_score_history

    signal.signal(signal.SIGINT, signal_handler)

    # --- Load CSV Data ---
    if not os.path.exists(CSV_FILE):
        print(f"[ERROR] CSV file not found: {CSV_FILE}")
        sys.exit(1)

    print(f"[DATA] Loading sensor data from {CSV_FILE}")
    df = pd.read_csv(CSV_FILE, encoding='latin1')
    df.columns = ['timestamp_ms', 'humidity', 'temp_f', 'temp_c']
    print(f"[DATA] Loaded {len(df)} readings")

    # --- Setup MQTT Client ---
    client = mqtt.Client(
        client_id=MQTT_CLIENT_ID,
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        protocol=mqtt.MQTTv5
    )

    # Set Last Will and Testament (LWT)
    lwt_payload = json.dumps({
        "device_id": DEVICE_ID,
        "status": "OFFLINE",
        "timestamp": datetime.now(timezone.utc).isoformat()
    })
    client.will_set(TOPIC_STATUS, lwt_payload, qos=QOS, retain=True)

    # Set callbacks
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    # Connect to broker
    print(f"[MQTT] Connecting to {MQTT_BROKER}:{MQTT_PORT}...")
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
    except ConnectionRefusedError:
        print(f"[ERROR] Cannot connect to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")
        print(f"[ERROR] Make sure Docker stack is running: docker-compose up -d")
        sys.exit(1)

    # Start MQTT loop in background thread
    client.loop_start()
    time.sleep(1)  # Wait for connection

    # --- Main Publishing Loop ---
    print(f"\n{'='*60}")
    print(f"  Smart Humidity Monitoring — Edge Device Simulator")
    print(f"  Device ID: {DEVICE_ID}")
    print(f"  Publishing every {PUBLISH_INTERVAL}s to {MQTT_BROKER}")
    print(f"{'='*60}\n")

    reading_count = 0

    for idx, row in df.iterrows():
        if not running:
            break

        humidity = float(row['humidity'])
        temperature = float(row['temp_c'])

        # Run Edge AI analysis
        ai_result = analyze_data(humidity, temperature)

        # Track anomaly scores for RUL
        if ai_result["anomaly_status"] != "INITIALIZING":
            anomaly_score_history.append(ai_result["anomaly_score"])

        # Estimate RUL
        rul = estimate_rul(anomaly_score_history)

        # Build telemetry payload
        timestamp = datetime.now(timezone.utc).isoformat()
        payload = {
            "timestamp": timestamp,
            "device_id": DEVICE_ID,
            "humidity": round(humidity, 2),
            "temperature": round(temperature, 2),
            "ml_prediction": round(ai_result["prediction"], 2),
            "anomaly_score": ai_result["anomaly_score"],
            "anomaly_status": ai_result["anomaly_status"],
            "trend": ai_result["trend"],
            "dehumidifier_state": dehumidifier_state,
            "status": "ONLINE"
        }

        # Publish telemetry to main data topic
        payload_json = json.dumps(payload)
        result = client.publish(TOPIC_DATA, payload_json, qos=QOS)

        reading_count += 1
        status_icon = {
            "NORMAL": "[OK]",
            "WARNING": "[!!]",
            "CRITICAL": "[XX]",
            "INITIALIZING": "[..]"
        }.get(ai_result["anomaly_status"], "[??]")

        print(f"[{reading_count:4d}] {status_icon} H={humidity:.1f}% T={temperature:.1f}C "
              f"| Pred={ai_result['prediction']:.1f} Err={ai_result['anomaly_score']:.2f} "
              f"| {ai_result['anomaly_status']:12s} | {ai_result['trend']:10s} "
              f"| Dehum={dehumidifier_state}")

        # Publish anomaly alerts to separate topic
        if ai_result["anomaly_status"] in ("WARNING", "CRITICAL"):
            anomaly_payload = {
                "timestamp": timestamp,
                "device_id": DEVICE_ID,
                "anomaly_status": ai_result["anomaly_status"],
                "anomaly_score": ai_result["anomaly_score"],
                "humidity": round(humidity, 2),
                "ml_prediction": round(ai_result["prediction"], 2),
                "message": f"Humidity anomaly detected: {ai_result['anomaly_status']} "
                           f"(score: {ai_result['anomaly_score']:.2f})"
            }
            client.publish(TOPIC_ANOMALY, json.dumps(anomaly_payload), qos=QOS)

        # Publish RUL estimation
        if rul is not None:
            rul_payload = {
                "timestamp": timestamp,
                "device_id": DEVICE_ID,
                "rul_seconds": rul,
                "rul_status": "DEGRADING" if rul >= 0 else "HEALTHY",
                "rul_display": f"{rul:.0f}s" if rul >= 0 else "Healthy"
            }
            client.publish(TOPIC_RUL, json.dumps(rul_payload), qos=QOS)

        # Wait before next reading
        time.sleep(PUBLISH_INTERVAL)

    # --- Graceful Shutdown ---
    print(f"\n[SYSTEM] Published {reading_count} readings total.")

    # Publish OFFLINE status before disconnecting
    offline_payload = json.dumps({
        "device_id": DEVICE_ID,
        "status": "OFFLINE",
        "timestamp": datetime.now(timezone.utc).isoformat()
    })
    client.publish(TOPIC_STATUS, offline_payload, qos=QOS, retain=True)
    time.sleep(0.5)

    client.loop_stop()
    client.disconnect()
    print("[MQTT] Disconnected. Goodbye!")


if __name__ == "__main__":
    main()
