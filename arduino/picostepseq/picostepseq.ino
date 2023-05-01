/**
 * picostepseq.ino  -- picostepseq MIDI step sequencer in Arduino
 * 28 Apr 2023 - @todbot / Tod Kurt
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
 * To upload:
 * - Use Arduino IDE 1.8.19
 * - Install arduino-pico Arduino core https://arduino-pico.readthedocs.io/en/latest/install.html
 * - In "Tools", set "Flash Size: 2MB (Sketch: 1MB / FS: 1MB)"
 * - In "Tools", set "Tools / USB Stack: Adafruit TinyUSB"
 * - Program the sketch to the Pico with "Upload"
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
#include "fonts/ter_u12n7b.h"  // CirPy's default displayio font, it seems

#include "Sequencer.h"
#include "saved_sequences_json.h"  // to bootstrap the "saved_sequences.json" file

#define myfont helvnCB6pt7b  // sigh
#define myfont2 ter_u12n7b

typedef struct {
  uint8_t midi_chan;
  uint8_t midi_velocity;
  bool midi_send_clock;
  bool midi_forward_usb;
  bool midi_forward_uart;
} Config;

Config cfg  = {
  .midi_chan = 1,
  .midi_velocity = 80,
  .midi_send_clock = true,
  .midi_forward_usb = true,
  .midi_forward_uart = true,
  .steps_per_beat = 4,
};

const char* save_file = "/saved_sequences.json";
const bool midi_out_debug = false;
const bool midi_in_debug = true;

const int numseqs = 8;

// begin hardware definitions
const int dw = 128;
const int dh = 64;

const int led_pins[] = { 0, 2, 4, 6, 8, 10, 12, 14 };
const int key_pins[] = { 1, 3, 5, 7, 9, 11, 13, 15 };

const int encoderA_pin = 18;
const int encoderB_pin = 19;
const int encoderSW_pin = 22;

const int oled_sda_pin = 20;
const int oled_scl_pin = 21;

const int oled_i2c_addr = 0x3C;

const int midi_tx_pin = 16;
const int midi_rx_pin = 17;
// end hardware definitions

Adafruit_USBD_MIDI usb_midi;                                  // USB MIDI object
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDIusb);  // USB MIDI
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDIuart);      // Serial MIDI


Adafruit_SSD1306 display(dw, dh, &Wire, -1);

Bounce2::Button keys[numsteps];
Bounce2::Button encoder_switch;

RotaryEncoder encoder(encoderB_pin, encoderA_pin, RotaryEncoder::LatchMode::FOUR3);
void checkEncoderPosition() {  encoder.tick();  } // call tick() to check the state.
int encoder_pos_last = 0;

int led_vals[numsteps];
int led_fade = 35;

float tempo = 100;
StepSequencer seqr;
Step sequences[numseqs][numsteps];

uint8_t midiclk_cnt = 0;
uint32_t midiclk_last_micros = 0;

//
// -- MIDI sending & receiving functions
//

// callback used by Sequencer to trigger note on
void send_note_on(uint8_t note, uint8_t vel, uint8_t gate, bool on) {
  if (on) {
    MIDIusb.sendNoteOn(note, vel, cfg.midi_chan);
    MIDIuart.sendNoteOn(note, vel, cfg.midi_chan);
  }
  if (midi_out_debug) { Serial.printf("noteOn:  %d %d %d %d\n", note, vel, gate, on); }
}

// callback used by Sequencer to trigger note off
void send_note_off(uint8_t note, uint8_t vel, uint8_t gate, bool on) {
  if (on) {
    MIDIusb.sendNoteOff(note, vel, cfg.midi_chan);
    MIDIuart.sendNoteOff(note, vel, cfg.midi_chan);
  }
  if (midi_out_debug) { Serial.printf("noteOff: %d %d %d %d\n", note, vel, gate, on); }
}

// callback used by Sequencer to send midi clock when internally triggered
void send_clock_start_stop(clock_type_t type) {
  if (type == START) {
    MIDIusb.sendStart();
    MIDIuart.sendStart();
  } else if (type == STOP) {
    MIDIusb.sendStop();
    MIDIuart.sendStop();
  } else if (type == CLOCK) {
    MIDIusb.sendClock();
    MIDIuart.sendClock();
  }
  if (midi_out_debug) { Serial.printf("clk:%d\n", type); }
}

// void handle_midi_in_songpos(unsigned int beats) {
//     Serial.printf("songpos:%d\n", beats);
//     if( beats == 0 ) {
//         seqr.stepi = 0;
//     }
// }

void handle_midi_in_start() {
  seqr.play();
  midiclk_cnt = 0;
  if (midi_in_debug) { Serial.println("midi in start"); }
}

void handle_midi_in_stop() {
  seqr.stop();

  if (midi_in_debug) { Serial.println("midi in stop"); }
}
void handle_active_sensing() {
  Serial.printf("activesensing\n");
}

// FIXME: midi continue?
void handle_midi_in_clock() {
  seqr.ext_clock = true;
  // once every ticks_per_step, play note (24 ticks per quarter note => 6 ticks per 16th note)
  if (midiclk_cnt % ticks_per_step == 0) {  // ticks_per_step = 6
    uint32_t now_micros = micros();
    seqr.trigger(now_micros, 0);  // FIXME: figure out 2nd arg (was step_millis)

    // once every quarter note (just so we aggregate some time to minimize error)
    if (midiclk_cnt % (ticks_per_step * steps_per_beat) == 0) {
      uint32_t step_micros = (now_micros - midiclk_last_micros) / (steps_per_beat * ticks_per_step);
      midiclk_last_micros = now_micros;
      seqr.tick_micros = step_micros;
      midiclk_cnt = 0;
      Serial.printf("%ld %.2f\n", step_micros, step_micros/1000.0);
    }
  }
  midiclk_cnt++;
}

//
void midi_read_and_forward() {
  if (MIDIusb.read()) {
    midi::MidiType t = MIDIusb.getType();
    switch (t) {
      case midi::Start:
        handle_midi_in_start();  break;
      case midi::Stop:
        handle_midi_in_stop();  break;
      case midi::Clock:
        handle_midi_in_clock();  break;
      default: break;
    }
    // forward the midi msg to other port
    MIDIuart.send(t, MIDIusb.getData1(), MIDIusb.getData2(), MIDIusb.getChannel());
  }

  if (MIDIuart.read()) {
    midi::MidiType t = MIDIuart.getType();
    switch (t) {
      case midi::Start:
        handle_midi_in_start();  break;
      case midi::Stop:
        handle_midi_in_stop();  break;
      case midi::Clock:
        handle_midi_in_clock();  break;
      default: break;
    }
    MIDIusb.send(t, MIDIuart.getData1(), MIDIuart.getData2(), MIDIuart.getChannel());
  }
}

////////////////////////////

//
// ---  MIDI in/out setup on core0
//
void setup() {
  USBDevice.setManufacturerDescriptor("todbot");
  USBDevice.setProductDescriptor("PicoStepSeq");

  Serial1.setRX(midi_rx_pin);
  Serial1.setTX(midi_tx_pin);

  MIDIusb.begin();
  MIDIusb.turnThruOff();   // turn off echo
  MIDIuart.begin(MIDI_CHANNEL_OMNI); // don't forget OMNI
  MIDIuart.turnThruOff();  // turn off echo

  Serial.begin(115200);
}

//
// --- MIDI in/out on core0
//
void loop() {
  midi_read_and_forward();
  seqr.update();  // will call send_note_{on,off} callbacks
  yield();
}

//
// --- UI setup on core1 (buttons, knobs, display, LEDs)
//
void setup1() {
  // delay(5000);  // for debugging

  LittleFS.begin();

  sequences_read();
  sequence_load(seqr.seqno);  // 0

  seqr.set_tempo(tempo);
  seqr.velocity = cfg.midi_velocity;
  seqr.on_func = send_note_on;
  seqr.off_func = send_note_off;
  seqr.clk_func = send_clock_start_stop;
  seqr.send_clock = cfg.midi_send_clock;

  // KEYS
  for (uint8_t i = 0; i < numsteps; i++) {
    keys[i].attach(key_pins[i], INPUT_PULLUP);
    keys[i].setPressedState(LOW);
  }

  // LEDS
  for (uint8_t i = 0; i < numsteps; i++) {
    pinMode(led_pins[i], OUTPUT);
    analogWrite(led_pins[i], i * 10);  // just to test
  }

  // ENCODER
  pinMode(encoderA_pin, INPUT_PULLUP);
  pinMode(encoderB_pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(encoderA_pin), checkEncoderPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderB_pin), checkEncoderPosition, CHANGE);
  encoder_switch.attach(encoderSW_pin, INPUT_PULLUP);
  encoder_switch.setPressedState(LOW);

  // DISPLAY
  Wire.setSDA(oled_sda_pin);
  Wire.setSCL(oled_scl_pin);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, oled_i2c_addr)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;) ;  // Don't proceed, loop forever
  }

  displaySplash();
  displayConfig();
  delay(1000);
}

// variables for UI state management
int encoder_delta = 0;
uint32_t encoder_push_millis;
uint32_t step_push_millis;
//bool steps_pushed[numsteps]; // which keys are pressed
int step_push = -1;
bool step_edited = false;
char seq_info[11];  // 10 chars + nul FIXME
bool display_mode = 0;

/**
 * Let's talk about UI input "verbs"
 *
 * encoder tap                -- play/stop
 * encoder pushhold           -- modifier for combo
 * encoder turn               -- transpose
 * encoder pushhold + turn    -- bpm
 *
 * key tap                    -- mute/unmute or play step
 * key hold                   -- modifier for combo ()
 *
 * key hold + encoder turn
 * key hold + encoder tap  (unused)
 * key hold + encoder pushhold + encoder turn
 *
 * encoder hold + key tap
 * encoder hold + key hold
 * encoder hold + key hold + encoder turn (currently unused)
 *
 */

