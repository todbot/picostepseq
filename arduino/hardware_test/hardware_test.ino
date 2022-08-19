/**
 * hardware_test.ino  -- simple hardware test in Arduino for picostepseq
 * 15 Aug 2022 - @todbot / Tod Kurt
 * Part of https://github.com/todbot/picostepseq/
 * 
 **/
 
#include <Wire.h>
#include <RotaryEncoder.h>
#include <Bounce2.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMono18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include "Font5x7FixedMono.h"


#define myfont FreeMono9pt7b
#define myfont2 Font5x7FixedMono 
// see: https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts

const int num_steps = 8;
const int dw = 128;
const int dh = 64;

const int led_pins[] = {0, 2, 4, 6, 8, 10, 12, 14};
const int key_pins[] = {1, 3, 5, 7, 9, 11, 13, 15};

const int encoderA_pin = 18;
const int encoderB_pin = 19;
const int encoderSW_pin = 22;

const int oled_sda_pin = 20;
const int oled_scl_pin = 21;

const int oled_i2c_addr = 0x3C;

Bounce keys[num_steps];
Bounce encoder_switch;

RotaryEncoder encoder(encoderB_pin, encoderA_pin, RotaryEncoder::LatchMode::FOUR3);
void checkEncoderPosition() {  encoder.tick(); } // just call tick() to check the state.
int encoder_pos = 0; // our encoder position state

Adafruit_SSD1306 display(dw, dh, &Wire, -1);


void setup() {
  // KEYS
  for (uint8_t i=0; i< num_steps; i++) {
    keys[i].attach( key_pins[i], INPUT_PULLUP);
  }

  // LEDS
  for (uint8_t i=0; i< num_steps; i++) {
    pinMode(led_pins[i], OUTPUT);
  }

  // ENCODER
  //pinMode(encoderSW_pin, INPUT_PULLUP);
  pinMode(encoderA_pin, INPUT_PULLUP);
  pinMode(encoderB_pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(encoderA_pin), checkEncoderPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderB_pin), checkEncoderPosition, CHANGE);
  encoder_switch.attach(encoderSW_pin, INPUT_PULLUP);

  // DISPLAY
  Wire.setSDA( oled_sda_pin );
  Wire.setSCL( oled_scl_pin );
  Wire.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, oled_i2c_addr)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay();
  display.display();  // must clear before display, otherwise shows adafruit logo

}

char keyspressed[num_steps+1];

void loop() 
{
  // KEYS
  for (uint8_t i=0; i< num_steps; i++) {
    keys[i].update();
    bool keypressed = keys[i].read() == LOW; // active low
    keyspressed[i] = keypressed ? '1' : '0';
    digitalWrite( led_pins[i], keypressed); // light up the key LEDs for yucks
  }
  // ENCODER
  encoder_switch.update();
  encoder.tick();
  int newPos = encoder.getPosition();
  if (encoder_pos != newPos) {
    Serial.print("Encoder:");
    Serial.print(newPos);
    Serial.print(" Direction:");
    Serial.println((int)(encoder.getDirection()));
    encoder_pos = newPos;
  }
  bool encoder_pressed = encoder_switch.read() == LOW;
 
  display.clearDisplay();
  display.setFont(&myfont);
  display.setCursor(0, 10);
//  display.setTextSize(2); // Draw 2X-scale text
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, 0);
  display.println(F("hello world"));

  //display.setFont(0); // go back to built-in font
  display.setFont(&myfont2);
  display.setTextSize(1);
  display.setCursor(0,45);
  display.print("enc:");
  display.setCursor(50, 45);
  display.print(encoder_pos);
  display.setCursor(110, 45);
  display.print(encoder_pressed ? "*" : ".");

  display.setCursor(0, 60);
  display.print("key:");
  display.setCursor(50, 60);
  display.print(keyspressed);
  display.display();
  
}
