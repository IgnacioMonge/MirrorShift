SECTION code_user

; ROM IM1 requires IY=$5C3A while interrupts are live. esxDOS calls are
; allowed to clobber IY, so every RST 8 site restores it before returning to
; resident C or pumping the UART.

; SDCC/IY: two uint8 args are packed into one stack word:
;   SP+2 = ovl_id, SP+3 = entry_id.

PUBLIC _spectrum_overlay_exec
PUBLIC _spectrum_overlay_exec_cached
PUBLIC _spectrum_assets_load
PUBLIC _spectrum_assets_fatal
PUBLIC _spectrum_render_about
PUBLIC _netchesszx_piece_set_load
PUBLIC _spectrum_overlay_loaded_id
PUBLIC _overlay_code_slot
PUBLIC _overlay_scratch_base
PUBLIC _spectrum_overlay_context
PUBLIC ovl_close_overlay_file
PUBLIC ovl_read_chunked
IFDEF NETCHESSZX_SPECTRANEXT
PUBLIC ovl_open_readonly_at
PUBLIC _spxn_overlay_page_table
PUBLIC _spxn_overlay_loader_start
PUBLIC _spxn_overlay_loader_end
PUBLIC _spectrum_spxn_frame_wait
PUBLIC _spxn_rom_held
ENDIF

EXTERN _spectrum_uart_background_pump
EXTERN _netchesszx_piece_set_index

IFDEF NETCHESSZX_SPECTRANEXT
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result
EXTERN _esx_fopen
EXTERN _esx_fread
EXTERN _esx_fclose
EXTERN _spxn_xfs_fseek
EXTERN _spxn_xfs_init
EXTERN _spxn_detect
EXTERN _spectrum_input_frame_tick
EXTERN asm_z80_push_di
EXTERN asm_z80_pop_ei
EXTERN __bss_user_head
EXTERN __bss_user_size
ENDIF

_spectrum_overlay_context EQU 0x5FF8
IFDEF NETCHESSZX_SPECTRANEXT
_overlay_code_slot EQU 0x2000
_spxn_overlay_page_table EQU 0x6800
spxn_pagein EQU 0x3FF9
spxn_pageout EQU 0x007C
spxn_set_page_b EQU 0x3E36
spxn_push_page_b EQU 0x3E87
spxn_pop_page_b EQU 0x3E8A
spxn_reserve_page EQU 0x3E9F
spxn_free_page EQU 0x3EA2
spxn_overlay_owner EQU 0x4E
spxn_overlay_stage EQU 0x6820
spxn_overlay_chunk EQU 1502
spxn_overlay_chunk_size EQU spxn_overlay_stage + spxn_overlay_chunk
ELSE
_overlay_code_slot EQU 0x6800
ENDIF

_overlay_scratch_base EQU 0x672B
ovl_invalid_id EQU 0xff
INCLUDE "overlay_atlas_table.asm"
asset_load_addr EQU 0x6000
asset_ui_size EQU 812
asset_piece_offset EQU 812
piece_sprite_set_size EQU 64
piece_reflection_set_size EQU 32
asset_load_size_classic EQU 908
asset_load_size_next EQU 876
IFDEF NETCHESSZX_NEXT
asset_load_size EQU asset_load_size_next
piece_bundle_size EQU piece_sprite_set_size
ELSE
asset_load_size EQU asset_load_size_classic
piece_bundle_size EQU piece_sprite_set_size + piece_reflection_set_size
ENDIF
; Live A/B masks plus Classic reflection. XFS READDIR aliases this range and
; preserves it around directory scans; DAT fread uses esx_buf as destination
; and must land here. Staging the 96-byte bundle at overlay scratch $672B
; overwrites SET_PREV_MASKS at $676B and corrupts setup morph.
piece_sprite_addr EQU 0x662b
about_board_offset EQU asset_load_size
about_board_size EQU 5184
piece_set_extra_offset EQU about_board_offset + about_board_size
about_ovl_id EQU 12
about_render_entry EQU 0

IFDEF NETCHESSZX_SPECTRANEXT
ovl_atlas_fingerprint_offset EQU 96
ovl_atlas_fingerprint_size EQU 4
ENDIF

SECTION bss_user
IFDEF NETCHESSZX_SPECTRANEXT
ORG 0x5B00
ENDIF