//
// --- UI handling on core1
//
void loop1() {
  // LEDS update
  for (int i = 0; i < numsteps; i++) {
    Step s = seqr.steps[i];
    int v = 0;                         // UI: off = muted
    if (i == seqr.stepi) { v = 255; }  // UI: bright red = indicates sequence postion
    else if (s.on) {
      v = 40;
    }                                    // UI: dim red = indicates mute/unmute state
    v = max(led_vals[i] - led_fade, v);  // nice 80s fade
    led_vals[i] = v;
    analogWrite(led_pins[i], v);
  }

  // ENCODER update
  encoder_switch.update();
  encoder.tick();
  int encoder_pos = encoder.getPosition();
  if (encoder_pos != encoder_pos_last) {
    encoder_delta = encoder_pos - encoder_pos_last;
    encoder_pos_last = encoder_pos;
  }

  uint32_t now = millis();

  if (encoder_push_millis > 0 ) {
    if (!step_push_millis && (now-encoder_push_millis) > 1000 ) { // encoder push only > 1sec
      display_mode =  (display_mode + 1) % 2; // FIXME: flickers between screens
    }
    if (step_push_millis > 0) {
      if (encoder_push_millis < step_push_millis) {  // encoder pushed first
        strcpy(seq_info, "saveseq");
      }
    }
  }

  // on encoder turn
  if (encoder_delta) {
    // UI: encoder turn + push while step key held = change step's gate
    if (step_push > -1 && encoder_push_millis > 0) {
      //key_with_turn_push(encoder_delta);
      int gate = seqr.steps[step_push].gate;
      gate = constrain(gate + encoder_delta, 1, 15);
      seqr.steps[step_push].gate = gate;
      step_edited = true;
    }
    // UI: encoder turn while step key held = change step's note
    else if (step_push > -1) {
      Step s = seqr.steps[step_push];
      if (!seqr.playing) {  // step preview note off
        send_note_off(s.note, s.vel, s.gate, true);
      }
      s.note = constrain(s.note + encoder_delta, 1, 127);
      if (!seqr.playing) {  // step preview note on
        send_note_on(s.note, s.vel, s.gate, true);
      }
      seqr.steps[step_push] = s;
      step_edited = true;
    }
    // UI: encoder turn while encoder pushed = change tempo
    else if (encoder_push_millis > 0) {
      tempo = tempo + encoder_delta;
      seqr.set_tempo(tempo);
      Serial.printf("TEMPO: %d %f\n", (int)seqr.tempo(), seqr.tempo());
    }
    // UI: encoder turn = change transpose
    else {
      seqr.transpose = constrain(seqr.transpose + encoder_delta, -36, 36);
    }
    encoder_delta = 0;  // we own turning, and we've used it
  }

  // on encoder push
  if (encoder_switch.pressed()) {  // push encoder button
    encoder_push_millis = now;     // save when (and that) it was pushed
  }

  if (encoder_switch.released()) {                // release encoder button
    if (step_push == -1 && encoder_delta == 0) {  // step key is not pressed and no encoder turn
      // UI: encoder tap with no key == play/pause
      if (now - encoder_push_millis < 300) {
        seqr.toggle_play_pause();
        delay(10);  // wait a bit for sequencer to change state
        if (!seqr.playing) {
          sequences_write();  // write to disk on pause
        }
      }
    }
    encoder_push_millis = 0;  // we own it, and we're done with it
  }


  // KEYS update
  // held check
  // for(uint8_t i=0; i< numsteps; i++) {
  //   if( keys[i].isPressed() && step_push_millis > 1000 && encoder_push_millis > 0 ) {
  //     strcpy(seq_info, "saveseq");
  //   }
  // }

  // event check
  for (uint8_t i = 0; i < numsteps; i++) {
    keys[i].update();
    Step s = seqr.steps[i];

    if (keys[i].pressed()) {  // press event
      step_push = i;
      step_push_millis = now;

      if (encoder_push_millis > 0) { // not pushing encoder
        // do nothing
      } else {
        if (!seqr.playing) {
          send_note_on(s.note, s.vel, s.gate, true);
        }
      }
    }

    else if (keys[i].released()) {    // release event
      if (encoder_push_millis > 0) {  // UI: load/save sequence mode
        // UI: encoder push + hold step key = save sequence
        if (now - step_push_millis > 1000) {
          sequence_save(step_push);
          strcpy(seq_info, "");
        }
        // UI: encoder push + tap step key = load sequence
        else {
          sequence_load(step_push);
        }
      } else {  // UI: encoder not pushed, mutes or play notes
        // UI: playing: step keys = mutes/unmutes
        if (seqr.playing) {
          if (step_edited) {
          } else {
            // UI: if playing, step keys == mute/unmute toggle
            seqr.steps[i].on = !seqr.steps[i].on;
          }
        }
        // UI: paused: step keys = play step notes
        else {
          send_note_off(s.note, s.vel, s.gate, true);
        }
      }
      step_push_millis = 0;
      step_push = -1;  //
      step_edited = false;
    }
  }

  // DISPLAY update
  if (display_mode == 1) {
    displayConfig();
  }
  else {
    displayUpdate(step_push);
  }

}


