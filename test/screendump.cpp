// Renders one representative frame of the 8 digit display as ASCII art,
// to eyeball the 7 segment glyphs without hardware.
#include "arduboy_stub.h"

uint8_t     hostScreen[64][128];
uint8_t     hostJustPressed = 0;
std::string hostLastSound;
EEPROMClass EEPROM;
bool        Arduboy2Audio::on = true;

#include "../DigitalInvader/DigitalInvader.ino"

static void dump(const char *caption, const char *text) {
  memset(hostScreen, 0, sizeof(hostScreen));
  drawSegLine(text);
  printf("--- %s : \"%s\"\n", caption, text);
  for (int y = 24; y < 50; y++) {
    for (int x = 0; x < 128; x++) putchar(hostScreen[y][x] ? '#' : '.');
    putchar('\n');
  }
  putchar('\n');
}

int main() {
  dump("digits", "01234567");
  dump("digits", "89  n T=");
  dump("in play (sight 3, 3 planes)", "3T  4 71");
  dump("stage clear / game over", "1-001230");
  return 0;
}
