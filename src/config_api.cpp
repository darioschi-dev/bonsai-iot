#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include "config_api.h"
#include "mqtt.h"   // publishMqtt()

const char* CONFIG_PATH = "/config.json";

// ---------------------------------------------------------------------------
// FS Helpers
// ---------------------------------------------------------------------------

bool mountFS(bool formatOnFail)
{
    if (FS_IMPL.begin(formatOnFail))
        return true;

    if (!formatOnFail)
        return false;

    return FS_IMPL.begin(true);
}

bool writeFileAtomic(const char* path, const String& data)
{
    String tmp = String(path) + ".tmp";

    File f = FS_IMPL.open(tmp, "w");
    if (!f) return false;

    size_t w = f.print(data);
    f.flush();
    f.close();

    if (w != data.length()) {
        FS_IMPL.remove(tmp);
        return false;
    }

    FS_IMPL.remove(path);
    return FS_IMPL.rename(tmp, path);
}

bool readFileToString(const char* path, String& out)
{
    File f = FS_IMPL.open(path, "r");
    if (!f) return false;

    out = f.readString();
    f.close();
    return true;
}

// ---------------------------------------------------------------------------
// JSON <-> Config
// ---------------------------------------------------------------------------

String configToJson(const Config& c)
{
    StaticJsonDocument<2048> d;

    d["wifi_ssid"]     = c.wifi_ssid;
    d["wifi_password"] = c.wifi_password;

    d["mqtt_broker"]   = c.mqtt_broker;
    d["mqtt_port"]     = c.mqtt_port;
    d["mqtt_username"] = c.mqtt_username;
    d["mqtt_password"] = c.mqtt_password;

    d["sensor_pin"]           = c.sensor_pin;
    d["pump_pin"]             = c.pump_pin;
    d["relay_pin"]            = c.relay_pin;
    d["battery_pin"]          = c.battery_pin;
    d["moisture_threshold"]   = c.moisture_threshold;
    d["pump_duration"]        = c.pump_duration;
    d["measurement_interval"] = c.measurement_interval;

    d["use_pump"]  = c.use_pump;
    d["debug"]     = c.debug;

    d["sleep_hours"] = c.sleep_hours;

    d["use_dhcp"]   = c.use_dhcp;
    d["ip_address"] = c.ip_address;
    d["gateway"]    = c.gateway;
    d["subnet"]     = c.subnet;

    d["ota_manifest_url"] = c.ota_manifest_url;
    d["update_server"]    = c.update_server;
    d["config_version"]   = c.config_version;

    String out;
    serializeJson(d, out);
    return out;
}

bool jsonToConfig(const String& json, Config& out)
{
    StaticJsonDocument<2048> d;
    auto err = deserializeJson(d, json);
    if (err) return false;

    if (d.containsKey("wifi_ssid"))     out.wifi_ssid     = d["wifi_ssid"].as<String>();
    if (d.containsKey("wifi_password")) out.wifi_password = d["wifi_password"].as<String>();

    if (d.containsKey("mqtt_broker"))   out.mqtt_broker   = d["mqtt_broker"].as<String>();
    if (d.containsKey("mqtt_port"))     out.mqtt_port     = d["mqtt_port"].as<int>();
    if (d.containsKey("mqtt_username")) out.mqtt_username = d["mqtt_username"].as<String>();
    if (d.containsKey("mqtt_password")) out.mqtt_password = d["mqtt_password"].as<String>();

    if (d.containsKey("sensor_pin"))           out.sensor_pin           = d["sensor_pin"].as<int>();
    if (d.containsKey("pump_pin"))             out.pump_pin             = d["pump_pin"].as<int>();
    if (d.containsKey("relay_pin"))            out.relay_pin            = d["relay_pin"].as<int>();
    if (d.containsKey("battery_pin"))          out.battery_pin          = d["battery_pin"].as<int>();
    if (d.containsKey("moisture_threshold"))   out.moisture_threshold   = d["moisture_threshold"].as<int>();
    if (d.containsKey("pump_duration"))        out.pump_duration        = d["pump_duration"].as<int>();
    if (d.containsKey("measurement_interval")) out.measurement_interval = d["measurement_interval"].as<int>();

    if (d.containsKey("use_pump")) out.use_pump = d["use_pump"].as<bool>();
    if (d.containsKey("debug"))    out.debug    = d["debug"].as<bool>();

    if (d.containsKey("sleep_hours")) out.sleep_hours = d["sleep_hours"].as<int>();

    if (d.containsKey("use_dhcp"))   out.use_dhcp   = d["use_dhcp"].as<bool>();
    if (d.containsKey("ip_address")) out.ip_address = d["ip_address"].as<String>();
    if (d.containsKey("gateway"))    out.gateway    = d["gateway"].as<String>();
    if (d.containsKey("subnet"))     out.subnet     = d["subnet"].as<String>();

    if (d.containsKey("ota_manifest_url")) out.ota_manifest_url = d["ota_manifest_url"].as<String>();
    if (d.containsKey("update_server"))    out.update_server    = d["update_server"].as<String>();
    if (d.containsKey("config_version"))   out.config_version   = d["config_version"].as<String>();

    return true;
}