//
// --- sequence load / save functions
//

uint32_t last_sequence_write_millis = 0;

// write all sequences to "disk"
void sequences_write() {
  Serial.println("sequences_write");
  // save wear & tear on flash, only allow writes every 10 seconds
  if (millis() - last_sequence_write_millis < 10 * 1000) {  // only allow writes every 10 secs
    Serial.println("sequences_write: too soon, wait a bit more");
  }
  last_sequence_write_millis = millis();

  DynamicJsonDocument doc(8192);  // assistant said 6144
  for (int j = 0; j < numseqs; j++) {
    JsonArray seq_array = doc.createNestedArray();
    for (int i = 0; i < numsteps; i++) {
      Step s = sequences[j][i];
      JsonArray step_array = seq_array.createNestedArray();
      step_array.add(s.note);
      step_array.add(s.vel);
      step_array.add(s.gate);
      step_array.add(s.on);
    }
  }

  LittleFS.remove(save_file);
  File file = LittleFS.open(save_file, "w");
  if (!file) {
    Serial.println("sequences_write: Failed to create file");
    return;
  }
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("sequences_write: Failed to write to file"));
  }
  file.close();
  Serial.print("saved_sequences_json = \"");
  serializeJson(doc, Serial);
  Serial.println("\"\nsequences saved");
}

