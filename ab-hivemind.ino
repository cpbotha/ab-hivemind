// status on 2018-03-04
// - I have sporadic communication with xbees connected via sparkfun dline (SoftSerial 2,3)
// - on the hardware UART (d0,d1) I can't even get the first AT command to respond;
//   First 4 LEDS flash to indicate that nothing came back.

// LED feedback
// - LED0 goes very bright right after initial serial init + 2s wait
// - during looping, LED0 will pulse to indicate that the board is processing
// - LED1 will flash if we are able to read MY source address from xbee
// - LED0-3 will flash if we are NOT able to read MY source address
// - LED2 will flash every time we attempt to read a packet
// - LED4 will flash if we have success
// - LED6 will flash if error

// how many xbees in total in the swarm? At AB, we hope to have 8.
// this is currently only used to calculate the receive interval
#define XBEE_SWARM_SIZE 2
// each xbee will broadcast to the swarm at this interval, e.g. every 2000 milliseconds
#define XBEE_SEND_INTERVAL 1000
// if we have 8 xbees in our swarm (harr harr) in total it means this xbee
// could have to process 7 incoming packets every XBEE_SEND_INTERVAL
// we try to service the incoming buffer faster than the incoming packets with a 15% margin
#define XBEE_RECEIVE_INTERVAL int(0.85 * XBEE_SEND_INTERVAL / (XBEE_SWARM_SIZE - 1))

// 8 xbees, 2 second send interval == 242ms receive interval so about 4 times per second

// set to 1 when you have the special SparkFun shield with xbee switchable to pins 2,3
// and you would like to see debug output on the arduino serial monitor
// by default it's 0, which means xbee connected to hardware uart on 0,1 and NO serial monitor :(
#define XBEE_DEBUG 0

#include <Arduino.h>

#include <FastLED.h>
#define NUM_LEDS 40
#define NUM_LEGS 4
#define NUM_PER_LEG 10
// joystick shield with xbee uses everything 0-8
// 9-13 are usually used by nRF24 module which we don't have
#define DATA_PIN 9

// analogue!
#define AUDIO_PIN 1


// XBEE setup ==========================================================================

// need to define this so that XBee includes Arduino.h and not the old WProgram.h
#define ARDUINO 185
#include <XBee.h>

// construct XBee object to be use as API to the xbee module
XBee xbee = XBee();

#if (XBEE_DEBUG == 1)
  // for the special sparkfun shield with DLINE switch, xbee is connected to lines 2,3 (not the default 0,1)
  // PRO-TIP: AltSoftSerial looks great, but on Uno has to use pins 8,9!
  // https://github.com/andrewrapp/xbee-arduino/issues/13#issuecomment-147622072
  // furthermore, in production we use hardware serial, this is only for dev and debugging
  #include <SoftwareSerial.h>
  // have to instantiate here of course so it persists
  SoftwareSerial xbee_serial(2,3);

#define PRINTLN(msg) Serial.println(msg)
#define PRINT(msg) Serial.print(msg)
#define PRINT2(msg, b) Serial.print(msg, b)
#else
#define PRINTLN(msg)
#define PRINT(msg)
#define PRINT2(msg, b)
#endif


// Sets the size of the payload: 2 for MY but 4 for SL
//uint8_t payload[2];
uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };

// Sets the command to get the Serial Low Address.
uint8_t FIX_PAYLOAD_SIZE_FIRST_slCmd[] = {'S','L'};
// sets the command to get the MY address
uint8_t myCmd[] = {'M', 'Y'};
// Sets the command to exit AT mode. 
uint8_t cnCmd[] = {'C','N'};

// Initialises the AT Request and Response.
AtCommandRequest atRequest = AtCommandRequest();
AtCommandResponse atResponse = AtCommandResponse();

// we'll use this to receive incoming packets
Rx16Response rx16 = Rx16Response();

// XBEE setup END ==========================================================================

CRGBArray<NUM_LEDS> leds;
//CRGB leds[NUM_LEDS];
#define FRAMES_PER_SECOND 120

// ============================================================================
// ============================================================================
void setup() {

  // initialise FastLED
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

#if (XBEE_DEBUG == 1)
  // need this for the serial console
  // speed has to match what you setup the connection for (see status line bottom right)
  Serial.begin(9600);
  Serial.println("Hello, hivemind starting up down here...");

  Serial.print("Receive interval: ");
  Serial.println(XBEE_RECEIVE_INTERVAL);

  // use special SoftwareSerial object to talk to xbee on pins 2,3
  // eventually we'll just xbee.begin(Serial);
  
  xbee_serial.begin(9600);
  xbee.begin(xbee_serial);

  Serial.println("About to read SL from connected XBEE:");
#else
  Serial.begin(9600);
  xbee.begin(Serial);
#endif

  // startup delay for xbee to wake-up (I'm desperate)
  delay(2000);
  // show that we're doing something
  leds[0] = CHSV(0.5, 255, 255);
  FastLED.show();

  // read the XBee's serial low address and install it into the payload
  addressRead();

}

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

