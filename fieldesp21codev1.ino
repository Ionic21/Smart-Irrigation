#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>
#include <math.h>

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
unsigned long lastDebugPrint = 0;
const unsigned long debugInterval = 8000;

// ----------------------------
// Setup
// ----------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;

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

    // Wait for response from Home ESP
    unsigned long startTime = millis();
    bool gotResponse = false;
    while (millis() - startTime < 3000) {
      if (LoRa.parsePacket()) {
        String incoming = LoRa.readString();
        Serial.print("Received: ");
        Serial.println(incoming);
        processIncoming(incoming);
        gotResponse = true;
        break;
      }
    }

    if (!gotResponse) {
      Serial.println("No response from Home ESP");
    }

    isInitiator = false;  // Wait for next cycle
  }

  // Periodic debug output
  if (millis() - lastDebugPrint > debugInterval) {
    readSensorsAndPrint();
    lastDebugPrint = millis();
  }

  delay(1000);         // Limit loop frequency
  isInitiator = true;  // Ready for next send-receive cycle
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
  if(soil == 4095 || soil == 0 || isnan(soil)){
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
void processIncoming(String incoming) {
  incoming.trim();
  int colonIndex = incoming.indexOf(':');

  if (colonIndex == -1) {
    Serial.println("Invalid message format");
    return;
  }

  String boolPart = incoming.substring(0, colonIndex);
  String valPart = incoming.substring(colonIndex + 1);

  bool command;
  int receivedValue;

  if (boolPart == "1") command = true;
  else if (boolPart == "0") command = false;
  else {
    Serial.println("Invalid boolean value");
    return;
  }

  receivedValue = valPart.toInt();

  float temp = dht.readTemperature();
  temp = (temp - 20)*5;
  temp = 100 - temp; 
  float hum = dht.readHumidity();
  int soil = analogRead(SOIL_PIN);
  soil = (3500-soil)/25;

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Sensor error - ignoring command");
    return;
  }

  float current = 0.9 * soil + 0.05 * temp + 0.05 * hum;
  current = constrain(round(current * 10) / 10.0, 30, 100);

  Serial.print("current current: ");
  Serial.println(current);

  if (command) {
    digitalWrite(VALVE_PIN, LOW);  // Valve ON
    Serial.println("Command = true: Valve ON");
  } else {
    if (current < receivedValue) {
      digitalWrite(VALVE_PIN, LOW);  // Valve ON
      Serial.println("current < received: Valve ON");
    } else {
      digitalWrite(VALVE_PIN, HIGH);  // Valve OFF
      Serial.println("current >= received: Valve OFF");
    }
  }
}

// ----------------------------
// Sensor Debug Print
// ----------------------------
void readSensorsAndPrint() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int soil = analogRead(SOIL_PIN);

  Serial.print("Temp: ");
  Serial.print(isnan(temp) ? -100 : temp);
  Serial.print(" °C, Humidity: ");
  Serial.print(isnan(hum) ? -100 : hum);
  Serial.print(" %, Soil: ");
  Serial.println(soil);
}


