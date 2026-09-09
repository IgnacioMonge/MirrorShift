SECTION code_user

PUBLIC _menu_config_run_ovl_entry
PUBLIC _menu_config_paint_attrs_ovl_entry
PUBLIC _menu_config_validate_ip_ovl_entry
PUBLIC _menu_config_edit_line_ovl_entry
PUBLIC _menu_config_render_ovl_entry
PUBLIC _menu_config_nav_ovl_entry
PUBLIC _menu_config_piece_set_options_asm

EXTERN _spectrum_info_line
EXTERN _spectrum_info_show_game_setup
EXTERN _spectrum_overlay_context
EXTERN _spectrum_info_show_setup
EXTERN _setup_choice
EXTERN _setup_focus_choice
EXTERN _setup_focus_board_theme
EXTERN _setup_game_focus
EXTERN _setup_visible_mask
EXTERN _setup_defined_mask
EXTERN _setup_cursor
EXTERN _setup_room_editing
EXTERN _setup_edit_row
EXTERN _setup_config_dirty
EXTERN _setup_action_focus
EXTERN _netchesszx_direct_host
EXTERN _last_ip

CTX             EQU 0x5FF8
CTX_KEY         EQU CTX
CTX_FORCE_LO    EQU CTX + 1
CTX_CLEAR_FROM  EQU CTX + 3
CTX_FLAGS       EQU CTX + 6

KEY_UP          EQU 0x81
KEY_DOWN        EQU 0x82
FLAG_PAINT      EQU 0x02
FLAG_TIME_UI    EQU 0x10
FLAG_ACTION_UI  EQU 0x20
PAINT_MASK_ALL  EQU 0x01EF

ROW_TIME        EQU 4
ROW_SIDE        EQU 5
ROW_BOARD       EQU 6
ROW_SET         EQU 7
ROW_HINTS       EQU 8
ROW_ACTION      EQU 9
SETUP_CLEAR_ENDPOINT EQU 0xfe

    DEFB 6
    DW _menu_config_run_ovl_entry
    DW _menu_config_paint_attrs_ovl_entry
    DW _menu_config_validate_ip_ovl_entry
    DW _menu_config_edit_line_ovl_entry
    DW _menu_config_render_ovl_entry
    DW _menu_config_nav_ovl_entry

_menu_config_run_ovl_entry:
    inc de
    inc de
    ld a, (de)
    ld c, a
    inc de
    ld a, (de)
    ld b, a
menu_config_run_dirty:
    bit 0, c
    ld hl, menu_config_line_game
    call nz, menu_config_info_line
    bit 1, c
    ld hl, menu_config_line_link
    call nz, menu_config_info_line
    bit 5, c
    jr nz, menu_config_run_game_header
    bit 6, c
    jr z, menu_config_run_game_header_done
    ld a, (_setup_visible_mask)
    bit 5, a
    jr nz, menu_config_run_game_header_done
menu_config_run_game_header:
    call menu_config_show_game_setup
menu_config_run_game_header_done:
    bit 5, c
    ld hl, menu_config_line_side
    call nz, menu_config_info_line
    bit 6, c
    ld hl, menu_config_line_board
    call nz, menu_config_info_line
    bit 7, c
    ld hl, menu_config_line_set
    call nz, menu_config_info_line
    bit 0, b
    ld hl, menu_config_line_hints
    call nz, menu_config_info_line
    ld l, 1
    ret

_menu_config_render_ovl_entry:
    push de
    ld hl, (_setup_visible_mask)
    ld a, (_setup_cursor)
    ld b, a
    inc b
    ld de, 1
menu_config_render_cursor_shift:
    dec b
    jr z, menu_config_render_cursor_mask
    sla e
    rl d
    jr menu_config_render_cursor_shift
menu_config_render_cursor_mask:
    ld a, l
    and e
    ld c, a
    ld a, h
    and d
    or c
    jr nz, menu_config_render_cursor_ready
    xor a
    ld (_setup_cursor), a
menu_config_render_cursor_ready:
    pop de
    inc de
    inc de
    ld a, (de)
    or a
    jr z, menu_config_render_incremental
    ld hl, (_setup_visible_mask)
    ld (menu_config_render_dirty), hl
    call _spectrum_info_show_setup
    jr menu_config_render_force
