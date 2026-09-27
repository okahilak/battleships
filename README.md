# c64

C64 stuff built with cc65 and run in VICE (`brew install cc65 vice`).

```sh
make                  # build everything into build/
make run              # battleships (PRG=hello, border, song, galway)
make autoplay         # battleships, bots vs bots
make play             # song.sid in VSID (SID=galway)
make disk             # build/disk.d64
make run AUDIO="MacBook Air Speakers"   # pick an output device
```

`src/c/*.c` and `src/asm/*.s` each become `build/<name>.prg`. `make run` loads labels into the VICE monitor (Alt+H).

## Battleships

Two-player naval duel, top view. P1 (yellow) uses port 2 or W/A/S/D + Space, P2 (red) port 1 or I/J/K/L + Return ([vice/keys.cfg](vice/keys.cfg)).

- Stick: move crosshair.
- Fire + left/right: rudder hard over (stays until changed). Fire + up: centre rudder, or if straight, more speed. Fire + down: less speed/astern. Speed follows the slider on the bottom row with a lag.
- Tap fire: shell lobbed at the crosshair, lands after a delay. 3 shots, then 2.5 s reload.
- 3 hearts. Hits, rams (both ships) and icebergs cost one; running aground sinks you.
- Random mirrored map each round, 1–2 hidden icebergs (visible only up close), power-up crates: fast shells, repair, spotter (bigger hit radius, shows icebergs).
- Title screen: left/right picks the music.

Generated data: ship sprites from `tools/gen_ship_sprites.py`, charset from `tools/gen_charset.py` (both rerun by `make`, `--preview out.png` renders a sheet).

## Music

`src/music/player.s` is a small SID driver (data format in its header). Songs: `song.s` "Neon Tide" (Jeroen Tel style) and `galway.s` "Moonlit Harbour" (Martin Galway style). Each builds to a `.prg` and a PSID `.sid`; the game links both via `sound.s`, with effects borrowing voice 2.
