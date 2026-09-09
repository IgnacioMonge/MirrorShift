; Executes the production loader with modeled ROM vectors, not cartridge hardware.
SECTION code_user
test_start:
    ld sp, 0xFF00
    ld ix, 0x1234
    im 1
    ld a, 0xFE
    ld (0x7000), a
    ld a, 0xFB
    ld (0x0038), a
    ld a, 0xC9
    ld (0x0039), a
    ld hl, mock_pagein
    ld de, 0x3FF9
    call install_jump
    ld hl, mock_pageout
    ld de, 0x007C
    call install_jump
    ld hl, mock_setpage
    ld de, 0x3E36
    call install_jump
    ld hl, mock_pushpage
    ld de, 0x3E87
    call install_jump
    ld hl, mock_poppage
    ld de, 0x3E8A
    call install_jump
    ld hl, mock_reserve
    ld de, 0x3E9F
    call install_jump
    ld hl, mock_free
    ld de, 0x3EA2
    call install_jump
    xor a
    ld (test_error), a
    ld (test_mode), a
    ld (ovl_file_open), a
    ld (ovl_xfs_ready), a
    ld (ovl_call_active), a
    ld (_spxn_rom_held), a
    ld a, 0x49
    ld (test_page), a
    ld iy, 0x5C3A
    ld hl, ovl_handle
    ld b, __bss_user_size
    ld a, 0xAA
dirty_bss:
    ld (hl), a
    inc hl
    djnz dirty_bss
    ld hl, 0xA55A
    ld (test_bss_tail), hl
    ei
    call _spectrum_assets_load
    ld a, l
    cp 1
    jp nz, test_fail
    ld a, (test_reserves)
    cp ovl_atlas_count
    jp nz, test_fail
    ld a, (test_page)
    cp 0x49
    jp nz, test_fail
    ld a, (test_reads)
    cp ovl_atlas_count + 3 ; fingerprint + three chunks + remaining small overlays
    jp nz, test_fail
    ld a, (0x2008)
    cp 2
    jp nz, test_fail
    ld a, (0x25DE)
    cp 3
    jp nz, test_fail
    ld a, (0x2BBC)
    cp 4
    jp nz, test_fail
    ld a, (0x2FFF)
    cp 4
    jp nz, test_fail
    call assert_restored

    ; Successful entry calls the bridge and attempts nested dispatch while
    ; held and again from the input tick while the ROM is temporarily out.
    ld hl, test_overlay
    ld de, 0x2000
    ld bc, test_overlay_end - test_overlay
    ldir
    ld hl, ovl_atlas_count - 1 ; Last page: NOTICES.
    push hl
    call _spectrum_overlay_exec
    pop bc
    ld de, 0x1234
    or a
    sbc hl, de
    jp nz, test_fail
    call assert_restored
    ld a, (test_ticks)
    cp 1
    jp nz, test_fail

    ; Original DI survives return too.
    di
    ld hl, 0
    push hl
    call _spectrum_overlay_exec_cached
    pop bc
    ld a, i
    jp pe, test_fail
    ei

    ; Invalid ID, invalid entry index, and an out-of-payload target fail closed.
    ld hl, ovl_atlas_count
    call expect_rejected
    ld hl, 0x0100
    call expect_rejected
    ld hl, 0x3000
    ld (0x2001), hl
    ld hl, 0
    call expect_rejected

    ; Fingerprint mismatch allocates nothing.
    ld a, 1
    ld (test_mode), a
    push ix ; assets_load owns IX across this private preload call.
    call ovl_spxn_preload_all
    pop ix
    jp nc, test_fail
    ld a, (test_reserves)
    or a
    jp nz, test_fail
    ; Allocation failure frees all pages already reserved.
    ld a, 2
    ld (test_mode), a
    push ix ; assets_load owns IX across this private preload call.
    call ovl_spxn_preload_all
    pop ix
    jp nc, test_fail
    ld a, (test_frees)
    cp 2
    jp nz, test_fail
    ; Duplicate allocation is rejected and freed only once.
    ld a, 3
    ld (test_mode), a
    push ix ; assets_load owns IX across this private preload call.
    call ovl_spxn_preload_all
    pop ix
    jp nc, test_fail
    ld a, (test_frees)
    cp 1
    jp nz, test_fail
    ; A short read releases its page and never publishes ready.
    ld a, 4
    ld (test_mode), a
    push ix ; assets_load owns IX across this private preload call.
    call ovl_spxn_preload_all
    pop ix
    jp nc, test_fail
    ld a, (test_frees)
    cp 1
    jp nz, test_fail
    ld a, (ovl_spxn_pages_ready)
    or a
    jp nz, test_fail
    call assert_restored
    ld hl, 0
    add hl, sp
    ld de, 0xFF00
    or a
    sbc hl, de
    jp nz, test_fail
    ld a, (test_error)
    ld (0x7000), a
    jp 0
test_fail:
    ld a, 0xFF
    ld (0x7000), a
    jp 0

expect_rejected:
    push hl
    call _spectrum_overlay_exec_cached
    pop bc
    ld a, h
    or l
    jp nz, test_fail
