#include "logger.h"

namespace Logger {

// ===== STATE =====
static WiFiUDP      _udp;
static Syslog*      _syslog = nullptr;
static int          _minLevel = LOG_INFO;
static bool         _mqttEnabled = false;
static int          _mqttMinLevel = LOG_WARNING;
static const char*  _mqttTopic = "bonsai/log";
static MqttPublishFn _mqttPublish = nullptr;

// ===== IMPLEMENTATION =====

void begin(const char* syslogHost,
           uint16_t syslogPort,
           const char* hostname,
           const char* appName,
           int minLevel)
{
  _minLevel = minLevel;

  _udp.beginPacket(syslogHost, syslogPort);
  _udp.endPacket();

  if (_syslog) delete _syslog;
  _syslog = new Syslog(_udp, syslogHost, syslogPort, hostname, appName, LOG_INFO);
}

void setMinLevel(int level) {
  _minLevel = level;
}

void enableMqtt(bool on) {
  _mqttEnabled = on;
}

void setMqttMinLevel(int level) {
  _mqttMinLevel = level;
}

void setMqttTopic(const char* topic) {
  _mqttTopic = topic ? topic : "bonsai/log";
}

void setMqttPublish(MqttPublishFn fn) {
  _mqttPublish = fn;
}

void logf(int level, const char* fmt, ...) {
  if (level > _minLevel) return;

  char line[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);

  if (_syslog) {
    _syslog->logf(level, "%s", line);
  }

  if (_mqttEnabled && _mqttPublish && level >= _mqttMinLevel) {
    _mqttPublish(_mqttTopic, line, false);
  }
}

const char* resetReasonStr(esp_reset_reason_t r) {
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
