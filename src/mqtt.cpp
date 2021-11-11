
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <AsyncMqttClient.h>
#include <Ticker.h>
#include "mqtt.h"
#include "configuration.h"
#include "globals.h"
#include "webserver.h"

// Global objects
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

// MQTT stuff
char* willTopic = NULL; // Need a static copy of LWTTopic
char* getWillTopic() {
  if (willTopic == NULL) {
    String topic = settings.mqtt_topic;
    if (!topic.endsWith("/")) topic += "/";
    String lwt_topic = topic + "lwt";
    int topic_length = lwt_topic.length();
    willTopic = (char*)malloc(topic_length+1);
    strncpy(willTopic, lwt_topic.c_str(), topic_length);
    willTopic[topic_length]= '\0';
    Console.println(willTopic);
  }
  return willTopic;
}

void connectToMqtt() {
  if (strncmp(settings.mqtt_server  , "", MAX_SERVER_LENGTH) != 0 &&
      strncmp(settings.mqtt_username, "", MAX_USERNAME_LENGTH) != 0 &&
      strncmp(settings.mqtt_password, "", MAX_PASSWORD_LENGTH) != 0) {
    //String topic = settings.mqtt_topic;
    //String lwt_topic = topic + "lwt";
   
   

    Console.println("Connecting to MQTT...");
    mqttClient.setServer(settings.mqtt_server, 1883);
    mqttClient.setCredentials(settings.mqtt_username, settings.mqtt_password);
    mqttClient.setWill(getWillTopic(), 2, true, "Offline");
    mqttClient.connect();
  } else {
    Console.println("Incomplete MQTT Settings... connect skipped.");
  }
}

void onMqttConnect(bool sessionPresent) {
  Console.println("Connected to MQTT.");
  Console.print("Session present: ");
  Console.println(sessionPresent);

  if (strncmp(settings.mqtt_topic, "", MAX_TOPIC_LENGTH) != 0) {
    String topic = settings.mqtt_topic;
    if (!topic.endsWith("/")) topic += "/";
    String set_topic = topic + "power/set";
    Console.printf("Subscribing to topic: %s\n", set_topic.c_str());
    uint16_t packetIdSub = mqttClient.subscribe(set_topic.c_str(), 2);
    Console.print("Subscribing at QoS 2, packetId: ");
    Console.println(packetIdSub);
  }

  // Publish the current state upon connect.
  if (settings.mode == OFF) {
     mqttTopicPublish("power/status", "OFF");    
  } else {
     mqttTopicPublish("power/status", "ON");
  }

  mqttClient.publish(getWillTopic(), 2, true, "Online");
//  mqttClient.publish("test/lol", 0, true, "test 1");
  //Console.println("Publishing at QoS 0");  
//  uint16_t packetIdPub1 = mqttClient.publish("test/lol", 1, true, "test 2");
//  Console.print("Publishing at QoS 1, packetId: ");
//  Console.println(packetIdPub1);
//  uint16_t packetIdPub2 = mqttClient.publish("test/lol", 2, true, "test 3");
//  Console.print("Publishing at QoS 2, packetId: ");
//  Console.println(packetIdPub2);
}

void mqttTopicPublish(const char* publishSubTopic, const char* message) {
  if (mqttClient.connected() && strncmp(settings.mqtt_topic, "", MAX_TOPIC_LENGTH) != 0) {
    String topic = settings.mqtt_topic;
    if (!topic.endsWith("/") && publishSubTopic[0] != '/') topic += "/";
    topic += publishSubTopic;
    Console.printf("Publishing at QoS 0 %s: %s\n", topic.c_str(), message);  
    uint16_t packetIdPub1 = mqttClient.publish(topic.c_str(), 0, false, message);
    Console.print("Publishing at QoS 0, packetId: ");
    Console.println(packetIdPub1);
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Console.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Console.println("Subscribe acknowledged.");
  Console.print("  packetId: ");
  Console.println(packetId);
  Console.print("  qos: ");
  Console.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Console.println("Unsubscribe acknowledged.");
  Console.print("  packetId: ");
  Console.println(packetId);
}

void onMqttMessage(char* inTopic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Console.println("Publish received.");
  Console.print("  topic: ");
  Console.println(inTopic);
  Console.print("  current_mode: " );
  Console.println(settings.mode);
  // Console.print("  qos: ");
  // Console.println(properties.qos);
  // Console.print("  dup: ");
  // Console.println(properties.dup);
  // Console.print("  retain: ");
  // Console.println(properties.retain);
  // Console.print("  len: ");
  // Console.println(len);
  // Console.print("  index: ");
  // Console.println(index);
  // Console.print("  total: ");
  // Console.println(total);
  
  if (strncmp(settings.mqtt_topic, "", MAX_TOPIC_LENGTH) != 0) { 
    String topic = settings.mqtt_topic;
    if (!topic.endsWith("/")) topic += "/";    
    topic += "power/set";
    Console.print(topic);
    if (strncmp(inTopic, topic.c_str(), topic.length()) == 0) {
      Console.println("Here1");
      if (strncasecmp(payload, "ON", 2)==0 && settings.mode == OFF) {
        Console.println("Here2");
        // If both mode and last_mode are off, then this command won't do
        // anything.  Nonetheless the user as asked for the string to turn
        // on via MQTT... so in this case, turn on to ALL.
        if (settings.last_mode != OFF) {
          set_mode(LAST);
        } else {
          set_mode(ALL);
        }
      } else if (strncasecmp(payload, "OFF", 3)==0 && settings.mode != OFF) {
        Console.println("Here3"); 
        set_mode(OFF);
      }
    }
  }

}

void onMqttPublish(uint16_t packetId) {
  Console.println("Publish acknowledged.");
  Console.print("  packetId: ");
  Console.println(packetId);
}

void mqttInit() {
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
}

void mqttCancelReconnectTimer() {
  mqttReconnectTimer.detach();
}

