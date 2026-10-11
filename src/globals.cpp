#include "globals.h"
#include "configuration.h"
#include "mqtt.h"
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include "colormodes.h"
#include "webserver.h"

CRGB* leds = NULL;
int initializedLedCount = 0;
ConsoleImpl Console;

void init_leds() {
  if (settings.numLeds != initializedLedCount) {
    if (leds != NULL) {
      free(leds);
      initializedLedCount = 0;
    }

    // Initialize leds
    leds = (CRGB*)malloc(sizeof(CRGB) * settings.numLeds);
    memset((uint8_t*)leds, 0, sizeof(CRGB) * settings.numLeds);
    initializedLedCount = settings.numLeds;
  
    // Print some statistics
    Console.printf("Initialized for %d number of LEDs.\n", initializedLedCount);
  }
}

void set_mode(MODE new_mode) {
  if (new_mode == LAST) new_mode = settings.last_mode;

  if (new_mode == OFF) {
     // The off mode does a fade to black and then shuts off the power supply.
     // Hence, this logic is in rendering loop.
     //digitalWrite(RELAY_PIN, false);
     //mqttTopicPublish("status", "OFF");
  } else {
     digitalWrite(RELAY_PIN, true);
     mqttTopicPublish("power/status", "ON");
  }
  
  settings.last_mode = settings.mode;
  settings.mode = new_mode;

  /*if (new_mode == OFF && mqtt_client.connected()) {
    //mqtt_client.publish(mqtt_status_topic,"OFF");
  } else {
    //mqtt_client.publish(mqtt_status_topic,"ON");
  }*/
  printStatusJsonToSocketClient(NULL);
}

// Console is called from MQTT and websocket callbacks, which run inside
// AsyncTCP. events.send() transmits from that stack and re-enters lwIP.
// Queue the text here and flush it from loop().
static const size_t CONSOLE_EVENT_CAP = 480;
static char consoleEventBuf[CONSOLE_EVENT_CAP];
static size_t consoleEventLen = 0;

static void queueConsoleEvent(const char* data, size_t len) {
  if (data == NULL || len == 0) return;
  size_t room = (CONSOLE_EVENT_CAP - 1) - consoleEventLen;
  if (room == 0) return;
  if (len > room) len = room;
  memcpy(consoleEventBuf + consoleEventLen, data, len);
  consoleEventLen += len;
}

void flushConsoleEvents() {
  if (consoleEventLen == 0) return;
  char local[CONSOLE_EVENT_CAP];
  size_t n = consoleEventLen;
  memcpy(local, consoleEventBuf, n);
  local[n] = '\0';
  consoleEventLen = 0;
  if (events.count() == 0) return;
  events.send(local);
}

size_t ConsoleImpl::write(uint8_t character) {
  Serial.println("Character");
  queueConsoleEvent((const char*)&character, 1);
  return Serial.write(character);
}

size_t ConsoleImpl::write(const char *str) {
  Serial.println("String");
  if (str != NULL) queueConsoleEvent(str, strlen(str));
  return Serial.write(str);
}

size_t ConsoleImpl::write(const uint8_t *buffer, size_t size) {
  queueConsoleEvent((const char*)buffer, size);
  return Serial.write(buffer, size);
}
