# CO326 Smart Humidity Monitoring Digital Twin — Completion Plan

> **Deadline:** Tomorrow (May 1, 2026) — Final Presentation  
> **Goal:** Complete the project with **minimum viable requirements** that cover all assessment criteria

---

## 📊 Current State and Requirements

### What's DONE ✅
| Component | Status | Details |
|-----------|--------|---------|
| Docker Compose | ✅ Done | Mosquitto, InfluxDB, Node-RED, Grafana all configured |
| Mosquitto Config | ✅ Done | `mosquitto.conf` with anonymous access |
| ML Model Training | ✅ Done | `train_model.py` — Linear Regression with 6 features |
| Edge AI Model | ✅ Done | `ai_model.h` — Hardcoded coefficients, anomaly detection, trend analysis |
| Sensor Simulator | ✅ Done | `sensor_simulator.c` — Reads CSV, runs AI model, outputs JSON |
| Dataset | ✅ Done | `data_3hrs.csv` — 5400 readings |

### What's MISSING ❌ (Gaps)
| Component | Priority | Assessment Weight | Effort |
|-----------|----------|-------------------|--------|
| **MQTT Publisher** (sensor→broker) | 🔴 Critical | System functionality (25%) | ~30 min |
| **Node-RED Flows** (MQTT→InfluxDB + RUL + alerts) | 🔴 Critical | Edge-Logic Layer (20%) | ~1.5 hr |
| **Grafana Dashboard** (Digital Twin) | 🔴 Critical | Digital Twin Sync (20%) + Dashboard (10%) | ~1.5 hr |
| **Sparkplug B / UNS topic structure** | 🟡 Medium | Network & UNS Design (20%) | ~20 min |
| **Bidirectional control** (dashboard → actuator) | 🟡 Medium | Digital Twin requirement | ~30 min |
| **RUL Estimation** (cloud-side) | 🟡 Medium | Cloud Analytics requirement | ~30 min |
| **Documentation** (README, report, diagrams) | 🟠 Important | Documentation (20%) | ~1 hr |
| **Cybersecurity** (MQTT auth, LWT) | 🟢 Nice-to-have | Security requirement | ~20 min |

---

## 🗺️ Implementation Roadmap (6 Phases)

> **Total estimated time: ~6–7 hours of focused work**

---

### Phase 1: Python MQTT Publisher (⏱️ ~30 min)
> **Goal:** Replace `stdout` JSON output with MQTT publishing

**What to do:**
1. Create a **Python-based MQTT publisher** (`python/mqtt_publisher.py`) that:
   - Reads `data_3hrs.csv` line by line (same logic as `sensor_simulator.c`)
   - Runs the same ML model (using `humidity_model.joblib`)
   - Publishes JSON payloads to Mosquitto via `paho-mqtt`
   - Uses **Sparkplug B-style UNS topics**

2. **MQTT Topic Structure (UNS):**
   ```
   spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01   → all telemetry data
   spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01/anomaly → anomaly alerts only
   spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01/status  → device status (LWT)
   spBv1.0/SmartFactory/DCMD/HumidityMonitoring/humidityTwin01   → commands TO the device (bidirectional)
   ```

3. **Key features to include:**
   - Last Will and Testament (LWT) → publishes `{"status": "OFFLINE"}` on disconnect
   - MQTT QoS 1 for reliability
   - JSON payloads with timestamps
   - Separate anomaly alerts to dedicated topic

**Files to create:**
- `python/mqtt_publisher.py` — Main MQTT publisher script
- `python/requirements.txt` — `paho-mqtt`, `pandas`, `numpy`, `scikit-learn`, `joblib`

---

### Phase 2: Node-RED Flows (⏱️ ~1.5 hr)
> **Goal:** Process MQTT data, store in InfluxDB, implement RUL, handle alerts

**What to do:**
1. **MQTT → InfluxDB Pipeline:**
   - MQTT-in node subscribing to `spBv1.0/SmartFactory/DDATA/HumidityMonitoring/#`
   - JSON parser function node
   - InfluxDB-out node writing to `co326_bucket`

2. **RUL Estimation (Function Node):**
   - Simple linear trend analysis over last N readings
   - Calculate rate of degradation (anomaly score trend)
   - Estimate time until anomaly score reaches CRITICAL threshold
   - Publish RUL to: `spBv1.0/SmartFactory/DDATA/HumidityMonitoring/humidityTwin01/rul`

3. **Alert Logic:**
   - Switch node: filter WARNING/CRITICAL anomaly statuses
   - Publish alerts to anomaly topic
   - Dashboard notification (Node-RED dashboard or Grafana)

4. **Bidirectional Control (Command):**
   - Node-RED dashboard button or Grafana panel → publishes to DCMD topic
   - Simulates: "Activate Dehumidifier" / "Deactivate Dehumidifier"
   - Python subscriber in `mqtt_publisher.py` listens for commands and acknowledges

