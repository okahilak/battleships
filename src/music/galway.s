; "Moonlit Harbour" - an original tune in the style of Martin Galway.
;
; A minor, 125 BPM (6 frames per row, 16 rows per bar).
;   voice 1: bass with a resonant filter opening on every note, light drums
;   voice 2: soft-attack chord arpeggios (2 frames per step); in section B
;            an echo of the lead, 3 rows behind and quieter
;   voice 3: singing lead with slow pulse sweep, delayed vibrato, slides
;
; Form: intro (4 bars) | A (8) | B (8) | A (8) | back to A
; Chords  A: Am F C G Am F G E    B: F G Em Am Dm G C E

        .include "notes.inc"

        .export galway_song              ; song descriptor for music_init

REST = $60
TIE  = $61
END  = $ff

.macro DUR rows
        .byte $80 + (rows) - 1
.endmacro

.macro INS n
        .byte $a0 + (n)
.endmacro

.macro SLIDE speed
        .byte $c0, speed
.endmacro

.define TR(semitones) ($a0 + (semitones))

.segment "RODATA"

; Descriptor: where this song's tables are (order fixed by player.s)
galway_song:
        .word song_tempo, song_order_lo, song_order_hi, pat_lo, pat_hi
        .word ins_ad, ins_sr, ins_wave, ins_pw, ins_pwspd
        .word ins_vibdepth, ins_vibdelay, ins_vibspeed, ins_filt, ins_fsweep
        .word wt_wave, wt_note, song_text

song_tempo:     .byte 6

; ---------------------------------------------------------------------------
; Instruments

I_BASS   = 1
I_KICK   = 2
I_SNARE  = 3
I_ARPMIN = 4
I_ARPMAJ = 5
I_LEAD   = 6
I_ECHO   = 7

;                  -    bass kick snare arpm arpM lead echo
ins_ad:     .byte $00, $08, $0a, $00, $4a, $4a, $3a, $3a
ins_sr:     .byte $00, $a9, $80, $f9, $a9, $a9, $b9, $69
ins_wave:   .byte W_BASS, W_BASS, W_KICK, W_SNARE, W_ARPMIN, W_ARPMAJ, W_LEAD, W_LEAD
ins_pw:     .byte $00, $30, $40, $80, $60, $60, $20, $20      ; x16
ins_pwspd:  .byte $00, $10, $20, $00, $0c, $0c, $08, $08      ; slow sweeps
ins_vibdepth: .byte 0,   0,   0,   0,   0,   0,  40,  40
ins_vibdelay: .byte 0,   0,   0,   0,   0,   0,  18,  18
ins_vibspeed: .byte 0,   0,   0,   0,   0,   0,   5,   5
ins_filt:   .byte $00, $20, $00, $00, $00, $00, $00, $00      ; bass: filter opens...
ins_fsweep: .byte $00, $04, $00, $00, $00, $00, $00, $00      ; ...by 4 per frame

; ---------------------------------------------------------------------------
; Wave table: (control, note) per frame. $80+n = absolute note n.

W_BASS   = 0
W_KICK   = 2
W_SNARE  = 7
W_ARPMIN = 12
W_ARPMAJ = 19
W_LEAD   = 26

wt_wave:
        .byte $41, $ff                          ; 0  bass
        .byte $81, $41, $41, $41, $ff           ; 2  kick into bass note
        .byte $81, $41, $81, $81, $ff           ; 7  snare
        .byte $41, $41, $41, $41, $41, $41, $ff ; 12 minor chord, 2 frames per note
        .byte $41, $41, $41, $41, $41, $41, $ff ; 19 major chord
        .byte $41, $ff                          ; 26 lead
wt_note:
        .byte 0, 0
        .byte $80|80, $80|34, $80|28, 0, 5
        .byte $80|92, $80|36, $80|86, $80|82, 10
        .byte 0, 0, 3, 3, 7, 7, 12
        .byte 0, 0, 4, 4, 7, 7, 19
        .byte 0, 26

; ---------------------------------------------------------------------------
; Order lists. Bass and chords are written on A and transposed:
; F = -4, C = +3, G = -2, E/Em = -5, Dm = -7.

song_order_lo:  .byte <order1, <order2, <order3
song_order_hi:  .byte >order1, >order2, >order3

order1:                                         ; bass + drums
        .byte TR(0),2, TR(-4),2, TR(3),2, TR(-2),2
@loop:  .byte TR(0),0, TR(-4),0, TR(3),0, TR(-2),0, TR(0),0, TR(-4),0, TR(-2),0, TR(-5),1
        .byte TR(-4),0, TR(-2),0, TR(-5),0, TR(0),0, TR(-7),0, TR(-2),0, TR(3),0, TR(-5),1
        .byte TR(0),0, TR(-4),0, TR(3),0, TR(-2),0, TR(0),0, TR(-4),0, TR(-2),0, TR(-5),1
        .byte $ff, <(@loop - order1)

order2:                                         ; chords, echo in B
        .byte TR(0),3, TR(-4),4, TR(3),4, TR(-2),4
@loop:  .byte TR(0),3, TR(-4),4, TR(3),4, TR(-2),4, TR(0),3, TR(-4),4, TR(-2),4, TR(-5),4
        .byte TR(0),9, 10
        .byte TR(0),3, TR(-4),4, TR(3),4, TR(-2),4, TR(0),3, TR(-4),4, TR(-2),4, TR(-5),4
        .byte $ff, <(@loop - order2)

