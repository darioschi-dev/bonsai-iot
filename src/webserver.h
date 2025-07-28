#pragma once
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

AsyncWebServer server(80);
int globalSoil = 0;
int globalPerc = 0;

void setup_webserver(int pumpPin) {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/api/soil", HTTP_GET, [pumpPin](AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;
    doc["soilValue"] = globalSoil;
    doc["percentage"] = globalPerc;
    doc["pumpStatus"] = digitalRead(pumpPin) == LOW ? "on" : "off";
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/api/pump/on", HTTP_POST, [pumpPin](AsyncWebServerRequest *request) {
    digitalWrite(pumpPin, LOW);
    request->send(200, "application/json", "{\"status\":\"on\"}");
  });

  server.on("/api/pump/off", HTTP_POST, [pumpPin](AsyncWebServerRequest *request) {
    digitalWrite(pumpPin, HIGH);
    request->send(200, "application/json", "{\"status\":\"off\"}");
  });

  server.begin();
}
