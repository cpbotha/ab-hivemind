// animated led going to the end of the strip, simply wrapping back, changing hue the whole time

#include <Arduino.h>

#include <FastLED.h>
#define NUM_LEDS 40
#define DATA_PIN 6

CRGBArray<NUM_LEDS> leds;
//CRGB leds[NUM_LEDS];
#define FRAMES_PER_SECOND 120

void setup() { 
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
}

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void loop() {
  static int wait = 5;
  static int idx = 0;
  static int dir = 1;
  static int h = 0;

  //sinelon();
  leds[0] = CRGB::Black;

  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow  
}

// from https://github.com/FastLED/FastLED/blob/master/examples/DemoReel100/DemoReel100.ino
// added docs for moi
void sinelon() {
  // great technique: darken all leds by a little bit, then lightend the moving one
  // will generate brilliant trails.
  //leds.fadeToBlackBy(20);
  fadeToBlackBy(leds, NUM_LEDS, 20);
  // http://fastled.io/docs/3.1/group__lib8tion.html#gaa46e5de1c4c27833359e7a97a18c839b
  // bpm, min, max
  int pos = beatsin16( 26, 0, NUM_LEDS-1 );
  leds[pos] += CHSV(gHue, 255, 192);  
}

