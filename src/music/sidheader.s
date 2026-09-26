; PSID v2 header for build/song.sid (linked with sid.cfg, player at $1000).
; Header fields are big-endian; strings are plain ASCII (assemble with -t none).

        .import music_init, music_play

.segment "SIDHEADER"
        .byte "PSID"
        .dbyt 2                 ; version
        .dbyt $7c               ; data offset
        .dbyt 0                 ; load address: taken from the data
        .dbyt init              ; init address
        .dbyt play              ; play address
        .dbyt 1                 ; songs
        .dbyt 1                 ; start song
        .dword 0                ; speed: vertical blank (50 Hz)
name:   .byte "Neon Tide"
        .res 32 - (* - name)
author: .byte "okahilak"
        .res 32 - (* - author)
rel:    .byte "2026"
        .res 32 - (* - rel)
        .dbyt $0014             ; flags: PAL, 6581
        .byte 0, 0              ; start page, page length
        .dbyt 0                 ; reserved
        .word $1000             ; load address (little-endian, start of data)

.segment "SIDJMP"
init:   jmp music_init
play:   jmp music_play
