# Battleships (C64)

- Build with `make`. Output lands in `build/` (object files in `build/obj/`, mirroring `src/`; nothing is written into `src/`). Each file in `src/c/*.c` or `src/asm/*.s` becomes `build/<name>.prg`.
- Toolchain is cc65 (target `-t c64`). Assembly uses **ca65 syntax** and links with `-C c64-asm.cfg -u __EXEHDR__`, which adds the BASIC SYS stub.
- To check output headlessly: `x64sc -default -warp -limitcycles 8000000 -exitscreenshot out.png -autostartprgmode 1 build/<name>.prg` (exits with status 1 when the cycle limit is hit; that is expected).
- cc65 conio: use `\r\n` for newlines in `cputs`/`cprintf`.
- `make run PRG=<name>` launches a program (default `battleships`) with the two-player keyboard-to-joystick mapping from `vice/keys.cfg`.
- battleships: VIC uses bank 2 (screen $8000, sprites generated at runtime from $8400), so the program plus BSS must end below $8000 (checked at startup). It links src/music (player, song, sound.s IRQ with effects on voice 2). To test gameplay headlessly, build with `-DAUTOPLAY` (`build/battleships-auto.prg`), which lets two bots play.
- Music: `src/music/player.s` (driver, format documented in its header) + `song.s` (data). Built as `build/song.prg` (raster IRQ demo) and `build/song.sid` (PSID via `src/music/sid.cfg`, header in `sidheader.s`, assembled with `-t none` so strings stay ASCII). To check audio, record in real time (not warp): `x64sc -default -sounddev wav -soundarg out.wav -limitcycles 30000000 -autostartprgmode 1 build/song.prg`.
- Ship sprites/heading tables (36 headings, 10° each) are generated: `tools/gen_ship_sprites.py` → `src/c/ship_sprites.h` (don't hand-edit; `--preview out.png` renders a sheet).
- Custom charset: `tools/gen_charset.py` → `src/c/charset.h` (256 glyphs, standard screen codes), copied to $A000 at startup (VIC bank 2, VIC.addr = $08).
- battleships runs its logic at a fixed 25 Hz tick (`wait_tick()`, counted by `irq_frames` in `src/music/sound.s`); all per-tick constants assume that. C is compiled with `-Oirs -Cl` (static locals). Keep hot paths free of int multiply/divide and pointer-heavy struct access; the worst case must stay under ~2 frames (~37k cycles).
