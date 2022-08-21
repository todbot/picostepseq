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
 * - ArduinoJson -- https://arduinojson.org/
 *
 * IDE change:
 * - Select "Tools / Flash Size: 2MB (Sketch: 1MB / FS: 1MB)
 * - Select "Tools / USB Stack: Adafruit TinyUSB"
 *
 **/

#include <Wire.h>
#include <RotaryEncoder.h>
#include <Bounce2.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "fonts/helvnCB6pt7b.h"
#include "fonts/ter_u12n7b.h" // CirPy's default displayio font, it seems

#define myfont helvnCB6pt7b   // sigh
#define myfont2 ter_u12n7b

const int numsteps = 8;
const int numseqs = 8;

#include "Sequencer.hpp"

// begin hardware definitions
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

const int midi_tx_pin = 16;
const int midi_rx_pin = 17;
// end hardware definitions

int led_vals[numsteps];
int led_fade = 20;

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

Step sequences[numseqs][numsteps];

const char* save_file = "/saved_sequences.json";

// callback used by Sequencer to trigger note on
void play_note_on( uint8_t note, uint8_t vel, uint8_t gate, bool on ) {
    Serial.printf("play_note_on: %d %d %d %d\n", note,vel,gate,on);
    if( on ) {
        MIDIusb.sendNoteOn(note, vel, 1); // 1?
        MIDIserial.sendNoteOn(note, vel, 1); // 1?
    }
}

// callback used by Sequencer to trigger note off
void play_note_off(uint8_t note, uint8_t vel, uint8_t gate, bool on ) {
    Serial.printf("play_note_off: %d %d %d %d\n", note,vel,gate,on);
    // always send note off for now
    MIDIusb.sendNoteOff(note, vel, 1);
    MIDIserial.sendNoteOff(note, vel, 1);
}

// core1 is only for MIDI output
void setup1() { }

// core1 is only for MIDI output
void loop1() {
    seqr.update();
}

//  core0 is UI (buttons, knobs, display)
void setup() {
    LittleFS.begin();

    // USB and MIDI
    USBDevice.setManufacturerDescriptor("todbot");
    USBDevice.setProductDescriptor     ("PicoStepSeq");

    Serial1.setRX(midi_rx_pin);
    Serial1.setTX(midi_tx_pin);

    MIDIusb.begin(MIDI_CHANNEL_OMNI);
    MIDIserial.begin(MIDI_CHANNEL_OMNI);

    MIDIusb.turnThruOff();    // turn off echo
    MIDIserial.turnThruOff(); // turn off echo

    sequences_read();
    sequence_load( seqr.seqno ); // 0

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
    pinMode(encoderA_pin, INPUT_PULLUP);
    pinMode(encoderB_pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(encoderA_pin), checkEncoderPosition, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderB_pin), checkEncoderPosition, CHANGE);
    encoder_switch.attach(encoderSW_pin, INPUT_PULLUP);
    encoder_switch.setPressedState(LOW);

    displaySetup();

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

// variables for UI state management
int encoder_delta = 0;
uint32_t encoder_push_millis;
uint32_t step_push_millis;
int step_push = -1;
bool step_edited = false;

// main UI loop
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
            step_edited = true;
        }
        // UI: encoder turn while step key held = change step's note
        else if( step_push > -1 ) {
            int note = seqr.steps[step_push].note;
            note = constrain( note + encoder_delta, 1,127);
            seqr.steps[step_push].note = note;
            step_edited = true;
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
                if( ! seqr.playing ) {
                    sequences_write();  // write to disk on pause
                }
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
                    sequence_save( step_push );
                }
                // UI: encoder push + tap step key = load sequence
                else {
                    sequence_load( step_push );
                }
            }
            else {  // UI: encoder not pushed, mutes or play notes
                // UI: playing: step keys = mutes/unmutes
                if( seqr.playing ) {
                    if( step_edited ) {
                    }
                    else {
                        // UI: if playing, step keys == mute/unmute toggle
                        seqr.steps[i].on = !seqr.steps[i].on;
                    }
                }
                // UI: paused: step keys = play step notes
                else {
                    play_note_off( s.note, s.vel, s.gate, s.on );
                }
            }
            step_push_millis = 0;
            step_push = -1; //
            step_edited = false;
        }
    }

    // DISPLAY update
    displayUpdate();

}

