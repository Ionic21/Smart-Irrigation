// HOME_WIFI.ino
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

HardwareSerial mySerial(2);  // UART2 (TX2=17, RX2=16)

// WiFi credentials
const char* ssid = "AmanS23";
const char* password = "nopassword123";

// Server URLs
const char* getUrl = "http://192.168.6.6:5000/thresh_info";
const char* postUrl = "http://192.168.6.6:5000/sensor-data";

// Variables
float latestThreshold = 50;
bool manualPumpActive = false;

// ✅ Track whether threshold has ever been received
bool hasReceivedThreshold = false;

float temp = 0.0, hum = 0.0;
int soil = 0;
bool receivedSensorData = false;

void setup() {
  Serial.begin(115200);jjgasdhj
  mySerial.begin(115200, SERIAL_8N1, 16, 17); // UART2 (TX2=17, RX2=16)

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
}

void loop() {
  // Step 1: Try to update threshold
  bool gotNewThreshold = fetchThreshInfo();

  // ✅ Fallback: use last threshold or default
  float thresholdToSend = hasReceivedThreshold ? latestThreshold : 50;
  bool manualToSend = hasReceivedThreshold ? manualPumpActive : false;

  // Step 2: Send threshold to LoRa ESP via UART
  String msg = String(manualToSend) + ":" + String(thresholdToSend);
  mySerial.println(msg);
  Serial.print("Sent to LoRa ESP: ");
  Serial.println(msg);

  // Step 3: Wait for sensor data from LoRa ESP
  String incoming = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    if (mySerial.available()) {
      incoming = mySerial.readStringUntil('\n');
      Serial.print("Received from LoRa ESP: ");
      Serial.println(incoming);

      if (parseSensorData(incoming)) {
        receivedSensorData = true;
        break;
      }
    }
  }

  // Step 4: Post data to server
  if (receivedSensorData) {
    sendSensorData();
    receivedSensorData = false;
  }

  delay(10000);
}

// Fetch threshold from server
bool fetchThreshInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(getUrl);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println("Received Threshold JSON:");
      Serial.println(payload);

      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        latestThreshold = doc["target_moisture"];
        manualPumpActive = doc["manual_pump_active"];
        hasReceivedThreshold = true; // ✅ Mark as received
        Serial.print("Target Moisture: ");
        Serial.println(latestThreshold);
        Serial.print("Manual Pump: ");
        Serial.println(manualPumpActive ? "Yes" : "No");
        http.end();
        return true;
      } else {
        Serial.println("❌ JSON parse failed");
      }
    } else {
      Serial.print("❌ HTTP GET Error: ");
      Serial.println(httpCode);
    }

    http.end();
  } else {
    Serial.println("⚠ WiFi not connected");
  }
  return false;
}

// Parse sensor data from field
bool parseSensorData(String incoming) {
  incoming.trim(); // Remove leading/trailing whitespace or newlines

  int tempIndex = incoming.indexOf("Temp:");
  int humIndex = incoming.indexOf("Hum:");
  int soilIndex = incoming.indexOf("Soil:");

  if (tempIndex == -1 || humIndex == -1 || soilIndex == -1) {
    return false; // Something is missing
  }

  int tempEnd = incoming.indexOf(",", tempIndex);
  int humEnd = incoming.indexOf(",", humIndex);

  if (tempEnd == -1 || humEnd == -1) {
    return false; // Malformed string
  }

  String tempStr = incoming.substring(tempIndex + 5, tempEnd);
  String humStr = incoming.substring(humIndex + 4, humEnd);
  String soilStr = incoming.substring(soilIndex + 5);

  temp = tempStr.toFloat();
  hum = humStr.toFloat();
  soil = soilStr.toInt();

  return true;
}

// Send data to server
void sendSensorData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(postUrl);
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"temperature\": " + String(temp) + ",";
    json += "\"humidity\": " + String(hum) + ",";
    json += "\"soil_moisture\": " + String(soil) + ",";
    json += "\"millis\": " + String(millis());
    json += "}";

    int httpCode = http.POST(json);

    if (httpCode > 0) {
      Serial.println("✅ Data sent successfully:");
      Serial.println(json);
    } else {
      Serial.print("❌ Error sending data: ");
      Serial.println(httpCode);
    }

    http.end();
  } else {
    Serial.println("⚠ WiFi not connected");
  }
}
