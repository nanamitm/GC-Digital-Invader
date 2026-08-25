/*
 * GC DIGITAL INVADER  --  Arduboy port
 *
 * original game design 1980 CASIO COMPUTER CO., LTD. (MG-880 "DIGITAL INVADER")
 * JavaScript version 2016-2020 Akebi   http://akebi.jp/gamecalc/
 * Arduboy port 2026
 *
 * Controls
 *   LEFT  / A : AIM   (sight 0..9 -> UFO mark -> 0 ...)
 *   RIGHT / B : FIRE
 *   title: UP/DOWN select level, A start
 */

#include <Arduboy2.h>
#include <ArduboyTones.h>
#include <EEPROM.h>

Arduboy2 arduboy;
ArduboyTones sound(Arduboy2Audio::enabled);

// ---------------------------------------------------------------- constants

#define MS_PER_FRAME 16                       // JS: dividingRatio = 1000/60
#define MS2F(ms)     ((uint16_t)((ms) / MS_PER_FRAME))

#define MY_LEFT      3                        // stock (planes)
#define DISP_LEN     8                        // 7 segment digits
#define TARGET_LEN   6                        // digits used by the invader field
#define TARGET_MAX   8

// segment bits
#define SEG_A 0x01
#define SEG_B 0x02
#define SEG_C 0x04
#define SEG_D 0x08
#define SEG_E 0x10
#define SEG_F 0x20
#define SEG_G 0x40

static const uint8_t PROGMEM digitSeg[10] = {
  SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F,        // 0
  SEG_B|SEG_C,                                // 1
  SEG_A|SEG_B|SEG_G|SEG_E|SEG_D,              // 2
  SEG_A|SEG_B|SEG_G|SEG_C|SEG_D,              // 3
  SEG_F|SEG_G|SEG_B|SEG_C,                    // 4
  SEG_A|SEG_F|SEG_G|SEG_C|SEG_D,              // 5
  SEG_A|SEG_F|SEG_G|SEG_E|SEG_C|SEG_D,        // 6
  SEG_A|SEG_B|SEG_C,                          // 7
  SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,  // 8
  SEG_A|SEG_B|SEG_C|SEG_D|SEG_F|SEG_G         // 9
};

// 'n' UFO mark / 'T' stock 3 / '=' stock 2 / '-' stock 1 and separator
static uint8_t segOf(char c) {
  if (c >= '0' && c <= '9') return pgm_read_byte(&digitSeg[c - '0']);
  switch (c) {
    case 'n': return SEG_G | SEG_E | SEG_C;   // UFO: lower 'n'
    case 'T': return SEG_A | SEG_G | SEG_D;
    case '=': return SEG_G | SEG_D;
    case '-': return SEG_G;
    default : return 0;
  }
}

// ---------------------------------------------------------------- levels

struct Level {
  const char *name;
  uint8_t  targetLeft;                        // invaders per stage
  uint8_t  shotLeft;                          // shots per stage
  uint16_t wait1;                             // ms per step at pattern 1
  uint16_t wait2;                             // ms per step at pattern 9
};

static const Level levels[4] = {
  { "EASY",   16, 30, 1500, 600 },
  { "NORMAL", 16, 30, 1200, 390 },
  { "HARD",   16, 30,  800, 320 },
  { "DEATH",  32, 35,  450, 200 }
};
static uint8_t levelNo = 1;

// ---------------------------------------------------------------- sound

// tone/duration pairs, closest thing to the beeper of the original calculator

static const uint16_t hitSeq[]   PROGMEM = { 1760,35, 1245,35, 880,45, TONES_END };
static const uint16_t errorSeq[] PROGMEM = { 165,50, 110,90, TONES_END };
static const uint16_t ufoSeq[]   PROGMEM = { 1568,30, 1976,30, 1568,30, 1976,30,
                                             2349,60, 1568,50, 1047,80, TONES_END };
