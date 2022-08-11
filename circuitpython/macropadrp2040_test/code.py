# macropadrp2040_step_sequencer_test.py --
# 6 Aug 2022 - @todbot / Tod Kurt
# Part of picostepseq: https://github.com/todbot/picostepseq/
#
# User interface:
# "Encocder-first" actions:
# - Tap encoder to toggle play / pause
# - Turn encoder to change tranpose
# - Push + turn encoder to change transpose
# "Step key-first" actions:
# - Tap step button to enable/disable from sequence
# - Hold step button + turn encoder to change note
# - Hold step button + push encoder + turn encoder to change gate length
#


# built in libraries
import board
import keypad
import gc
import json

# installed via circup
import adafruit_midi
import adafruit_macropad

# local libraries in CIRCUITPY
from sequencer import StepSequencer, ticks_ms, ticks_diff
from sequencer_display_macropad import StepSequencerDisplay

playdebug = False

base_note = 60  #  60 = C4, 48 = C3
num_steps = 8
tempo = 100
gate_default = 8    # ranges 0-15
sequences = [ [(None)] * num_steps ] * num_steps  # pre-fill arrays for easy use later

macropad = adafruit_macropad.MacroPad()
macropad.pixels.brightness = 0.2
macropad.pixels.auto_write = False
macropad.display.rotation = 90

macropad._encoder_switch.deinit()  # so we can use keypad for debounce, like civilized folk
encoder_switch = keypad.Keys((board.BUTTON,), value_when_pressed=False, pull=True)

# map step position to macropad key number
step_to_key_pos = (1, 4, 7, 10, 0, 3, 6, 9)

# callback for sequencer
def play_note_on(note, vel, gate, on):  #
    if on:
        if playdebug: print("on :%d n:%3d v:%3d %d %d" % (note,vel, gate,on), end="\n" )
        macropad.midi.send( macropad.NoteOn(note, vel), channel=0)

# callback for sequencer
def play_note_off(note, vel, gate, on):  #
    #if on:
    # FIXME: always do note off to since race condition of note muted right after playing
    if playdebug: print("off:%d n:%3d v:%3d %d %d" % (note,vel, gate,on), end="\n" )
    macropad.midi.send( macropad.NoteOff(note, vel), channel=0)

def load_sequence(seq_num):
    new_seq = sequences[seq_num]
    print("new_seq=",new_seq)
    seqr.steps = new_seq

def save_sequence(seq_num):
    sequences[seq_num] = seqr.steps
    print("sequences:",sequences)

def read_sequences():
    global sequences
    with open('/saved_sequences.json', 'r') as fp:
        sequences = json.load(fp)

def write_sequences():
    print("SAVING ALL SEQUENCES")
    with open('/saved_sequences.json', 'w') as fp:
        json.dump(sequences, fp)

seqr = StepSequencer(num_steps, tempo, play_note_on, play_note_off)
seqr.playing = False
read_sequences()

print("sequences:",sequences)
seqr_display = StepSequencerDisplay(seqr)

macropad.display.show( seqr_display )

load_sequence(0)

# init display UI
seqr_display.update_ui_all()

# various state for UI hanlding
last_debug_millis = 0
last_encoder_val = macropad.encoder  # needed to calculate encoder_delta
encoder_push_millis = 0  # when was encoder pushed, 0 == no push
encoder_delta = 0 # how much encoder was turned, 0 == no turn
step_push = -1  # which step button is being pushed, -1 == no push
step_push_millis = 0  # when was a step button pushed (extra? maybe
step_edited = False

