# picostepseq

An 8-step sequencer using a Raspberry Pi Pico, a 0.9" I2C SSD1306 OLED display, and a rotary encoder

<img width=700 src="./docs/picostepseq_render2.jpg"/>

Demo video of it action:


https://user-images.githubusercontent.com/274093/185005269-afb4c3f7-0ca1-40c8-a17d-6e4756943d87.mov


## Bill of Materials

Digikey cart with most parts is here.
Links also included below for major parts

USB MIDI:

- 1 - "picostepseq" PCB ([OSHpark](https://oshpark.com/shared_projects/vPWjBrmO))
- 1 - Raspberry Pi Pico ([Adafruit](https://www.adafruit.com/product/4864), [Digikey](https://www.digikey.com/en/products/detail/raspberry-pi/SC0915/13624793))
- 1 - 0.96" I2C OLED SSD1306 128x64 display ([Amazon](https://amzn.to/3K1ZAoo))
  - w/ pins in order GND, VCC, SCL, SDA, some have GND & VCC swapped
- 1 - EC11 style rotary encoder w/ switch ([Digikey](https://www.digikey.com/en/products/detail/bourns-inc/PEC11R-4215F-S0024/4499665), [Adafruit](https://www.adafruit.com/product/377)]
- 8 - "step switch" w/ built-in LED ([Adafruit](https://www.adafruit.com/product/5519)]
- 8 - resistor 1k (500 ohm also works) ([Digikey](https://www.digikey.com/en/products/detail/stackpole-electronics-inc/CF18JT1K00/1741612), [Adafruit](https://www.adafruit.com/product/4294)]
- 1 - encoder knob ("Davies 1900" style works well, [Adafruit](https://www.adafruit.com/product/5541))

USB + Serial MIDI, add:

- 2 - 3.5mm TRS jack, SJ-3523-SMT-TR
([Digikey](https://www.digikey.com/en/products/detail/cui-devices/SJ-3523-SMT-TR/281297))
- 1 - 6N138 optoisolator ([Digikey](https://www.digikey.com/en/products/detail/liteon/6N138/1969179))
- 1 - 100n capacitor ([Digikey](https://www.digikey.com/en/products/detail/vishay-beyschlag-draloric-bc-components/K104K15X7RF5TL2/286538), [Adafruit](https://www.adafruit.com/product/753))
- 1 - resistor 10 ohm
- 1 - resistor 30 ohm (47 ohm also works)
- 1 - resistor 220 ohm

If using the 3d-printable "picostepseq_headers" case, you will also need:

- 2 - 20-pin header socket ([Digikey](https://www.digikey.com/en/products/detail/sullins-connector-solutions/PPPC201LFBN-RC/810192) ), [Adafruit](https://www.adafruit.com/product/5583))
- 2 - 20-pin header pins ([Digikey](https://www.digikey.com/en/products/detail/adam-tech/PH1-20-UA/9830398), [Adafruit ](https://www.adafruit.com/product/392))
- 1 - 4-pin female (same as above, break off 4-pin chunk)

## PCB layout:

<img width=700 src="./docs/picostepseq_pcbbot.png"/>

<img width=700 src="./docs/picostepseq_pcbtop.png"/>
