SECTION code_user

PUBLIC _board_apply_ovl_entry
PUBLIC _board_snapshot_save_ovl_entry
PUBLIC _board_snapshot_restore_ovl_entry
PUBLIC _board_undo_restore_ovl_entry
PUBLIC _board_cell_transition_ovl_entry
PUBLIC _board_set_morph_ovl_entry
PUBLIC _board_piece_reflection_ovl_entry
PUBLIC boa_wait_frame
PUBLIC _board_seed_reveal_ovl_entry

IFNDEF NETCHESSZX_NEXT
IFNDEF NETCHESSZX_SPECTRANEXT
defc MIRRORSHIFT_CLASSIC_BOARD_ANIMATION = 1
EXTERN _render_square_from_board
EXTERN _spectrum_render_square_attr
EXTERN _spectrum_gui_sync_board_coords
EXTERN _spectrum_gui_hide_board_pieces
EXTERN _spectrum_gui_set_board_pieces_visible
EXTERN _spectrum_gui_about_visible_state
ENDIF
ENDIF

EXTERN _spectrum_gui_board_flipped
EXTERN _spectrum_gui_active_coord_valid
EXTERN _spectrum_gui_active_coord_row
EXTERN _spectrum_gui_active_coord_col
EXTERN _spectrum_frame_wait
EXTERN _spectrum_uart_background_pump
EXTERN _spectrum_gui_tick
EXTERN _spectrum_board_view_redraw_square
EXTERN _spectrum_flip_blit_frame
EXTERN square_parity
EXTERN compute_square_bc
EXTERN board_row
EXTERN board_col
EXTERN tmp_char
EXTERN tmp_scan
EXTERN piece_row
EXTERN piece_col
EXTERN piece_char
EXTERN piece_scan
EXTERN scaled_row
EXTERN scaled_col
EXTERN _netchesszx_piece_set_index
EXTERN _ms_rules_side
EXTERN _ms_rules_is_over
IFDEF NETCHESSZX_NEXT
EXTERN next_draw_piece_sprite_16x16
NEXT_CAPTURE_PATTERN_BASE EQU 28
NEXT_PIECE_FINAL_PATTERN_BASE EQU 56
ENDIF

MIRRORSHIFT_BOARD_STATE EQU 0x5f60
MIRRORSHIFT_FLIP_ROWS EQU 0x5fe4
MIRRORSHIFT_LAST_SQUARE EQU 0x5fed
IFDEF NETCHESSZX_NEXT
MIRRORSHIFT_PIECE_MASKS EQU 0x3b2b
MIRRORSHIFT_FLIP_SCRATCH EQU 0x3c2b
ELSE
MIRRORSHIFT_PIECE_MASKS EQU 0x662b
MIRRORSHIFT_FLIP_SCRATCH EQU 0x672b
ENDIF
MIRRORSHIFT_PIECE_REFLECTION EQU MIRRORSHIFT_PIECE_MASKS + 64
MIRRORSHIFT_SET_PREV_MASKS EQU MIRRORSHIFT_FLIP_SCRATCH + 64
MIRRORSHIFT_REFLECT_BIT_PTR EQU 0x674b
MIRRORSHIFT_REFLECT_BIT_BUF EQU 0x674d
MIRRORSHIFT_REFLECT_BIT_LEFT EQU 0x674e
MIRRORSHIFT_REFLECT_ROW_BITS EQU 0x674f
MIRRORSHIFT_REFLECT_X_BASE EQU 0x6750
MIRRORSHIFT_REFLECT_PHYSICAL EQU 0x6751
MIRRORSHIFT_REFLECT_Y EQU 0x6752
MIRRORSHIFT_REFLECT_ROWS EQU 0x6753
MIRRORSHIFT_REFLECT_FRAME EQU 0x6754
MIRRORSHIFT_REFLECT_RESULT EQU 0x6755
MIRRORSHIFT_REFLECT_WIDTH EQU 0x6756
MIRRORSHIFT_REFLECT_X EQU 0x6757
MIRRORSHIFT_REFLECT_TARGET EQU 0x6758
MIRRORSHIFT_REFLECT_CURSOR EQU 0x6759
MIRRORSHIFT_REFLECT_TARGETS EQU 0x675a ; three logical squares
MIRRORSHIFT_REFLECT_COUNT EQU 0x675d
MIRRORSHIFT_REFLECT_INDEX EQU 0x675e
MIRRORSHIFT_REFLECT_STREAM EQU 0x675f ; saved pointer, buffer, bits left
    DEFB 8
    DW _board_apply_ovl_entry
    DW _board_snapshot_save_ovl_entry
    DW _board_snapshot_restore_ovl_entry
    DW _board_undo_restore_ovl_entry
    DW _board_cell_transition_ovl_entry
    DW _board_set_morph_ovl_entry
    DW _board_piece_reflection_ovl_entry
    DW _board_seed_reveal_ovl_entry

