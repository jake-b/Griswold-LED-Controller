# Griswold LED Controller

Version 3. ESP8266 controller for a string of WS2811 or WS2812B lights, with a web interface and MQTT. It is a fork of [Russell's FASTLEDs](https://github.com/russp81/LEDLAMP_FASTLEDs), which is a fork of [McLighting](https://github.com/toblum/McLighting). The name is Clark Griswold's.

Version 2 was the async rewrite of the original sketch. This line is the PlatformIO firmware in `src/`, and it targets a Wemos D1 mini.

## Hardware

- Board: Wemos D1 mini (ESP8266), 160 MHz, 4 MB flash
- Data: GPIO14
- Relay: GPIO12, the pinout of an original Sonoff
- Default build: WS2811, color order RGB
- Garage install: uncomment `#define GARAGE` in `src/globals.h` for WS2812B, color order GRB

Interrupts stay off for a whole WS2811 frame. A WiFi interrupt between pixels makes the string latch a partial frame. After each frame, `loop()` leaves an unlocked window so the radio can run.

## Build

Clone with the library submodules, then build with PlatformIO:

```
git clone --recursive https://github.com/jake-b/Griswold-LED-Controller.git
cd Griswold-LED-Controller
pio run -e d1_mini
```

If the `lib/` directories are empty, run `git submodule update --init`.

Each build writes `src/build_version.h` with the short git hash and the local time. That file is not committed. The web footer shows `3.x`, the git hash, and the build time. A `-dirty` suffix means the tree had uncommitted changes when it was compiled.

Upload firmware over Arduino OTA. This does not touch the filesystem:

```
pio run -e d1_mini -t upload --upload-port <device-ip>
```

`upload_flags` in `platformio.ini` pins the OTA callback port. Change it if that port is already in use.

## Web interface and settings

On first boot the device opens a WiFi setup portal. Settings are saved in `/config.json` on SPIFFS and loaded again on boot, including the light mode. A reboot restores that saved mode and drives the relay from it.

The factory web username and password are `admin` and `admin`. Change them in the configuration. The filesystem editor is at `http://<device>/edit` and uses HTTP digest with those credentials.

The page and the palette files live in `data/`. Put individual files on the device through `/edit`. Do not run `pio run -t uploadfs` on a device you have already configured. That command replaces the whole SPIFFS image, including `config.json` and the palettes.

`/config.json` on the device holds the HTTP and MQTT passwords, and SPIFFS files are served from the web root. Do not commit a copy of that file, and do not expose the device to the public internet.

MQTT topics follow the configured topic prefix so Home Assistant can turn the string on and off.

## Palettes

PaletteKnife sources are the `.c3g` files in `palettes/`. The files the device loads are the `.bin` files in `data/palettes/`. Convert a source with:

```
python3 palette_convert/palette_convert.py palettes/christmas-candy.c3g
```

That writes `palettes/christmas-candy.bin`. Copy the binaries you want into `data/palettes/` and upload those files.

## Libraries

`lib/` contains pinned git submodules:

- [FastLED](https://github.com/FastLED/FastLED) 3.4.0
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) 5
- [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP)
- [ESPAsyncWebServer](https://github.com/arjenhiemstra/ESPAsyncWebServer)
- [ESPAsyncWiFiManager](https://github.com/alanswx/ESPAsyncWiFiManager)
- [AsyncMqttClient](https://github.com/marvinroger/async-mqtt-client)

## License

Griswold is released under the GNU Lesser General Public License v3.0, the same license as McLighting. See [LICENSE](LICENSE).
