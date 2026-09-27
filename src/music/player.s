; Compact SID music driver (ca65 syntax).
;
;   music_init   A/X = song descriptor (low/high); call before playing,
;                or again to switch songs (with interrupts off)
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
; Song descriptor: .word addresses of, in this order:
;   tempo byte, order_lo, order_hi, pat_lo, pat_hi, ins_ad, ins_sr, ins_wave,
;   ins_pw, ins_pwspd, ins_vibdepth, ins_vibdelay, ins_vibspeed, ins_filt,
;   ins_fsweep, wt_wave, wt_note, title text
; music_init writes these into the table-reading instructions below (marked
; "<- name"), so the player runs at full speed with any song.
;
; Uses zero page $FB-$FE.

        .export music_init, music_play, music_mute

SID       = $d400
MUTED_REGS = $19                ; offset of SID $d419-$d41f: read-only/unused,
                                ; so writes of a muted voice go nowhere
ptr       = $fb
site      = $fd                 ; music_init: operand being patched
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
; Patch sites: operand address and descriptor offset (field * 2)
site_lo:
        .byte <sm_0
        .byte <sm_1
        .byte <sm_2
        .byte <sm_3
        .byte <sm_4
        .byte <sm_5
        .byte <sm_6
        .byte <sm_7
        .byte <sm_8
        .byte <sm_9
        .byte <sm_10
        .byte <sm_11
        .byte <sm_12
        .byte <sm_13
        .byte <sm_14
        .byte <sm_15
        .byte <sm_16
        .byte <sm_17
        .byte <sm_18
        .byte <sm_19
site_hi:
        .byte >sm_0
        .byte >sm_1
        .byte >sm_2
        .byte >sm_3
        .byte >sm_4
        .byte >sm_5
        .byte >sm_6
        .byte >sm_7
        .byte >sm_8
        .byte >sm_9
        .byte >sm_10
        .byte >sm_11
        .byte >sm_12
        .byte >sm_13
        .byte >sm_14
        .byte >sm_15
        .byte >sm_16
        .byte >sm_17
        .byte >sm_18
        .byte >sm_19
site_field:
        .byte  2          ; song_order_lo
        .byte  4          ; song_order_hi
        .byte  6          ; pat_lo
        .byte  8          ; pat_hi
        .byte  0          ; song_tempo
        .byte 14          ; ins_wave
        .byte 16          ; ins_pw
        .byte 18          ; ins_pwspd
        .byte 22          ; ins_vibdelay
        .byte 24          ; ins_vibspeed
        .byte 26          ; ins_filt
        .byte 28          ; ins_fsweep
        .byte 12          ; ins_sr
        .byte 10          ; ins_ad
        .byte 30          ; wt_wave
        .byte 32          ; wt_note
        .byte 30          ; wt_wave
        .byte 32          ; wt_note
        .byte 20          ; ins_vibdepth
        .byte 24          ; ins_vibspeed
NSITES = 20


.segment "CODE"

music_init:
        sta ptr                 ; song descriptor
        stx ptr+1
        ldx #NSITES - 1
music_init_patch: lda site_lo,x
        sta site
        lda site_hi,x
        sta site+1
        ldy site_field,x
        lda (ptr),y
        pha
        iny
        lda (ptr),y
        ldy #1
        sta (site),y
        pla
        dey
        sta (site),y
        dex
        bpl music_init_patch

        lda #0
        sta music_mute
        ldx #vars_end - vars_start - 1
music_init_clr:   sta vars_start,x
        dex
        bpl music_init_clr
        ldx #$18
music_init_sid:   sta SID,x
        dex
        bpl music_init_sid
        ldx #2
music_init_voice: lda #1
        sta v_count,x
        sta v_dur,x
        jsr next_pattern
        dex
        bpl music_init_voice
        rts

music_play:
        ldx #2
music_play_voice: jsr do_voice
        dex
        bpl music_play_voice

        ; filter sweep, clamped to 0..255
        lda f_sweep
        beq music_play_fout
        bmi music_play_fneg
        clc
        adc f_cut
        bcc music_play_fset
        lda #$ff
        bne music_play_fset
music_play_fneg:  clc
        adc f_cut
        bcs music_play_fset
        lda #0
music_play_fset:  sta f_cut
music_play_fout:  lda #0
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
        bne do_voice_hr
        jsr read_events
do_voice_hr:    lda v_count,x
        cmp #HR_FRAMES
        bne do_voice_fx
        jsr peek_byte           ; is the next event a fresh note?
        cmp #$60                ; rest: gate goes off by itself
        beq do_voice_fx
        cmp #$61                ; tie: keep sounding
        beq do_voice_fx
        cmp #$ff
        beq do_voice_dohr
        cmp #$c0                ; slide: legato, no restart
        bcs do_voice_fx
do_voice_dohr:  jsr voice_regs
        lda #0
        sta v_gate,x
        sta SID+5,y
        sta SID+6,y
do_voice_fx:    jmp effects

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
sm_0 = * + 1
        lda $ffff,x   ; <- song_order_lo
        sta ptr
sm_1 = * + 1
        lda $ffff,x   ; <- song_order_hi
        sta ptr+1
next_pattern_next:  ldy v_ordpos,x
        lda (ptr),y
        iny
        cmp #$ff
        bne next_pattern_notjump
        lda (ptr),y
        sta v_ordpos,x
        jmp next_pattern_next
next_pattern_notjump:
        pha
        tya
        sta v_ordpos,x
        pla
        cmp #$80
        bcc next_pattern_pattern
        sec
        sbc #$a0
        sta v_trans,x
        jmp next_pattern_next
next_pattern_pattern:
        tay
