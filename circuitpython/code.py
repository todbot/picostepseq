# picostepseq_code.py -- picostepseq MIDI step sequencer
# 6 Aug 2022 - @todbot / Tod Kurt
# Part of picostepseq: https://github.com/todbot/picostepseq/
#
# User interface:
# "Encocder-first" actions:
# - Tap encoder to toggle play / pause
# - Turn encoder to change tranpose
# - Push + turn encoder to change transpose
# - Push encoder + push step key to load sequence 1-8
# - Hold encoder + hold step key > 1 sec to save sequence 1-8
# - Sequences saved to disk on pause
# "Step key-first" actions:
# - Tap step button to enable/disable from sequence
# - Hold step button + turn encoder to change note
# - Hold step button + push encoder + turn encoder to change gate length
#

# built in libraries
import board
import gc
import json
import usb_midi

# local libraries in CIRCUITPY
import winterbloom_smolmidi as smolmidi
from sequencer import StepSequencer, ticks_ms, ticks_diff

if 'macropad' in board.board_id:
    from sequencer_display_macropad import StepSequencerDisplay
    from sequencer_hardware_macropad import Hardware
else:
    import displayio
    displayio.release_displays() # can we put this in sequencer_hardware?
    from sequencer_display import StepSequencerDisplay
    from sequencer_hardware import Hardware

do_usb_midi = True
do_serial_midi = True

playdebug = False

base_note = 60  #  60 = C4, 48 = C3
num_steps = 8
tempo = 100
gate_default = 8    # ranges 0-15

# array of sequences used by Sequencer (which only knows about one sequence)
sequences = [ [(None)] * num_steps ] * num_steps  # pre-fill arrays for easy use later


usb_out = usb_midi.ports[1]
usb_in = usb_midi.ports[0]

usb_midi_in = smolmidi.MidiIn(usb_in)


midiclk_cnt = 0
midiclk_last_millis = 0
def midi_receive():
    """Handle MIDI Clock and Start/Stop"""
    global midiclk_cnt, midiclk_last_millis

    msg = usb_midi_in.receive()

    if not msg: return

    if msg.type == smolmidi.START:
        print("MIDI START")
        seqr.play()
        seqr_display.update_ui_playing()

    elif msg.type == smolmidi.STOP:
        print("MIDI STOP")
        seqr.stop()
        seqr_display.update_ui_playing()

    elif msg.type == smolmidi.CLOCK:
        midiclk_cnt += 1
        if midiclk_cnt % 6 == 0:  # once every 1/16th note (24 pulses per quarter note => 6 pulses per 16th note)
            now = ticks_ms()
            seqr.trigger_next(now)

            #print("!", beat_millis)
            if midiclk_cnt % 24 == 0:  # once every quarter note
                beat_millis = (now - midiclk_last_millis) / 4  # beat_millis is 1/16th note time
                midiclk_last_millis = now
                seqr.beat_millis = beat_millis
                seqr_display.update_ui_bpm()
                seqr_display.update_ui_playing()


def play_note_on(note, vel, gate, on):  #
    """Callback for sequencer when note should be tured on"""
    if not on: return
    if playdebug: print("on :%d n:%3d v:%3d %d %d" % (note,vel, gate,on), end="\n" )
    midi_msg = bytearray([0x90, note, vel])  # FIXME
    if do_usb_midi:
        usb_out.write( midi_msg )
    if do_serial_midi:
        hw.midi_uart.write( midi_msg )

def play_note_off(note, vel, gate, on):  #
    """Callback for sequencer when note should be tured off"""
    #if on: # FIXME: always do note off to since race condition of note muted right after playing
    if playdebug: print("off:%d n:%3d v:%3d %d %d" % (note,vel, gate,on), end="\n" )
    midi_msg = bytearray([0x80, note, vel])  # FIXME
    if do_usb_midi:
        usb_out.write( midi_msg )
    if do_serial_midi:
        hw.midi_uart.write( midi_msg )

def sequence_load(seq_num):
    """Load a single sequence into the sequencer from RAM storage"""
    new_seq = sequences[seq_num].copy()
    seqr.steps = new_seq
    seqr.seqno = seq_num

def sequence_save(seq_num):
    """Store current sequence in sequencer to RAM storage"""
    sequences[seq_num] = seqr.steps.copy()

def sequences_read():
    """Read entire sequence set from disk into RAM"""
    global sequences
    print("READING ALL SEQUENCES")
    with open('/saved_sequences.json', 'r') as fp:
        sequences = json.load(fp)

last_write_time = ticks_ms()
def sequences_write():
    """Write  entire sequence set from RAM to disk"""
    global last_write_time
    if ticks_ms() - last_write_time < 20000: # only allow writes every 10 seconds
        print("NO WRITE: TOO SOON")
        return
    last_write_time = ticks_ms()
    print("WRITING ALL SEQUENCES")
    with open('/saved_sequences.json', 'w') as fp:
        json.dump(sequences, fp)


hw = Hardware()

