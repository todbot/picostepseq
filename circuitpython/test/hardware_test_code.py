# hardware_test_code.py -- basic proof-of-concept of all IO on picostepseq, copy as 'code.py'
# 5 Aug 2022 - @todbot / Tod Kurt
# Part of https://github.com/todbot/picostepseq/
#

# built-in libraries
import time
import board
import digitalio, pwmio
import busio
import keypad
import rotaryio
import displayio, terminalio

# libraries needed to be installed by circup
import adafruit_debouncer
import adafruit_displayio_ssd1306
from adafruit_display_text import bitmap_label as label

# all the pin functions
led_pins = (board.GP0, board.GP2, board.GP4, board.GP6,
            board.GP8, board.GP10, board.GP12, board.GP14)

key_pins = (board.GP1, board.GP3, board.GP5, board.GP7,
            board.GP9, board.GP11, board.GP13, board.GP15)

encoderA_pin, encoderB_pin, encoderSW_pin = board.GP18, board.GP19, board.GP22

oled_sda_pin, oled_scl_pin = board.GP20, board.GP21

midi_tx_pin, midi_rx_pin = board.GP16, board.GP17


# create the objects handling those pin functions
def make_led_digital(p): po=digitalio.DigitalInOut(p); po.switch_to_output(); return po
def make_led(p): po = pwmio.PWMOut(p, frequency=25000, duty_cycle=0); return po

keys = keypad.Keys(key_pins, value_when_pressed=False, pull=True)
leds = [ make_led(p) for p in led_pins ]

encoder = rotaryio.IncrementalEncoder(encoderA_pin, encoderB_pin)
enctmp = digitalio.DigitalInOut(encoderSW_pin)
enctmp.pull = digitalio.Pull.UP
encoder_sw = adafruit_debouncer.Debouncer( enctmp )

# midi setup
midi_timeout = 0.01
uart = busio.UART(tx=midi_tx_pin, rx=midi_rx_pin,
                  baudrate=31250, timeout=midi_timeout)

# display setup
displayio.release_displays()
dw,dh = 128,64
oled_i2c = busio.I2C( scl=oled_scl_pin, sda=oled_sda_pin )
display_bus = displayio.I2CDisplay(oled_i2c, device_address=0x3C)  # or 0x3D depending on display
display = adafruit_displayio_ssd1306.SSD1306(display_bus, width=dw, height=dh, rotation=0)
maingroup = displayio.Group()
display.show(maingroup)

text_area1 = label.Label(terminalio.FONT, text="hello\nworld...", line_spacing=0.75, x=5,y=dh//4,scale=2)
text_area2 = label.Label(terminalio.FONT, text="@todbot", line_spacing=0.75, x=dw-60, y=dh-10, scale=1)
maingroup.append(text_area1)
maingroup.append(text_area2)

step_cnt = len(leds)

last_time = 0
led_i=0
last_encoder_pos = encoder.position

while True:

    for i in range(step_cnt):
        leds[i].duty_cycle = max(leds[i].duty_cycle - 100, 0)

    if time.monotonic() - last_time > 0.2:
        last_time = time.monotonic()
        led_i = (led_i + 1) % step_cnt
        leds[led_i].duty_cycle = 65535
        print("led_i",led_i, encoder.position)

    key = keys.events.get()
    if key:
        print("key:",key)
        if key.pressed:
            leds[key.key_number].duty_cycle = 65535

    delta_pos = encoder.position - last_encoder_pos
    if delta_pos:
        text_area2.x = text_area2.x + 2*delta_pos
        last_encoder_pos = encoder.position

    encoder_sw.update()
    if encoder_sw.fell:
        print("encoder sw fell")
        text_area1.x = text_area1.x + 20
    if encoder_sw.rose:
        print("encoder sw rose")
        text_area1.x = text_area1.x - 20