order3:                                         ; lead
        .byte TR(0), 11, 11, 11, 11
@loop:  .byte 5, 6, 7, 8, 5, 6
        .byte $ff, <(@loop - order3)

; ---------------------------------------------------------------------------
; Patterns (16 rows = 1 bar)

pat_lo: .byte <p_bass, <p_bassfill, <p_bassintro, <p_arpmin, <p_arpmaj
        .byte <p_leada1, <p_leada2, <p_leadb1, <p_leadb2, <p_echob1, <p_echob2, <p_rest
pat_hi: .byte >p_bass, >p_bassfill, >p_bassintro, >p_arpmin, >p_arpmaj
        .byte >p_leada1, >p_leada2, >p_leadb1, >p_leadb2, >p_echob1, >p_echob2, >p_rest

p_bass:                                         ; 0: kick on 1, snare on 3
        DUR 2
        INS I_KICK
        .byte A2
        INS I_BASS
        .byte A3, E3, A3
        INS I_SNARE
        .byte A3
        INS I_BASS
        .byte A3, E3, A3
        .byte END

p_bassfill:                                     ; 1: into the next section
        DUR 2
        INS I_KICK
        .byte A2
        INS I_BASS
        .byte A3, E3, A3
        INS I_SNARE
        .byte A3
        DUR 1
        .byte A3, A3
        INS I_KICK
        .byte A2
        INS I_SNARE
        .byte A3, A3, A3
        .byte END

p_bassintro:                                    ; 2: bass alone, filter sweeps
        DUR 2
        INS I_BASS
        .byte A2, A3, E3, A3, A2, A3, E3, A3
        .byte END

p_arpmin:                                       ; 3
        INS I_ARPMIN
        DUR 8
        .byte A3, A3
        .byte END

p_arpmaj:                                       ; 4
        INS I_ARPMAJ
        DUR 8
        .byte A3, A3
        .byte END

p_leada1:                                       ; 5: Am F C G
        INS I_LEAD
        DUR 6
        .byte E5
        DUR 2
        .byte D5
        DUR 4
        .byte C5, A4

        DUR 6
        .byte A4
        DUR 2
        .byte C5
        SLIDE $60
        DUR 8
        .byte F5

        DUR 6
        .byte E5
        DUR 2
        .byte D5
        DUR 8
        .byte C5

        DUR 4
        .byte B4, D5
        SLIDE $60
        DUR 8
        .byte G5
        .byte END

p_leada2:                                       ; 6: Am F G E
        DUR 6
        .byte A5
        DUR 2
        .byte G5
        DUR 4
        .byte E5, C5

        DUR 6
        .byte F5
        DUR 2
        .byte E5
        DUR 8
        .byte C5

        DUR 4
        .byte D5, B4, G4, B4

        DUR 12
        .byte Gs4
        DUR 4
        .byte B4
        .byte END

p_leadb1:                                       ; 7: F G Em Am
        INS I_LEAD
        DUR 8
        .byte C6
        DUR 4
        .byte A5, F5

        DUR 8
        .byte D6
        DUR 4
        .byte B5, G5

        DUR 6
        .byte B5
        DUR 2
        .byte A5
        DUR 4
        .byte G5, E5

        DUR 16
        .byte A5
        .byte END

p_leadb2:                                       ; 8: Dm G C E
        DUR 4
        .byte F5, A5
        SLIDE $a0
        DUR 8
        .byte D6

        DUR 4
        .byte D6, B5, G5, D5

        DUR 6
        .byte E5
        DUR 2
        .byte F5
        DUR 8
        .byte G5

        DUR 8
        .byte Gs5
        DUR 4
        .byte B5, E6
        .byte END

p_echob1:                                       ; 9: p_leadb1, 3 rows later
        INS I_ECHO
        DUR 3
        .byte REST
        DUR 8
        .byte C6
        DUR 4
        .byte A5, F5

        DUR 8
        .byte D6
        DUR 4
        .byte B5, G5

        DUR 6
        .byte B5
        DUR 2
        .byte A5
        DUR 4
        .byte G5, E5

        DUR 13
        .byte A5
        .byte END

p_echob2:                                       ; 10: p_leadb2, 3 rows later
        DUR 3
        .byte REST
        DUR 4
        .byte F5, A5
        SLIDE $a0
        DUR 8
        .byte D6

        DUR 4
        .byte D6, B5, G5, D5

        DUR 6
        .byte E5
        DUR 2
        .byte F5
        DUR 8
        .byte G5

        DUR 8
        .byte Gs5
        DUR 4
        .byte B5
        DUR 1
        .byte E6
        .byte END

p_rest:                                         ; 11: one silent bar
        DUR 16
        .byte REST
        .byte END

; Title screen text for demo.s (PETSCII, 0-terminated, < 256 bytes)
song_text:
        .byte $93, $05, 13, 13
        .byte "           moonlit harbour", 13, 13
        .byte $9e, "      a sid tune in the style of", 13
        .byte "            martin galway", 13, 13, 13
        .byte $9f, "  voice 1  bass, resonant filter sweep", 13
        .byte "  voice 2  soft arpeggios / lead echo", 13
        .byte "  voice 3  lead: pwm, vibrato, slides", 13, 13
        .byte $98, "  grey border = player cpu time", 0