ovl_handle:   DEFS 1
ovl_entry_id: DEFS 1
ovl_id:       DEFS 1
ovl_cache_ready: DEFS 1
ovl_file_open: DEFS 1
ovl_call_active: DEFS 1
asset_set_index: DEFS 1
_spectrum_overlay_loaded_id: DEFS 1
ovl_load_size: DEFS 2
IFDEF NETCHESSZX_SPECTRANEXT
ovl_xfs_ready: DEFS 1
ovl_atlas_ready: DEFS 1
ovl_spxn_pages_ready: DEFS 1
ovl_spxn_page_count: DEFS 1
ovl_spxn_current_page: DEFS 1
ovl_spxn_return_value: DEFS 2
ovl_spxn_copy_dst: DEFS 2
_spxn_rom_held: DEFS 1
ENDIF

SECTION code_user

_spectrum_overlay_exec:
    ld a, (ovl_call_active)
    or a
    jp nz, ovl_nested_fail
    xor a
    ld (ovl_cache_ready), a

_spectrum_overlay_exec_cached:
IFDEF NETCHESSZX_SPECTRANEXT
    ld a, (ovl_call_active)
    or a
    jp nz, ovl_nested_fail
ENDIF
    ld hl, 2
    add hl, sp
    ld a, (hl)
    inc hl
    ld b, (hl)
ovl_args_canonical:
    ld (ovl_id), a
    ld a, b
    ld (ovl_entry_id), a
    ld a, (ovl_call_active)
    or a
    jp nz, ovl_nested_fail

    push ix
    push iy

    call ovl_ensure_loaded
    jp c, ovl_fail
    jp ovl_call_loaded

_spectrum_assets_load:
IFDEF NETCHESSZX_SPECTRANEXT
    ; bss_user is anchored below the TAP CODE block to recover the inactive
    ; printer buffer.  This is the first product call from main, so clear its
    ; exact linked section before consulting any loader or driver state.
    ; BSS_UNINITIALIZED follows it and owns the CRT's saved BASIC stack.
    ld hl, __bss_user_head
    ld de, __bss_user_head + 1
    ld bc, __bss_user_size - 1
    xor a
    ld (hl), a
    ldir
    ld (_spxn_rom_held), a
    call _spxn_xfs_init
ENDIF
    push ix
    push iy
    call ovl_close_overlay_file
    ld hl, asset_filename
IFDEF NETCHESSZX_SPECTRANEXT
    call ovl_xfs_open_readonly
    jr c, assets_fail
ELSE
    push hl
    pop ix
    ld b, 0x01
    ld a, '*'
    push iy
    rst 8
    defb 0x9a
    pop iy
    jr c, assets_fail
ENDIF
    ld (ovl_handle), a

    ld a, (ovl_handle)
    ld ix, asset_load_addr
    ld bc, asset_ui_size
    call ovl_read_chunked
    jr c, assets_fail_close

    ld a, (ovl_handle)
    ld ix, piece_sprite_addr
    ld bc, piece_bundle_size
    call ovl_read_chunked
    jr c, assets_fail_close

    call ovl_close
IFDEF NETCHESSZX_SPECTRANEXT
    jr c, assets_fail
    call ovl_spxn_preload_all
    jr c, assets_fail
ENDIF
    pop iy
    pop ix
    ld hl, 1
    ret

assets_fail_close:
    call ovl_close
assets_fail:
    pop iy
    pop ix
    ld hl, 0
    ret

_netchesszx_piece_set_load:
    ld a, l
    cp 3
    jr c, npsl_index_ok
    xor a
npsl_index_ok:
    ld (asset_set_index), a
    push ix
    push iy
    call ovl_close_overlay_file

    ld hl, asset_filename
IFDEF NETCHESSZX_SPECTRANEXT
    call ovl_xfs_open_readonly
    jr c, npsl_fail
ELSE
    push hl
    pop ix
    ld b, 0x01
    ld a, '*'
    push iy
    rst 8
    defb 0x9a
    pop iy
    jr c, npsl_fail
ENDIF
    ld (ovl_handle), a

    ld a, (asset_set_index)
    add a, a
    ld e, a
    ld d, 0
    ld hl, piece_set_offsets
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld bc, 0
    ld a, (ovl_handle)