_board_apply_ovl_entry:
IFDEF MIRRORSHIFT_CLASSIC_BOARD_ANIMATION
    ld a, (_spectrum_gui_about_visible_state)
    or a
    ret nz
    ld a, (MIRRORSHIFT_LAST_SQUARE)
    or a
    ret m
ENDIF
    push ix
IFDEF MIRRORSHIFT_CLASSIC_BOARD_ANIMATION
    call boa_flash_square
ENDIF
    ; flash_square already leaves the placed disc in its stable state. Keep
    ; the reveal beat, but do not repaint that same square at overlay entry.
    call boa_wait_frame
    call boa_wait_frame
IFDEF NETCHESSZX_NEXT
    xor a
boa_next_phase:
    push af
    ld c, a
    ld a, (MIRRORSHIFT_LAST_SQUARE)
    ld e, a
    ld d, 0
    ld hl, MIRRORSHIFT_BOARD_STATE
    add hl, de
    ld a, (hl)
    cp 'B'
    ld a, c
    jr z, boa_next_pattern_ready
    ld a, 13
    sub c
boa_next_pattern_ready:
    add a, NEXT_CAPTURE_PATTERN_BASE
    ld (piece_char), a
    call boa_draw_rows_next
    call boa_wait_frame
    call boa_wait_frame
    pop af
    inc a
    cp 14
    jr nz, boa_next_phase

    ; Leave every captured disc on the stable full-size selected-set pair.
    ld a, (MIRRORSHIFT_LAST_SQUARE)
    ld e, a
    ld d, 0
    ld hl, MIRRORSHIFT_BOARD_STATE
    add hl, de
    ld a, (hl)
    sub 'A'
    add a, NEXT_PIECE_FINAL_PATTERN_BASE
    ld (piece_char), a
    call boa_draw_rows_next
ELSE
    ld a, 1
boa_phase:
    ; Phase 14 is already the new face at the identity map. Skip it and let
    ; phase 15 perform the single canonical redraw of the stable board state.
    cp 14
    jr nz, boa_capture_phase_ready
    inc a
boa_capture_phase_ready:
    push af
    ld (tmp_char), a
    cp 15
    call nz, boa_prepare_frames
    ld a, (MIRRORSHIFT_LAST_SQUARE)
    ld e, a
    ld d, 0
    ld hl, MIRRORSHIFT_BOARD_STATE
    add hl, de
    ld a, (hl)
    sub 'A'
    ld (piece_char), a
    call boa_draw_rows
    call boa_wait_frame
    call boa_wait_frame
    pop af
    inc a
    cp 16
    jr nz, boa_phase
ENDIF
    pop ix
    ld hl, 1
    ret

IFDEF NETCHESSZX_NEXT
; Update only the captured hardware sprites. The board sprite, ULA bitmap and
; ULA attributes are never hidden or rewritten by the Next capture turn.
boa_draw_rows_next:
    xor a
    ld (scaled_row), a
boa_next_row:
    ld e, a
    ld d, 0
    ld hl, MIRRORSHIFT_FLIP_ROWS
    add hl, de
    ld a, (hl)
    ld (piece_scan), a
    xor a
    ld (scaled_col), a
boa_next_col:
    ld a, (piece_scan)
    rrca
    ld (piece_scan), a
    call c, boa_draw_one_next
    ld a, (scaled_col)
    inc a
    ld (scaled_col), a
    cp 8
    jr nz, boa_next_col
    ld a, (scaled_row)
    inc a
    ld (scaled_row), a
    cp 8
    jr nz, boa_next_row
    ret

boa_draw_one_next:
    ld a, (scaled_row)
    ld d, a
    ld a, (scaled_col)
    ld e, a
    ld a, (_spectrum_gui_board_flipped)
    or a
    jr nz, boa_next_coords_flipped
    ld a, 7
    sub d
    ld d, a
    jr boa_next_coords_ready
