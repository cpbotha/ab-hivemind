// afrikaburn 2018 hivemind-tv
// copyright 2018 Charl Botha

// status on 2018-03-26
// - problems solved by NOT touching RTS, waiting forever for xbee to respond to initial contact (takes 10s)
//   and finally figuring out right way to jumper up itead.
// - we are just waiting for hardware to 1. scale up and 2. test battery duration.

// status on 2018-03-04
// - I have sporadic communication with xbees connected via sparkfun dline (SoftSerial 2,3)
// - on the hardware UART (d0,d1) I can't even get the first AT command to respond;
//   First 4 LEDS flash to indicate that nothing came back.
//   also after startup, I can see only the DIN light on the sparkfun shield and the TX light on the arduino flash, but never the DOUT / RX
// - BOOHOO I have also disabled ATD7 CTS flow control pin on the xbee still no love.
//   - useful http://randomstuff-ole.blogspot.co.za/2009/09/xbee-woes.html
// - left a comment here also https://www.sparkfun.com/products/12847#comment-5a9f0ba9807fa8ad5f8b4567

// BEFORE YOU FLASH THIS TO THE SWARM UNITS FOR PRODUCTION:
// 1. set the swarm size
// 2. set the relevant HARDWARE_CONFIG
// 3. disable DEBUG_MODE

// how many xbees in total in the swarm? At AB, we hope to have 8.
// this is currently only used to calculate the receive interval
#define XBEE_SWARM_SIZE 8
// each xbee will broadcast to the swarm at this interval, e.g. every 2000 milliseconds
#define XBEE_SEND_INTERVAL 1000
// if we have 8 xbees in our swarm (harr harr) in total it means this xbee
// could have to process 7 incoming packets every XBEE_SEND_INTERVAL
// we try to service the incoming buffer faster than the incoming packets with a 15% margin
#define XBEE_RECEIVE_INTERVAL int(0.85 * XBEE_SEND_INTERVAL / (XBEE_SWARM_SIZE - 1))
// if our last comms from another xbee is older than this, then it is probably out of range
#define XBEE_LOST_WAIT 3*XBEE_SEND_INTERVAL

// 8 xbees, 2 second send interval == 242ms receive interval so about 4 times per second

// set to 1 when you have the special SparkFun shield with xbee switchable to pins 2,3
// and you would like to see debug output on the arduino serial monitor
// by default it's 0, which means xbee connected to hardware uart on 0,1 and NO serial monitor :(
#define DEBUG_MODE 1
#define HARDWARE_CONFIG 0

#if (HARDWARE_CONFIG == 0)
// m0 + itead shield
  #define SERIAL_XBEE Serial1
  #define SERIAL_MON SerialUSB
  // fastled needs this
  // https://github.com/FastLED/FastLED/issues/393
  #define ARDUINO_SAMD_ZERO

  // joystick shield with xbee uses everything 0-8
  // 9-13 are usually used by nRF24 module which we don't have
  // we're on itead, so pin 5 is ok
  #define LEDS_PIN 7

#elif (HARDWARE_CONFIG == 1)
// uno + sparkfun shield in UART mode

// in this mode, there can be no serial monitor
#define DEBUG_MODE 0
#define SERIAL_XBEE Serial
#define SERIAL_MON

#define LEDS_PIN 7
#elif (HARDWARE_CONFIG == 2)
// uno + sparkfun shield in dline mode
  // NOT RECOMMENDED FOR PRODUCTION
  #include <SoftwareSerial.h>
  SoftwareSerial SERIAL_XBEE(2, 3);
  #define SERIAL_MON Serial

  #define LEDS_PIN 7
#endif

#include <Arduino.h>

#include <FastLED.h>
#define NUM_LEDS 20

CRGBArray<NUM_LEDS> leds;

// XBEE setup ==========================================================================

// need to define this so that XBee includes Arduino.h and not the old WProgram.h
#define ARDUINO 185
#include <XBee.h>

// construct XBee object to be use as API to the xbee module
XBee xbee = XBee();

#if (DEBUG_MODE == 1)
// for the special sparkfun shield with DLINE switch, xbee is connected to lines 2,3 (not the default 0,1)
  // PRO-TIP: AltSoftSerial looks great, but on Uno has to use pins 8,9!
  // https://github.com/andrewrapp/xbee-arduino/issues/13#issuecomment-147622072
  // furthermore, in production we use hardware serial, this is only for dev and debugging
  //#include <SoftwareSerial.h>
  // have to instantiate here of course so it persists
  //SoftwareSerial xbee_serial(2,3);

