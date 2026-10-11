#include "globals.h"
#include "configuration.h"
#include "colormodes.h"
#include "webserver.h"
#include <Hash.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include "mqtt.h"
#include "build_version.h"
#include <ESP8266mDNS.h>
#include <SPIFFSEditor.h>
#include <ESPAsyncWiFiManager.h> //https://github.com/alanswx/ESPAsyncWiFiManager


AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");
DNSServer dns;

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  shouldSaveConfig = true;
}

void initWifiManager() {
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  AsyncWiFiManager wifiManager(&server, &dns);
  AsyncWiFiManagerParameter custom_mqtt_server("server",     "mqtt server",   settings.mqtt_server, sizeof(settings.mqtt_server));
  AsyncWiFiManagerParameter custom_mqtt_username("username", "mqtt username", settings.mqtt_username, sizeof(settings.mqtt_username));
  AsyncWiFiManagerParameter custom_mqtt_password("password", "mqtt password", settings.mqtt_password, sizeof(settings.mqtt_password));
  AsyncWiFiManagerParameter custom_mqtt_topic("base_topic", "mqtt base topic", settings.mqtt_topic, sizeof(settings.mqtt_topic));
  
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_username);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_mqtt_topic);
  
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.autoConnect(settings.hostname);

  //if you get here you have connected to the WiFi
  Console.println("connected...yeey :)");

  //read updated parameters
  strncpy(settings.mqtt_server,   custom_mqtt_server.getValue(),   sizeof(settings.mqtt_server));
  strncpy(settings.mqtt_username, custom_mqtt_username.getValue(), sizeof(settings.mqtt_username));
  strncpy(settings.mqtt_password, custom_mqtt_password.getValue(), sizeof(settings.mqtt_password));
  strncpy(settings.mqtt_topic,    custom_mqtt_topic.getValue(),    sizeof(settings.mqtt_topic));
  
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    saveConfigurationFile(settings);
  }

}

