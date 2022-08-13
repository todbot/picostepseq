import time, random
import board, digitalio, busio
#import adafruit_midi
#from adafruit_midi.note_on import NoteOn

midi_uart = busio.UART(tx=board.GP16, rx=board.GP17, baudrate=31250, timeout=0.01)
#midi_serial = adafruit_midi.MIDI(midi_out=midi_uart)

midi_out_buf = bytearray([0x90, 0, 0])

scale_lydian = [0, 2, 4, 6, 7, 9, 11]
base_note = 48

import gc
while True:
    st = time.monotonic()
    gc.collect()

    bytes_in = midi_uart.read(30)
    if bytes_in:
        print("read bytes:",bytes_in)

    note_val =  base_note + scale_lydian[ random.randint(0,len(scale_lydian)-1) ]
    #print("note:",note_val)

    #noteon = NoteOn(note_val, 100)
    #midi_serial.send( noteon )
    midi_out_buf[1] = note_val
    midi_out_buf[2] = 64
    midi_uart.write(midi_out_buf)
    time.sleep(0.1)

    #noteoff = NoteOn(note_val, 0)
    #midi_serial.send( noteoff )
    midi_out_buf[1] = note_val
    midi_out_buf[2] = 0
    midi_uart.write(midi_out_buf)
    time.sleep(0.1)

    et = time.monotonic() - st
    print(int(et*1000))
