#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <ESP_Mail_Client.h>
#include <ESPAsyncWebServer.h>

#pragma region WiFi_settings
const bool wokwi = false;

const char *ssid = wokwi ? "Wokwi-GUEST" : "D-Link-2BE3C3"; // Enter your Wi-Fi name
const char *password = wokwi ? "" : "K3RazVjHpN";           // Enter Wi-Fi password
#pragma endregion WiFi_settings

#pragma region NTP_settings
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;
#pragma endregion NTP_settings

/**

#pragma region MQTT_Broker
const char *mqtt_broker = "mqtt.darioschiavano.it";
const char *topic = "emqx/esp32";

// credentials for the MQTT broker
// const char *mqtt_username = "emqx";
// const char *mqtt_password = "public";

const int mqtt_port = 1883; // default port for MQTT
#pragma endregion MQTT_Broker

 */

WiFiClient espClient;
// PubSubClient client(espClient);

// Preferences object, used to store data in RTC memory
Preferences preferences;

/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;

/* Declare the message class */
SMTP_Message message;

/* Declare the Session_Config for user defined session credentials */
Session_Config config;

/** Declare the server */
AsyncWebServer server(80);

/** Declare the json document */
JsonDocument doc;

#pragma region setup_variables

const int value_ref = 20; // reference value for the soil moisture

// long lastMsg = 0; // last time a message was sent to the broker
// char msg[50];     // message to send to the broker
// int value = 0;    // value to send to the broker

long humidity = 0; // initialize humidity

const int ledPin = 4;     // LED Pin
const int sensorPin = 32; // Sensor Pin for the water level -- input
const int pumpPin = 26;   // Pump Pin -- output

int soil, percentage; // soil, percentage
String stringa = "";

int soilMoistureValue = 0;
const bool debug = true;    // debug mode, if true the ESP32 will not go to sleep and will not send an email
const bool pump = false;    // pump status, if true the pump is on, if false the pump is off
const bool use_pomp = true; // if true the pump is used, if false the pump is not used
#pragma endregion setup_variables

// enter in sleep mode for 5 seconds and wake up by timer
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 3600     /* Time ESP32 will go to sleep (in seconds) */

// RTC_DATA_ATTR is a variable that is stored in RTC memory and will be retained after deep sleep
RTC_DATA_ATTR int bootCount = 0; // count the number of boot

#pragma region SMTP_settings

/** The smtp host name e.g. smtp.gmail.com for GMail or smtp.office365.com for Outlook or smtp.mail.yahoo.com */
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

/* The sign in credentials */
#define AUTHOR_EMAIL "dario.schiavano@gmail.com"
#define AUTHOR_PASSWORD "sazf pnfo jode zbig"

/* Recipient's email*/
#define RECIPIENT_EMAIL "dario.schiavano@gmail.com"

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success())
  {
    // ESP_MAIL_PRINTF used in the examples is for format printing via debug Serial port
    // that works for all supported Arduino platform SDKs e.g. AVR, SAMD, ESP32 and ESP8266.
    // In ESP8266 and ESP32, you can use Serial.printf directly.

    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);

      // In case, ESP32, ESP8266 and SAMD device, the timestamp get from result.timestamp should be valid if
      // your device time was synched with NTP server.
      // Other devices may show invalid timestamp as the device time was not set i.e. it will show Jan 1, 1970.
      // You can call smtp.setSystemTime(xxx) to set device time manually. Where xxx is timestamp (seconds since Jan 1, 1970)

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
      // Remove the line that calls the sendCustomData method
      Serial.println("----------------\n");

      // You need to clear sending result as the memory usage will grow up.
      smtp.sendingResult.clear();
    }
  }
}

#pragma endregion SMTP_settings

