/**
 * Sequencer.hpp -- Sequencer for picostepseq
 * 15 Aug 2022 - @todbot / Tod Kurt
 * Part of https://github.com/todbot/picostepseq/
 */

extern const int numsteps;
// how many steps this sequencer has
//const int step_count = 8;  // FIXME


typedef struct {
    uint8_t note;
    uint8_t vel;
    uint8_t gate;
    bool on;
} Step;

typedef void (*TriggerFunc)(uint8_t note, uint8_t vel, uint8_t gate, bool on);


class StepSequencer {
public:

    uint32_t beat_millis;
    uint32_t last_beat_millis;
    uint32_t held_gate_millis;
    int transpose;
    uint8_t steps_per_beat;
    uint8_t stepi;
    int seqno;
    bool playing;
    TriggerFunc on_func;
    TriggerFunc off_func;
    Step held_note;
    Step steps[numsteps];

    StepSequencer( float atempo=120, uint8_t aseqno=0 ) {
        steps_per_beat = 4;  // 16th note
        transpose = 0;
        last_beat_millis = 0;
        stepi = 0;
        seqno = aseqno;
        playing = false;
        set_tempo(atempo);
    }

    float tempo() {
        return 60 * 1000  / beat_millis / steps_per_beat;
    }

    void set_tempo(float tempo) {
        beat_millis = 60 * 1000 / steps_per_beat / tempo;
    }

    void trigger(uint32_t now, uint16_t delta_t) {
        if( !playing ) { return; }
        (void)delta_t; // silence unused variable
        stepi = (stepi + 1) % numsteps;
        Step s = steps[stepi];
        s.note += transpose;

        if( held_gate_millis )  { // just in case we have an old note hanging around
            Serial.println("HELD NOTE");
            off_func( held_note.note, held_note.vel, held_note.gate, held_note.on);
        }

        on_func(s.note, s.vel, s.gate, s.on);
        //uint16_t err_t = delta_t - beat_millis; // may not need this if we run sequencer on rp2040 core1
        last_beat_millis = now; // - err_t - fudge;
        held_note = s;
        held_gate_millis = now + ((s.gate * beat_millis) / 16);
    }

    void update() {

        uint32_t now = millis();
        uint16_t delta_t = now - last_beat_millis;

        if( held_gate_millis != 0 && now >= held_gate_millis ) {
            held_gate_millis = 0;
            off_func( held_note.note, held_note.vel, held_note.gate, held_note.on);
        }

        if( delta_t >= beat_millis ) {
            trigger(now, delta_t);
        }

    }

    void toggle_play_pause() {
        if( playing ) { pause(); }
        else { play(); }
    }

    void play() {
        playing = true;
    }

    void pause() {
        playing = false;
    }

    void stop() {
        playing = false;
        stepi = 0;
        last_beat_millis = 0;
    }

};