// --- sequence load / save functions

// write all sequences to "disk"
void sequences_write() {
    Serial.println("sequences_write");
    DynamicJsonDocument doc(8192); // assistant said 6144
    for( int j=0; j < numseqs; j++ ) {
        JsonArray seq_array = doc.createNestedArray();
        for( int i=0; i< numsteps; i++ ) {
            Step s = sequences[j][i];
            JsonArray step_array = seq_array.createNestedArray();
            step_array.add( s.note );
            step_array.add( s.vel );
            step_array.add( s.gate );
            step_array.add( s.on );
        }
    }

    LittleFS.remove( save_file );
    File file = LittleFS.open( save_file, "w");
    if( !file ) {
        Serial.println("sequences_write: Failed to create file");
        return;
    }
    if(serializeJson(doc, file) == 0) {
        Serial.println(F("sequences_write: Failed to write to file"));
    }
    file.close();
    serializeJson(doc, Serial);
}

// read all sequences from "disk"
void sequences_read() {
    Serial.println("sequences_read");

    // File f = LittleFS.open( save_file, "r");
    // String s = f.readStringUntil('\n');
    // f.close();
    // Serial.println("  contents:"); Serial.println(s);

    File file = LittleFS.open( save_file, "r");
    if( !file ) {
        Serial.println("sequences_read: no sequences file");
        return;
    }

    DynamicJsonDocument doc(8192); // assistant said 6144
    DeserializationError error = deserializeJson(doc, file); // inputLength);
    if(error) {
        Serial.print("sequences_read: deserialize failed: ");
        Serial.println(error.c_str());
        return;
    }

    for( int j=0; j < numseqs; j++ ) {
        JsonArray seq_array = doc[j];
        for( int i=0; i< numsteps; i++ ) {
            JsonArray step_array = seq_array[i];
            Step s;
            s.note = step_array[0];
            s.vel  = step_array[1];
            s.gate = step_array[2];
            s.on   = step_array[3];
            sequences[j][i] = s;
        }
    }
    file.close();
}

// Load a single sequence from into the sequencer from RAM storage
void sequence_load(int seq_num) {
    Serial.printf("sequence_load:%d\n", seq_num);
    for( int i=0; i< numsteps; i++) {
        seqr.steps[i] = sequences[seq_num][i];
    }
    seqr.seqno = seq_num;
}

// Store current sequence in sequencer to RAM storage"""
void sequence_save(int seq_num) {
    Serial.printf("sequence_save:%d\n", seq_num);
    for( int i=0; i< numsteps; i++) {
        sequences[seq_num][i] = seqr.steps[i];;
    }
}

// --- display details

////
const int step_text_pos[] = { 0,12, 16,12, 32,12, 48,12,  64,12, 80,12, 96,12, 112,12 };
const int bpm_text_pos[] = {0, 57};
const int bpm_val_pos[] = {25, 57};
const int trans_text_pos[] = {55, 57};
const int seqno_text_pos[] = {0, 45};
const int play_text_pos[] = {110, 57};
const int oct_text_offset[] = {3,12};
const int gate_bar_offset[] = {0,-12};
const int gate_bar_width = 14;
const int gate_bar_height = 4;
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
    display.setTextColor(WHITE, 0);
    for( int i=0; i< numsteps; i++ ) {
        Step s = seqr.steps[i];
        const char* nstr = notenum_to_notestr( s.note );
        int o = notenum_to_oct( s.note );
        int x = step_text_pos[i*2],  y = step_text_pos[i*2+1];
        display.setCursor( x,y );
        display.print( nstr );
        display.setCursor( x + oct_text_offset[0], y + oct_text_offset[1] );
        display.printf( "%1d", o );
        int gate_w = 1 + (s.gate * gate_bar_width / 16);
        display.fillRect( x + gate_bar_offset[0], y + gate_bar_offset[1], gate_w, gate_bar_height, WHITE);
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
