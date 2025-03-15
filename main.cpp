/******************************************************************
 * ESP32 ECG Monitoring System with SVM Anomaly Detection
 * 
 * Features:
 * - Reads ECG data from AD8232 sensor
 * - Processes data in 30ms windows
 * - Detects anomalies using SVM with RBF kernel
 * - Calculates calories based on heart rate
 * - Activates buzzer on anomaly detection
 * - Provides web interface with WebSockets for real-time monitoring
 ******************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <vector>
#include <cmath>

// Network credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Pin definitions
const int ECG_PIN = 34;      // AD8232 ECG sensor output
const int LO_PLUS_PIN = 32;  // AD8232 LO+ pin
const int LO_MINUS_PIN = 33; // AD8232 LO- pin
const int BUZZER_PIN = 25;   // Buzzer pin

// ECG and heart rate variables
const int ECG_BUFFER_SIZE = 140;              // Size of our feature vector
const int SAMPLING_RATE = 360;                // Sampling rate in Hz
const unsigned long SAMPLE_INTERVAL = 1000000 / SAMPLING_RATE; // Microseconds between samples
std::vector<float> ecgBuffer(ECG_BUFFER_SIZE, 0.0);
unsigned long lastSampleTime = 0;
int bufferIndex = 0;
float heartRate = 0.0;
float dailyCalories = 0.0;
bool anomalyDetected = false;
unsigned long lastAnomalyTime = 0;
const unsigned long BUZZER_DURATION = 500;    // Buzzer duration in ms
const unsigned long ANOMALY_COOLDOWN = 3000;  // Time between consecutive anomaly alerts

// Time tracking
unsigned long startTime = 0;
unsigned long lastCalorieUpdate = 0;
const unsigned long CALORIE_UPDATE_INTERVAL = 60000; // Update calories every minute

// SVM model parameters
// These would be extracted from the trained model
struct SVMModel {
    int numSupportVectors;
    float gamma;  // RBF kernel parameter
    float bias;
    std::vector<float> supportVectors;  // Flattened support vectors
    std::vector<float> dualCoefficients;
    std::vector<float> featureMeans;
    std::vector<float> featureStds;
} svmModel;

// Web server
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Function prototypes
void setupWiFi();
void setupSPIFFS();
void setupWebServer();
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                     AwsEventType type, void *arg, uint8_t *data, size_t len);
void processECGData();
float calculateHeartRate(const std::vector<float>& ecgData);
float calculateCalories(float heartRate, unsigned long elapsedMinutes);
bool detectAnomaly(const std::vector<float>& ecgData);
float rbfKernel(const std::vector<float>& x1, const std::vector<float>& x2, float gamma);
void standardizeFeatures(std::vector<float>& features);

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    
    // Initialize pins
    pinMode(ECG_PIN, INPUT);
    pinMode(LO_PLUS_PIN, INPUT);
    pinMode(LO_MINUS_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    
    // Initialize system components
    setupSVMModel();
    setupSPIFFS();
    setupWiFi();
    setupWebServer();
    
    // Record start time
    startTime = millis();
    lastCalorieUpdate = startTime;
    
    Serial.println("ECG Monitoring System Initialized");
}

void loop() {
    // Check if leads are properly attached
    if (digitalRead(LO_PLUS_PIN) == HIGH || digitalRead(LO_MINUS_PIN) == HIGH) {
        // Leads are off, send alert
        String message = "{\"type\":\"alert\",\"message\":\"Leads are not properly attached\"}";
        ws.textAll(message);
        delay(1000);
        return;
    }
    
    // Read ECG data
    unsigned long currentTime = micros();
    if (currentTime - lastSampleTime >= SAMPLE_INTERVAL) {
        lastSampleTime = currentTime;
        
        // Read ECG value and add to buffer
        int ecgValue = analogRead(ECG_PIN);
        
        // Convert to voltage (0-3.3V for ESP32 ADC)
        float voltage = ecgValue * (3.3 / 4095.0);
        
        // Add to buffer (circular buffer implementation)
        ecgBuffer[bufferIndex] = voltage;
        bufferIndex = (bufferIndex + 1) % ECG_BUFFER_SIZE;
        
        // Stream real-time ECG data to WebSocket clients
        if (bufferIndex % 5 == 0) { // Send every 5 samples to reduce traffic
            String ecgData = "{\"type\":\"ecg\",\"value\":" + String(voltage, 3) + "}";
            ws.textAll(ecgData);
        }
        
        // Process complete buffer every 30ms for anomaly detection
        // 30ms at 360Hz is about 11 samples, but we'll use the full buffer once filled
        if (bufferIndex == 0) { // Buffer is full and wrapped around
            processECGData();
        }
    }
    
    // Handle buzzer timeout
    if (anomalyDetected && (millis() - lastAnomalyTime > BUZZER_DURATION)) {
        digitalWrite(BUZZER_PIN, LOW);
        anomalyDetected = false;
    }
    
    // Update calories every minute
    if (millis() - lastCalorieUpdate >= CALORIE_UPDATE_INTERVAL) {
        unsigned long elapsedMinutes = (millis() - startTime) / 60000;
        dailyCalories = calculateCalories(heartRate, elapsedMinutes);
        
        // Send calorie update to WebSocket clients
        String calorieData = "{\"type\":\"calories\",\"value\":" + String(dailyCalories, 1) + 
                            ",\"heartRate\":" + String(heartRate, 1) + "}";
        ws.textAll(calorieData);
        
        lastCalorieUpdate = millis();
    }
}

void setupSVMModel() {
    // In a real implementation, you would load these values from SPIFFS or hardcode from the trained model
    
    // Example values - THESE NEED TO BE REPLACED with actual values from your trained model
    svmModel.numSupportVectors = 50;  // Example number of support vectors
    svmModel.gamma = 0.01;            // RBF kernel parameter
    svmModel.bias = -0.5;             // Model bias term
    
    // Reserve space for support vectors (flattened)
    svmModel.supportVectors.resize(svmModel.numSupportVectors * ECG_BUFFER_SIZE);
    svmModel.dualCoefficients.resize(svmModel.numSupportVectors);
    svmModel.featureMeans.resize(ECG_BUFFER_SIZE);
    svmModel.featureStds.resize(ECG_BUFFER_SIZE);
    
    // Populate with example values
    // In a real implementation, these would be the actual values from your trained model
    
    // Feature means and standard deviations for standardization
    for (int i = 0; i < ECG_BUFFER_SIZE; i++) {
        svmModel.featureMeans[i] = 0.0;  // Mean of feature i
        svmModel.featureStds[i] = 1.0;   // Standard deviation of feature i
    }
    
    // Support vectors and coefficients
    for (int i = 0; i < svmModel.numSupportVectors; i++) {
        svmModel.dualCoefficients[i] = (i % 2 == 0) ? 1.0 : -1.0;  // Alternating signs for example
        
        for (int j = 0; j < ECG_BUFFER_SIZE; j++) {
            // Example support vector values - replace with actual values
            svmModel.supportVectors[i * ECG_BUFFER_SIZE + j] = 0.1 * i * j;
        }
    }
    
    Serial.println("SVM model initialized");
}

void setupSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully");
}

void setupWiFi() {
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void setupWebServer() {
    // WebSocket event handler
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);
    
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });
    
    // Route for style.css
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/style.css", "text/css");
    });
    
    // Route for script.js
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/script.js", "text/javascript");
    });
    
    // Start server
    server.begin();
    Serial.println("HTTP server started");
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                     AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            // Send initial data
            client->text("{\"type\":\"calories\",\"value\":" + String(dailyCalories, 1) + 
                        ",\"heartRate\":" + String(heartRate, 1) + "}");
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            // Handle any incoming messages from clients
            break;
        case WS_EVT_ERROR:
            Serial.printf("WebSocket error for client #%u\n", client->id());
            break;
    }
}

void processECGData() {
    // Create a copy of the current ECG buffer
    std::vector<float> currentECG(ECG_BUFFER_SIZE);
    
    // Reconstruct buffer in correct order (since we use a circular buffer)
    for (int i = 0; i < ECG_BUFFER_SIZE; i++) {
        int idx = (bufferIndex + i) % ECG_BUFFER_SIZE;
        currentECG[i] = ecgBuffer[idx];
    }
    
    // Calculate heart rate from ECG data
    float newHeartRate = calculateHeartRate(currentECG);
    if (newHeartRate > 0) {
        heartRate = 0.7 * heartRate + 0.3 * newHeartRate; // Smoothing
    }
    
    // Detect anomalies
    bool isAnomaly = detectAnomaly(currentECG);
    
    // If anomaly detected and cooldown period passed, trigger alert
    if (isAnomaly && (millis() - lastAnomalyTime > ANOMALY_COOLDOWN)) {
        Serial.println("Anomaly detected!");
        lastAnomalyTime = millis();
        anomalyDetected = true;
        digitalWrite(BUZZER_PIN, HIGH);
        
        // Notify clients
        String anomalyData = "{\"type\":\"anomaly\",\"timestamp\":" + String(millis()) + "}";
        ws.textAll(anomalyData);
    }
}

float calculateHeartRate(const std::vector<float>& ecgData) {
    // Simple peak detection for heart rate calculation
    // A more sophisticated algorithm would be used in a real implementation
    
    int peakCount = 0;
    bool rising = false;
    float threshold = 1.5; // Threshold for peak detection
    
    for (size_t i = 1; i < ecgData.size(); i++) {
        if (!rising && ecgData[i] > ecgData[i-1] && ecgData[i] > threshold) {
            rising = true;
        } else if (rising && ecgData[i] < ecgData[i-1]) {
            peakCount++;
            rising = false;
        }
    }
    
    // Calculate heart rate in BPM
    // Time window is ECG_BUFFER_SIZE / SAMPLING_RATE seconds
    float timeWindowInSeconds = (float)ECG_BUFFER_SIZE / SAMPLING_RATE;
    float heartRate = (peakCount * 60.0) / timeWindowInSeconds;
    
    return heartRate;
}

float calculateCalories(float heartRate, unsigned long elapsedMinutes) {
    // Simple calorie calculation - replace with a more accurate formula if needed
    // This is a rough estimate using heart rate
    
    // Example formula: calories = average_heart_rate * minutes * factor
    // where factor depends on gender, age, weight, etc.
    float factor = 0.1; // Example factor, adjust based on individual
    
    return heartRate * elapsedMinutes * factor;
}

bool detectAnomaly(const std::vector<float>& ecgData) {
    // Use SVM model to detect anomalies
    
    // Standardize features first (same as in training)
    std::vector<float> features = ecgData;
    standardizeFeatures(features);
    
    // SVM prediction with RBF kernel
    float decision = svmModel.bias;
    
    // For each support vector
    for (int i = 0; i < svmModel.numSupportVectors; i++) {
        // Extract support vector
        std::vector<float> supportVector(ECG_BUFFER_SIZE);
        for (int j = 0; j < ECG_BUFFER_SIZE; j++) {
            supportVector[j] = svmModel.supportVectors[i * ECG_BUFFER_SIZE + j];
        }
        
        // Calculate kernel and add weighted contribution
        float kernelValue = rbfKernel(features, supportVector, svmModel.gamma);
        decision += svmModel.dualCoefficients[i] * kernelValue;
    }
    
    // Decision boundary: decision < 0 means anomaly
    return decision < 0;
}

float rbfKernel(const std::vector<float>& x1, const std::vector<float>& x2, float gamma) {
    // Calculate RBF kernel: K(x,y) = exp(-gamma * ||x-y||^2)
    float squaredDistance = 0.0;
    
    for (size_t i = 0; i < x1.size(); i++) {
        float diff = x1[i] - x2[i];
        squaredDistance += diff * diff;
    }
    
    return exp(-gamma * squaredDistance);
}

void standardizeFeatures(std::vector<float>& features) {
    // Apply same standardization as used during training
    for (size_t i = 0; i < features.size(); i++) {
        features[i] = (features[i] - svmModel.featureMeans[i]) / svmModel.featureStds[i];
    }
}