boa_next_coords_flipped:
    ld a, 7
    sub e
    ld e, a
boa_next_coords_ready:
    ld a, d
    ld (board_row), a
    ld a, e
    ld (board_col), a
    ld a, (piece_char)
    or 0x80
    jp next_draw_piece_sprite_16x16
ELSE

; Build the two physical A/B frames for this phase. X is never changed. Each
; top-half source row is mapped by an exact 4.4 DDA; its bottom partner goes to
; 15-y'. The second half reverses the phase and swaps the logical face.
boa_prepare_frames:
    ld ix, MIRRORSHIFT_PIECE_MASKS
    ld hl, MIRRORSHIFT_PIECE_MASKS + 32
    jr boa_prepare_ready

boa_prepare_morph_a:
boa_prepare_morph_b:
    ld ix, MIRRORSHIFT_SET_PREV_MASKS + 32
    ld hl, MIRRORSHIFT_PIECE_MASKS + 32
    ld a, (tmp_char)
    jr boa_prepare_ready

boa_prepare_ready:
    ; Preserve caller IFF while borrowing IY. NMOS-safe Zilog sampling:
    ; an interrupt racing LD A,I leaves its return PC below SP.
    ld e, a
    push hl
    ld hl, 0
    push hl
    pop hl
    scf
    ld a, i
    jp pe, boa_iff_sampled
    dec sp
    dec sp
    pop hl
    ld a, h
    or l
    jr z, boa_iff_sampled
    scf
boa_iff_sampled:
    di
    pop hl
    push af
    push iy
    ld a, e
    push hl
    cp 8
    jr c, boa_first_half
    cpl
    and 0x0f
    ld c, 1
    jr boa_phase_ready
boa_first_half:
    ld c, 0
boa_phase_ready:
    add a, a
    ld e, a
    ld a, c
    ld (tmp_scan), a
    ld d, 0
    ld h, d
    ld l, e
    ld de, boa_phase_params
    add hl, de
    ld d, (hl)
    inc hl
    ld e, (hl)
    push de

    ld hl, MIRRORSHIFT_FLIP_SCRATCH
    ld de, MIRRORSHIFT_FLIP_SCRATCH + 1
    ld bc, 63
    ld (hl), 0
    ldir

    pop de
    push de
    push ix
    pop iy
    ld bc, 30
    add iy, bc
    ld c, 0x2b
    call boa_project_face
    pop de
    pop ix
    push ix
    pop iy
    ld bc, 30
    add iy, bc
    ld c, 0x4b
    call boa_project_face
    pop iy
    pop af
    ret nc
    ei
    ret

boa_project_face:
    ld b, 8
boa_project_row:
    ld a, d
    rrca
    rrca
    rrca
    rrca
    and 0x0f
    add a, a
    push af
    add a, c
    ld l, a
    ld h, 0x67
    ld a, (ix+0)
    or (hl)
    ld (hl), a
    inc hl
    ld a, (ix+1)
    or (hl)
    ld (hl), a

    pop af
    neg
    add a, c
    add a, 30
    ld l, a
    ld a, (iy+0)
    or (hl)
    ld (hl), a
    inc hl
    ld a, (iy+1)
    or (hl)
    ld (hl), a

    inc ix
    inc ix
    dec iy
    dec iy
    ld a, d
    add a, e
    ld d, a
    djnz boa_project_row
    ret

boa_draw_rows:
    xor a
    ld (scaled_row), a
boa_row:
    ld e, a
    ld d, 0
    ld hl, MIRRORSHIFT_FLIP_ROWS
    add hl, de
    ld a, (hl)
    ld (piece_scan), a
    xor a
    ld (scaled_col), a
boa_col:
    ld a, (piece_scan)
    rrca
    ld (piece_scan), a
    call c, boa_draw_one
    ld a, (scaled_col)
    inc a
    ld (scaled_col), a
    cp 8
    jr nz, boa_col
    ld a, (scaled_row)
    inc a
    ld (scaled_row), a
    cp 8
    jr nz, boa_row
    ret

boa_draw_one:
    ld a, (tmp_char)
    cp 15
    jr z, boa_redraw_one
    ld a, (scaled_row)
    ld d, a
    ld a, (scaled_col)
    ld e, a
    ld a, (_spectrum_gui_board_flipped)
    or a
    jr nz, boa_coords_flipped
    ld a, 7
    sub d
    ld d, a
    jr boa_coords_ready
