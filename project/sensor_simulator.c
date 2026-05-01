#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "ai_model.h"

#define CSV_FILE "data_3hrs.csv"
#define DEVICE_ID "humidityTwin01"

void get_iso_timestamp(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}

int main() {
    FILE *file = fopen(CSV_FILE, "r");
    if (!file) {
        perror("Error opening CSV file");
        return 1;
    }

    char line[256];
    // Skip header
    if (!fgets(line, sizeof(line), file)) {
        printf("Empty file\n");
        return 1;
    }

    printf("Starting Synthetic Sensor Simulator (ESP32 Simulation)...\n");
    printf("Press Ctrl+C to stop.\n\n");

    while (fgets(line, sizeof(line), file)) {
        long timestamp_ms;
        float humidity, temp_f, temp_c;

        // Parse CSV line (Timestamp, Humidity, TempF, TempC)
        if (sscanf(line, "%ld,%f,%f,%f", &timestamp_ms, &humidity, &temp_f, &temp_c) != 4) {
            continue;
        }

        // Run Edge AI Model
        AIResult ai = analyze_data(humidity, temp_c);

        // Generate ISO Timestamp
        char iso_time[32];
        get_iso_timestamp(iso_time, sizeof(iso_time));

        // Generate JSON Payload
        printf("{\n");
        printf("  \"timestamp\": \"%s\",\n", iso_time);
        printf("  \"device_id\": \"%s\",\n", DEVICE_ID);
        printf("  \"humidity\": %.2f,\n", humidity);
        printf("  \"temperature\": %.2f,\n", temp_c);
        printf("  \"ml_prediction\": %.2f,\n", ai.prediction);
        printf("  \"anomaly_score\": %.4f,\n", ai.error);
        printf("  \"anomaly_status\": \"%s\",\n", ai.status);
        printf("  \"trend\": \"%s\",\n", ai.trend);
        printf("  \"status\": \"ONLINE\"\n");
        printf("}\n");
        printf("---\n");

        // Simulate 2s interval
        // sleep(2); // Uncomment for real-time simulation
        usleep(500000); // 0.5s for faster demonstration
    }

    fclose(file);
    return 0;
}
