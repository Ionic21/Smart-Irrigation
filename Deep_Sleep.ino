#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>
#include <math.h>

#define uS_to_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 1800 // 30 minutes in seconds
RTC_DATA_ATTR int bootCount = 0;

// ----------------------------
// Pin Definitions
// ----------------------------
#define NSS 4
#define RST 5
#define DI0 2

#define DHTPIN 22
#define DHTTYPE DHT11
#define SOIL_PIN 15
#define VALVE_PIN 21

DHT dht(DHTPIN, DHTTYPE);

bool isInitiator = true;

// ----------------------------
// Setup
// ----------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial);

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  pinMode(VALVE_PIN, OUTPUT);
  digitalWrite(VALVE_PIN, HIGH);  // Valve OFF initially

  dht.begin();

  LoRa.setPins(NSS, RST, DI0);
  Serial.println("LoRa Hybrid Mode");

  delay(3000);

  while (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa...");
    delay(500);
  }

  LoRa.setSyncWord(0xF1);
  Serial.println("LoRa Initialized.");
  delay(1000);
}

// ----------------------------
// Main Loop
// ----------------------------
void loop() {
  if (isInitiator) {
    sendSensorData();

    unsigned long startTime = millis();
    bool gotValidResponse = false;

    while (millis() - startTime < 3000) {
      if (LoRa.parsePacket()) {
        String incoming = LoRa.readString();
        Serial.print("Received: ");
        Serial.println(incoming);
        gotValidResponse = processIncoming(incoming);
        break;
      }
    }

    if (!gotValidResponse) {
      Serial.println("No valid command received. Using last known threshold = 50");
      evaluateAndControlValve(false, 50); // Default threshold
    }

    Serial.println("Going to deep sleep for 30 minutes...");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_to_S_FACTOR);
    esp_deep_sleep_start();
  }
}

// ----------------------------
// Send Sensor Data
// ----------------------------
void sendSensorData() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int soil = analogRead(SOIL_PIN);

  int tempInt = isnan(temp) ? -100 : round(temp);
  int humInt = isnan(hum) ? -100 : round(hum);
  if (soil == 4095 || soil == 0 || isnan(soil)) {
    soil = -100;
  }

  String msg = "Temp:" + String(tempInt) + ",Hum:" + String(humInt) + ",Soil:" + String(soil);
  Serial.print("Sending: ");
  Serial.println(msg);

  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
}

// ----------------------------
// Process Received Command
// ----------------------------
bool processIncoming(String incoming) {
  incoming.trim();
  int colonIndex = incoming.indexOf(':');

  if (colonIndex == -1) {
    Serial.println("Invalid message format");
    return false;
  }

  String boolPart = incoming.substring(0, colonIndex);
  String valPart = incoming.substring(colonIndex + 1);

  bool command;
  int receivedValue;

  if (boolPart == "1") command = true;
  else if (boolPart == "0") command = false;
  else {
    Serial.println("Invalid boolean value");
    return false;
  }

  receivedValue = valPart.toInt();

  evaluateAndControlValve(command, receivedValue);
  return true; // Successfully processed
}

// ----------------------------
// Evaluate and Control Valve
// ----------------------------
void evaluateAndControlValve(bool forceOn, int threshold) {
  float temp = dht.readTemperature();
  temp = (temp - 20) * 5;
  temp = 100 - temp;

  float hum = dht.readHumidity();
  int soil = analogRead(SOIL_PIN);
  soil = (3500 - soil) / 25;

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Sensor error - ignoring valve control");
    return;
  }

  float current = 0.9 * soil + 0.05 * temp + 0.05 * hum;
  current = constrain(round(current * 10) / 10.0, 30, 100);

  Serial.print("Current value: ");
  Serial.println(current);

  if (forceOn) {
    digitalWrite(VALVE_PIN, LOW);  // Valve ON
    Serial.println("Force ON: Valve ON");
  } else if (current < threshold) {
    digitalWrite(VALVE_PIN, LOW);  // Valve ON
    Serial.println("current < threshold: Valve ON");
  } else {
    digitalWrite(VALVE_PIN, HIGH);  // Valve OFF
    Serial.println("current >= threshold: Valve OFF");
  }
}