menu_config_render_incremental:
    inc de
    ld a, (de)
    cp SETUP_CLEAR_ENDPOINT
    jr nz, menu_config_render_incremental_normal
    ; CONNECTION rows only. Keep the GAME SETUP header and options intact.
    ld a, (_setup_visible_mask)
    and 0x0f
    ld l, a
    ld h, 0
    ld (menu_config_render_dirty), hl
    ld a, (_setup_visible_mask)
    bit 5, a
    jr nz, menu_config_endpoint_side
    ld a, 14
    call menu_config_clear_row
    jr menu_config_endpoint_action
menu_config_endpoint_side:
    ld a, (CTX)
    bit 5, a
    jr z, menu_config_endpoint_action
    ld hl, menu_config_line_side
    call menu_config_info_line
menu_config_endpoint_action:
    ld a, (_setup_visible_mask + 1)
    bit 1, a
    jr nz, menu_config_endpoint_apply
    ld a, 20
    call menu_config_clear_row
menu_config_endpoint_apply:
    ld hl, (menu_config_render_dirty)
    jr menu_config_render_apply
menu_config_render_incremental_normal:
    ld hl, (_setup_visible_mask)
    ld (menu_config_render_dirty), hl
    cp 0xff
    jr z, menu_config_render_force
    cp ROW_ACTION
    jr nz, menu_config_render_clear_normal
    ld a, 13
    jr menu_config_render_clear
menu_config_render_clear_normal:
    cp ROW_TIME
    jr c, menu_config_render_clear
    jr nz, menu_config_render_clear_game
    ld a, 3
    jr menu_config_render_clear
menu_config_render_clear_game:
    add a, 2
menu_config_render_clear:
    ld l, a
    call menu_config_clear_tail
menu_config_render_force:
    ld hl, (_spectrum_overlay_context)
    ld de, (_setup_visible_mask)
    ld a, l
    and e
    ld l, a
    ld a, h
    and d
    ld h, a
    ld de, (menu_config_render_dirty)
    ld a, l
    or e
    ld l, a
    ld a, h
    or d
    ld h, a
    ld (menu_config_render_dirty), hl
menu_config_render_apply:
    ld a, l
    and 0x0c
    cpl
    and l
    ld l, a
    ld a, h
    or l
    jr z, menu_config_render_overlay_done
    ld b, h
    ld c, l
    call menu_config_run_dirty
menu_config_render_overlay_done:
    ld hl, (menu_config_render_dirty)
    ld a, h
    or a
    jr nz, menu_config_paint_attrs_all
    ld a, l
    and 0xc0
    jr nz, menu_config_paint_attrs_all
    ; CONNECTION-only dirty: preserve GAME SETUP attributes and swatches.
    ld a, (_setup_visible_mask)
    and 0x20
    or l
    ld l, a
    jp menu_config_paint_attrs_masked

menu_config_clear_tail:
    ld a, l
    add a, 7
menu_config_clear_tail_loop:
    cp 21
    ret nc
    push af
    call menu_config_clear_row
    pop af
    inc a
    jr menu_config_clear_tail_loop

menu_config_clear_row:
    ld c, a
    and 7
    rrca
    rrca
    rrca
    add a, 18
    ld e, a
    ld a, c
    and 0x18
    add a, 0x40
    ld d, a
    ld b, 8
menu_config_clear_pixels:
    push bc
    push de
    ld h, d
    ld l, e
    ld (hl), 0
    inc de
    ld bc, 13
    ldir
    pop de
    pop bc
    inc d
    djnz menu_config_clear_pixels
    ld l, e
    ld a, c
    srl a
    srl a
    srl a
    add a, 0x58
    ld h, a
    ld (hl), 0x07
    ld d, h
    ld e, l
    inc de
    ld bc, 13
    ldir
    ret

menu_config_info_line:
    push bc
    call _spectrum_info_line
    pop bc
    ret

menu_config_show_game_setup:
    push bc
    call _spectrum_info_show_game_setup
    pop bc
    ret

_menu_config_paint_attrs_ovl_entry:
    ld hl, (CTX_FORCE_LO)
    ld a, h
    or l
    jr nz, menu_config_paint_attrs_masked
menu_config_paint_attrs_all:
    ld hl, PAINT_MASK_ALL
