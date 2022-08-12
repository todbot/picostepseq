import asyncio
import time, random
import board, digitalio, busio
import adafruit_midi
from adafruit_midi.note_on import NoteOn

midi_uart = busio.UART(tx=board.SDA, rx=board.SCL, baudrate=31250, timeout=0.01)
#midi_serial = adafruit_midi.MIDI(midi_out=midi_uart)

midi_out_buf = bytearray([0x90, 0, 0])
#port_out.write(bytearray([smolmidi.NOTE_ON, 0x64, 0x42]))

scale_lydian = [0, 2, 4, 6, 7, 9, 11]
base_note = 48

import gc
async def midi_boop( interval, count):  # Don't forget the async!
    while True:
    #for _ in range(count):
        st = time.monotonic()
        #gc.collect()
        note_val =  base_note + scale_lydian[ random.randint(0,len(scale_lydian)-1) ]
        #print("note",note_val)
        midi_out_buf[1] = note_val
        midi_out_buf[2] = 64
        midi_uart.write(midi_out_buf)
        await asyncio.sleep(interval)  # Don't forget the await!


        midi_out_buf[1] = note_val
        midi_out_buf[2] = 0
        midi_uart.write(midi_out_buf)
        await asyncio.sleep(interval)  # Don't forget the await!

        et = time.monotonic() - st
        print(int(et*1000), gc.mem_free())


async def main():
    midi_boop_task = asyncio.create_task( midi_boop(interval=0.1, count=100) )
    await asyncio.gather(midi_boop_task)

asyncio.run(main())

while True:
    print("done")
    time.sleep(1)
