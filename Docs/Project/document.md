# Project Implementations Summary

This document outlines the current implementations found in the workspace. The project consists of a Machine Learning model pipeline and an Edge AI Sensor Simulator written in C, simulating an ESP32 device.

## 1. Machine Learning Model Training (`train_model.py`)
This Python script is responsible for training a predictive model for humidity.
- **Algorithm:** Uses a `LinearRegression` model from `scikit-learn`.
- **Data Source:** Reads from `data_3hrs.csv`.
- **Features:** It uses a sliding window approach, taking the previous 5 humidity readings and the most recent temperature reading to predict the current humidity.
- **Output:**
  - Evaluates the model using Mean Squared Error (MSE).
  - Saves the trained model to `humidity_model.joblib`.
  - Prints the model coefficients (intercept and weights) to the console so they can be hardcoded into the C implementation for Edge deployment.

## 2. Edge AI Model Implementation (`ai_model.h`)
This C header file contains the logic for running the trained machine learning model on an edge device (e.g., a microcontroller).
- **Hardcoded Model:** The coefficients extracted from `train_model.py` are hardcoded as macros.
- **State Management:** Maintains a rolling history of the last 5 humidity and temperature readings.
- **`analyze_data` Function:** 
  - **Prediction:** Calculates the expected humidity based on the linear regression formula.
  - **Anomaly Detection:** Compares the actual humidity against the predicted humidity. If the absolute difference (error) exceeds a defined threshold (5.0), it flags the reading as an anomaly (`CRITICAL`). If it exceeds 60% of the threshold, it flags it as a `WARNING`.
  - **Trend Analysis:** Calculates the short-term trend (`INCREASING`, `DECREASING`, or `STABLE`) based on the most recent change in humidity.

## 3. Synthetic Sensor Simulator (`sensor_simulator.c`)
This C program simulates an IoT edge device (like an ESP32) reading sensor data and applying the edge AI model locally.
- **Data Ingestion:** Reads the historical dataset line-by-line from `data_3hrs.csv`.
- **Processing:** Passes the humidity and temperature values to the `analyze_data` function from `ai_model.h`.
- **Payload Generation:** Generates a structured JSON payload for each reading, which includes:
  - ISO timestamp
  - Device ID (`humidityTwin01`)
  - Raw humidity and temperature
  - ML Prediction
  - Anomaly Score (prediction error)
  - Anomaly Status (`NORMAL`, `WARNING`, or `CRITICAL`)
  - Trend (`INCREASING`, `DECREASING`, `STABLE`)
- **Simulation Delay:** Includes a delay (currently set to 0.5 seconds for faster demonstration) to simulate real-time sensor polling.

## Summary
In short, the implementation involves training a lightweight linear regression model in Python to predict humidity, porting that model to C using hardcoded weights, and simulating an edge IoT device that runs this model over historical data to perform real-time anomaly detection and trend analysis.
