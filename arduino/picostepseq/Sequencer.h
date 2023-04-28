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
    RUN,
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
        //uint32_t now = millis();
        //uint16_t delta_t = now - last_step_millis;
        uint32_t nowm = micros();
        uint16_t delta_t = nowm - last_tick_micros;
        if( delta_t < tick_micros ) {
            return;
        }
        last_tick_micros = nowm;
        //int16_t ddt = delta_t - tick_micros; // typically ranges 0-8, we are good

        // handle communication from UI core to sequencer/MIDI core
        // translates 'playstate_change' to 'playing' and sending clocks
        if( playstate_change == START ) {
            playstate_change = NONE;
            //playing = true;
            if(send_clock) { clk_func( START, 0 ); }
        }
        else if( playstate_change == STOP ) {
            playstate_change = NONE;
            //playing = false;
            if(send_clock) { clk_func( STOP, 0); }
        }

        if( send_clock && playing ) {
            clk_func( RUN, 0 );
        }

        ticki = (ticki + 1) % ticks_per_step;  // increment our ticks in a step counter: 0,1,2,3,4,5,0,1,2,3,4,5

        if( ticki != 1 ) { // this should 'ticki!=0', but we just incremented
            return; // not step time yet
        }

        uint32_t now = millis();

        // if we have a held note and it's time to turn it off, turn it off
        if( held_gate_millis != 0 && now >= held_gate_millis ) {
            held_gate_millis = 0;
            off_func( held_note.note, held_note.vel, held_note.gate, held_note.on);
        }

        //if( delta_t >= step_millis ) {
        if( ext_clock ) {
            // do nothing, let midi clock trigger notes, but fall back to
            // internal clock if not externally clocked for a while
            if( delta_t > tick_micros * 16 * 1000 ) { // FIXME
                ext_clock = false;
                Serial.println("Turning EXT CLOCK off");
            }
        }
        else {
            trigger(now, delta_t);
        }
        //}
    }

    // Trigger next step in sequence (and make externally clocked)
    void trigger_ext(uint32_t now) {
        ext_clock = true;
        trigger(now, 0); // FIXME: was step_millis);
    }

    // Trigger step in sequence, when internally clocked
    //void trigger(uint32_t now, uint16_t delta_t) {
    void trigger(uint32_t now, uint16_t delta_t) {
        if( !playing ) { return; }
        (void)delta_t; // silence unused variable

        Step s = steps[stepi];
        s.note += transpose;

        if( held_gate_millis )  { // just in case we have an old note hanging around
            Serial.println("HELD NOTE");
            off_func( held_note.note, held_note.vel, held_note.gate, held_note.on);
        }

        on_func(s.note, s.vel, s.gate, s.on);
        //uint16_t err_t = delta_t - beat_millis; // may not need this if we run sequencer on rp2040 core1
        //last_step_millis = now; // - err_t - fudge;
        held_note = s;
        uint32_t step_millis = tick_micros / 1000; // convert micros to millis
        held_gate_millis = now + ((s.gate * step_millis) / 16);
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
        //last_step_millis = millis() - step_millis;
        playing = true;
        last_tick_micros = micros() - tick_micros*2; // FIXME: hmmm
        playstate_change = START;
        //if( send_clock ) {
        //    clk_func( START, 0);
        //}
    }

    // signal to sequencer/MIDI core we want to stop/pause playing
    void pause() {
        playing = false;
        playstate_change = STOP;
        //if( send_clock ) {
        //    clk_func( STOP, 0);
        //}
    }

    // signal to sequencer/MIDI core we want to stop playing
    void stop() {
        //playing = false;
        stepi = 0;
        last_tick_micros = 0;
        playstate_change = STOP;
        //if( send_clock ) {
        //    clk_func( STOP, 0);
        //}
    }

};