// read all sequences from "disk"
void sequences_read() {
  Serial.println("sequences_read");
  DynamicJsonDocument doc(8192);  // assistant said 6144

  File file = LittleFS.open(save_file, "r");
  if (!file) {
    Serial.println("sequences_read: no sequences file. Using ROM default...");
    DeserializationError error = deserializeJson(doc, default_saved_sequences_json);
    if (error) {
      Serial.print("sequences_read: deserialize default failed: ");
      Serial.println(error.c_str());
      return;
    }
  } else {
    DeserializationError error = deserializeJson(doc, file);  // inputLength);
    if (error) {
      Serial.print("sequences_read: deserialize failed: ");
      Serial.println(error.c_str());
      return;
    }
  }

  for (int j = 0; j < numseqs; j++) {
    JsonArray seq_array = doc[j];
    for (int i = 0; i < numsteps; i++) {
      JsonArray step_array = seq_array[i];
      Step s;
      s.note = step_array[0];
      s.vel = step_array[1];
      s.gate = step_array[2];
      s.on = step_array[3];
      sequences[j][i] = s;
    }
  }
  file.close();
}

// Load a single sequence from into the sequencer from RAM storage
void sequence_load(int seq_num) {
  Serial.printf("sequence_load:%d\n", seq_num);
  for (int i = 0; i < numsteps; i++) {
    seqr.steps[i] = sequences[seq_num][i];
  }
  seqr.seqno = seq_num;
}

// Store current sequence in sequencer to RAM storage"""
void sequence_save(int seq_num) {
  Serial.printf("sequence_save:%d\n", seq_num);
  for (int i = 0; i < numsteps; i++) {
    sequences[seq_num][i] = seqr.steps[i];
    ;
  }
}

//
// --- display details
//
typedef struct { int x; int y; int vx; int vy; const char* str;} pos_t;

