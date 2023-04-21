/**
 * Sequencer.hpp -- Sequencer for picostepseq
 * 15 Aug 2022 - @todbot / Tod Kurt
 * Part of https://github.com/todbot/picostepseq/
 */

const int numsteps = 8;

typedef struct {
    uint8_t note;
    uint8_t vel;
    uint8_t gate;
    bool on;
} Step;

typedef void (*TriggerFunc)(uint8_t note, uint8_t vel, uint8_t gate, bool on);


class StepSequencer {
public:

    uint32_t step_millis; // number of millis per step
    uint32_t last_step_millis;
    uint32_t held_gate_millis;
    int transpose;
    uint8_t steps_per_beat;
    uint8_t stepi;
    int seqno;
    bool playing;
    bool ext_trigger;
    TriggerFunc on_func;
    TriggerFunc off_func;
    Step held_note;
    Step steps[numsteps];

    StepSequencer( float atempo=120, uint8_t aseqno=0 ) {
        steps_per_beat = 4;  // 16th note, beats are quarter notes, 120bpm = 120 quarter notes per min
        transpose = 0;
        last_step_millis = 0;
        stepi = 0;
        seqno = aseqno;
        playing = false;
        ext_trigger = false;
        set_tempo(atempo);
    }

    // get tempo as floating point, computed dynamically from steps_millis
    float tempo() {
        return 60 * 1000  / step_millis / steps_per_beat;
    }

    // set tempo as floating point, computes steps_millis
    void set_tempo(float tempo) {
        step_millis = 60 * 1000 / steps_per_beat / tempo;
    }

    // Trigger next step in sequence (and make externally triggered)
    void trigger_ext(uint32_t now) {
        ext_trigger = true;
        trigger(now, step_millis);
    }

    // Trigger step in sequence, when internally triggered
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
        last_step_millis = now; // - err_t - fudge;
        held_note = s;
        held_gate_millis = now + ((s.gate * step_millis) / 16);
        stepi = (stepi + 1) % numsteps;
    }

    void update() {

        uint32_t now = millis();
        uint16_t delta_t = now - last_step_millis;

        if( held_gate_millis != 0 && now >= held_gate_millis ) {
            held_gate_millis = 0;
            off_func( held_note.note, held_note.vel, held_note.gate, held_note.on);
        }

        if( delta_t >= step_millis ) {
            if( ext_trigger ) {
                // do nothing, let midi clock trigger notes, but fall bakc to
                // internal triggering if not externally clocked for a while
                if( delta_t > step_millis * 16 ) {
                    ext_trigger = false;
                    Serial.println("Turning EXT TRIGGER off");
                }
            }
            else {
                trigger(now, delta_t);
            }
        }
    }

    void toggle_play_pause() {
        if( playing ) { pause(); }
        else { play(); }
    }

    void play() {
        stepi=0;
        last_step_millis = millis() - step_millis;
        playing = true;
    }

    void pause() {
        playing = false;
    }

    void stop() {
        playing = false;
        stepi = 0;
        last_step_millis = 0;
    }

};
