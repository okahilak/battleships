; Plays the song from a raster interrupt. The border shows the player's
; CPU time per frame.

        .import music_init, music_play

.segment "CODE"
start:
        ldx #0
@print: lda text,x
        beq @go
        jsr $ffd2               ; CHROUT
        inx
        bne @print

@go:    sei
        lda #$7f                ; no CIA interrupts
        sta $dc0d
        lda $dc0d
        lda #$20                ; raster interrupt at line $20 (in the top border)
        sta $d012
        lda $d011
        and #$7f
        sta $d011
        lda #1
        sta $d01a
        lda #<irq
        sta $0314
        lda #>irq
        sta $0315
        jsr music_init
        cli
forever:
        jmp forever

irq:    asl $d019               ; acknowledge
        lda #COLOR_RASTER
        sta $d020
        jsr music_play
        lda #0
        sta $d020
        jmp $ea81               ; restore registers, rti

COLOR_RASTER = 11

.segment "RODATA"
text:   .byte $93, $05, 13, 13
        .byte "             neon tide", 13, 13
        .byte $9e, "      a sid tune in the style of", 13
        .byte "             jeroen tel", 13, 13, 13
        .byte $9f, "  voice 1  bass + drums, filter pluck", 13
        .byte "  voice 2  1-frame arpeggios, pwm", 13
        .byte "  voice 3  lead: vibrato, slides", 13, 13
        .byte $98, "  grey border = player cpu time", 0