static const uint16_t clearSeq[] PROGMEM = { 1047,90, 1319,90, 1568,90, 2093,240, TONES_END };
static const uint16_t missSeq[]  PROGMEM = { 880,60, 659,60, 440,90, 330,240, TONES_END };
static const uint16_t overSeq[]  PROGMEM = { 392,180, 349,180, 294,180, 262,180,
                                             196,500, TONES_END };
static const uint16_t startSeq[] PROGMEM = { 880,70, 1319,70, 1760,140, TONES_END };

static void seAim()      { sound.tone(2637, 15); }   // sight click
static void seHit()      { sound.tones(hitSeq); }
static void seError()    { sound.tones(errorSeq); }
static void seUfo()      { sound.tones(ufoSeq); }
static void seClear()    { sound.tones(clearSeq); }
static void seMiss()     { sound.tones(missSeq); }
static void seGameOver() { sound.tones(overSeq); }
static void seStart()    { sound.tones(startSeq); }

// ---------------------------------------------------------------- xorshift
// same generator as the JavaScript version

static uint32_t xs_x, xs_y, xs_z, xs_w;

static void xorShiftInit(uint32_t seed) {
  xs_x = 123456789UL; xs_y = 362436069UL; xs_z = 521288629UL; xs_w = seed;
}

static uint32_t xorShiftNext() {
  uint32_t t = xs_x ^ (xs_x << 11);
  xs_x = xs_y; xs_y = xs_z; xs_z = xs_w;
  xs_w = (xs_w ^ (xs_w >> 19)) ^ (t ^ (t >> 8));
  return xs_w;
}

static uint8_t xorShiftRand10() {
  uint32_t v = xorShiftNext() + 0x80000000UL;      // (w + 2^31) / 2^32 * 10
  return (uint8_t)(((uint64_t)v * 10ULL) >> 32);
}

// ---------------------------------------------------------------- state

enum GameState : uint8_t {
  ST_TITLE, ST_HISCORE, ST_SHOTNUM, ST_PLAY,
  ST_MISSWAIT, ST_CLEARWAIT, ST_OVER
};

static GameState state = ST_TITLE;

static uint32_t score, highScore;
static uint8_t  part, pattern, loopCnt;
static uint8_t  num;                          // sight 0..10 (10 = UFO)
static uint8_t  stock;
static char     targetString[TARGET_MAX + 1];
static uint8_t  targetSpace;
static uint8_t  targetCount, shotCount, hitCount;
static uint16_t sum, sumOld;
static uint8_t  ufoStack;
static uint16_t moveCount, moveCountDown;
static uint8_t  plusCount;
static bool     onKey, clearFlg;
static uint16_t waitFrames;                   // state timer
static uint16_t pauseFrames;                  // UFO hit pause
static uint16_t blankFrames;                  // display blink
static uint16_t clearSeFrames;                // delayed stage clear sound
static char     dispBuf[DISP_LEN + 1];
static bool     dispDirect;                   // dispBuf holds a full message

#define lv() (&levels[levelNo])

// ---------------------------------------------------------------- eeprom

#define EE_ADDR  (EEPROM_STORAGE_SPACE_START + 64)
#define EE_MAGIC 0x4744

static void saveConfig() {
  uint16_t magic = EE_MAGIC;
  EEPROM.put(EE_ADDR, magic);
  EEPROM.put(EE_ADDR + 2, highScore);
  EEPROM.put(EE_ADDR + 6, levelNo);
}

static void loadConfig() {
  uint16_t magic;
  EEPROM.get(EE_ADDR, magic);
  if (magic == EE_MAGIC) {
    EEPROM.get(EE_ADDR + 2, highScore);
    EEPROM.get(EE_ADDR + 6, levelNo);
    if (levelNo > 3) levelNo = 1;
  } else {
    highScore = 0;
    levelNo = 1;
  }
}

// ---------------------------------------------------------------- drawing

