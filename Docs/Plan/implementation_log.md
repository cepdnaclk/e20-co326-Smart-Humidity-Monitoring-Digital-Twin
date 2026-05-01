# Implementation Log — Phase 1, 2, 3 + Bug Fixes

> **Date:** April 30, 2026  
> **Status:** ✅ All systems verified working end-to-end

---

## What Was Implemented

### Phase 1: Python MQTT Publisher ✅
**File:** `python/mqtt_publisher.py`

Created a full Python MQTT publisher that replaces the C sensor simulator's stdout output with MQTT publishing:

- Reads `data_3hrs.csv` line by line (same as `sensor_simulator.c`)
- Runs the same Linear Regression model (hardcoded coefficients matching `ai_model.h`)
- Publishes to **Sparkplug B UNS topics** via `paho-mqtt` v2
- Features:
  - Last Will and Testament (LWT) — auto-publishes OFFLINE on disconnect
  - QoS 1 for reliable delivery
  - Bidirectional control — subscribes to DCMD topic, handles Dehumidifier ON/OFF commands
  - RUL estimation — linear trend analysis on anomaly scores
  - Separate anomaly alerts published to dedicated topic
  - Graceful shutdown on Ctrl+C

**Dependencies:** `python/requirements.txt` (paho-mqtt, pandas, numpy, scikit-learn, joblib)

---

### Phase 2: Node-RED Flows ✅
**File:** `node-red/flows.json`

Created importable Node-RED flows with 4 pipelines:

1. **Data Ingestion Pipeline:**
   - MQTT-in → JSON Parse → InfluxDB write
   - Subscribes to `spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01`
   - Formats data for InfluxDB 2.0 (measurement: `humidity_telemetry`)
   - Fields: humidity, temperature, ml_prediction, anomaly_score, dehumidifier_state, anomaly_status, trend, device_id, is_online

2. **Alert Pipeline:**
   - Switch node filters WARNING and CRITICAL anomaly statuses
   - Formats and publishes alerts to `...humidityTwin01/anomaly` topic
   - Also writes alerts to InfluxDB (measurement: `humidity_alerts`)

3. **RUL Estimation Pipeline:**
   - Function node with linear regression on anomaly score sliding window
   - Calculates steps to CRITICAL threshold
   - Writes to InfluxDB (measurement: `humidity_rul`)
   - Publishes to `...humidityTwin01/rul` MQTT topic

4. **Bidirectional Control Pipeline:**
   - Inject nodes for "Activate Dehumidifier" / "Deactivate Dehumidifier"
   - Publishes to DCMD topic → Python publisher receives and acknowledges

**Pre-configured connections:**
- Mosquitto broker: `mosquitto_broker:1883` (Docker container hostname)
- InfluxDB: `http://influxdb:8086`, org=`co326_org`, bucket=`co326_bucket`, token=`mytoken123456789`

**NOTE:** `node-red-contrib-influxdb` palette must be installed in Node-RED before importing flows.

---

### Phase 3: Grafana Dashboard ✅
**File:** `grafana/dashboard.json`

Created a SCADA-style Digital Twin dashboard with 10 panels in 3 rows:

**Row 1 — System Overview:**
| Panel | Type | Purpose |
|-------|------|---------|
| Device Status | Stat | ONLINE/OFFLINE indicator (green/red) |
| Current Humidity | Gauge | 0-100% with color thresholds |
| Current Temperature | Gauge | 0-50°C with color thresholds |
| Anomaly Score | Stat | Color-coded (green/yellow/red) |
| RUL Estimate | Stat | Remaining useful life in seconds |
| Dehumidifier State | Stat | ON/OFF indicator |

**Row 2 — Real-Time Monitoring:**
| Panel | Type | Purpose |
|-------|------|---------|
| Humidity — Actual vs ML Prediction | Time Series | Overlay chart with dashed prediction line |
| Temperature | Time Series | Live temperature readings |

**Row 3 — Edge AI Analytics:**
| Panel | Type | Purpose |
|-------|------|---------|
| Anomaly Score | Time Series (bars) | Bar chart with WARNING (3.0) and CRITICAL (5.0) threshold lines |
| RUL Estimation | Time Series | RUL trend over time |

**Auto-provisioning:**
- `Docker/co326/grafana/provisioning/datasources/influxdb.yaml` — auto-configures InfluxDB datasource
- `Docker/co326/grafana/provisioning/dashboards/dashboards.yaml` — auto-loads dashboard JSON
- `docker-compose.yaml` updated to mount dashboard JSON into Grafana container