boa_coords_flipped:
    ld a, 7
    sub e
    ld e, a
boa_coords_ready:
    ld a, d
    ld (board_row), a
    ld a, e
    ld (board_col), a
    call square_parity
    ld hl, piece_char
    xor (hl)
    ld hl, tmp_scan
    xor (hl)
    xor 1
    ld de, MIRRORSHIFT_FLIP_SCRATCH
    jr z, boa_source_ready
    ld de, MIRRORSHIFT_FLIP_SCRATCH + 32
boa_source_ready:
    call compute_square_bc
    ld a, b
    ld (piece_row), a
    ld a, c
    ld (piece_col), a
    jp _spectrum_flip_blit_frame

boa_redraw_one:
    ld a, (scaled_row)
    ld l, a
    ld a, (scaled_col)
    ld h, a
    push hl
    call _spectrum_board_view_redraw_square
    pop bc
    ret
ENDIF

boa_wait_frame:
    call _spectrum_frame_wait
    call _spectrum_uart_background_pump
    call _spectrum_gui_tick
    jp _spectrum_uart_background_pump

IFDEF MIRRORSHIFT_CLASSIC_BOARD_ANIMATION
; Keep logical coordinates and loop counts across GUI/UART callbacks, which
; may reuse the resident renderer's scratch registers and bytes.
boa_wait_frames:
    push bc
    push hl
    call boa_wait_frame
    pop hl
    pop bc
    djnz boa_wait_frames
    ret

boa_render_square:
    push hl
    call _render_square_from_board
    pop hl
    ret

boa_flash_square:
    ld a, (MIRRORSHIFT_LAST_SQUARE)
    ld h, a
    and 7
    ld l, a
    ld a, h
    rrca
    rrca
    rrca
    and 31
    ld h, l
    ld l, a
    call boa_render_square
    ld b, 2
boa_pulse:
    push bc
    push hl
    ld e, h
    ld d, l
    ld a, (_spectrum_gui_board_flipped)
    or a
    ld a, 7
    jr nz, boa_pulse_flipped
    sub d
    ld d, a
    jr boa_pulse_coords_ready
boa_pulse_flipped:
    sub e
    ld e, a
boa_pulse_coords_ready:
    ld a, d
    ld (MIRRORSHIFT_FLIP_SCRATCH), a
    ld a, e
    ld (MIRRORSHIFT_FLIP_SCRATCH + 1), a
    ld a, 0xff
    ld (MIRRORSHIFT_FLIP_SCRATCH + 2), a
    ld a, l
    add a, a
    add a, a
    add a, a
    add a, h
    ld e, a
    ld d, 0
    ld hl, MIRRORSHIFT_BOARD_STATE
    add hl, de
    ld a, (hl)
    ld (MIRRORSHIFT_FLIP_SCRATCH + 3), a
    ld hl, MIRRORSHIFT_FLIP_SCRATCH
    call _spectrum_render_square_attr
    pop hl
    ld b, 6
    call boa_wait_frames
    call boa_render_square
    ld b, 6
    call boa_wait_frames
    pop bc
    djnz boa_pulse
    ret

_board_seed_reveal_ovl_entry:
    push ix
    call _spectrum_gui_sync_board_coords
    call _spectrum_gui_hide_board_pieces
    ld l, 1
    call _spectrum_gui_set_board_pieces_visible
    ld ix, boa_seed_coords
    ld b, 4
boa_seed_reveal:
    push bc
    ld l, (ix+0)
    ld h, (ix+1)
    inc ix
    inc ix
    call boa_render_square
    ld b, 5
    call boa_wait_frames
    pop bc
    djnz boa_seed_reveal
    pop ix
    ld hl, 1
    ret
boa_seed_coords:
    DEFB 3,3, 3,4, 4,3, 4,4
ENDIF

; 4.4 start/step pairs for phases 0..7. These reproduce the approved rounded
; horizontal-axis maps; the bottom half mirrors the top vertically.
boa_phase_params:
    DEFB 15,15, 15,15, 18,14, 24,14
    DEFB 42,11, 64,8, 84,6, 112,0

_board_set_morph_ovl_entry:
    push ix
IFDEF NETCHESSZX_NEXT
    xor a