// one 7 segment digit, cell 11 x 22, segment thickness 2
static void drawDigit(int8_t x, int8_t y, uint8_t seg) {
  const uint8_t w = 11, h = 22, t = 2;
  if (seg & SEG_A) arduboy.fillRect(x + 2,     y,             w - 4, t);
  if (seg & SEG_G) arduboy.fillRect(x + 2,     y + h / 2 - 1, w - 4, t);
  if (seg & SEG_D) arduboy.fillRect(x + 2,     y + h - t,     w - 4, t);
  if (seg & SEG_F) arduboy.fillRect(x,         y + 2,         t, h / 2 - 3);
  if (seg & SEG_B) arduboy.fillRect(x + w - t, y + 2,         t, h / 2 - 3);
  if (seg & SEG_E) arduboy.fillRect(x,         y + h / 2 + 1, t, h / 2 - 3);
  if (seg & SEG_C) arduboy.fillRect(x + w - t, y + h / 2 + 1, t, h / 2 - 3);
}

static void drawSegLine(const char *s) {
  for (uint8_t i = 0; i < DISP_LEN; i++) drawDigit(4 + i * 15, 26, segOf(s[i]));
}

// build the 8 digit line from the live game state
static void buildPlayLine(char *out) {
  out[0] = (num == 10) ? 'n' : (char)('0' + num);
  out[1] = (stock >= 3) ? 'T' : (stock == 2) ? '=' : '-';
  for (uint8_t i = 0; i < TARGET_LEN; i++) out[2 + i] = ' ';
  uint8_t len = strlen(targetString);
  for (uint8_t i = 0; i < len && (uint8_t)(targetSpace + i) < TARGET_LEN; i++) {
    out[2 + targetSpace + i] = targetString[i];
  }
  out[DISP_LEN] = '\0';
}

static void setSixDigits(uint32_t value, char *out) {
  uint32_t v = value % 1000000UL;
  for (int8_t i = 5; i >= 0; i--) { out[i] = '0' + (char)(v % 10); v /= 10; }
  out[6] = '\0';
}

// "  " + 6 digit score, or "<pattern>-<score>" like the original
static void setScoreLine(char lead) {
  char body[7];
  setSixDigits(score, body);
  if (lead) { dispBuf[0] = lead; dispBuf[1] = '-'; }
  else      { dispBuf[0] = ' ';  dispBuf[1] = ' '; }
  memcpy(dispBuf + 2, body, 7);
  dispDirect = true;
}

// ---------------------------------------------------------------- game flow

static void setMoveCount() {
  int32_t mc = (int32_t)lv()->wait1
             - ((int32_t)(lv()->wait1 - lv()->wait2) / 8) * (pattern - 1);
  mc = mc * 10 / (10 + loopCnt);
  mc /= MS_PER_FRAME;
  if (mc < 1) mc = 1;
  moveCount = (uint16_t)mc;
}

static void startStage(bool next) {
  num = 0;
  targetString[0] = '\0';

  if (next) {
    targetCount = 0; hitCount = 0; sum = 0; sumOld = 0;
    ufoStack = 0; stock = MY_LEFT; shotCount = 0;
    if (++pattern > 9) {
      pattern = 1;
      if (++part > 2) { part = 1; loopCnt++; }
    }
  } else {
    targetCount = hitCount;
  }
  clearFlg = false;

  targetSpace = (part == 1) ? 6 : 5;
  setMoveCount();
  moveCountDown = 0;
  plusCount = 0;
  onKey = false;
  pauseFrames = 0;
  dispDirect = false;
  state = ST_PLAY;
}

static void gameStart() {
  xorShiftInit(arduboy.generateRandomSeed());
  for (uint8_t i = 0; i < 200; i++) xorShiftNext();

  score = 0;
  part = 1; pattern = 1; loopCnt = 0;
  num = 0;
  stock = MY_LEFT;
  targetString[0] = '\0';
  targetSpace = 6;
  targetCount = 0; shotCount = 0; hitCount = 0;
  sum = 0; sumOld = 0; ufoStack = 0;
  plusCount = 0; onKey = false; clearFlg = false;
  pauseFrames = 0; blankFrames = 0; clearSeFrames = 0;
  setMoveCount();

  seStart();

  char body[7];
  setSixDigits(highScore, body);
  dispBuf[0] = ' '; dispBuf[1] = ' ';
  memcpy(dispBuf + 2, body, 7);
  dispDirect = true;
  waitFrames = MS2F(2000);
  state = ST_HISCORE;
}

