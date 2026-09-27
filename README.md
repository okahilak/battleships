# C64 development

Toolchain (installed with Homebrew):

| Tool | Purpose |
|------|---------|
| `cl65` / `cc65` / `ca65` / `ld65` | C compiler, assembler, linker ([cc65](https://cc65.github.io/doc/)) |
| `x64sc` | VICE cycle-exact C64 emulator |
| `c1541` | Create and edit `.d64` disk images |
| `petcat` | Convert BASIC listings to and from `.prg` |
| `acme` | Alternative assembler (ACME syntax) |

## Layout

```
src/c/*.c     C programs    -> build/<name>.prg
src/asm/*.s   ca65 programs -> build/<name>.prg (with BASIC "SYS 2061" stub)
src/music/    SID music driver + song -> build/song.prg and build/song.sid
build/        output: .prg, .map, .lbl (VICE labels), disk.d64
```

Each source file becomes its own program. To add one, drop a new file into `src/c/` or `src/asm/` and run `make`.

## Commands

```sh
make                  # build everything
make run              # run battleships in VICE
make run PRG=hello    # run any other program, e.g. hello or border
make autoplay         # battleships with two bots playing (quick test)
make run PRG=song     # play the SID tune on the C64
make play             # play build/song.sid in VSID (VICE's SID player)
make disk             # pack all programs into build/disk.d64
make clean
```

## Battleships

A real-time naval duel for two players, seen from above ([src/c/battleships.c](src/c/battleships.c)). Each ship survives 3 hits; the first to sink the other wins the round. A ship that runs aground on an island is wrecked at once (the screen edges just stop it). Shells fly over islands.

| | Player 1 (yellow) | Player 2 (red) |
|---|---|---|
| Joystick | port 2 | port 1 |
| Keyboard in VICE (`make run`) | W A S D + Space | I J K L + Return |

- Moving the joystick (no button) moves your crosshair (X). The ship keeps its course meanwhile.
- Holding fire makes the joystick steer. Left/right puts the rudder hard left or hard right, and it stays there: the ship keeps turning (in 10° steps). Right then left turns left. Up centres the rudder, so right then up goes straight ahead.
- With the rudder already straight, up raises the engine setting (the speed you order), and down lowers it or goes astern. A slider on the bottom row shows it: the white ◆ is the setting, and the lit part of the track is the actual speed, which follows the setting with a delay.
- Ships are heavy, as in Beach-Head. They take about 1.3 seconds to reach full speed and then keep going, since nothing but the engines slows them down. A ship always moves the way its bow points.
- Tapping fire (without moving) fires a shell toward the crosshair. It flies in an arc, looking bigger the higher it is, and lands after a delay that grows with distance, so aim where the enemy will be. A ring in your colour marks the landing spot until the shell comes down. It damages any ship within range of the landing point, including your own.
- Each ship fires 3 shots, then has to reload for 2.5 seconds. The top row shows it next to your hearts: shell icons (grey once used), or a bar that shrinks one block every half second while reloading. While reloading, the ship also changes colour (yellow → light green, red → violet).

The keyboard mapping lives in [vice/keys.cfg](vice/keys.cfg).

The 36 ship images (one every 10°) are pre-rotated by [tools/gen_ship_sprites.py](tools/gen_ship_sprites.py). The script writes `src/c/ship_sprites.h`, and `make` reruns it when the script changes. To reshape the ship, edit `HULL` in the script; `--preview sheet.png` draws every heading into one image.

The font and graphics characters (islands, waves, hearts, ammo, markers, slider) are a custom character set from [tools/gen_charset.py](tools/gen_charset.py). The script writes `src/c/charset.h`, and the game copies it to $A000 at startup. The glyphs keep the standard C64 screen codes, so editing a pattern in the script changes that character everywhere. Islands use 16 coast tiles (one per combination of land neighbours) with rounded corners and an irregular shoreline. The game picks the right tile for each land square, and land checks are pixel-precise, so ships can sail into the rounded bays.

"Neon Tide" plays throughout. Sound effects borrow the song's arpeggio voice (voice 2) while they play, and the bass/drums and lead continue. The interrupt driver is [src/music/sound.s](src/music/sound.s).

## Music

"Neon Tide" ([src/music/song.s](src/music/song.s)) is an original tune in the style of Jeroen Tel. It is in D minor at 150 BPM:

- Voice 1 plays bass and drums together, with a filter "pluck" on the bass.
- Voice 2 plays chords as fast one-frame arpeggios.
- Voice 3 plays the lead, with delayed vibrato and pitch slides.

The driver is [src/music/player.s](src/music/player.s). Its header comment documents the song data format: order lists with transpose, patterns, instruments, and a per-frame wave table used for drums and arpeggios. It builds two ways:

- `build/song.sid`: a PSID file (player at $1000, init $1000, play $1003) for VSID, SIDPlay and similar players.
- `build/song.prg`: a C64 program that plays the tune from a raster interrupt. The grey band in the top border shows how much CPU time the player uses each frame.

To write your own tune, edit the patterns and order lists in `song.s`. Note names such as `D5` and `As4` (A-sharp/B-flat) come from `notes.inc`.

## No sound?

VICE plays through the macOS default output device. If that is a monitor or TV without speakers, you hear nothing. Either switch the output in macOS sound settings, or pick a device for VICE:

```sh
make play AUDIO="MacBook Air Speakers"      # also works with make run / make autoplay
```

## Debugging

`make run` loads the `.lbl` symbol file into VICE, so in the monitor (**Alt+H** in VICE) you can use your own label names, for example `d .start` or `break .loop`.

## References

- cc65 C64 notes: https://cc65.github.io/doc/c64.html
- C64 memory map: https://sta.c64.org/cbm64mem.html
- 6502 instruction reference: https://www.masswerk.at/6502/6502_instruction_set.html
- Codebase64 (tricks and routines): https://codebase64.org
