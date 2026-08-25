// Minimal Arduboy2 / ArduboyTones / EEPROM stubs so the sketch logic can be
// compiled and exercised on the host for testing.
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>

#define PROGMEM
#define F(x) (x)
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#define TONES_END 0x8000
#define VOLUME_ALWAYS_HIGH 2

#define A_BUTTON     0x08
#define B_BUTTON     0x04
#define UP_BUTTON    0x80
#define DOWN_BUTTON  0x10
#define LEFT_BUTTON  0x20
#define RIGHT_BUTTON 0x40

#define EEPROM_STORAGE_SPACE_START 16

// ---- pixel buffer -------------------------------------------------------

extern uint8_t hostScreen[64][128];

// ---- buttons ------------------------------------------------------------

extern uint8_t hostJustPressed;      // set by the driver, cleared by pollButtons

// ---- sound log ----------------------------------------------------------

extern std::string hostLastSound;

struct Arduboy2Audio {
  static bool on;
  static bool enabled() { return on; }
  void toggle() { on = !on; }
  void saveOnOff() {}
};

class Arduboy2 {
public:
  Arduboy2Audio audio;
  void begin() {}
  void clear() { memset(hostScreen, 0, sizeof(hostScreen)); }
  void display() {}
  void setFrameRate(uint8_t) {}
  void initRandomSeed() {}
  uint32_t generateRandomSeed() { return 12345678UL; }
  bool nextFrame() { return true; }
  void pollButtons() {}
  bool justPressed(uint8_t b) { return (hostJustPressed & b) != 0; }
  void fillRect(int x, int y, int w, int h) {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++)
        if (i >= 0 && i < 128 && j >= 0 && j < 64) hostScreen[j][i] = 1;
  }
  void drawRect(int, int, int, int) {}
  void setCursor(int, int) {}
  void print(const char *) {}
  void print(char) {}
  void print(unsigned long) {}
  void print(unsigned int) {}
  void print(int) {}
};

class ArduboyTones {
public:
  ArduboyTones(bool (*)()) {}
  void tone(uint16_t f, uint16_t) { hostLastSound = "tone" + std::to_string(f); }
  void tones(const uint16_t *) { hostLastSound = "seq"; }
  void volumeMode(uint8_t) {}
};

class EEPROMClass {
public:
  uint8_t data[1024] = {0};
  template <typename T> void put(int addr, const T &v) { memcpy(data + addr, &v, sizeof(T)); }
  template <typename T> void get(int addr, T &v) { memcpy(&v, data + addr, sizeof(T)); }
};
extern EEPROMClass EEPROM;