menu_config_paint_attrs_masked:
    ld (menu_config_paint_mask), hl
    ld hl, _setup_choice + 3
    call menu_config_pack_choices
    ld (menu_config_values), a
    ld hl, (_setup_visible_mask)
    ld a, l
    ld (menu_config_visible_l), a
    ld a, h
    ld (menu_config_visible_h), a
    ld hl, (_setup_defined_mask)
    ld a, l
    ld (menu_config_defined_l), a
    ld a, h
    ld (menu_config_defined_h), a
    ld a, (_setup_cursor)
    ld (menu_config_cursor), a
    ld hl, _setup_focus_choice + 3
    call menu_config_pack_choices
    ld (menu_config_focus_bits), a
    ld a, (_setup_focus_board_theme)
    ld (menu_config_board_theme), a
    ld a, (_setup_focus_choice + 4)
    ld (menu_config_piece_set), a
    call menu_config_paint_binary_attrs
    ld a, (menu_config_paint_mask)
    bit 2, a
    call nz, menu_config_paint_room_attrs
    ld a, (menu_config_paint_mask)
    bit 3, a
    call nz, menu_config_paint_mqtt_attrs
    ld a, (menu_config_paint_mask)
    bit 6, a
    call nz, menu_config_paint_board_attrs
    ld a, (menu_config_paint_mask)
    bit 7, a
    call nz, menu_config_paint_set_attrs
    ld l, 1
    ret

_menu_config_nav_ovl_entry:
    call menu_config_nav_clear_ctx
    ld a, (CTX_KEY)
    cp KEY_UP
    jr z, menu_config_nav_prev
    cp KEY_DOWN
    jr z, menu_config_nav_next
    ld l, 1
    ret

menu_config_nav_clear_ctx:
    xor a
    ld hl, CTX_FORCE_LO
    ld b, 6
menu_config_nav_clear_loop:
    ld (hl), a
    inc hl
    djnz menu_config_nav_clear_loop
    dec a
    ld (CTX_CLEAR_FROM), a
    ret

menu_config_nav_prev:
    xor a
    jr menu_config_nav_go
menu_config_nav_next:
    ld a, 1
menu_config_nav_go:
    ld b, a
    ld a, (_setup_cursor)
    ld (menu_config_nav_scratch), a
    ld a, b
    call menu_config_nav_step_row
    ld (_setup_cursor), a
    cp ROW_ACTION
    jr nz, menu_config_nav_paint
    ld a, (_setup_config_dirty)
    xor 1
    ld (_setup_action_focus), a
menu_config_nav_paint:
    ld a, (menu_config_nav_scratch)
    call menu_config_nav_mark_row
    ld a, (menu_config_nav_scratch)
    cp ROW_BOARD
    jr nz, menu_config_nav_mark_next
    ld a, (_setup_game_focus)
    ld (_setup_focus_board_theme), a
menu_config_nav_mark_next:
    ld a, (_setup_cursor)
    call menu_config_nav_mark_row
    ld l, 1
    ret
menu_config_nav_mark_row:
    push af
    call menu_config_nav_row_bit
    ld hl, (CTX_FORCE_LO)
    ld a, l
    or c
    ld l, a
    ld a, h
    or b
    ld h, a
    ld (CTX_FORCE_LO), hl
    pop af
    ld d, FLAG_TIME_UI
    cp ROW_TIME
    jr z, menu_config_nav_mark_flag
    ld d, FLAG_ACTION_UI
    cp ROW_ACTION
    jr z, menu_config_nav_mark_flag
    ld d, FLAG_PAINT
menu_config_nav_mark_flag:
    ld a, (CTX_FLAGS)
    or d
    ld (CTX_FLAGS), a
    ret

menu_config_nav_step_row:
    or a
    jr z, menu_config_nav_step_prev
    ld a, (_setup_cursor)
    inc a
menu_config_nav_step_next_loop:
    cp 10
    jr nc, menu_config_nav_step_restore
    call menu_config_nav_visible
    ld a, e
    ret nz
    inc a
    jr menu_config_nav_step_next_loop
menu_config_nav_step_prev:
    ld a, (_setup_cursor)
    or a
    jr z, menu_config_nav_step_restore
    dec a
menu_config_nav_step_prev_loop:
    call menu_config_nav_visible
    ld a, e
    ret nz
    or a
    jr z, menu_config_nav_step_restore
    dec a
    jr menu_config_nav_step_prev_loop
menu_config_nav_step_restore:
    ld a, (_setup_cursor)
    ret

menu_config_nav_visible:
    ld e, a
    cp 2
    jr nz, menu_config_nav_check_port
    ld a, (_setup_choice + 1)
    or a
    jr nz, menu_config_nav_bit_restore
    ld a, (_setup_choice)
    or a
    jr z, menu_config_nav_false
