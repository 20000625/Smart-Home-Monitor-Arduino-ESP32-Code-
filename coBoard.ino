#include <WiFi.h>          // For ESP32. Use <ESP8266WiFi.h> for ESP8266
#include <WiFiUdp.h>       // For UDP communication
#include <WiFiClient.h>    // For TCP communication

const char* ssid = "Toby's Galaxy S21 5G";       // Replace with your WiFi SSID
const char* password = "123456782";  // Replace with your WiFi password

WiFiUDP udp;
unsigned int localUdpPort = 8888;    // Port to listen on
IPAddress remoteIp;                  // IP address of the device that responds with "SensorBoard"
bool ipReceived = false;             // Flag to check if IP is received
WiFiClient client;                   // TCP client object
const int remotePort = 12345;        // Remote port for TCP communication

const float maxValue = 20.0;
int ledPin = 5;
bool ledControlAuto = true; // Flag to track if LED control is automatic or manual

double co;

int loopCounter = 0;  // Counter for loop iterations

  // Perform TCP request every 100 loops
unsigned long lastRequestTime = 0; // Timestamp of the last TCP request
const unsigned long requestInterval = 10000; // Interval between requests (10 seconds)


void setup() {
  WiFi.setHostname("Extractor Fan");
  Serial.begin(115200);             // Start Serial communication
  Serial.println();
  pinMode(ledPin, OUTPUT); // Set LED pin as output

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start listening for UDP packets
  udp.begin(localUdpPort);
  Serial.print("UDP initialized on port ");
  Serial.println(localUdpPort);
}

void loop() {
  if (!ipReceived) {
    const char* message = "Fan"; // Message to broadcast

    // Broadcast to all devices on the network
    udp.beginPacket("255.255.255.255", localUdpPort); // Send to broadcast address
    udp.write((uint8_t*)message, strlen(message));
    udp.endPacket();

    Serial.println("Broadcasted UDP message: Fan");

    // Wait for a response for up to 2 seconds
    unsigned long startMillis = millis();
    while (millis() - startMillis < 2000) { // Wait for 2 seconds
      int packetSize = udp.parsePacket();
      if (packetSize) {
        // Read the incoming packet into a buffer
        char incomingPacket[255];
        udp.read(incomingPacket, 255);
        incomingPacket[packetSize] = 0;  // Null-terminate the string

        // Check if the message is "SensorBoard"
        if (strcmp(incomingPacket, "SensorBoard") == 0) {
          // Store the sender's IP address
          remoteIp = udp.remoteIP();
          Serial.print("Received 'SensorBoard' from: ");
          Serial.println(remoteIp);
          ipReceived = true;  // Set the flag to true, so broadcasting stops
        }
      }
    }
  }

  // Listen for commands on UDP port 8888
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incomingPacket[255];
    udp.read(incomingPacket, 255);
    incomingPacket[packetSize] = 0;  // Null-terminate the string

    String command = String(incomingPacket);
    command.trim();  // Remove leading/trailing whitespace

    // Handle the commands
    if (command == "ON") {
      digitalWrite(ledPin, HIGH);  // Turn the LED on manually
      ledControlAuto = false;      // Override automatic control
      Serial.println("LED ON (Manual Override)");
    } else if (command == "OFF") {
      digitalWrite(ledPin, LOW);   // Turn the LED off manually
      ledControlAuto = false;      // Override automatic control
      Serial.println("LED OFF (Manual Override)");
    } else if (command == "AUTO") {
      ledControlAuto = true;       // Re-enable automatic LED control
      Serial.println("Automatic LED control re-enabled");
      if (co > maxValue) {
        digitalWrite(ledPin, HIGH);  // Turn the LED on
        Serial.println("Humidity out of threshold range, LED ON.");
      } else {
        digitalWrite(ledPin, LOW);   // Turn the LED off
        Serial.println("Humidity within threshold range, LED OFF.");
      }
    }
  }

  // Check if it's time to send a TCP request
  if (ipReceived && (millis() - lastRequestTime >= requestInterval)) {
    // Try to connect to the remote device via TCP
    if (!client.connected()) {
      Serial.println("Connecting to TCP server...");
      if (client.connect(remoteIp, remotePort)) {
        Serial.println("Connected to TCP server");
      } else {
        Serial.println("Connection failed!");
        return; // Retry later
      }
    }

    // Send the "CO" request
    client.println("CO");
    Serial.println("Sent CO request to server");

    // Update the last request timestamp
    lastRequestTime = millis();
  }

  // Continuously check for responses
  if (client.connected() && client.available()) {
    String message = client.readStringUntil('\n'); // Read the message until newline
    Serial.print("Received message: ");
    Serial.println(message); // Print the received message

    // Process the message
    message.trim();
    Serial.print("Trimmed message: ");
    Serial.println(message);

    co = message.toDouble();
    Serial.print("Converted CO: ");
    Serial.println(co);

    // Check the CO level and control the LED
    if (ledControlAuto) {
      if (co > maxValue) {
        digitalWrite(ledPin, HIGH); // Turn the LED on
        Serial.println("CO levels are too high, LED ON.");
      } else {
        digitalWrite(ledPin, LOW); // Turn the LED off
        Serial.println("CO levels are acceptable, LED OFF.");
      }
    }
  }

  // Increment the loop counter (if needed elsewhere)
  loopCounter++;

  delay(10); // Short delay for stability
}
