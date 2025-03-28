# Heart Health Monitoring System

An ESP32-based ECG monitoring system that uses machine learning to detect cardiac anomalies in real-time.

## Overview

This project implements a wearable ECG monitoring system that:

- Captures ECG signals using an AD8232 sensor
- Processes the signals in real-time on an ESP32 microcontroller
- Uses a Support Vector Machine (SVM) model to detect cardiac anomalies
- Alerts the user via buzzer when anomalies are detected
- Calculates calorie expenditure based on heart rate
- Provides a web interface for real-time monitoring and visualization

## Project Structure

- **Web Files/**: Contains the web interface files (HTML, CSS, JavaScript)
- **main.cpp**: Main ESP32 code for data acquisition and processing
- **svm-extraction.py**: Script to convert trained ML model to ESP32-compatible format
- **train.ipynb**: Jupyter notebook for training and evaluating ML models

## Model Performance

Several machine learning models were trained and evaluated on the ECG dataset:

| Model                | Accuracy | Precision | Recall  | F1 Score |
|----------------------|----------|-----------|---------|----------|
| Logistic Regression  | 0.9215   | 0.9180    | 0.9202  | 0.9191   |
| Random Forest        | 0.9615   | 0.9615    | 0.9628  | 0.9621   |
| SVM                  | 0.9487   | 0.9462    | 0.9475  | 0.9468   |
| Gradient Boosting    | 0.9574   | 0.9550    | 0.9562  | 0.9556   |
| Neural Network       | 0.9651   | 0.9630    | 0.9642  | 0.9636   |

While the Neural Network based model achieved the highest accuracy (96.51%), the SVM model (94.87%) was selected for deployment on the ESP32 due to its more efficient memory usage and computational requirements. The performance difference was minimal making SVM an excellent choice for resource-constrained embedded systems.

## Features

- **Real-time ECG Monitoring**: Visualize ECG waveforms in real-time
- **Anomaly Detection**: Automatic detection of cardiac anomalies using machine learning
- **Alert System**: Buzzer alert when anomalies are detected
- **Calorie Tracking**: Estimates calorie expenditure based on heart rate
- **Web Dashboard**: Access monitoring data from any device on the local network

## Hardware Requirements

- ESP32 Development Board
- AD8232 ECG Sensor
- Buzzer
- ECG Electrodes (3-lead configuration)
- Power Supply/Battery

## Getting Started

1. Train the model using `train.ipynb`
2. Extract model parameters using `svm-extraction.py`
3. Upload web files to the ESP32 file system
4. Compile and upload the main program to ESP32
5. Connect the ECG sensor and electrodes
6. Access the web interface using the ESP32's IP address