menu_config_nav_bit_restore:
    ld a, e
menu_config_nav_check_port:
    cp 3
    jr nz, menu_config_nav_bit
    ld a, (_setup_choice + 1)
    or a
    jr nz, menu_config_nav_false
    ld a, e
menu_config_nav_bit:
    call menu_config_nav_row_bit
    ld hl, (_setup_visible_mask)
    ld a, l
    and c
    ret nz
    ld a, h
    and b
    ret
menu_config_nav_false:
    xor a
    ret

menu_config_nav_row_bit:
    ld bc, 1
    or a
    ret z
menu_config_nav_row_bit_loop:
    sla c
    rl b
    dec a
    jr nz, menu_config_nav_row_bit_loop
    ret

menu_config_pack_choices:
    ld b, 4
    xor a
menu_config_pack_choices_loop:
    add a, a
    or (hl)
    dec hl
    djnz menu_config_pack_choices_loop
    ret

_menu_config_edit_line_ovl_entry:
    ld l, 0
    ret

menu_config_setup_screen_row:
    cp ROW_ACTION
    jr z, menu_config_setup_screen_row_action
    cp ROW_TIME
    jr z, menu_config_setup_screen_row_time
    cp 3
    jr nz, menu_config_setup_screen_row_normal
    dec a
menu_config_setup_screen_row_normal:
    ld e, a
menu_config_setup_screen_row_norm:
    ld a, e
    cp ROW_SIDE
    ld a, e
    jr c, menu_config_setup_screen_row_base
    add a, 2
menu_config_setup_screen_row_base:
    add a, 7
    ret
menu_config_setup_screen_row_time:
    ld a, 10
    ret
menu_config_setup_screen_row_action:
    ld a, 20
    ret

menu_config_paint_binary_attrs:
    ld hl, menu_config_binary_attrs
    ld a, 1
menu_config_paint_binary_loop:
    ld (menu_config_binary_bit), a
    ld a, (hl)
    or a
    ret z
    ld (menu_config_binary_mask), a
    inc hl
    ld a, (hl)
    ld c, a
    and 0x3f
    ld (menu_config_binary_row), a
    ld a, c
    ld (menu_config_binary_invert), a
    inc hl
    push hl
    bit 7, c
    jr nz, menu_config_paint_binary_high
    ld a, (menu_config_paint_mask)
    ld b, a
    ld a, (menu_config_visible_l)
    jr menu_config_paint_binary_active
menu_config_paint_binary_high:
    ld a, (menu_config_paint_mask + 1)
    ld b, a
    ld a, (menu_config_visible_h)
menu_config_paint_binary_active:
    and b
    ld b, a
    ld a, (menu_config_binary_mask)
    and b
    jr z, menu_config_paint_binary_skip
    call menu_config_prepare_binary
    ld a, (menu_config_binary_row)
    call menu_config_setup_screen_row
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, 0x5812
    add hl, de
    ex (sp), hl
    ld b, (hl)
    inc hl
    ex (sp), hl
    call menu_config_header_span
    ; CONNECTION SETUP uses NetChessZX's shifted half-character grid;
    ; GAME SETUP retains its four-cell labels without extra padding.
menu_config_paint_binary_left:
    ex (sp), hl
    ld b, (hl)
    inc hl
    ld c, b
    ex (sp), hl
    call menu_config_paint_left_binary
    ld a, 5
    sub c
menu_config_paint_binary_gap:
    inc hl
    dec a
    jr nz, menu_config_paint_binary_gap
    ex (sp), hl
    ld b, (hl)
    inc hl
    ex (sp), hl
    call menu_config_paint_right_binary
    pop hl
    jr menu_config_paint_binary_next
menu_config_paint_binary_skip:
    pop hl
    inc hl
    inc hl
    inc hl
menu_config_paint_binary_next:
    ld a, (menu_config_binary_bit)
    add a, a
    jr menu_config_paint_binary_loop

menu_config_prepare_binary:
    ld a, (menu_config_binary_bit)
    ld b, a
    ld a, (menu_config_values)
    and b
    ld c, a
    ld a, (menu_config_focus_bits)
    and b
    ld e, a
    ld a, (menu_config_binary_invert)
    bit 6, a
    jr z, menu_config_prepare_binary_no_invert
    ld a, c
    xor b
    ld c, a
    ld a, e
    xor b
    ld e, a
