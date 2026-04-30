# Smart Humidity Monitoring Digital Twin

This repository contains the GitHub documentation structure for the **CO326 Industrial Digital Twin & Cyber-Physical Security** project.

## Project idea

The system monitors humidity variation using an ESP32-S3 edge device. Humidity values are read or simulated, abnormal conditions are detected at the edge, telemetry is published using MQTT, data is processed in Node-RED, stored in InfluxDB, and visualized using Grafana.

## Technology stack

| Component | Technology | Purpose |
|---|---|---|
| Edge device | ESP32-S3 | Humidity reading/simulation and anomaly detection |
| Messaging | MQTT / Mosquitto | Edge-to-cloud telemetry transfer |
| Flow logic | Node-RED | Data processing, validation, routing |
| Historian | InfluxDB | Time-series data storage |
| Visualization | Grafana | Dashboard and digital twin-style view |
| Deployment | Docker | Containerized local deployment |

## Repository structure

```text
.
├── README.md
├── docs/
│   ├── architecture.md
│   ├── mqtt-topic-hierarchy.md
│   ├── node-red-flow.md
│   ├── dashboard-design.md
│   ├── ml-model-description.md
│   ├── cybersecurity-design.md
│   ├── rul-estimation.md
│   ├── wiring-diagram.md
│   ├── p-and-id.md
│   ├── testing-results.md
│   ├── docker-deployment.md
│   └── demo-checklist.md
├── edge/
├── node-red/
├── grafana/
└── docker/
```

## High-level data flow

```mermaid
flowchart LR
    A[Humidity Sensor / Simulation] --> B[ESP32-S3]
    B --> C[Mosquitto MQTT Broker]
    C --> D[Node-RED]
    D --> E[InfluxDB]
    E --> F[Grafana Dashboard]
```

## Current status

| Component | Status | Notes |
|---|---|---|
| ESP32-S3 firmware | In progress | Add final code in `/edge` |
| MQTT publishing | In progress | Follow `docs/mqtt-topic-hierarchy.md` |
| Node-RED flow | In progress | Export final flow to `/node-red/flows.json` |
| InfluxDB storage | In progress | Add bucket and measurement details |
| Grafana dashboard | In progress | Add dashboard JSON/screenshots |
| Testing | Pending | Fill `docs/testing-results.md` |

## Team members

| Name | Registration No. |
|---|---|
| Tharusha Haththella | E/20/133 |
| Sachindu Premasiri | E/20/305 |
| Nimesha Somathilaka | E/20/381 |
| Devin Sriyarathna | E/20/385 |