---

### Phase 5: Documentation ✅

| File | Purpose |
|------|---------|
| `README.md` (root) | Project README with all mandatory sections |
| `docs/report.md` | 10-section technical report |
| `Docs/Plan/completion_plan.md` | Project completion plan |
| `Docs/Plan/implementation_log.md` | This file |

---

## Bug Fixes (Session 2 — April 30/May 1)

### Fix 1: Node-RED InfluxDB Data Format ✅
**Problem:** InfluxDB was receiving data but only storing a `measurement` field — all actual numeric fields (humidity, temperature, etc.) were missing.

**Root Cause:** The Node-RED function nodes were formatting `msg.payload` as the InfluxDB 1.x array format `[{measurement, tags, fields, timestamp}]`, but the InfluxDB out node with version `2.0` expects a simple key-value object `{field1: value1, field2: value2}` where the measurement name is set on the InfluxDB out node itself.

**Fix:** Updated all function nodes to output `msg.payload` as simple key-value objects instead of the array format.

### Fix 2: Node-RED InfluxDB Token Authentication ✅
**Problem:** InfluxDB was returning "unauthorized" errors from Node-RED.

**Root Cause:** The InfluxDB config node's token is stored in Node-RED's credentials system, not in the flows.json directly. Re-deploying flows via the API with the `credentials` field fixed this.

**Fix:** Re-deployed flows via Node-RED API with `credentials: {token: 'mytoken123456789'}` on the InfluxDB config node.

### Fix 3: Device Status Panel "No data" ✅
**Problem:** The Device Status panel was showing "No data" because the `device_status` measurement was empty.

**Root Cause:** The device status message is only published on connect/disconnect and was a retained MQTT message. When InfluxDB was cleared, the retained message had already been processed.

**Fix:** Added `is_online` field to the main `humidity_telemetry` measurement (written with every telemetry payload) and updated the Grafana panel query to use `humidity_telemetry.is_online` instead of `device_status.is_online`.

---

## Docker Compose Changes

Updated `Docker/co326/docker-compose.yaml`:
- Removed obsolete `version: '3.8'` key (was causing warnings)
- Added Grafana dashboard volume mount: `../../grafana/dashboard.json:/var/lib/grafana/dashboards/dashboard.json`
- Removed unused nodered custom nodes mount

---

## Files Created/Modified Summary

| File | Action | Phase |
|------|--------|-------|
| `python/mqtt_publisher.py` | Created | Phase 1 |
| `python/requirements.txt` | Created | Phase 1 |
| `node-red/flows.json` | Created + Fixed | Phase 2 |
| `grafana/dashboard.json` | Created + Fixed | Phase 3 |
| `Docker/co326/grafana/provisioning/datasources/influxdb.yaml` | Created | Phase 3 |
| `Docker/co326/grafana/provisioning/dashboards/dashboards.yaml` | Created | Phase 3 |
| `Docker/co326/docker-compose.yaml` | Modified | Phase 3 |
| `README.md` | Created | Phase 5 |
| `docs/report.md` | Created | Phase 5 |
| `Docs/Plan/completion_plan.md` | Created | Plan |
| `Docs/Plan/implementation_log.md` | Updated | Log |

---

## End-to-End Verification ✅

All components verified working:

1. ✅ Docker stack (Mosquitto, InfluxDB, Node-RED, Grafana) — all 4 containers running
2. ✅ Python MQTT publisher — data flowing, anomaly detection working, RUL estimation active
3. ✅ Node-RED flows — receiving MQTT data, writing to InfluxDB correctly
4. ✅ InfluxDB — all 4 measurements have correct fields with real data
5. ✅ Grafana dashboard — all 10 panels showing live data
6. ✅ Bidirectional control — Dehumidifier ON/OFF commands received and acknowledged
7. ✅ LWT — device status ONLINE indicator working

## Demo Steps

1. Start Docker Desktop
2. Run `docker-compose up -d` from `Docker/co326/`
3. Wait ~15 seconds for all containers to start
4. Import Node-RED flows from `node-red/flows.json` (if first time)
5. Run `python python/mqtt_publisher.py` from project root
6. Open Grafana at http://localhost:3000 (admin/admin)
7. Navigate to CO326 Digital Twin → Smart Humidity Monitoring — Digital Twin
8. Watch live data flowing
9. Test bidirectional control via Node-RED inject nodes at http://localhost:1880
10. Stop publisher (Ctrl+C) → Device Status goes OFFLINE