int min = 1024;
int max = 0;

// ============================================================================
// ============================================================================
void loop() {

  EVERY_N_MILLIS(XBEE_SEND_INTERVAL) {
    // broadcast out MY id
    packetSend();
  }

  EVERY_N_MILLIS(XBEE_RECEIVE_INTERVAL) {
    // and we check if there's anything incoming
    // we have to read packets faster than they come in
    packetRead();
  }

  EVERY_N_MILLIS(8) {

    // TEMP: debugging RSSI
    // fade out by a bit whatever we have
    fadeToBlackBy(leds, NUM_LEDS, 16);
    leds[0] += CHSV(gHue, 255, 32);

    // each of these functions is an evented effect, i.e. it does what it
    // needs to do every N milliseconds, fully timesliced.

    //bidi_bounce();
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

// ============================================================================
// ============================================================================
// Reads the Serial Low Address of the XBee and adds it to the payload.
// taken from https://github.com/DBeath/rssi-aggregator
void addressRead()
{
  // Sets the AT Command to Serial Low Read.
  atRequest.setCommand(myCmd);
  
  // Sends the AT Command.
  xbee.send(atRequest);
  
  // Waits for a response and checks for the correct response code.
  if (xbee.readPacket(1000))
  {  
    if(xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE)
    {
      xbee.getResponse().getAtCommandResponse(atResponse);
      
      if(atResponse.isOk())
      {
        // Reads the entire response and adds it to the payload.
        for (int i = 0; i < atResponse.getValueLength(); i++)
        {
          payload[i] = (atResponse.getValue()[i]);
          PRINT2(payload[i], HEX);
          PRINT(" ");
          // in the case of SL we get for example 41 55 41 85
          // in the case of MY we get for example 0 1

          // second LED means we could read MY from the xbee
          leds[1] += CHSV(gHue, 255, 192);
        }
      }
      PRINTLN(sizeof(payload));
    }
  }
  else {
    leds(0,3) = CHSV(128, 255, 192);
  }
  
  // Sets the AT Command to the Close Command.
  atRequest.setCommand(cnCmd);
  xbee.send(atRequest);
  
  // Wait two seconds. is 1 second enough?
  delay(2000);
  PRINTLN("Done with trying to read SL / MY.");
}

void packetSend()
{ 
  // broadcast to all xbees on our PAN
  // see https://www.digi.com/resources/documentation/digidocs/pdfs/90001500.pdf p35
  Tx16Request tx = Tx16Request(0xFFFF, payload, sizeof(payload));
  xbee.send(tx);
}

// Reads any incoming packets.
void packetRead()
{
  // show that we are trying to read at least
  leds[2] += CHSV(gHue, 255, 192);

  // check if there's anything available for us;
  // rationale here is that we call packetRead() at 20% faster than maximum incoming packets (based on swarm)
  // so I don't want to busy wait here (i.e. with timeout) -- simply check if there's something.
  xbee.readPacket(100);
  if (xbee.getResponse().isAvailable())
  {
    // Checks if the packet is the right type.
    if (xbee.getResponse().getApiId() == RX_16_RESPONSE)
    {
      // Reads the packet.
      xbee.getResponse().getRx16Response(rx16);

      // we have received a packet, yay.
      leds[4] += CHSV(gHue, 255, 192);

      // with every received packet, we get the RSSI
      // this is in -dBm
      PRINT(rx16.getRssi());
      PRINT(" ---> remote: ");
      PRINT(rx16.getRemoteAddress16());
      PRINT(" ---> data ");

      for (int i = 0; i < rx16.getDataLength(); i++)
      {
        //Serial.print(rx16.getData(i),HEX);
        PRINT2(rx16.getData(i), HEX);
      }      
      
      PRINTLN("");

    }  
#ifdef XBEE_REPORT_DIFF_PACKET_TYPE
    else 
    {
      // we get 89 often which is TX_STATUS_RESPONSE
      // could be what's mentioned on p39 of https://www.digi.com/resources/documentation/digidocs/pdfs/90001500.pdf
      // "When working in API mode, a transmit request frame sent by the user is always answered with a
      //  transmit status frame sent by the device, if the frame ID is non-zero."
      // also see p108 where it talks about 0x89 Tx status
      Serial.print(xbee.getResponse().getApiId(), HEX);
      Serial.println(" :: Unexpected Api");
    } 
#endif
  }
  else if (xbee.getResponse().isError())
  {
    PRINT("No Packet :: ");
    PRINTLN(xbee.getResponse().getErrorCode());
    // we get 3 often, which is invalid start byte??! UNEXPECTED_START_BYTE

    leds[6] += CHSV(gHue, 255, 192);
  } 


  FastLED.show();
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