**Node-RED palettes to install:**
- `node-red-contrib-influxdb` (for InfluxDB 2.x)

**Files to export:**
- `node-red/flows.json` — Export and save to repo

---

### Phase 3: Grafana Digital Twin Dashboard (⏱️ ~1.5 hr)
> **Goal:** Create a SCADA-style dashboard that acts as a Digital Twin

**Dashboard Panels:**

| Panel | Type | Data Source | Purpose |
|-------|------|-------------|---------|
| **Live Humidity** | Time Series | InfluxDB | Real-time humidity chart |
| **Live Temperature** | Time Series | InfluxDB | Real-time temperature chart |
| **ML Prediction vs Actual** | Time Series (overlay) | InfluxDB | Shows prediction accuracy |
| **Anomaly Score** | Time Series + Threshold | InfluxDB | Anomaly score with WARNING/CRITICAL lines |
| **Current Status** | Stat/Gauge | InfluxDB | Current anomaly status (color-coded) |
| **Humidity Gauge** | Gauge | InfluxDB | Current humidity level |
| **Trend Indicator** | Stat | InfluxDB | INCREASING/DECREASING/STABLE |
| **RUL Display** | Stat | InfluxDB | Remaining Useful Life estimate |
| **Dehumidifier Control** | Button (via MQTT plugin) | MQTT | Bidirectional control toggle |
| **Device Status** | Stat | InfluxDB/MQTT | ONLINE/OFFLINE indicator |

**Steps:**
1. Connect InfluxDB data source (already documented)
2. Create dashboard with above panels
3. Use Flux queries for each panel
4. Add alerting rules for CRITICAL status
5. Export dashboard JSON and save to `grafana/dashboard.json`

---

### Phase 4: MQTT Auth & Security (⏱️ ~20 min)
> **Goal:** Add minimal cybersecurity features

**What to do:**
1. **Mosquitto Authentication:**
   - Create password file
   - Update `mosquitto.conf` with `allow_anonymous false` and `password_file`
   - Update all clients with credentials

2. **LWT (Already planned in Phase 1)**

3. **Document the security design** in the report

> **Tip:** For the demo, you can keep `allow_anonymous true` and just document that authentication is supported. This saves time and avoids debugging auth issues during the live demo.

---

### Phase 5: Documentation (⏱️ ~1 hr)
> **Goal:** Create README, report, and diagrams

**5a. README.md** (root of repo)

**5b. Technical Report** (`docs/report.md` — 5-10 pages)

**5c. Diagrams:**
- System architecture diagram
- MQTT topic hierarchy tree
- Node-RED flow screenshot

---

### Phase 6: Testing & Demo Prep (⏱️ ~30 min)
> **Goal:** Verify end-to-end data flow and prepare for live demo

**Demo Script:**
1. Start Docker stack → `docker-compose up -d`
2. Show all 4 containers running
3. Run Python MQTT publisher → data starts flowing
4. Show Node-RED: MQTT messages arriving, being processed, stored to InfluxDB
5. Show Grafana dashboard: live updating charts, anomaly detection, RUL
6. Trigger bidirectional control: click "Activate Dehumidifier" → show command published
7. Show anomaly alert appearing on dashboard
8. Kill the publisher → show LWT "OFFLINE" status on dashboard

---

## 🎯 Priority Order (If Running Out of Time)

| Priority | Task | Why |
|----------|------|-----|
| 1️⃣ | Python MQTT Publisher | Without this, nothing flows through the system |
| 2️⃣ | Node-RED: MQTT→InfluxDB flow | Connects publisher to database |
| 3️⃣ | Grafana Dashboard (basic panels) | Shows data visually for demo |
| 4️⃣ | README.md | Mandatory deliverable |
| 5️⃣ | RUL in Node-RED | Cloud analytics requirement |
| 6️⃣ | Bidirectional control | Upgrades from "Digital Shadow" to "Digital Twin" |
| 7️⃣ | Full report | Important but can be concise |
| 8️⃣ | MQTT Auth/Security | Document it even if not fully implemented |

---

## 📁 Final Project Structure

```
Project/
├── Docker/co326/
│   ├── docker-compose.yaml
│   ├── mosquitto/config/mosquitto.conf
│   └── grafana/provisioning/
├── python/
│   ├── mqtt_publisher.py
│   ├── requirements.txt
│   └── data_3hrs.csv
├── Project/
│   ├── train_model.py
│   ├── ai_model.h
│   ├── sensor_simulator.c
│   ├── sensor_simulator.exe
│   ├── data_3hrs.csv
│   └── humidity_model.joblib
├── node-red/
│   └── flows.json
├── grafana/
│   └── dashboard.json
├── docs/
│   └── report.md
├── Docs/
│   ├── Plan/completion_plan.md
│   ├── Docker/implementation_upto_now.md
│   └── Project/README.md, document.md
└── README.md
```
