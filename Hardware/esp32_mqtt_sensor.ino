/*
 * ESmart Humidity Monitoring Digital Twin
 * 
 * Reads DHT22 sensor data from GPIO27, runs a trained linear regression
 * model for humidity prediction and anomaly detection, then publishes
 * JSON telemetry to an MQTT broker.
 * 
 * Required Libraries (install via Arduino Library Manager):
 *   - DHT sensor library by Adafruit
 *   - Adafruit Unified Sensor
 *   - PubSubClient by Nick O'Leary
 * 
 * Board: ESP32 Dev Module (or your specific ESP32 variant)
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <time.h>
#include <math.h>

// ============================================================
//  USER CONFIGURATION — Update these before uploading
// ============================================================

// WiFi credentials
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// MQTT broker settings
#define MQTT_BROKER     "YOUR_BROKER_ADDRESS"  // Public broker for testing
#define MQTT_PORT       1883
#define MQTT_TOPIC      "digitaltwin/humidity/data"
#define MQTT_CLIENT_ID  "humidityTwin01_ESP32"
#define MQTT_USERNAME   ""
#define MQTT_PASSWORD   ""

// Device identity
#define DEVICE_ID       "humidityTwin01"

// DHT22 sensor pin
#define DHT_PIN         27
#define DHT_TYPE        DHT22

// Sampling interval (milliseconds)
#define SAMPLE_INTERVAL 2000  // 2 seconds

// NTP server for timestamps
#define NTP_SERVER      "pool.ntp.org"
#define UTC_OFFSET_SEC  0     // UTC
#define DST_OFFSET_SEC  0

// ============================================================
//  ML MODEL COEFFICIENTS (from train_model.py)
// ============================================================
#define INTERCEPT       46.125165f
#define H_LAG_1_COEF   -0.000474f
#define H_LAG_2_COEF    0.009150f
#define H_LAG_3_COEF   -0.009703f
#define H_LAG_4_COEF   -0.022380f
#define H_LAG_5_COEF   -0.006806f
#define T_LAG_1_COEF    0.009196f

#define WINDOW_SIZE         5
#define ANOMALY_THRESHOLD   5.0f  // RH% deviation

// ============================================================
//  GLOBAL OBJECTS
// ============================================================
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);
DHT          dht(DHT_PIN, DHT_TYPE);

// History buffers for the ML model
float humidity_history[WINDOW_SIZE]    = {0};
float temperature_history[WINDOW_SIZE] = {0};
int   history_count = 0;

// Timing
unsigned long lastSampleTime = 0;

// ============================================================
//  ML MODEL — identical logic to ai_model.h
// ============================================================

typedef struct {
    float       prediction;
    float       error;
    int         is_anomaly;
    const char* trend;
    const char* status;
} AIResult;

void update_history(float humidity, float temperature) {
    for (int i = WINDOW_SIZE - 1; i > 0; i--) {
        humidity_history[i]    = humidity_history[i - 1];
        temperature_history[i] = temperature_history[i - 1];
    }
    humidity_history[0]    = humidity;
    temperature_history[0] = temperature;

    if (history_count < WINDOW_SIZE) history_count++;
}

AIResult analyze_data(float current_humidity, float current_temp) {
    AIResult result;
    result.prediction = 0;
    result.error      = 0;
    result.is_anomaly = 0;
    result.trend      = "STABLE";
    result.status     = "NORMAL";

    if (history_count < WINDOW_SIZE) {
        result.status = "INITIALIZING";
        update_history(current_humidity, current_temp);
        return result;
    }

    // Prediction using Linear Regression model
    result.prediction = INTERCEPT
        + (humidity_history[0] * H_LAG_1_COEF)
        + (humidity_history[1] * H_LAG_2_COEF)
        + (humidity_history[2] * H_LAG_3_COEF)
        + (humidity_history[3] * H_LAG_4_COEF)
        + (humidity_history[4] * H_LAG_5_COEF)
        + (temperature_history[0] * T_LAG_1_COEF);

    // Anomaly Detection
    result.error = fabs(current_humidity - result.prediction);
    if (result.error > ANOMALY_THRESHOLD) {
        result.is_anomaly = 1;
        result.status = "CRITICAL";
    } else if (result.error > ANOMALY_THRESHOLD * 0.6f) {
        result.status = "WARNING";
    }

    // Trend Analysis (short-term)
    float diff = current_humidity - humidity_history[0];
    if (diff > 0.2f)       result.trend = "INCREASING";
    else if (diff < -0.2f) result.trend = "DECREASING";

    // Update history for next call
    update_history(current_humidity, current_temp);

    return result;
}

// ============================================================
//  WIFI
// ============================================================

void connectWiFi() {
    Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] Connection FAILED — restarting...");
        ESP.restart();
    }
}

// ============================================================
//  NTP TIMESTAMP
// ============================================================

void initNTP() {
    configTime(UTC_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
    Serial.print("[NTP] Synchronizing time");
    struct tm timeinfo;
    int attempts = 0;
    while (!getLocalTime(&timeinfo) && attempts < 10) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println(" done");
}

void getISOTimestamp(char* buf, size_t len) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    } else {
        snprintf(buf, len, "1970-01-01T00:00:00Z");
    }
}

// ============================================================
//  MQTT
// ============================================================

void connectMQTT() {
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setBufferSize(512);  // Ensure enough room for JSON payload

    while (!mqttClient.connected()) {
        Serial.printf("[MQTT] Connecting to %s:%d ...\n", MQTT_BROKER, MQTT_PORT);

        bool connected;
        if (strlen(MQTT_USERNAME) > 0) {
            connected = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
        } else {
            connected = mqttClient.connect(MQTT_CLIENT_ID);
        }

        if (connected) {
            Serial.println("[MQTT] Connected!");
        } else {
            Serial.printf("[MQTT] Failed (rc=%d). Retrying in 3s...\n", mqttClient.state());
            delay(3000);
        }
    }
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  ESP32 Humidity Digital Twin Sensor");
    Serial.println("  Edge AI + MQTT Telemetry");
    Serial.println("========================================");
    Serial.println();

    // Initialize DHT22 sensor
    dht.begin();
    Serial.println("[DHT22] Sensor initialized on GPIO27");

    // Connect to WiFi
    connectWiFi();

    // Sync time via NTP
    initNTP();

    // Connect to MQTT broker
    connectMQTT();

    Serial.println();
    Serial.println("[READY] Starting telemetry loop...");
    Serial.println();
}

// ============================================================
//  MAIN LOOP
// ============================================================

void loop() {
    // Maintain MQTT connection
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();

    // Reconnect WiFi if dropped
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Lost connection — reconnecting...");
        connectWiFi();
        initNTP();
    }

    // Sample at the configured interval
    unsigned long now = millis();
    if (now - lastSampleTime < SAMPLE_INTERVAL) {
        return;
    }
    lastSampleTime = now;

    // ---- Read DHT22 ----
    float humidity    = dht.readHumidity();
    float temperature = dht.readTemperature();  // Celsius

    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("[DHT22] Sensor read error — skipping this cycle");
        return;
    }

    // ---- Run Edge AI Model ----
    AIResult ai = analyze_data(humidity, temperature);

    // ---- Generate ISO Timestamp ----
    char iso_time[32];
    getISOTimestamp(iso_time, sizeof(iso_time));

    // ---- Build JSON Payload ----
    char payload[400];
    snprintf(payload, sizeof(payload),
        "{"
            "\"timestamp\":\"%s\","
            "\"device_id\":\"%s\","
            "\"humidity\":%.2f,"
            "\"temperature\":%.2f,"
            "\"ml_prediction\":%.2f,"
            "\"anomaly_score\":%.4f,"
            "\"anomaly_status\":\"%s\","
            "\"trend\":\"%s\","
            "\"status\":\"ONLINE\""
        "}",
        iso_time,
        DEVICE_ID,
        humidity,
        temperature,
        ai.prediction,
        ai.error,
        ai.status,
        ai.trend
    );

    // ---- Publish to MQTT ----
    bool published = mqttClient.publish(MQTT_TOPIC, payload);

    // ---- Serial output for debugging ----
    Serial.println("---");
    Serial.printf("  Humidity: %.2f %%  |  Temp: %.2f °C\n", humidity, temperature);
    Serial.printf("  ML Pred:  %.2f    |  Anomaly: %.4f (%s)\n",
                  ai.prediction, ai.error, ai.status);
    Serial.printf("  Trend:    %s      |  MQTT: %s\n",
                  ai.trend, published ? "OK" : "FAIL");
    Serial.printf("  Topic:    %s\n", MQTT_TOPIC);
    Serial.println(payload);
    Serial.println("---");
}
