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

#include "globals.h"

#ifndef COLORMODES_H
#define COLORMODES_H

#define MAX_EYES 20
#define SPARKLE_SPREAD (_max(settings.numLeds/80,3))
#define WIPE_SPEED  (_max(settings.numLeds/120,1))

// enum DIRECTION {
//   BACK = 0,
//   FORWARD = 1, };
  
extern uint8_t gHue; // rotating "base color" used by many of the patterns

void addGlitter(fract8 chanceOfGlitter);
void rainbow();
void confetti();
void sinelon();
void juggle();
void bpm();
void ripple();
void comet();
void theaterChase();
void colorWipe();
void tv();
void eyes();
void speckle_palette();
void sparkle();

// Place to store an array of patterns participating in "MIXED SHOW"
// Not all patterns are in the rotation.
typedef void (*SimplePatternList[])();
extern SimplePatternList participatingPatterns;
void renderCurrentPattern();
void nextPattern();

void palette_anims();
String getCurrentPaletteName();
void setPaletteIndex(uint index);
void enableRandomPaletteMode();
void nextPalette();
void prevPalette();
DIRECTION togglePaletteAnimationDirection();
#endif
