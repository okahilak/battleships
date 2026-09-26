; Raster bar demo in ca65 syntax.
; Built with: cl65 -t c64 -C c64-asm.cfg -u __EXEHDR__
; The BASIC stub ("10 SYS 2061") is provided by the c64-asm.cfg linker config.

BORDER  = $d020
BG      = $d021
RASTER  = $d012

.segment "CODE"
start:
        sei
        lda #$00
        sta BG
loop:
        lda RASTER          ; current raster line
wait:   cmp RASTER          ; wait until it changes
        beq wait
        sta BORDER          ; use raster line as the border colour
        jmp loop
