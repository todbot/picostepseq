# macropadrp2040_step_sequencer_test.py -- 
# 6 Aug 2022 - @todbot / Tod Kurt
#
# User interface:
# - turn encoder to change tempo
# - tap encoder to toggle play / pause
#


# built in modules
import random
import board, keypad
import displayio, terminalio

# installed via circup
import adafruit_midi
import adafruit_macropad
from adafruit_display_text import bitmap_label as label

# local libraries in CIRCUITPY
from step_sequencer import StepSequencer, ticks_ms, ticks_diff

printdebug = False

base_note = 60  #  60 = C4, 48 = C3
num_steps = 8
tempo = 100
gate_default = 8    # ranges 0-15

macropad = adafruit_macropad.MacroPad()
macropad.pixels.brightness = 0.2
macropad._encoder_switch.deinit()
encoder_switch = keypad.Keys((board.BUTTON,), value_when_pressed=False, pull=True)

maingroup = displayio.Group()
macropad.display.show(maingroup)

step_to_key_pos = (1, 4, 7, 10, 0, 3, 6, 9)

#step_text_pos = ( (45,10), (45,20), (45,30), (45,40),
#                  (5,10), (5,20), (5,30), (5,40) )
step_text_pos = ( (0,10), (30,10), (60,10), (90,10),
                  (0,25), (30,25), (60,25), (90,25) )
                  
stepgroup = displayio.Group()
maingroup.append(stepgroup)
for (x,y) in step_text_pos:
    stepgroup.append( label.Label(terminalio.FONT, text="txt ", x=x, y=y))
tempo_text = label.Label(terminalio.FONT, text="tmpo", x=0, y=57)
play_text = label.Label(terminalio.FONT, text="play", x=100, y=57)
transpose_text = label.Label(terminalio.FONT, text="trans", x=50, y=57)
maingroup.append(tempo_text)
maingroup.append(play_text)
maingroup.append(transpose_text)


def play_on_note(step, note, vel, gate, on):  # 
    global play_note_last
    #macropad.pixels.fill(0)
    macropad.pixels[step_to_key_pos[step]] = 0xff0000 if on else 0x888888  # FIXME: what to do for step switches?
    if on:
        if printdebug: print("on :%d n:%3d v:%3d %d %d" % (step, note,vel, gate,on), end="\n" )
        macropad.midi.send( macropad.NoteOn(note, vel), channel=0)

def play_off_note(step, note, vel, gate, on):  # 
    macropad.pixels[step_to_key_pos[step]] = 0 #0xff0000 if on else 0x888888  # FIXME: what to do for step switches?
    if on:
        if printdebug: print("off:%d n:%3d v:%3d %d %d" % (step, note,vel, gate,on), end="\n" )
        macropad.midi.send( macropad.NoteOff(note, vel), channel=0)


def update_disp_step(i, n, v=127, gate=8, on=True):
    print("udpate_disp_step:", i,n,v,gate,on )
    nstr = seq.notenum_to_name(n) # if on else '---'
    gstr = " " if on else '*'
    stepgroup[i].text = "%1s%3s%1s" % (" ", nstr, gstr)

def update_disp_tempo():
    tempo_text.text = str(tempo)

def update_disp_playing():
    play_text.text = " > " if seq.playing else "|| "

def update_disp_transpose():
    transpose_text.text = "%+2d" % seq.transpose 

def update_display():
    update_disp_tempo()
    update_disp_playing()
    update_disp_transpose()
    for i in range(num_steps):
        (n,v,gate,on) = seq.steps[i]
        update_disp_step( i, n, v, gate, on)


seq = StepSequencer(num_steps, tempo, play_on_note, play_off_note)

last_debug_millis = 0
last_encoder_val = macropad.encoder
encoder_push_millis = 0  # when was encoder pushed
encoder_delta = 0
step_push = -1
step_push_millis = 0   # when was a step button pushed

# init sequencer with saved / default
for i in range(num_steps):
    (n,v,gate,on) = (base_note, 127, gate_default, True)
    seq.steps[ i ] = (n,v,gate,on)
    update_disp_step(i, n, v, gate, on)
# init display
update_display()


while True:
    
    seq.update()

    #if ticks_diff(ticks_ms(), last_debug_millis) > 5000:
    #    last_debug_millis = ticks_ms()
    #    print(last_debug_millis, "----------- hi")

    # update encoder turning
    encoder_val = macropad.encoder
    if encoder_val != last_encoder_val:
        encoder_delta = (encoder_val - last_encoder_val)
        last_encoder_val = encoder_val
        
    # if encoder turned
    if encoder_delta:
        # encoder turned while step key held == change step's note
        if step_push > -1:  # step key pressed
            (n,v,gate,on) = seq.steps[ step_push ]
            n = n + encoder_delta
            seq.steps[ step_push ] = (n,v,gate,on)
            update_disp_step( step_push, n, v, gate, on)

        # encoder turned while encoder pushed == change transpose
        elif encoder_push_millis > 0:
            seq.transpose += encoder_delta
            update_disp_transpose()
        
        # encoder turned without any modifiers == change tempo
        else:
            tempo = tempo + encoder_delta
            seq.set_tempo(tempo)
            update_disp_tempo()

        encoder_delta = 0  # we used up the encoder

    # encoder push
    encsw = encoder_switch.events.get()
    if encsw:
        if encsw.pressed:
            encoder_push_millis = ticks_ms()  # save when we pushed encoder
            
            # encoder push while holding step key == mute/unmute
            if step_push > -1:
                (n,v,gate,on) = seq.steps[step_push]
                on = not on  # toggle
                seq.steps[ step_push ] = (n,v,gate,on)
                update_disp_step( step_push, n, v, gate, on )

        if encsw.released:
            # encoder tap, with no key == play/pause
            if ticks_diff( ticks_ms(), encoder_push_millis) < 300 and step_push == -1:
                seq.playing = not seq.playing
                update_disp_playing()
            # # encoder hold with no key == STOP and reset playhead to 0
            # FIXME: broken. doesn't re-start at 0 properly
            # elif ticks_diff( ticks_ms(), encoder_push_millis) > 1000 and step_push == -1:
            #     seq.stop()
            #     update_display()
            
            encoder_push_millis = 0
    
    # step key press
    key = macropad.keys.events.get()
    if key:
        if key.pressed:
            print("press",key.key_number) #, macropad.encoder, macropad.encoder_switch)
            try:
                # record which step key is pushed for other UI modifiers 
                step_push = step_to_key_pos.index(key.key_number) # map key pos back to step num
                step_push_millis = ticks_ms()
                # if not playing, step keys play their pitches
                if not seq.playing:
                    (n,v,on) = seq.steps[step_push]
                    play_note( step_push, n, v, True ) # note on
            except ValueError:
                pass
                
        if key.released:
            print("release", key.key_number)
            #if ticks_diff( ticks_ms(), step_push_millis)
            step_push = -1
            # if not playing, step keys play their pitches
            if not seq.playing:
                (n,v,on) = seq.steps[step_push]
                play_note( step_push, n, 0, True ) # note off = note on w/ zero vel