IFDEF NETCHESSZX_SPECTRANEXT
    ex de, hl
    call _spxn_xfs_fseek
    ld a, l
    or a
    jr z, npsl_fail_close
ELSE
    ld ix, 0
    ld l, 0
    push iy
    rst 8
    defb 0x9f
    pop iy
    jr c, npsl_fail_close
ENDIF

    ld a, (ovl_handle)
    ld ix, piece_sprite_addr
    ld bc, piece_bundle_size
    call ovl_read_chunked
    jr c, npsl_fail_close

    call ovl_close
IFDEF NETCHESSZX_SPECTRANEXT
    jr c, npsl_fail
ENDIF
    pop iy
    pop ix
    ld hl, 1
    ret
npsl_fail_close:
    call ovl_close
npsl_fail:
    pop iy
    pop ix
    ld hl, 0
    ret

piece_set_offsets:
    DW asset_piece_offset
    DW piece_set_extra_offset
    DW piece_set_extra_offset + piece_bundle_size

_spectrum_assets_fatal:
    di
    ld a, 2
    out (254), a
    ld hl, assets_fatal_bitmap
    ld de, 0x4000
    ld b, 8
assets_fatal_row:
    push bc
    ld b, 4
assets_fatal_col:
    ld a, (hl)
    ld (de), a
    inc hl
    inc e
    djnz assets_fatal_col
    ld a, e
    sub 4
    ld e, a
    inc d
    pop bc
    djnz assets_fatal_row
    ld hl, 0x5800
    ld a, 0x42
    ld b, 4
assets_fatal_attr:
    ld (hl), a
    inc hl
    djnz assets_fatal_attr
assets_fatal_halt:
    jr assets_fatal_halt

_spectrum_render_about:
    ld hl, about_ovl_id + (about_render_entry * 256)
    push hl
    call _spectrum_overlay_exec
    pop bc
    push hl
    ld a, (_netchesszx_piece_set_index)
    ld l, a
    call _netchesszx_piece_set_load
    ld a, h
    or l
    pop de
    jr z, about_restore_fail
    ex de, hl
    ret
about_restore_fail:
    ld hl, 0
    ret

assets_fatal_bitmap:
    DEFB 0xf8,0x38,0xfe,0x7c
    DEFB 0x84,0x44,0x10,0x82
    DEFB 0x82,0x82,0x10,0x02
    DEFB 0x82,0xfe,0x10,0x0c
    DEFB 0x82,0x82,0x10,0x10
    DEFB 0x84,0x82,0x10,0x00
    DEFB 0xf8,0x82,0x10,0x10
    DEFB 0x00,0x00,0x00,0x00

ovl_ensure_loaded:
IFDEF NETCHESSZX_SPECTRANEXT
    ld a, (ovl_spxn_pages_ready)
    or a
    jr z, ovl_spxn_not_ready
    call ovl_select_atlas_entry
    ret c
    ld a, (ovl_id)
    ld (_spectrum_overlay_loaded_id), a
    ld a, 1
    ld (ovl_cache_ready), a
    ret
ovl_spxn_not_ready:
    scf
    ret
ELSE
    ld a, (ovl_cache_ready)
    or a
    jr z, ovl_load
    ld a, (_spectrum_overlay_loaded_id)
    ld hl, ovl_id
    cp (hl)
    jr nz, ovl_load
    ret
ENDIF

IFNDEF NETCHESSZX_SPECTRANEXT
ovl_load:
    xor a
    ld (ovl_cache_ready), a

    ld a, (ovl_file_open)
    or a
    jr nz, ovl_file_ready
    ld hl, ovl_filename
IFDEF NETCHESSZX_SPECTRANEXT
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jr z, ovl_load_fail
ELSE
    push hl
    pop ix
    ld b, 0x01
    ld a, '*'
    push iy
    rst 8
    defb 0x9a
    pop iy
    jp c, ovl_load_fail
ENDIF
    ld (ovl_handle), a
    ld a, 1
    ld (ovl_file_open), a

ovl_file_ready:
    call ovl_select_atlas_entry
    jr c, ovl_load_fail_close

