#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h> // ESP32-compatible Servo library

// Wi-Fi Credentials
const char* ssid = "YourWiFiSSID"; // Replace with your Wi-Fi SSID
const char* password = "YourWiFiPassword"; // Replace with your Wi-Fi password

// TCP Server Configuration
WiFiServer server(8080); // TCP server on port 8080
WiFiClient client;

// Sensor Pins
#define TEMP_SENSOR_PIN 4
#define TURBIDITY_SENSOR_PIN 32
#define WATER_SENSOR_PIN 33
#define PH_SENSOR_PIN 34

// Relay and Servo Pins
#define RELAY_PIN 25
#define SERVO_PIN 26

// Ideal Ranges
const float idealTempMin = 24.0; // °C
const float idealTempMax = 27.0; // °C
const float idealPHMin = 6.5;    // pH
const float idealPHMax = 8.0;    // pH

// Temperature Sensor Setup
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

// Servo Control
Servo feedingServo;

void setup() {
  // Serial Monitor
  Serial.begin(115200);

  // Initialize Wi-Fi
  connectWiFi();

  // Start TCP Server
  server.begin();

  // Sensor Initialization
  tempSensor.begin();
  pinMode(TURBIDITY_SENSOR_PIN, INPUT);
  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(PH_SENSOR_PIN, INPUT);

  // Relay and Servo Initialization
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Ensure relay is OFF initially
  feedingServo.attach(SERVO_PIN); // Attach the servo to SERVO_PIN

  Serial.println("Smart Aquarium System Initialized!");
}

void connectWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");
}

void loop() {
  // Check for Incoming Client Connections
  client = server.available();
  if (client) {
    Serial.println("Client Connected!");
    while (client.connected()) {
      if (client.available()) {
        String message = client.readStringUntil('\n');
        message.trim(); // Remove extra whitespace
        Serial.println("Received Message: " + message);

        // Respond to Specific Command
        if (message.equalsIgnoreCase("SEND_VALUES")) {
          sendSensorValues();
        } else {
          client.println("Unknown Command! Send 'SEND_VALUES' to receive sensor data.");
        }
      }
    }
    client.stop();
    Serial.println("Client Disconnected!");
  }

  // Perform Regular Aquarium Tasks
  unsigned long currentTime = millis();
  performRegularTasks(currentTime);
}

void performRegularTasks(unsigned long currentTime) {
  // Read Sensors
  float temperature = readTemperature();
  float turbidity = readTurbidity();
  float waterLevel = readWaterLevel();
  float pH = readPH();

  // Print Sensor Readings
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" °C | Turbidity: ");
  Serial.print(turbidity);
  Serial.print(" NTU | Water Level: ");
  Serial.print(waterLevel);
  Serial.print(" cm | pH: ");
  Serial.println(pH);

  // Check Ideal Ranges and Notify
  checkTemperatureRange(temperature);
  checkPHRange(pH);

  // Trigger Feeding Mechanism Every Interval
  const unsigned long interval = 10000; // 10 seconds interval for feeding mechanism
  static unsigned long previousTime = 0;
  if (currentTime - previousTime >= interval) {
    previousTime = currentTime;
    Serial.println("Activating Feeding Mechanism");
    rotateServo();
  }

  // Auto Pump Control (e.g., if water level goes below a threshold)
  if (waterLevel < 10.0) { // Example condition
    Serial.println("Water level low! Activating pump...");
    activatePump();
  }

  delay(1000); // Delay for stability
}

// Send Sensor Values to TCP Client
void sendSensorValues() {
  float temperature = readTemperature();
  float turbidity = readTurbidity();
  float waterLevel = readWaterLevel();
  float pH = readPH();

  String data = "Temperature: " + String(temperature) + " °C\n" +
                "Turbidity: " + String(turbidity) + " NTU\n" +
                "Water Level: " + String(waterLevel) + " cm\n" +
                "pH: " + String(pH);

  client.println(data); // Send data back to client
  Serial.println("Sensor Values Sent to Client:\n" + data);
}

// Sensor Read Functions
float readTemperature() {
  tempSensor.requestTemperatures();
  return tempSensor.getTempCByIndex(0); // Read temperature
}

float readTurbidity() {
  int sensorValue = analogRead(TURBIDITY_SENSOR_PIN);
  float voltage = sensorValue * (3.3 / 4095.0); // 12-bit ADC
  return map(voltage, 0, 3.3, 0, 3000); // Example mapping
}

float readWaterLevel() {
  int sensorValue = analogRead(WATER_SENSOR_PIN);
  float voltage = sensorValue * (3.3 / 4095.0); // 12-bit ADC
  return voltage * 100; // Example scaling for water level in cm
}

float readPH() {
  int sensorValue = analogRead(PH_SENSOR_PIN);
  float voltage = sensorValue * (3.3 / 4095.0); // 12-bit ADC
  return map(voltage, 0, 3.3, 0, 14); // Example pH mapping
}

// Check Ideal Temperature Range
void checkTemperatureRange(float temperature) {
  if (temperature < idealTempMin || temperature > idealTempMax) {
    String message = "ALERT! Temperature out of range: " + String(temperature) + " °C";
    sendNotification(message);
    Serial.println(message);
  }
}

// Check Ideal pH Range
void checkPHRange(float pH) {
  if (pH < idealPHMin || pH > idealPHMax) {
    String message = "ALERT! pH out of range: " + String(pH);
    sendNotification(message);
    Serial.println(message);
  }
}

// Servo Control
void rotateServo() {
  for (int pos = 0; pos <= 180; pos++) {
    feedingServo.write(pos); // Move to the specified position
    delay(15);
  }
  for (int pos = 180; pos >= 0; pos--) {
    feedingServo.write(pos); // Return to the initial position
    delay(15);
  }
}

// Pump Control
void activatePump() {
  digitalWrite(RELAY_PIN, HIGH); // Turn ON pump
  Serial.println("Pump Activated!");
  delay(5000); // Pump ON for 5 seconds
  digitalWrite(RELAY_PIN, LOW); // Turn OFF pump
  Serial.println("Pump Deactivated!");
}

// Send Notification via TCP
void sendNotification(String message) {
  if (client.connected()) { // Send to connected TCP client
    client.println(message); 
    Serial.println("Notification sent: " + message);
  } else {
    Serial.println("No client connected to send notification.");
  }
}