sm_2 = * + 1
        lda $ffff,y   ; <- pat_lo
        sta v_patlo,x
sm_3 = * + 1
        lda $ffff,y   ; <- pat_hi
        sta v_pathi,x
        rts

; Processes pattern bytes up to and including the next note/rest/tie.
read_events:
read_events_next:  jsr read_byte
        cmp #$ff
        bne read_events_notend
        jsr next_pattern
        jmp read_events_next
read_events_notend:
        cmp #$c0
        bcc read_events_notslide
        jsr read_byte
        sta v_slpend,x
        jmp read_events_next
read_events_notslide:
        cmp #$a0
        bcc read_events_notins
        and #$1f
        sta v_ins,x
        jmp read_events_next
read_events_notins:
        cmp #$80
        bcc read_events_event
        and #$1f
        clc
        adc #1
        sta v_dur,x
        jmp read_events_next

read_events_event: pha                     ; frames = rows * tempo
        lda #0
        ldy v_dur,x
        clc
sm_4 = * + 1
read_events_mul:   adc $ffff   ; <- song_tempo
        dey
        bne read_events_mul
        sta v_count,x
        pla

        cmp #$60
        bne read_events_notrest
        lda #0
        sta v_gate,x
        rts
read_events_notrest:
        cmp #$61
        bne read_events_note
        rts

read_events_note:  clc
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
sm_5 = * + 1
        lda $ffff,y   ; <- ins_wave
        sta v_wpos,x
sm_6 = * + 1
        lda $ffff,y            ; pulse width = value * 16   ; <- ins_pw
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
sm_7 = * + 1
        lda $ffff,y   ; <- ins_pwspd
        sta v_pwspd,x
sm_8 = * + 1
        lda $ffff,y   ; <- ins_vibdelay
        sta v_vdel,x
sm_9 = * + 1
        lda $ffff,y   ; <- ins_vibspeed
        lsr
        sta v_vphase,x          ; start mid-slope so vibrato centres on the note

sm_10 = * + 1
        lda $ffff,y   ; <- ins_filt
        beq trigger_nofilt
        sta f_cut
sm_11 = * + 1
        lda $ffff,y   ; <- ins_fsweep
        sta f_sweep
        lda f_route
        ora vbit,x
        sta f_route
        jmp trigger_adsr
trigger_nofilt:
        lda vbit,x
        eor #$ff
        and f_route
        sta f_route

sm_12 = * + 1
trigger_adsr:  lda $ffff,y   ; <- ins_sr
        pha
sm_13 = * + 1
        lda $ffff,y   ; <- ins_ad
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
sm_14 = * + 1
        lda $ffff,y   ; <- wt_wave
        cmp #$ff
        bne effects_wave
sm_15 = * + 1
        lda $ffff,y   ; <- wt_note
        tay
sm_16 = * + 1
        lda $ffff,y   ; <- wt_wave
effects_wave:  sta tmp_ctrl
sm_17 = * + 1
        lda $ffff,y   ; <- wt_note
        sta tmp_arp
        iny
        tya
        sta v_wpos,x
        lda v_gate,x
        bne effects_gated
        lda tmp_ctrl
        and #$fe
        sta tmp_ctrl
effects_gated:
        lda tmp_arp
        bmi effects_absnote
        clc
        adc v_note,x
        jmp effects_setnote
effects_absnote:
        and #$7f
effects_setnote:
        tay
        lda v_slide,x
        beq effects_noslide
        jsr do_slide
        jmp effects_vibrato
effects_noslide:
        lda freq_lo,y
        sta v_frlo,x
        lda freq_hi,y
        sta v_frhi,x

effects_vibrato:
        ldy v_ins,x
sm_18 = * + 1
        lda $ffff,y   ; <- ins_vibdepth
        beq effects_output
        sta tmp_depth
        lda v_vdel,x
        beq effects_vibgo
        dec v_vdel,x
        jmp effects_output
sm_19 = * + 1
effects_vibgo: lda $ffff,y   ; <- ins_vibspeed
        sta tmp_spd
        lda v_vphase,x
        cmp tmp_spd
        bcs effects_vibdown
        clc
        lda v_vofslo,x
        adc tmp_depth
        sta v_vofslo,x
        lda v_vofshi,x
        adc #0
        sta v_vofshi,x
        jmp effects_vibphase
effects_vibdown:
        sec
        lda v_vofslo,x
        sbc tmp_depth
        sta v_vofslo,x
        lda v_vofshi,x
        sbc #0
        sta v_vofshi,x
effects_vibphase:
        inc v_vphase,x
        lda tmp_spd
        asl
        cmp v_vphase,x
        bne effects_output
        lda #0
        sta v_vphase,x

effects_output:
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
        beq effects_pwout
        bmi effects_pwdown
        clc
        adc v_pwlo,x
        sta v_pwlo,x
        lda v_pwhi,x
        adc #0
        sta v_pwhi,x
        cmp #$0e
        bcc effects_pwout
        bcs effects_pwflip
effects_pwdown:
        clc
        adc v_pwlo,x
        sta v_pwlo,x
        lda v_pwhi,x
        adc #$ff
        sta v_pwhi,x
        cmp #$02
        bcs effects_pwout
effects_pwflip:
        lda v_pwspd,x
        eor #$ff
        clc
        adc #1
        sta v_pwspd,x
effects_pwout: lda v_pwlo,x
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
        bcs do_slide_down
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
        bcc do_slide_done
        bcs do_slide_arrive
do_slide_down:  sec
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
        bcs do_slide_done
do_slide_arrive:
        lda v_tglo,x
        sta v_frlo,x
        lda v_tghi,x
        sta v_frhi,x
        lda #0
        sta v_slide,x
do_slide_done:  rts