//// {x,y} locations of play screen items
const int step_text_pos[] = { 0, 15, 16, 15, 32, 15, 48, 15, 64, 15, 80, 15, 96, 15, 112, 15 };
const pos_t bpm_text_pos    = {.x=0,  .y=57, .vx=0, .vy=0, .str="bpm:%3d" };
const pos_t trans_text_pos  = {.x=55, .y=57, .vx=0, .vy=0, .str="trs:%+2d" };
const pos_t seqno_text_pos  = {.x=0,  .y=45, .vx=0, .vy=0, .str="seq:%d" };
const pos_t seq_info_pos    = {.x=60, .y=45 };
const pos_t play_text_pos   = {.x=110,.y=57 };

const pos_t oct_text_offset = { .x=3, .y=10 };
const pos_t gate_bar_offset = { .x=0, .y=-15 };
const pos_t edit_text_offset= { .x=3, .y=22 };
const int gate_bar_width = 14;
const int gate_bar_height = 4;

// {x,y} locations of config screen
const pos_t midichan_text_pos  = {.x=0, .y=30, .vx=80, .vy = 30, "midi ch:" };
const pos_t midiclock_text_pos = {.x=0, .y=50, .vx=80, .vy = 50, "midi clk:" };

const char* note_strs[] = { "C ", "C#", "D ", "D# ", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B ", "C " };

int notenum_to_oct(int notenum) {
  return (notenum / 12) - 2;
}
const char* notenum_to_notestr(int notenum) {
  return note_strs[notenum % 12];
}

void displayUpdate(int selected_step) {
  display.clearDisplay();
  display.setFont(&myfont);
  display.setTextColor(WHITE, 0);
  for (int i = 0; i < numsteps; i++) {
    Step s = seqr.steps[i];
    const char* nstr = notenum_to_notestr(s.note);
    int o = notenum_to_oct(s.note);
    int x = step_text_pos[i * 2], y = step_text_pos[i * 2 + 1];
    display.setCursor(x, y);
    display.print(nstr);
    display.setCursor(x + oct_text_offset.x, y + oct_text_offset.y);
    display.printf("%1d", o);
    display.setCursor(x + edit_text_offset.x, y + edit_text_offset.y);
    display.print((i == selected_step) ? '^' : (s.on) ? ' '
                                                      : '*');
    int gate_w = 1 + (s.gate * gate_bar_width / 16);
    display.fillRect(x + gate_bar_offset.x, y + gate_bar_offset.y, gate_w, gate_bar_height, WHITE);
  }

  display.setFont(&myfont2);

  // bpm
  display.setCursor(bpm_text_pos.x, bpm_text_pos.y);
  display.printf(bpm_text_pos.str, (int)(seqr.tempo()));

  // transpose
  display.setCursor(trans_text_pos.x, trans_text_pos.y);
  display.printf(trans_text_pos.str, seqr.transpose);

  // seqno
  display.setCursor(seqno_text_pos.x, seqno_text_pos.y);
  display.printf(seqno_text_pos.str, seqr.seqno + 1);  // user sees 1-8

  // seq info / meta
  display.setCursor(seq_info_pos.x, seq_info_pos.y);
  display.print(seq_info);

  // play/pause
  display.setCursor(play_text_pos.x, play_text_pos.y);
  display.print(seqr.playing ? " >" : "[]");

  display.display();
}

void displayConfig() {
  display.clearDisplay();
  display.setFont(&myfont2);
  display.setTextColor(WHITE, 0);

  display.setCursor( midichan_text_pos.x, midichan_text_pos.y);
  display.print(midichan_text_pos.str);
  display.setCursor( midichan_text_pos.vx, midichan_text_pos.vy);
  display.printf("%d", cfg.midi_chan);

  display.setCursor( midiclock_text_pos.x, midiclock_text_pos.y);
  display.print(midiclock_text_pos.str);
  display.setCursor( midiclock_text_pos.vx, midiclock_text_pos.vy);
  display.printf("%s", cfg.midi_send_clock ? "on":"off");

  display.display();
}

void displaySplash() {
  display.clearDisplay();
  display.setFont(&myfont2);
  display.setTextColor(WHITE, 0);
  display.drawRect(0,0, dw-1, dh-1, WHITE);
  display.setCursor(30, 32);
  display.print("PICOSTEPSEQ");
  display.display();
  // a little LED dance
  for( int i=0; i<1000; i++) {
    for( int j=0; j<numsteps; j++) {
      int v = 30 + 30 * sin( (j*6.2 / 8 ) + i/50.0 ) ;
      analogWrite( led_pins[j], v);
    }
    delay(1);
  }
}
