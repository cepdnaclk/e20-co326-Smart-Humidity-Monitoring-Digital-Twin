#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "ai_model.h"

// CSV file that contains the simulated humidity and temperature data
#define CSV_FILE "data_3hrs.csv"

// Unique ID used to identify this simulated ESP32 / digital twin device
#define DEVICE_ID "humidityTwin01"

/*
 * Function to generate the current timestamp in ISO 8601 format.
 * Example format: 2026-05-01T10:30:45Z
 * This is useful when sending time-series data to MQTT, InfluxDB, or dashboards.
 */
void get_iso_timestamp(char *buf, size_t len) {
    time_t now = time(NULL);              // Get current system time
    struct tm *tm_info = gmtime(&now);    // Convert time to UTC format

    // Format the time as ISO 8601 string
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}

int main() {
    /*
     * Open the CSV file in read mode.
     * The CSV file acts as the synthetic sensor data source.
     */
    FILE *file = fopen(CSV_FILE, "r");

    // Check whether the file opened successfully
    if (!file) {
        perror("Error opening CSV file");
        return 1;
    }

    char line[256];

    /*
     * Read and skip the first line of the CSV file.
     * The first line is assumed to be the header.
     * Example: Timestamp,Humidity,TempF,TempC
     */
    if (!fgets(line, sizeof(line), file)) {
        printf("Empty file\n");
        return 1;
    }

    printf("Starting Synthetic Sensor Simulator (ESP32 Simulation)...\n");
    printf("Press Ctrl+C to stop.\n\n");

    /*
     * Read the CSV file line by line.
     * Each line represents one sensor reading.
     */
    while (fgets(line, sizeof(line), file)) {
        long timestamp_ms;
        float humidity, temp_f, temp_c;

        /*
         * Parse values from the CSV line.
         * Expected CSV format:
         * timestamp_ms,humidity,temp_f,temp_c
         *
         * Example:
         * 1000,65.4,77.2,25.1
         */
        if (sscanf(line, "%ld,%f,%f,%f",
                   &timestamp_ms, &humidity, &temp_f, &temp_c) != 4) {
            // If the line is invalid, skip it and move to the next line
            continue;
        }

        /*
         * Run the Edge AI model using humidity and temperature in Celsius.
         * The analyze_data() function returns:
         * - predicted value
         * - anomaly error/score
         * - anomaly status
         * - trend information
         */
        AIResult ai = analyze_data(humidity, temp_c);

        /*
         * Generate the current ISO timestamp.
         * This timestamp represents when the reading is processed/published.
         */
        char iso_time[32];
        get_iso_timestamp(iso_time, sizeof(iso_time));

        /*
         * Print the simulated MQTT/JSON payload.
         * In the real ESP32 implementation, this JSON payload can be published
         * to an MQTT topic and later processed by Node-RED, InfluxDB, and Grafana.
         */
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

        // Separator used only for readability in the terminal output
        printf("---\n");

        /*
         * Delay between two simulated sensor readings.
         *
         * sleep(2) can be used to simulate a real 2-second sensor interval.
         * usleep(500000) gives a faster demonstration with 0.5 seconds delay.
         */
        // sleep(2);        // Uncomment this for real-time 2-second simulation
        usleep(500000);    // 0.5 second delay for faster demo
    }

    // Close the CSV file after all readings are processed
    fclose(file);

    return 0;
}
