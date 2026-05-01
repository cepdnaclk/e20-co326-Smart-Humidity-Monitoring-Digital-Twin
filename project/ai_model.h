#ifndef AI_MODEL_H
#define AI_MODEL_H

#include <stdio.h>
#include <string.h>
#include <math.h>

// Model Coefficients (Extracted from Python training)
#define INTERCEPT 46.125165
#define H_LAG_1_COEF -0.000474
#define H_LAG_2_COEF 0.009150
#define H_LAG_3_COEF -0.009703
#define H_LAG_4_COEF -0.022380
#define H_LAG_5_COEF -0.006806
#define T_LAG_1_COEF 0.009196

#define WINDOW_SIZE 5
#define ANOMALY_THRESHOLD 5.0 // RH% deviation

typedef struct {
    float prediction;
    float error;
    int is_anomaly;
    const char* trend;
    const char* status;
} AIResult;

// Global buffer for historical values
float humidity_history[WINDOW_SIZE] = {0};
float temperature_history[WINDOW_SIZE] = {0};
int history_count = 0;

void update_history(float humidity, float temperature) {
    // Shift history
    for (int i = WINDOW_SIZE - 1; i > 0; i--) {
        humidity_history[i] = humidity_history[i - 1];
        temperature_history[i] = temperature_history[i - 1];
    }
    humidity_history[0] = humidity;
    temperature_history[0] = temperature;
    
    if (history_count < WINDOW_SIZE) history_count++;
}

AIResult analyze_data(float current_humidity, float current_temp) {
    AIResult result;
    result.prediction = 0;
    result.error = 0;
    result.is_anomaly = 0;
    result.trend = "STABLE";
    result.status = "NORMAL";

    if (history_count < WINDOW_SIZE) {
        result.status = "INITIALIZING";
        update_history(current_humidity, current_temp);
        return result;
    }

    // 1. Prediction using Linear Regression model
    // Y = Intercept + h1*w1 + h2*w2 + h3*w3 + h4*w4 + h5*w5 + t1*wt1
    result.prediction = INTERCEPT + 
                        (humidity_history[0] * H_LAG_1_COEF) + 
                        (humidity_history[1] * H_LAG_2_COEF) + 
                        (humidity_history[2] * H_LAG_3_COEF) + 
                        (humidity_history[3] * H_LAG_4_COEF) + 
                        (humidity_history[4] * H_LAG_5_COEF) +
                        (temperature_history[0] * T_LAG_1_COEF);

    // 2. Anomaly Detection
    result.error = fabs(current_humidity - result.prediction);
    if (result.error > ANOMALY_THRESHOLD) {
        result.is_anomaly = 1;
        result.status = "CRITICAL";
    } else if (result.error > ANOMALY_THRESHOLD * 0.6) {
        result.status = "WARNING";
    }

    // 3. Trend Analysis (Short-term)
    float diff = current_humidity - humidity_history[0];
    if (diff > 0.2) result.trend = "INCREASING";
    else if (diff < -0.2) result.trend = "DECREASING";

    // Update history for next call
    update_history(current_humidity, current_temp);

    return result;
}

#endif
