/**
 * Sequencer.hpp -- Sequencer for picostepseq
 * 15 Aug 2022 - @todbot / Tod Kurt
 * Part of https://github.com/todbot/picostepseq/
 */


const int numsteps = 8;
const int steps_per_beat = 4; // 16th note, beats are quarter notes, 120bpm = 120 quarter notes per min
const int ticks_per_step = 6; // 24 ppq = 4 steps_per_beat * 6 ticks_per_step

typedef struct {
    uint8_t note;
    uint8_t vel;
    uint8_t gate;
    bool on;
} Step;

typedef enum {
    NONE,
    START,
    STOP,
    CLOCK,
} clock_type_t;

typedef void (*TriggerFunc)(uint8_t note, uint8_t vel, uint8_t gate, bool on);
typedef void (*ClockFunc)(clock_type_t type, int pos);

class StepSequencer {
public:

    uint32_t tick_micros; // "micros_per_tick", microsecs per clock (6 clocks / step; 4 steps / quarternote)
    uint32_t last_tick_micros;
    uint32_t held_gate_millis;
    uint8_t ticki;
    uint8_t stepi;
    int seqno;
    int transpose;
    bool playing;
    clock_type_t playstate_change;
    bool ext_clock;
    bool send_clock;
    TriggerFunc on_func;
    TriggerFunc off_func;
    ClockFunc clk_func;
    Step held_note;
    Step steps[numsteps];

    StepSequencer( float atempo=120, uint8_t aseqno=0 ) {
        transpose = 0;
        last_tick_micros = 0;
        stepi = 0;
        ticki = 0;
        seqno = aseqno;
        playing = false;
        ext_clock = false;
        send_clock = false;
        set_tempo(atempo);
    }

    // get tempo as floating point, computed dynamically from steps_millis
    float tempo() {
        //return 60 * 1000 / step_millis / steps_per_beat ;
        return 60 * 1000 * 1000 / tick_micros / (steps_per_beat * ticks_per_step);
    }

    // set tempo as floating point, computes steps_millis
    void set_tempo(float bpm) {
        //step_millis = (60 * 1000) / steps_per_beat / bpm;
        tick_micros = 60 * 1000 * 1000 / bpm / (steps_per_beat*ticks_per_step) ; // 24 ppq
    }

    void update() {
        uint32_t now_micros = micros();
        uint16_t delta_t = now_micros - last_tick_micros;
        if( delta_t < tick_micros ) { return; }  // not yet
        last_tick_micros = now_micros;
        //int16_t ddt = delta_t - tick_micros; // typically ranges 0-8, we are good

        // if we have a held note and it's time to turn it off, turn it off
        if( held_gate_millis != 0 && millis() >= held_gate_millis ) {
            held_gate_millis = 0;
            off_func( held_note.note, held_note.vel, held_note.gate, held_note.on);
        }

        // this is kind of a hack, maybe use rp2040 fifo?
        // handle communication from UI core to sequencer/MIDI core
        // translates 'playstate_change' to 'playing' and sending clocks
        if( playstate_change == START ) {
            playstate_change = NONE;
            if(send_clock) { clk_func( START, 0 ); }
        }
        else if( playstate_change == STOP ) {
            playstate_change = NONE;
            if(send_clock) { clk_func( STOP, 0); }
        }

        if( ticki == 0 ) {
            // do a sequence step (i.e. every "ticks_per_step" ticks)
            if( ext_clock ) {
                // do nothing, let midi clock trigger notes, but fall back to
                // internal clock if not externally clocked for a while
                if( delta_t > tick_micros * 1000 * 16 ) { // FIXME
                    ext_clock = false;
                    Serial.println("Turning EXT CLOCK off");
                }
            }
            else {  // else internally clocked
                trigger(now_micros, delta_t);
            }
        }

        if( send_clock && playing ) { // FIXME: && !ext_clock ?
            clk_func( CLOCK, 0 );
        }

        // increment our ticks-per-step counter: 0,1,2,3,4,5, 0,1,2,3,4,5, ...
        ticki = (ticki + 1) % ticks_per_step;
    }

    // Trigger next step in sequence (and make externally clocked)
    void trigger_ext(uint32_t now_micros) {
        ext_clock = true;
        trigger(now_micros, 0); // FIXME: "0" was step_millis);
    }

    // Trigger step in sequence, when internally clocked
    //void trigger(uint32_t now, uint16_t delta_t) {
    void trigger(uint32_t now_micros, uint16_t delta_t) {
        if( !playing ) { return; }
        (void)delta_t; // silence unused variable

        Step s = steps[stepi];
        s.note += transpose;

        // if( held_gate_millis )  { // just in case we have an old note hanging around
        //    Serial.println("\nHELD NOTE\n");
        //    off_func( held_note.note, held_note.vel, held_note.gate, held_note.on);
        //}

        on_func(s.note, s.vel, s.gate, s.on);

        held_note = s;
        uint32_t step_micros = ticks_per_step * tick_micros;
        uint32_t gate_micros = s.gate * step_micros / 16;
        held_gate_millis = (now_micros + gate_micros) / 1000;

        stepi = (stepi + 1) % numsteps;
    }

    void toggle_play_pause() {
        if( playing ) { pause(); }
        else { play(); }
    }

    // signal to sequencer/MIDI core we want to start playing
    void play() {
        stepi = 0;
        ticki = 0;
        playstate_change = START;
        last_tick_micros = micros() - tick_micros*2; // FIXME: hmmm
        playing = true;
        //if( send_clock ) {
        //    clk_func( START, 0);
        //}
    }

    // signal to sequencer/MIDI core we want to stop/pause playing
    void pause() {
        playstate_change = STOP;
        playing = false;
        //if( send_clock ) {
        //    clk_func( STOP, 0);
        //}
    }

    // signal to sequencer/MIDI core we want to stop playing
    void stop() {
        stepi = 0;
        last_tick_micros = 0;
        playing = false;
        playstate_change = STOP;
        //if( send_clock ) {
        //    clk_func( STOP, 0);
        //}
    }

};