ovl_read:
    ld a, (ovl_handle)
    ld ix, _overlay_code_slot
    ld bc, (ovl_load_size)
    call ovl_read_chunked
    jr c, ovl_load_fail_close

    ld a, (ovl_id)
    ld (_spectrum_overlay_loaded_id), a
    ld a, 1
    ld (ovl_cache_ready), a
IFDEF NETCHESSZX_SPECTRANEXT
    ; The XFS adapter owns one live handle. Close the atlas before invoking
    ; the overlay; the next cache miss reopens and seeks its atlas entry.
    call ovl_close_overlay_file
ENDIF
    ret

ENDIF

; A=handle, IX=destination, BC=length. Read in bounded chunks and drain the
; polling UART between them. Returns the original length in BC, CF on error or
; short read. The handle is mirrored in ovl_handle because esxDOS may clobber A.
ovl_read_chunked:
IFDEF NETCHESSZX_SPECTRANEXT
    ld (ovl_handle), a
    push bc
    ld (_esx_handle), a
    push ix
    pop hl
    ld (_esx_buf), hl
    ld (_esx_count), bc
    call _esx_fread
    pop bc
    ld hl, (_esx_result)
    or a
    sbc hl, bc
    ret z
    scf
    ret
ELSE
    ld (ovl_handle), a
    push bc
    ld h, b
    ld l, c
ovl_read_chunk_loop:
    ld a, h
    or l
    jr z, ovl_read_chunk_done
    ld bc, 64
    ld a, h
    or a
    jr nz, ovl_read_chunk_size_ready
    ld a, l
    cp 64
    jr nc, ovl_read_chunk_size_ready
    ld c, l
    ld b, 0
ovl_read_chunk_size_ready:
    push hl
    push ix
    push bc
    ld a, (ovl_handle)
    push iy
    rst 8
    defb 0x9d
    pop iy
    pop de
    jr c, ovl_read_chunk_fail
    ld a, b
    cp d
    jr nz, ovl_read_chunk_fail
    ld a, c
    cp e
    jr nz, ovl_read_chunk_fail
    pop ix
    pop hl
    or a
    sbc hl, de
    push hl
    push ix
    pop hl
    add hl, de
    push hl
    pop ix
    pop hl
    push hl
    push ix
    call _spectrum_uart_background_pump
    pop ix
    pop hl
    jr ovl_read_chunk_loop
ovl_read_chunk_fail:
    pop ix
    pop hl
    pop bc
    scf
    ret
ovl_read_chunk_done:
    pop bc
    or a
    ret
ENDIF

IFNDEF NETCHESSZX_SPECTRANEXT
ovl_load_fail_close:
    call ovl_close_overlay_file

ovl_load_fail:
    xor a
    ld (ovl_cache_ready), a
    ld a, ovl_invalid_id
    ld (_spectrum_overlay_loaded_id), a
    scf
    ret
ENDIF

ovl_call_loaded:
IFDEF NETCHESSZX_SPECTRANEXT
    jp ovl_spxn_call_loaded
ELSE
    ld a, (ovl_entry_id)
    ld hl, _overlay_code_slot
    cp (hl)
    jr nc, ovl_fail
    add a, a
    jr c, ovl_fail
    ld e, a
    ld d, 0
    ld hl, _overlay_code_slot + 1
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)

    push de
    ex de, hl
    ld de, _overlay_code_slot
    or a
    sbc hl, de
    jr c, ovl_bad_entry
    ld de, (ovl_load_size)
    or a
    sbc hl, de
    jr nc, ovl_bad_entry
    ld a, 1
    ld (ovl_call_active), a
    pop de
    pop iy
    pop ix
    ; C fastcall entries need the context in HL; native ASM entries use DE.
    ; Stack the target so both registers can carry the same context pointer.
    ld bc, ovl_return
    push bc
    push de
    ld de, _spectrum_overlay_context
    ld h, d
    ld l, e
    ; Overlay entry starts under DI and ovl_return unconditionally enables IM1.
    ; Overlay callees may re-enable interrupts (for example via frame_wait), so
    ; this is not a whole-overlay DI guarantee.
    di
    ret

ovl_return:
    xor a
    ld (ovl_call_active), a
    ei
    ret

ENDIF

ovl_nested_fail:
    ld hl, 0
    ret

IFNDEF NETCHESSZX_SPECTRANEXT
ovl_bad_entry:
    pop de
    jr ovl_fail
