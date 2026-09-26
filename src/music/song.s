; "Neon Tide" - an original tune in the style of Jeroen Tel.
;
; D minor, 150 BPM (5 frames per row, 16 rows per bar).
;   voice 1: bass and drums in one channel, filtered bass "pluck"
;   voice 2: fast 1-frame chord arpeggios with pulse width sweep
;   voice 3: lead with delayed vibrato and slides (hi-hats in the intro)
;
; Form: intro (4 bars) | A (8) | B (8) | A (8) | break (4) | back to A
; Chords  A: Dm Bb C Am Dm Bb Gm A    B: Gm Dm Bb C Gm Dm Bb A

        .include "notes.inc"

        .export song_tempo, song_order_lo, song_order_hi
        .export pat_lo, pat_hi
        .export ins_ad, ins_sr, ins_wave, ins_pw, ins_pwspd
        .export ins_vibdepth, ins_vibdelay, ins_vibspeed, ins_filt, ins_fsweep
        .export wt_wave, wt_note

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

song_tempo:     .byte 5

; ---------------------------------------------------------------------------
; Instruments

I_BASS    = 1
I_KICK    = 2           ; kick drum that turns into a bass note
I_SNARE   = 3
I_HAT     = 4
I_ARPMIN  = 5
I_ARPMAJ  = 6
I_LEAD    = 7
I_SOFTBAS = 8           ; darker filtered bass for the intro

;                  -    bass kick snare hat  arpm arpM lead soft
ins_ad:     .byte $00, $0a, $0a, $00, $00, $05, $05, $08, $0a
ins_sr:     .byte $00, $80, $80, $f9, $f2, $a8, $a8, $b9, $80
ins_wave:   .byte W_BASS, W_BASS, W_KICK, W_SNARE, W_HAT, W_ARPMIN, W_ARPMAJ, W_LEAD, W_BASS
ins_pw:     .byte $00, $40, $40, $80, $80, $80, $80, $30, $40      ; x16
ins_pwspd:  .byte $00, $20, $20, $00, $00, $30, $30, $18, $10
ins_vibdepth: .byte 0,   0,   0,   0,   0,   0,   0,  60,   0
ins_vibdelay: .byte 0,   0,   0,   0,   0,   0,   0,  10,   0
ins_vibspeed: .byte 0,   0,   0,   0,   0,   0,   0,   4,   0
ins_filt:   .byte $00, $90, $00, $00, $00, $00, $00, $00, $50      ; start cutoff, 0 = unfiltered
ins_fsweep: .byte $00, $f4, $00, $00, $00, $00, $00, $00, $fc      ; cutoff change per frame

; ---------------------------------------------------------------------------
; Wave table: (control, note) per frame. $80+n = absolute note n.

W_BASS   = 0
W_KICK   = 3
W_SNARE  = 8
W_HAT    = 13
W_ARPMIN = 16
W_ARPMAJ = 20
W_LEAD   = 24

wt_wave:
        .byte $41, $41, $ff                     ; 0  bass: octave blip, then root
        .byte $81, $41, $41, $41, $ff           ; 3  kick: noise click, pitch drop, bass note
        .byte $81, $41, $81, $81, $ff           ; 8  snare: noise / pulse snap / noise
        .byte $81, $80, $ff                     ; 13 hi-hat
        .byte $41, $41, $41, $ff                ; 16 minor arpeggio
        .byte $41, $41, $41, $ff                ; 20 major arpeggio
        .byte $41, $ff                          ; 24 lead
wt_note:
        .byte 12, 0, 1
        .byte $80|80, $80|34, $80|28, 0, 6
        .byte $80|92, $80|36, $80|86, $80|82, 11
        .byte $80|94, $80|94, 14
        .byte 0, 3, 7, 16
        .byte 0, 4, 7, 20
        .byte 0, 24

; ---------------------------------------------------------------------------
; Order lists

song_order_lo:  .byte <order1, <order2, <order3
song_order_hi:  .byte >order1, >order2, >order3

order1:                                         ; bass + drums
        .byte TR(0),2, TR(-4),2, TR(-2),2, TR(-5),2
@loop:  .byte TR(0),0, TR(-4),0, TR(-2),0, TR(-5),0, TR(0),0, TR(-4),0, TR(-7),0, TR(-5),1
        .byte TR(-7),0, TR(0),0, TR(-4),0, TR(-2),0, TR(-7),0, TR(0),0, TR(-4),0, TR(-5),1
        .byte TR(0),0, TR(-4),0, TR(-2),0, TR(-5),0, TR(0),0, TR(-4),0, TR(-7),0, TR(-5),1
        .byte TR(0),0, TR(-4),0, TR(-2),0, TR(-5),1
        .byte $ff, <(@loop - order1)

order2:                                         ; arpeggios
        .byte TR(0),3, TR(-4),4, TR(-2),4, TR(-5),4
@loop:  .byte TR(0),3, TR(-4),4, TR(-2),4, TR(-5),3, TR(0),3, TR(-4),4, TR(-7),3, TR(-5),4
        .byte TR(-7),3, TR(0),3, TR(-4),4, TR(-2),4, TR(-7),3, TR(0),3, TR(-4),4, TR(-5),4
        .byte TR(0),3, TR(-4),4, TR(-2),4, TR(-5),3, TR(0),3, TR(-4),4, TR(-7),3, TR(-5),4
        .byte TR(0),3, TR(-4),4, TR(-2),4, TR(-5),4
        .byte $ff, <(@loop - order2)

