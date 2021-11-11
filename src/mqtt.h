
#ifndef MQTT_H
#define MQTT_H

#include <AsyncMqttClient.h>
#include <Ticker.h>

extern AsyncMqttClient mqttClient;
extern Ticker mqttReconnectTimer;
void mqttInit();
void connectToMqtt();
void mqttCancelReconnectTimer();
void mqttTopicPublish(const char* publishSubTopic, const char* message);

#endif
