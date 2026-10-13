// OTA and Filesystem
#include <ArduinoOTA.h>
#include <FS.h>

// Global stuff
#include "globals.h"
#include "colormodes.h"
#include "palettes.h"

// For MQTT
#include "mqtt.h"

// Main configuration
#include "configuration.h"

// Webserver handlers
#include "webserver.h"

// SDK internals. There is no public wrapper. wDevDisableRx clears the MAC
// receive-enable bit and wDevEnableRx sets it again.
//
// show() locks out the WiFi task for the whole string. 800 WS2811s is about
// 25ms, and the WiFi NMI still runs during that lock. If the NMI starts a
// transmit whose buffer pointer is null, it prints dev 1163 and spins at
// wDev_ProcessFiq until the hardware watchdog resets (flag 1). Gating
// receive for the bit-bang keeps the MAC from starting that work.
extern "C" {
  void wDevDisableRx(void);
  void wDevEnableRx(void);
}

// SKETCH BEGIN
// For managing WiFi disconnects
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

// Wifi Disconnection Handling
void connectToWifi() {
  Console.println("Disconnected from Wi-Fi...");
}

#ifndef GARAGE
// Modem sleep sends a null frame from pm_send_nullfunc. If the management
// buffer pool is empty, that function asserts and spins at a jump-to-self
// until the software watchdog resets. show() holds interrupts for the whole
// WS2811 frame, which is what empties the pool. The SDK restores modem sleep
// when the station starts, so the sleep type has to be set again after connect.
static void keepRadioAwake() {
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
}
#endif

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Console.println("Connected to Wi-Fi.");
  #ifndef GARAGE
  keepRadioAwake();
  #endif
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Console.println("Disconnected from Wi-Fi.");
  mqttCancelReconnectTimer(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
}

bool mount() {
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    return true;
  }
  Serial.println("failed to mount FS");
  return false;
}

// Keep the last few reset reasons. Serial output is lost when nothing is
// connected, and a growing log would crowd out config and palettes.
static void logResetReason(const String& info) {
  const char *path = "/reset.log";
  const int keepLines = 7;
  const size_t cap = 768;
  char buf[cap];
  size_t n = 0;
  bool partial = false;

  File in = SPIFFS.open(path, "r");
  if (in) {
    size_t sz = in.size();
    if (sz > cap - 1) {
      in.seek(sz - (cap - 1), SeekSet);
      partial = true;
    }
    n = in.readBytes(buf, cap - 1);
    in.close();
  }
  buf[n] = '\0';

  char *start = buf;
  if (partial) {
    char *nl = strchr(start, '\n');
    start = nl ? nl + 1 : buf + n;
  }

  int lines = 0;
  for (char *p = start; *p; p++) {
    if (*p == '\n') lines++;
  }
  size_t startLen = strlen(start);
  if (startLen > 0 && start[startLen - 1] != '\n') lines++;
  while (lines > keepLines) {
    char *nl = strchr(start, '\n');
    if (!nl) break;
    start = nl + 1;
    lines--;
  }

  char line[180];
  size_t w = 0;
  for (unsigned i = 0; i < info.length() && w + 1 < sizeof(line); i++) {
    char c = info[i];
    if (c == '\r' || c == '\n' || c == '\t') c = ' ';
    line[w++] = c;
  }
  line[w] = '\0';

  File out = SPIFFS.open(path, "w");
  if (!out) {
    Serial.println("reset log write failed");
    return;
  }
  if (*start) {
    out.print(start);
    startLen = strlen(start);
    if (start[startLen - 1] != '\n') out.print('\n');
  }
  out.println(line);
  out.close();
}

// Save parameters from the WiFiManager if needed.