boa_morph_phase:
    push af
    ld (tmp_char), a
    call boa_morph_show_next_phase
    call boa_wait_frame
    pop af
    inc a
    ; Skip the last 15,15 DDA. Finalize commits the real pair; playing both
    ; stamps the new face twice and reads as a reprint of the same colour.
    cp 13
    jr nz, boa_morph_phase
ELSE
    ld a, 1
boa_morph_phase:
    push af
    ld (tmp_char), a
    call boa_prepare_morph_a
    call boa_morph_draw_a
    call boa_morph_draw_b
    call boa_wait_frame
    pop af
    inc a
    ; Phases 1..14 already end on the new face at identity. Phase 15 redrew
    ; the seeds and reprinted the original colour after the coin-turn.
    cp 15
    jr nz, boa_morph_phase
ENDIF
    pop ix
    ld hl, 1
    ret

IFDEF NETCHESSZX_NEXT
; The graphics bank has already uploaded fourteen full-colour A/B pattern
; pairs. Change only the four hardware-sprite patterns; the ULA board pixels
; and attributes remain untouched throughout the setup morph.
boa_morph_show_next_phase:
    ld a, (tmp_char)
    add a, a
    ld (piece_char), a
    ld d, 3
    ld e, 4
    call boa_morph_show_next_one
    ld d, 4
    ld e, 3
    call boa_morph_show_next_one
    ld hl, piece_char
    inc (hl)
    ld d, 3
    ld e, 3
    call boa_morph_show_next_one
    ld d, 4
    ld e, 4
    jr boa_morph_show_next_one
ELSE
; The reset position is B A / A B. Both logical sides use physical face 1 in
; these four squares: A is on light cells and B on dark cells, whose opposite
; attributes turn the same bitmap into white and black discs respectively.
; square_parity converts the one-axis display transform back to logical board
; parity, so these selector bits match the A=0/B=1 cell values.
boa_morph_draw_a:
    ld a, 1
    ld (piece_char), a
    ld a, 3
    ld (scaled_row), a
    ld (scaled_col), a
    call boa_draw_one
    ld a, 4
    ld (scaled_row), a
    ld (scaled_col), a
    jp boa_draw_one

boa_morph_draw_b:
    xor a
    ld (piece_char), a
    ld a, 3
    ld (scaled_row), a
    inc a
    ld (scaled_col), a
    call boa_draw_one
    ld a, 4
    ld (scaled_row), a
    dec a
    ld (scaled_col), a
    jp boa_draw_one
ENDIF

IFDEF NETCHESSZX_NEXT
boa_morph_show_next_one:
    ld a, (_spectrum_gui_board_flipped)
    or a
    jr nz, boa_morph_next_flipped
    ld a, 7
    sub d
    ld d, a
    jr boa_morph_next_coords_ready
boa_morph_next_flipped:
    ld a, 7
    sub e
    ld e, a
boa_morph_next_coords_ready:
    ld a, d
    ld (board_row), a
    ld a, e
    ld (board_col), a
    ld a, (piece_char)
    or 0x80
    jp next_draw_piece_sprite_16x16
ENDIF

_board_piece_reflection_ovl_entry:
IFDEF NETCHESSZX_NEXT
    ld hl, 1
    ret
ELSE
    push ix
    ld a, (_spectrum_gui_active_coord_valid)
    or a
    ld a, 0xff
    jr z, boa_reflect_cursor_ready
    ld a, (_spectrum_gui_board_flipped)
    or a
    jr nz, boa_reflect_cursor_flipped
    ld a, 7
    ld hl, _spectrum_gui_active_coord_row
    sub (hl)
    ld d, a
    ld a, (_spectrum_gui_active_coord_col)
    jr boa_reflect_cursor_coords
boa_reflect_cursor_flipped:
    ld a, (_spectrum_gui_active_coord_row)
    ld d, a
    ld a, 7
    ld hl, _spectrum_gui_active_coord_col
    sub (hl)
boa_reflect_cursor_coords:
    ld e, a
    ld a, d
    add a, a
    add a, a
    add a, a
    add a, e
boa_reflect_cursor_ready:
    ld (MIRRORSHIFT_REFLECT_CURSOR), a
    call boa_reflect_wanted
    jp c, boa_reflect_fail
    ld (piece_char), a
    ld a, (0x5c78)
    ld hl, 0x5c79
    xor (hl)
    and 63
    ld e, a
    xor a
    ld (MIRRORSHIFT_REFLECT_COUNT), a
    ld b, 64
