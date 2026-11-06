// OTA and Filesystem
#include <ArduinoOTA.h>
#include <FS.h>
#include <ping.h>
#include <user_interface.h>

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
// The SDK turns modem sleep back on when the station starts, and a dropped
// station still sends a sleep null-frame unless NULL-mode auto-sleep is off.
// Neither of those stops pm_send_nullfunc. An empty management pool makes
// that send fail. show() holds interrupts for the whole frame, which is what
// empties the pool. scripts/patch_pm_nullfunc.py and pm_nullfunc_fail.S
// call system_restart_local when that happens. system_restart only arms a
// timer and returns here, so that timer never runs. Falling back into
// pm_suspend lets the SDK wait in light sleep with the watchdog stopped:
// the last frame stays lit and Wi-Fi stays down.
static void keepRadioAwake() {
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  wifi_fpm_auto_sleep_set_in_null_mode(0);
}
#endif

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Console.println("Connected to Wi-Fi.");
  wifi_station_set_reconnect_policy(true);
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
// RTC user word 120, absolute word 184. system_rtc_mem word 0 is at
// 0x60001100, so this word is 0x600013e0. That is past the 32-word eboot
// command and still inside the retained user window, which ends at
// 0x600013ff. 0x600014e0 is outside it, so a marker there never came back.
// pm_nullfunc_fail.S stores 0x6e31 here. The stall watchdog stores 0x6e32
// or 0x6e33. Soft restart only; a power cycle must not inherit a stale marker.
static const uint32_t kNullFrameMagic = 0x6e31;
static const uint32_t kWifiLinkMagic = 0x6e32;
static const uint32_t kWifiEchoMagic = 0x6e33;
static volatile uint32_t* const kNullFrameSlot = (volatile uint32_t*)0x600013e0;

// A minute lets a roam or a DHCP retry finish. Each echo round is three
// requests, a second apart, and a round fails only when none come back.
// Three failed rounds is still about a minute, after the gateway has
// already answered once. One reply lost while show() has RX off does not
// count: the MAC is deaf for the whole WS2811 frame.
static const uint32_t kLinkDownRestartMs = 60000;
static const uint32_t kProbeEveryMs = 20000;
static const uint32_t kProbeGiveUpMs = 12000;
static const uint8_t kProbeFailLimit = 3;

static bool wifiDown = false;
static uint32_t wifiDownAt = 0;
static uint32_t lastProbeMs = 0;
static uint32_t probeStartedMs = 0;
static volatile bool probeBusy = false;
static volatile bool probeGotReply = false;
static volatile bool gatewayAnswers = false;
static volatile uint8_t probeFails = 0;
static volatile bool otaBusy = false;

static String markedResetInfo() {
  uint32_t mark = *kNullFrameSlot;
  *kNullFrameSlot = 0;
  if (system_get_rst_info()->reason == REASON_SOFT_RESTART) {
    if (mark == kNullFrameMagic) return String("Null frame restart");
    if (mark == kWifiLinkMagic) return String("WiFi link restart");
    if (mark == kWifiEchoMagic) return String("WiFi echo restart");
  }
  return ESP.getResetInfo();
}

// GOT_IP with RSSI 31 is the SDK's "no beacon" value left next to a stale
// station record. Treat that the same as having no address.
static bool stationLinked() {
  if (wifi_station_get_connect_status() != STATION_GOT_IP)
    return false;
  return wifi_station_get_rssi() != 31;
}

// system_restart() only arms a timer. That timer runs from this loop task.
// The null-frame path cannot use it, because that path is the Wi-Fi task.
// Clear RTC word 0 first so a stored exception or watchdog reason cannot
// hide this restart. The marker above is a different word.
static void restartMarked(uint32_t magic) {
  *kNullFrameSlot = magic;
  uint32_t zero = 0;
  system_rtc_mem_write(0, &zero, sizeof(zero));
  Serial.println(magic == kWifiEchoMagic ? "WiFi echo restart" : "WiFi link restart");
  ESP.restart();
  while (true)
    delay(50);
}

// The SDK ping callbacks run on the Wi-Fi task. Flags only; loop() restarts.
// These stay C++ functions. The sketch prototype generator gives them C++
// linkage, and a later extern "C" definition will not compile.
static void onProbeRecv(void* arg, void* pdata) {
  (void)arg;
  const ping_resp* resp = (const ping_resp*)pdata;
  if (resp && resp->ping_err == 0)
    probeGotReply = true;
}