ENDIF

ovl_fail:
    xor a
    ld (ovl_cache_ready), a
    pop iy
    pop ix
    ld hl, 0
    ret

ovl_select_atlas_entry:
    ld a, (ovl_id)
    cp ovl_atlas_count
    jr nc, ovl_select_bad
    add a, a
    ld e, a
    ld d, 0
    ld hl, ovl_atlas_table
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld c, (hl)
    inc hl
    ld b, (hl)
    ld h, b
    ld l, c
    or a
    sbc hl, de
    ld (ovl_load_size), hl
    ld a, h
    or l
    jr z, ovl_select_bad
    ld a, h
IFDEF NETCHESSZX_SPECTRANEXT
    cp 16
ELSE
    cp 8
ENDIF
    jr c, ovl_select_size_ok
    jr nz, ovl_select_bad
    ld a, l
    or a
    jr nz, ovl_select_bad
ovl_select_size_ok:
    ld bc, 0
    ld a, (ovl_handle)
IFDEF NETCHESSZX_SPECTRANEXT
    or a
    ret
ELSE
    ld ix, 0
    ld l, 0
    push iy
    rst 8
    defb 0x9f
    pop iy
    ret
ENDIF
ovl_select_bad:
    scf
    ret

ovl_close_overlay_file:
    ld a, (ovl_file_open)
    or a
    ret z
    call ovl_close
IFDEF NETCHESSZX_SPECTRANEXT
    ret c
    ld a, 0
ELSE
    xor a
ENDIF
    ld (ovl_file_open), a
    ret

IFDEF NETCHESSZX_SPECTRANEXT
; HL=ASCIIZ filename, DE=absolute offset. Opens through the XFS adapter,
; seeks to the payload, and leaves the one live handle owned by this loader.
ovl_open_readonly_at:
    push de
    call ovl_close_overlay_file
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jr z, ovl_open_readonly_fail_pop
    ld (ovl_handle), a
    ld a, 1
    ld (ovl_file_open), a
    pop de
    ld bc, 0
    ex de, hl
    call _spxn_xfs_fseek
    ld a, l
    or a
    jr z, ovl_open_readonly_fail_close
    ld a, (ovl_handle)
    or a
    ret
ovl_open_readonly_fail_pop:
    pop de
    scf
    ret
ovl_open_readonly_fail_close:
    call ovl_close_overlay_file
    scf
    ret
ENDIF

ovl_close:
    ld a, (ovl_handle)
IFDEF NETCHESSZX_SPECTRANEXT
    ld (_esx_handle), a
    call _esx_fclose
    ld a, l
    or a
    ret z
    scf
    ret
ELSE
    push iy
    rst 8
    defb 0x9b
    pop iy
    ret
ENDIF


IFDEF NETCHESSZX_SPECTRANEXT
ovl_xfs_open_readonly:
    ld a, (ovl_xfs_ready)
    or a
    jr nz, ovl_xfs_open
    push hl
    call _spxn_detect
    ld a, l
    pop hl
    cp 1
    jr nz, ovl_xfs_open_fail
    ld a, 1
    ld (ovl_xfs_ready), a
ovl_xfs_open:
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    ret nz
ovl_xfs_open_fail:
    xor a
    ld (_esx_handle), a
    scf
    ret

ovl_verify_atlas:
    ld a, (ovl_handle)
    ld ix, _overlay_scratch_base
    ld bc, ovl_atlas_fingerprint_offset + ovl_atlas_fingerprint_size
    call ovl_read_chunked
    jr c, ovl_verify_atlas_bad
    ld a, (_overlay_scratch_base + ovl_atlas_fingerprint_offset + 0)
    cp ovl_atlas_fingerprint_0
    jr nz, ovl_verify_atlas_bad
    ld a, (_overlay_scratch_base + ovl_atlas_fingerprint_offset + 1)
    cp ovl_atlas_fingerprint_1
    jr nz, ovl_verify_atlas_bad
    ld a, (_overlay_scratch_base + ovl_atlas_fingerprint_offset + 2)
    cp ovl_atlas_fingerprint_2
    jr nz, ovl_verify_atlas_bad
    ld a, (_overlay_scratch_base + ovl_atlas_fingerprint_offset + 3)
    cp ovl_atlas_fingerprint_3
    jr nz, ovl_verify_atlas_bad
    ld a, 1
    ld (ovl_atlas_ready), a
    or a
    ret
