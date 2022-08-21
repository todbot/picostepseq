/**
 * picostepseq.ino  -- basic picostepseq functionality in Arduino
 * 15 Aug 2022 - @todbot / Tod Kurt
 * Part of https://github.com/todbot/picostepseq/
 *
 * Libraries needed (all available via Library Manager):
 * - Bounce2 -- https://github.com/thomasfredericks/Bounce2
 * - RotaryEncoder -- http://www.mathertel.de/Arduino/RotaryEncoderLibrary.aspx
 * - Adafruit_SSD1306 -- https://github.com/adafruit/Adafruit_SSD1306
 * - Adafruit_TinyUSB -- https://github.com/adafruit/Adafruit_TinyUSB_Arduino
 * - MIDI -- https://github.com/FortySevenEffects/arduino_midi_library
 *
 * IDE change:
 * - Select "Tools / USB Stack: Adafruit TinyUSB"
 *
 **/

#include <Wire.h>
#include <RotaryEncoder.h>
#include <Bounce2.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

#include "fonts/helvnCB6pt7b.h"
#include "fonts/ter_u12n7b.h" // CirPy's default displayio font
#include "Sequencer.hpp"

#define myfont helvnCB6pt7b
#define myfont2 ter_u12n7b
// see: https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts

const int numsteps = 8;
const int dw = 128;
const int dh = 64;

const int led_pins[] = {0, 2, 4, 6, 8, 10, 12, 14};
const int key_pins[] = {1, 3, 5, 7, 9, 11, 13, 15};

const int encoderA_pin = 18;
const int encoderB_pin = 19;
const int encoderSW_pin = 22;

const int oled_sda_pin = 20;
const int oled_scl_pin = 21;

const int oled_i2c_addr = 0x3C;

Bounce2::Button keys[numsteps];
Bounce2::Button encoder_switch;

RotaryEncoder encoder(encoderB_pin, encoderA_pin, RotaryEncoder::LatchMode::FOUR3);
void checkEncoderPosition() {  encoder.tick(); } // just call tick() to check the state.
int encoder_pos_last = 0;

Adafruit_SSD1306 display(dw, dh, &Wire, -1);

Adafruit_USBD_MIDI usb_midi;  // USB MIDI object

MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDIusb); // USB MIDI
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDIserial);   // Serial MIDI

float tempo = 100;
StepSequencer seqr;

void play_note_on( uint8_t note, uint8_t vel, uint8_t gate, bool on ) {
    Serial.printf("play_note_on: %d %d %d %d\n", note,vel,gate,on);
}

void play_note_off(uint8_t note, uint8_t vel, uint8_t gate, bool on ) {
    Serial.printf("play_note_off: %d %d %d %d\n", note,vel,gate,on);
}

void setup1() {
}

void loop1() {
    //
    seqr.update();

    //delay(10);
}

void setup() {
    // USB and MIDI
    USBDevice.setManufacturerDescriptor("todbot");
    USBDevice.setProductDescriptor     ("PicoStepSeq");

    MIDIusb.begin(MIDI_CHANNEL_OMNI);
    MIDIserial.begin(MIDI_CHANNEL_OMNI);

    MIDIusb.turnThruOff();    // turn off echo
    MIDIserial.turnThruOff(); // turn off echo


    seqr.set_tempo(tempo);
    seqr.on_func = play_note_on;
    seqr.off_func = play_note_off;

    // KEYS
    for (uint8_t i=0; i< numsteps; i++) {
        keys[i].attach( key_pins[i], INPUT_PULLUP);
        keys[i].setPressedState(LOW);
    }

    // LEDS
    for (uint8_t i=0; i< numsteps; i++) {
        pinMode(led_pins[i], OUTPUT);
        analogWrite(led_pins[i], i*31); // just to test
    }

    // ENCODER
    //pinMode(encoderSW_pin, INPUT_PULLUP);
    pinMode(encoderA_pin, INPUT_PULLUP);
    pinMode(encoderB_pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(encoderA_pin), checkEncoderPosition, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderB_pin), checkEncoderPosition, CHANGE);
    encoder_switch.attach(encoderSW_pin, INPUT_PULLUP);
    encoder_switch.setPressedState(LOW);

    // random setup data
    for( int i=0; i< numsteps; i++) {
        seqr.steps[i].note = random(36,60);
        seqr.steps[i].vel = 100;
        seqr.steps[i].gate = random(1,16);
        seqr.steps[i].on = true;
    }

    displaySetup();
    delay(1000);
}

void displaySetup() {
    // DISPLAY
    Wire.setSDA( oled_sda_pin );
    Wire.setSCL( oled_scl_pin );
    Wire.begin();

    if(!display.begin(SSD1306_SWITCHCAPVCC, oled_i2c_addr)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }
    display.clearDisplay();
    display.display();  // must clear before display, otherwise shows adafruit logo
}

char keyspressed[numsteps+1];
int encoder_delta = 0;
uint32_t encoder_push_millis;
uint32_t step_push_millis;
int step_push = -1;
int led_vals[numsteps]; // need this because we cannot read back analogWrite() values
int led_fade = 20;