#define PRINTLN(msg) SERIAL_MON.println(msg)
#define PRINTLN2(msg, b) SERIAL_MON.println(msg, b)
#define PRINT(msg) SERIAL_MON.print(msg)
#define PRINT2(msg, b) SERIAL_MON.print(msg, b)
#else
#define PRINTLN(msg)
#define PRINTLN2(msg, b)
#define PRINT(msg)
#define PRINT2(msg, b)
#endif


// Sets the size of the payload: 2 for MY but 4 for SL
//uint8_t payload[2];
// let's try 1 byte payload to see if it helps
uint8_t payload[] = { 0xDE };

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

// proximity / hivemind data structures ====================================================

// last time we saw the relevant xbee
decltype(millis()) swarm_last_contacts[XBEE_SWARM_SIZE];

typedef decltype(rx16.getRssi()) rssi_t;
// rssi_t is uint8_t with range 0..255
// this will initialize each element to 0, which we take to mean NO CONTACT
// usually it's -dBm, but 0 is an impossibility (-10dBm is very close, -70dBm is far)
rssi_t swarm_rssis[XBEE_SWARM_SIZE];
// swarm_min will be the closest that any other xbee has come to me
rssi_t swarm_min = 255;
// swarm_max will be the furthest that any other xbee has been from me before losing contact
rssi_t swarm_max = 1;

float circle_pos[XBEE_SWARM_SIZE];
float circle_speed[XBEE_SWARM_SIZE];

// 8-class pastel1 qualitative colour scheme
// http://colorbrewer2.org/?type=qualitative&scheme=Pastel1&n=8
const CRGB pastel1[] = { 0xfbb4ae, 0xb3cde3, 0xccebc5, 0xdecbe4, 0xfed9a6, 0xffffcc, 0xe5d8bd, 0xfddaec };
// 8-class dark 2
// http://colorbrewer2.org/?type=qualitative&scheme=Dark2&n=8
// green, brown, purple, pink, light green, mustard, khaki, grey
const CRGB dark2[] = { 0x1b9e77, 0xd95f02, 0x7570b3, 0xe7298a, 0x66a61e, 0xe6ab02, 0xa6761d, 0x666666 };

// function prototypes -- eventually move these out
void addressRead();
void sinelon();
void packetSend();
void packetRead();

// ============================================================================
// ============================================================================
void setup() {

#if (DEBUG_MODE == 1)
    SERIAL_MON.begin(9600);

    // at least on the M0 we have to wait for the serial port (USB) to connect
    // let's not wait longer than 5 seconds, else arduino does not get past this bit
    // if no monitor is connected, and we forgot to disable debug mode.
    auto usb_start_wait = millis();
    while (!SERIAL_MON && millis() - usb_start_wait < 5000) {
        ;
    }

    // need this for the serial console
    // speed has to match what you setup the connection for (see status line bottom right)
    SERIAL_MON.begin(9600);
    SERIAL_MON.println("Hello, hivemind starting up down here...");

    SERIAL_MON.print("Receive interval: ");
    SERIAL_MON.println(XBEE_RECEIVE_INTERVAL);

#endif

    SERIAL_XBEE.begin(9600);
    xbee.begin(SERIAL_XBEE);

    // DANGER WILL ROBINSON DANGER!
    // on the itead shield, doing ANYTHING with a0 which is connected to RTS and sometimes to DIN
    // will cause the xbee to hardware reset every 5 seconds
    //#define RTS_PIN A0
    //pinMode(RTS_PIN, OUTPUT);
    //digitalWrite(RTS_PIN, HIGH);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(LED_BUILTIN, LOW);

    // initialise FastLED
    FastLED.addLeds<NEOPIXEL, LEDS_PIN>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

    // read the XBee's serial low address and install it into the payload
    addressRead();

}

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

int min = 1024;
int max = 0;

// ============================================================================
// ============================================================================

