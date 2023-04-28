/**
 * picostepseq.ino  -- basic picostepseq functionality in Arduino
 * 15 Aug 2022 - @todbot / Tod Kurt
 * Part of https://github.com/todbot/picostepseq/
 *
 * Note: This is a very fast implemenation of the CircuitPython PicoStepSeq firmware
 *       It is not complete.
 *
 * Libraries needed (all available via Library Manager):
 * - Bounce2 -- https://github.com/thomasfredericks/Bounce2
 * - RotaryEncoder -- http://www.mathertel.de/Arduino/RotaryEncoderLibrary.aspx
 * - Adafruit_SSD1306 -- https://github.com/adafruit/Adafruit_SSD1306
 * - Adafruit_TinyUSB -- https://github.com/adafruit/Adafruit_TinyUSB_Arduino
 * - MIDI -- https://github.com/FortySevenEffects/arduino_midi_library
 * - ArduinoJson -- https://arduinojson.org/
 *
 * To upload:
 * - Use Arduino IDE 1.8.19
 * - Install arduino-pico Arduino core https://arduino-pico.readthedocs.io/en/latest/install.html
 * - Install the "PicoLittleFS tool" as described here:
 *    https://arduino-pico.readthedocs.io/en/latest/fs.html#uploading-files-to-the-littlefs-file-system
 * - Once it's installed, restart the Arduino IDE
 * - Open up the 'picostepseq/arduino/picostep' sketch
 * - In "Tools", set "Flash Size: 2MB (Sketch: 1MB / FS: 1MB)"
 * - In "Tools", set "Tools / USB Stack: Adafruit TinyUSB"
 * - *In "Tools", choose "Pico LittleFS Data Upload" (this will upload default 'saved_sequences.json)
 * - Finally you can program the sketch to the Pico with "Upload"
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

#include "Sequencer.h"

#define myfont helvnCB6pt7b   // sigh
#define myfont2 ter_u12n7b

uint8_t midi_chan = 1;  // MIDI channel to send/receive on
const char* save_file = "/saved_sequences.json";
const bool send_midi_clock = true;
const bool midi_debug = false;

bool midi_uart_enable = true; // unused

const int numseqs = 8;

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
int led_fade = 25;

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

uint8_t midiclk_cnt = 0;
uint32_t midiclk_last_millis = 0; // FIXME: use micros()

// callback used by Sequencer to trigger note on
void play_note_on( uint8_t note, uint8_t vel, uint8_t gate, bool on ) {
    if( on ) {
        MIDIusb.sendNoteOn(note, vel, midi_chan);
        MIDIserial.sendNoteOn(note, vel, midi_chan);
    }
    if(midi_debug) { Serial.printf("noteOn:  %d %d %d %d\n", note,vel,gate,on); }
}

// callback used by Sequencer to trigger note off
void play_note_off(uint8_t note, uint8_t vel, uint8_t gate, bool on ) {
    MIDIusb.sendNoteOff(note, vel, midi_chan);
    MIDIserial.sendNoteOff(note, vel, midi_chan);
    if(midi_debug) { Serial.printf("noteOff: %d %d %d %d\n", note,vel,gate,on); }
}
// callback used by Sequencer to send midi clock
void send_clock(clock_type_t type, int pos) {
    (void)pos;  // not used yet
    if( type == START ) {
        MIDIusb.sendStart();
        MIDIserial.sendStart();
    }
    else if( type == STOP ) {
        MIDIusb.sendStop();
        MIDIserial.sendStop();
    }
    else if( type == RUN ) {
        MIDIusb.sendClock();
        MIDIserial.sendClock();
    }
    if(midi_debug) { Serial.printf("clk:%d %d\n", type,pos); }
}

void handle_midi_in_songpos(unsigned int beats) {
    Serial.printf("songpos:%d\n", beats);
    if( beats == 0 ) {
        seqr.stepi = 0;
    }
}
void handle_midi_in_start() {
    seqr.play();
    if(midi_debug) { Serial.println("midi in start"); }
    midiclk_cnt = 0;
}
void handle_midi_in_stop() {
    seqr.stop();
    if(midi_debug) { Serial.println("midi in stop"); }
}
void handle_midi_in_clock() {
    // once every 1/16th note (24 ticks (pulses) per quarter note => 6 pulses per 16th note)
    if( midiclk_cnt % ticks_per_step == 0 ) {  // ticks_per_step = 6
        uint32_t now = millis();
        seqr.trigger_ext(now);

        // once every quarter note (just so we aggregate some time to minimize error)
        if( midiclk_cnt % (ticks_per_step * steps_per_beat) == 0 ) {
            uint32_t step_millis = (now - midiclk_last_millis) / steps_per_beat;
            midiclk_last_millis = now;
            //seqr.step_millis = step_millis;
            seqr.tick_micros = step_millis * 1000;
            midiclk_cnt = 0;
        }
    }
    midiclk_cnt++;
}
//void handle_midi_note_on_test(byte channel, byte note, byte velocity) {
//    Serial.printf("MIDI NOTE ON: %d %d %d\n", channel, note, velocity);
//}

////////////////////////////

//  core0 is MIDI in/output
void setup() {
    // USB and MIDI
    USBDevice.setManufacturerDescriptor("todbot");
    USBDevice.setProductDescriptor     ("PicoStepSeq");

    Serial1.setRX(midi_rx_pin);
    Serial1.setTX(midi_tx_pin);

    MIDIusb.begin();
    MIDIserial.begin();

    MIDIusb.turnThruOff();    // turn off echo
    MIDIserial.turnThruOff(); // turn off echo

    MIDIusb.setHandleClock(handle_midi_in_clock);
    MIDIusb.setHandleStart(handle_midi_in_start);
    MIDIusb.setHandleStop(handle_midi_in_stop);
    MIDIusb.setHandleSongPosition(handle_midi_in_songpos);

    MIDIserial.setHandleClock(handle_midi_in_clock);
    MIDIserial.setHandleStart(handle_midi_in_start);
    MIDIserial.setHandleStop(handle_midi_in_stop);
    MIDIserial.setHandleSongPosition(handle_midi_in_songpos);

    //MIDIserial.setHandleNoteOn(handle_midi_note_on_test);
}

// core0 is only for MIDI in/out
void loop() {
    MIDIusb.read();
    MIDIserial.read();
    seqr.update();  // will call play_note_{on,off} callbacks
    yield();
}

// core1 is only for UI (buttons, knobs, display)
void setup1() {
    delay(2000);

    LittleFS.begin();

    sequences_read();
    sequence_load( seqr.seqno ); // 0

    seqr.set_tempo(tempo);
    seqr.on_func = play_note_on;
    seqr.off_func = play_note_off;
    seqr.clk_func = send_clock;
    seqr.send_clock = send_midi_clock;

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
char seq_meta[6]; // 5 chars + nul FIXME

// main UI loop
void loop1()
{
    // LEDS update
    for( int i=0; i<numsteps; i++) {
        Step s = seqr.steps[i];
        int v = 0;                         // UI: off = muted
        if( i == seqr.stepi ) { v = 255; } // UI: bright red = indicates sequence postion
        else if( s.on )       { v = 40;  } // UI: dim red = indicates mute/unmute state
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

    if( encoder_push_millis > 0 && step_push_millis > 0 ) {
        if( encoder_push_millis < step_push_millis ) { // encoder pushed first
            //Serial.println("SAVE sequence");
        }
    }

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
            Step s = seqr.steps[step_push];
            if( ! seqr.playing ) { // step preview note off
                play_note_off( s.note, s.vel, s.gate, true);
            }
            s.note = constrain( s.note + encoder_delta, 1,127);
            if( ! seqr.playing ) { // step preview note on
                play_note_on( s.note, s.vel, s.gate, true);
            }
            seqr.steps[step_push] = s;
            step_edited = true;
        }
        // UI: encoder turn while encoder pushed = change tempo
        else if( encoder_push_millis > 0 ) {
            tempo = tempo + encoder_delta;
            seqr.set_tempo( tempo );
            Serial.printf("TEMPO: %d %f\n", (int)seqr.tempo(), seqr.tempo());
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
                delay(10); // wait a bit for sequencer to change state
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
                    play_note_on( s.note, s.vel, s.gate, true );
                }
            }
        }

        else if( keys[i].released() ) { // release
            if( encoder_push_millis > 0 ) {  // UI: load/save sequence mode
                // UI: encoder push + hold step key = save sequence
                if( now - step_push_millis > 1000 ) {
                    sequence_save( step_push );
                    strcpy(seq_meta, "saved");
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
                    play_note_off( s.note, s.vel, s.gate, true );
                }
            }
            step_push_millis = 0;
            step_push = -1; //
            step_edited = false;
        }
    }

    // DISPLAY update
    displayUpdate(step_push);

}

// --- sequence load / save functions

uint32_t last_sequence_write_millis = 0;
// write all sequences to "disk"
void sequences_write() {
    Serial.println("sequences_write");
    // save wear & tear on flash, only allow writes every 10 seconds
    if( millis() - last_sequence_write_millis < 10*1000 ) { // only allow writes every 10 secs
        Serial.println("sequences_write: too soon, wait a bit more");
    }
    last_sequence_write_millis = millis();

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
    Serial.println("\nsequences saved");
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
        // make up some filler sequences
        for( int j=0; j < numseqs; j++ ) {
          for( int i=0; i< numsteps; i++ ) {
            Step s;
            s.note = 60 + random(-12,12);
            s.vel  = 127;
            s.gate = 10;
            s.on   = true;
            sequences[j][i] = s;
          }
        }
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

//// {x,y} locations of screen items
const int step_text_pos[] = { 0,15, 16,15, 32,15, 48,15,  64,15, 80,15, 96,15, 112,15 };
const int bpm_text_pos[] = {0, 57};
const int bpm_val_pos[] = {25, 57};
const int trans_text_pos[] = {55, 57};
const int seqno_text_pos[] = {0, 45};
const int seq_meta_pos[]   = {60, 45};
const int play_text_pos[] = {110, 57};
const int oct_text_offset[] = {3,10};
const int gate_bar_offset[] = {0,-15};
const int gate_bar_width = 14;
const int gate_bar_height = 4;
const int edit_text_offset[] = {3,22};

const char* note_strs[] = { "C ", "C#", "D ", "D# ", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B ", "C "};

int notenum_to_oct(int notenum) {
    return (notenum / 12) - 2;
}

const char* notenum_to_notestr(int notenum) {
    return note_strs[ notenum % 12 ];
}

void displayUpdate(int selected_step)
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
        display.setCursor(x + edit_text_offset[0], y + edit_text_offset[1] );
        display.print( (i==selected_step) ? '^' : (s.on) ? ' ': '*');
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

    // seq meta
    display.setCursor(seq_meta_pos[0], seq_meta_pos[1]);
    display.print( seq_meta ); // FIXME: this is onscren too briefly and what does CirPy version do?
    strcpy(seq_meta,"     ");

    // play/pause
    display.setCursor(play_text_pos[0], play_text_pos[1]);
    display.print( seqr.playing ? " >" : "||" );

    display.display();
}
