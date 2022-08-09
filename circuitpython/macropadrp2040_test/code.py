# macropadrp2040_step_sequencer_test.py -- 
# 6 Aug 2022 - @todbot / Tod Kurt
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
import random
import board, keypad
import displayio, terminalio, vectorio

# installed via circup
import adafruit_midi
import adafruit_macropad
from adafruit_display_text import bitmap_label as label

# local libraries in CIRCUITPY
from step_sequencer import StepSequencer, ticks_ms, ticks_diff
from step_sequencer_display import StepSequencerDisplay

playdebug = False
uidebug = True

base_note = 60  #  60 = C4, 48 = C3
num_steps = 8
tempo = 80
gate_default = 8    # ranges 0-15

macropad = adafruit_macropad.MacroPad()
macropad.pixels.brightness = 0.2
macropad.pixels.auto_write = False

macropad._encoder_switch.deinit()  # so we can use keypad for debounce, like civilized folk
encoder_switch = keypad.Keys((board.BUTTON,), value_when_pressed=False, pull=True)

stepseq_display = StepSequencerDisplay()

maingroup = displayio.Group()
macropad.display.show(maingroup)

step_to_key_pos = (1, 4, 7, 10, 0, 3, 6, 9)

step_text_pos = ( (0,10), (30,10), (60,10), (90,10),
                  (0,25), (30,25), (60,25), (90,25) )

gate_pal = displayio.Palette(1)
gate_pal[0] = 0xffffff
stepgroup = displayio.Group()
gategroup = displayio.Group()
gatewidth = 16
maingroup.append(stepgroup)
maingroup.append(gategroup)
for (x,y) in step_text_pos:
    stepgroup.append( label.Label(terminalio.FONT, text="txt ", x=x, y=y))
    gategroup.append( vectorio.Rectangle(pixel_shader=gate_pal, width=gatewidth, height=4, x=x+1, y=y+6))

tempo_text = label.Label(terminalio.FONT, text="tmpo", x=0, y=57)
play_text = label.Label(terminalio.FONT, text="play", x=100, y=57)
transpose_text = label.Label(terminalio.FONT, text="trans", x=50, y=57)
maingroup.append(tempo_text)
maingroup.append(play_text)
maingroup.append(transpose_text)

# callback for sequencer
def play_note_on(stepi, note, vel, gate, on):  #
    if on:
        if playdebug: print("on :%d n:%3d v:%3d %d %d" % (stepi, note,vel, gate,on), end="\n" )
        macropad.midi.send( macropad.NoteOn(note, vel), channel=0)

# callback for sequencer
def play_note_off(step, note, vel, gate, on):  #
    if on:
        if playdebug: print("off:%d n:%3d v:%3d %d %d" % (stepi, note,vel, gate,on), end="\n" )
        macropad.midi.send( macropad.NoteOff(note, vel), channel=0)

# 
def update_ui_step(step, n, v=127, gate=8, on=True):
    if uidebug: print("udpate_disp_step:", step,n,v,gate,on )
    notestr = seq.notenum_to_name(n) # if on else '---'
    onstr = " " if on else '*'
    editstr = "e" if step_push == step else ' '
    gategroup[step].width = gate * gatewidth // 16
    #step2_rect.height = gate * 12 // 16
    #step2_rect.width = gate * 12 // 16
    stepgroup[step].text = "%3s%1s%1s" % (notestr, onstr, editstr)

def update_ui_tempo():
    tempo_text.text = str(tempo)

def update_ui_playing():
    play_text.text = " > " if seq.playing else "|| "

def update_ui_transpose():
    transpose_text.text = "%+2d" % seq.transpose 

def update_ui_all():
    update_ui_tempo()
    update_ui_playing()
    update_ui_transpose()
    for i in range(num_steps):
        (n,v,gate,on) = seq.steps[i]
        update_ui_step( i, n, v, gate, on)


seq = StepSequencer(num_steps, tempo, play_note_on, play_note_off)

# various state for UI hanlding
last_debug_millis = 0
last_encoder_val = macropad.encoder  # needed to calculate encoder_delta
encoder_push_millis = 0  # when was encoder pushed, 0 == no push
encoder_delta = 0 # how much encoder was turned, 0 == no turn
step_push = -1  # which step button is being pushed, -1 == no push
step_push_millis = 0  # when was a step button pushed (extra? maybe 
step_edited = False