while True:
    gc.collect()  # just to make the timing of this consistent

    # update step LEDs
    for i in range(num_steps):
        (n,v,gate,on) = seqr.steps[ i ]
        c = 0x000000 # turn LED off
        if i == seqr.i:  c = 0xff0000  # UI: bright red = indicate sequence position
        elif on:        c = 0x110000  # UI: dim red = indicate mute/unmute state
        macropad.pixels[step_to_key_pos[i]] = c
    macropad.pixels.show()

    seqr.update()

    now = ticks_ms()

    # update encoder turning
    encoder_val = macropad.encoder
    if encoder_val != last_encoder_val:
        encoder_delta = (encoder_val - last_encoder_val)
        last_encoder_val = encoder_val

    # idea: pull out all UI options into state variables
    # encoder_push_and_turn_push = encoder_delta and encoder_push_millis > 0

    # on encoder turn
    if encoder_delta:

        # UI: encoder turned and pushed while step key held == change step's gate
        if step_push > -1 and encoder_push_millis > 0:
            (n,v,gate,on) = seqr.steps[ step_push ]
            gate = min(max(gate + encoder_delta, 1), 15)
            seqr.steps[ step_push ] = (n,v,gate,on)
            step_edited = True
            seqr_display.update_ui_step( step_push, n, v, gate, on, True)
            encoder_delta = 0  # we used up encoder delta

        # UI:  encoder turned while step key held == change step's note
        elif step_push > -1:  # step key pressed
            (n,v,gate,on) = seqr.steps[ step_push ]
            if not seqr.playing:
                play_note_off( n, v, gate, True)

            n = min(max(n + encoder_delta, 1), 127)

            if not seqr.playing:
                play_note_on( n, v, gate, True )

            seqr.steps[ step_push ] = (n,v,gate,on)
            step_edited = True
            seqr_display.update_ui_step( step_push, n, v, gate, on, True)
            encoder_delta = 0  # we used up encoder delta

        # UI: encoder turned while encoder pushed == change tempo
        elif encoder_push_millis > 0:
            tempo = tempo + encoder_delta
            seqr.set_tempo(tempo)
            seqr_display.update_ui_tempo()
            encoder_delta = 0  # we've used up the encoder delta

        # UI: encoder turned without any modifiers == change transpose
        else:
            seqr.transpose = min(max(seqr.transpose + encoder_delta, -36), 36)
            seqr_display.update_ui_transpose()
            encoder_delta = 0  # we used up encoder delta

    # on encoder push
    encsw = encoder_switch.events.get()
    if encsw:
        if encsw.pressed:
            encoder_push_millis = now  # save when we pushed encoder

        if encsw.released:
            if step_push == -1 and encoder_delta == 0:  # step key is not pressed and no turn
                # UI: encoder tap, with no key == play/pause
                if ticks_diff( ticks_ms(), encoder_push_millis) < 300:
                    seqr.toggle_play_pause()
                    seqr_display.update_ui_playing()
                    if not seqr.playing:
                        write_sequences()
                # UI encoder hold with no key == STOP and reset playhead to 0
                # FIXME: broken. doesn't re-start at 0 properly
                # elif ticks_diff( ticks_ms(), encoder_push_millis) > 1000:
                #     seqr.stop()
                #     seqr_display.update_ui_all()
            else:  # step key is pressed
                pass
            encoder_push_millis = 0  # say we are done with encoder, on key release


    # on step key push
    key = macropad.keys.events.get()
    if key:
        try:
            # record which step key is pushed for other UI modifiers
            # .index() throws the ValueError, thus the try/except
            step_push = step_to_key_pos.index(key.key_number) # map key pos back to step num
            (n,v,gate,on) = seqr.steps[step_push]

            if key.pressed:
                print("+ press",key.key_number, step_push)
                step_push_millis = ticks_ms()

                # encoder push + key push = load/save sequence
                if encoder_push_millis > 0:
                    pass
                else:
                    seqr_display.update_ui_step( step_push, n, v, gate, on, True)
                    if seqr.playing:
                        pass
                    # UI: if not playing, step keys == play their pitches
                    else:
                        play_note_on( n, v, gate, True )

            elif key.released:
                print("- release", key.key_number, step_push)

                if encoder_push_millis > 0:   # UI load /save sequence mode
                    # UI: encoder push + hold step key = save sequence
                    if now - step_push_millis > 1000:
                        print("save sequence!", step_push)
                        save_sequence( step_push )
                    # UI: encoder push + tap step key = load sequence
                    else:
                        print("load sequence!", step_push, (now - step_push_millis))
                        load_sequence( step_push )
                        seqr_display.update_ui_steps()
                else:
                    if seqr.playing:
                        if not step_edited:
                            # UI: if playing, step keys == toggles enable (must be on relase)
                            on = not on
                            seqr.steps[step_push] = (n,v,gate, on)
                    else:
                        # UI: if not playing, step key == play their pitches
                        (n,v,gate,on) = seqr.steps[step_push]
                        play_note_off( n, v, gate, True )

                print("release done")
                sp_tmp = step_push  # for update_ui_step() below
                step_push = -1  # say we are done with key
                step_edited = False  # done editing
                seqr_display.update_ui_step( sp_tmp, n, v, gate, on)

        except ValueError:  # undefined macropad key was pressed, ignore
            pass

    #
    #emillis = ticks_ms() - now
    #print("emillis:",emillis)
