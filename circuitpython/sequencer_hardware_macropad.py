#
#
#

import board
import neopixel

# OLED display
display = board.DISPLAY

# LEDS
leds = neopixel.NeoPixel(board.NEOPIXEL, 12)
leds.brightness = 0.2
leds.auto_write = False

# KEYS
key_pins = [getattr(board, "KEY%d" % (num + 1)) for num in order]
keys = keypad.Keys(key_pins, value_when_pressed=False, pull=True)

# KNOB
encoder = rotaryio.IncrementalEncoder(board.ROTA, board.ROTB)
encoder_switch = keypad.Keys((board.BUTTON,), value_when_pressed=False, pull=True)

# map step position to macropad key number
step_to_key_pos = (1, 4, 7, 10, 0, 3, 6, 9)

# set LED brightness to value from 0-255
def set_led(i,v):
    leds[i].duty_cycle = v * 256  # duty_cycle 0-65535

# refresh all LEDs (if meaningful)
def show_leds():
    leds.show()
