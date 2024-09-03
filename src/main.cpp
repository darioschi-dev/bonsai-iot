#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <ESP_Mail_Client.h>

#pragma region WiFi_settings
const char *ssid = "D-Link-2BE3C3";  // Enter your Wi-Fi name
const char *password = "K3RazVjHpN"; // Enter Wi-Fi password
#pragma endregion WiFi_settings

#pragma region MQTT_Broker
const char *mqtt_broker = "mqtt.darioschiavano.it";
const char *topic = "emqx/esp32";

// credentials for the MQTT broker
// const char *mqtt_username = "emqx";
// const char *mqtt_password = "public";

const int mqtt_port = 1883; // default port for MQTT
#pragma endregion MQTT_Broker

WiFiClient espClient;
PubSubClient client(espClient);

#pragma region setup_variables
long lastMsg = 0; // last time a message was sent to the broker
char msg[50];     // message to send to the broker
int value = 0;    // value to send to the broker

// Adafruit_BME280 bme(BME_CS); // hardware SPI
// Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI

float temperature = 0; // initialize temperature
long humidity = 0;     // initialize humidity

// LED Pin
const int ledPin = 4;

int soil, percentage; // soil, percentage
String stringa = "";

const int AirValue = 520;   // you need to replace this value with Value_1
const int WaterValue = 260; // you need to replace this value with Value_2
int intervals = (AirValue - WaterValue) / 3;
int soilMoistureValue = 0;
#pragma endregion setup_variables

// uncomment the following lines if you're using SPI
/*#include <SPI.h>
#define BME_SCK 18
#define BME_MISO 19
#define BME_MOSI 23
#define BME_CS 5*/

// comment by me
// Adafruit_BME280 bme; // I2C
// end comment

// enter in sleep mode for 5 seconds and wake up by timer
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 600      /* Time ESP32 will go to sleep (in seconds) */

// RTC_DATA_ATTR is a variable that is stored in RTC memory and will be retained after deep sleep
RTC_DATA_ATTR int bootCount = 0; // count the number of boot

// Preferences object, used to store data in RTC memory
Preferences preferences;

#pragma region SMTP_settings

/** The smtp host name e.g. smtp.gmail.com for GMail or smtp.office365.com for Outlook or smtp.mail.yahoo.com */
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

/* The sign in credentials */
#define AUTHOR_EMAIL "YOUR_EMAIL@XXXX.com"
#define AUTHOR_PASSWORD "YOUR_EMAIL_APP_PASS"

/* Recipient's email*/
#define RECIPIENT_EMAIL "RECIPIENTE_EMAIL@XXXX.com"

/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

#pragma endregion SMTP_settings

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
}

// function to read the soil moisture value and return the percentage
long igro()
{
  soil = analogRead(A0);
  percentage = map(soil, 4095, 0, 0, 100);
  Serial.print("Valore misurato: ");
  Serial.println(soil);
  Serial.println("Valore convertito: ");
  Serial.println(percentage);
  if (percentage < 20)
  {
    Serial.println("Turn on the irrigation pump");
    // todo: insert write message for need turn on the irrigation pump
  }

  // continue on node-red for check time, only after sunset --> connect to meteo's services API and check input by the float tank (galleggiante serbatoio) and if the voltage battery is enough

  /*

    display.setTextSize(1);
    display.setTextColor(1);
    display.setCursor(0, 0);
    display.println("Umidita' soil:");

    display.setTextSize(2);
    display.setTextColor(1,0);
    display.setCursor(0, 15);
    display.println(percentage);

    display.setTextSize(2);
    display.setTextColor(1);
    display.setCursor(25, 15);
    display.println("%");

    display.display();
    */

  return percentage;
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

/**
 * Connect to the Wi-Fi network
 */
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
  // Print local IP address and start web server
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

/**
 * Setup system
 */
void setup()
{
  // Set software serial baud to 115200;
  Serial.begin(115200);

  delay(1000); // Take some time to open up the Serial Monitor

  setup_wifi();
  pinMode(ledPin, OUTPUT);

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

  // set the soil moisture value on RTC memory
  soilMoistureValue = percentage;
  preferences.begin("esp32", false);
  preferences.putUInt("soilMoistureValue", soilMoistureValue);
  preferences.end();

  // Publish and subscribe
  client.publish(topic, "Hi, I'm AZ-DELIVERY ESP32");
  client.subscribe(topic);

  // Increment boot number and print it every reboot
  ++bootCount;
  Serial.print("Boot number: ");
  Serial.println(bootCount);

  // Print the wakeup reason for ESP32
  print_wakeup_reason();

  /*
  First we configure the wake up source
  We set our ESP32 to wake up every 5 seconds
  */
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

  Serial.flush();

  /*
   Now that we have setup a wake cause and if needed setup the
   peripherals state in deep sleep, we can now start going to
   deep sleep.
   In the case that no wake up sources were provided but deep
   sleep was started, it will sleep forever unless hardware
   reset occurs.
   */
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
}

void reconnect()
{
  Serial.println("Attempting WiFi connection...");
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
    humidity = igro();

    // set the soil moisture value on RTC memory
    preferences.begin("esp32", false);
    soilMoistureValue = preferences.getUInt("soilMoistureValue", 0);
    // if the soil moisture value is different of 1% from the saved value, then update the value
    if (abs(soilMoistureValue - percentage) > 1)
    {
      Serial.println("Updating soil moisture value");
      preferences.putUInt("soilMoistureValue", percentage);
      preferences.end();
    }

    // Convert the value to a char array
    char humString[8];
    dtostrf(humidity, 1, 2, humString);
    Serial.print("Humidity: ");
    Serial.println(humString);

    // Publish the value to the broker
    client.publish("esp32/humidity", humString);
  }
}