// <!DOCTYPE html>
// <html lang="it">
// <head>
//     <meta charset="UTF-8">
//     <meta name="viewport" content="width=device-width, initial-scale=1.0">
//     <title>Controllo Pompa</title>
//     <style>
//         body {
//             display: flex;
//             flex-direction: column;
//             justify-content: center;
//             align-items: center;
//             height: 100vh;
//             background-color: #f0f0f0;
//             font-family: Arial, sans-serif;
//             margin: 0;
//         }
//         .row {
//             display: flex;
//             margin-bottom: 20px;
//             align-items: center;
//         }
//         button {
//             padding: 10px 20px;
//             font-size: 20px;
//             margin: 0 10px;
//             border: none;
//             border-radius: 5px;
//             cursor: pointer;
//             transition: background-color 0.3s ease;
//         }
//         button.on {
//             background-color: green;
//             color: white;
//         }
//         button.off {
//             background-color: red;
//             color: white;
//         }
//         button.standby {
//             background-color: #555;
//             color: white;
//         }
//         p {
//             font-size: 18px;
//             margin: 0 10px;
//         }
//         img {
//             vertical-align: middle;
//             width: 40px;
//             height: 40px;
//         }
//     </style>
// </head>
// <body>

//     <div class="row">
//         <button class="on">Accendi</button>
//         <button class="off">Spegni</button>
//     </div>

//     <div class="row">
//         <p>Stato pompa:
//             <span id="pump-status">Spenta</span>
//         </p>
//         <p>Icone:
//             <span id="pump-icon">
//                 <img src="https://img.icons8.com/?size=100&id=63312&format=png&color=#FA5252" alt="Pompa Icona" />
//             </span>
//         </p>
//     </div>

//     <div class="row">
//         <button class="standby">Standby</button>
//     </div>

//     <script>
//         const pumpStatus = document.getElementById('pump-status');
//         const pumpIcon = document.getElementById('pump-icon');

//         const buttons = document.querySelectorAll('button');
//         buttons.forEach(button => {
//             button.addEventListener('click', () => {
//                 const action = button.classList.contains('on') ? 'on' : 'off';
//                 fetch(`/api/pump/${action}`, { method: 'POST' })
//                     .then(response => response.json())
//                     .then(data => {
//                         pumpStatus.textContent = data.status === 'on' ? 'Accesa' : 'Spenta';
//                         pumpIcon.innerHTML = data.status === 'on'
//                             ? '<img src="https://img.icons8.com/?size=100&id=63312&format=png&color=000000" />'
//                             : '<img src="https://img.icons8.com/?size=100&id=63312&format=png&color=#FA5252" />';
//                     });
//             });
//         });

//         const standbyButton = document.querySelector('.standby');
//         standbyButton.addEventListener('click', () => {
//             fetch('/api/standby', { method: 'POST' });
//         });
//     </script>
// </body>
// </html>

