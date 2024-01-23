// ------------------------------LIBRERÍAS------------------------------
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>

// ------------------------Definición de pines---------------------------
#define RELAY_PIN 12
#define DRY_LED 13
#define ON_LED 14
#define DRY_PIN 15
#define ON_PIN 16

// ------------------------------CONSTANTES------------------------------
const char *ssid = "Flores 2.4GHz";
const char *password = "Lilas549";
const char *mqtt_server = "192.168.1.11";
const char *clientId = "humidifier";
String clientIp = "";
const String ON = "ON";
const String OFF = "OFF";
const String DRY = "DRY";
const unsigned long drySignalInterval = 10000; // 10 seconds

const char *humidifierStatusTopic = "homeassistant/switch/humidifier/status";
const char *humidifierSetTopic = "homeassistant/switch/humidifier/set";
const char *humidifierIpTopic = "homeassistant/text/humidifier/ip";

// ------------------------------VARIABLES------------------------------
volatile bool drySignalDetected = false;
unsigned long lastDrySignalTime = 0;
bool firstDrySignal = true;
bool interruptEnabled = false;

// ----------------------Inicialización de librerías----------------------
WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

// ------------------------------DECLARACIÓN DE FUNCIONES------------------------------

void setup_serial();
void setup_pins();
void setup_wifi();
void setup_ota();
void setup_mqtt();
void connect();
void callback(char *topic, byte *message, unsigned int length);
void handleDrySignal();
void setStatus(String status);
void publishStatus(String status);
void setInterrupt(bool value);

void setup()
{
  // Initialize serial communication, setup pins, and attach the interrupt for the dry signal
  setup_serial();
  setup_pins();
  setup_wifi();
  setup_ota();
  setup_mqtt();
  connect();
  setStatus(OFF);
  setInterrupt(true);
}

void loop()
{
  // Reconnect to the MQTT server if not connected and handle the dry signal if detected
  if (!client.connected())
  {
    connect();
  }
  server.handleClient();
  ElegantOTA.loop();
  client.loop();

  if (drySignalDetected)
  {
    if (firstDrySignal)
    {
      Serial.println("First dry signal detected");
      setStatus(DRY);
      firstDrySignal = false;
      drySignalDetected = false;
      lastDrySignalTime = millis();
    }
  }
  if (!firstDrySignal && millis() - lastDrySignalTime < drySignalInterval)
  {
    if (interruptEnabled == true)
    {
      setInterrupt(false);
    }
  }
  else
  {
    setInterrupt(true);
    firstDrySignal = true;
  }
}

void setup_serial()
{
  Serial.begin(115200);
  delay(5000);
  Serial.println("\nEncendido.");
}
void setup_pins()
{
  // Set the pin modes for the humidifier, LEDs, and sensors
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(ON_PIN, INPUT_PULLUP);
  pinMode(DRY_PIN, INPUT_PULLUP);
  pinMode(ON_LED, OUTPUT);
  pinMode(DRY_LED, OUTPUT);
}
void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  clientIp = WiFi.localIP().toString();
  Serial.printf("\nWiFi connected. IP address: %s\n", clientIp.c_str());
}
void setup_ota()
{
  server.on("/",
            []()
            {
              server.send(200, "text/html", (String("Este es el webserver para subir firmware en <b>") + clientId + String("</b>. <a href=\"http://") + clientIp + String("/update\">Subir firmware</a>")).c_str());
            });

  ElegantOTA.begin(&server); // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");
}
void setup_mqtt()
{
  // Setup the MQTT client
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}
void connect()
{
  // Reconnect to the MQTT server
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(clientId))
    {
      client.subscribe(humidifierSetTopic);
      client.publish(humidifierIpTopic, clientIp.c_str(), true);
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
void callback(char *topic, byte *message, unsigned int length)
{
  // Handle incoming MQTT messages
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if ((String(topic) == humidifierSetTopic))
  {
    setStatus(messageTemp);
  }
}
void setStatus(String status)
{
  publishStatus(status);
  Serial.printf("Setting status to %s\n", status.c_str());
  if (status == DRY)
  {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(ON_LED, LOW);
    digitalWrite(DRY_LED, HIGH);
  }
  else if (status == ON)
  {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(ON_LED, HIGH);
    digitalWrite(DRY_LED, LOW);
  }
  else // OFF
  {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(ON_LED, LOW);
    digitalWrite(DRY_LED, LOW);
  }
}
void publishStatus(String status)
{
  // Publish the status to the MQTT server
  client.publish(humidifierStatusTopic, String(status).c_str(), true);
}
void setInterrupt(bool value)
{
  if (value)
  {
    interruptEnabled = true;
    attachInterrupt(digitalPinToInterrupt(DRY_PIN), handleDrySignal, RISING);
  }
  else
  {
    interruptEnabled = false;
    detachInterrupt(digitalPinToInterrupt(DRY_PIN));
  }
}
void handleDrySignal()
{
  drySignalDetected = true;
}