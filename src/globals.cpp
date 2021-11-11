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

size_t ConsoleImpl::write(uint8_t character) {
  Serial.println("Character");
  char buf[2] = {0,0};
  buf[0]=character;
  events.send(buf);
  return Serial.write(character);

}

size_t ConsoleImpl::write(const char *str) {
  Serial.println("String");
  events.send(str);
  return Serial.write(str);
}

size_t ConsoleImpl::write(const uint8_t *buffer, size_t size) {
  //Serial.print("Buffer: '");
  //Serial.write(buffer,size);
  //Serial.print("'");
  //Serial.println(size);
  char* buf = (char*)malloc(size+1);
  memcpy(buf, buffer, size);
  buf[size] = 0;
  events.send(buf, NULL, millis(), 1000);
  free(buf);
  return Serial.write(buffer, size);
}
