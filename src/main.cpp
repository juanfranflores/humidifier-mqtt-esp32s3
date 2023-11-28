#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ---------------------------- DEFINICIÓN DE PINES ----------------------------

#define HUMIDIFIER_PIN 12
#define DRY_LED 13
#define ON_LED 14
#define DRY_PIN 15
#define ON_PIN 16

// ---------------------------- CREDENCIALES DE CONEXIÓN ----------------------------

const char *ssid = "Flores 2.4GHz";
const char *password = "Lilas549";
const char *mqtt_server = "192.168.1.11";
const char *clientId = "humidifier";

// ---------------------------- ESTADOS ----------------------------

const String ON = "Encendido";
const String OFF = "Apagado";
const String DRY = "Falta agua";

// ---------------------------- VARIABLES GLOBALES ----------------------------

volatile bool drySignalDetected = false;

//  ---------------------------- TOPICS  ----------------------------

const char *humidifierStatusTopic = "homeassistant/switch/humidifier/status";
const char *humidifierCmdTopic = "homeassistant/switch/humidifier/command";

//  ---------------------------- INSTANCIACIÓN DE OBJETOS ----------------------------
WiFiClient espClient;
PubSubClient client(espClient);

//  ---------------------------- DECLARACIÓN DE FUNCIONES ----------------------------
void setup_wifi();
void reconnect();
void callback(char *topic, byte *message, unsigned int length);
void setStatus(String command);
void publishStatus(const char *topic, String command);
void handleDrySignal();

void setup()
{
  Serial.begin(115200);
  pinMode(HUMIDIFIER_PIN, OUTPUT);
  pinMode(ON_PIN, INPUT_PULLUP);
  pinMode(DRY_PIN, INPUT_PULLUP);
  pinMode(ON_LED, OUTPUT);
  pinMode(DRY_LED, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(DRY_PIN), handleDrySignal, RISING);
  delay(2000);
  Serial.println("Encendido :)");
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  setStatus(OFF);
  reconnect();
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
  if (drySignalDetected)
  {
    Serial.println("Turning off humidifier due to dry signal");
    digitalWrite(HUMIDIFIER_PIN, LOW); // turn off the humidifier
    digitalWrite(ON_LED, LOW);
    digitalWrite(DRY_LED, HIGH);
    publishStatus(humidifierStatusTopic, DRY); // change the status to dry
    drySignalDetected = false;                 // reset the flag
  }
}
//  ---------------------------- DEFINICIÓN DE FUNCIONES ----------------------------
void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientId))
    {
      client.subscribe(humidifierCmdTopic);
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char *topic, byte *message, unsigned int length)
{
  // Llegó un nuevo mensaje
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

  // Leo feeds
  if ((String(topic) == humidifierCmdTopic))
  {
    if (messageTemp == "1")
    {
      setStatus(ON);
    }
    else if (messageTemp == "0")
    {
      setStatus(OFF);
    }
  }
}

void setStatus(String status)
{
  if (status == ON)
  {
    detachInterrupt(digitalPinToInterrupt(DRY_PIN));
    digitalWrite(HUMIDIFIER_PIN, HIGH);
    delay(1000);
    if (digitalRead(DRY_PIN) == LOW) 
    {
      digitalWrite(HUMIDIFIER_PIN, LOW);
      delay(1000);
      digitalWrite(HUMIDIFIER_PIN, HIGH);
      digitalWrite(ON_LED, HIGH);
      digitalWrite(DRY_LED, LOW);
    }
    else
    {
      status = DRY;
      digitalWrite(HUMIDIFIER_PIN, LOW); // turn off the humidifier
      digitalWrite(ON_LED, LOW);
      digitalWrite(DRY_LED, HIGH);
      Serial.println("Turning off humidifier due to dry signal");
    }
  }
  else if (status == OFF)
  {
    digitalWrite(HUMIDIFIER_PIN, LOW);
    digitalWrite(ON_LED, LOW);
    digitalWrite(DRY_LED, LOW);
  }
  publishStatus(humidifierStatusTopic, status);
  Serial.print("Humidifier status: ");
  Serial.println(status);
  delay(1000);
  attachInterrupt(digitalPinToInterrupt(DRY_PIN), handleDrySignal, RISING);
}

void publishStatus(const char *topic, String status)
{
  client.publish(topic, String(status).c_str(), true);
}

void handleDrySignal()
{
  if (digitalRead(DRY_PIN) == HIGH)
  {
    drySignalDetected = true;
  }
}