void loop()
{
    // LEDS update
    for( int i=0; i<numsteps; i++) {
        Step s = seqr.steps[i];
        int v = 0;                         // UI: off = muted
        if( i == seqr.stepi ) { v = 255; } // UI: bright red = indicates sequence postion
        else if( s.on )       { v = 20;  } // UI: dim red = indicates mute/unmute state
        v = max( led_vals[i] - led_fade, v); // nice 80s fade
        led_vals[i] = v;
        analogWrite(led_pins[i], v);
    }

    // ENCODER update
    encoder_switch.update();
    encoder.tick();
    int encoder_pos = encoder.getPosition();
    if(encoder_pos != encoder_pos_last) {
        encoder_delta = encoder_pos - encoder_pos_last;
        encoder_pos_last = encoder_pos;
    }

    uint32_t now = millis();

    // on encoder turn
    if( encoder_delta ) {
        // UI: encoder turn + push while step key held = change step's gate
        if( step_push > -1 && encoder_push_millis > 0 ) {
            int gate = seqr.steps[step_push].gate;
            gate = constrain( gate + encoder_delta, 1,15);
            seqr.steps[step_push].gate = gate;
        }
        // UI: encoder turn while step key held = change step's note
        else if( step_push > -1 ) {
            int note = seqr.steps[step_push].note;
            note = constrain( note + encoder_delta, 1,127);
            seqr.steps[step_push].note = note;
        }
        // UI: encoder turn while encoder pushed = change tempo
        else if( encoder_push_millis > 0 ) {
            tempo = tempo + encoder_delta;
            seqr.set_tempo( tempo );
            Serial.printf("TEMPO: %d %f\n", seqr.tempo(), seqr.tempo());
        }
        // UI: encoder turn = change transpose
        else {
            seqr.transpose = constrain(seqr.transpose + encoder_delta, -36, 36);
        }
        encoder_delta = 0;  // we own turning, and we've used it
    }

    // on encoder push
    if( encoder_switch.pressed() ) { // push encoder button
        encoder_push_millis = now; // save when (and that) it was pushed
    }

    if( encoder_switch.released() ) { // release encoder button
        if( step_push == -1 && encoder_delta == 0 ) { // step key is not pressed and no encoder turn
            // UI: encoder tap with no key == play/pause
            if( now - encoder_push_millis < 300 ) {
                seqr.toggle_play_pause();
            }
        }
        encoder_push_millis = 0;  // we own it, and we're done with it
    }

    // KEYS update
    for (uint8_t i=0; i< numsteps; i++) {
        keys[i].update();
        Step s = seqr.steps[i]; // FIXME: is this ia copy?

        if( keys[i].pressed() ) { // press
            step_push = i;
            step_push_millis = now;

            if( encoder_push_millis > 0 ) {
                // do nothing
            }
            else {
                if( seqr.playing ) {
                    // do nothing, happens on release
                }
                // UI: if not playing, step keys = play their notes
                else {
                    play_note_on( s.note, s.vel, s.gate, s.on );
                }
            }
        }

        else if( keys[i].released() ) { // release
            if( encoder_push_millis > 0 ) {  // UI: load/save sequence mode
                // UI: encoder push + hold step key = save sequence
                if( now - step_push_millis > 1000 ) {
                    Serial.printf("save sequence: %d\n", i);
                }
                // UI: encoder push + tap step key = load sequence
                else {
                    seqr.seqno = i;
                    Serial.printf("load sequence: %d\n", i);
                }
            }
            else {  // UI: encoder not pushed, mutes or play notes
                // UI: playing: step keys = mutes/unmutes
                if( seqr.playing ) {
                    seqr.steps[i].on = !seqr.steps[i].on;
                }
                // UI: paused: step keys = play step notes
                else {
                    play_note_off( s.note, s.vel, s.gate, s.on );
                }
            }
            step_push_millis = 0;
            step_push = -1; //
        }
    }

    // DISPLAY update
    displayUpdate();

}

////
const int step_text_pos[] = { 0,12, 16,12, 32,12, 48,12,  64,12, 80,12, 96,12, 112,12 };
const int bpm_text_pos[] = {0, 57};
const int bpm_val_pos[] = {25, 57};
const int trans_text_pos[] = {55, 57};
const int seqno_text_pos[] = {0, 45};
const int play_text_pos[] = {110, 57};
const int oct_text_offset[] = {3,12};
const int gate_bar_offset[] = {0,-12};
const int gate_bar_dim[] = {14,4};
const int edit_text_offset[] = {3,22};

const char* note_strs[] = { "A ", "A#", "B ", "C ", "C#", "D ", "D# ", "E ", "F ", "F#", "G ", "G#"};

int notenum_to_oct(int notenum) {
    return (notenum / 12) - 1;
}

const char* notenum_to_notestr(int notenum) {
    return note_strs[ notenum % 12 ];
}

void displayUpdate()
{
    display.clearDisplay();
    display.setFont(&myfont);
    display.setTextColor(SSD1306_WHITE, 0);
    for( int i=0; i< numsteps; i++ ) {
        Step s = seqr.steps[i];
        const char* nstr = notenum_to_notestr( s.note );
        int o = notenum_to_oct( s.note );
        int x = step_text_pos[i*2],  y = step_text_pos[i*2+1];
        display.setCursor( x,y );
        display.print( nstr );
        display.setCursor( x + oct_text_offset[0], y + oct_text_offset[1] );
        display.printf( "%1d", o );
    }

    display.setFont(&myfont2);

    // bpm
    display.setCursor(bpm_text_pos[0], bpm_text_pos[1]);
    display.print("bpm:");
    //display.printf("bpm:%3d", (int)(seqr.tempo()) );
    display.setCursor(bpm_val_pos[0], bpm_val_pos[1]);
    display.printf("%3d", (int)(seqr.tempo()) );

    // trans
    display.setCursor(trans_text_pos[0], trans_text_pos[1]);
    display.printf("trs:%+2d", seqr.transpose);

    // seqno
    display.setCursor(seqno_text_pos[0], seqno_text_pos[1]);
    display.printf("seq:%d", seqr.seqno + 1); // user sees 1-8

    // play/pause
    display.setCursor(play_text_pos[0], play_text_pos[1]);
    display.print( seqr.playing ? " >" : "||" );

    display.display();
}
