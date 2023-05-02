/**
 * sequencer.h -- Sequencer for picostepseq
 * 28 Apr 2023 - @todbot / Tod Kurt
 * 15 Aug 2022 - @todbot / Tod Kurt
 * Part of https://github.com/todbot/picostepseq/
 */


const int numsteps = 8;
const int ticks_per_quarternote = 24; // 24 ppq per midi std
const int steps_per_beat_default = 4;

typedef struct {
    uint8_t note; // midi note
    uint8_t vel;  // midi velocity (always 127 currently)
    uint8_t gate; // how long note should be on in 0-15 arbitrary units
    bool on;      // does this note play or is or muted
} Step;

typedef enum {
    NONE,
    START,
    STOP,
    CLOCK,
} clock_type_t;

typedef enum {
  QUARTER_NOTE = 24,
  EIGHTH_NOTE = 12,
  SIXTEENTH_NOTE = 6,
} valid_ticks_per_step;

typedef void (*TriggerFunc)(uint8_t note, uint8_t vel, uint8_t gate, bool on);
typedef void (*ClockFunc)(clock_type_t type); // , int pos);

void fake_note_callback(uint8_t note, uint8_t vel, uint8_t gate, bool on) {  }
void fake_clock_callback(clock_type_t type) { }

class StepSequencer {
public:

    uint32_t tick_micros; // "micros_per_tick", microsecs per clock (6 clocks / step; 4 steps / quarternote)
    uint32_t last_tick_micros; // only change in update()
    uint32_t held_gate_millis;
    int ticks_per_step;  // expected values: 6 = 1/16th, 12 = 1/8, 24 = 1/4,
    int ticki; // which midi clock we're on
    int stepi; // which sequencer step we're on
    int seqno;
    int transpose;
    bool playing;
  //bool ext_clock;
    bool send_clock;
    uint32_t extclk_micros; // 0 = internal clock, non-zero = external clock
    TriggerFunc on_func;
    TriggerFunc off_func;
    ClockFunc clk_func;
    Step held_note;
    Step steps[numsteps];
    uint8_t velocity;   // if set, velocity to use instead of saved

    StepSequencer( float atempo=120, uint8_t aseqno=0 ) {
        transpose = 0;
        last_tick_micros = 0;
        stepi = 0;
        ticki = 0;
        ticks_per_step = 6; // 6 = 1/16th, 12 = 1/8, 24 = 1/4,
        seqno = aseqno;
        playing = false;
        //ext_clock = false;
        extclk_micros = 0;
        send_clock = false;
        velocity = 0;
        set_tempo(atempo);
        on_func = fake_note_callback;
        off_func = fake_note_callback;
        clk_func = fake_clock_callback;
    }

    // get tempo as floating point, computed dynamically from ticks_micros
    float tempo() {
        return 60 * 1000 * 1000 / tick_micros / ticks_per_quarternote; //steps_per_beat * ticks_per_step);
    }

    // set tempo as floating point, computes ticks_micros
    void set_tempo(float bpm) {
        tick_micros = 60 * 1000 * 1000 / bpm / ticks_per_quarternote;
    }

    void update() {
        uint32_t now_micros = micros();
        if( (now_micros - last_tick_micros) < tick_micros ) { return; }  // not yet
        last_tick_micros = now_micros;

        // // serious debug cruft here
        // Serial.printf("up:%2d %d  %6ld %6ld  dt:%6ld t:%6ld last:%6ld\n",
        //               ticki, playing,  held_gate_millis, millis(),
        //               delta_t, tick_micros, last_tick_micros );
        //Serial.printf("up: %ld \t h:%2x/%2x/%1x/%d\n", held_gate_millis,
        //              held_note.note, held_note.vel, held_note.gate, held_note.on);

        // if we have a held note and it's time to turn it off, turn it off
        if( held_gate_millis != 0 && millis() >= held_gate_millis ) {
            held_gate_millis = 0;
            off_func( held_note.note, held_note.vel, held_note.gate, held_note.on);
        }

        if( send_clock && playing && !extclk_micros ) {
            clk_func( CLOCK );
        }

        if( ticki == 0 ) {
            // do a sequence step (i.e. every "ticks_per_step" ticks)
            if( extclk_micros ) {
                // do nothing, let midi clock trigger notes, but fall back to
                // internal clock if not externally clocked for a while
                if( (now_micros - extclk_micros) > tick_micros * ticks_per_quarternote ) {
                    extclk_micros = 0;
                    Serial.println("Turning EXT CLOCK off");
                }
            }
            else {  // else internally clocked
              trigger(now_micros, 0); // delta_t);
            }
        }

        // increment our ticks-per-step counter: 0,1,2,3,4,5, 0,1,2,3,4,5, ...
        ticki = (ticki + 1) % ticks_per_step;
    }

    // Trigger step when externally clocked (turns on external clock flag)
    void trigger_ext(uint32_t now_micros) {
        extclk_micros = now_micros;
        trigger(now_micros,0);
    }

    // Trigger step in sequence, when internally clocked
    //void trigger(uint32_t now, uint16_t delta_t) {
    void trigger(uint32_t now_micros, uint16_t delta_t) {
        if( !playing ) { return; }
        (void)delta_t; // silence unused variable

        stepi = (stepi + 1) % numsteps; // go to next step

        Step s = steps[stepi];
        s.note += transpose;
        s.vel = (velocity) ? velocity : s.vel;

        on_func(s.note, s.vel, s.gate, s.on);

        held_note = s;
        uint32_t micros_per_step = ticks_per_step * tick_micros;
        uint32_t gate_micros = s.gate * micros_per_step / 16;  // s.gate is arbitary 0-15 value
        held_gate_millis = (now_micros + gate_micros) / 1000;
        Serial.printf("trigger: held_gate_milils: %6ld\n", held_gate_millis);
    }

    void toggle_play_stop() {
        if( playing ) { stop(); }
        else { play(); }
    }

    // signal to sequencer/MIDI core we want to start playing
    void play() {
        stepi = -1; // hmm, but goes to 0 on first downbeat
        ticki = 0;
        playing = true;
        if(send_clock && !extclk_micros) {
          clk_func( START );
        }
    }

    // signal to sequencer/MIDI core we want to stop/pause playing
    void pause() {
        playing = false;
    }

    // signal to sequencer/MIDI core we want to stop playing
    void stop() {
        playing = false;
        stepi = -1;
        if(send_clock && !extclk_micros) {
          clk_func( STOP );
        }
    }

};