assert_restored:
    push ix
    pop hl
    ld de, 0x1234
    or a
    sbc hl, de
    jp nz, test_fail
    ld a, (test_mapped)
    or a
    jp nz, test_fail
    ld a, (_spxn_rom_held)
    or a
    jp nz, test_fail
    ld a, (ovl_call_active)
    or a
    jp nz, test_fail
    ld a, (test_page)
    cp 0x49
    jp nz, test_fail
    push iy
    pop hl
    ld de, 0x5C3A
    or a
    sbc hl, de
    jp nz, test_fail
    ld a, i
    jp po, test_fail
    ret

test_overlay:
    defb 1
    defw 0x2003
    jp test_entry
test_overlay_end:
test_entry:
    ld bc, 0x5FF8
    or a
    sbc hl, bc
    jp nz, test_fail
    ex de, hl
    or a
    sbc hl, bc
    jp nz, test_fail
    ld hl, 0x0101
    call expect_nested
    ld ix, 0x5678
    call _spectrum_spxn_frame_wait
    push ix
    pop hl
    ld de, 0x5678
    or a
    sbc hl, de
    jp nz, test_fail
    ld a, (_spxn_rom_held)
    cp 1
    jp nz, test_fail
    ld a, i
    jp pe, test_fail
    ld hl, 0x1234
    ret
expect_nested:
    push hl
    call _spectrum_overlay_exec_cached
    pop bc
    ld a, h
    or l
    jp nz, test_fail
    ret
_spectrum_input_frame_tick:
    ld a, (test_mapped)
    or a
    jp nz, test_fail
    ld a, (_spxn_rom_held)
    or a
    jp nz, test_fail
    ld hl, test_ticks
    inc (hl)
    ld hl, 0x0202
    jp expect_nested

install_jump:
    ld a, 0xC3
    ld (de), a
    inc de
    ld a, l
    ld (de), a
    inc de
    ld a, h
    ld (de), a
    ret
mock_pagein:
    ld iy, 0xDEAD
    ld ix, 0xDEAD ; ROM ABI promises neither index register.
    call assert_di
    ld a, 1
    ld (test_mapped), a
    ret
mock_pageout:
    ld iy, 0xDEAD
    ld ix, 0xDEAD
    call assert_di
    xor a
    ld (test_mapped), a
    ret
assert_di:
    push af
    ld a, i
    jp pe, test_fail
    pop af
    ret
mock_setpage:
    call assert_di
    ld (test_page), a
    ld iy, 0xDEAD ; ROM does not promise IY
    ld ix, 0xDEAD
    ret
mock_pushpage:
    pop hl
    ld c, a
    ld a, (test_page)
    push af
    push hl
    ld a, c
    jp mock_setpage
mock_poppage:
    pop hl
    pop af
    push hl
    jp mock_setpage
mock_reserve:
    call assert_di
    or a
    jp z, test_fail
    ld a, (test_reserves)
    ld b, a
    ld a, (test_mode)
    cp 2
    jr nz, reserve_no_fail
    ld a, b
    cp 2
    jr nz, reserve_no_fail
    scf
    ret
reserve_no_fail:
    ld a, b
    inc a
    ld (test_reserves), a
    ld a, (test_mode)
    cp 3
    ld a, 0xC3
    ret z
    add a, b
    or a
    ret
mock_free:
    call assert_di
    ld hl, test_frees
    inc (hl)
    ret

_spxn_detect:
    ld hl, 1
    ret
_esx_fopen:
    xor a
    ld (test_reserves), a
    ld (test_reads), a
    ld (test_frees), a
    inc a
    ld (_esx_handle), a
    ret
_esx_fread:
    ld hl, (_esx_buf)
    ld a, h
    cp 0x40
    jp c, test_fail
    ld a, (test_reads)
    inc a
    ld (test_reads), a
    cp 1
    jr nz, read_payload
    ld hl, _overlay_scratch_base + 96
    ld (hl), 11
    inc hl
    ld (hl), 22
    inc hl
    ld (hl), 33
    inc hl
    ld a, (test_mode)
    cp 1
    ld a, 44
    jr nz, fingerprint_good
    xor a
fingerprint_good:
    ld (hl), a
    jr read_count
read_payload:
    ld hl, (_esx_buf)
    ld bc, (_esx_count)
    ld a, (test_reads)
read_fill:
    ld (hl), a
    inc hl
    dec bc
    ld d, a
    ld a, b
    or c
    ld a, d
    jr nz, read_fill
read_count:
    ld hl, (_esx_count)
    ld a, (test_mode)
    cp 4
    jr nz, read_done
    ld a, (test_reads)
    cp 2
    jr nz, read_done
    dec hl
read_done:
    ld (_esx_result), hl
    ret
_esx_fclose:
_spxn_xfs_fseek:
    ld hl, 0
    ret
_spxn_xfs_init:
    ld hl, ovl_handle
    ld b, __bss_user_size
check_bss_clear:
    ld a, (hl)
    or a
    jp nz, test_fail
    inc hl
    djnz check_bss_clear
    ld hl, (test_bss_tail)
    ld de, 0xA55A
    or a
    sbc hl, de
    jp nz, test_fail
    ret
_spectrum_uart_background_pump:
    ret
test_error: defb 0
test_mode: defb 0
test_page: defb 0
test_mapped: defb 0
test_reserves: defb 0
test_reads: defb 0
test_frees: defb 0
test_ticks: defb 0
_esx_handle: defb 0
_esx_buf: defw 0
_esx_count: defw 0
_esx_result: defw 0
_netchesszx_piece_set_index: defb 0
