SECTION code_user

PUBLIC _rules_play_ovl
PUBLIC _rules_check_ovl
PUBLIC _rules_hints_ovl
PUBLIC _rules_hints_clear_ovl

EXTERN _spectrum_board_is_legal_index
EXTERN _spectrum_board_view_redraw_square
EXTERN _netchesszx_board_theme_index
EXTERN _netchesszx_hinted_rows
EXTERN _spectrum_board_view_flipped
EXTERN board_theme_hint_inks
EXTERN compute_square_bc
EXTERN compute_attr_base
EXTERN set_square_attr_2x2
EXTERN compute_screen_base
EXTERN board_row
EXTERN board_col
EXTERN tmp_attr

; Historical chess legality/state entries remain fail-closed ABI tombstones.
_rules_play_ovl:
    ld l, 0
    ret

_rules_check_ovl:
    ld l, 0
    ret

; Show every legal placement. The shared resident Reversi core owns legality;
; this cold overlay owns only hint bookkeeping and pixels.
_rules_hints_ovl:
    ld a, (_netchesszx_board_theme_index)
    ld hl, board_theme_hint_inks
    ld e, a
    ld d, 0
    add hl, de
    ld a, (hl)
    ld (r_hint_ink), a
    xor a
rh_loop:
    ld (r_to), a
    ld l, a
    ld h, 0
    call _spectrum_board_is_legal_index
    ld a, l
    or a
    call nz, rh_draw_to
    ld a, (r_to)
    inc a
    cp 64
    jr nz, rh_loop
    ld l, 1
    ret

rh_draw_to:
    ld a, (r_to)
    ld c, a
    rrca
    rrca
    rrca
    and 7
    ld b, a
    ld a, c
    and 7
    ld c, a
    ld hl, _netchesszx_hinted_rows
    ld e, b
    ld d, 0
    add hl, de
    ld d, c
    inc d
    xor a
    scf
rh_rot_loop:
    rla
    dec d
    jr nz, rh_rot_loop
    or (hl)
    ld (hl), a
    ld a, (_spectrum_board_view_flipped)
    or a
    jr nz, rh_flipped
    ld a, 7
    sub b
    ld b, a
    jr rh_store_spec
rh_flipped:
    ld a, 7
    sub c
    ld c, a
rh_store_spec:
    ld hl, r_tmp
    ld (hl), b
    inc hl
    ld (hl), c
    inc hl
    ld a, (r_hint_ink)
    ld (hl), a
    dec hl
    dec hl
    jp draw_square_hint

_rules_hints_clear_ovl:
    ld hl, _netchesszx_hinted_rows
    ld d, 0
rhc_row_loop:
    ld c, (hl)
    ld (hl), 0
    ld a, c
    or a
    jr z, rhc_next_row
    ld e, 0
rhc_col_loop:
    srl c
    jr nc, rhc_skip_square
    push hl
    push de
    push bc
    ld l, d
    ld h, e
    push hl
    call _spectrum_board_view_redraw_square
    pop bc
    pop bc
    pop de
    pop hl
rhc_skip_square:
    inc e
    ld a, c
    or a
    jr nz, rhc_col_loop
rhc_next_row:
    inc hl
    inc d
    ld a, d
    cp 8
    jr nz, rhc_row_loop
    ld l, 1
    ret

draw_square_hint:
    ld a, (hl)
    ld (board_row), a
    inc hl
    ld a, (hl)
    ld (board_col), a
    inc hl
    ld a, (hl)
    ld (tmp_attr), a

    call compute_square_bc
    ld a, b
    call compute_attr_base
    ld a, c
    add a, l
    ld l, a
    ld a, (hl)
    and 0x78
    ld d, a
    ld a, (tmp_attr)
    and 0x07
    or d
    call set_square_attr_2x2

    ld a, b
    call compute_screen_base
    ld a, 6
    add a, h
    ld h, a
    ld a, c
    add a, l
    ld l, a

    ld de, 0x8001
    call draw_dot_row
    dec l
    inc h
    ld de, 0xc003
    call draw_dot_row

    ld a, b
    inc a
    call compute_screen_base
    ld a, c
    add a, l
    ld l, a

    ld de, 0xc003
    call draw_dot_row
    dec l
    inc h
    ld de, 0x8001
draw_dot_row:
    ld a, (hl)
    or e
    ld (hl), a
    inc l
    ld a, (hl)
    or d
    ld (hl), a
    ret

r_to:       DEFB 0
r_hint_ink: DEFB 0
; Shared overlay scratch; no state survives the return.
IFDEF NETCHESSZX_NEXT
r_tmp       EQU 0x3C2B
ELSE
r_tmp       EQU 0x672B
ENDIF
rules_tmp_size EQU 3