boa_reflect_pick_loop:
    ld d, 0
    ld hl, MIRRORSHIFT_BOARD_STATE
    add hl, de
    ld a, (piece_char)
    cp (hl)
    jr nz, boa_reflect_pick_next
    ld a, (MIRRORSHIFT_REFLECT_CURSOR)
    cp e
    jr z, boa_reflect_pick_next
    ld a, (MIRRORSHIFT_REFLECT_COUNT)
    ld c, a
    ld hl, MIRRORSHIFT_REFLECT_TARGETS
    add a, l
    ld l, a
    ld (hl), e
    ld a, c
    inc a
    ld (MIRRORSHIFT_REFLECT_COUNT), a
    cp 3
    jr z, boa_reflect_picked
boa_reflect_pick_next:
    ld a, e
    add a, 13
    and 63
    ld e, a
    djnz boa_reflect_pick_loop
    ld a, (MIRRORSHIFT_REFLECT_COUNT)
    or a
    jp z, boa_reflect_fail
boa_reflect_picked:

    ; Replay each packed frame for up to three targets, using each physical
    ; mask independently. Wait once per batch to preserve the animation cadence.
    ld hl, MIRRORSHIFT_PIECE_REFLECTION
    ld (MIRRORSHIFT_REFLECT_BIT_PTR), hl
    xor a
    ld (MIRRORSHIFT_REFLECT_BIT_LEFT), a
    ld (MIRRORSHIFT_REFLECT_FRAME), a

    ld a, (_netchesszx_piece_set_index)
    cp 3
    jr c, boa_reflect_set_valid
    xor a
boa_reflect_set_valid:
    ld e, a
    ld d, 0
    ld hl, boa_reflect_row_bits
    add hl, de
    ld a, (hl)
    ld (MIRRORSHIFT_REFLECT_ROW_BITS), a
    ld hl, boa_reflect_x_bases
    add hl, de
    ld a, (hl)
    ld (MIRRORSHIFT_REFLECT_X_BASE), a

boa_reflect_frame:
    ld hl, MIRRORSHIFT_REFLECT_BIT_PTR
    ld de, MIRRORSHIFT_REFLECT_STREAM
    ld bc, 4
    ldir
    xor a
    ld (MIRRORSHIFT_REFLECT_INDEX), a
boa_reflect_target_frame:
    ld hl, MIRRORSHIFT_REFLECT_STREAM
    ld de, MIRRORSHIFT_REFLECT_BIT_PTR
    ld bc, 4
    ldir
    call boa_reflect_prepare_target
    call boa_reflect_copy_base
    jr boa_reflect_header

; GUI timer/marker painting may reuse resident board temporaries while the
; overlay waits. Rebuild every target-dependent value before each frame.
boa_reflect_prepare_target:
    ld a, (MIRRORSHIFT_REFLECT_INDEX)
    ld hl, MIRRORSHIFT_REFLECT_TARGETS
    add a, l
    ld l, a
    ld a, (hl)
    ld (MIRRORSHIFT_REFLECT_TARGET), a
    ld e, a
    ld d, 0
    ld hl, MIRRORSHIFT_BOARD_STATE
    add hl, de
    ld a, (hl)
    ld (piece_char), a

    ld a, (MIRRORSHIFT_REFLECT_TARGET)
    and 7
    ld e, a
    ld a, (MIRRORSHIFT_REFLECT_TARGET)
    rrca
    rrca
    rrca
    and 7
    ld d, a
    ld a, (_spectrum_gui_board_flipped)
    or a
    jr nz, boa_reflect_coords_flipped
    ld a, 7
    sub d
    ld d, a
    jr boa_reflect_coords_ready
boa_reflect_coords_flipped:
    ld a, 7
    sub e
    ld e, a
boa_reflect_coords_ready:
    ld a, d
    ld (board_row), a
    ld a, e
    ld (board_col), a

    ld a, (piece_char)
    sub 'A'
    ld c, a
    call square_parity
    xor c
    ld (MIRRORSHIFT_REFLECT_PHYSICAL), a
    ret

boa_reflect_copy_base:
    ld hl, MIRRORSHIFT_PIECE_MASKS
    ld a, (MIRRORSHIFT_REFLECT_PHYSICAL)
    or a
    jr z, boa_reflect_source_ready
    ld de, 32
    add hl, de
boa_reflect_source_ready:
    ld de, MIRRORSHIFT_FLIP_SCRATCH
    ld bc, 32
    ldir
    ret