ovl_verify_atlas_bad:
    scf
    ret

SECTION spxn_overlay_backend
ORG 0x6E00
_spxn_overlay_loader_start:

; Verify the packed atlas through the ordinary VFS seam, reserve one cartridge
; SRAM page per payload, then stage XFS reads in Spectrum RAM and copy them
; into Page B. ROM READ rejects destinations in the cart window.
ovl_spxn_preload_all:
    xor a
    ld (ovl_spxn_pages_ready), a
    ld (ovl_spxn_page_count), a
    call ovl_close_overlay_file
    ld hl, ovl_filename
    call ovl_xfs_open_readonly
    jp c, ovl_spxn_preload_fail
    ld a, (_esx_handle)
    ld (ovl_handle), a
    ld a, 1
    ld (ovl_file_open), a
    call ovl_verify_atlas
    jp c, ovl_spxn_preload_fail_close
ovl_spxn_preload_loop:
    ld a, (ovl_spxn_page_count)
    cp ovl_atlas_count
    jp z, ovl_spxn_preload_done
    ld (ovl_id), a
    call ovl_select_atlas_entry
    jp c, ovl_spxn_preload_fail_allocated

    call asm_z80_push_di
    push iy
    call spxn_pagein
    ld a, spxn_overlay_owner
    call spxn_reserve_page
    jp c, ovl_spxn_reserve_fail_paged
    ld (ovl_spxn_current_page), a
    call spxn_pageout
    pop iy
    call asm_z80_pop_ei
    ld a, (ovl_spxn_page_count)
    or a
    jr z, ovl_spxn_page_unique
    ld b, a
    ld hl, _spxn_overlay_page_table
ovl_spxn_page_unique_loop:
    ld a, (ovl_spxn_current_page)
    cp (hl)
    jp z, ovl_spxn_preload_fail_allocated
    inc hl
    djnz ovl_spxn_page_unique_loop
ovl_spxn_page_unique:
    ld a, (ovl_spxn_page_count)
    ld e, a
    ld d, 0
    ld hl, _spxn_overlay_page_table
    add hl, de
    ld a, (ovl_spxn_current_page)
    ld (hl), a
    ld a, e
    inc a
    ld (ovl_spxn_page_count), a

    ld hl, _overlay_code_slot
    ld (ovl_spxn_copy_dst), hl
ovl_spxn_copy_loop:
    ld hl, (ovl_load_size)
    ld a, h
    or l
    jr z, ovl_spxn_preload_loop
    ld bc, spxn_overlay_chunk
    or a
    sbc hl, bc
    jr nc, ovl_spxn_chunk_ready
    add hl, bc
    ld b, h
    ld c, l
ovl_spxn_chunk_ready:
    ld (spxn_overlay_chunk_size), bc
    ld a, (ovl_handle)
    ld ix, spxn_overlay_stage
    call ovl_read_chunked
    jr c, ovl_spxn_preload_fail_allocated

    call asm_z80_push_di
    push iy
    call spxn_pagein
    ld a, (ovl_spxn_current_page)
    call spxn_push_page_b
    ld bc, (spxn_overlay_chunk_size)
    ld hl, spxn_overlay_stage
    ld de, (ovl_spxn_copy_dst)
    ldir
    ld (ovl_spxn_copy_dst), de
    call spxn_pop_page_b
    call spxn_pageout
    pop iy
    call asm_z80_pop_ei

    ld bc, (spxn_overlay_chunk_size)
    ld hl, (ovl_load_size)
    or a
    sbc hl, bc
    ld (ovl_load_size), hl
    jr ovl_spxn_copy_loop

ovl_spxn_reserve_fail_paged:
    call spxn_pageout
    pop iy
    call asm_z80_pop_ei
    jr ovl_spxn_preload_fail_allocated

ovl_spxn_preload_done:
    call ovl_close_overlay_file
    jr c, ovl_spxn_preload_fail_allocated
    ld a, 1
    ld (ovl_spxn_pages_ready), a
    or a
    ret

ovl_spxn_preload_fail_close:
    call ovl_close_overlay_file
