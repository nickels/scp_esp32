#include "config.h"

#include <OneWire.h>
#include <ArduinoJson.h>
#include <DallasTemperature.h>

#include <WiFiClientSecure.h>
#include <MQTT.h>

#define ONE_WIRE_BUS 5

MQTTClient client(1024);
WiFiClientSecure espClient;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

extern const uint8_t mqtt_certfile_start[] asm("_binary_certificates_cert_full_pem_start");
extern const uint8_t mqtt_certfile_end[] asm("_binary_certificates_cert_full_pem_end");
extern const uint8_t mqtt_cafile_start[] asm("_binary_certificates_rootca_pem_start");
extern const uint8_t mqtt_cafile_end[] asm("_binary_certificates_rootca_pem_end");
extern const uint8_t mqtt_keyfile_start[] asm("_binary_certificates_key_full_pem_start");
extern const uint8_t mqtt_keyfile_end[] asm("_binary_certificates_key_full_pem_end");

float temp;
unsigned long lastMillis = 0;
const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3);

String messageTopic = "measures/";
String messageAckTopic = "ack/";

void connect()
{
  Serial.print("Checking WiFi connection...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println('connected.');

  Serial.print("Connecting MQTT...");
  while (!client.connect((const char *)MQTT_DEVICE_ALTERNATE_ID, false))
  {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("connected!");

  Serial.println("Subscribed to: " + messageAckTopic);
  client.subscribe(messageAckTopic.c_str());
}

void messageReceived(String &topic, String &payload)
{
  Serial.println("Received: " + topic + " - " + payload);
}

void setup()
{
  Serial.begin(115200);

  // Start up the OneWire Dallas Temp Sensor library
  sensors.setResolution(11);
  sensors.begin();

  Serial.print("Initializing WiFi connection...");
  WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("connected.");

  espClient.setCACert((const char *)mqtt_cafile_start);
  espClient.setPrivateKey((const char *)mqtt_keyfile_start);   // for client verification - key
  espClient.setCertificate((const char *)mqtt_certfile_start); // for client verification - certificate

  messageTopic += MQTT_DEVICE_ALTERNATE_ID;
  messageAckTopic += MQTT_DEVICE_ALTERNATE_ID;

  client.begin((const char *)MQTT_ENDPOINT, (int)MQTT_ENDPOINT_PORT, espClient);
  client.onMessage(messageReceived);
  connect();
}

void loop()
{
  client.loop();
  delay(10); // <- fixes some issues with WiFi stability

  if (!client.connected())
  {
    connect();
  };

  // publish a message roughly every five seconds.
  if (millis() - lastMillis > 5000)
  {
    lastMillis = millis();

    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);
    while (!sensors.isConversionComplete())
    {
    }
    
    Serial.println(temp);

    DynamicJsonDocument doc(capacity);
    doc["capabilityAlternateId"] = MQTT_CAPABILITY_ALTERNATE_ID;
    doc["sensorAlternateId"] = MQTT_SENSOR_ALTERNATE_ID;

    JsonArray measures = doc.createNestedArray("measures");
    JsonObject measures_0 = measures.createNestedObject();

    measures_0["temperature"] = temp;

    char buffer[1024];
    size_t n = serializeJson(doc, buffer);

    client.publish(messageTopic.c_str(), buffer, n);
    Serial.println(messageTopic);
    Serial.println("Message sent");
  }
}