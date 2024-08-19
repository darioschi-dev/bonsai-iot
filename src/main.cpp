#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// WiFi
const char *ssid = "D-Link-2BE3C3";  // Enter your Wi-Fi name
const char *password = "K3RazVjHpN"; // Enter Wi-Fi password

// MQTT Broker
const char *mqtt_broker = "192.168.1.158";
const char *topic = "emqx/esp32";
// const char *mqtt_username = "emqx";
// const char *mqtt_password = "public";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

int suolo, percentuale;
String stringa = "";

const int AirValue = 520;   // you need to replace this valuewith Value_1
const int WaterValue = 260; // you need to replace this valuewith Value_2
int intervals = (AirValue - WaterValue) / 3;
int soilMoistureValue = 0;

// uncomment the following lines if you're using SPI
/*#include <SPI.h>
#define BME_SCK 18
#define BME_MISO 19
#define BME_MOSI 23
#define BME_CS 5*/

// comment by me
// Adafruit_BME280 bme; // I2C
// end comment

// Adafruit_BME280 bme(BME_CS); // hardware SPI
// Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI
float temperature = 0;
long humidity = 0;

// LED Pin
const int ledPin = 4;

// comment by me
long igro()
{
  suolo = analogRead(A0);

  soilMoistureValue = analogRead(A0);
  if (soilMoistureValue > WaterValue && soilMoistureValue < (WaterValue + intervals))
  {
    Serial.println("VeryWet");
  }
  else if (soilMoistureValue > (WaterValue + intervals) && soilMoistureValue < (AirValue - intervals))
  {
    Serial.println("Wet");
  }
  else if (soilMoistureValue < AirValue && soilMoistureValue > (AirValue - intervals))
  {
    Serial.println("Dry");
  }

  percentuale = map(suolo, 4095, 0, 0, 100);
  Serial.print("Valore misurato: ");
  Serial.println(suolo);
  Serial.println("Valore convertito: ");
  Serial.println(percentuale);
  if (percentuale < 20)
  {
    // todo: insert write message for need turn on the irrigation pump
  }

  // continue on node-red for check time, only after sunset --> connect to meteo's services API and check input by the float tank (galleggiante serbatoio) and if the voltage battery is enough

  /*

    display.setTextSize(1);
    display.setTextColor(1);
    display.setCursor(0, 0);
    display.println("Umidita' suolo:");

    display.setTextSize(2);
    display.setTextColor(1,0);
    display.setCursor(0, 15);
    display.println(percentuale);

    display.setTextSize(2);
    display.setTextColor(1);
    display.setCursor(25, 15);
    display.println("%");

    display.display();
    */

  return percentuale;
}

void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  if (String(topic) == "esp32/output")
  {
    Serial.print("Changing output to ");
    if (messageTemp == "on")
    {
      Serial.println("on");
      // digitalWrite(ledPin, HIGH);
    }
    else if (messageTemp == "off")
    {
      Serial.println("off");
      // digitalWrite(ledPin, LOW);
    }
  }
}

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // setup OTA (Over-The-Air)
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
      .onStart([]()
               {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + String(type)); })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  ArduinoOTA.begin();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  // Set software serial baud to 115200;
  Serial.begin(115200);
  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  // status = bme.begin();

  // comment by me
  // if (!bme.begin(0x76)) {
  //   Serial.println("Could not find a valid BME280 sensor, check wiring!");
  //   while (1);
  // }
  setup_wifi();
  // client.setServer(mqtt_server, 1883);
  // client.setCallback(callback);
  // pinMode(ledPin, OUTPUT);

  /*
  // Connecting to a WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the Wi-Fi network");
*/

  // connecting to a mqtt broker
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected())
  {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str()))
    {
      Serial.println("Public PI MQTT broker connected");
    }
    else
    {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  // Publish and subscribe
  client.publish(topic, "Hi, I'm AZ-DELIVERY ESP32");
  client.subscribe(topic);
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client"))
    {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
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

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  ArduinoOTA.handle();
  client.loop();

  long now = millis();
  if (now - lastMsg > 1000)
  {
    lastMsg = now;

    // Temperature in Celsius

    // commented by me
    // temperature = bme.readTemperature();

    temperature = random(0, 99);

    // Uncomment the next line to set temperature in Fahrenheit
    // (and comment the previous temperature line)
    // temperature = 1.8 * bme.readTemperature() + 32; // Temperature in Fahrenheit

    // Convert the value to a char array
    char tempString[8];
    dtostrf(temperature, 1, 2, tempString);
    Serial.print("Temperature: ");
    Serial.println(tempString);
    client.publish("esp32/temperature", tempString);

    // commented by me
    // humidity = bme.readHumidity();

    humidity = igro();

    // Convert the value to a char array
    char humString[8];
    dtostrf(humidity, 1, 2, humString);
    Serial.print("Humidity: ");
    Serial.println(humString);
    client.publish("esp32/humidity", humString);
  }
}
