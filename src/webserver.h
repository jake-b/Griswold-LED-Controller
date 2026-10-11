#include <globals.h>
#include "configuration.h"
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern AsyncEventSource events;
extern DNSServer dns;

void initWifiManager();
void init_httpd();

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
void handleWebSocketCommand(char* payload, size_t length, AsyncWebSocketClient* client);
void printStatusJsonToSocketClient(AsyncWebSocketClient* client);
