; Compact SID music driver (ca65 syntax).
;
;   music_init   call once before playing
;   music_play   call once per frame (50 Hz)
;   music_mute   bit n set = voice n+1 keeps playing its part silently, so a
;                game can borrow that voice for sound effects
;
; Song data format (see song.s):
;   order list, one per voice:
;       $00-$7F  play pattern n
;       $80-$BF  set transpose to (value - $A0) semitones
;       $FF nn   jump to order list position nn
;   pattern bytes:
;       $00-$5F  note (C0..B7), played with current instrument and duration
;       $60      rest (gate off)
;       $61      tie (keep the current note sounding)
;       $80-$9F  set duration to (value & $1F) + 1 rows
;       $A0-$BF  set instrument (value & $1F)
;       $C0 ss   slide into the next note, ss = frequency step per frame
;       $FF      end of pattern
;   wave table: one row per frame, two parallel arrays:
;       wt_wave  SID control byte (gate bit is masked off on release),
;                or $FF = jump to the row given in wt_note
;       wt_note  $00-$7F added to the played note (arpeggios),
;                $80+n = absolute note n (drums)
;
; Uses zero page $FB/$FC.

        .export music_init, music_play, music_mute
        .import song_tempo, song_order_lo, song_order_hi
        .import pat_lo, pat_hi
        .import ins_ad, ins_sr, ins_wave, ins_pw, ins_pwspd
        .import ins_vibdepth, ins_vibdelay, ins_vibspeed, ins_filt, ins_fsweep
        .import wt_wave, wt_note

SID       = $d400
MUTED_REGS = $19                ; offset of SID $d419-$d41f: read-only/unused,
                                ; so writes of a muted voice go nowhere
ptr       = $fb
HR_FRAMES = 2                   ; hard restart this many frames before a new note
RESONANCE = $c0

.segment "BSS"
vars_start:
v_ordpos:   .res 3              ; position in order list
v_patlo:    .res 3              ; current pattern read pointer
v_pathi:    .res 3
v_trans:    .res 3              ; transpose (semitones)
v_dur:      .res 3              ; note length in rows
v_count:    .res 3              ; frames until next pattern event
v_note:     .res 3
v_ins:      .res 3
v_gate:     .res 3              ; 1 = gate on
v_wpos:     .res 3              ; wave table row
v_pwlo:     .res 3              ; pulse width
v_pwhi:     .res 3
v_pwspd:    .res 3              ; signed pulse width change per frame
v_vdel:     .res 3              ; vibrato delay countdown
v_vphase:   .res 3
v_vofslo:   .res 3              ; vibrato frequency offset (signed)
v_vofshi:   .res 3
v_slpend:   .res 3              ; slide speed for the next note
v_slide:    .res 3              ; 1 = slide in progress
v_slspd:    .res 3
v_frlo:     .res 3              ; current base frequency
v_frhi:     .res 3
v_tglo:     .res 3              ; slide target frequency
v_tghi:     .res 3
f_cut:      .res 1              ; filter cutoff (high byte)
f_sweep:    .res 1              ; signed cutoff change per frame
f_route:    .res 1              ; voices routed through the filter
tmp_note:   .res 1
tmp_ctrl:   .res 1
tmp_arp:    .res 1
tmp_depth:  .res 1
tmp_spd:    .res 1
vars_end:
music_mute: .res 1

.segment "RODATA"
sidofs:     .byte 0, 7, 14
vbit:       .byte 1, 2, 4
        .include "freqtable.inc"

.segment "CODE"

music_init:
        lda #0
        sta music_mute
        ldx #vars_end - vars_start - 1
@clr:   sta vars_start,x
        dex
        bpl @clr
        ldx #$18
@sid:   sta SID,x
        dex
        bpl @sid
        ldx #2
@voice: lda #1
        sta v_count,x
        sta v_dur,x
        jsr next_pattern
        dex
        bpl @voice
        rts

music_play:
        ldx #2
@voice: jsr do_voice
        dex
        bpl @voice

        ; filter sweep, clamped to 0..255
        lda f_sweep
        beq @fout
        bmi @fneg
        clc
        adc f_cut
        bcc @fset
        lda #$ff
        bne @fset
@fneg:  clc
        adc f_cut
        bcs @fset
        lda #0