void init_httpd() {

  // Setup MDNS for the HTTP
  MDNS.addService("http", "tcp", 80);

  // Setup Websockets
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient * client) {
    //Console.println("Client to connect to events source");
  });
  server.addHandler(&events);

  server.addHandler(new SPIFFSEditor(settings.http_username, settings.http_password));

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.on("/resetSettings", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!request->authenticate(settings.http_username, settings.http_password))
      return request->requestAuthentication();
    AsyncWiFiManager wifiManager(&server, &dns);
    wifiManager.resetSettings();
    request->send(200, "text/plain", "Settings.reset");
  });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

  server.onNotFound([](AsyncWebServerRequest * request) {
    Console.printf("NOT_FOUND: ");
    if (request->method() == HTTP_GET)
      Console.printf("GET");
    else if (request->method() == HTTP_POST)
      Console.printf("POST");
    else if (request->method() == HTTP_DELETE)
      Console.printf("DELETE");
    else if (request->method() == HTTP_PUT)
      Console.printf("PUT");
    else if (request->method() == HTTP_PATCH)
      Console.printf("PATCH");
    else if (request->method() == HTTP_HEAD)
      Console.printf("HEAD");
    else if (request->method() == HTTP_OPTIONS)
      Console.printf("OPTIONS");
    else
      Console.printf("UNKNOWN");
    Console.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if (request->contentLength()) {
      Console.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Console.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for (i = 0; i < headers; i++) {
      AsyncWebHeader* h = request->getHeader(i);
      Console.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for (i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->isFile()) {
        Console.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if (p->isPost()) {
        Console.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Console.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });
  server.onFileUpload([](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index)
      Console.printf("UploadStart: %s\n", filename.c_str());
    Console.printf("%s", (const char*)data);
    if (final)
      Console.printf("UploadEnd: %s (%u)\n", filename.c_str(), index + len);
  });
  server.onRequestBody([](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!index)
      Console.printf("BodyStart: %u\n", total);
    Console.printf("%s", (const char*)data);
    if (index + len == total)
      Console.printf("BodyEnd: %u\n", total);
  });

  server.begin();
}

// Handler for Websockets
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Console.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->ping();
  } else if (type == WS_EVT_DISCONNECT) {
    Console.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if (type == WS_EVT_ERROR) {
    Console.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if (type == WS_EVT_PONG) {
    Console.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char*)data : "");
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if (info->final && info->index == 0 && info->len == len) {
      //the whole message is in a single frame and we got all of it's data
      int len = info->len;
      Console.printf("ws[%s][%u] %s-message[%u]: \n", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", len);
      handleWebSocketCommand((char*)data, info->len, client);
    } else {
      // message is comprised of multiple frames or the frame is split into multiple packets
      // this should not happen, as our application doesn't use multi-frame packets
      if (info->index == 0) {
        if (info->num == 0)
          Console.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        Console.printf("ws[%s][%u] frame[%u] start[%u]\n", server->url(), client->id(), info->num, info->len);
      }

      Console.printf("ws[%s][%u] frame[%u] %s[%u - %u]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

      if (info->opcode == WS_TEXT) {
        for (size_t i = 0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for (size_t i = 0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }

      //Console.printf("%s\n",msg.c_str());
      
      if ((info->index + len) == info->len) {
        Console.printf("ws[%s][%u] frame[%u] end[%u]\n", server->url(), client->id(), info->num, info->len);
        if (info->final) {
          Console.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT) {
            // process the fully received the text message
            //client->text("I got your text message");
          } else {
            // process the fully receive dbinary message
            //client->binary("I got your binary message");
          }
        }
      }
    }
  }
}

// Handler for 
void handleWebSocketCommand(char* payload, size_t length, AsyncWebSocketClient* client) {
  // # ==> Set main color
  if (payload[0] == '#') {
    // decode rgb data
    uint32_t rgb = (uint32_t) strtol((const char *) &payload[1], NULL, 16);
    settings.main_color.red = ((rgb >> 16) & 0xFF);
    settings.main_color.green = ((rgb >> 8) & 0xFF);
    settings.main_color.blue = ((rgb >> 0) & 0xFF);
    Console.printf("Set main color to: [%u] [%u] [%u]\n", settings.main_color.red, settings.main_color.green, settings.main_color.blue);
    client->text("OK");
  }

  // # ==> Set glitter color
  if (payload[0] == 'G') {
    // decode rgb data
    uint32_t rgb = (uint32_t) strtol((const char *) &payload[1], NULL, 16);
    settings.glitter_color.red = ((rgb >> 16) & 0xFF);
    settings.glitter_color.green = ((rgb >> 8) & 0xFF);
    settings.glitter_color.blue = ((rgb >> 0) & 0xFF);
    Console.printf("Set glitter color to: [%u] [%u] [%u]\n", settings.glitter_color.red, settings.glitter_color.green, settings.glitter_color.blue);
    client->text("OK");
  }

  // # ==> Set delay
  if (payload[0] == '?') {
    // decode delay data
    uint8_t d = (uint8_t) strtol((const char *) &payload[1], NULL, 10);
    settings.fps = ((d >> 0) & 0xFF);
    if (settings.fps == 0) settings.fps = 1;   // Prevent divide by zero.
    Console.printf("WS: Set FPS: [%u]\n", settings.fps);
    client->text("OK");
  }

  // # ==> Set brightness
  if (payload[0] == '%') {
    uint8_t b = (uint8_t) strtol((const char *) &payload[1], NULL, 10);
    settings.overall_brightness = ((b >> 0) & 0xFF);
    Console.printf("WS: Set brightness to: [%u]\n", settings.overall_brightness);
    FastLED.setBrightness(settings.overall_brightness);
    client->text("OK");
  }

  // e ==> Set effect brightness
  if (payload[0] == 'e') {
    uint8_t b = (uint8_t) strtol((const char *) &payload[1], NULL, 10);
    settings.effect_brightness = ((b >> 0) & 0xFF);
    Console.printf("WS: Set effect brightness to: [%u]\n", settings.effect_brightness);
    client->text("OK");
  }

  // # ==> Set show_length
  if (payload[0] == '^') {
    uint8_t b = (uint8_t) strtol((const char *) &payload[1], NULL, 10);
    settings.show_length = ((b >> 0) & 0xFF);
    Console.printf("WS: Set show_length to: [%u]\n", settings.show_length);
    client->text("OK");
  }

  // # ==> Set fade to black speed
  if (payload[0] == '_') {
    uint8_t b = (uint8_t) strtol((const char *) &payload[1], NULL, 10);
    settings.ftb_speed = ((b >> 0) & 0xFF);
    Console.printf("WS: Set fade to black speed to: [%u]\n", settings.ftb_speed);
    client->text("OK");
  }

  // # ==> Set fade glitter density
  if (payload[0] == '+') {
    uint8_t b = (uint8_t) strtol((const char *) &payload[1], NULL, 10);
    settings.glitter_density = ((b >> 0) & 0xFF);
    Console.printf("WS: Set fade to glitter density to: [%u]\n", settings.glitter_density);
    client->text("OK");
  }


  // * ==> Set main color and light all LEDs (Shortcut)
  if (payload[0] == '*') {
    // decode rgb data
    uint32_t rgb = (uint32_t) strtol((const char *) &payload[1], NULL, 16);

    settings.main_color.red = ((rgb >> 16) & 0xFF);
    settings.main_color.green = ((rgb >> 8) & 0xFF);
    settings.main_color.blue = ((rgb >> 0) & 0xFF);

    for (int i = 0; i < settings.numLeds; i++) {
      leds[i] = CRGB(settings.main_color.red, settings.main_color.green, settings.main_color.blue);
    }
    FastLED.show();
    Console.printf("WS: Set all leds to main color: [%u] [%u] [%u]\n", settings.main_color.red, settings.main_color.green, settings.main_color.blue);
    //exit_func = true;
    set_mode(ALL);
    client->text("OK");
  }

  // ! ==> Set single LED in given color
  if (payload[0] == '!') {
    // decode led index
    uint64_t rgb = (uint64_t) strtol((const char *) &payload[1], NULL, 16);

    uint8_t led =          ((rgb >> 24) & 0xFF);
    if (led < settings.numLeds) {
      uint8_t red =   ((rgb >> 16) & 0xFF);
      uint8_t green = ((rgb >> 8)  & 0xFF);
      uint8_t blue =  ((rgb >> 0)  & 0xFF);
      Console.printf("WS: Set single led [%u] to [%u] [%u] [%u]!\n", led, red, green, blue);

      for (uint8_t i = 0; i < settings.numLeds; i++) {
        leds[i] = CRGB(red, green, blue);

      }
      FastLED.show();
    }
    //exit_func = true;
    set_mode(ALL);
    client->text("OK");
  }

  // ! ==> Activate mode
  if (payload[0] == '=') {
    // we get mode data
    String str_mode = String((char *) &payload[0]);

    //exit_func = true;

    if (str_mode.startsWith("=off")) {
      set_mode(OFF);
    }
    if (str_mode.startsWith("=all")) {
      set_mode(ALL);
    }
    if (str_mode.startsWith("=mixedshow")) {
      set_mode(MIXEDSHOW);
    }
    if (str_mode.startsWith("=rainbow")) {
      set_mode(RAINBOW);
    }
    if (str_mode.startsWith("=confetti")) {
      set_mode(CONFETTI);
    }
    if (str_mode.startsWith("=sinelon")) {
      set_mode(SINELON);
    }
    if (str_mode.startsWith("=juggle")) {
      set_mode(JUGGLE);
    }
    if (str_mode.startsWith("=bpm")) {
      set_mode(BPM);
    }
    if (str_mode.startsWith("=sparkle")) {
      set_mode(SPARKLE);
    }
    if (str_mode.startsWith("=palette_anims")) {
      if (settings.palette_ndx != -1) {
        setPaletteIndex(settings.palette_ndx);
      }
      set_mode(PALETTE_ANIMS);
    }
    if (str_mode.startsWith("=speckle_palette")) {
      if (settings.palette_ndx != -1) {
        setPaletteIndex(settings.palette_ndx);
      }
      set_mode(SPECKLE_PALETTE);
    }
    if (str_mode.startsWith("=ripple")) {
      set_mode(RIPPLE);
    }
    if (str_mode.startsWith("=comet")) {
      set_mode(COMET);
    }
    if (str_mode.startsWith("=theaterchase")) {
      set_mode(THEATERCHASE);
    }
    if (str_mode.startsWith("=add_glitter")) {
      settings.glitter_on = true;
    }
    if (str_mode.startsWith("=stop_glitter")) {
      settings.glitter_on = false;
    }
    if (str_mode.startsWith("=start_glitter_wipe")) {
      settings.glitter_wipe_on = true;
    }
    if (str_mode.startsWith("=stop_glitter_wipe")) {
      settings.glitter_wipe_on = false;
    }
    if (str_mode.startsWith("=wipe")) {
      set_mode(WIPE);
    }
    if (str_mode.startsWith("=tv")) {
      set_mode(TV);
    }
    if (str_mode.startsWith("=eyes")) {
      set_mode(EYES);
    }

    Console.printf("Activated mode [%u]!\n", settings.mode);
    client->text("OK");
  }

  // $ ==> Get status Info.
  if (payload[0] == '$') {
    Console.printf("Get status info.\n");
    printStatusJsonToSocketClient(client);
  }

  // ` ==> Restore defaults.
  if (payload[0] == '`') {
    Console.printf("Restore defaults.\n");
    loadConfigurationDefaults();
    printStatusJsonToSocketClient(client);
  }

  // | ==> Save settings.
  if (payload[0] == '|') {
    Console.printf("Save settings.\n");
    saveConfigurationFile(settings);
    client->text("OK");
  }

  // ~ ==> Save settings.
  if (payload[0] == '~') {
    Console.printf("Reboot.\n");
    client->text("OK");
    ESP.reset();
  }

  // \ ==> Load settings.
  if (payload[0] == '\\') {
    Console.printf("Load settings.\n");
    loadConfigurationFile(settings);
    printStatusJsonToSocketClient(client);

    // After loading config file, try to connect to MQTT if not already connected
    // This is how you pick up new MQTT settings from the json file and connect.
    if (!mqttClient.connected()) {
      connectToMqtt();
    }
  }

  // { ==> Change palette
  if (payload[0] == '{') {
    if (length == 2) {
      if (payload[1] == '+') {
        Console.printf("Next palette\n");
        nextPalette();
      } else if (payload[1] == 'r') {
        Console.printf("Enable random palette mode\n");
        enableRandomPaletteMode();
      } else if (payload[1] == 'd') {
        DIRECTION dir = togglePaletteAnimationDirection();
        Console.printf("Change direction to: %d\n", dir);
      }
    }
    printStatusJsonToSocketClient(client);  
  }
 
  // " ==> Confetti Density
  if (payload[0] == '"') {
    uint8_t b = (uint8_t) strtol((const char *) &payload[1], NULL, 10);
    settings.confetti_dens = ((b >> 0) & 0xFF);
    Console.printf("WS: Set confetti density to: [%u]\n", settings.confetti_dens);
    client->text("OK");
  }
}

void printStatusJsonToSocketClient(AsyncWebSocketClient* client) {
  StaticJsonBuffer<800> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["mode"] = (int)settings.mode;
  root["version"] = GRISWOLD_VERSION;
  root["git"] = BUILD_GIT_HASH;
  root["built"] = BUILD_DATE;
  root["FPS"] = settings.fps;
  root["show_length"] = settings.show_length;
  root["ftb_speed"] = settings.ftb_speed;
  root["overall_brightness"] = settings.overall_brightness;
  root["effect_brightness"] = settings.effect_brightness;
  root.createNestedArray("color").copyFrom(settings.main_color.raw);
  root.createNestedArray("glitter_color").copyFrom(settings.glitter_color.raw);
  root["glitter_density"] = settings.glitter_density;
  root["glitter_on"] = settings.glitter_on;
  root["confetti_density"] = settings.confetti_dens;
  root["palette_name"] = getCurrentPaletteName();
  root["glitter_wipe_on"] = settings.glitter_wipe_on;
  
  // From https://github.com/me-no-dev/ESPAsyncWebServer/issues/203
  size_t len = root.measureLength();
  AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len);

  if (buffer) {
    //root.printTo((char *)buffer->get(), len + 1);
    root.printTo((char *)buffer->get(), len + 1);    

    if (client) {
      client->text(buffer);
    } else {
      ws.textAll(buffer);     
    }
  }
}

