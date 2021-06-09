#include <stdio.h>
#include <string.h>
#include <si5351.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <EasyButton.h>
#include <Encoder.h>

// Pin Assignments (for the Teensy 3.5 on the Breadboard)
#define PIN_LCD_RS     2
#define PIN_LCD_E      4
#define PIN_LCD_D4     9
#define PIN_LCD_D5    10
#define PIN_LCD_D6    11
#define PIN_LCD_D7    12
#define PIN_ROTARY_A  32
#define PIN_ROTARY_B  31
#define PIN_BUTTON1   34
#define PIN_BUTTON2   33

// Auto Repeat Parameters
//#define AUTO_DELAY  1000
//#define AUTO_RATE    100

Si5351 si5351;

LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_E, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);

char buffer [256];
char *msg = buffer;

EasyButton b1(PIN_BUTTON1);
//int b1repeat = AUTO_DELAY;

EasyButton b2(PIN_BUTTON2);
//int b2repeat = AUTO_DELAY;

Encoder rotary(PIN_ROTARY_A, PIN_ROTARY_B);

uint32_t freq[] = {
  4000000,
  1843200
};

int step[] = {
  100000,
  10000
};

unsigned int channel = 0;

#define NUM_CHANNELS (sizeof(freq) / sizeof(uint32_t))

void print_frequency(unsigned int c) {
  char fstr[32];
  uint32_t f = freq[c];
  sprintf(fstr, "%2ld.%06ld MHz", f / 1000000, f % 1000000);
  lcd.setCursor(2, c);
  lcd.print(fstr);
}

void update_display() {
  for (unsigned int i = 0; i < NUM_CHANNELS; i++) {
    lcd.setCursor(0, i);
    lcd.print(i);
    lcd.print((i == channel) ? '>' : ' ');
    print_frequency(i);
    lcd.print((i == channel) ? '<' : ' ');
  }
}

void set_frequency(unsigned int c, uint32_t f) {
  if (c >= 0 && c < NUM_CHANNELS && f >= 8000 && f < 100000000) {
    si5351.set_freq(f * 100ULL, (si5351_clock) c);
    freq[c] = f;
    print_frequency(c);
  }
}

void update_rotary() {
  rotary.write(4 * freq[channel] / step[channel]);
}

void b1Pressed() {
  if (channel > 0) {
    channel--;
  }
  update_rotary();
  update_display();
}

void b2Pressed() {
  if (channel < NUM_CHANNELS - 1) {
    channel++;
  }
  update_rotary();
  update_display();
}

void handle_message(char *msg) {
  Serial.print("Si5351 received message: " );
  Serial.println(msg);
  // Format is <number> or <channel>:<number>
  if (msg[1] == ':') {
    set_frequency(msg[0] - '0', atoi(msg + 2));
  } else {
    set_frequency(channel, atoi(msg));
  }
}

void setup(void)
{
  b1.begin();
  b1.onPressed(b1Pressed);
  b2.begin();
  b2.onPressed(b2Pressed);
  lcd.begin(16, 2);
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  for (unsigned int c = 0; c < NUM_CHANNELS; c++) {
    set_frequency(c, freq[c]);
    si5351.drive_strength((si5351_clock) c, SI5351_DRIVE_8MA);
  }
  channel = 0;
  update_rotary();
  update_display();
}

void loop(void) {
  uint32_t r;

  b1.read();
  b2.read();

#if 0
  if (b1.isPressed()) {
    if (b1.pressedFor(b1repeat)) {
      b1Pressed();
      b1repeat += AUTO_RATE;
    }
  } else {
    b1repeat = AUTO_DELAY;
  }

  if (b2.isPressed()) {
    if (b2.pressedFor(b2repeat)) {
      b2Pressed();
      b2repeat += AUTO_RATE;
    }
  } else {
    b2repeat = AUTO_DELAY;
  }
#endif

  r = (rotary.read() / 4) * step[channel];
  if (r != freq[channel]) {
    set_frequency(channel, r);
  }

  if ( Serial.available() ) {
    char c = Serial.read();
    if ( c != 10 ) {
      *msg++ = c;
    } else {
      // end of message, so null terminate and process
      *msg = 0;
      msg = buffer;
      if ( msg) {
        handle_message(msg);
      }
      *msg = 0;
    }
  }
}
