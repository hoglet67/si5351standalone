#include <stdio.h>
#include <string.h>
#include <si5351.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <EasyButton.h>

#define DEFAULT_FREQ 32000000

// Pin Assignments (for the Teensy 3.5 on the Breadboard)

#define PIN_LCD_RS  2
#define PIN_LCD_E   4
#define PIN_LCD_D4  9
#define PIN_LCD_D5 10
#define PIN_LCD_D6 11
#define PIN_LCD_D7 12
#define PIN_BUTTON1 34
#define PIN_BUTTON2 33

#define AUTO_DELAY 1000
#define AUTO_RATE 100
#define FREQ_STEP 100000

typedef void (*callback_t)();

Si5351 si5351;

LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_E, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);

char buffer [256];
char *msg = buffer;

EasyButton b1(PIN_BUTTON1);
int b1repeat = AUTO_DELAY;

EasyButton b2(PIN_BUTTON2);
int b2repeat = AUTO_DELAY;

uint32_t freq;

void print_frequency(uint32_t f) {
  char fstr[32];
  sprintf(fstr, "%ld.%06ld MHz  ", f / 1000000, f % 1000000);
  lcd.setCursor(0, 1);
  lcd.print(fstr);
}

void set_frequency(uint32_t f) {
  if (f >= 8000 && f <= 160000000) {
    si5351.set_freq(f * 100ULL, SI5351_CLK1);
    print_frequency(f);
    freq = f;
  }
}

void b1Pressed() {
  set_frequency(freq - FREQ_STEP);
}

void b2Pressed() {
  set_frequency(freq + FREQ_STEP);
}

void handle_message(char *msg) {
  Serial.print("Si5351 received message: " );
  Serial.println(msg);
  set_frequency(atoi(msg));
}

void setup(void)
{
  b1.begin();
  b1.onPressed(b1Pressed);
  b2.begin();
  b2.onPressed(b2Pressed);
  lcd.begin(16, 2);
  lcd.print("Clock Generator");
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  set_frequency(DEFAULT_FREQ);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA);
}

void loop(void) {
  b1.read();
  if (b1.isPressed()) {
    if (b1.pressedFor(b1repeat)) {
      b1Pressed();
      b1repeat += AUTO_RATE;
    }
  } else {
    b1repeat = AUTO_DELAY;
  }

  b2.read();
  if (b2.isPressed()) {
    if (b2.pressedFor(b2repeat)) {
      b2Pressed();
      b2repeat += AUTO_RATE;
    }
  } else {
    b2repeat = AUTO_DELAY;
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
