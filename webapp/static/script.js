// === Configuration ===
const API_URL = '/control-pump';
const SENSOR_API_URL = '/sensor-data';

// === DARK MODE ===
function toggleDarkMode() {
    const body = document.body;
    body.classList.toggle('dark-mode');
    localStorage.setItem('darkMode', body.classList.contains('dark-mode'));
}

window.onload = () => {
    if (localStorage.getItem('darkMode') === 'true') {
        document.body.classList.add('dark-mode');
    }
};

// === Pump Control ===
function controlPump(action) {
    const duration = document.getElementById('duration').value;

    const payload = {
        action,
        duration: action === 'on' ? parseInt(duration) || 0 : 0
    };

    fetch(API_URL, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            updatePumpStatus(action, payload.duration);
        } else {
            alert('Failed to control pump');
        }
    })
    .catch(error => console.error('Error:', error));
}

let countdownTimer = null;
function updatePumpStatus(action, duration) {
    const pumpStatus = document.getElementById('pumpStatus');
    const countdown = document.getElementById('countdown');

    if (action === 'on') {
        pumpStatus.className = 'badge badge-success';
        pumpStatus.textContent = 'ON';

        if (countdownTimer) clearInterval(countdownTimer);

        if (duration > 0) {
            let timeLeft = duration * 60;
            countdownTimer = setInterval(() => {
                const minutes = Math.floor(timeLeft / 60);
                const seconds = timeLeft % 60;
                countdown.textContent = `Auto-off in: ${minutes}m ${seconds}s`;

                if (timeLeft <= 0) {
                    clearInterval(countdownTimer);
                    controlPump('off');
                } else {
                    timeLeft--;
                }
            }, 1000);
        }
    } else {
        if (countdownTimer) clearInterval(countdownTimer);
        countdown.textContent = '';
        pumpStatus.className = 'badge badge-secondary';
        pumpStatus.textContent = 'OFF';
    }
}

// === Crop Info and Confirmation ===
async function fetchCropInfo() {
    const cropName = document.getElementById('cropName').value;
    if (!cropName) {
        alert("Please enter a crop name!");
        return;
    }

    try {
        const response = await fetch(`/crop-info?crop=${encodeURIComponent(cropName)}`);
        const data = await response.json();

        if (data.error) {
            alert(data.error);
            return;
        }

        displayCropInfo(data);
    } catch (error) {
        console.error("Error fetching crop info:", error);
        alert("Failed to fetch crop info.");
    }
}

function displayCropInfo(data) {
    const cropInfoDiv = document.getElementById('cropInfo');
    const cropInfo = typeof data.info === 'string' ? JSON.parse(data.info) : data.info;

    const { temperature, soil_moisture, humidity, irrigation_tips } = cropInfo;

    cropInfoDiv.innerHTML = `
        <h5>Crop: ${data.crop.charAt(0).toUpperCase() + data.crop.slice(1)}</h5>
        <p id="tempVal"><strong>Temperature Range:</strong> ${temperature}</p>
        <p id="moistureVal"><strong>Soil Moisture Level:</strong> ${soil_moisture}</p>
        <p id="humidityVal"><strong>Humidity Range:</strong> ${humidity}</p>
        <h6>Irrigation Tips:</h6>
        <ul>${irrigation_tips.map(tip => `<li>${tip}</li>`).join("")}</ul>
    `;

    const confirmButton = document.createElement('button');
    confirmButton.innerText = "Confirm Crop";
    confirmButton.classList.add("confirm-button");
    confirmButton.onclick = async () => {
        const temperatureEl = document.querySelector('#cropInfo p:nth-child(2)');
        const soilMoistureEl = document.querySelector('#cropInfo p:nth-child(3)');
        const humidityEl = document.querySelector('#cropInfo p:nth-child(4)');

        const data = {
            temperature: temperatureEl ? temperatureEl.innerText.replace("Temperature Range: ", "") : "N/A",
            soil_moisture: soilMoistureEl ? soilMoistureEl.innerText.replace("Soil Moisture Level: ", "") : "N/A",
            humidity: humidityEl ? humidityEl.innerText.replace("Humidity Range: ", "") : "N/A"
        };

        try {
            const response = await fetch("/confirm_crop", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(data)
            });

            const result = await response.json();

            if (response.ok) {
                showToast("✅ Crop parameters confirmed and published.");

                // Replace Confirm with Reset button
                confirmButton.remove();
                const resetButton = document.createElement('button');
                resetButton.innerText = "Reset Selection";
                resetButton.classList.add("confirm-button");

                resetButton.onclick = async () => {
                    // Clear UI
                    document.getElementById('cropInfo').innerHTML = '';
                    document.getElementById('cropName').value = '';

                    const defaultData = {
                        temperature: "20-30°C",
                        soil_moisture: "Moderate",
                        humidity: "50-70%"
                    };

                    try {
                        const res = await fetch("/confirm_crop", {
                            method: "POST",
                            headers: { "Content-Type": "application/json" },
                            body: JSON.stringify(defaultData)
                        });

                        if (res.ok) {
                            showToast("🔄 Crop reset to default thresholds.");
                        } else {
                            const err = await res.json();
                            showToast(err.message || "Failed to reset crop.", "error");
                        }
                    } catch (e) {
                        showToast("❌ Reset request failed.", "error");
                        console.error("Reset error:", e);
                    }
                };

                cropInfoDiv.appendChild(resetButton);
            } else {
                showToast(result.message || "❌ Failed to confirm parameters.", "error");
            }
        } catch (err) {
            console.error("Confirm error:", err);
            showToast("❌ Could not confirm crop parameters.", "error");
        }
    };

    cropInfoDiv.appendChild(confirmButton);
}

