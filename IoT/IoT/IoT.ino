#include <Arduino.h>
#include <SensirionI2cSht4x.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char* ssid = "dlink-0B78";     // Replace with your network SSID
const char* password = "ezghl38678"; // Replace with your network password

// MQTT broker
const char* mqtt_server = "192.168.0.101"; // Replace with your MQTT broker address
const int mqtt_port = 1883; // MQTT port

#define SDA 41
#define SCL 40

#define NO_ERROR 0 // Define NO_ERROR

SensirionI2cSht4x sensor;

static char errorMessage[64];
static int16_t error;

WiFiClient espClient;
PubSubClient client(espClient);

// Unique client ID
const char* clientId = "ESP1"; // Change this for each device

// Function prototypes
void setup_wifi();
void reconnect();
void errorToString(int16_t error, char* buffer, size_t len);

void setup() {
    Serial.begin(115200);
    setup_wifi();
    
    client.setServer(mqtt_server, mqtt_port);
    reconnect();

    Wire.begin(SDA, SCL);
    sensor.begin(Wire, SHT40_I2C_ADDR_44);

    sensor.softReset();
    delay(10);
    uint32_t serialNumber = 0;
    error = sensor.serialNumber(serialNumber);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute serialNumber(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    Serial.print("serialNumber: ");
    Serial.print(serialNumber);
    Serial.println();
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    float aTemperature = 0.0;
    float aHumidity = 0.0;
    delay(20);
    error = sensor.measureMediumPrecision(aTemperature, aHumidity);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute measureLowestPrecision(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    Serial.print("aTemperature: ");
    Serial.print(aTemperature);
    Serial.print("\t");
    Serial.print("aHumidity: ");
    Serial.print(aHumidity);
    Serial.println();

    // Create JSON object
    StaticJsonDocument<200> doc;
    doc["device"] = clientId;
    doc["temperature"] = aTemperature;
    doc["humidity"] = aHumidity;

    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);

    // Publish sensor data to MQTT
    client.publish("sensor/ESP1", jsonBuffer);

    delay(10000); // Delay 10 seconds between measurements
}

void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect(clientId)) { // Use unique client ID
            Serial.println("connected");
            client.publish("test/topic", "Hello from ESP32");
            client.subscribe("test/topic");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void errorToString(int16_t error, char* buffer, size_t len) {
    if (error == NO_ERROR) {
        strncpy(buffer, "No error", len);
    } else {
        snprintf(buffer, len, "Error code: %d", error);
    }
}
