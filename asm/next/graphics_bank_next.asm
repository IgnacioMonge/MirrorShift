SECTION code_user

; Next-only graphics services remain resident under DI. Bundle reads page
; slot 0 temporarily and restore its exact mapping before returning. Palette
; and projection staging use slot-1 scratch, preserving mapped overlay code.

PUBLIC next_graphics_bank_init
PUBLIC next_graphics_bank_set
PUBLIC next_graphics_bank_about
PUBLIC next_graphics_bank_final

EXTERN _netchesszx_piece_set_index
EXTERN _spectrum_next_sprites_hide_all
EXTERN _spectrum_next_setup_pieces_use_final
EXTERN asset_load_size
EXTERN asset_set_index
EXTERN next_copy_bundle
EXTERN nextreg_read
EXTERN nextreg_write

INCLUDE "asm/next/extension_bank_layout.asm"

; Resident entries preserve the Mirror Shift graphics service ABI.
next_graphics_bank_init:
    jp ngb_init
next_graphics_bank_set:
    jp ngb_set
next_graphics_bank_about:
    jp ngb_about
next_graphics_bank_final:
    jp ngb_final

ngb_init:
    call ngb_sprite_system_init
    call ngb_sprite_upload_set_current
    call ngb_sprite_upload_capture_current
    call ngb_sprite_upload_final_current
    call ngb_sprite_upload_flash_current
    call ngb_sprite_upload_common
    call ngb_piece_masks_load_current
    ld hl, 1
    ret

; asset_set_index was normalized and stored by the resident ABI wrapper.
ngb_set:
    call ngb_sprite_upload_set_current
    call ngb_sprite_upload_capture_current
    call ngb_piece_masks_load_current
    ld hl, 1
    ret

; Commit the selected full-size pair only after the setup morph has finished.
; Until this entry runs, the visible setup pieces keep using the old stable
; pair, so preparing patterns cannot cause a pre-animation flash.
ngb_final:
    call ngb_sprite_upload_final_current
    call ngb_sprite_upload_flash_current
    call _spectrum_next_setup_pieces_use_final
    ld hl, 1
    ret

ngb_piece_masks_load_current:
    ld a, (asset_set_index)
    add a, a
    ld e, a
    ld d, 0
    ld hl, ngb_piece_set_offsets
    add hl, de
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a
    ld bc, next_bundle_dat_offset
    add hl, bc
    ld de, next_sprite_stage
    ld bc, piece_sprite_set_size
    jp next_copy_bundle

ngb_piece_set_offsets:
    DW asset_piece_offset
    DW asset_load_size
    DW asset_load_size + piece_sprite_set_size

ngb_about:
    push ix
    push iy
    call _spectrum_next_sprites_hide_all
    ld hl, next_about_pal_offset
    ld de, next_palette_stage
    ld bc, 512
    call next_copy_bundle
    ld a, nextreg_palette_control
    ld e, 0x10
    call nextreg_write
    ld a, nextreg_palette_index
    ld e, 0
    call nextreg_write
    ld hl, next_palette_stage
    ld d, 0
    ld bc, nextreg_select
    ld a, nextreg_palette_value_9
    out (c), a
    ld bc, nextreg_data
ngb_about_palette_loop:
    ld a, (hl)
    out (c), a
    inc hl
    ld a, (hl)
    out (c), a
    inc hl
    dec d
    jr nz, ngb_about_palette_loop
    ld a, nextreg_layer2_bank
    ld e, next_about_bank
    call nextreg_write
    ld bc, layer2_port
    ld a, 2
    out (c), a
    pop iy
    pop ix
    ld hl, 1
    ret

ngb_sprite_system_init:
    ld a, nextreg_sprite_transparency_index
    ld e, next_sprite_transparency
    call nextreg_write
    ld a, nextreg_palette_control
    ld e, 0x20
    call nextreg_write
    ld a, nextreg_palette_index
    ld e, 0
    call nextreg_write
    ld hl, next_sprite_pal_offset
    ld de, next_palette_stage
    ld bc, next_sprite_palette_size * 2 + next_ula_standard_palette_size
    call next_copy_bundle
    ld hl, next_palette_stage
    ld d, next_sprite_palette_size & 0xff
    call ngb_palette_upload_pairs
    ld a, nextreg_palette_control
    ld e, 0
    call nextreg_write
    ld a, nextreg_palette_index
    ld e, 192
    call nextreg_write
    ld d, next_ula_standard_palette_size / 2
    call ngb_palette_upload_pairs
    ; Groups 0/1 mirror classic ULA colours; groups 2/3 are private board/menu
    ; colours. Keep ULA+ enabled for the whole application so theme changes do
    ; not leave the attribute file interpreted under a stale palette mode.
    ld a, nextreg_ula_control
    call nextreg_read
    or 0x08
    ld e, a
    ld a, nextreg_ula_control
    call nextreg_write
    ld a, nextreg_sprite_layer_system
    call nextreg_read
    or 0x01
    ld e, a
    ld a, nextreg_sprite_layer_system
    call nextreg_write
    jp _spectrum_next_sprites_hide_all

