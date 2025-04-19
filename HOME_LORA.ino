#include <SPI.h>
#include <LoRa.h>

HardwareSerial mySerial(2);  // UART2 (TX2=17, RX2=16)

#define NSS 4
#define RST 5
#define DI0 2

String lastReceivedFromField = "";
bool isInitiator = false;

void setup() {
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, 16, 17); // RX=16, TX=17
  
  LoRa.setPins(NSS, RST, DI0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setSyncWord(0xF1);
  Serial.println("LoRa Initialized!");
}

void loop() {
  if (!isInitiator && LoRa.parsePacket()) {
    // Step 1: Receive message from Field ESP
    String incoming = LoRa.readString();
    Serial.print("Received from Field: ");
    Serial.println(incoming);
    lastReceivedFromField = incoming;

    // Step 2: Forward to WiFi ESP via UART
    mySerial.println(incoming);
    Serial.println("Forwarded to WiFi ESP");

    while (mySerial.available()) mySerial.read();

    // Step 3: Wait for response from WiFi ESP (timeout after 3 seconds)
    unsigned long startTime = millis();
    while (!mySerial.available() && millis() - startTime < 3000);

    if (mySerial.available()) {
      String response = mySerial.readStringUntil('\n');
      response.trim();
      Serial.print("Received from WiFi ESP: ");
      Serial.println(response);

      // Step 4: Send response to Field ESP
      LoRa.beginPacket();
      LoRa.print(response);
      LoRa.endPacket();
      Serial.print("Sent to Field: ");
      Serial.println(response);
    } else {
      Serial.println("No response from WiFi ESP");
    }

    isInitiator = false; // Always false in Home ESP
  }

  delay(100); // Small delay to prevent tight loop
}
