#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h> // Libreria per la gestione OTA

// Variabili globali
String wifi_ssid;
String wifi_password;
String mqtt_server;
String mqtt_user;
String mqtt_password;
int mqtt_port;
int soil_sensor_pin;
int pump_pin;
int relay_pin;
float humidity_threshold;
int pump_duration;
unsigned long measurement_interval;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long previousMillis = 0;

void logMessage(const String &message)
{
  Serial.print("[");
  Serial.print(millis());
  Serial.print("] ");
  Serial.println(message);
}

void loadConfiguration()
{
  if (!SPIFFS.begin(true))
  {
    logMessage("Errore durante il montaggio di SPIFFS.");
    return;
  }

  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile)
  {
    logMessage("Impossibile aprire il file di configurazione.");
    return;
  }

  size_t size = configFile.size();
  if (size > 1024)
  {
    logMessage("Il file di configurazione è troppo grande.");
    return;
  }

  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, buf.get());

  if (error)
  {
    logMessage("Errore nel parsing del file di configurazione.");
    return;
  }

  wifi_ssid = doc["network"]["wifi_ssid"].as<String>();
  wifi_password = doc["network"]["wifi_password"].as<String>();
  mqtt_server = doc["network"]["mqtt_server"].as<String>();
  mqtt_user = doc["network"]["mqtt_user"].as<String>();
  mqtt_password = doc["network"]["mqtt_password"].as<String>();
  mqtt_port = doc["network"]["mqtt_port"];
  soil_sensor_pin = doc["device"]["soil_sensor_pin"];
  pump_pin = doc["device"]["pump_pin"];
  relay_pin = doc["device"]["relay_pin"];
  humidity_threshold = doc["device"]["humidity_threshold"];
  pump_duration = doc["device"]["pump_duration"];
  measurement_interval = doc["device"]["measurement_interval"];

  logMessage("Configurazione caricata correttamente.");
}

void reconnectWiFi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    logMessage("Tentativo di riconnessione WiFi...");
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    logMessage("WiFi riconnesso.");
  }
}

void reconnectMQTT()
{
  while (!client.connected())
  {
    logMessage("Tentativo di riconnessione al server MQTT...");
    if (client.connect("ESP32Client", mqtt_user.c_str(), mqtt_password.c_str()))
    {
      logMessage("MQTT riconnesso.");
    }
    else
    {
      logMessage("Connessione fallita al server MQTT.");
      delay(2000);
    }
  }
}

int readSoilHumidity()
{
  const int numReadings = 10;
  int total = 0;

  for (int i = 0; i < numReadings; i++)
  {
    total += analogRead(soil_sensor_pin);
    delay(10);
  }

  return total / numReadings;
}

void managePump(int soil_humidity)
{
  static unsigned long pumpStartTime = 0;
  static bool pumpActive = false;

  if (soil_humidity < humidity_threshold && !pumpActive)
  {
    pumpActive = true;
    pumpStartTime = millis();
    digitalWrite(pump_pin, HIGH);
    digitalWrite(relay_pin, HIGH);
    logMessage("Pompa attivata.");
  }

  if (pumpActive && millis() - pumpStartTime >= pump_duration * 1000)
  {
    pumpActive = false;
    digitalWrite(pump_pin, LOW);
    digitalWrite(relay_pin, LOW);
    logMessage("Pompa disattivata.");
  }
}

void setup()
{
  Serial.begin(115200);
  loadConfiguration();

  pinMode(soil_sensor_pin, INPUT);
  pinMode(pump_pin, OUTPUT);
  pinMode(relay_pin, OUTPUT);

  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  logMessage("WiFi connesso.");

  // Configurazione OTA
  ArduinoOTA.onStart([]()
                     { logMessage("Inizio aggiornamento OTA..."); });
  ArduinoOTA.onEnd([]()
                   { logMessage("Aggiornamento OTA completato."); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { logMessage("OTA Progress: " + String((progress / (total / 100))) + "%"); });
  ArduinoOTA.onError([](ota_error_t error)
                     { logMessage("Errore OTA: " + String(error)); });
  ArduinoOTA.begin();

  client.setServer(mqtt_server.c_str(), mqtt_port);
}

void loop()
{
  reconnectWiFi();
  if (!client.connected())
  {
    reconnectMQTT();
  }
  // Gestione OTA
  ArduinoOTA.handle();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= measurement_interval)
  {
    previousMillis = currentMillis;

    int soil_humidity = readSoilHumidity();
    logMessage("Umidità del suolo: " + String(soil_humidity));

    managePump(soil_humidity);
  }

  client.loop();
}