ngb_palette_upload_pairs:
    ld bc, nextreg_select
    ld a, nextreg_palette_value_9
    out (c), a
    ld bc, nextreg_data
ngb_palette_upload_loop:
    ld a, (hl)
    out (c), a
    inc hl
    ld a, (hl)
    out (c), a
    inc hl
    dec d
    jr nz, ngb_palette_upload_loop
    ret

ngb_sprite_upload_set_current:
    xor a
    ld (ngb_pattern_index), a
    ld a, (_netchesszx_piece_set_index)
    ld (ngb_source_set), a
    ld hl, ngb_morph_phase_params
    ld b, 7
    call ngb_sprite_upload_phase_group
    ld a, (asset_set_index)
    ld (ngb_source_set), a
    ld b, 7
    jp ngb_sprite_upload_phase_group

; Build the gameplay A->B capture turn from the newly selected set. B->A is
; the same fourteen patterns played backwards by the BOARD overlay.
ngb_sprite_upload_capture_current:
    ld a, next_capture_sprite_pattern_base
    ld (ngb_pattern_index), a
    ld a, (asset_set_index)
    ld (ngb_source_set), a
    xor a
    ld (ngb_source_side), a
    ld hl, ngb_morph_phase_params
    ld b, 7
    call ngb_sprite_upload_capture_group
    ld a, 1
    ld (ngb_source_side), a
    ld b, 7

ngb_sprite_upload_capture_group:
ngb_sprite_upload_capture_loop:
    ld a, (hl)
    ld (ngb_phase_start), a
    inc hl
    ld a, (hl)
    ld (ngb_phase_step), a
    inc hl
    push bc
    push hl
    ld a, (ngb_source_side)
    call ngb_sprite_upload_morph_side
    pop hl
    pop bc
    djnz ngb_sprite_upload_capture_loop
    ret

; Upload the full-colour A/B sources into the stable gameplay pair.
ngb_sprite_upload_final_current:
    ld a, (asset_set_index)
    ld (ngb_source_set), a
    ld a, next_piece_final_pattern_base
    ld (ngb_pattern_index), a
    xor a
    call ngb_sprite_upload_source_side
    ld a, 1

ngb_sprite_upload_source_side:
    ld (ngb_source_side), a
    call ngb_piece_source_offset
ngb_sprite_upload_source:
    ld de, next_sprite_stage
    ld bc, 256
    call next_copy_bundle
    ld a, (ngb_pattern_index)
    ld bc, sprite_slot_port
    out (c), a
    ld hl, next_sprite_stage
    ld bc, sprite_pattern_port
    otir
    ld hl, ngb_pattern_index
    inc (hl)
    ret

; Upload the build-derived A/B sources with their one-pixel outer flash halo.
; Original pixels retain their palette entries; only the halo uses entry 159.
ngb_sprite_upload_flash_current:
    ld a, (asset_set_index)
    ld (ngb_source_set), a
    ld a, next_piece_flash_pattern_base
    ld (ngb_pattern_index), a
    xor a
    call ngb_sprite_upload_flash_side
    ld a, 1
    call ngb_sprite_upload_flash_side
    ; Source side is B: advance from source 1 to the four marked patterns 2..5.
    call ngb_piece_source_offset
    inc h
    ld a, next_piece_cursor_pattern_base
    ld d, next_piece_cursor_pattern_count
    jp ngb_sprite_upload

ngb_sprite_upload_flash_side:
    ld (ngb_source_side), a
    call ngb_piece_flash_source_offset
    jp ngb_sprite_upload_source

; Upload seven A/B phases from the selected source set. The first group uses
; the old set while it collapses; the second uses the new set while it opens.
ngb_sprite_upload_phase_group:
ngb_sprite_upload_phase_loop:
    ld a, (hl)
    ld (ngb_phase_start), a
    inc hl
    ld a, (hl)
    ld (ngb_phase_step), a
    inc hl
    push bc
    push hl
    xor a
    call ngb_sprite_upload_morph_side
    ld a, 1
    call ngb_sprite_upload_morph_side
    pop hl
    pop bc
    djnz ngb_sprite_upload_phase_loop
    ret

