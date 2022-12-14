# picostepseq_test_code.py -- test framework for picostepseq
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

# installed via circup
#import adafruit_midi

# local libraries in CIRCUITPY
from sequencer import StepSequencer, ticks_ms, ticks_diff

if 'macropad' in board.board_id:
    from sequencer_display_macropad import StepSequencerDisplay
    from sequencer_hardware_macropad import StepSequencerHardware
    # display, encoder, encoder_switch, keys, set_led, show_leds
    #import sequencer_hardware_macropad
else:
    from sequencer_display import StepSequencerDisplay
    #from sequencer_hardware import display, encoder, encoder_switch, keys, led_set, leds_show
    #from sequencer_hardware import Hardware



import board
import busio
import pwmio
import rotaryio
import keypad
import displayio
import adafruit_displayio_ssd1306

led_pins = (board.GP0, board.GP2, board.GP4, board.GP6,
            board.GP8, board.GP10, board.GP12, board.GP14)

key_pins = (board.GP1, board.GP3, board.GP5, board.GP7,
            board.GP9, board.GP11, board.GP13, board.GP15)

encoderA_pin, encoderB_pin, encoderSW_pin = board.GP18, board.GP19, board.GP22

oled_sda_pin, oled_scl_pin = board.GP20, board.GP21

midi_tx_pin, midi_rx_pin = board.GP16, board.GP17

# KEYS
keys = keypad.Keys(key_pins, value_when_pressed=False, pull=True)
step_to_key_pos = (0,1,2,3,4,5,6,7)

# LEDS
# create the objects handling those pin functions
def make_led(p): po = pwmio.PWMOut(p, frequency=25000, duty_cycle=0); return po
leds = [ make_led(p) for p in led_pins ]

# KNOB
encoder = rotaryio.IncrementalEncoder(encoderA_pin, encoderB_pin)
encoder_switch = keypad.Keys((encoderSW_pin,), value_when_pressed=False, pull=True)

# DISPLAY
displayio.release_displays()

dw,dh = 128,64
oled_i2c = busio.I2C( scl=oled_scl_pin, sda=oled_sda_pin )
display_bus = displayio.I2CDisplay(oled_i2c, device_address=0x3C)  # or 0x3D depending on display
display = adafruit_displayio_ssd1306.SSD1306(display_bus, width=dw, height=dh)

usb_out = usb_midi.ports[1]

# # uart midi setup
# midi_timeout = 0.01
# uart = busio.UART(tx=midi_tx_pin, rx=midi_rx_pin, baudrate=31250) # timeout=midi_timeout)

# set LED brightness to value from 0-255
def led_set(i,v):
    leds[i].duty_cycle = v * 256  # duty_cycle 0-65535

# refresh all LEDs (if meaningful)
def leds_show():
    pass


playdebug = False

base_note = 60  #  60 = C4, 48 = C3
num_steps = 8
tempo = 100
gate_default = 8    # ranges 0-15
sequences = [ [(None)] * num_steps ] * num_steps  # pre-fill arrays for easy use later


# callback for sequencer
def play_note_on(note, vel, gate, on):  #
    if not on: return
    if playdebug: print("on :%d n:%3d v:%3d %d %d" % (note,vel, gate,on), end="\n" )
    usb_out.write( bytearray([0x90, note, vel]) )  # FIXME
    #macropad.midi.send( macropad.NoteOn(note, vel), channel=0)

# callback for sequencer
def play_note_off(note, vel, gate, on):  #
    #if on:
    # FIXME: always do note off to since race condition of note muted right after playing
    if playdebug: print("off:%d n:%3d v:%3d %d %d" % (note,vel, gate,on), end="\n" )
    usb_out.write( bytearray([0x80, note, vel]))  # FIXME
    #macropad.midi.send( macropad.NoteOff(note, vel), channel=0)

def sequence_load(seq_num):
    new_seq = sequences[seq_num].copy()
    seqr.steps = new_seq
    seqr.seqno = seq_num

def sequence_save(seq_num):
    sequences[seq_num] = seqr.steps.copy()

def sequences_read():
    global sequences
    print("READING ALL SEQUENCES")
    with open('/saved_sequences.json', 'r') as fp:
        sequences = json.load(fp)

last_write_time = ticks_ms()
def sequences_write():
    global last_write_time
    if ticks_ms() - last_write_time < 5000: # only allow writes every 5 seconds
        print("NO WRITE: TOO SOON")
        return
    last_write_time = ticks_ms()
    print("WRITING ALL SEQUENCES")
    with open('/saved_sequences.json', 'w') as fp:
        json.dump(sequences, fp)

#hardware = Hardware()

seqr = StepSequencer(num_steps, tempo, play_note_on, play_note_off, playing=False)

sequences_read()

seqr_display = StepSequencerDisplay(seqr)
#display.rotation = seqr_display.rotation
display.show(seqr_display)

sequence_load(0)

# init display UI
seqr_display.update_ui_all()

# various state for UI hanlding
last_debug_millis = 0
last_encoder_val = encoder.position  # needed to calculate encoder_delta
encoder_push_millis = 0  # when was encoder pushed, 0 == no push
encoder_delta = 0 # how much encoder was turned, 0 == no turn
step_push = -1  # which step button is being pushed, -1 == no push
step_push_millis = 0  # when was a step button pushed (extra? maybe
step_edited = False

print("Ready.")
while True:
    gc.collect()  # just to make the timing of this consistent

    # update step LEDs
    for i in range(num_steps):
        (n,v,gate,on) = seqr.steps[ i ]
        c = 0 # turn LED off
        if i == seqr.i:  c = 0xff  # UI: bright red = indicate sequence position
        elif on:         c = 0x11  # UI: dim red = indicate mute/unmute state
        led_set(i,c)
    leds_show()

    seqr.update()

    now = ticks_ms()

    # update encoder turning
    encoder_val = encoder.position
    if encoder_val != last_encoder_val:
        encoder_delta = (encoder_val - last_encoder_val)
        last_encoder_val = encoder_val

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
    key = keys.events.get()
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
                        print("save sequence:", step_push)
                        sequence_save( step_push )
                        seqr_display.update_ui_seqno()
                    # UI: encoder push + tap step key = load sequence
                    else:
                        print("load sequence:", step_push)
                        sequence_load( step_push )
                        seqr_display.update_ui_seqno()
                        seqr_display.update_ui_steps()
                        (n,v,gate,on) = seqr.steps[step_push]
                else:
                    if seqr.playing:
                        if step_edited:
                            print("here")
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
