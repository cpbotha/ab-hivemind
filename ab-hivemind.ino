// animated led going to the end of the strip, simply wrapping back, changing hue the whole time

#include <Arduino.h>

#include <FastLED.h>
#define NUM_LEDS 40
#define NUM_LEGS 4
#define NUM_PER_LEG 10
#define DATA_PIN 6

//#include <XBee.h>

//TxStatusResponse txStatus = TxStatusResponse();
//XBee xbee = XBee();

CRGBArray<NUM_LEDS> leds;
//CRGB leds[NUM_LEDS];
#define FRAMES_PER_SECOND 120

void setup() { 
  //Serial.begin(115200);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
}

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void loop() {
  static int wait = 5;
  static int idx = 0;
  static int dir = 1;
  static int h = 0;

  bidi_bounce();
  //sinelon();
  //scanWithConfetti();
  //scanLRUD();
  //scan();
  //leds[0] = CRGB::Black;
  //scanUD();
  //happyScanUD();
  //happyConfetti();

  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow  

  //EVERY_N_MILLISECONDS(500) { Serial.println(millis()); }
}

// from https://github.com/FastLED/FastLED/blob/master/examples/DemoReel100/DemoReel100.ino
// added docs for moi
// changing colours warm going to and fro between the far ends
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

/**
 * Per leg we two balls bouncing up and down in opposite directions.
 * 
 */
void bidi_bounce() {
  // fade out by a bit whatever we have
  fadeToBlackBy(leds, NUM_LEDS, 64);

  int pos0 = beatsin16(30, 0, 10);
  int pos1 = 10 - pos0;


  for (int leg = 0; leg < NUM_LEGS; leg++) {
    leds[translateLegPos(leg, pos0)] += CHSV(gHue, 255, 128);
    leds[translateLegPos(leg, pos1)] += CHSV(255 - gHue, 255, 128);
  }


}

// christmas tree everywhere
// are you very happy to see me?
void happyConfetti() {
  fadeToBlackBy(leds, NUM_LEDS, 10);
  
  int leg = beatsin8(60, 0, 3);

  int pos = random(NUM_LEDS);

  // random(256) hue is too much. gHue is much better.
  leds[pos] += CHSV(gHue, 255, 64);
}

// scan with full leg on
void scan() {
  fadeToBlackBy(leds, NUM_LEDS, 20);

  int leg = beatsin8(60, 0, 3);
  
  // rgbset and C++ give us this nice loop
  for (CRGB& pixel : leds(leg * 10, (leg+1)*10 - 1)) {
    // washed out blue; trying to look sad here.
    pixel += CHSV(192, 255, 16);
  }
}

// to and fro over legs, but each leg gets a bunch of random lights on and off
void scanWithConfetti() {
  fadeToBlackBy(leds, NUM_LEDS, 10);
  
  int leg = beatsin8(60, 0, 3);

  int pos = random(10);

  leds[leg*10 + pos] += CHSV(192, 255, 64);
}

/**
 * Calculate correct global LED index based on leg (0-3) and 0-based position from top.
 * 
 */
int translateLegPos(int leg, int pos) {
  if (leg % 2 == 0) {
    return leg*10 + pos;
  } else {
    // get the next legs start index, subtract one to get on the END of our leg, then subtract pos
    // TODO: factor out into utility function
    return (leg+1)*10 - 1 - pos;
  }
}

// to and fro over legs, but also upward / downward over leg at lower frequency
// quite sad -- for I am alone mode!
void scanLRUD() {

  fadeToBlackBy(leds, NUM_LEDS, 10);
  
  int leg = beatsin8(60, 0, 3);

  int pos = beatsin8(6, 0, 9);
  
  FastLED.setBrightness(128);
  if (leg % 2 == 0) {
    leds[leg*10 + pos] += CRGB::DarkBlue;
  } else {
    // get the next legs start index, subtract one to get on the END of our leg, then subtract pos
    // TODO: factor out into utility function
    leds[(leg+1)*10 - 1 - pos] += CRGB::DarkBlue;
  }
}

// sad blue scanning up and down the four legs
void scanUD() {
  fadeToBlackBy(leds, NUM_LEDS, 20);

  int pos = beatsin8(30, 0, 9);

  // half brightness
  FastLED.setBrightness(32);

  for (int leg = 0; leg < 4; leg++) {
    leds[translateLegPos(leg, pos)] += CRGB::DarkBlue;
  }
}

// sad blue scanning up and down the four legs
void happyScanUD() {
  fadeToBlackBy(leds, NUM_LEDS, 20);

  int pos = beatsin8(60, 0, 9);

  // half brightness
  //FastLED.setBrightness(128);

  for (int leg = 0; leg < 4; leg++) {
    leds[translateLegPos(leg, pos)] += CHSV(gHue, 255, 192);
  }
}