static void onProbeDone(void* arg, void* pdata) {
  (void)arg;
  (void)pdata;
  // A late callback after the give-up path already counted this round.
  if (!probeBusy)
    return;
  // The SDK's done record reports ping_err 0 even when every echo timed
  // out. Only a recv callback with ping_err 0 means the gateway answered.
  bool ok = probeGotReply;
  probeGotReply = false;
  probeBusy = false;
  if (ok) {
    gatewayAnswers = true;
    probeFails = 0;
  } else if (gatewayAnswers && probeFails < 255) {
    probeFails++;
  }
}

static void startGatewayProbe(uint32_t now) {
  lastProbeMs = now;
  if (ESP.getFreeHeap() < 12000)
    return;
  IPAddress gw = WiFi.gatewayIP();
  if (gw == IPAddress(0, 0, 0, 0))
    return;

  static ping_option probe;
  memset(&probe, 0, sizeof(probe));
  probe.count = 3;
  probe.ip = (uint32_t)gw;
  probe.coarse_time = 1;
  probe.recv_function = reinterpret_cast<ping_recv_function>(onProbeRecv);
  probe.sent_function = reinterpret_cast<ping_sent_function>(onProbeDone);
  probeGotReply = false;
  probeBusy = true;
  probeStartedMs = now;
  if (!ping_start(&probe))
    probeBusy = false;
}

// The November stall kept painting frames, so the software watchdog never
// fired and the null-frame handler never ran. The station was simply gone.
// If the association drops, kick it once and restart after a minute. If the
// station record stays up but the gateway stops answering, the radio is the
// same kind of stuck: restart from here. Echo failures count only after one
// real answer, so a gateway that never replies to ICMP cannot reboot us.
static void pollWifiStall() {
  uint32_t now = millis();
  if (stationLinked()) {
    wifiDown = false;
    // ping_start's callbacks run on the Wi-Fi task. If they never arrive,
    // leave the round so the next probe can run.
    if (probeBusy && (uint32_t)(now - probeStartedMs) >= kProbeGiveUpMs) {
      probeBusy = false;
      probeGotReply = false;
      if (gatewayAnswers && probeFails < 255)
        probeFails++;
    }
    if (!otaBusy && !probeBusy && gatewayAnswers && probeFails >= kProbeFailLimit)
      restartMarked(kWifiEchoMagic);
    if (!otaBusy && !probeBusy && (uint32_t)(now - lastProbeMs) >= kProbeEveryMs)
      startGatewayProbe(now);
    return;
  }

  if (!wifiDown) {
    wifiDown = true;
    wifiDownAt = now;
    wifi_station_set_reconnect_policy(true);
    station_status_t st = wifi_station_get_connect_status();
    if (st == STATION_IDLE || st == STATION_NO_AP_FOUND ||
        st == STATION_CONNECT_FAIL || st == STATION_WRONG_PASSWORD)
      wifi_station_connect();
  }
  if ((uint32_t)(now - wifiDownAt) >= kLinkDownRestartMs)
    restartMarked(kWifiLinkMagic);
}

int wifiHealthLine(char* buf, size_t cap) {
  if (!buf || cap == 0)
    return 0;
  return snprintf(buf, cap, "link:%s rssi:%d miss:%u gateway:%s\n",
                  stationLinked() ? "up" : "down",
                  (int)wifi_station_get_rssi(),
                  (unsigned)probeFails,
                  gatewayAnswers ? "ok" : "none");
}

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
  String resetInfo = markedResetInfo();
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
  wifi_station_set_reconnect_policy(true);
  #ifndef GARAGE
  keepRadioAwake();
  #endif

  //Send OTA events to the browser
  ArduinoOTA.onStart([]() {
    otaBusy = true;
    events.send("Update Start", "ota");
  });
  ArduinoOTA.onEnd([]() {
    otaBusy = false;
    events.send("Update End", "ota");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "Progress: %u%%\n", (progress / (total / 100)));
    events.send(p, "ota");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    otaBusy = false;
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
    // A stalled client keeps every ping. Drop the oldest extra socket before
    // asking the rest to answer, and skip the ping while the heap is low.
    if (ESP.getFreeHeap() < 12000)
      ws.cleanupClients(1);
    else
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
  pollWifiStall();
}

