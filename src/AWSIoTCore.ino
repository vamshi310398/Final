#include "application.h"
#include "MQTT-TLS.h"
#include "keys.h"
#include "ArduinoJson.h"

SerialLogHandler logHandler;

void callback(char *topic, byte *payload, unsigned int length);

const char amazonIoTRootCaPem[] = AMAZON_IOT_ROOT_CA_PEM;
const char clientKeyCrtPem[] = CELINT_KEY_CRT_PEM;
const char clientKeyPem[] = CELINT_KEY_PEM;

MQTT client("a3bpvmc2wz7ctn-ats.iot.us-east-1.amazonaws.com", 8883, callback);

#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
unsigned long lastSync = millis();
int counter = 0;

// Motor status
bool motorStatus = false;

void setup()
{
  if (millis() - lastSync > ONE_DAY_MILLIS)
  {
    Particle.syncTime();
    lastSync = millis();
  }

  pinMode(D2, OUTPUT);
  digitalWrite(D2, HIGH); // Default state

  // Initialize your capacitive soil sensor
  pinMode(A2, INPUT);

  RGB.control(true);
  client.enableTls(amazonIoTRootCaPem, sizeof(amazonIoTRootCaPem),
                   clientKeyCrtPem, sizeof(clientKeyCrtPem),
                   clientKeyPem, sizeof(clientKeyPem));
  Log.info("TLS enabled");

  bool status = client.connect("psych");

  if (status)
  {
    Log.info("Connection completed");
    client.subscribe("power");
  }
  else
  {
    Log.info("Connection not completed");
  }
}

void loop()
{
  if (client.isConnected())
  {
    client.loop();

    // Read data from the capacitive soil sensor
    int soilMoisture = analogRead(A2);

    // Update motor status immediately
    digitalWrite(D2, motorStatus ? LOW : HIGH);

    // Create JSON document
    DynamicJsonDocument jsonDocument(256);

    // Add data to the JSON document
    jsonDocument["client"] = "psych";
    jsonDocument["soil_moisture"] = soilMoisture;
    jsonDocument["motor_status"] = motorStatus ? "on" : "off";

    // Serialize the JSON document to a char array
    char jsonString[256];
    serializeJson(jsonDocument, jsonString, sizeof(jsonString));

    // Publish the message to the "led" MQTT topic
    client.publish("led", jsonString);
  }

  delay(200);
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Log.info("Received Data on topic %s", topic);
  char p[length + 1];
  memcpy(p, payload, length);
  p[length] = '\0'; // Null-terminate the string

  DynamicJsonDocument jsonDocument(256);

  // Deserialize the JSON payload
  DeserializationError error = deserializeJson(jsonDocument, p);

  if (error)
  {
    Log.error("JSON parsing failed: %s", error.c_str());
    Log.error("Payload: %s", p);
    return;
  }

  // Extract "message" value from JSON
  const char *status = jsonDocument["message"];

  if (strcmp(topic, "power") == 0)
  {
    if (strcmp(status, "ON") == 0)
      motorStatus = true;
    else if (strcmp(status, "OFF") == 0)
      motorStatus = false;
  }
}

