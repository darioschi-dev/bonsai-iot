#include "webserver.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

AsyncWebServer server(80);

int globalSoil = 0;
int globalPerc = 0;

void setup_webserver(int pumpPin) {

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
    req->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/api/soil", HTTP_GET, [pumpPin](AsyncWebServerRequest* req){
    StaticJsonDocument<512> doc;
    doc["soilValue"] = globalSoil;
    doc["percentage"] = globalPerc;
    doc["pumpStatus"] = digitalRead(pumpPin) == LOW ? "on" : "off";

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/pump/on", HTTP_POST, [pumpPin](AsyncWebServerRequest* req){
    digitalWrite(pumpPin, LOW);
    req->send(200, "application/json", "{\"status\":\"on\"}");
  });

  server.on("/api/pump/off", HTTP_POST, [pumpPin](AsyncWebServerRequest* req){
    digitalWrite(pumpPin, HIGH);
    req->send(200, "application/json", "{\"status\":\"off\"}");
  });

  server.begin();
}