@fset:  sta f_cut
@fout:  lda #0
        sta SID+$15
        lda f_cut
        sta SID+$16
        lda f_route
        ora #RESONANCE
        sta SID+$17
        lda #$1f                ; low pass, volume 15
        sta SID+$18
        rts

; ---------------------------------------------------------------------------
; Per voice, X = voice (0-2). All subroutines keep X.

do_voice:
        dec v_count,x
        bne @hr
        jsr read_events
@hr:    lda v_count,x
        cmp #HR_FRAMES
        bne @fx
        jsr peek_byte           ; is the next event a fresh note?
        cmp #$60                ; rest: gate goes off by itself
        beq @fx
        cmp #$61                ; tie: keep sounding
        beq @fx
        cmp #$ff
        beq @dohr
        cmp #$c0                ; slide: legato, no restart
        bcs @fx
@dohr:  jsr voice_regs
        lda #0
        sta v_gate,x
        sta SID+5,y
        sta SID+6,y
@fx:    jmp effects

; Y = SID register offset for voice X (or the dummy area when muted).
voice_regs:
        lda music_mute
        and vbit,x
        beq :+
        ldy #MUTED_REGS
        rts
:       ldy sidofs,x
        rts

; Returns the next pattern byte in A without consuming it.
peek_byte:
        lda v_patlo,x
        sta ptr
        lda v_pathi,x
        sta ptr+1
        ldy #0
        lda (ptr),y
        rts

read_byte:
        jsr peek_byte
        inc v_patlo,x
        bne :+
        inc v_pathi,x
:       rts

; Reads the order list until the next pattern starts.
next_pattern:
        lda song_order_lo,x
        sta ptr
        lda song_order_hi,x
        sta ptr+1
@next:  ldy v_ordpos,x
        lda (ptr),y
        iny
        cmp #$ff
        bne @notjump
        lda (ptr),y
        sta v_ordpos,x
        jmp @next
@notjump:
        pha
        tya
        sta v_ordpos,x
        pla
        cmp #$80
        bcc @pattern
        sec
        sbc #$a0
        sta v_trans,x
        jmp @next
@pattern:
        tay
        lda pat_lo,y
        sta v_patlo,x
        lda pat_hi,y
        sta v_pathi,x
        rts

; Processes pattern bytes up to and including the next note/rest/tie.
read_events:
@next:  jsr read_byte
        cmp #$ff
        bne @notend
        jsr next_pattern
        jmp @next
@notend:
        cmp #$c0
        bcc @notslide
        jsr read_byte
        sta v_slpend,x
        jmp @next
@notslide:
        cmp #$a0
        bcc @notins
        and #$1f
        sta v_ins,x
        jmp @next
@notins:
        cmp #$80
        bcc @event
        and #$1f
        clc
        adc #1
        sta v_dur,x
        jmp @next

@event: pha                     ; frames = rows * tempo
        lda #0
        ldy v_dur,x
        clc
@mul:   adc song_tempo
        dey
        bne @mul
        sta v_count,x
        pla

        cmp #$60
        bne @notrest
        lda #0
        sta v_gate,x
        rts
@notrest:
        cmp #$61
        bne @note
        rts

@note:  clc
        adc v_trans,x
        sta tmp_note
        sta v_note,x
        lda v_slpend,x
        beq trigger

        ; legato slide from the current frequency to the new note
        sta v_slspd,x
        lda #0
        sta v_slpend,x
        ldy tmp_note
        lda freq_lo,y
        sta v_tglo,x
        lda freq_hi,y
        sta v_tghi,x
        lda #1
        sta v_slide,x
        rts

trigger:
        lda #0
        sta v_slide,x
        sta v_vofslo,x
        sta v_vofshi,x
        ldy v_ins,x
        lda ins_wave,y
        sta v_wpos,x
        lda ins_pw,y            ; pulse width = value * 16
        pha
        asl
        asl
        asl
        asl
        sta v_pwlo,x
        pla
        lsr
        lsr
        lsr
        lsr
        sta v_pwhi,x
        lda ins_pwspd,y
        sta v_pwspd,x
        lda ins_vibdelay,y
        sta v_vdel,x
        lda ins_vibspeed,y
        lsr
        sta v_vphase,x          ; start mid-slope so vibrato centres on the note

        lda ins_filt,y
        beq @nofilt
        sta f_cut
        lda ins_fsweep,y
        sta f_sweep
        lda f_route
        ora vbit,x
        sta f_route
        jmp @adsr