menu_config_prepare_binary_no_invert:
    ld a, c
    ld (menu_config_value_flag), a
    ld a, e
    ld (menu_config_focus_value), a
    ld a, (menu_config_binary_invert)
    bit 7, a
    ld a, (menu_config_defined_l)
    jr z, menu_config_prepare_binary_defined
    ld a, (menu_config_defined_h)
menu_config_prepare_binary_defined:
    ld c, a
    ld a, (menu_config_binary_mask)
    and c
    ld (menu_config_defined_flag), a
    ld a, (menu_config_cursor)
    ld c, a
    ld a, (menu_config_binary_row)
    cp c
    ld a, 0
    jr nz, menu_config_prepare_binary_row_done
    inc a
menu_config_prepare_binary_row_done:
    ld (menu_config_row_focus), a
    ret

menu_config_header_span:
    ld a, 0x03
    jp menu_config_attr_span

menu_config_paint_left_binary:
    ld a, b
    ld (menu_config_option_width), a
    ld d, 0
    ld a, (menu_config_defined_flag)
    or a
    jr z, menu_config_left_selected_done
    ld a, (menu_config_value_flag)
    or a
    jr nz, menu_config_left_selected_done
    inc d
menu_config_left_selected_done:
    ld e, 0
    ld a, (menu_config_row_focus)
    or a
    jr z, menu_config_left_focused_done
    ld a, (menu_config_focus_value)
    or a
    jr nz, menu_config_left_focused_done
    inc e
menu_config_left_focused_done:
    ld a, (menu_config_option_width)
    ld b, a
    jp menu_config_option_span

menu_config_paint_right_binary:
    ld a, b
    ld (menu_config_option_width), a
    ld d, 0
    ld a, (menu_config_defined_flag)
    or a
    jr z, menu_config_right_selected_done
    ld a, (menu_config_value_flag)
    or a
    jr z, menu_config_right_selected_done
    inc d
menu_config_right_selected_done:
    ld e, 0
    ld a, (menu_config_row_focus)
    or a
    jr z, menu_config_right_focused_done
    ld a, (menu_config_focus_value)
    or a
    jr z, menu_config_right_focused_done
    inc e
menu_config_right_focused_done:
    ld a, (menu_config_option_width)
    ld b, a
    jp menu_config_option_span

; A = defined bit, C = setup row. D=1 if defined and not being edited.
; B (span width) is preserved; E is scratch.
menu_config_text_selected:
    ld e, a
    ld d, 0
    ld a, (_setup_room_editing)
    or a
    jr z, menu_config_text_selected_def
    ld a, (_setup_edit_row)
    cp c
    ret z
menu_config_text_selected_def:
    ld a, (menu_config_defined_l)
    and e
    ret z
    inc d
    ret

menu_config_paint_room_attrs:
    ld a, (menu_config_visible_l)
    bit 2, a
    ret z
    ld hl, 0x5932
    ld de, 0x5935
    push de
    ld b, 3
    call menu_config_header_span
    pop hl
    ld a, (menu_config_values)
    bit 1, a
    jr nz, menu_config_room_mqtt
    ; DIRECT keeps IP and PORT as independent controls. Repaint the complete
    ; IP span first so a shorter address cannot inherit stale attributes.
    ld a, 4
    ld c, 2
    call menu_config_text_selected
    push hl
    ld e, 0
    ld b, 8
    call menu_config_option_span
    pop hl
    call menu_config_direct_ip_width
    ld b, a
    jr menu_config_room_width_done
menu_config_room_mqtt:
    ld b, 4
menu_config_room_width_done:
    ld a, 4
    ld c, 2
    call menu_config_text_selected
    call menu_config_focus_box
    jp menu_config_option_span

menu_config_direct_ip_width:
    ld de, _last_ip
    ld a, (_setup_choice)
    or a
    jr z, menu_config_direct_ip_count
    ld de, _netchesszx_direct_host
menu_config_direct_ip_count:
    ld b, 0
menu_config_direct_ip_count_loop:
    ld a, (de)
    or a
    jr z, menu_config_direct_ip_width_ready
    inc de
    inc b
    jr menu_config_direct_ip_count_loop
menu_config_direct_ip_width_ready:
    ld a, b
    or a
    jr nz, menu_config_direct_ip_nonempty
    inc a
menu_config_direct_ip_nonempty:
    add a, 2
    srl a
    cp 9
    ret c
    ld a, 8
    ret

