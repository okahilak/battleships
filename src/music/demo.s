; Plays a song from a raster interrupt. The border shows the player's
; CPU time per frame. Assemble with -D SONG_GALWAY for the second tune.

        .import music_init, music_play
.ifdef SONG_GALWAY
        .import galway_song
        SONG = galway_song
.else
        .import neon_song
        SONG = neon_song
.endif

text    = $fb                   ; title text pointer (descriptor field 17)

.segment "CODE"
start:
        lda SONG + 34
        sta text
        lda SONG + 35
        sta text+1
        ldy #0
@print: lda (text),y
        beq @go
        jsr $ffd2               ; CHROUT
        iny
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
        lda #<SONG
        ldx #>SONG
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