@nofilt:
        lda vbit,x
        eor #$ff
        and f_route
        sta f_route

@adsr:  lda ins_sr,y
        pha
        lda ins_ad,y
        pha
        jsr voice_regs
        pla
        sta SID+5,y
        pla
        sta SID+6,y
        lda #1
        sta v_gate,x
        rts

; ---------------------------------------------------------------------------
; Frame update: wave table, frequency (slide, vibrato), pulse, SID registers.

effects:
        ldy v_wpos,x
        lda wt_wave,y
        cmp #$ff
        bne @wave
        lda wt_note,y
        tay
        lda wt_wave,y
@wave:  sta tmp_ctrl
        lda wt_note,y
        sta tmp_arp
        iny
        tya
        sta v_wpos,x
        lda v_gate,x
        bne @gated
        lda tmp_ctrl
        and #$fe
        sta tmp_ctrl
@gated:
        lda tmp_arp
        bmi @absnote
        clc
        adc v_note,x
        jmp @setnote
@absnote:
        and #$7f
@setnote:
        tay
        lda v_slide,x
        beq @noslide
        jsr do_slide
        jmp @vibrato
@noslide:
        lda freq_lo,y
        sta v_frlo,x
        lda freq_hi,y
        sta v_frhi,x

@vibrato:
        ldy v_ins,x
        lda ins_vibdepth,y
        beq @output
        sta tmp_depth
        lda v_vdel,x
        beq @vibgo
        dec v_vdel,x
        jmp @output
@vibgo: lda ins_vibspeed,y
        sta tmp_spd
        lda v_vphase,x
        cmp tmp_spd
        bcs @vibdown
        clc
        lda v_vofslo,x
        adc tmp_depth
        sta v_vofslo,x
        lda v_vofshi,x
        adc #0
        sta v_vofshi,x
        jmp @vibphase
@vibdown:
        sec
        lda v_vofslo,x
        sbc tmp_depth
        sta v_vofslo,x
        lda v_vofshi,x
        sbc #0
        sta v_vofshi,x
@vibphase:
        inc v_vphase,x
        lda tmp_spd
        asl
        cmp v_vphase,x
        bne @output
        lda #0
        sta v_vphase,x

@output:
        jsr voice_regs
        clc
        lda v_frlo,x
        adc v_vofslo,x
        sta SID+0,y
        lda v_frhi,x
        adc v_vofshi,x
        sta SID+1,y

        ; pulse width sweep, bouncing between $200 and $e00
        lda v_pwspd,x
        beq @pwout
        bmi @pwdown
        clc
        adc v_pwlo,x
        sta v_pwlo,x
        lda v_pwhi,x
        adc #0
        sta v_pwhi,x
        cmp #$0e
        bcc @pwout
        bcs @pwflip
@pwdown:
        clc
        adc v_pwlo,x
        sta v_pwlo,x
        lda v_pwhi,x
        adc #$ff
        sta v_pwhi,x
        cmp #$02
        bcs @pwout
@pwflip:
        lda v_pwspd,x
        eor #$ff
        clc
        adc #1
        sta v_pwspd,x
@pwout: lda v_pwlo,x
        sta SID+2,y
        lda v_pwhi,x
        sta SID+3,y
        lda tmp_ctrl
        sta SID+4,y
        rts

; Moves v_fr toward v_tg by v_slspd; ends the slide on arrival.
do_slide:
        lda v_frlo,x
        cmp v_tglo,x
        lda v_frhi,x
        sbc v_tghi,x
        bcs @down
        clc
        lda v_frlo,x
        adc v_slspd,x
        sta v_frlo,x
        lda v_frhi,x
        adc #0
        sta v_frhi,x
        lda v_frlo,x
        cmp v_tglo,x
        lda v_frhi,x
        sbc v_tghi,x
        bcc @done
        bcs @arrive
@down:  sec
        lda v_frlo,x
        sbc v_slspd,x
        sta v_frlo,x
        lda v_frhi,x
        sbc #0
        sta v_frhi,x
        lda v_frlo,x
        cmp v_tglo,x
        lda v_frhi,x
        sbc v_tghi,x
        bcs @done
@arrive:
        lda v_tglo,x
        sta v_frlo,x
        lda v_tghi,x
        sta v_frhi,x
        lda #0
        sta v_slide,x
@done:  rts
