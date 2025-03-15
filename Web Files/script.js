// WebSocket connection
let ws;
let reconnectInterval = 5000; // Reconnect every 5 seconds if connection fails
let ecgChart;
let ecgData = Array(100).fill(0); // Store 100 ECG data points for display

// Initialize WebSocket connection
function initWebSocket() {
    const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${wsProtocol}//${window.location.host}/ws`;
    
    ws = new WebSocket(wsUrl);
    
    ws.onopen = function() {
        console.log('WebSocket connection established');
        document.getElementById('status').textContent = 'Connected';
        document.getElementById('status').style.color = '#27ae60';
    };
    
    ws.onmessage = function(event) {
        handleWebSocketMessage(event.data);
    };
    
    ws.onclose = function() {
        console.log('WebSocket connection closed. Reconnecting...');
        document.getElementById('status').textContent = 'Disconnected';
        document.getElementById('status').style.color = '#e74c3c';
        
        // Attempt to reconnect
        setTimeout(initWebSocket, reconnectInterval);
    };
    
    ws.onerror = function(error) {
        console.error('WebSocket error:', error);
    };
}

// Handle incoming WebSocket messages
function handleWebSocketMessage(message) {
    try {
        const data = JSON.parse(message);
        
        switch(data.type) {
            case 'ecg':
                // Update ECG data and chart
                updateECGData(data.value);
                break;
                
            case 'calories':
                // Update calories and heart rate display
                document.getElementById('calories').textContent = `${data.value.toFixed(1)} kcal`;
                document.getElementById('heart-rate').textContent = `${data.heartRate.toFixed(0)} BPM`;
                break;
                
            case 'anomaly':
                // Handle anomaly detection
                handleAnomaly(data.timestamp);
                break;
                
            case 'alert':
                // Display alert message
                addLogEntry(data.message, true);
                break;
                
            default:
                console.log('Unknown message type:', data.type);
        }
    } catch (e) {
        console.error('Error parsing message:', e);
    }
}

// Update ECG data and chart
function updateECGData(value) {
    // Add new value to the end and remove oldest value
    ecgData.push(value);
    ecgData.shift();
    
    // Update chart
    ecgChart.update();
}

// Handle anomaly detection
function handleAnomaly(timestamp) {
    // Update status indicator
    const statusElement = document.getElementById('status');
    statusElement.textContent = 'ANOMALY DETECTED';
    statusElement.className = 'status-value anomaly';
    
    // Add to log
    const date = new Date(timestamp);
    const timeString = date.toLocaleTimeString();
    addLogEntry(`Anomaly detected at ${timeString}`, true);
    
    // Reset status after 5 seconds
    setTimeout(() => {
        statusElement.textContent = 'Normal';
        statusElement.className = 'status-value normal';
    }, 5000);
}

// Add entry to anomaly log
function addLogEntry(message, isAnomaly = false) {
    const logContainer = document.getElementById('anomaly-log');
    
    // Create new log entry
    const logEntry = document.createElement('div');
    logEntry.className = isAnomaly ? 'log-entry anomaly' : 'log-entry';
    logEntry.textContent = message;
    
    // Add to top of log
    logContainer.insertBefore(logEntry, logContainer.firstChild);
    
    // Remove default "No anomalies" message if it exists
    const defaultEntry = logContainer.querySelector('.log-entry:last-child');
    if (defaultEntry && defaultEntry.textContent === 'No anomalies detected') {
        logContainer.removeChild(defaultEntry);
    }
    
    // Keep log size reasonable (max 50 entries)
    const logEntries = logContainer.querySelectorAll('.log-entry');
    if (logEntries.length > 50) {
        logContainer.removeChild(logEntries[logEntries.length - 1]);
    }
}

// Initialize ECG chart
function initECGChart() {
    const ctx = document.getElementById('ecg-chart').getContext('2d');
    
    ecgChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: Array(100).fill(''),  // Empty labels
            datasets: [{
                label: 'ECG Signal',
                data: ecgData,
                borderColor: '#3498db',
                borderWidth: 1.5,
                fill: false,
                tension: 0.2
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: {
                duration: 0  // Disable animation for performance
            },
            scales: {
                x: {
                    display: false
                },
                y: {
                    min: 0,
                    max: 3.3,  // 3.3V max for ESP32 ADC
                    grid: {
                        color: '#f0f0f0'
                    }
                }
            },
            plugins: {
                legend: {
                    display: false
                }
            }
        }
    });
}

// Initialize the application
window.onload = function() {
    initECGChart();
    initWebSocket();
    
    // Set initial status
    document.getElementById('status').textContent = 'Connecting...';
    document.getElementById('status').style.color = '#f39c12';
};
