SECTION code_user

EXTERN _board_apply_ovl_entry
EXTERN _board_seed_reveal_ovl_entry
EXTERN _board_set_morph_ovl_entry
EXTERN netchesszx_piece_sprites_16x16

PUBLIC _spectrum_gui_board_flipped, _spectrum_gui_active_coord_valid
PUBLIC _spectrum_gui_active_coord_row, _spectrum_gui_active_coord_col
PUBLIC _spectrum_frame_wait, _spectrum_uart_background_pump, _spectrum_gui_tick
PUBLIC _spectrum_board_view_redraw_square, _spectrum_flip_blit_frame
PUBLIC square_parity, compute_square_bc, board_row, board_col
PUBLIC tmp_char, tmp_scan, piece_row, piece_col, piece_char, piece_scan
PUBLIC scaled_row, scaled_col, _netchesszx_piece_set_index
PUBLIC _ms_rules_side, _ms_rules_is_over
PUBLIC _render_square_from_board, _spectrum_render_square_attr
PUBLIC _spectrum_gui_sync_board_coords, _spectrum_gui_hide_board_pieces
PUBLIC _spectrum_gui_set_board_pieces_visible
PUBLIC _spectrum_gui_board_pieces_visible_state, _spectrum_gui_about_visible_state

test_start:
    ld sp, 0xff00
    ld hl, 0xa000
    ld (log_ptr), hl
    ld hl, 0x5f60
    ld de, 0x5f61
    ld bc, 63
    ld (hl), '.'
    ldir
    ld hl, 0x5fe4
    ld de, 0x5fe5
    ld bc, 7
    ld (hl), 0
    ldir
    ld a, 19
    ld (0x5fed), a
    ld a, 'B'
    ld (0x5f73), a

    ; Exercise complete production APPLY with enabled and retained-DI waits.
    ld a, 1
    ld (frame_ei), a
    ld (_spectrum_gui_board_pieces_visible_state), a
    ei
    call test_apply
    ld a, 1
    ld (_spectrum_gui_board_flipped), a
    xor a
    ld (_spectrum_gui_board_pieces_visible_state), a
    ld (frame_ei), a
    di
    call test_apply

    ld a, 1
    ld (_spectrum_gui_about_visible_state), a
    call test_apply
    ld a, 2
    ld (_spectrum_gui_about_visible_state), a
    call test_apply
    xor a
    ld (_spectrum_gui_about_visible_state), a
    ld a, 0xff
    ld (0x5fed), a
    call test_apply

    ld a, 1
    ld (_spectrum_gui_board_pieces_visible_state), a
    call test_seed
    xor a
    ld (_spectrum_gui_board_pieces_visible_state), a
    call test_seed
    ld hl, morph_cases
    ld (morph_case_ptr), hl
    ld a, 6
    ld (morph_cases_left), a
test_morph_cases:
    ld hl, (morph_case_ptr)
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    push hl
    ex de, hl
    ld de, 0x676b
    ld bc, 64
    ldir
    pop hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    push hl
    ex de, hl
    ld de, 0x662b
    ld bc, 64
    ldir
    pop hl
    ld a, (hl)
    inc hl
    ld (_netchesszx_piece_set_index), a
    ld a, 'M'
    call log_a
    ld a, (_netchesszx_piece_set_index)
    call log_a
    ld a, (hl)
    inc hl
    ld (_spectrum_gui_board_flipped), a
    call log_a
    ld (morph_case_ptr), hl
    ld a, 1
    ld (morph_mode), a
    call test_morph
    xor a
    ld (morph_mode), a
    ld a, (morph_cases_left)
    dec a
    ld (morph_cases_left), a
    jr nz, test_morph_cases

    ld hl, (log_ptr)
    ld (0x7002), hl
    jp 0

test_apply:
    ld a, 'A'
    call log_a
    ld ix, 0x1234
    ld iy, 0x5c3a
    call _board_apply_ovl_entry
    jr test_registers
test_seed:
    ld a, 'S'
    call log_a
    ld ix, 0x1234
    ld iy, 0x5c3a
    call _board_seed_reveal_ovl_entry
    jr test_registers
test_morph:
    ld ix, 0x1234
    ld iy, 0x5c3a
    call _board_set_morph_ovl_entry
test_registers:
    push ix
    pop hl
    ld de, 0x1234
    or a
    sbc hl, de
    jr nz, test_fail
    push iy
    pop hl
    ld de, 0x5c3a
    or a
    sbc hl, de
    jr nz, test_fail
    ld hl, 0
    add hl, sp
    ld de, 0xfefe
    or a
    sbc hl, de
    ret z