// if we haven't seen someone in more than XBEE_LOST_WAIT, we indicate that we've lost it by resetting rssi to 0
void detect_lost_souls() {
    for (int i = 0; i < XBEE_SWARM_SIZE; i++) {
        if (swarm_rssis[i] != 0 && millis() - swarm_last_contacts[i] > XBEE_LOST_WAIT) {
            swarm_rssis[i] = 0;
            PRINT("Lost MY=");
            PRINTLN(i+1);
        }
    }
}

void soul_lights() {

    // for each soul, we show two rotating LEDs, opposite side of the circle
    // rotation speed + brightness is a function of closeness
    // if lost?

    // highest speed is 1/4 light per frame, i.e. 15 positions per second

    // 2018-03-27: still experimenting with exact visualization
    // todo: remove 1 light (that's me)
    // todo: make soul light significantly dimmer if it's far away (so we have slow-moving AND dim, haha)
    // todo: stochastic direction change, i.e. a 10% chance that a led can change direction; NO WAIT
    //       if someone is moving closer, they move in one direction; further, they go in the other?

    fadeToBlackBy(leds, NUM_LEDS, 192);

    for (int i = 0; i < XBEE_SWARM_SIZE; i++) {
        //circle_pos[i] += circle_speed[i];
        // dummy speeds
        //circle_pos[i] += random8() / 255.0;
        circle_pos[i] += (i+1) / 32.0;

        if (circle_pos[i] > NUM_LEDS-1) circle_pos[i]=0;

        auto new_led = int(round(circle_pos[i]));
        leds[new_led] += CRGB(dark2[i]).fadeLightBy(128);
        //leds[new_led + NUM_LEDS / 2] += dark2[i];
    }

}