boa_reflect_header:
    ld b, 3
    call boa_reflect_read_bits
    add a, 3
    ld (MIRRORSHIFT_REFLECT_Y), a
    ld b, 4
    call boa_reflect_read_bits
    inc a
    ld (MIRRORSHIFT_REFLECT_ROWS), a

boa_reflect_row:
    ld a, (MIRRORSHIFT_REFLECT_ROW_BITS)
    ld b, a
    call boa_reflect_read_bits
    ld c, a
    ld a, (MIRRORSHIFT_REFLECT_ROW_BITS)
    cp 5
    ld a, c
    jr nz, boa_reflect_code_ready
    or a
    jr z, boa_reflect_next_row
    dec a
boa_reflect_code_ready:
    ld c, a
    and 1
    inc a
    ld (MIRRORSHIFT_REFLECT_WIDTH), a
    ld a, c
    srl a
    ld hl, MIRRORSHIFT_REFLECT_X_BASE
    add a, (hl)
    ld (MIRRORSHIFT_REFLECT_X), a
boa_reflect_pixel:
    call boa_reflect_apply_pixel
    ld a, (MIRRORSHIFT_REFLECT_X)
    inc a
    ld (MIRRORSHIFT_REFLECT_X), a
    ld a, (MIRRORSHIFT_REFLECT_WIDTH)
    dec a
    ld (MIRRORSHIFT_REFLECT_WIDTH), a
    jr nz, boa_reflect_pixel

boa_reflect_next_row:
    ld a, (MIRRORSHIFT_REFLECT_Y)
    inc a
    ld (MIRRORSHIFT_REFLECT_Y), a
    ld a, (MIRRORSHIFT_REFLECT_ROWS)
    dec a
    ld (MIRRORSHIFT_REFLECT_ROWS), a
    jr nz, boa_reflect_row

boa_reflect_blit:
    call compute_square_bc
    ld a, b
    ld (piece_row), a
    ld a, c
    ld (piece_col), a
    ld de, MIRRORSHIFT_FLIP_SCRATCH
    call _spectrum_flip_blit_frame
    call boa_reflect_next_target
    jp c, boa_reflect_target_frame
    ld a, (MIRRORSHIFT_REFLECT_FRAME)
    inc a
    ld (MIRRORSHIFT_REFLECT_FRAME), a
    cp 7
    jr nc, boa_reflect_restore
    call boa_wait_frame
    jp boa_reflect_frame

boa_reflect_restore:
    call boa_wait_frame
    xor a
    ld (MIRRORSHIFT_REFLECT_INDEX), a
boa_reflect_restore_target:
    call boa_reflect_prepare_target
    call boa_reflect_copy_base
    call compute_square_bc
    ld a, b
    ld (piece_row), a
    ld a, c
    ld (piece_col), a
    ld de, MIRRORSHIFT_FLIP_SCRATCH
    call _spectrum_flip_blit_frame
    call boa_reflect_next_target
    jr c, boa_reflect_restore_target
    pop ix
    ld hl, 1
    ret

boa_reflect_next_target:
    ld a, (MIRRORSHIFT_REFLECT_INDEX)
    inc a
    ld (MIRRORSHIFT_REFLECT_INDEX), a
    ld hl, MIRRORSHIFT_REFLECT_COUNT
    cp (hl)
    ret

boa_reflect_wanted:
    call _ms_rules_is_over
    ld a, l
    or a
    scf
    ret nz
    call _ms_rules_side
    ld a, l
    add a, 'A'
    or a
    ret

boa_reflect_fail:
    pop ix
    ld hl, 1
    ret

; LSB-first packed stream. B=width (3..5), A=value.
boa_reflect_read_bits:
    xor a
    ld (MIRRORSHIFT_REFLECT_RESULT), a
    ld c, 1
boa_reflect_bit:
    ld a, (MIRRORSHIFT_REFLECT_BIT_LEFT)
    or a
    jr nz, boa_reflect_have_byte
    ld hl, (MIRRORSHIFT_REFLECT_BIT_PTR)
    ld a, (hl)
    inc hl
    ld (MIRRORSHIFT_REFLECT_BIT_PTR), hl
    ld (MIRRORSHIFT_REFLECT_BIT_BUF), a
    ld a, 8
    ld (MIRRORSHIFT_REFLECT_BIT_LEFT), a