// === Sensor Data + Charts ===
const soilMoistureCtx = document.getElementById('soilMoistureChart').getContext('2d');
const temperatureCtx = document.getElementById('temperatureChart').getContext('2d');
const humidityCtx = document.getElementById('humidityChart').getContext('2d');

const data = {
    labels: [],
    soilMoisture: [],
    temperature: [],
    humidity: []
};

const createChart = (ctx, label, color, minY, maxY) => new Chart(ctx, {
    type: 'line',
    data: {
        labels: data.labels,
        datasets: [{
            label: label,
            data: [],
            borderColor: color,
            backgroundColor: color + '33',
            fill: false,
            tension: 0.2
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        scales: {
            x: { title: { display: true, text: 'Time' } },
            y: { title: { display: true, text: label }, min: minY, max: maxY }
        }
    }
});

const soilMoistureChart = createChart(soilMoistureCtx, 'Soil Moisture (%)', 'rgba(75, 192, 192, 1)', 0, 100);
const temperatureChart = createChart(temperatureCtx, 'Temperature (°C)', 'rgba(255, 99, 132, 1)', -20, 60);
const humidityChart = createChart(humidityCtx, 'Humidity (%)', 'rgba(54, 162, 235, 1)', 0, 100);

const ESP32_TIMEOUT = 15000; // 15 sec
let lastESP32Millis = null;
let lastMillisUpdateTime = Date.now();
let previousMillis = null;

const fetchData = async () => {
    try {
        const response = await fetch(SENSOR_API_URL);
        if (!response.ok) throw new Error('Failed to fetch sensor data');

        const sensorData = await response.json();
        const { temperature, humidity, soil_moisture, millis } = sensorData;
        const nowMillis = Date.now();
        const isDataFresh = millis !== previousMillis;
        previousMillis = millis;

        const timestampLabel = new Date().toLocaleTimeString();
        const moisturePercent = Math.round((1 - soil_moisture / 4095) * 100);

        data.labels.push(timestampLabel);
        data.soilMoisture.push(moisturePercent);
        data.temperature.push(temperature);
        data.humidity.push(humidity);

        if (data.labels.length > 20) {
            data.labels.shift();
            data.soilMoisture.shift();
            data.temperature.shift();
            data.humidity.shift();
        }
        if (isDataFresh && temperature!==-101){
            // Update charts
            if (soil_moisture !== -100){
                soilMoistureChart.data.labels = data.labels;
                soilMoistureChart.data.datasets[0].data = data.soilMoisture;
                soilMoistureChart.update();
            }
            if (temperature !== -100){
                temperatureChart.data.labels = data.labels;
                temperatureChart.data.datasets[0].data = data.temperature;
                temperatureChart.update();
            }
            if (humidity !== -100){
                humidityChart.data.labels = data.labels;
                humidityChart.data.datasets[0].data = data.humidity;
                humidityChart.update();
            }
        }
        // === Status Updates ===
        updateStatusESP("local-esp32-status", isDataFresh);
        updateStatusESP("remote-esp32-status", isDataFresh && temperature!==-101);
        updateStatusSens("temperature-status", isDataFresh && temperature !== -100 && temperature!==-101);
        updateStatusSens("humidity-status", isDataFresh && humidity !== -100 && temperature!==-101);
        updateStatusSens("moisture-status", isDataFresh && soil_moisture !== -100 && temperature!==-101);

    } catch (error) {
        console.error('Error fetching sensor data:', error);
        updateStatusESP("local-esp32-status", false);
        updateStatusESP("remote-esp32-status", false);
        updateStatusSens("temperature-status", false);
        updateStatusSens("humidity-status", false);
        updateStatusSens("moisture-status", false);
    }
};

function showToast(message = "Action completed!") {
    const toast = document.getElementById("toast");
    toast.textContent = message;
    toast.classList.remove("hidden");
    toast.classList.add("show");

    setTimeout(() => {
        toast.classList.remove("show");
        toast.classList.add("hidden");
    }, 3000); // Toast visible for 3 seconds
}

let lastSensorData = null;
let lastUpdateTime = 0;

// Dummy local ESP32 status (assume always connected unless you want to implement a ping)
updateStatusESP("local-esp32-status", true);

// Helper function to set text and class
function updateStatusESP(id, isConnected) {
  const el = document.getElementById(id);
  el.textContent = isConnected ? "Connected" : "Disconnected";
  el.className = `status ${isConnected ? "connected" : "disconnected"}`;
}

function updateStatusSens(id, isConnected) {
  const el = document.getElementById(id);
  el.textContent = isConnected ? "Working" : "Damaged";
  el.className = `status ${isConnected ? "connected" : "disconnected"}`;
}

function downloadCSV() {
    window.location.href = "/download-csv";
}

fetchData();
setInterval(fetchData, 30000);