menu_config_paint_mqtt_attrs:
    ld a, (menu_config_visible_l)
    bit 2, a
    ret z
    ld a, (_setup_choice + 1)
    or a
    jr z, menu_config_port_attrs
    ; HIVEMQ starts after the same four-pixel shift as GAME/LINK.
    ld hl, 0x593a
    ld b, 4
    ld a, 0x05
    jp menu_config_attr_span
menu_config_port_attrs:
    ld a, (menu_config_visible_l)
    bit 3, a
    ret z
    ; Colon plus five port digits occupy attrs 29..31.
    ld hl, 0x593d
    ld b, 3
    ld a, 8
    ld c, 3
    call menu_config_text_selected
    call menu_config_focus_box
    ld a, e
    or a
    jr z, menu_config_port_unfocused
    ; As in NetChessZX, the focused PORT marker temporarily occupies ':';
    ; clearing focus restores the separator pixels exactly.
    push hl
    ld hl, 0x4b3d
    res 7, (hl)
    ld h, 0x4e
    res 7, (hl)
    pop hl
    jp menu_config_option_span
menu_config_port_unfocused:
    call menu_config_option_span
    ld hl, 0x4b3d
    set 7, (hl)
    ld h, 0x4e
    set 7, (hl)
    ret

; C = setup row. Sets E if the cursor is on that row and it is not being edited.
menu_config_focus_box:
    ld e, 0
    ld a, (_setup_room_editing)
    or a
    jr z, menu_config_focus_box_cursor
    ld a, (_setup_edit_row)
    cp c
    ret z
menu_config_focus_box_cursor:
    ld a, (menu_config_cursor)
    cp c
    ret nz
    inc e
    ret

menu_config_paint_board_attrs:
    ld a, (menu_config_visible_l)
    bit 6, a
    ret z
    ld hl, 0x59f2
    ld b, 4
    call menu_config_header_span
    ld a, (menu_config_cursor)
    cp ROW_BOARD
    jr nz, menu_config_board_not_cursor
    ld a, (menu_config_board_theme)
    ld l, a
    jp menu_config_board_swatches
menu_config_board_not_cursor:
    ld a, (menu_config_defined_l)
    bit 6, a
    jr z, menu_config_board_not_defined
    ld a, (menu_config_board_theme)
    add a, 5
    ld l, a
    jp menu_config_board_swatches
menu_config_board_not_defined:
    ld l, 0xff
    jp menu_config_board_swatches

menu_config_paint_set_attrs:
    ld a, (menu_config_visible_l)
    bit 7, a
    ret z
    ld hl, 0x5a12
    ld b, 3
    call menu_config_header_span
    ld a, (menu_config_defined_l)
    bit 7, a
    jr nz, menu_config_set_selected_ready
    ld a, 0xff
    jr menu_config_set_selected_store
menu_config_set_selected_ready:
    ld a, (_setup_choice + 4)
menu_config_set_selected_store:
    ld (menu_config_piece_selected), a
    ld a, (menu_config_cursor)
    cp ROW_SET
    jr nz, menu_config_set_not_cursor
    ld a, (menu_config_piece_set)
    ld l, a
    jp _menu_config_piece_set_options_asm
menu_config_set_not_cursor:
    ld l, 0xff
    jp _menu_config_piece_set_options_asm

menu_config_option_span:
    call menu_config_option_marker
    ld a, e
    or a
    jr z, menu_config_option_not_focused
    ; Ink-only focus avoids spilling paper into the neighbouring 4-pixel glyph.
    ld a, d
    or a
    ld a, 0x47
    jr z, menu_config_attr_span
    dec a
    jr menu_config_attr_span
menu_config_option_not_focused:
    ld a, d
    or a
    ld a, 0x05
    jr nz, menu_config_attr_span
    ld a, 0x07
menu_config_attr_span:
    ld (hl), a
    inc hl
    djnz menu_config_attr_span
    ret

; HL = first option attribute cell, E = focus. Marker occupies the preceding
; blank 4-pixel character; preserve selection and focus in DE for the ink span.
menu_config_option_marker:
    push bc
    push de
    push hl
    ld b, 0
    ld a, e
    or a
    jr z, menu_config_marker_focus_ready
    inc b
