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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdint.h>
#include <FastLED.h>

#define CONFIG_FILE_PATH "/config.json"

#define MAX_SERVER_LENGTH   40
#define MAX_USERNAME_LENGTH 32
#define MAX_PASSWORD_LENGTH 32
#define MAX_TOPIC_LENGTH 12
#define MAX_HOSTNAME_LENGTH 16

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
    
//#pragma pack(push)  // push current alignment to stack
//#pragma pack(1)     // set alignment to 1 byte boundary
typedef struct {
  uint16_t numLeds = 50;
  char hostname[MAX_HOSTNAME_LENGTH] = "";
  char http_username[MAX_USERNAME_LENGTH] = "admin";
  char http_password[MAX_PASSWORD_LENGTH] = "admin";
  char mqtt_server[MAX_SERVER_LENGTH]     = "";
  char mqtt_username[MAX_USERNAME_LENGTH] = "";
  char mqtt_password[MAX_PASSWORD_LENGTH] = "";
  char mqtt_topic[MAX_TOPIC_LENGTH] = "";
  MODE mode = OFF;
  MODE last_mode = OFF;
  uint8_t fps = 50;  // Global variable for storing the frames per second
  uint8_t overall_brightness = 255;  // Global variable for storing the brightness (255 == 100%)
  uint8_t show_length = 15;  // Global variable for storing the show_time (in seconds)
  uint8_t ftb_speed = 50;        // Global variable for fade to black speed
  uint8_t glitter_density = 50;  // Global variable for glitter density
  bool glitter_on = false;  // Global to add / remove glitter to any animation
  CRGB main_color = CRGB(127,127,127);  // Store the "main color" of the strip used in single
  // color modes
  CRGB glitter_color = CRGB(127,127,127);  // Store the "glitter color" of the strip for glitter mode
  uint8_t effect_brightness = 255;  // Brightness used for effect animations
  int8_t palette_ndx = -1; // Palette to use for PALETTE_ANIMS.  -1 is change periodically
  uint8_t confetti_dens = 1;  // Density for the confetti effect.  More confetti needed for longer strings.
  bool glitter_wipe_on = false;
  
  bool fixed_region_enabled = false;
  uint16_t fixed_region_start = 0;
  uint16_t fixed_region_length = 0;
  // Formerly, in Griswold-v1 there was some padding and a CRC here.  Not doing that anymore, 
  // because we're using A JSON file stored in SPIFFS instead of a raw struct in the eeprom.
} SettingsStruct;
//#pragma pack(pop)

extern SettingsStruct settings;

void loadConfigurationDefaults();
void loadConfigurationFile(SettingsStruct& destination);
void saveConfigurationFile(SettingsStruct& settingsToSave);
void printConfiguration(SettingsStruct& settingsToPrint);
#endif

