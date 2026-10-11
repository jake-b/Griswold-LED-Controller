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
#include "configuration.h"
#include "colormodes.h"
#include "palettes.h"

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
CRGBPalette16 targetPalette = RainbowStripeColors_p;

// beatsin16(bpm, 0, numLeds) returns numLeds at the peak (SCALE8_FIXED).
static uint16_t lastPixelIndex() {
  return settings.numLeds > 0 ? settings.numLeds - 1 : 0;
}

void addGlitter(fract8 chanceOfGlitter) {
  for (int x=0; x<10; x++) {
    if (random8() < chanceOfGlitter) {
      leds[random16(settings.numLeds)] +=
          CRGB(settings.glitter_color.red, settings.glitter_color.green,
               settings.glitter_color.blue);
      //leds[random16(settings.numLeds)] = ColorFromPalette(targetPalette, random8(), settings.effect_brightness);
    }
  }
}

void rainbow() {
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, settings.numLeds, gHue, 7);
}

void confetti() {
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, settings.numLeds, settings.ftb_speed);
  for (int x=0; x<settings.confetti_dens; x++) {
    int pos = random16(settings.numLeds);
    leds[pos] += CHSV(gHue + random8(64), 200, settings.effect_brightness);
  }
}

void sparkle() {
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, settings.numLeds, settings.ftb_speed);
  for (int x=0; x<settings.confetti_dens; x++) {
    int pos = random16(settings.numLeds);
    CHSV color = rgb2hsv_approximate( CRGB(settings.main_color.red, settings.main_color.green,
               settings.main_color.blue) );
    leds[pos] += color;
  }
}

void sinelon() {
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, settings.numLeds, settings.ftb_speed);
  int pos = beatsin16(13, 0, lastPixelIndex());
  leds[pos] += CHSV(gHue, 255, settings.effect_brightness);
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, settings.numLeds, settings.ftb_speed);
  int dothue = 0;
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, lastPixelIndex())] |=
        CHSV(dothue, 200, settings.effect_brightness);
    dothue += 32;
  }
}