# init sequencer with saved / default
for i in range(num_steps):
    (n,v,gate,on) = (base_note, 127, gate_default, True)
    seq.steps[ i ] = (n,v,gate,on)
    update_ui_step(i, n, v, gate, on)

# init display UI
update_ui_all()

while True:
    # update step LEDs
    for i in range(num_steps):
        (n,v,gate,on) = seq.steps[ i ]
        c = 0x000000 # turn LED off
        if i == seq.i:  c = 0xff0000  # UI: bright red = indicate sequence position
        elif on:        c = 0x110000  # UI: dim red = indicate mute/unmute state
        macropad.pixels[step_to_key_pos[i]] = c
    macropad.pixels.show()
    
    #step = seq.update()
    seq.update()

    now = ticks_ms()
    
    # update encoder turning
    encoder_val = macropad.encoder
    if encoder_val != last_encoder_val:
        encoder_delta = (encoder_val - last_encoder_val)
        last_encoder_val = encoder_val

    # on encoder turn
    if encoder_delta:

        # UI: encoder turned and pushed while step key held == change step's gate
        if step_push > -1 and encoder_push_millis > 0:
            (n,v,gate,on) = seq.steps[ step_push ]
            gate = min(max(gate + encoder_delta, 1), 15)
            seq.steps[ step_push ] = (n,v,gate,on)
            step_edited = True
            update_ui_step( step_push, n, v, gate, on)

        # UI:  encoder turned while step key held == change step's note
        elif step_push > -1:  # step key pressed
            (n,v,gate,on) = seq.steps[ step_push ]
            if not seq.playing:
                play_note_off( step_push, n, v, gate, True )
            n = min(max(n + encoder_delta, 1), 127)
            if not seq.playing:
                play_note_on( step_push, n, v, gate, True )
            
            seq.steps[ step_push ] = (n,v,gate,on)
            step_edited = True
            update_ui_step( step_push, n, v, gate, on)
        
        # UI: encoder turned while encoder pushed == change tempo
        elif encoder_push_millis > 0:
            tempo = tempo + encoder_delta
            seq.set_tempo(tempo)
            update_ui_tempo()
        
        # UI: encoder turned without any modifiers == change transpose
        else:
            seq.transpose = min(max(seq.transpose + encoder_delta, -36), 36)
            update_ui_transpose()

        encoder_delta = 0  # say we are done with encoder

    # on encoder push
    encsw = encoder_switch.events.get()
    if encsw:
        if encsw.pressed:
            encoder_push_millis = now  # save when we pushed encoder

        if encsw.released:
            if step_push == -1:  # step key is not pressed
                # UI: encoder tap, with no key == play/pause
                if ticks_diff( ticks_ms(), encoder_push_millis) < 300:
                    seq.playing = not seq.playing
                    update_ui_playing()
                # UI encoder hold with no key == STOP and reset playhead to 0
                # FIXME: broken. doesn't re-start at 0 properly
                elif ticks_diff( ticks_ms(), encoder_push_millis) > 1000:
                    seq.stop()
                    update_ui_all()
            else:  # step key is pressed
                pass
            encoder_push_millis = 0  # say we are done with encoder

    # on step key push
    key = macropad.keys.events.get()
    if key:
        try:
            # record which step key is pushed for other UI modifiers
            # .index() throws the ValueError, thus the try/except
            step_push = step_to_key_pos.index(key.key_number) # map key pos back to step num
            step_push_millis = ticks_ms()
            (n,v,gate,on) = seq.steps[step_push]

            if key.pressed:
                print("+ press",key.key_number, step_push)
                update_ui_step( step_push, n, v, gate, on)
                
                # UI: if not playing, step keys == play their pitches
                if not seq.playing: 
                    play_note_on( step_push, n, v, gate, True )

            elif key.released:
                print("- release", key.key_number, step_push)
                if seq.playing:
                    if not step_edited:
                        # UI: if playing, step keys == toggles enable
                        on = not on
                        seq.steps[step_push] = (n,v,gate, on)
                else:
                    # UI: if not playing, step key == play their pitches
                    (n,v,gate,on) = seq.steps[step_push]
                    play_note_off( step_push, n, v, gate, True )
                sp_tmp = step_push
                step_push = -1  # say we are done with key
                step_edited = False  # done editing
                update_ui_step( sp_tmp, n, v, gate, on)

        except ValueError:  # undefined macropad key was pressed, ignore
            pass
            
    #
    #emillis = ticks_ms() - now
    #print("emillis:",emillis)
