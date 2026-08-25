// Host side driver: runs the sketch logic headless and checks the rules
// against the behaviour of the original JavaScript version.
#include "arduboy_stub.h"

uint8_t     hostScreen[64][128];
uint8_t     hostJustPressed = 0;
std::string hostLastSound;
EEPROMClass EEPROM;
bool        Arduboy2Audio::on = true;

#include "../DigitalInvader/DigitalInvader.ino"

static int failures = 0;

static void check(bool cond, const char *what) {
  printf("[%s] %s\n", cond ? " OK " : "FAIL", what);
  if (!cond) failures++;
}

static void frame(uint8_t press = 0) {
  hostJustPressed = press;
  loop();
  hostJustPressed = 0;
}

static void frames(int n, uint8_t press = 0) {
  for (int i = 0; i < n; i++) frame(i == 0 ? press : 0);
}

static std::string line() {
  char buf[DISP_LEN + 1];
  buildPlayLine(buf);
  return std::string(buf);
}

int main() {
  setup();

  // ---- title -----------------------------------------------------------
  frame();
  check(state == ST_TITLE, "starts on the title screen");
  frame(DOWN_BUTTON);
  check(levelNo == 2, "DOWN selects the next level (HARD)");
  frame(UP_BUTTON);
  check(levelNo == 1, "UP goes back to NORMAL");

  bool sndWas = arduboy.audio.enabled();
  frame(B_BUTTON);
  check(arduboy.audio.enabled() != sndWas, "B toggles the sound on the title");
  frame(B_BUTTON);
  check(arduboy.audio.enabled() == sndWas, "and toggles it back");
  check(state == ST_TITLE, "B no longer starts the game");

  frame(A_BUTTON);
  check(state == ST_HISCORE, "A starts the game, high score is shown first");
  frames(MS2F(2000));
  check(state == ST_SHOTNUM, "then the invader/shot count is shown");
  check(std::string(dispBuf) == "   16-30", "shows '   16-30' on NORMAL");
  frames(MS2F(2000));
  check(state == ST_PLAY, "the game starts");
  check(line() == "0T      ", "display: sight 0, 3 planes, empty field");

  // ---- invaders march in ----------------------------------------------
  frames(moveCount);
  check(line()[7] != ' ', "an invader appears at the right end");
  std::string first = line();
  frames(moveCount);
  check(line()[6] == first[7], "the row steps one digit to the left");

  // ---- aiming ----------------------------------------------------------
  uint8_t before = num;
  hostLastSound.clear();
  frame(LEFT_BUTTON);
  check(num == (uint8_t)(before + 1), "AIM raises the sight");
  check(!hostLastSound.empty(), "AIM makes a click");
  for (int i = 0; i < 10; i++) frame(LEFT_BUTTON);
  check(num <= 10, "the sight wraps around through the UFO mark");

  // ---- firing at the nearest matching invader ---------------------------
  {
    // line up the sight with the leftmost invader, then fire
    char want = targetString[0];
    while (((num == 10) ? 'n' : (char)('0' + num)) != want) frame(LEFT_BUTTON);
    uint32_t sc = score;
    uint8_t  hits = hitCount, shots = shotCount, len = strlen(targetString);
    hostLastSound.clear();
    frame(RIGHT_BUTTON);
    check(!hostLastSound.empty(), "a hit makes a sound");
    check(strlen(targetString) == (size_t)(len - 1), "FIRE removes the matching invader");
    check(hitCount == hits + 1, "the hit counter goes up");
    check(shotCount == shots + 1, "a shot is used");
    check(score > sc, "the score goes up");
  }

  // ---- firing at nothing ------------------------------------------------
  {
    // pick a sight value that is not on the field
    for (uint8_t v = 0; v <= 9; v++) {
      if (!strchr(targetString, (char)('0' + v))) {
        while (num != v) frame(LEFT_BUTTON);
        break;
      }
    }
    uint32_t sc = score;
    uint8_t  shots = shotCount, len = strlen(targetString);
    hostLastSound.clear();
    frame(RIGHT_BUTTON);
    check(!hostLastSound.empty(), "a shot that misses buzzes");
    check(score == sc, "a shot that misses scores nothing");
    check(strlen(targetString) == len, "and removes no invader");
    check(shotCount == shots + 1, "but still uses a shot");
  }

  // ---- an invader reaching the base costs a plane ------------------------
  {
    uint8_t planes = stock;
    int guard = 0;
    while (state == ST_PLAY && guard++ < 100000) frame();   // never aim, never fire
    check(state == ST_MISSWAIT, "letting the row through ends the round");
    check(!hostLastSound.empty(), "losing a plane makes a sound");
    check(stock == planes - 1, "one plane is lost");
    frames(MS2F(2200));
    check(state == ST_PLAY, "play resumes after the miss");
    check(targetSpace == 6, "the field is reset to the far end");
  }

  // ---- game over on the last plane ---------------------------------------
  {
    int guard = 0;
    while (state != ST_OVER && guard++ < 400000) frame();
    check(state == ST_OVER, "the game ends when the last plane is lost");
    check(dispBuf[1] == '-', "the final display is '<pattern>-<score>'");
    check(highScore == score % 1000000UL || highScore >= score, "the high score is kept");
  }

  // ---- running out of ammunition ------------------------------------------
  {
    frame(A_BUTTON);                       // back to the title
    frame(A_BUTTON);                       // start again
    frames(MS2F(2000));
    frames(MS2F(2000));
    check(state == ST_PLAY, "a second game starts");
    int guard = 0;
    while (state == ST_PLAY && shotCount < lv()->shotLeft && guard++ < 400000) {
      // fire at a digit that is almost never there, to burn shots
      frame(RIGHT_BUTTON);
      frame();
    }
    check(shotCount <= lv()->shotLeft, "the shot counter never passes the limit");
  }

  // ---- stage clear --------------------------------------------------------
  {
    // restart and shoot everything down with an ideal player
    state = ST_TITLE;
    frame(A_BUTTON);
    frames(MS2F(2000));
    frames(MS2F(2000));
    check(state == ST_PLAY, "a fresh game for the clear test");
    uint8_t startPattern = pattern;
    int guard = 0;
    while (state == ST_PLAY && guard++ < 400000) {
      if (targetString[0]) {
        char want = targetString[0];
        if (((num == 10) ? 'n' : (char)('0' + num)) == want) frame(RIGHT_BUTTON);
        else                                                 frame(LEFT_BUTTON);
      } else {
        frame();
      }
    }
    check(state == ST_CLEARWAIT, "clearing every invader clears the stage");
    frames(MS2F(2000 + 400) + 2);
    check(state == ST_PLAY, "the next stage starts");
    check(pattern == startPattern + 1, "the pattern advances");
    check(shotCount == 0 && hitCount == 0, "shots and hits are reset for the new stage");
  }

  // ---- UFO ---------------------------------------------------------------
  {
    // the UFO is queued when the sum of the numbers shot crosses a multiple of 10
    sum = 8; sumOld = 8; ufoStack = 0;
    strcpy(targetString, "2");
    targetSpace = 5;
    num = 2;
    state = ST_PLAY; clearFlg = false; pauseFrames = 0;
    hitCount = 0; shotCount = 0;
    frame(RIGHT_BUTTON);
    check(sum == 10 && ufoStack == 1, "a UFO is queued when the sum reaches 10");

    // and it takes the next slot on the field
    uint8_t len = strlen(targetString);
    targetCount = 0;
    moveCountDown = 1;
    frame();
    check(strlen(targetString) == (size_t)(len + 1) && targetString[len] == 'n',
          "the queued UFO appears on the field");

    // hitting it with the UFO sight is worth 300 extra
    strcpy(targetString, "n");
    targetSpace = 5;
    num = 10;
    uint32_t sc = score;
    pauseFrames = 0;
    hostLastSound.clear();
    frame(RIGHT_BUTTON);
    check(hostLastSound == "seq", "the UFO has its own warble");
    check(score - sc >= 300, "the UFO is worth 300 points on top of the position score");
  }

  printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL CHECKS PASSED",
         failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
