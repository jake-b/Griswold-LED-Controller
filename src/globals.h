// Copyright (c) 2017 @jake-b, @russp81, @toblum
// Griswold LED Lighting Controller

// Griswold is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as 
// published by the Free Software Foundation, either version 3 of 
// the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Griswold-v2 is a rewrite of Griswold-1.0 using Async libararies.

// Griswold-v1 is a fork of the LEDLAMP project at 
//        https://github.com/russp81/LEDLAMP_FASTLEDs

// The LEDLAMP project is a fork of the McLighting Project at
//        https://github.com/toblum/McLighting

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

//#define GARAGE

// For FASTLED
// Keep interrupts off for the whole WS2811 frame. A WiFi interrupt between
// pixels stretches the reset gap, the string latches a partial frame, and
// FastLED retries it. That is the full-string flash. loop() leaves an
// unlocked window after each show so the radio still runs.
#ifndef GARAGE
#define FASTLED_ALLOW_INTERRUPTS 0
#endif

#include <FastLED.h>

// Standard pins for an old-school original Sonoff
// NOT newer Sonoff basic, which does not break out GPIO14
#define DATA_PIN 14
#define RELAY_PIN 12

#ifdef GARAGE
#define LED_TYPE WS2812B // 2019 production value
//#define LED_TYPE WS2811 // long shot
//#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#else
#define LED_TYPE WS2811
#define COLOR_ORDER RGB
#endif

#define WEBSOCKET_PING_TIME_MS 60000
extern CRGB* leds;

enum MODE { LAST = -1,
    HOLD = 0,
    OFF = 1,
    ALL,
    MIXEDSHOW,
    RAINBOW,
    CONFETTI,
    SINELON,
    JUGGLE,
    BPM,
    SPARKLE,
    PALETTE_ANIMS,
    SPECKLE_PALETTE,
    RIPPLE,
    COMET,
    THEATERCHASE,
    WIPE,
    TV,
    EYES };

 enum DIRECTION {
  BACK = 0,
  FORWARD = 1, };   

void init_leds();
extern void set_mode(MODE new_mode);

class ConsoleImpl : public Print  {
        public:
                virtual size_t write(uint8_t);
                virtual size_t write(const char *str);
                virtual size_t write(const uint8_t *buffer, size_t size);

        private:
                // otherstuff
};

extern ConsoleImpl Console;

// Send queued console lines to /events from loop(), never from a callback.
void flushConsoleEvents();

#endif