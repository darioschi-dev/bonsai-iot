#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <stdarg.h>

namespace Logger {

// ===== Config & State =====
static WiFiUDP      _udp;
static Syslog*      _syslog = nullptr;
static int          _minLevel = LOG_INFO;       // LOG_ERR, LOG_WARNING, LOG_INFO
static bool         _mqttEnabled = false;
static int          _mqttMinLevel = LOG_WARNING;
static const char*  _mqttTopic = "bonsai/log";

// Callback opzionale per publish MQTT (per evitare dipendenze circolari)
using MqttPublishFn = void(*)(const char* topic, const char* payload, bool retain);
static MqttPublishFn _mqttPublish = nullptr;

// ===== API =====
static inline void begin(const char* syslogHost,
                         uint16_t syslogPort,
                         const char* hostname,
                         const char* appName,
                         int minLevel = LOG_INFO)
{
  _minLevel = minLevel;
  // warm-up per evitare ritardi ARP alla prima send
  _udp.beginPacket(syslogHost, syslogPort); _udp.endPacket();
  if (_syslog) { delete _syslog; _syslog = nullptr; }
  _syslog = new Syslog(_udp, syslogHost, syslogPort, hostname, appName, LOG_INFO);
}

static inline void setMinLevel(int level)            { _minLevel = level; }
static inline void enableMqtt(bool on)               { _mqttEnabled = on; }
static inline void setMqttMinLevel(int level)        { _mqttMinLevel = level; }
static inline void setMqttTopic(const char* topic)   { _mqttTopic = topic ? topic : "bonsai/log"; }
static inline void setMqttPublish(MqttPublishFn fn)  { _mqttPublish = fn; }

static inline void logf(int level, const char* fmt, ...) {
  if (level > _minLevel) return;
  static char line[192];
  va_list ap; va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);

  if (_syslog) _syslog->logf(level, "%s", line);

  if (_mqttEnabled && _mqttPublish && level >= _mqttMinLevel) {
    _mqttPublish(_mqttTopic, line, false);
  }
}

// Helpers “stringa” per reset reason
static inline const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "POWERON";
    case ESP_RST_EXT:      return "EXT";
    case ESP_RST_SW:       return "SW";
    case ESP_RST_PANIC:    return "PANIC";
    case ESP_RST_INT_WDT:  return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT:      return "WDT";
    case ESP_RST_DEEPSLEEP:return "DEEPSLEEP";
    default:               return "OTHER";
  }
}

} // namespace Logger

// ===== Macros comode =====
#define LOGI(fmt, ...) Logger::logf(LOG_INFO,    fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) Logger::logf(LOG_WARNING, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) Logger::logf(LOG_ERR,     fmt, ##__VA_ARGS__)