order3:                                         ; lead
        .byte TR(0), 10, 10, 10, 10
@loop:  .byte 5, 6, 7, 8, 5, 6, 9
        .byte $ff, <(@loop - order3)

; ---------------------------------------------------------------------------
; Patterns (16 rows = 1 bar). Bass patterns are written on D and transposed.

pat_lo: .byte <p_bass, <p_bassfill, <p_bassintro, <p_arpmin, <p_arpmaj
        .byte <p_leada1, <p_leada2, <p_leadb1, <p_leadb2, <p_leadc, <p_hats
pat_hi: .byte >p_bass, >p_bassfill, >p_bassintro, >p_arpmin, >p_arpmaj
        .byte >p_leada1, >p_leada2, >p_leadb1, >p_leadb2, >p_leadc, >p_hats

p_bass:                                         ; 0
        DUR 1
        INS I_KICK
        .byte D2
        INS I_BASS
        .byte D3, D2, D3
        INS I_SNARE
        .byte D3
        INS I_BASS
        .byte D2, D3, D2
        INS I_KICK
        .byte D2
        INS I_BASS
        .byte D3
        INS I_KICK
        .byte D2
        INS I_BASS
        .byte D3
        INS I_SNARE
        .byte D3
        INS I_BASS
        .byte D2, A2, D3
        .byte END

p_bassfill:                                     ; 1: first half as p_bass, then a snare roll
        DUR 1
        INS I_KICK
        .byte D2
        INS I_BASS
        .byte D3, D2, D3
        INS I_SNARE
        .byte D3
        INS I_BASS
        .byte D2, D3, D2
        INS I_KICK
        .byte D2
        INS I_SNARE
        .byte D3, D3
        INS I_KICK
        .byte D2
        INS I_SNARE
        .byte D3, D3, D3, D3
        .byte END

p_bassintro:                                    ; 2
        DUR 2
        INS I_SOFTBAS
        .byte D2, D3, D2, D3, D2, D3, A2, D3
        .byte END

p_arpmin:                                       ; 3: syncopated chord stabs
        INS I_ARPMIN
        DUR 3
        .byte D4, D4
        DUR 2
        .byte D4
        DUR 4
        .byte D4, D4
        .byte END

p_arpmaj:                                       ; 4
        INS I_ARPMAJ
        DUR 3
        .byte D4, D4
        DUR 2
        .byte D4
        DUR 4
        .byte D4, D4
        .byte END

p_leada1:                                       ; 5: Dm Bb C Am
        INS I_LEAD
        DUR 4
        .byte A4
        DUR 2
        .byte D5, E5
        DUR 4
        .byte F5
        DUR 2
        .byte E5, D5

        DUR 6
        .byte D5
        DUR 2
        .byte C5
        DUR 4
        .byte D5, F5

        DUR 6
        .byte E5
        DUR 2
        .byte G5
        DUR 4
        .byte E5, C5

        DUR 12
        .byte A4
        DUR 2
        .byte C5, E5
        .byte END

p_leada2:                                       ; 6: Dm Bb Gm A
        DUR 4
        .byte F5
        DUR 2
        .byte E5, D5
        SLIDE $ff
        DUR 8
        .byte A5

        DUR 4
        .byte G5, F5, D5, As4

        DUR 4
        .byte D5
        DUR 2
        .byte As4, G4
        DUR 4
        .byte D5, F5

        DUR 8
        .byte E5, Cs5
        .byte END

p_leadb1:                                       ; 7: Gm Dm Bb C
        INS I_LEAD
        DUR 2
        .byte G5, A5
        DUR 4
        .byte As5
        DUR 2
        .byte A5, G5
        DUR 4
        .byte D5

        DUR 6
        .byte F5
        DUR 2
        .byte E5
        DUR 8
        .byte D5

        DUR 2
        .byte D5, F5
        DUR 4
        .byte As5
        DUR 2
        .byte A5, F5
        DUR 4
        .byte D5

        DUR 4
        .byte E5, G5
        SLIDE $ff
        DUR 8
        .byte C6
        .byte END

p_leadb2:                                       ; 8: Gm Dm Bb A
        DUR 4
        .byte As5
        DUR 2
        .byte A5, G5
        DUR 8
        .byte D6

        DUR 4
        .byte C6, A5, F5, D5

        DUR 2
        .byte D5, F5
        DUR 4
        .byte As5, A5, G5

        DUR 16
        .byte A5
        .byte END

p_leadc:                                        ; 9: break, falling arpeggios
        INS I_LEAD
        DUR 2
        .byte D6, A5, F5, D5, A5, F5, D5, A4
        .byte D6, As5, F5, D5, As5, F5, D5, As4
        .byte C6, G5, E5, C5, G5, E5, C5, G4
        .byte A5, E5, Cs5, A4
        DUR 8
        .byte E5
        .byte END

p_hats:                                         ; 10: intro hi-hats
        INS I_HAT
        DUR 2
        .byte C5, C5, C5, C5, C5, C5, C5, C5
        .byte END