menu_config_marker_focus_ready:
    ld a, h
    sub 0x58
    add a, a
    add a, a
    add a, a
    ld d, a
    ld a, l
    and 0xe0
    rlca
    rlca
    rlca
    or d
    ld d, a
    ld a, l
    and 0x1f
    add a, a
    ld e, a
    ld a, d
    cp 14
    jr c, menu_config_marker_col
    cp 19
    jr nc, menu_config_marker_col
    dec e
menu_config_marker_col:
    ld a, b
    or a
    ld c, 0x03
    jr z, menu_config_marker_attr_ready
    ld c, 0x47
menu_config_marker_attr_ready:
    ld a, d
    cp 14
    jr c, menu_config_marker_attr_store
    cp 19
    jr nc, menu_config_marker_attr_store
    dec hl
menu_config_marker_attr_store:
    ld (hl), c

    ld a, e
    srl a
    ld c, a
    ld a, d
    and 0x18
    add a, 0x40
    ld h, a
    ld a, d
    and 0x07
    rrca
    rrca
    rrca
    add a, c
    ld l, a
    bit 0, e
    ld de, menu_config_marker_left
    jr z, menu_config_marker_pixels
    ld de, menu_config_marker_right
menu_config_marker_pixels:
    inc h
    ld c, 6
menu_config_marker_scan:
    ld a, (de)
    cpl
    and (hl)
    ld (hl), a
    ld a, b
    or a
    jr z, menu_config_marker_next
    ld a, (de)
    or (hl)
    ld (hl), a
menu_config_marker_next:
    inc de
    inc h
    dec c
    jr nz, menu_config_marker_scan
    pop hl
    pop de
    pop bc
    ret

; 4x8 glyph; the cell supplies blank top and bottom scanlines.
menu_config_marker_left:
    DEFB 0x00, 0x40, 0x20, 0x20, 0x40, 0x00
menu_config_marker_right:
    DEFB 0x00, 0x04, 0x02, 0x02, 0x04, 0x00

_menu_config_validate_ip_ovl_entry:
    ld l, 0
    ret

_menu_config_piece_set_options_asm:
    ld c, l
    ld b, 0
    ld hl, 0x5a16
    ld d, 2
    call menu_config_piece_set_option_span
    ld b, 1
    ld hl, 0x5a19
    ld d, 2
    call menu_config_piece_set_option_span
    ld b, 2
    ld hl, 0x5a1c
    ld d, 2
menu_config_piece_set_option_span:
    ld e, 0
    ld a, c
    cp b
    jr nz, menu_config_piece_set_marker
    inc e
menu_config_piece_set_marker:
    call menu_config_option_marker
    call menu_config_piece_set_option_attr
menu_config_piece_set_option_store:
    ld (hl), a
    inc hl
    dec d
    jr nz, menu_config_piece_set_option_store
    ret

menu_config_piece_set_option_attr:
    ld a, c
    cp b
    jr z, menu_config_piece_set_option_focus
    ld a, (menu_config_piece_selected)
    cp b
    jr z, menu_config_piece_set_option_selected
    ld a, 0x07
    ret
menu_config_piece_set_option_focus:
    ld a, (menu_config_piece_selected)
    cp b
    ld a, 0x47
    ret nz
    dec a
    ret
menu_config_piece_set_option_selected:
    ld a, 0x05
    ret

; Classic swatches read the theme attrs from the resident DAT. Next uses five
; dedicated ULA+ group-3 pairs that mirror the glass-board sprite palettes.
IFDEF NETCHESSZX_NEXT
menu_config_dat_light_attrs EQU 0x3500 + 786
ELSE
menu_config_dat_light_attrs EQU 0x6000 + 786
ENDIF

menu_config_board_swatches:
IFDEF NETCHESSZX_NEXT
    ; The setup font's 127 glyph is only a small ornament inside the attribute
    ; cell. Replace it with a full 8x8 diagonal split so every swatch shows the
    ; actual light/dark board pair without a black gap or irregular outline.
    push hl
    call menu_config_next_board_swatch_pixels
    pop hl
ENDIF
    ld c, l
    ld hl, 0x59f6
    ld b, 0
IFDEF NETCHESSZX_NEXT
    ld de, menu_config_next_board_swatch_attrs
ELSE
    ld de, menu_config_dat_light_attrs
ENDIF
menu_config_board_swatch_loop:
    ld a, (de)
    push de
    call menu_config_board_swatch
    pop de
    inc de
    inc b
    ld a, b
    cp 5
    jr nz, menu_config_board_swatch_loop
    ret