ovl_spxn_preload_fail:
    xor a
    ld (ovl_atlas_ready), a
    scf
    ret

ovl_spxn_preload_fail_allocated:
    call ovl_close_overlay_file
    ld a, (ovl_spxn_page_count)
    or a
    jr z, ovl_spxn_preload_freed
    call asm_z80_push_di
    push iy
    call spxn_pagein
    ld a, (ovl_spxn_page_count)
    ld b, a
    ld hl, _spxn_overlay_page_table
ovl_spxn_preload_free_loop:
    ld a, (hl)
    push hl
    push bc
    call spxn_free_page
    pop bc
    pop hl
    inc hl
    djnz ovl_spxn_preload_free_loop
    call spxn_pageout
    pop iy
    call asm_z80_pop_ei
ovl_spxn_preload_freed:
    xor a
    ld (ovl_spxn_page_count), a
    ld (ovl_atlas_ready), a
    scf
    ret

ovl_spxn_call_loaded:
    pop iy
    pop ix
    call asm_z80_push_di
    push ix
    push iy
    call spxn_pagein
    ld a, (ovl_id)
    ld e, a
    ld d, 0
    ld hl, _spxn_overlay_page_table
    add hl, de
    ld a, (hl)
    ld (ovl_spxn_current_page), a
    call spxn_push_page_b
    ld a, 1
    ld (_spxn_rom_held), a
    ld (ovl_call_active), a

    ld a, (ovl_entry_id)
    ld hl, _overlay_code_slot
    cp (hl)
    jr nc, ovl_spxn_bad_entry
    add a, a
    jr c, ovl_spxn_bad_entry
    ld e, a
    ld d, 0
    ld hl, _overlay_code_slot + 1
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)
    push de
    ex de, hl
    ld de, _overlay_code_slot
    or a
    sbc hl, de
    jr c, ovl_spxn_bad_entry_pop
    ld de, (ovl_load_size)
    or a
    sbc hl, de
    jr nc, ovl_spxn_bad_entry_pop
    pop de
    ld bc, ovl_spxn_return
    push bc
    push de
    ld de, _spectrum_overlay_context
    ld h, d
    ld l, e
    di
    ret

ovl_spxn_bad_entry_pop:
    pop de
ovl_spxn_bad_entry:
    call spxn_pop_page_b
    xor a
    ld (_spxn_rom_held), a
    call spxn_pageout
    pop iy
    xor a
    ld (ovl_call_active), a
    pop ix
    call asm_z80_pop_ei
    ld hl, 0
    ret

ovl_spxn_return:
    ld (ovl_spxn_return_value), hl
    di
    call spxn_pop_page_b
    xor a
    ld (_spxn_rom_held), a
    call spxn_pageout
    pop iy
    xor a
    ld (ovl_call_active), a
    pop ix
    call asm_z80_pop_ei
    ld hl, (ovl_spxn_return_value)
    ret

; Called by spectrum_frame_wait.  While an overlay owns Page B, expose the
; Spectrum ROM only for HALT/IM1 and the resident input tick, then restore the
; exact overlay page before returning.
_spectrum_spxn_frame_wait:
    push ix
    push iy
    push af
    push bc
    push de
    push hl
    ld a, (_spxn_rom_held)
    or a
    jr z, ovl_spxn_frame_plain
    di
    xor a
    ld (_spxn_rom_held), a
    call spxn_pageout
    ld a, 1
    jr ovl_spxn_frame_marker
ovl_spxn_frame_plain:
    xor a
ovl_spxn_frame_marker:
    push af
    ld iy, 0x5C3A
    ei
    halt
    call _spectrum_input_frame_tick
    pop af
    or a
    jr z, ovl_spxn_frame_done
    di
    call spxn_pagein
    ld a, (ovl_spxn_current_page)
    call spxn_set_page_b
    ld a, 1
    ld (_spxn_rom_held), a
ovl_spxn_frame_done:
    pop hl
    pop de
    pop bc
    pop af
    pop iy
    pop ix
    ret

_spxn_overlay_loader_end:
SECTION code_user
ENDIF


ovl_filename:
    DEFM "MIRSHIFT.OVL"
    DEFB 0

asset_filename:
    DEFM "MIRSHIFT.DAT"
    DEFB 0
