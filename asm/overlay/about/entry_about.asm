SECTION code_user

PUBLIC _about_render_ovl_entry

IFDEF NETCHESSZX_NEXT

    DEFB 1
    DW _about_render_ovl_entry

; The banking Next build owns a separate Layer 2 About image. Keep only a
; valid atlas entry; the loader never calls this ZX renderer.
_about_render_ovl_entry:
    ld hl, 0
    ret

ELSE

EXTERN ovl_close_overlay_file
EXTERN ovl_read_chunked
IFDEF NETCHESSZX_SPECTRANEXT
EXTERN ovl_open_readonly_at
ENDIF

about_payload_offset_classic EQU 908
about_payload_offset_next EQU 876
IFDEF NETCHESSZX_NEXT
about_payload_offset EQU about_payload_offset_next
ELSE
about_payload_offset EQU about_payload_offset_classic
ENDIF
about_payload_size EQU 5184
about_row_bytes EQU 32
about_chunk_rows EQU 14
about_chunk_bytes EQU 448
about_tail_rows EQU 4
about_tail_bytes EQU 128
about_pixel_chunks EQU 10
about_board_top_y EQU 32
about_board_attr_top EQU 4
about_board_attr_rows EQU 18
about_attr_bytes EQU about_board_attr_rows * about_row_bytes
about_attr_base EQU 0x5800 + (about_board_attr_top * about_row_bytes)
; Transient DAT buffer: immediately after MQTT scratch, ending 21 bytes below
; the 0x6800 overlay slot. The low-memory guard checks both boundaries.
IFDEF NETCHESSZX_NEXT
about_input EQU 0x3B2B
ELSE
about_input EQU 0x662B
ENDIF
about_input_size EQU 448
about_path_stage EQU 0x672B

    DEFB 1
    DW _about_render_ovl_entry

_about_render_ovl_entry:
    push ix
    push iy
IFDEF NETCHESSZX_SPECTRANEXT
    ld hl, about_asset_filename
    ld de, about_path_stage
    ld bc, about_asset_filename_end - about_asset_filename
    ldir
    ld hl, about_path_stage
    ld de, about_payload_offset
    call ovl_open_readonly_at
    jp c, about_fail
ELSE
    call ovl_close_overlay_file
    ld ix, about_asset_filename
    ld b, 0x01
    ld a, '*'
    push iy
    rst 8
    defb 0x9a
    pop iy
    jp c, about_fail
ENDIF
    ld (about_handle), a

    ld a, (about_handle)
    ld de, about_payload_offset
    ld bc, 0
    ld ix, 0
    ld l, 0
IFDEF NETCHESSZX_SPECTRANEXT
    ; ovl_open_readonly_at already performed the absolute seek.
ELSE
    push iy
    rst 8
    defb 0x9f
    pop iy
    jp c, about_fail_close
ENDIF

    ; Hide stale board colours while pixels stream in. Attributes arrive last.
    ld hl, about_attr_base
    ld (hl), 0
    ld d, h
    ld e, l
    inc de
    ld bc, about_attr_bytes - 1
    ldir

    xor a
    ld (about_row), a
    ld a, about_pixel_chunks
    ld (about_chunks_left), a
about_pixel_chunk_loop:
    ld bc, about_chunk_bytes
    call about_read_input
    jp c, about_fail_close
    ld hl, about_input
    ld a, (about_row)
    ld b, about_chunk_rows
    call about_copy_pixels
    ld a, (about_chunks_left)
    dec a
    ld (about_chunks_left), a
    jr nz, about_pixel_chunk_loop

    ld bc, about_tail_bytes
    call about_read_input
    jp c, about_fail_close
    ld hl, about_input
    ld a, (about_row)
    ld b, about_tail_rows
    call about_copy_pixels

    ld bc, about_chunk_bytes
    call about_read_input
    jp c, about_fail_close
    ld hl, about_input
    xor a
    ld b, about_chunk_rows
    call about_copy_attrs
    ld bc, about_tail_bytes
    call about_read_input
    jp c, about_fail_close
    ld hl, about_input
    ld a, about_chunk_rows
    ld b, about_tail_rows
    call about_copy_attrs

about_ok:
    call about_close
    pop iy
    pop ix
    ld hl, 1
    ret

about_fail_close:
    call about_close
about_fail:
    pop iy
    pop ix
    ld hl, 0
    ret

; HL=source, A=top row inside the 144-line band, B=row count.
about_copy_pixels:
    ld (about_row), a
    ld a, b
    ld (about_rows_left), a
about_copy_pixels_loop:
    push hl
    ld a, (about_row)
    call about_screen_addr
    ex de, hl
    pop hl
    ld bc, about_row_bytes
    ldir
    ld a, (about_row)
    inc a
    ld (about_row), a
    ld a, (about_rows_left)
    dec a
    ld (about_rows_left), a
    jr nz, about_copy_pixels_loop
    ret

; HL=source, A=top row inside the 18-row attribute band, B=row count.
about_copy_attrs:
    ld (about_row), a
    ld a, b
    ld (about_rows_left), a
about_copy_attrs_loop:
    push hl
    ld a, (about_row)
    call about_attr_addr
    ex de, hl
    pop hl
    ld bc, about_row_bytes
    ldir
    ld a, (about_row)
    inc a
    ld (about_row), a
    ld a, (about_rows_left)
    dec a
    ld (about_rows_left), a
    jr nz, about_copy_attrs_loop
    ret

about_read_input:
    ld ix, about_input
    ld a, (about_handle)
    jp ovl_read_chunked

about_close:
IFDEF NETCHESSZX_SPECTRANEXT
    jp ovl_close_overlay_file
ELSE
    ld a, (about_handle)
    push iy
    rst 8
    defb 0x9b
    pop iy
    ret
ENDIF

; A=row inside the About band. Returns the ULA bitmap address in HL.
about_screen_addr:
    add a, about_board_top_y
    ld b, a
    and 0x07
    add a, 0x40
    ld h, a
    ld a, b
    and 0xc0
    srl a
    srl a
    srl a
    add a, h
    ld h, a
    ld a, b
    and 0x38
    add a, a
    add a, a
    ld l, a
    ret

about_attr_addr:
    add a, about_board_attr_top
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, 0x5800
    add hl, de
    ret

about_handle: DEFB 0
about_row: DEFB 0
about_rows_left: DEFB 0
about_chunks_left: DEFB 0
about_asset_filename:
    DEFM "MIRSHIFT.DAT"
    DEFB 0
about_asset_filename_end:

ENDIF
