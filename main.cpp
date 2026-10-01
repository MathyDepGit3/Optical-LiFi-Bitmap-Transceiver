#include <Arduino.h>
#include <WiFiS3.h>

const char ssid[] = "LiFi-Transmitter";
const char pass[] = "12345678";

WiFiServer server(4242);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Hardware UART naar de IR-driver op 9600 baud
  Serial1.begin(19200);

  // Start Access Point
  WiFi.beginAP(ssid, pass);
  server.begin();

  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    // Lees door zolang er verbinding is OF zolang er nog bytes in de buffer wachten
    while (client.connected() || client.available()) {
      while (client.available()) {
        uint8_t byteIn = client.read();
        Serial1.write(byteIn);
      }
    }
    Serial1.flush(); // Wacht tot alle UART-bits fysiek verzonden zijn
    client.stop();
  }
}