void setup() {
  Serial.begin(115200);
  Serial.println();
  String resetInfo = ESP.getResetInfo();
  Serial.print("Reset: ");
  Serial.println(resetInfo);

  // Setup the RELAY output

  // setup the relay
  pinMode(RELAY_PIN, OUTPUT);

 // set builtin led pin as output
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);
  
 // set up output pin
  pinMode(DATA_PIN, OUTPUT);
  
  // Initialize the RNG
  ///*** Random Seed***
  randomSeed(analogRead(0));

  // Mount the filesystem and load the configuration (if present)
  if (mount()) {
    logResetReason(resetInfo);
  }
  loadConfigurationFile(settings); 
  Console.print("Settings initialized on boot: ");
  printConfiguration(settings);
  Console.print("Mode is");
  Console.println(settings.mode);

  // determine the hostname
  if (strncmp(settings.hostname, "", MAX_HOSTNAME_LENGTH) == 0) {
    uint16_t chipid = ESP.getChipId() & 0xFFFF;
    snprintf(settings.hostname, MAX_HOSTNAME_LENGTH, "%s-%04x", "GRISWOLD", chipid);
  }

  //initialize leds for the first time
  init_leds();

  // load the initial palette if necessary
  if (settings.palette_ndx != -1) {
    setPaletteIndex(settings.palette_ndx);
  }
  Console.printf("Number of Patterns: %d\n", getPaletteCount());

  // Setup Wifi Callbacks
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  // Setup MQTT
  mqttInit();
   
  initWifiManager();
  #ifndef GARAGE
  keepRadioAwake();
  #endif

  //Send OTA events to the browser
  ArduinoOTA.onStart([]() {
    events.send("Update Start", "ota");
  });
  ArduinoOTA.onEnd([]() {
    events.send("Update End", "ota");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "Progress: %u%%\n", (progress / (total / 100)));
    events.send(p, "ota");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR) events.send("Auth Failed", "ota");
    else if (error == OTA_BEGIN_ERROR) events.send("Begin Failed", "ota");
    else if (error == OTA_CONNECT_ERROR) events.send("Connect Failed", "ota");
    else if (error == OTA_RECEIVE_ERROR) events.send("Recieve Failed", "ota");
    else if (error == OTA_END_ERROR) events.send("End Failed", "ota");
  });
  ArduinoOTA.setHostname(settings.hostname);
  ArduinoOTA.begin();

  // Setup FastLed
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, settings.numLeds)
  .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(settings.overall_brightness);

  init_httpd();

  // Explicity set the relay state.
  if (settings.mode == OFF) {
     digitalWrite(RELAY_PIN, false);
     Console.printf("INITIAL MODE OFF: %d\n", settings.mode);
  } else {
     digitalWrite(RELAY_PIN, true);
     Console.printf("INITIAL MODE ON: %d\n", settings.mode);
  }

  // Turn off LED at boot complete
  digitalWrite(BUILTIN_LED, LOW);

}

//#ifdef GARAGE
#define NUM_GLITTER_PARTICLES 3
#define NUM_GLITTER_FRAMES 2
int garageGlitterIndex[NUM_GLITTER_PARTICLES];
int frameCount = 255;

int randomNumberNotAlreadyUsed(int base, int range) {
    int newRandom = base + random(range);
    for (int timeout = 0; timeout < 999; timeout++) {
      newRandom = base + random(range);
      for (int x = 0; x < NUM_GLITTER_PARTICLES; x++) {
        if (newRandom == garageGlitterIndex[x]) {
          goto retry;  
        }       
      }
      return newRandom;
retry:
      continue;
    }
    return newRandom;
}
//#endif

