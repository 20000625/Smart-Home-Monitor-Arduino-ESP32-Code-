#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <DHT.h>
#include <WiFiUdp.h>

// WiFi credentials
const char* ssid = "Toby's Galaxy S21 5G";
const char* password = "123456782";

// DHT sensor settings
#define DHTPIN 2
#define DHTTYPE DHT11

// CO sensor pin
const int coPin = A0;

// TFT screen settings
#define TFT_CS 10
#define TFT_RST 8
#define TFT_DC 9

// UDP settings
WiFiUDP udp;
unsigned int localPort = 8888;  // Listen on port 8888
char incomingPacket[255];       // Buffer for incoming data

// Declare sender's IP and port globally
IPAddress senderIP;
unsigned int senderPort;

//Conected device details
IPAddress fanIP;
unsigned int fanPort;
bool fanConnected = false;

IPAddress humidifierIP;
unsigned int humidifierPort;
bool humidifierConnected = false;

IPAddress radiatorIP;
unsigned int radiatorPort;
bool radiatorConnected = false;

// Declare TCP server on port 12345
WiFiServer server(12345);

DHT dht(DHTPIN, DHTTYPE);
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

float temperature, humidity, coPpm;

// Thresholds for bar colors
const float TEMP_MEDIUM = 30.0;
const float TEMP_HIGH = 40.0;

const float HUMID_GREEN_LOW = 40.0;
const float HUMID_GREEN_HIGH = 60.0;
const float HUMID_YELLOW_LOW = 30.0;
const float HUMID_YELLOW_HIGH = 70.0;

const float CO_MEDIUM = 20.0;
const float CO_HIGH = 50.0;

void setup() {
  Serial.begin(115200);

  // Initialize DHT sensor
  dht.begin();

  // Initialize TFT screen
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start listening for UDP packets on port 8888
  udp.begin(localPort);
  Serial.println("Listening for UDP packets on port 8888");

  // Start the TCP server on port 12345
  server.begin();
  Serial.println("TCP server started on port 12345");
}

void drawBarWithBox(int x, int y, int width, int height, float value, float maxValue, float mediumThreshold, float highThreshold, bool isHumidity = false) {
  int barHeight = map(value, 0, maxValue, 0, height);
  uint16_t barColor;

  // Determine bar color
  if (isHumidity) {
    if (value >= HUMID_GREEN_LOW && value <= HUMID_GREEN_HIGH) {
      barColor = ST77XX_GREEN;
    } else if ((value >= HUMID_YELLOW_LOW && value < HUMID_GREEN_LOW) || 
               (value > HUMID_GREEN_HIGH && value <= HUMID_YELLOW_HIGH)) {
      barColor = ST77XX_YELLOW;
    } else {
      barColor = ST77XX_RED;
    }
  } else {
    if (value < mediumThreshold) {
      barColor = ST77XX_GREEN;
    } else if (value < highThreshold) {
      barColor = ST77XX_YELLOW;
    } else {
      barColor = ST77XX_RED;
    }
  }
  tft.drawRect(x - 1, y - 1, width + 2, height + 2, ST77XX_WHITE);
  tft.fillRect(x, y, width, height, ST77XX_BLACK);
  tft.fillRect(x, y + (height - barHeight), width, barHeight, barColor);
}

void updateUI() {
  int barWidth = 20;
  int barHeight = 80;
  int gap = 15;
  int barY = 20;
  int labelY = barY + barHeight + 5;

  // Temperature Bar
  drawBarWithBox(10, barY, barWidth, barHeight, temperature, 50.0, TEMP_MEDIUM, TEMP_HIGH);
  tft.setCursor(10, labelY);
  tft.fillRect(10, labelY, barWidth + 10, 30, ST77XX_BLACK);
  tft.print("Temp");
  tft.setCursor(10, labelY + 10);
  tft.print(temperature, 1);
  tft.setCursor(10, labelY + 20);
  tft.print("C");

  // Humidity Bar
  drawBarWithBox(40 + gap, barY, barWidth, barHeight, humidity, 100.0, 0, 0, true);
  tft.setCursor(40 + gap, labelY);
  tft.fillRect(40 + gap, labelY, barWidth + 10, 30, ST77XX_BLACK);
  tft.print("Humid");
  tft.setCursor(40 + gap, labelY + 10);
  tft.print(humidity, 1);
  tft.setCursor(40 + gap, labelY + 20);
  tft.print("%");

  // CO Level Bar
  drawBarWithBox(70 + 2 * gap, barY, barWidth, barHeight, coPpm, 100.0, CO_MEDIUM, CO_HIGH);
  tft.setCursor(70 + 2 * gap, labelY);
  tft.fillRect(70 + 2 * gap, labelY, barWidth + 10, 30, ST77XX_BLACK);
  tft.print("CO");
  tft.setCursor(70 + 2 * gap, labelY + 10);
  tft.print(coPpm, 1);
  tft.setCursor(70 + 2 * gap, labelY + 20);
  tft.print("ppm");
}