// ---------------------------------------------------------------------------
// Persistenza
// ---------------------------------------------------------------------------

bool saveConfigStruct(const Config& c)
{
    return writeFileAtomic(CONFIG_PATH, configToJson(c));
}

bool loadConfig(Config& out)
{
    if (!mountFS(true)) {
        Serial.println("[FS] Mount fallito anche dopo format");
        return false;
    }

    String json;
    if (!readFileToString(CONFIG_PATH, json)) {
        Serial.println("[CONFIG] Nessun config.json, uso default");
        return false;
    }

    return jsonToConfig(json, out);
}

// ---------------------------------------------------------------------------
// Confronto parametri critici MQTT
// ---------------------------------------------------------------------------

bool mqttCriticalChanged(const Config& a, const Config& b)
{
    return a.mqtt_broker   != b.mqtt_broker ||
           a.mqtt_port     != b.mqtt_port   ||
           a.mqtt_username != b.mqtt_username ||
           a.mqtt_password != b.mqtt_password;
}

// ---------------------------------------------------------------------------
// APPLY + SAVE
// ---------------------------------------------------------------------------

bool applyAndPersistConfigJson(const String& json, bool rebootAfter)
{
    Config newCfg = config;

    if (!jsonToConfig(json, newCfg)) {
        mqttClient.publish("bonsai/ack/config", "{\"status\":\"failed\",\"reason\":\"parse\"}");
        return false;
    }

    bool crit = mqttCriticalChanged(config, newCfg);

    if (!saveConfigStruct(newCfg)) {
        mqttClient.publish("bonsai/ack/config", "{\"status\":\"failed\",\"reason\":\"fs_write\"}");
        return false;
    }

    config = newCfg;

    mqttClient.publish("bonsai/ack/config", "{\"status\":\"applied\"}");

    if (rebootAfter) {
        delay(500);
        ESP.restart();
    }

    if (crit) {
        Serial.println("[CONFIG] Cambiati parametri MQTT: riavvio consigliato");
    }

    return true;
}

// ---------------------------------------------------------------------------
// API HTTP
// ---------------------------------------------------------------------------

void setupConfigApi()
{
    if (!mountFS(false)) {
        mountFS(true);
    }

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req){
        String json;
        if (readFileToString(CONFIG_PATH, json))
            req->send(200, "application/json", json);
        else
            req->send(200, "application/json", configToJson(config));
    });

    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* req){}, nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t){

            String body; body.reserve(len);
            for (size_t i=0; i<len; i++) body += (char)data[i];

            bool reboot = true;
            if (req->hasParam("reboot", true)) {
                reboot = req->getParam("reboot", true)->value() != "0";
            }

            bool ok = applyAndPersistConfigJson(body, reboot);
            if (ok) req->send(200, "application/json", "{\"status\":\"applied\"}");
            else     req->send(400, "application/json", "{\"error\":\"invalid_or_write_failed\"}");
        }
    );

    Serial.println("[🌐] API config pronta");
}

// ---------------------------------------------------------------------------
// MQTT HANDLER
// ---------------------------------------------------------------------------

void handleMqttConfigCommands(char* topic, byte* payload, unsigned int length)
{
    String msg; msg.reserve(length);
    for (unsigned int i=0; i<length; i++) msg += (char)payload[i];

    String t(topic);

    if (t == "bonsai/config" || t == ("bonsai/config/" + deviceId)) {
        applyAndPersistConfigJson(msg, true);
        return;
    }

    if (t == "bonsai/command/config/update") {
        applyAndPersistConfigJson(msg, true);
    }
    else if (t == "bonsai/command/restart") {
        ESP.restart();
    }
}
