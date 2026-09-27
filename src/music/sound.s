; Game sound: plays the music and sound effects from a raster interrupt.
; Effects borrow SID voice 2 (the song's arpeggio voice) while they play;
; the other two voices keep the music going.
;
; C interface:
;   void __fastcall__ sound_start(unsigned char song);  start interrupt + music
;   void __fastcall__ music_select(unsigned char song); switch song
;   void __fastcall__ sfx_play(unsigned char);          start an effect
;
; Songs: 0 = Neon Tide, 1 = Moonlit Harbour, 2 = music off (effects only).
;
; A higher effect number has priority: it replaces a lower one that is still
; playing, a lower one is ignored while a higher one plays.
;
; The CIA timer interrupt is switched off, so the KERNAL no longer scans the
; keyboard (it would also disturb joystick reads through $dc00).

        .export _sound_start, _music_select, _sfx_play, _irq_frames
        .import music_init, music_play, music_mute
        .import neon_song, galway_song

VOICE    = $d407                ; SID voice 2 registers
VOICEBIT = 2                    ; voice 2 in music_mute
IRQLINE  = 0                    ; raster line of the interrupt

.segment "RODATA"
; effect:       splash shot   ice    pickup hit    sink
sfx_wave:  .byte $81,   $81,   $41,   $41,   $81,   $81
sfx_ad:    .byte $09,   $08,   $00,   $00,   $0a,   $0c
sfx_sr:    .byte $00,   $00,   $f0,   $a0,   $00,   $00
sfx_freq:  .byte $18,   $30,   $40,   $18,   $12,   $0c   ; start frequency (high byte)
sfx_delta: .byte $ff,   $fe,   $00,   $04,   $ff,   $00   ; frequency change per frame (signed)
sfx_len:   .byte 16,    12,    47,    16,    26,    80    ; frames
sfx_gate:  .byte 0,     0,     8,     0,     0,     0     ; nonzero: gate on only while
                                                   ; (frames left & mask) != 0,
                                                   ; e.g. 8 = 3 beeps in 47 frames
SFX_COUNT = * - sfx_len

.segment "RODATA"
songs_lo:  .byte <neon_song, <galway_song
songs_hi:  .byte >neon_song, >galway_song
SONG_COUNT = * - songs_hi

.segment "BSS"
music_on:  .res 1               ; 0 = music off
_irq_frames: .res 1             ; frames since start (for the game's 25 Hz tick)
pending:   .res 1               ; requested effect, $ff = none
current:   .res 1               ; playing effect, $ff = none
timer:     .res 1
tmpgate:   .res 1
freq:      .res 1

.segment "CODE"

_sound_start:
        sei
        pha
        lda #$7f                ; no CIA interrupts
        sta $dc0d
        lda $dc0d
        pla
        jsr select
        lda #IRQLINE
        sta $d012
        lda $d011
        and #$7f
        sta $d011
        lda #<irq
        sta $0314
        lda #>irq
        sta $0315
        lda #1
        sta $d01a
        sta $d019
        cli
        rts

; A = song (0..SONG_COUNT-1), or anything else for music off.
_music_select:
        sei
        jsr select
        cli
        rts

; With interrupts off: stops any effect and starts song A (or silence).
select: ldx #$ff
        stx pending
        stx current
        cmp #SONG_COUNT
        bcs @off
        tax
        lda #1
        sta music_on
        lda songs_lo,x
        pha
        lda songs_hi,x
        tax
        pla
        jmp music_init
@off:   lda #0
        sta music_on
        sta music_mute
        ldx #$17
:       sta $d400,x             ; silence all voices and the filter
        dex
        bpl :-
        lda #$0f                ; volume on for the effects
        sta $d418
        rts

; A = effect number. The interrupt starts it on its next run.
_sfx_play:
        ldx pending
        cpx #$ff
        beq @store
        cmp pending
        bcc @done
@store: sta pending
@done:  rts

irq:    asl $d019               ; acknowledge raster interrupt
        inc _irq_frames
        jsr sfx_tick            ; before music, so the mute is set in time
        lda music_on
        beq :+
        jsr music_play
:       jmp $ea81               ; restore registers, rti

sfx_tick:
        lda pending
        cmp #$ff
        beq @run
        ldx #$ff
        stx pending
        ldx current
        cpx #$ff
        beq @start
        cmp current
        bcc @run                ; lower priority than what is playing

@start: sta current
        tax
        lda music_mute
        ora #VOICEBIT
        sta music_mute
        lda #0                  ; gate off, then on again: restart envelope
        sta VOICE+4
        sta VOICE+0
        lda sfx_ad,x
        sta VOICE+5
        lda sfx_sr,x
        sta VOICE+6
        lda sfx_freq,x
        sta freq
        sta VOICE+1
        lda #$08
        sta VOICE+3             ; pulse width, for pulse effects
        lda sfx_len,x
        sta timer
        lda sfx_wave,x
        sta VOICE+4
        rts

@run:   ldx current
        cpx #$ff
        beq @done
        dec timer
        beq @stop
        lda sfx_gate,x          ; beeping effects switch the gate on and off
        beq @sweep
        and timer               ; nonzero: gate on
        beq :+
        lda #1
:       sta tmpgate
        lda sfx_wave,x
        and #$fe
        ora tmpgate
        sta VOICE+4
@sweep: lda sfx_delta,x         ; sweep the pitch, clamped to 1..255
        beq @done
        bmi @down
        clc
        adc freq
        bcc @set
        lda #$ff
        bne @set
@down:  clc
        adc freq
        bcs :+
        lda #1
:       bne @set
        lda #1
@set:   sta freq
        sta VOICE+1
@done:  rts

@stop:  lda sfx_wave,x          ; gate off, give the voice back to the music
        and #$fe
        sta VOICE+4
        lda #$ff
        sta current
        lda music_mute
        and #<~VOICEBIT
        sta music_mute
        rts
