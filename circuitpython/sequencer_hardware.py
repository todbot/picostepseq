# sequencer_hardware.py -- picostepseq hardware specific setup
# 10 Aug 2022 - @todbot / Tod Kurt
# Part of picostepseq : https://github.com/todbot/picostepseq/


import board
import busio
import pwmio
import rotaryio
import keypad
import displayio
import adafruit_displayio_ssd1306

dw,dh = 128,64

led_pins = (board.GP0, board.GP2, board.GP4, board.GP6,
            board.GP8, board.GP10, board.GP12, board.GP14)

key_pins = (board.GP1, board.GP3, board.GP5, board.GP7,
            board.GP9, board.GP11, board.GP13, board.GP15)

encoderA_pin, encoderB_pin, encoderSW_pin = board.GP18, board.GP19, board.GP22

oled_sda_pin, oled_scl_pin = board.GP20, board.GP21

midi_tx_pin, midi_rx_pin = board.GP16, board.GP17

def make_led(p): po = pwmio.PWMOut(p, frequency=25000, duty_cycle=0); return po

class Hardware():
    def __init__(self):
        # KEYS
        self.keys = keypad.Keys(key_pins, value_when_pressed=False, pull=True)
        self.step_to_key_pos = (0,1,2,3,4,5,6,7)

        # LEDS
        # create the objects handling those pin functions
        self.leds = [ make_led(p) for p in led_pins ]

        # KNOB
        self.encoder = rotaryio.IncrementalEncoder(encoderA_pin, encoderB_pin)
        self.encoder_switch = keypad.Keys((encoderSW_pin,), value_when_pressed=False, pull=True)

        # DISPLAY
        oled_i2c = busio.I2C( scl=oled_scl_pin, sda=oled_sda_pin, frequency=400_000 )
        display_bus = displayio.I2CDisplay(oled_i2c, device_address=0x3C)  # or 0x3D depending on display
        self.display = adafruit_displayio_ssd1306.SSD1306(display_bus, width=dw, height=dh)

        # # uart midi setup
        #midi_timeout = 0.01
        self.midi_uart = busio.UART(tx=midi_tx_pin, rx=midi_rx_pin, baudrate=31250) # timeout=midi_timeout)

    # set LED brightness to value from 0-255
    def led_set(self,i,v):
        self.leds[i].duty_cycle = v * 256  # duty_cycle 0-65535

    # get LED brightness to value 0-255
    def led_get(self,i):
        return self.leds[i].duty_cycle // 256

    # refresh all LEDs (if meaningful)
    def leds_show(self):
        pass