void handleUDP() {
  int packetSize = udp.parsePacket();  // Check if a packet is available

  if (packetSize) {
    // Capture the sender's IP address and port
    IPAddress senderIP = udp.remoteIP();
    unsigned int senderPort = udp.remotePort();

    // Read the packet into the buffer
    udp.read(incomingPacket, 255);
    incomingPacket[packetSize] = 0;  // Null-terminate the string

    // Print received packet
    Serial.print("UDP packet received from ");
    Serial.print(senderIP);
    Serial.print(":");
    Serial.println(senderPort);
    Serial.print("Packet contents: ");
    Serial.println(incomingPacket);

    // If the message is from the fan, save its IP and port
    if (strcmp(incomingPacket, "Fan") == 0) {
      fanIP = senderIP;
      fanPort = senderPort;
      fanConnected = true;
      Serial.print("Fan connected. IP: ");
      Serial.println(fanIP);
    }
    else if (strcmp(incomingPacket, "Humidifier") == 0) {
      humidifierIP = senderIP;
      humidifierPort = senderPort;
      humidifierConnected = true;
      Serial.print("Humidifier connected. IP: ");
      Serial.println(humidifierIP);
    }
    else if (strcmp(incomingPacket, "Radiator") == 0) {
      radiatorIP = senderIP;
      radiatorPort = senderPort;
      radiatorConnected = true;
      Serial.print("Radiator connected. IP: ");
      Serial.println(radiatorIP);
    }

    // Send a response back to the sender
    udp.beginPacket(senderIP, senderPort);
    udp.write("SensorBoard");  // Send the response message
    udp.endPacket();
  }
}

void handleTCP() {
  WiFiClient client = server.available();  // Check if a client has connected

  if (client) {
    Serial.println("New client connected");
    String message = "";

    // Read the message from the client
    while (client.available()) {
      char c = client.read();
      message += c;
    }

    // Trim the message (removes leading and trailing whitespace)
    message.trim();
    Serial.println(message);

    // Check for fan control commands
    if (message.startsWith("FAN:")) {
      if (!fanConnected) {
        Serial.println("Fan not connected. Cannot send FAN commands.");
        client.println("ERROR: Fan not connected");
      } else {
        String command = message.substring(4);  // Extract "ON", "OFF", or "AUTO"
        Serial.print("Sending FAN command: ");
        Serial.println(command);

        // Send the command to the fan via UDP
        udp.beginPacket(fanIP, fanPort);
        udp.write(command.c_str());  // Convert String to C-string for UDP
        udp.endPacket();

        // Acknowledge the client
        client.println("FAN command sent: " + command);
      }
    } 
    else if (message.startsWith("HUMIDIFIER:")) {
      if (!humidifierConnected) {
        Serial.println("Humidifier not connected. Cannot send HUMIDIFIER commands.");
        client.println("ERROR: Humidifier not connected");
      } else {
        String command = message.substring(11);  // Extract "ON", "OFF", or "AUTO"
        Serial.print("Sending HUMIDIFIER command: ");
        Serial.println(command);

        // Send the command to the humidifier via UDP
        udp.beginPacket(humidifierIP, humidifierPort);
        udp.write(command.c_str());  // Convert String to C-string for UDP
        udp.endPacket();

        // Acknowledge the client
        client.println("HUMIDIFIER command sent: " + command);
      }
    }
    else if (message.startsWith("RADIATOR:")) {
      if (!radiatorConnected) {
        Serial.println("Radiator not connected. Cannot send RADIATOR commands.");
        client.println("ERROR: Ratiator not connected");
      } else {
        String command = message.substring(9);  // Extract "ON", "OFF", or "AUTO"
        Serial.print("Sending RADIATOR command: ");
        Serial.println(command);

        // Send the command to the humidifier via UDP
        udp.beginPacket(radiatorIP, radiatorPort);
        udp.write(command.c_str());  // Convert String to C-string for UDP
        udp.endPacket();

        // Acknowledge the client
        client.println("RADIATOR command sent: " + command);
      }
    }
    else if (message == "TEMP") {
      // Send the temperature data to the client
      client.print(temperature, 1);
      Serial.println("Sent temperature data to client: " + String(temperature) + " C");
    }
    else if (message == "HUMIDITY") {
      // Send the humidity data to the client
      client.print(humidity, 1);
      Serial.println("Sent humidity data to client: " + String(humidity) + "%");
    }
    else if (message == "CO") {
      // Send the CO data to the client
      client.print(coPpm, 1);
      Serial.println("Sent CO data to client: " + String(coPpm) + "ppm");
    }
    else if (message.equals("ALL")) {
      client.print(temperature, 1);
      Serial.println(temperature, 1);
      client.print(":");
      client.print(humidity, 1);
      Serial.println(humidity, 1);
      client.print(":");
      client.print(coPpm, 1);
      Serial.println(coPpm, 1);
      Serial.println("Sent all data to client");
    }
    else {
      Serial.println("Received unexpected message: " + message);
    }

    delay(500);  // Give the client some time to receive the data
    client.stop();  // Close the connection after sending data
    Serial.println("Client disconnected");
  }
}

void loop() {
  // Read sensors
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  int rawCO = analogRead(coPin);
  coPpm = calculateCOppm(rawCO);
  // Handle incoming UDP packets
  handleUDP();

  // Handle TCP connections
  handleTCP();

  // Update the display
  updateUI();

  delay(1000); // Update every second
}

float calculateCOppm(int rawValue) {
  // Dummy CO ppm calculation, replace with actual formula
  return rawValue * 0.1;
}