String prepare_html_page()
{
  return R"(
    <!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Controllo Pompa</title>
    <script src="https://kit.fontawesome.com/7c86fa4f5a.js" crossorigin="anonymous"></script>
    <style>
        body {
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            height: 100vh;
            background-color: #f0f0f0;
            font-family: Arial, sans-serif;
            margin: 0;
        }
        .row {
            display: flex;
            margin-bottom: 20px;
            align-items: center;
        }
        button {
            padding: 10px 20px;
            font-size: 20px;
            margin: 0 10px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            transition: background-color 0.3s ease;
        }
        button.on {
            background-color: green;
            color: white;
        }
        button.off {
            background-color: red;
            color: white;
        }
        button.standby {
            background-color: #555;
            color: white;
        }
        p {
            font-size: 18px;
            margin: 0 10px;
        }
        img {
            vertical-align: middle;
            width: 40px;
            height: 40px;
        }
    </style>
</head>
<body>

    <div class="row">
        <button class="on">Accendi</button>
        <button class="off">Spegni</button>
    </div>

    <div class="row">
        <p>Stato pompa: 
            <span id="pump-status">Spenta</span>
        </p>
        <p>Icone: 
            <span id="pump-icon">
            // icon from fontawesome
                <i class="fas fa-faucet" style="font-size: 40px; color: #FA5252;"></i>
            </span>
        </p>
    </div>

    <div class="row">
        <button class="standby">Standby</button>
    </div>

    <script>
        const pumpStatus = document.getElementById('pump-status');
        const pumpIcon = document.getElementById('pump-icon');

        const buttons = document.querySelectorAll('button');
        buttons.forEach(button => {
            button.addEventListener('click', () => {
                const action = button.classList.contains('on') ? 'on' : 'off';
                fetch(`/api/pump/${action}`, { method: 'POST' })
                    .then(response => response.json())
                    .then(data => {
                        pumpStatus.textContent = data.status === 'on' ? 'Accesa' : 'Spenta';
                        console.log(data.status);
                        pumpIcon.innerHTML = data.status == 'on'
                            ? '<i class="fas fa-faucet" style="font-size: 40px; color: #000000;"></i>'
                            : '<i class="fas fa-faucet" style="font-size: 40px; color: #FA5252;"></i>';
                            // ? '<img src="https://img.icons8.com/?size=100&id=63312&format=png&color=000000" />'
                            // : '<img src="https://img.icons8.com/?size=100&id=63312&format=png&color=#FA5252" />';
                    });
            });
        });

        const standbyButton = document.querySelector('.standby');
        standbyButton.addEventListener('click', () => {
            fetch('/api/standby', { method: 'POST' });
        });
    </script>
</body>
</html>
)";
}

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
  percentage = map(soil, 4095, 0, 0, 100); // map the value from 0 to 100
  Serial.print("Valore misurato: ");
  Serial.println(soil);
  Serial.println("Valore convertito: ");
  Serial.println(percentage);

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

void check_update_soil(int valueSoil)
{
  // set the soil moisture value on RTC memory
  preferences.begin("esp32", false);
  soilMoistureValue = preferences.getUInt("soilValue", 0); // get the value of the soil moisture saved in RTC memory
  // if the soil moisture value is different of 1% from the saved value, then update the value
  if (abs(soilMoistureValue - valueSoil) > 1)
  {
    Serial.println("Updating soil moisture value");
    preferences.putUInt("soilValue", valueSoil);
    preferences.end();
  }
  else
  {
    Serial.println("Soil moisture value is the same");
  }
}

/**
 * Callback function to handle the message received from the MQTT broker
 */
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

void turnOnPump()
{
  Serial.println("Turning on the pump");
  digitalWrite(pumpPin, LOW);
}

void turnOffPump()
{
  Serial.println("Turning off the pump");
  digitalWrite(pumpPin, HIGH);
}

/**
 * turn on the pump for 3 seconds or while the soil moisture is less than 20%
 * @param timeOn the time to turn on the pump (nullable)
 */
void autoTurnOnPump(int timeOn = 3)
{
  if (use_pomp == false)
  {
    Serial.println("Pump is not used");
    return;
  }
  Serial.println("Turning on the pump");
  // turn on the pump
  digitalWrite(pumpPin, LOW);
  delay(timeOn * 1000); // turn on the pump for the specified time
  Serial.println("Pump is on, waiting for");
  Serial.print(timeOn);
  Serial.print(" seconds\n");
  digitalWrite(pumpPin, HIGH);
}

/**
 * check the water level and return true if the water level is ok, otherwise return false
 */