void bpm() {
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, settings.effect_brightness);
  for (int i = 0; i < settings.numLeds; i++) {  // 9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

// Ripple and related functions
void one_color_allHSV(int ahue,
                      int abright) {  // SET ALL LEDS TO ONE COLOR (HSV)
  for (int i = 0; i < settings.numLeds; i++) {
    leds[i] = CHSV(ahue, 255, abright);
  }
}

int wrap(int step) {
  if (step < 0) return settings.numLeds + step;
  if (step > settings.numLeds - 1) return step - settings.numLeds;
  return step;
}


// Ripple and related variables
int color;
int center = 0;
int step = -1;
int maxSteps = 32;
float fadeRate = 0.8;
int diff;

//background color
uint32_t currentBg = random(256);
uint32_t nextBg = currentBg;

void ripple() {
  if (currentBg == nextBg) {
    nextBg = random(256);
  } else if (nextBg > currentBg) {
    currentBg++;
  } else {
    currentBg--;
  }
  for (uint16_t l = 0; l < settings.numLeds; l++) {
    leds[l] = CHSV(currentBg, 255,
                   settings.effect_brightness);  // strip.setPixelColor(l,
                                                 // Wheel(currentBg, 0.1));
  }

  if (step == -1) {
    center = random(settings.numLeds);
    color = random(256);
    step = 0;
  }

  if (step == 0) {
    leds[center] = CHSV(
        color, 255, settings.effect_brightness);  // strip.setPixelColor(center,
                                                  // Wheel(color, 1));
    step++;
  } else {
    if (step < maxSteps) {
      //Console.println(pow(fadeRate, step));

      leds[wrap(center + step)] =
          CHSV(color, 255,
               pow(fadeRate, step) * 255);  //   strip.setPixelColor(wrap(center
                                            //   + step), Wheel(color,
                                            //   pow(fadeRate, step)));
      leds[wrap(center - step)] =
          CHSV(color, 255,
               pow(fadeRate, step) * 255);  //   strip.setPixelColor(wrap(center
                                            //   - step), Wheel(color,
                                            //   pow(fadeRate, step)));
      if (step > 3) {
        leds[wrap(center + step - 3)] =
            CHSV(color, 255, pow(fadeRate, step - 2) *
                                 255);  //   strip.setPixelColor(wrap(center +
                                        //   step - 3), Wheel(color,
                                        //   pow(fadeRate, step - 2)));
        leds[wrap(center - step + 3)] =
            CHSV(color, 255, pow(fadeRate, step - 2) *
                                 255);  //   strip.setPixelColor(wrap(center -
                                        //   step + 3), Wheel(color,
                                        //   pow(fadeRate, step - 2)));
      }
      step++;
    } else {
      step = -1;
    }
  }
}

// Comet and related variables
byte dothue = 0;
int lead_dot = 0;

void comet() {
  fadeToBlackBy(leds, settings.numLeds, settings.ftb_speed);
  lead_dot = beatsin16(int(float(settings.fps / 3)), 0, lastPixelIndex());
  leds[lead_dot] = CHSV(dothue, 200, 255);
  dothue += 8;
}

// Theatre-style crawling lights.
void theaterChase() {
  static int8_t frame = 0;

  // turn off the previous frame's led
  for (int i = 0; i < settings.numLeds; i = i + 3) {
    if (i + frame < settings.numLeds) {
      leds[i + frame] = CRGB(0, 0, 0);  // turn every third pixel off
    }
  }

  // advance the frame
  frame++;
  if (frame > 2) frame = 0;

  // turn on the current frame's leds
  for (int i = 0; i < settings.numLeds; i = i + 3) {
    if (i + frame < settings.numLeds) {
      leds[i + frame] =
          CRGB(settings.main_color.red, settings.main_color.green,
               settings.main_color.blue);  // turn every third pixel on
    }
  }
}

// Wipe and related variables
int16_t wipePos = 0;
void colorWipe() {
  static CRGB prevColor = CHSV(gHue, 255, settings.effect_brightness);
  static CRGB currentColor = CHSV(gHue+60, 255, settings.effect_brightness);

  // Wrap around if necessary
  if (wipePos >= settings.numLeds) {
    wipePos = 0;
    prevColor = currentColor;
    gHue += 60;
    currentColor = CHSV(gHue, 255, settings.effect_brightness);
  }
  
  // Render the first half of the wipe
  for (int x=0; x<wipePos; x++) {
    leds[x] = currentColor;
  }
  // Render the second half
  for (int x=wipePos; x<settings.numLeds; x++) {
    leds[x] = prevColor;
  }

  //Render the glitter at the intersection
  if (settings.glitter_wipe_on) {
    for (int x=0; x < 3; x++) {
      int speckle = wipePos + random(-SPARKLE_SPREAD,SPARKLE_SPREAD);
      if (speckle >= 0 && speckle < settings.numLeds) {
          leds[speckle] +=  CRGB(settings.glitter_color.red, settings.glitter_color.green,
               settings.glitter_color.blue);
      }    
    }
  }
  
  // Advance for next frame
  wipePos+=WIPE_SPEED;
}

// TV and related variables, functions
//***********TV
int dipInterval = 10;
int darkTime = 250;
unsigned long currentDipTime;
unsigned long dipStartTime;
unsigned long currentMillis;
int ledState = LOW;
long previousMillis = 0; 
int* ledBrightness = NULL;
uint16_t* ledHue = NULL;
int led = 5;
int interval = 2000;
int twitch = 50;
int dipCount = 0;
int analogLevel = 100;
boolean timeToDip = false;

CRGB hsb2rgbAN1(uint16_t index, uint8_t sat, uint8_t bright) {
    // Source: https://blog.adafruit.com/2012/03/14/constant-brightness-hsb-to-rgb-algorithm/
    uint8_t temp[5], n = (index >> 8) % 3;
    temp[0] = temp[3] = (uint8_t)((                                        (sat ^ 255)  * bright) / 255);
    temp[1] = temp[4] = (uint8_t)((((( (index & 255)        * sat) / 255) + (sat ^ 255)) * bright) / 255);
    temp[2] =          (uint8_t)(((((((index & 255) ^ 255) * sat) / 255) + (sat ^ 255)) * bright) / 255);

    return CRGB(temp[n + 2], temp[n + 1], temp[n]);
}

void _tvUpdateLed (int led, int brightness) {
  if (ledHue == NULL) {
    int sizeOfArray = settings.numLeds * sizeof(uint16_t);
    ledHue = (uint16_t*)malloc(sizeOfArray);
    memset((uint8_t*)ledHue, 0, sizeOfArray);
  }
  ledBrightness[led] = brightness;
  for (int i=0; i<settings.numLeds; i++) {
    uint16_t index = (i%3 == 0) ? 400 : random(0,767);
    ledHue[led] = index;
  }
}

// See: http://forum.mysensors.org/topic/85/phoneytv-for-vera-is-here/13
void tv() {
  if (ledBrightness == NULL) {
    int sizeOfArray = settings.numLeds * sizeof(int);
    ledBrightness = (int*)malloc(sizeOfArray);
    memset((uint8_t*)ledBrightness, 0, sizeOfArray);
  }
  if (timeToDip == false) {
    currentMillis = millis();
    if (currentMillis-previousMillis > interval)  {
      previousMillis = currentMillis;
      interval = random(750,4001);//Adjusts the interval for more/less frequent random light changes
      twitch = random(40,100);// Twitch provides motion effect but can be a bit much if too high
      dipCount++;      
    }
    if (currentMillis-previousMillis<twitch) {
      led=random(0, settings.numLeds-1);
      analogLevel=random(50,255);// set the range of the 3 pwm leds
      ledState = ledState == LOW ? HIGH: LOW; // if the LED is off turn it on and vice-versa:
      
      
      _tvUpdateLed(led, (ledState) ? 255 : 0);
      
      if (dipCount > dipInterval) { 
        //Console.println("dip");
        timeToDip = true;
        dipCount = 0;
        dipStartTime = millis();
        darkTime = random(50,150);
        dipInterval = random(5,250);// cycles of flicker
      }
    } 
  } else {
    //Console.println("Dip Time");
    currentDipTime = millis();
    if (currentDipTime - dipStartTime < darkTime) {
      for (int i=3; i<settings.numLeds; i++) {
        _tvUpdateLed(i, 0);
      }
    } else {
      timeToDip = false;
    }
  }

  // Render the thing, with a little flicker  
  uint8_t flicker = 255;
  int sat = 200;

  EVERY_N_MILLISECONDS(150) {
    flicker = random(220,255);
    sat = random(180, 220);
  }
  
  for (int i=0; i<settings.numLeds; i++) {
    uint16_t index = (i%3 == 0) ? 400 : random(0,767);
    //leds[i] = ((index >> 8) % 3, 200, ledBrightness[i]);
    
    leds[i] = hsb2rgbAN1(ledHue[i], sat, ledBrightness[i]).nscale8_video(flicker);
  }
}

// EYES and related classes and variables.
// From https://learn.adafruit.com/random-spooky-led-eyes/the-code
// dead-time between lighting of a range of pixels
const int deadTimeMin = 50;
const int deadTimeMax = 500;

// interval between blink starts - independent of position
const int intervalMin = 10;
const int intervalMax = 300;

const int stepInterval = 10;
long lastStep = 0;

class blinker
{
  public:
  
  boolean m_active;  // blinker is in use.
  int m_deadTime;  // don't re-use this pair immediately
  
  int m_pos;  // position of the 'left' eye.  the 'right' eye is m_pos + 1
  
  int m_red;  // RGB components of the color
  int m_green;
  int m_blue;
  
  int m_increment;  // ramp increment - determines blink speed
  int m_repeats;  // not used
  int m_intensity;  // current ramp intensity
  
  public:
  // Constructor - start as inactive
  blinker()
  {
    m_active = false;
  }
  
  // Initiate a blink at the specified pixel position
  // All other blink parameters are randomly generated
  void StartBlink(int pos)
  {
    m_pos = pos;
    
    // Pick a random color - skew toward red/orange/yellow part of the spectrum for extra creepyness
    m_red = random(150, 255);
    m_blue = 0;
    m_green = random(100);
    
    m_repeats += random(1, 3);
    
    // set blink speed and deadtime between blinks
    m_increment = random(1, 6);
    m_deadTime = random(deadTimeMin, deadTimeMax);

    // Mark as active and start at intensity zero
    m_active = true;
    m_intensity = 0;
  }
  
  // Step the state machine:
  void step()
  {
    if (!m_active)
    { 
      // count down the dead-time when the blink is done
      if (m_deadTime > 0)
      {
        m_deadTime--;
      }
      return;
    }
    
    // Increment the intensity
    m_intensity += m_increment;
    if (m_intensity >= 75)  // max out at 75 - then start counting down
    {
      m_increment = -m_increment;
      m_intensity += m_increment;
    }
    if (m_intensity <= 0)
    {
        // make sure pixels all are off
      leds[m_pos] = CRGB(0,0,0);
      leds[m_pos+1] = CRGB(0,0,0);
      
      if (--m_repeats <= 0)      // Are we done?
      {
         m_active = false;
      }
      else // no - start to ramp up again
      {
          m_increment = random(1, 5);
      }
      return;
    }
    
    // Generate the color at the current intensity level
    int r =  map(m_red, 0, 255, 0, m_intensity);
    int g =  map(m_green, 0, 255, 0, m_intensity);
    int b =  map(m_blue, 0, 255, 0, m_intensity);
   
    leds[m_pos] = CRGB(r,g,b);
    leds[m_pos+1] = CRGB(r,g,b);
  }
};

// An array of blinkers - this is the maximum number of concurrently active blinks
blinker blinkers[MAX_EYES];
int countdown = 0;
void eyes() {
  fadeToBlackBy(leds, settings.numLeds, settings.ftb_speed);
  --countdown;
  for(int i = 0; i < MAX_EYES; i++) {
    // Only start a blink if the countdown is expired and there is an available blinker
    if ((countdown <= 0) && (blinkers[i].m_active == false)) {
      int newPos = random(0, settings.numLeds/2) * 2;
          
      for(int j = 0; j < MAX_EYES; j++) {
        // avoid active or recently active pixels
        if ((blinkers[j].m_deadTime > 0) && (abs(newPos - blinkers[j].m_pos) < 4)) {
          newPos = -1;  // collision - do not start
          break;
        }
      }

      if (newPos >= 0)  { // if we have a valid pixel to start with...
       blinkers[i].StartBlink(newPos);  
       countdown = random(intervalMin, intervalMax);  // random delay to next start
      }
    }
    // step all the state machines
     blinkers[i].step();
  }
}

//************Palette Animations*********
long paletteMillis = 0; // Global variable for timechecking color palette shifts
uint8_t targetPaletteIndex;
uint8_t currentPaletteIndex;
CRGBPalette16 currentPalette;
int wipeInProgress = 0;
DIRECTION anim_direction = FORWARD;

void ChangePalettePeriodically(bool forceNow) {
  if (forceNow || millis() - paletteMillis > (settings.show_length * 1000)) {
    paletteMillis = millis();

    int paletteCount = getPaletteCount();
    // random(0, 0) and random(0, 1) only ever return 0, and
    // currentPaletteIndex starts at 0, so this loop never exits.
    if (paletteCount < 2) {
      return;
    }

    do {
      targetPaletteIndex = random(0, paletteCount);
    } while (targetPaletteIndex == currentPaletteIndex);
    Console.printf("New random target palette index: %d\n", targetPaletteIndex);
    
    currentPalette = targetPalette;

    anim_direction = (DIRECTION)!anim_direction; // DIRECTION enum allows flipping by boolean not.

    loadPaletteFromFile(targetPaletteIndex, &targetPalette);

    if (settings.glitter_wipe_on) {
       Console.println("Begin glitter wipe");
       wipeInProgress = true;
    }
  }
}

void setPaletteIndex(uint index) {
  // Bounds check
  int lastPalette = getPaletteCount()-1;
  if (index > lastPalette) index = lastPalette;

  if (index )

  // Set the palette.
  currentPaletteIndex = index;
  settings.palette_ndx = index;
  loadPaletteFromFile(settings.palette_ndx, &targetPalette);

  // Do a glitter wipe if needed
  if (settings.glitter_wipe_on) {
    wipeInProgress = true;
  }
}

void enableRandomPaletteMode() {
  settings.palette_ndx = -1;
  ChangePalettePeriodically(true);
}

void nextPalette() {
  Console.printf("Palette is currently %d", settings.palette_ndx);
  int nextPalette = settings.palette_ndx + 1;
  if (nextPalette >= getPaletteCount()) {
    nextPalette = 0;
  }
  Console.printf("Palette is now  %d", nextPalette);
  setPaletteIndex(nextPalette);
}

void prevPalette() {
  int prevPalette = settings.palette_ndx - 1;
  if (nextPalette < 0) {
    prevPalette = getPaletteCount() - 1;
  }
  setPaletteIndex(prevPalette);
}

String getCurrentPaletteName() {
  return getPaletteNameWithIndex(currentPaletteIndex);
}

DIRECTION togglePaletteAnimationDirection() {
  anim_direction = (DIRECTION)!anim_direction;
  return anim_direction;
}

void FillLEDsFromPaletteColors(CRGBPalette16 palette, uint8_t paletteStartIndex, uint16_t endingLEDIndex=0xFFFF) {
  uint8_t colorIndex = paletteStartIndex;

  for (int i = 0; i < settings.numLeds; i++) {
    if (i > endingLEDIndex) return;  //stop condition
    
    // leds[i] = ColorFromPalette( currentPalette, colorIndex + sin8(i*16),
    // brightness);
    leds[i] = ColorFromPalette(palette, colorIndex,
                               settings.effect_brightness);
    if (anim_direction == FORWARD) {
      colorIndex += 3;
    }
    if (anim_direction == BACK) {
      colorIndex -= 3;
    }
  }
}

uint8_t* random_offsets = NULL;
void init_random_offsets() {
  if (random_offsets == NULL) {
    random_offsets = (uint8_t*)malloc(settings.numLeds);
    if (random_offsets == NULL) {
       Console.println("ERROR*** UNABLE TO INITIALIZE MEMROY FOR RANDOM OFFSETS");
    } else {
      for (int x=0; x<settings.numLeds; x++) {
        random_offsets[x] = random(0,255);
      }
      Console.println("Initialized random offsets");
    }
  }
}

void sparkle2() {
  static uint8_t colorIndex = 0;
  init_random_offsets();
  CHSV color = rgb2hsv_approximate( CRGB(settings.main_color.red, settings.main_color.green,
               settings.main_color.blue) );

  for (int x=0; x<settings.numLeds; x++) {
    uint8_t random_offset = 0;
    if (random_offsets == NULL) {
      random_offset = random_offsets[x];
    } else {
      random_offset = random(0,255);
    }

    color.val = 255-(random_offset + colorIndex);    
    leds[x] = color;
  }  
  colorIndex += 1;
}


void speckle_palette() {
  init_random_offsets();
  static int8_t colorIndex = 0;
  for (int x=0; x<settings.numLeds; x++) {
    int offset = random_offsets[x] + colorIndex;
    leds[x] = ColorFromPalette(targetPalette, offset, settings.effect_brightness);    
  }
  colorIndex += 1;
}

void palette_anims() {
  TBlendType currentBlending = LINEARBLEND; // This used to be a global but never changes

  if (settings.palette_ndx == -1) ChangePalettePeriodically(false);

  if (!settings.glitter_wipe_on) {
    uint8_t maxChanges = int(float(settings.fps / 2));
    nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);

    // Update the current palette if necessary-- and send to any connected clients.
    if (currentPaletteIndex != targetPaletteIndex) {
      currentPaletteIndex = targetPaletteIndex;
    
      // Send current palette name to the UI.
      String name = getPaletteNameWithIndex(currentPaletteIndex);
      //TODO: webSocket.broadcastTXT("p"+name);
    }
  }
  
  static uint8_t startIndex = 0;

  /* motion speed */
  startIndex = startIndex + 3;

  FillLEDsFromPaletteColors(currentPalette,startIndex);

  if (settings.glitter_wipe_on && wipeInProgress) {
    if (wipePos >= settings.numLeds) {
      Console.println("End glitter wipe");
      wipeInProgress = false;
      wipePos = 0;
      currentPalette = targetPalette;
      currentPaletteIndex = targetPaletteIndex;

      // Send current palette name to the UI.
      String name = getPaletteNameWithIndex(currentPaletteIndex);
      //TODO: webSocket.broadcastTXT("p"+name);      
      FillLEDsFromPaletteColors(targetPalette,startIndex);
    } else {
      FillLEDsFromPaletteColors(targetPalette,startIndex, wipePos);
      for (int x=0; x < 3; x++) {
        int speckle = wipePos + random(-SPARKLE_SPREAD,SPARKLE_SPREAD);
        if (speckle >= 0 && speckle < settings.numLeds) {
            leds[speckle] +=  CRGB(settings.glitter_color.red, settings.glitter_color.green,
                 settings.glitter_color.blue);
        }  
      }
      wipePos+=WIPE_SPEED;
    }
  }
}


//****************ARRAY OF PARITICPATING  SHOW ANIMATIONS FOR MIXED SHOW MODE***********************
SimplePatternList participatingPatterns = {rainbow, confetti,      sinelon, juggle,
                                           bpm,     palette_anims, ripple,  comet};
//**************************************************************************************************
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
uint8_t currentPatternNumber = 0; // Index number of which pattern is current
void renderCurrentPattern() {
  participatingPatterns[currentPatternNumber]();
}

void nextPattern() {
  // add one to the current pattern number, and wrap around at the end
  //  gCurrentPatternNumber = (gCurrentPatternNumber + random(0,
  //  ARRAY_SIZE(gPatterns))) % ARRAY_SIZE( gPatterns);
  currentPatternNumber = random(0, ARRAY_SIZE(participatingPatterns));
}