void loop() {  

  static int nextFrameTime = 0;
  static int nextWsPingTime = 0;
  if (millis() > nextWsPingTime) {
    nextWsPingTime = millis() + WEBSOCKET_PING_TIME_MS;
    ws.pingAll();
  }
  if (millis() > nextFrameTime) {
    nextFrameTime = millis() + int(float(1000.0 / settings.fps));
    gHue++;           // slowly cycle the "base color" through the rainbow
 
    if (settings.mode != OFF && settings.fixed_region_enabled) {
      for (int r = settings.fixed_region_start; r < settings.fixed_region_start + settings.fixed_region_length; r++) {
        if (r < settings.numLeds) {
          leds[r] = CRGB(settings.main_color.red, settings.main_color.green,
                                                 settings.main_color.blue);
        }
      }

      // Calculate some new glitter index
      if (frameCount > NUM_GLITTER_FRAMES) {
        for (int x=0; x<NUM_GLITTER_PARTICLES; x++) {
            if (random8() < settings.glitter_density) {
              int newGlitterIndex = randomNumberNotAlreadyUsed(settings.fixed_region_start, settings.fixed_region_length);//434+random16(50);
              garageGlitterIndex[x] = newGlitterIndex;
            }
        }
        frameCount = 0;
      }

      // Render glitter particles
      for (int x=0; x<NUM_GLITTER_PARTICLES; x++) {
          if (garageGlitterIndex[x] < settings.numLeds) {
            leds[garageGlitterIndex[x]] +=
            CRGB(settings.glitter_color.red, settings.glitter_color.green,
               settings.glitter_color.blue);
          }
      }

      frameCount++;
   }

    // Once OFF has faded and the relay is open, the pixels are unpowered.
    // Skip the bit-bang so the WiFi stack is not locked out while dark.
    uint32_t showUs = 0;
    if (!(settings.mode == OFF && digitalRead(RELAY_PIN) == LOW)) {
      yield();
      wDevDisableRx();
      uint32_t showStart = micros();
      FastLED.show();
      showUs = micros() - showStart;
      wDevEnableRx();
      // micros() reads a free-running timer, but if it ever stalls inside
      // the interrupt lock, fall back to the WS2811 bit time (~31us/LED).
      if (showUs < 50) showUs = (uint32_t)settings.numLeds * 31;
    }
    // show() holds interrupts for the whole frame. Leave a matching unlocked
    // slice afterwards, capped so a long string still moves.
    uint32_t pauseUs = showUs;
    if (pauseUs < 1000) pauseUs = 1000;
    if (pauseUs > 10000) pauseUs = 10000;
    uint32_t pauseUntil = micros() + pauseUs;
    while ((int32_t)(pauseUntil - micros()) > 0) {
      yield();
    }

    // Render the next frame
    switch (settings.mode) {
      default:
      case OFF: {
          // If the Power supply is on, check to see if all LEDs are off.         
          if (digitalRead(RELAY_PIN) == HIGH) {
            int x = settings.numLeds * 3 - 1;
            while (x > 0) {
              --x;
              if (((uint8_t*)leds)[x]) break;
            }
          
            if (x == 0) {
              Console.println("Turning off power supply");
              // If we made it to zero, then we can turn off the power
              digitalWrite(RELAY_PIN, false);
              mqttTopicPublish("power/status", "OFF");
            } else {
              Console.println("Fading LEDs");
              // If there are still some LEDs on, fade them more.
              fadeToBlackBy(leds, settings.numLeds, settings.ftb_speed);
            }
          } 
          break;
        }

      case ALL:
        fill_solid(leds, settings.numLeds,  CRGB(settings.main_color.red, settings.main_color.green,
                                                 settings.main_color.blue));
        break;

      case MIXEDSHOW: {
        renderCurrentPattern();

        // Advance the pattern every "show_length" seconds
        static int nextShowTime = 0;
        if (millis() > nextShowTime) {
          nextShowTime = millis() + settings.show_length * 1000;
          nextPattern();
          Console.println( "nextPattern was called at " + String(millis()) +
                          " and the current show length set to " + String(settings.show_length * 1000));
        }
        break; }

      case RAINBOW:
        rainbow();
        break;

      case CONFETTI:
        confetti();
        break;

      case SINELON:
        sinelon();
        break;

      case JUGGLE:
        juggle();
        break;

      case BPM:
        bpm();
        break;

      case PALETTE_ANIMS:
        palette_anims();        
        break;

      case RIPPLE:
        ripple();
        break;

      case SPARKLE:
        sparkle();
        break;

      case SPECKLE_PALETTE:
        speckle_palette();
        break;

      case COMET:
        comet();
        break;

      case THEATERCHASE:
        theaterChase();
        break;

      case WIPE:
        colorWipe();
        break;

      case TV:
        tv();
        break;

      case EYES:
        eyes();
        break;
    }

    // Add glitter if necessary
    if (settings.mode != OFF && settings.glitter_on == true) {
      addGlitter(settings.glitter_density);
    }
  } //main frame loop

  // This core's cleanupClients() closes the oldest socket once the
  // connected count passes the library max (4 on ESP8266).
  ws.cleanupClients();
  flushConsoleEvents();

  // Handle other processes
  ArduinoOTA.handle();
}

