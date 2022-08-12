# sequencer_hardware_macropad.py -- picostepseq hardware specific setup for MacroPad 2040
# 10 Aug 2022 - @todbot / Tod Kurt
# Part of picostepseq : https://github.com/todbot/picostepseq/

import board
import rotaryio
import keypad
import neopixel

dw,dh = 128,64

key_pins = [getattr(board, "KEY%d" % (num + 1)) for num in order]

encoderA_pin, encoderB_pin, encoderSW_pin = board.ROTA, board.ROTB, board.BUTTON

midi_tx_pin, midi_rx_pin = board.SDA, board.SCL

class Hardware():
    def __init__(self):
        # KEYS
        self.keys = keypad.Keys(key_pins, value_when_pressed=False, pull=True)
        self.step_to_key_pos = (1, 4, 7, 10, 0, 3, 6, 9)

        # LEDS
        self.leds = neopixel.NeoPixel(board.NEOPIXEL, 12)
        self.leds.brightness = 0.2
        self.leds.auto_write = False

        # KNOB
        self.encoder = rotaryio.IncrementalEncoder(encoderA_pin, encoderB_pin)
        self.encoder_switch = keypad.Keys((encoderSW_pin,), value_when_pressed=False, pull=True)

        # DISPLAY
        self.display = board.DISPLAY
        self.display.rotation = 90

        # # uart midi setup
        # midi_timeout = 0.01
        # self.midi_uart = busio.UART(tx=midi_tx_pin, rx=midi_rx_pin, baudrate=31250) # timeout=midi_timeout)

    # set LED brightness to value from 0-255
    def led_set(self,i,v):
        self.leds[i] = (v, 0,0)

    # get LED brightness to value 0-255
    def led_get(self,i):
        c = self.leds[i]
        return c[0]

    # refresh all LEDs (if meaningful)
    def leds_show(self):
        self.leds.show()
