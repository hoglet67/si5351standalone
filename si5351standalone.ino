#include <stdio.h>
#include <string.h>
#include <si5351.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <EasyButton.h>
#include <Encoder.h>
#include <EEPROM.h>

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

Si5351 si5351;

LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_E, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);

char buffer [256];
char *msg = buffer;

EasyButton b1(PIN_BUTTON1);

EasyButton b2(PIN_BUTTON2);

Encoder rotary(PIN_ROTARY_A, PIN_ROTARY_B);

#define NUM_CHANNELS 3

#define MAGIC 0x31415927

typedef struct {
  uint32_t magic;
  uint32_t count;
  uint32_t freq[NUM_CHANNELS];
  uint32_t step[NUM_CHANNELS];
} t_settings;

t_settings settings;

uint32_t *freq = &settings.freq[0];
uint32_t *step = &settings.step[0];

uint32_t default_freq[NUM_CHANNELS] = {
   4000000,
   1843200,
  50000000
};

uint32_t default_step[NUM_CHANNELS] = {
    100000,
     10000,
    100000
};

// Current channel
unsigned int channel = 0;

// Virtual display rows:
//     0123456789012345
// 0 - Clock Generator:   <<< top_row (upper position)
// 1 - 0 XX.XXXXXX MHz    <<< top_row + NUM_ROWS - 1
// 2 - 1>XX.XXXXXX MHz<
// 3 - 2 XX.XXXXXX MHz    <<< top_row (lower position)
// 4 - (c) 2021 Hoglet    <<< top_row + NUM_ROWS - 1

unsigned int top_row = 0;

#define NUM_ROWS 2

void print_frequency(unsigned int c) {
  uint32_t f = freq[c];
  if (f == 0) {
    lcd.print("Off          ");
  } else {
    char fstr[32];
    sprintf(fstr, "%2ld.%06ld MHz", f / 1000000, f % 1000000);
    lcd.print(fstr);
  }
}

void initialize_settings() {
    settings.magic = MAGIC;
    settings.count = 0;
    for (unsigned int i = 0; i < NUM_CHANNELS; i++) {
      settings.freq[i] = default_freq[i];
      settings.step[i] = default_step[i];
    }
}

void load_settings() {
  EEPROM.get(0, settings);
  if (settings.magic != MAGIC) {
    initialize_settings();
  }

}

void save_settings() {
  settings.count++;
  EEPROM.put(0, settings);
}


void update_display() {
  // Ensure the current channel is visible at all times
  if (top_row > channel + 1) {
    // Window is too far down, so channel is off the top
    // Position it so channel occpies the top physical row
    top_row = channel + 1;
  } else if (top_row < channel + 1 - (NUM_ROWS - 1)) {
    // Window is too far up, so channel is off the bottom
    // Position it so channel occpies the bottom physical row
    top_row = channel + 1 - (NUM_ROWS - 1);
  }
  // Render NUM_ROWS virtual rows
  for (unsigned int i = top_row; i < top_row + NUM_ROWS; i++) {
    lcd.setCursor(0, i - top_row);
    if (i == 0) {
      lcd.print("Clock Generator ");
    } else if (i > NUM_CHANNELS) {
      lcd.print("(c) 2021 Hoglet ");
    } else {
      unsigned int c = i - 1;
      lcd.print(c);
      lcd.print((c == channel) ? '>' : ' ');
      print_frequency(c);
      lcd.print((c == channel) ? '<' : ' ');
    }
  }
}

void set_frequency(unsigned int c, uint32_t f) {
  if (c >= 0 && c < NUM_CHANNELS && f >= 0 && f < 160000000) {
    if (f == 0) {
      if (freq[c] > 0) {
        si5351.output_enable((si5351_clock) c, 0);
      }
    } else {
      if (freq[c] == 0) {
        si5351.output_enable((si5351_clock) c, 1);
      }
      si5351.set_freq(f * 100ULL, (si5351_clock) c);
    }
    freq[c] = f;
  }
}

void update_rotary() {
  rotary.write(4 * freq[channel] / step[channel]);
}

void b1Pressed() {
  if (channel > 0) {
    channel--;
    update_rotary();
  } else {
    top_row = 0;
  }
  update_display();
}

void b2Pressed() {
  if (channel < NUM_CHANNELS - 1) {
    channel++;
    update_rotary();
  } else {
    top_row = NUM_CHANNELS;
  }
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
  update_display();
}

void setup(void)
{
  b1.begin();
  b1.onPressedFor(100, b1Pressed);
  b2.begin();
  b2.onPressedFor(100, b2Pressed);
  load_settings();
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
  int r;

  b1.read();
  b2.read();

  if (b1.isPressed() && b2.isPressed()) {
    lcd.clear();
    save_settings();
    lcd.print("Saved Settings");
    lcd.setCursor(0,1);
    lcd.print(settings.count);
    lcd.print(" EE Writes");
    delay(2000);
    update_display();
  }

  r = (rotary.read() / 4) * step[channel];
  if (r < 0) {
    rotary.write(0);
    r = 0;
  } else if (r > 99000000) {
    rotary.write(4 * r / step[channel]);
    r = 99000000;
  }
  if (((uint32_t) r) != freq[channel]) {
    set_frequency(channel, r);
    // TODO: optimise this case
    update_display();
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
