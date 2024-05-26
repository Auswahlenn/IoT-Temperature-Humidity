# IoT-Sensor-Data-Project

This project collects temperature and humidity data from ESP32-based IoT devices using MQTT and visualizes the data with Grafana and InfluxDB.

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Setup Instructions](#setup-instructions)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [License](#license)

## Introduction

This project demonstrates how to set up an IoT network of ESP32 devices that measure temperature and humidity, publish the data via MQTT, and visualize it using Grafana and InfluxDB.

## Features

- Collect temperature and humidity data from ESP32 sensors.
- Publish data to an MQTT broker.
- Store data in InfluxDB.
- Visualize data using Grafana.

## Hardware Requirements

- ESP32 development boards
- SHT4x temperature and humidity sensors
- Wi-Fi network

## Software Requirements

- Arduino IDE
- Docker
- MQTT broker (e.g., Mosquitto)
- InfluxDB
- Grafana

## Setup Instructions

### 1. ESP32 Setup

1. **Install the necessary libraries in Arduino IDE:**
   - WiFi
   - PubSubClient
   - ArduinoJson
   - SensirionI2cSht4x

2. **Upload the following code to each ESP32:**

    ```cpp
    #include <Arduino.h>
    #include <SensirionI2cSht4x.h>
    #include <Wire.h>
    #include <WiFi.h>
    #include <PubSubClient.h>
    #include <ArduinoJson.h>

    // Wi-Fi credentials
    const char* ssid = "your-SSID";     // Replace with your network SSID
    const char* password = "your-PASSWORD"; // Replace with your network password

    // MQTT broker
    const char* mqtt_server = "your-MQTT-server"; // Replace with your MQTT broker address
    const int mqtt_port = 1883; // MQTT port

    #define SDA 41
    #define SCL 40

    #define NO_ERROR 0 // Define NO_ERROR

    SensirionI2cSht4x sensor;

    static char errorMessage[64];
    static int16_t error;

    WiFiClient espClient;
    PubSubClient client(espClient);

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
        doc["device"] = "ESP1";  // Change this to a unique ID for each device
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
            if (client.connect("ESP32Client")) { // Change this to a unique client ID
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
    ```

### 2. Docker Setup

1. **Create a `docker-compose.yml` file:**

    ```yaml
    version: '3.1'

    services:
      influxdb:
        image: influxdb:2.0
        ports:
          - "8086:8086"
        environment:
          - INFLUXDB_DB=data
          - INFLUXDB_ADMIN_USER=admin
          - INFLUXDB_ADMIN_PASSWORD=adminadmin
        volumes:
          - influxdb-storage:/var/lib/influxdb

      grafana:
        image: grafana/grafana
        ports:
          - "3000:3000"
        environment:
          - GF_SECURITY_ADMIN_PASSWORD=admin
        depends_on:
          - influxdb
        volumes:
          - grafana-storage:/var/lib/grafana

      mosquitto:
        image: eclipse-mosquitto
        ports:
          - "1883:1883"
          - "9001:9001"
        volumes:
          - mosquitto-config:/mosquitto/config
          - mosquitto-data:/mosquitto/data
          - mosquitto-log:/mosquitto/log

      telegraf:
        image: telegraf
        environment:
          - HOST_PROC=/host/proc
          - HOST_SYS=/host/sys
          - HOST_ETC=/host/etc
        volumes:
          - /var/run/docker.sock:/var/run/docker.sock:ro
          - /proc:/host/proc:ro
          - /sys:/host/sys:ro
          - /etc:/host/etc:ro
          - ./telegraf.conf:/etc/telegraf/telegraf.conf:ro
        depends_on:
          - influxdb
          - mosquitto

    volumes:
      influxdb-storage:
      grafana-storage:
      mosquitto-config:
      mosquitto-data:
      mosquitto-log:
    ```

2. **Run Docker Compose:**

    ```bash
    docker-compose up -d
    ```

### 3. Telegraf Configuration

1. **Create a `telegraf.conf` file:**

    ```toml
    [[outputs.influxdb_v2]]
      urls = ["http://influxdb:8086"]
      token = "your-influxdb-token"
      organization = "your-org"
      bucket = "data"

    [[inputs.mqtt_consumer]]
      servers = ["tcp://mosquitto:1883"]
      topics = [
        "sensor/ESP1",
        "sensor/ESP2"
      ]
      qos = 0
      connection_timeout = "30s"
      persistent_session = false
      data_format = "json"
    ```

2. **Restart Telegraf to apply the configuration:**

    ```bash
    docker-compose restart telegraf
    ```

### 4. Grafana Setup

1. **Access Grafana:**
   - Open your browser and navigate to `http://localhost:3000`.
   - Log in with the default username (`admin`) and password (`adminadmin`).

2. **Add InfluxDB as a Data Source:**
   - Go to **Configuration** > **Data Sources**.
   - Add a new InfluxDB data source.
   - Set the URL to `http://influxdb:8086`.
   - Enter the InfluxDB token, organization, and bucket details.

3. **Create a Dashboard:**
   - Go to **Create** > **Dashboard**.
   - Add new panels to visualize the temperature and humidity data from your devices.

## Usage

1. Power on your ESP32 devices.
2. Ensure they are connected to your Wi-Fi network.
3. The devices will publish temperature and humidity data to the MQTT broker.
4. Telegraf will consume the data and store it in InfluxDB.
5. Visualize the data in Grafana.

## Project Structure

- `ESP32`: Arduino code for ESP32 devices.
- `docker-compose.yml`: Docker Compose file for setting up InfluxDB, Grafana, Mosquitto, and Telegraf.
- `telegraf.conf`: Configuration file for Telegraf.

## Contributing

1. Fork the repository.
2. Create a new branch.
3. Make your changes.
4. Submit a pull request.

## License

This project is licensed under the MIT License.