bool check_water_level()
{
  // int waterLevel = digitalRead(sensorPin);
  int waterLevel = analogRead(A6);
  Serial.println("Water level read: ");
  Serial.println(waterLevel);
  int waterLevelPercentage = map(waterLevel, 4095, 0, 0, 100);
  Serial.println("Water level percentage: ");
  Serial.println(waterLevelPercentage);
  if (waterLevel == HIGH)
  {
    Serial.println("Water level is ok");
    return true;
  }
  else
  {
    Serial.println("Water level is low");
    return false;
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

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Print local IP address and start OTA service
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

void setup_mail(const char *textMessage = NULL)
{
  // setup the mail client

  /*  Set the network reconnection option */
  MailClient.networkReconnect(true);

  /** Enable the debug via Serial port
   * 0 for no debugging
   * 1 for basic level debugging
   *
   * Debug port can be changed via ESP_MAIL_DEFAULT_DEBUG_PORT in ESP_Mail_FS.h
   */
  smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);

  /* Set the session config */
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "";

  /*
  Set the NTP config time
  For times east of the Prime Meridian use 0-12
  For times west of the Prime Meridian add 12 to the offset.
  Ex. American/Denver GMT would be -6. 6 + 12 = 18
  See https://en.wikipedia.org/wiki/Time_zone for a list of the GMT/UTC timezone offsets
  */
  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  // config offset for timezone "Europe/Rome"
  config.time.gmt_offset = 1;
  config.time.day_light_offset = 0;

  /* Set the message headers */
  message.sender.name = F("Igrometer");
  message.sender.email = AUTHOR_EMAIL;
  message.subject = F("Soil moisture alert");
  message.addRecipient(F("Dario"), RECIPIENT_EMAIL);

  uint lastValueSaved = preferences.getUInt("soilValue", 0);
  String htmlMsg = "<div style=\"color:#2f4468;\"><h1>Promemoria, ricordati di innaffiare il bonsai!</h1><p>- Sent from Igrometer ESP32 board</p></div>";

  if (textMessage != NULL)
  {
    htmlMsg = "<div style=\"color:#2f4468;\"><h1>";
    htmlMsg += textMessage;
    htmlMsg += "</h1><p>- Sent from Igrometer ESP32 board</p></div>";
  }

  message.html.content = htmlMsg.c_str();
  message.text.charSet = "us-ascii";
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  // Send raw text message
  // String textMsg = "Hello World! - Sent from ESP board";
  // message.text.content = textMsg.c_str();
  // message.text.charSet = "us-ascii";
  // message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

  /* Connect to the server */
  if (!smtp.connect(&config))
  {
    ESP_MAIL_PRINTF("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  if (!smtp.isLoggedIn())
  {
    Serial.println("\nNot yet logged in.");
  }
  else
  {
    if (smtp.isAuthenticated())
      Serial.println("\nSuccessfully logged in.");
    else
      Serial.println("\nConnected with no Auth.");
  }

  /* Start sending Email and close the session */
  // if (!MailClient.sendMail(&smtp, &message))
  //   ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
}

/**
 * Setup the web server
 */
void setup_webserver()
{
  // Start the server
  server.begin();

  // Print the IP address
  Serial.println(WiFi.localIP());

  // Handle the root URL and response with HTML page in ./assets/index.html
  // Read a file how a string and send it as a response
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", prepare_html_page().c_str()); });
  // Handle the /soil URL
  server.on("/api/soil", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    doc["soil"] = soilMoistureValue;
    doc["soilValue"] = soil;
    doc["soilValueRead"] = analogRead(A0);
    doc["percentage"] = percentage;
    doc["waterLevel"] = check_water_level();
    doc["waterLevelPercentage"] = analogRead(A6);
    doc["waterLevelPercentageMapped"] = map(analogRead(A6), 4095, 0, 0, 100);
    doc["bootCount"] = bootCount;
    doc["lastBoot"] = preferences.getUInt("soilValue", 0);
    bool out = digitalRead(pumpPin);
    // output status
    doc["outputPumpPinStatus"] = out;
    // pomp status
    doc["pumpStatus"] = pump ? "on" : "off";

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
    }
    doc["lastUpdate"] = asctime(&timeinfo);
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

  // Handle the /pump/on URL
  server.on("/api/pump/on", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    turnOnPump();
    // prepare the json response
    doc["status"] = "on";
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

  // Handle the /pump/off URL
  server.on("/api/pump/off", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    turnOffPump();
    // prepare the json response
    doc["status"] = "off";
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

  // Handle the /standby URL
  server.on("/api/standby", HTTP_POST, [](AsyncWebServerRequest *request)
            {

    Serial.println("Going to sleep now");
    esp_deep_sleep_start(); });

  // Handle the 404 URL
  server.onNotFound([](AsyncWebServerRequest *request)
                    { 
                      // prepare the json response
                      doc["status"] = "error";
                      doc["message"] = "Not found";
                      String response;
                      serializeJson(doc, response);
                      request->send(404, "application/json", response); });
}

/**
 * Setup system
 */
void setup()
{
  pinMode(ledPin, OUTPUT);
  pinMode(sensorPin, INPUT);
  // reset the pump
  digitalWrite(pumpPin, HIGH);
  pinMode(pumpPin, OUTPUT);

  // Set software serial baud to 115200;
  Serial.begin(115200);
  delay(1000); // Take some time to open up the Serial Monitor
  if (WiFi.status() != WL_CONNECTED)
  {
    setup_wifi();
  }
  delay(500);

  /* Connect to the server */
  if (!smtp.connect(&config))
  {
    setup_mail();
  }
  delay(500);

  // setup the web server
  setup_webserver();

  // // connecting to a mqtt broker
  // client.setServer(mqtt_broker, mqtt_port);
  // client.setCallback(callback);
  // while (!client.connected())
  // {
  //   String client_id = "esp32-client-";
  //   client_id += String(WiFi.macAddress());
  //   Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
  //   if (client.connect(client_id.c_str()))
  //   {
  //     Serial.println("Public PI MQTT broker connected");
  //   }
  //   else
  //   {
  //     Serial.print("failed with state ");
  //     Serial.print(client.state());
  //     delay(2000);
  //   }
  // }

  // set the soil moisture value on RTC memory
  soilMoistureValue = igro();
  check_update_soil(soilMoistureValue);

  delay(1000);

  // // Publish and subscribe
  // client.publish(topic, "Hi, I'm AZ-DELIVERY ESP32");
  // client.subscribe(topic);

  // Increment boot number and print it every reboot
  ++bootCount;
  Serial.print("Boot number: ");
  Serial.println(bootCount);

  // Print the wakeup reason for ESP32
  print_wakeup_reason();

  // Print the soil moisture value
  Serial.print("Soil moisture value: ");
  Serial.println(soilMoistureValue);

  // Print the soil moisture value saved in RTC memory
  Serial.print("Soil moisture value saved: ");
  Serial.println(preferences.getUInt("soilValue", 0));

  // before to go to sleep, I check if the soil moisture is less than 20% -- in the future I will check the time and the battery voltage
  // if the soil moisture is more than 20%, then send an email to the user
  if (soilMoistureValue > value_ref)
  // in normal condition, the value of the soil moisture is between 15% and 20%
  {
    Serial.println("Soil moisture is more than 20%, sending an email to the user");

    // // send an mail to the user
    // if (!MailClient.sendMail(&smtp, &message))
    //   ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());

    // turn on the pump for 3 seconds
    if (check_water_level())
    {
      // autoTurnOnPump(); // check the water level and turn on the pump if the water level is ok
      Serial.println("Water level is ok, turning on the pump");
    }
    else
    {
      String textMessage = "Water level is low, I prefer to not turn on the pump for security reasons";
      Serial.println(textMessage);
      setup_mail(textMessage.c_str());
      if (!debug)
      {
        if (!MailClient.sendMail(&smtp, &message))
          ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
      }
    }
  }
  else
  {
    Serial.println("Soil moisture is ok, no need to send an email");
  }

  /*
  First we configure the wake up source
  We set our ESP32 to wake up every --> 8 hours, 8 * 3600 seconds
  */
  esp_sleep_enable_timer_wakeup(8ULL * TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every 8 hours");

  Serial.flush();

  if (!debug)
  {
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
}

void loop()
{
  // if (!client.connected())
  // {
  //   reconnect();
  // }
  ArduinoOTA.handle();

  // read the soil moisture value
  soilMoistureValue = igro();
  check_update_soil(soilMoistureValue);

  // read the water level
  if (check_water_level())
  {
    Serial.println("Water level is ok");
  }
  else
  {
    Serial.println("Water level is low");
  }

  delay(5000);
  // test the power of the pump
  // autoTurnOnPump(1);
}