test_fail:
    ld a, 1
    ld (0x7000), a
    jp 0

log_a:
    push hl
    ld hl, (log_ptr)
    ld (hl), a
    inc hl
    ld (log_ptr), hl
    pop hl
    ret

_render_square_from_board:
    ld hl, 2
    add hl, sp
    ld a, 'R'
    call log_a
    ld a, (hl)
    call log_a
    inc hl
    ld a, (hl)
    call log_a
    ld a, (_spectrum_gui_board_pieces_visible_state)
    call log_a
    jr clobber
_spectrum_render_square_attr:
    ld a, 'P'
    call log_a
    ld b, 4
attr_loop:
    ld a, (hl)
    call log_a
    inc hl
    djnz attr_loop
    jr clobber
_spectrum_gui_sync_board_coords:
    ld a, 'C'
    jp log_a
_spectrum_gui_hide_board_pieces:
    ld a, 'H'
    call log_a
    xor a
    ld (_spectrum_gui_board_pieces_visible_state), a
    ret
_spectrum_gui_set_board_pieces_visible:
    ld a, l
    ld (_spectrum_gui_board_pieces_visible_state), a
    ld a, 'V'
    jp log_a
_spectrum_frame_wait:
    ld a, i
    ld a, 'D'
    jp po, frame_log
    ld a, 'E'
frame_log:
    call log_a
    ld a, (frame_ei)
    or a
    ret z
    ei
    ret
_spectrum_uart_background_pump:
    ld a, 'U'
    call log_a
    jr clobber
_spectrum_gui_tick:
    ld a, 'T'
    call log_a
clobber:
    ld hl, 0xdead
    ld de, 0xbeef
    ld bc, 0xface
    ret

_ms_rules_side:
_ms_rules_is_over:
    ld l, 0
    ret
_spectrum_board_view_redraw_square:
    ret
_spectrum_flip_blit_frame:
    ld a, (morph_mode)
    or a
    ret z
    ld a, 'F'
    call log_a
    ld a, (tmp_char)
    call log_a
    ld a, (board_row)
    call log_a
    ld a, (board_col)
    call log_a
    ld a, (piece_char)
    call log_a
    push de
    pop hl
    ld b, 32
morph_frame_log:
    ld a, (hl)
    call log_a
    inc hl
    djnz morph_frame_log
    ret
square_parity:
    ld a, (board_row)
    ld d, a
    ld a, (board_col)
    add a, d
    and 1
    xor 1
    ret
compute_square_bc:
    ret

log_ptr: DW 0
frame_ei: DEFB 0
morph_mode: DEFB 0
morph_case_ptr: DEFW 0
morph_cases_left: DEFB 0
; old face-A pointer, new face-A pointer, selected set, board orientation
morph_cases:
    DEFW netchesszx_piece_sprites_16x16 + 128
    DEFW netchesszx_piece_sprites_16x16
    DEFB 0, 0
    DEFW netchesszx_piece_sprites_16x16 + 128
    DEFW netchesszx_piece_sprites_16x16
    DEFB 0, 1
    DEFW netchesszx_piece_sprites_16x16
    DEFW netchesszx_piece_sprites_16x16 + 64
    DEFB 1, 0
    DEFW netchesszx_piece_sprites_16x16
    DEFW netchesszx_piece_sprites_16x16 + 64
    DEFB 1, 1
    DEFW netchesszx_piece_sprites_16x16 + 64
    DEFW netchesszx_piece_sprites_16x16 + 128
    DEFB 2, 0
    DEFW netchesszx_piece_sprites_16x16 + 64
    DEFW netchesszx_piece_sprites_16x16 + 128
    DEFB 2, 1
_spectrum_gui_board_flipped: DEFB 0
_spectrum_gui_active_coord_valid: DEFB 0
_spectrum_gui_active_coord_row: DEFB 0
_spectrum_gui_active_coord_col: DEFB 0
_spectrum_gui_about_visible_state: DEFB 0
_spectrum_gui_board_pieces_visible_state: DEFB 0
_netchesszx_piece_set_index: DEFB 0
board_row: DEFB 0
board_col: DEFB 0
tmp_char: DEFB 0
tmp_scan: DEFB 0
piece_row: DEFB 0
piece_col: DEFB 0
piece_char: DEFB 0
piece_scan: DEFB 0
scaled_row: DEFB 0
scaled_col: DEFB 0
