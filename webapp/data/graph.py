import pandas as pd
import matplotlib.pyplot as plt

# Load and preprocess
df = pd.read_csv("sensor_log.csv")
df['timestamp'] = pd.to_datetime(df['timestamp'], errors='coerce')
df = df.dropna(subset=['timestamp', 'humidity', 'temperature', 'soil_moisture'])

# Scale soil moisture
df['soil_moisture'] = df['soil_moisture'] / 4095 * 100

# Filter out soil moisture values > 90%
df = df[df['soil_moisture'] <= 90]
df = df[df['soil_moisture'] >= 30]
# Remove extreme outliers using IQR
def remove_outliers_iqr(data, column):
    Q1 = data[column].quantile(0.25)
    Q3 = data[column].quantile(0.75)
    IQR = Q3 - Q1
    return data[(data[column] >= Q1 - 1.5 * IQR) & (data[column] <= Q3 + 1.5 * IQR)]

for col in ['humidity', 'temperature', 'soil_moisture']:
    df = remove_outliers_iqr(df, col)

# Optional smoothing with a moving average
df = df.sort_values('timestamp')
df[['humidity', 'temperature', 'soil_moisture']] = df[['humidity', 'temperature', 'soil_moisture']].rolling(window=5, min_periods=1).mean()

# Plot
plt.figure(figsize=(14, 6))
plt.plot(df['timestamp'], df['humidity'], label='Humidity (%)', color='blue', alpha=0.8)
plt.plot(df['timestamp'], df['temperature'], label='Temperature (°C)', color='red', alpha=0.8)
plt.plot(df['timestamp'], df['soil_moisture'], label='Soil Moisture (%)', color='green', alpha=0.8)

plt.xlabel('Timestamp')
plt.ylabel('Sensor Values')
plt.title('Sensor Values Over Time (with Normalization & Filtering)')
plt.legend()
plt.grid(True)
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig("smoothed_sensor_data.png", dpi=300)
plt.show()
