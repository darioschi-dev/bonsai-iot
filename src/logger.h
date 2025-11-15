#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <stdarg.h>

namespace Logger {

using MqttPublishFn = void(*)(const char* topic, const char* payload, bool retain);

// API
void begin(const char* syslogHost,
           uint16_t syslogPort,
           const char* hostname,
           const char* appName,
           int minLevel = LOG_INFO);

void setMinLevel(int level);
void enableMqtt(bool on);
void setMqttMinLevel(int level);
void setMqttTopic(const char* topic);
void setMqttPublish(MqttPublishFn fn);

void logf(int level, const char* fmt, ...);

const char* resetReasonStr(esp_reset_reason_t r);

} // namespace Logger

// Macros
#define LOGI(fmt, ...) Logger::logf(LOG_INFO,    fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) Logger::logf(LOG_WARNING, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) Logger::logf(LOG_ERR,     fmt, ##__VA_ARGS__)
