# GC DIGITAL INVADER (Arduboy port)

An Arduboy port of **DIGITAL INVADER**, the game built into Casio's MG-880 game
calculator (1980). The rules follow [Akebi's JavaScript version](https://akebi.jp/gamecalc/)
(see the [write-up](https://qiita.com/akebi_mh/items/de4564ce9e02fed6fd7e)).
The resulting `.hex` runs as-is in [nanamitm/arduboy-emu](https://github.com/nanamitm/arduboy-emu).

## Display

The calculator's 8-digit seven-segment display is reproduced on the Arduboy's
128x64 OLED.

```
 digit0   digit1     digit2 .. digit7
 [sight]  [stock]    [invader row (6 digits)]
    3        T          4  71
```

- digit 0: the sight (0-9, `n` shaped glyph = UFO sight)
- digit 1: remaining planes (three bars = 3, two bars = 2, one bar = 1)
- digits 2-7: the number invaders closing in from the right

## Controls

| Key | Action |
|-----|--------|
| LEFT / A | AIM (step the sight 0 - 9 - UFO - 0) |
| RIGHT / B | FIRE (shoot the invader matching the sight) |
| UP / DOWN (title) | Select level |
| A (title) | Start the game |
| B (title) | Sound on / off (stored in EEPROM) |
| A / B (game over) | Back to the title |

## Rules

- Invaders appear at the right edge at a fixed interval and step one digit to
  the left each time.
- FIRE destroys the invader matching the sight. A shot that matches nothing is
  still spent (error tone).
- Score = (display position counted from the left x 10) x part. **The closer an
  invader gets, the less it is worth - shoot early, from far away, to score.**
- Every time the sum of the numbers you shoot passes a multiple of 10, a **UFO**
  is queued. Hitting it with the UFO sight is worth **+300**.
- An invader that passes the left edge (digit 2) costs a plane. At zero planes
  the game is over.
- Running out of ammunition also ends the game.
- A stage is cleared by destroying the required number of invaders (16 by
  default). Each pattern from 1 to 9 speeds the advance up; after pattern 9 comes
  part 2, where invaders start one digit closer, and after that the loop counter
  rises and everything gets faster still.

## Sound

Played through ArduboyTones:

| Event | Sound |
|-------|-------|
| AIM | Short click |
| Hit | Three descending notes |
| Missed shot | Low buzz |
| UFO destroyed | Fast warble |
| Stage clear | Rising fanfare |
| Plane lost | Four descending notes |
| Game over | Five notes sinking low |
| Game start | Three rising notes |

Press B on the title screen to toggle sound on and off (stored in the standard
Arduboy audio setting). On real hardware the tones are played with
`volumeMode(VOLUME_ALWAYS_HIGH)`, which drives both speaker pins in bridge mode.
There is no marching sound for the invaders - neither the original calculator
nor the JavaScript version has one.

## Levels

| Level | Invaders | Shots | Step interval (pattern 1 to 9) |
|-------|----------|-------|--------------------------------|
| EASY   | 16 | 30 | 1500ms - 600ms |
| NORMAL | 16 | 30 | 1200ms - 390ms |
| HARD   | 16 | 30 | 800ms - 320ms |
| DEATH  | 32 | 35 | 450ms - 200ms |

The high score and the selected level are stored in EEPROM (kept across runs by
arduboy-emu's EEPROM persistence).

## Building

You need:

- [arduino-cli](https://arduino.github.io/arduino-cli/) (or the Arduino IDE)
- The Arduino AVR board core
- Libraries: `Arduboy2`, `ArduboyTones`

```bash
arduino-cli core update-index
arduino-cli core install arduino:avr
arduino-cli lib install Arduboy2 ArduboyTones
arduino-cli compile --fqbn arduino:avr:leonardo --output-dir build DigitalInvader
```

On Windows the bundled PowerShell script does the same thing:

```bash
pwsh -File build.ps1
```

Either way the output is `build/DigitalInvader.ino.hex`.

## Running in the emulator

```bash
cargo run --release -- /path/to/GC-Digital-Invader/build/DigitalInvader.ino.hex
```

Or drop the `.hex` into arduboy-emu's `roms/` directory and pick it from the
list opened with the `O` key.

## Tests (on the host)

`test/` holds Arduboy2 stubs, so the game logic alone can be built and exercised
on a PC - invader spawning and marching, hit detection, scoring, losing a plane,
running out of ammunition, stage clear, and the UFO.

```bash
cd test && clang++ -std=c++17 -w -I. -o host_test host_test.cpp && ./host_test
```

`screendump.cpp` renders the 8-digit seven-segment display as ASCII art, to
eyeball the glyphs without hardware.

## Credits

- Original game design 1980 CASIO COMPUTER CO., LTD.
- JavaScript version 2016-2020 Akebi (http://akebi.jp/)
- Arduboy port: this repository