seqr = StepSequencer(num_steps, tempo, play_note_on, play_note_off, playing=False)

sequences_read()

seqr_display = StepSequencerDisplay(seqr)
hw.display.show(seqr_display)

sequence_load(0)

# init display UI
seqr_display.update_ui_all()

# various state for UI handling
last_debug_millis = 0
encoder_val_last = hw.encoder.position  # needed to calculate encoder_delta
encoder_push_millis = 0  # when was encoder pushed, 0 == no push
encoder_delta = 0  # how much encoder was turned, 0 == no turn
step_push = -1  # which step button is being pushed, -1 == no push
step_push_millis = 0  # when was a step button pushed (extra? maybe
step_edited = False

print("Ready.")

while True:
    gc.collect()  # just to make the timing of this consistent

    midi_receive()

    seqr.update()

    # update step LEDs
    for i in range(num_steps):
        (n,v,gate,on) = seqr.steps[ i ]
        if i == seqr.i:  cmax = 255  # UI: bright red = indicate sequence position
        elif on:         cmax = 10   # UI: dim red = indicate mute/unmute state
        else:            cmax = 0    # UI: off = muted
        c = max( hw.led_get(i) - 15, cmax)  # nice fade
        hw.led_set(i,c)
    hw.leds_show()

    seqr_display.update_ui_step()

    now = ticks_ms()

    # update encoder turning
    encoder_val = hw.encoder.position
    if encoder_val != encoder_val_last:
        encoder_delta = (encoder_val - encoder_val_last)
        encoder_val_last = encoder_val

    # idea: pull out all UI options into state variables
    # encoder_push_and_turn_push = encoder_delta and encoder_push_millis > 0

    # UI: encoder push + hold step key = save sequence
    #print(encoder_push_millis, now-step_push_millis)
    if encoder_push_millis > 0 and step_push_millis > 0:
        if encoder_push_millis < step_push_millis:  # encoder pushed first
            if now - step_push_millis > 1000:
                seqr_display.update_ui_seqno(f"SAVE:{step_push}")


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
            seqr_display.update_ui_bpm()
            encoder_delta = 0  # we've used up the encoder delta

        # UI: encoder turned without any modifiers == change transpose
        else:
            seqr.transpose = min(max(seqr.transpose + encoder_delta, -36), 36)
            seqr_display.update_ui_transpose()
            encoder_delta = 0  # we used up encoder delta

    # on encoder push
    encsw = hw.encoder_switch.events.get()
    if encsw:
        if encsw.pressed:
            print("encoder_switch: press")
            encoder_push_millis = now  # save when we pushed encoder

        if encsw.released:
            print("encoder_switch: release")
            if step_push == -1 and encoder_delta == 0:  # step key is not pressed and no turn
                # UI: encoder tap, with no key == play/pause
                if ticks_ms() - encoder_push_millis < 300:
                    seqr.toggle_play_pause()
                    seqr_display.update_ui_playing()
                    if not seqr.playing:
                        sequences_write()
                # UI encoder hold with no key == STOP and reset playhead to 0
                # FIXME: broken. doesn't re-start at 0 properly
                # elif ticks_diff( ticks_ms(), encoder_push_millis) > 1000:
                #     seqr.stop()
                #     seqr_display.update_ui_all()
            else:  # step key is pressed
                pass
            encoder_push_millis = 0  # say we are done with encoder, on key release


    # on step key push
    key = hw.keys.events.get()
    if key:
        try:
            # record which step key is pushed for other UI modifiers
            # .index() throws the ValueError, thus the try/except
            step_push = hw.step_to_key_pos.index(key.key_number) # map key pos back to step num
            (n,v,gate,on) = seqr.steps[step_push]

            if key.pressed:
                print("+ press", key.key_number, "step_push:",step_push)
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
                        print("save sequence:", step_push)
                        sequence_save( step_push )
                        seqr_display.update_ui_seqno()
                        seqr_display.update_ui_step()
                    # UI: encoder push + tap step key = load sequence
                    else:
                        print("load sequence:", step_push)
                        sequence_load( step_push )
                        seqr_display.update_ui_seqno()
                        if not seqr.playing:
                            seqr_display.update_ui_steps() # causes too much lag when playing
                        (n,v,gate,on) = seqr.steps[step_push]
                else:
                    if seqr.playing:
                        if step_edited:
                            pass
                        else:
                            # UI: if playing, step keys == toggles enable (must be on relase)
                            on = not on
                            seqr.steps[step_push] = (n, v, gate, on)
                    else:
                        # UI: if not playing, step key == play their pitches
                        (n,v,gate,on) = seqr.steps[step_push]
                        play_note_off( n, v, gate, True )

                seqr_display.update_ui_step( step_push, n, v, gate, on, False)
                step_push = -1  # say we are done with key
                step_push_millis = 0 # say we're done with key push
                step_edited = False  # done editing  # FIXME we need all these vars? I think so

        except ValueError:  # undefined macropad key was pressed, ignore
            pass

    #
    #emillis = ticks_ms() - now
    #print("emillis:",emillis)
