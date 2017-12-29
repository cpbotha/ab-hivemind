// proximity ideas:

// #1
// - each leg has two bouncing leds, where each led represents proximity to another
// - the closer you get, the bigger the bounce.
// - if everyone is together, we have full bounce
// - bounce could be audio sensitive

#include <Arduino.h>

#include <FastLED.h>
#define NUM_LEDS 40
#define NUM_LEGS 4
#define NUM_PER_LEG 10
#define DATA_PIN 6

// analogue!
#define AUDIO_PIN 1

//#include <XBee.h>

//TxStatusResponse txStatus = TxStatusResponse();
//XBee xbee = XBee();

CRGBArray<NUM_LEDS> leds;
//CRGB leds[NUM_LEDS];
#define FRAMES_PER_SECOND 120

void setup() { 
  // need this for the serial console
  // speed has to match what you setup the connection for (see status line bottom right)
  Serial.begin(115200);
  // initialise FastLED
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
}

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

int min = 1024;
int max = 0;

void loop() {

  EVERY_N_MILLIS(8) {

    // each of these functions is an evented effect, i.e. it does what it
    // needs to do every N milliseconds, fully timesliced.

    bidi_bounce();
    //sinelon();
    //scanWithConfetti();
    //scanLRUD();
    //scan();
    //leds = CRGB::Black;
    //scanUD();
    //happyScanUD();
    //happyConfetti();

#ifdef MICROPHONE_TEST
    // the robotdyn audio sensor needs 3.3V
    // max int I get out of it is 433
    // 
    fadeToBlackBy(leds, NUM_LEDS, 64);
    int aval = analogRead(AUDIO_PIN);
    // value should be between 0 and 1023 inclusive
    // in one session, the max was 459, in another it was 795.
    // https://robotdyn.com/upload/PHOTO/0G-00004696==Sens-SoundDetect/DOCS/Schematic==0G-00004696==Sens-SoundDetect.pdf
    // https://bochovj.wordpress.com/2013/06/23/sound-analysis-in-arduino/
    // https://lowvoltage.wordpress.com/2011/05/21/lm358-mic-amp/
    // Vcc is 3.3V; max output voltage for op-amp is Vcc - 1.5 which is 1.8V which is 1.8/5 * 1024 = 368 OR 1.8/3.3 * 1024 = 
    int numleds = (float)aval / 500.0 * NUM_LEDS;
    if (aval > max) max = aval;
    if (aval < min) min = aval;
    Serial.print(aval);
    Serial.print("\t");
    Serial.print(min);
    Serial.print("\t");
    Serial.println(max);
    // leds(0, numleds) = CHSV(gHue, 255, 192);
    for (CRGB& pixel : leds(0, numleds)) {
      pixel += CHSV(gHue, 255, 192);
    }
#endif

    FastLED.show();

  }
  
  
  //FastLED.delay(1000 / FRAMES_PER_SECOND);
  //FastLED.delay(1000 / 240);

  // do some periodic updates
  EVERY_N_MILLIS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow  

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
 * Inner and outer legs bounce at different frequencies.
 * 
 */
void bidi_bounce() {
  // fade out by a bit whatever we have
  fadeToBlackBy(leds, NUM_LEDS, 64);

  // connect this to proximity per leg?
  int max = 9;

  int pos0 = beatsin16(40, 0, max);
  int pos1 = max - pos0;

  // half the frequency of the above
  int pos2 = beatsin16(20, 0, max);
  int pos3 = max - pos2;
  
  // middle legs
  for (int leg = 1; leg < 3; leg++) {
    leds[translateLegPos(leg, pos0)] += CHSV(gHue, 255, 128);
    leds[translateLegPos(leg, pos1)] += CHSV(255 - gHue, 255, 128);
  }

  // outside legs
  for (int leg = 0; leg < 4; leg+=3) {
    leds[translateLegPos(leg, pos2)] += CHSV(255 - gHue, 255, 128);
    leds[translateLegPos(leg, pos3)] += CHSV(gHue, 255, 128);
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