static void updateHighScore() {
  if (score > highScore) {
    highScore = score;
    saveConfig();
  }
}

static void gameOver() {
  setScoreLine('0' + pattern);
  updateHighScore();
  seGameOver();
  state = ST_OVER;
}

static void missPlane() {
  if (--stock > 0) {
    setScoreLine(0);
    seMiss();
    waitFrames = MS2F(2200);
    state = ST_MISSWAIT;
  } else {
    gameOver();
  }
}

static void stageClear(uint16_t delayMs) {
  clearFlg = true;
  setScoreLine('0' + pattern);
  clearSeFrames = MS2F(delayMs) + 1;
  waitFrames = MS2F(2000 + delayMs);
  state = ST_CLEARWAIT;
}

// small grace period when a key is pressed (JS: mcdP)
static void keyGrace() {
  if (!onKey) return;
  onKey = false;
  if (plusCount++ < 3) moveCountDown += 2;
}

static void attack() {
  char checkChar = (num == 10) ? 'n' : (char)('0' + num);
  char *p = strchr(targetString, checkChar);
  shotCount++;

  if (p) {                                          // hit
    uint8_t idx = (uint8_t)(p - targetString);
    bool hitUfo = (num == 10);
    if (num >= 1 && num <= 9) sum += num;

    score += (uint32_t)(idx + 1 + targetSpace) * 10UL * part;
    if (hitUfo) score += 300;

    if (sum > sumOld && sum % 10 == 0) ufoStack++;  // every 10 points of sum: UFO
    sumOld = sum;

    memmove(p, p + 1, strlen(p));                   // remove that invader
    targetSpace++;

    uint16_t clearDelay = 200;
    if (hitUfo) {
      blankFrames = MS2F(360);
      seUfo();
      clearDelay = 400;
      pauseFrames = MS2F(400);
    } else {
      blankFrames = MS2F(80);
      seHit();
    }
    updateHighScore();

    if (++hitCount >= lv()->targetLeft) {
      stageClear(clearDelay);
      return;
    }
  } else {                                          // shot missed
    blankFrames = MS2F(80);
    seError();
  }

  if (shotCount >= lv()->shotLeft) gameOver();      // out of ammo
}

static void updatePlay() {
  if (pauseFrames) { pauseFrames--; return; }

  if (arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(A_BUTTON)) {
    onKey = true;
    keyGrace();
    if (++num > 10) num = 0;
    seAim();
  } else if (arduboy.justPressed(RIGHT_BUTTON) || arduboy.justPressed(B_BUTTON)) {
    onKey = true;
    keyGrace();
    attack();
    if (state != ST_PLAY) return;
  }

  if (moveCountDown && --moveCountDown > 0) return;
  moveCountDown = moveCount;
  plusCount = 0;

  if (++targetCount <= lv()->targetLeft) {          // a new invader appears
    uint8_t len = strlen(targetString);
    if (len < TARGET_MAX) {
      if (ufoStack) { targetString[len] = 'n'; ufoStack--; }
      else          { targetString[len] = '0' + xorShiftRand10(); }
      targetString[len + 1] = '\0';
    }
  }

  if (targetSpace > 0) targetSpace--;               // the row steps forward
  else                 missPlane();                 // it reached the base
}

// ---------------------------------------------------------------- title