boa_reflect_have_byte:
    ld a, (MIRRORSHIFT_REFLECT_BIT_BUF)
    rrca
    ld (MIRRORSHIFT_REFLECT_BIT_BUF), a
    jr nc, boa_reflect_bit_clear
    ld a, (MIRRORSHIFT_REFLECT_RESULT)
    or c
    ld (MIRRORSHIFT_REFLECT_RESULT), a
boa_reflect_bit_clear:
    sla c
    ld a, (MIRRORSHIFT_REFLECT_BIT_LEFT)
    dec a
    ld (MIRRORSHIFT_REFLECT_BIT_LEFT), a
    djnz boa_reflect_bit
    ld a, (MIRRORSHIFT_REFLECT_RESULT)
    ret

boa_reflect_apply_pixel:
    ld a, (MIRRORSHIFT_REFLECT_Y)
    add a, a
    ld e, a
    ld d, 0
    ld hl, MIRRORSHIFT_FLIP_SCRATCH
    add hl, de
    ld a, (MIRRORSHIFT_REFLECT_X)
    cp 8
    jr c, boa_reflect_pixel_byte_ready
    inc hl
    sub 8
boa_reflect_pixel_byte_ready:
    ld b, a
    or a
    ld a, 0x80
    jr z, boa_reflect_mask_ready
boa_reflect_mask_shift:
    rrca
    djnz boa_reflect_mask_shift
boa_reflect_mask_ready:
    ld d, a
    ld a, (MIRRORSHIFT_REFLECT_PHYSICAL)
    or a
    ld a, d
    jr nz, boa_reflect_clear_pixel
    or (hl)
    ld (hl), a
    ret
boa_reflect_clear_pixel:
    cpl
    and (hl)
    ld (hl), a
    ret

boa_reflect_row_bits:
    DEFB 5, 4, 4
boa_reflect_x_bases:
    DEFB 3, 6, 4
ENDIF

; Historical snapshot/undo IDs remain fail-closed ABI tombstones.
_board_snapshot_save_ovl_entry:
IFNDEF MIRRORSHIFT_CLASSIC_BOARD_ANIMATION
_board_seed_reveal_ovl_entry:
ENDIF
_board_snapshot_restore_ovl_entry:
_board_undo_restore_ovl_entry:
    ld l, 0
    ret

; Convergence / PARADOX / resign / draw: occupied fragments vanish from the
; rim toward the centre, leaving the empty Lattice. Display only.
_board_cell_transition_ovl_entry:
    push ix
    ld a, 3
boa_cr_ring:
    push af
    xor a
boa_cr_row:
    ld (scaled_row), a
    xor a
boa_cr_col:
    ld (scaled_col), a
    call boa_ring_dist
    pop bc
    push bc
    cp b
    call z, boa_collapse_one
    ld a, (scaled_col)
    inc a
    cp 8
    jr nz, boa_cr_col
    ld a, (scaled_row)
    inc a
    cp 8
    jr nz, boa_cr_row
    call boa_wait_frame
    call boa_wait_frame
    pop af
    dec a
    jp p, boa_cr_ring
    pop ix
    ld hl, 1
    ret

; A = max(3-min(row,7-row), 3-min(col,7-col))
boa_ring_dist:
    ld a, (scaled_row)
    call boa_edge_contrib
    ld b, a
    ld a, (scaled_col)
    call boa_edge_contrib
    cp b
    ret nc
    ld a, b
    ret

boa_edge_contrib:
    ld c, a
    ld a, 7
    sub c
    cp c
    jr c, boa_ec_ready
    ld a, c
boa_ec_ready:
    ld c, a
    ld a, 3
    sub c
    ret

boa_collapse_one:
    ld a, (scaled_row)
    add a, a
    add a, a
    add a, a
    ld e, a
    ld a, (scaled_col)
    add a, e
    ld e, a
    ld d, 0
    ld hl, MIRRORSHIFT_BOARD_STATE
    add hl, de
    ld a, (hl)
    cp '.'
    ret z
    ld c, a
    ld (hl), '.'
    push hl
    push bc
    ld hl, (scaled_row)
    push hl
    ld a, (scaled_row)
    ld l, a
    ld a, (scaled_col)
    ld h, a
    call _spectrum_board_view_redraw_square
    pop hl
    ld (scaled_row), hl
    pop bc
    pop hl
    ld (hl), c
    ret