; A = side (0=A, 1=B). Reload the canonical RGB333 source for every phase so
; every build and every asset edit feeds the animation automatically.
ngb_sprite_upload_morph_side:
    ld (ngb_source_side), a
    call ngb_piece_source_offset
    ld de, next_sprite_stage
    ld bc, 256
    call next_copy_bundle

    ld hl, next_palette_stage
    ld de, next_palette_stage + 1
    ld bc, 255
    ld (hl), next_sprite_transparency
    ldir
    call ngb_project_morph_frame

    ld a, (ngb_pattern_index)
    ld bc, sprite_slot_port
    out (c), a
    ld hl, next_palette_stage
    ld bc, sprite_pattern_port
    otir
    ld hl, ngb_pattern_index
    inc (hl)
    ret

; Return HL = bundle offset of the source set's full-colour A/B pattern.
ngb_piece_source_offset:
    ld a, (ngb_source_set)
    ld h, a
    ld l, 0
    add hl, hl
    add hl, hl
    ld d, h
    ld e, l
    add hl, hl
    add hl, de
    ld a, (ngb_source_side)
    or a
    jr z, ngb_piece_source_base_ready
    inc h
ngb_piece_source_base_ready:
    ld de, next_sprite_offset
    add hl, de
    ret

; Return HL = bundle offset of source pattern 10/11 (outlined A/B).
ngb_piece_flash_source_offset:
    call ngb_piece_source_offset
    ld de, 10 * 256
    add hl, de
    ret

; Project source rows with the same 4.4 DDA as the Classic coin flip.
; Transparent pixels do not erase a wider row already mapped to the same
; destination row, matching the Classic OR composition.
ngb_project_morph_frame:
    push ix
    push iy
    ld ix, next_sprite_stage
    ld iy, next_sprite_stage + 240
    ld a, (ngb_phase_start)
    ld (ngb_phase_accum), a
    ld b, 8
ngb_project_morph_row:
    push bc
    ld a, (ngb_phase_accum)
    rrca
    rrca
    rrca
    rrca
    and 0x0f
    ld c, a

    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, next_palette_stage
    add hl, de
    ex de, hl
    push ix
    pop hl
    call ngb_copy_visible_row

    ld a, 15
    sub c
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, next_palette_stage
    add hl, de
    ex de, hl
    push iy
    pop hl
    call ngb_copy_visible_row

    ld de, 16
    add ix, de
    ld de, -16
    add iy, de
    ld a, (ngb_phase_accum)
    ld hl, ngb_phase_step
    add a, (hl)
    ld (ngb_phase_accum), a
    pop bc
    djnz ngb_project_morph_row
    pop iy
    pop ix
    ret

ngb_copy_visible_row:
    ld b, 16
ngb_copy_visible_pixel:
    ld a, (hl)
    cp next_sprite_transparency
    jr z, ngb_copy_visible_skip
    ld (de), a
ngb_copy_visible_skip:
    inc hl
    inc de
    djnz ngb_copy_visible_pixel
    ret

; Phases 1..7 collapse the old set; phases 8..14 expand the new set.
ngb_morph_phase_params:
    DEFB 15,15, 18,14, 24,14, 42,11, 64,8, 84,6, 112,0
    DEFB 112,0, 84,6, 64,8, 42,11, 24,14, 18,14, 15,15

SECTION bss_user

ngb_pattern_index: DEFS 1
ngb_source_set:    DEFS 1
ngb_source_side:   DEFS 1
ngb_phase_start:   DEFS 1
ngb_phase_step:    DEFS 1
ngb_phase_accum:   DEFS 1

SECTION code_user

ngb_sprite_upload_common:
    ld hl, next_common_sprite_offset
    ld a, next_common_sprite_pattern_base
    ld d, next_board_sprite_pattern_count
    call ngb_sprite_upload
    ld hl, next_marker_sprite_offset
    ld a, next_marker_sprite_pattern_base
    ld d, next_marker_sprite_pattern_count

ngb_sprite_upload:
    ld bc, sprite_slot_port
    out (c), a
ngb_sprite_pattern_loop:
    push de
    push hl
    ld de, next_sprite_stage
    ld bc, 256
    call next_copy_bundle
    pop hl
    inc h
    push hl
    ld hl, next_sprite_stage
    ld bc, sprite_pattern_port
    otir
    pop hl
    pop de
    dec d
    jr nz, ngb_sprite_pattern_loop
    ret