static void drawTitle() {
  arduboy.setCursor(10, 2);
  arduboy.print(F("DIGITAL INVADER"));
  arduboy.setCursor(4, 14);
  arduboy.print(F("GAME CALCULATOR 1980"));

  arduboy.setCursor(22, 28);
  arduboy.print(F("LEVEL: "));
  arduboy.print(levels[levelNo].name);
  arduboy.setCursor(10, 37);
  arduboy.print(F("HI SCORE: "));
  arduboy.print(highScore);

  arduboy.setCursor(1, 47);
  arduboy.print(F("SOUND: "));
  arduboy.print(arduboy.audio.enabled() ? F("ON ") : F("OFF"));

  arduboy.setCursor(1, 56);
  arduboy.print(F("A:START B:SOUND UD:LV"));
}

static void updateTitle() {
  if (arduboy.justPressed(UP_BUTTON)   && levelNo > 0) { levelNo--; saveConfig(); }
  if (arduboy.justPressed(DOWN_BUTTON) && levelNo < 3) { levelNo++; saveConfig(); }
  if (arduboy.justPressed(B_BUTTON)) {                 // sound on / off
    arduboy.audio.toggle();
    arduboy.audio.saveOnOff();
    if (arduboy.audio.enabled()) seAim();
  }
  if (arduboy.justPressed(A_BUTTON)) gameStart();
}

// ---------------------------------------------------------------- arduino

void setup() {
  arduboy.begin();
  arduboy.setFrameRate(60);
  arduboy.initRandomSeed();
  // drive both speaker pins: twice as loud on real hardware
  sound.volumeMode(VOLUME_ALWAYS_HIGH);
  loadConfig();
  arduboy.clear();
}

void loop() {
  if (!arduboy.nextFrame()) return;
  arduboy.pollButtons();

  if (blankFrames) blankFrames--;
  if (clearSeFrames && --clearSeFrames == 0) seClear();

  switch (state) {
    case ST_TITLE:
      updateTitle();
      break;

    case ST_HISCORE:
      if (--waitFrames == 0) {                      // then "invaders - shots"
        dispBuf[0] = ' '; dispBuf[1] = ' '; dispBuf[2] = ' ';
        dispBuf[3] = '0' + lv()->targetLeft / 10;
        dispBuf[4] = '0' + lv()->targetLeft % 10;
        dispBuf[5] = '-';
        dispBuf[6] = '0' + lv()->shotLeft / 10;
        dispBuf[7] = '0' + lv()->shotLeft % 10;
        dispBuf[8] = '\0';
        waitFrames = MS2F(2000);
        state = ST_SHOTNUM;
      }
      break;

    case ST_SHOTNUM:
      if (--waitFrames == 0) {
        dispDirect = false;
        moveCountDown = 0;
        state = ST_PLAY;
      }
      break;

    case ST_PLAY:
      updatePlay();
      break;

    case ST_MISSWAIT:
      if (--waitFrames == 0) startStage(false);
      break;

    case ST_CLEARWAIT:
      if (--waitFrames == 0) startStage(true);
      break;

    case ST_OVER:
      if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) state = ST_TITLE;
      break;
  }

  // ---- draw

  arduboy.clear();

  if (state == ST_TITLE) {
    drawTitle();
  } else {
    arduboy.setCursor(0, 0);
    arduboy.print(F("HI:"));
    arduboy.print(highScore);
    arduboy.setCursor(66, 0);
    arduboy.print(F("SC:"));
    arduboy.print(score);

    arduboy.drawRect(0, 22, 128, 30);

    if (!blankFrames) {
      if (dispDirect) {
        drawSegLine(dispBuf);
      } else {
        char line[DISP_LEN + 1];
        buildPlayLine(line);
        drawSegLine(line);
      }
    }

    if (state == ST_OVER) {
      arduboy.setCursor(34, 56);
      arduboy.print(F("GAME OVER"));
    } else if (state == ST_PLAY) {
      arduboy.setCursor(0, 56);
      arduboy.print(F("SHOT:"));
      arduboy.print(lv()->shotLeft - shotCount);
      arduboy.setCursor(70, 56);
      arduboy.print(F("ST:"));
      arduboy.print(part);
      arduboy.print('-');
      arduboy.print(pattern);
    }
  }

  arduboy.display();
}