menu_config_board_swatch:
    push af
    ld e, 0
    ld a, c
    cp b
    jr nz, menu_config_board_swatch_marker
    inc e
menu_config_board_swatch_marker:
    call menu_config_option_marker
    pop af
    ld e, a
    ld a, c
    cp b
    ld a, e
    jr z, menu_config_board_swatch_flash
IFDEF NETCHESSZX_NEXT
    jr menu_config_board_swatch_store
ELSE
    ld a, b
    add a, 5
    cp c
    ld a, e
    jr nz, menu_config_board_swatch_store
    or 0x40
ENDIF
    jr menu_config_board_swatch_store
menu_config_board_swatch_flash:
IFDEF NETCHESSZX_NEXT
    ; Normal swatches use light/dark pairs in group 3. Focus moves to the
    ; matching brighter pair at group-2 indices 3..7; indices 0..2
    ; remain reserved for board coordinates and their selected state.
    ld a, b
    add a, 3
    ld d, a
    add a, a
    add a, a
    add a, a
    or d
    or 0x80
ELSE
    ld a, e
    and 0xc0
    ld d, a
    ld a, e
    and 0x07
    rlca
    rlca
    rlca
    or d
    ld d, a
    ld a, e
    and 0x38
    rrca
    rrca
    rrca
    or d
    or 0x80
ENDIF
menu_config_board_swatch_store:
    ld (hl), a
    inc hl
    ld (hl), 0x07
    inc hl
    ret

IFDEF NETCHESSZX_NEXT
menu_config_next_board_swatch_attrs:
    DEFB 0xc0,0xc9,0xd2,0xdb,0xe4

menu_config_next_board_swatch_pixels:
    ld hl, 0x48f6
    ld b, 5
menu_config_next_board_swatch_pixel_cell:
    push bc
    push hl
    ld de, menu_config_next_board_swatch_pattern
    ld b, 8
menu_config_next_board_swatch_pixel_scan:
    ld a, (de)
    ld (hl), a
    inc de
    inc h
    djnz menu_config_next_board_swatch_pixel_scan
    pop hl
    inc l
    inc l
    pop bc
    djnz menu_config_next_board_swatch_pixel_cell
    ret

; INK fills the lit upper-left triangle; PAPER fills the darker lower-right.
menu_config_next_board_swatch_pattern:
    DEFB 0xfe,0xfc,0xf8,0xf0,0xe0,0xc0,0x80,0x00
ENDIF

menu_config_line_game:
    DEFB 7, "GAME  CREATE    JOIN", 0
menu_config_line_link:
    DEFB 8, "LINK  MQTT      DIRECT", 0
menu_config_line_side:
    DEFB 14, "ALIGN  SELF      ECHO ", 0
menu_config_line_board:
    DEFB 15, "BOARD  ", 127, "   ", 127, "   ", 127, "   ", 127, "   ", 127, 0
menu_config_line_set:
    DEFB 16, "DISCS  LATT  NEXU  PARA", 0
menu_config_line_hints:
    DEFB 17, "HINTS  OFF       ON", 0
menu_config_binary_attrs:
    DEFB 1, 0x00, 3, 4, 3
    DEFB 2, 0x41, 3, 3, 4
    DEFB 32, 0x45, 4, 3, 3
    DEFB 1, 0x88, 4, 2, 1
    DEFB 0
menu_config_values:
    DEFB 0
menu_config_visible_l:
    DEFB 0
menu_config_visible_h:
    DEFB 0
menu_config_defined_l:
    DEFB 0
menu_config_defined_h:
    DEFB 0
menu_config_cursor:
    DEFB 0
menu_config_focus_bits:
    DEFB 0
menu_config_board_theme:
    DEFB 0
menu_config_piece_set:
    DEFB 0
menu_config_piece_selected:
    DEFB 0
menu_config_binary_mask:
    DEFB 0
menu_config_binary_row:
    DEFB 0
menu_config_binary_bit:
    DEFB 0
menu_config_binary_invert:
    DEFB 0
menu_config_defined_flag:
    DEFB 0
menu_config_value_flag:
    DEFB 0
menu_config_focus_value:
    DEFB 0
menu_config_row_focus:
    DEFB 0
menu_config_option_width:
    DEFB 0
menu_config_render_dirty:
    DEFW 0
menu_config_nav_scratch:
    DEFB 0
menu_config_paint_mask:
    DEFW 0