void loop() {

    EVERY_N_MILLIS(XBEE_SEND_INTERVAL) {
        digitalWrite(LED_BUILTIN, HIGH);
        // broadcast out MY id
        packetSend();

    }

    EVERY_N_MILLIS(XBEE_RECEIVE_INTERVAL) {
        digitalWrite(LED_BUILTIN, LOW);
        // and we check if there's anything incoming
        // we have to read packets faster than they come in
        packetRead();

        // right after packetRead, go through the last received timestamps and then:
        // if we haven't seen someone for a while (about 3 x broadcast period)
        // THEY HAVE BEEN LOST PLEASE FIND THEM PLEASE.
        detect_lost_souls();
    }

    EVERY_N_MILLIS(16) {
        // each of these functions is an evented effect, i.e. it does what it
        // needs to do every N milliseconds, fully timesliced.

        // sinelon is for testing: nicely coloured loop going to and fro
        //sinelon();

        soul_lights();

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
void addressRead() {
    // Sets the AT Command to Serial Low Read.
    atRequest.setCommand(myCmd);

    auto got_at_response = false;
    int progress = 0;
    while (!got_at_response) {

        // Sends the AT Command.
        xbee.send(atRequest);

        // during startup, light a new LED for each try to read from xbee
        leds = CRGB::Black;
        leds[progress] = dark2[progress % 8]; // test our palette; but modulo 8 so we wrap!
        leds[progress] %= 50; // make 50% darker

        if (++progress >= NUM_LEDS) progress = 0;
        FastLED.show();

        if (xbee.readPacket(1000)) {
            if(xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE) {

                got_at_response = true;
                xbee.getResponse().getAtCommandResponse(atResponse);

                if(atResponse.isOk()) {

#if (DEBUG_MODE == 1)
                    // Reads the entire response prints to serial monitor
                    for (int i = 0; i < atResponse.getValueLength(); i++) {
                        PRINT2(atResponse.getValue()[i], HEX);
                        PRINT(" ");
                        // in the case of SL we get for example 41 55 41 85
                        // in the case of MY we get for example 0 1
                    }

                    PRINTLN("Successfully read from xbee!");
#endif

                    // we only store the significant byte of MY: 1, 2, 3, ...
                    payload[0] = (atResponse.getValue()[1]);

                    // second LED means we could read MY from the xbee
                    //leds[1] += CHSV(gHue, 255, 192);
                }

            }
            else if (xbee.getResponse().getApiId() == MODEM_STATUS_RESPONSE) {
                auto msr = ModemStatusResponse();
                xbee.getResponse().getModemStatusResponse(msr);
                PRINT("MODEM_STATUS_RESPONSE: ");
                PRINTLN(msr.getStatus());
                // 0: HARDWARE_RESET
                // for the rest see https://github.com/andrewrapp/xbee-arduino/blob/4e839822724eadc58d4626e36baed235454a0b9d/XBee.h#L129

            }
            else {
                // could easily be RX_16_RESPONSE when other arduinos are transmitting
                PRINT("Unexpected ApiId :: ");
                PRINT2(xbee.getResponse().getApiId(), HEX);
                PRINTLN("");
            }
        } // if (xbee.readPacket(...) ...
        else {
            // error reading my own adress
            PRINTLN("Could net yet read MY address!");
        }
    }

    // Sets the AT Command to the Close Command.
    atRequest.setCommand(cnCmd);
    xbee.send(atRequest);

    // Wait two seconds. is 1 second enough?
    delay(2000);
    PRINTLN("Done with trying to read SL / MY.");
}

void packetSend() {
    // broadcast to all xbees on our PAN
    // see https://www.digi.com/resources/documentation/digidocs/pdfs/90001500.pdf p35
    Tx16Request tx = Tx16Request(0xFFFF, payload, sizeof(payload));
    xbee.send(tx);
}

#define XBEE_REPORT_DIFF_PACKET_TYPE 1

// Reads any incoming packets.
void packetRead() {
    // get at least one packet vs reading until there's nothing more to read
    auto got_my_packet = false;
    // if we're out of range of everyone, we won't get ANY packets so we have
    // to limit our retries
    int num_tries = 3;

    while (!got_my_packet && num_tries > 0) {
        --num_tries;
        if (xbee.readPacket(16)) {
            // Checks if the packet is the right type.
            if (xbee.getResponse().getApiId() == RX_16_RESPONSE) {
                // this is the right packet, make sure we get out of our loop
                got_my_packet = true;

                // Reads the packet.
                xbee.getResponse().getRx16Response(rx16);

                // first we rip out the low byte of the source address, and subtract 1 to get base-0 index
                auto remote_idx = (uint8_t)(rx16.getRemoteAddress16() & 0xFF) - 1;
                // store the successful contact
                swarm_last_contacts[remote_idx] = millis();
                // with every received packet, we get the RSSI
                // this is in -dBm, ranging from about -[20]dBm when very close to about -[70]dBm very far
                // we get the [x] back in the getRssi() byte
                // store it, min/max it
                auto rssi = rx16.getRssi();
                swarm_rssis[remote_idx] = rssi;
                if (rssi > swarm_max) swarm_max = rssi;
                if (rssi < swarm_min) swarm_min = rssi;

                PRINT(millis());
                PRINT(" -");
                PRINT(rssi);
                PRINT("dB ---> remote idx: ");
                // we only set the LOW byte of MY:
                PRINT(remote_idx);
                PRINT(" ---> data ");

                for (int i = 0; i < rx16.getDataLength(); i++)
                {
                    //Serial.print(rx16.getData(i),HEX);
                    PRINT2(rx16.getData(i), HEX);
                }

                PRINTLN("");

            }
#if (DEBUG_MODE == 1)
            else if (xbee.getResponse().getApiId() != TX_STATUS_RESPONSE) {
                // we get 89 often which is TX_STATUS_RESPONSE
                // could be what's mentioned on p39 of https://www.digi.com/resources/documentation/digidocs/pdfs/90001500.pdf
                // "When working in API mode, a transmit request frame sent by the user is always answered with a
                //  transmit status frame sent by the device, if the frame ID is non-zero."
                // also see p108 where it talks about 0x89 Tx status
                // it's normal to get these (every time), so we don't report.

                SERIAL_MON.print(xbee.getResponse().getApiId(), HEX);
                SERIAL_MON.println(" :: Unexpected Api");

                // In the bad old days, I used to get MODEM_STATUS_RESPONSE with the itead shield; that's because I should not touch RTS!
                // https://stackoverflow.com/questions/20839347/xbee-pro-s1-always-gets-response-with-api-modem-status-response-instead-of-nothi
                // this shows how to unpack the MODEM_STATUS_RESPONSE: https://forum.arduino.cc/index.php?topic=383874.15
            }
#endif
        } // readPacket() has returned false, which could be due to an error
        else if (xbee.getResponse().isError()) {
            PRINT("No Packet :: ");
            PRINTLN(xbee.getResponse().getErrorCode());
            // we get 3 often, which is invalid start byte??! UNEXPECTED_START_BYTE

            //leds[6] += CHSV(gHue, 255, 192);
        }
    }
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
