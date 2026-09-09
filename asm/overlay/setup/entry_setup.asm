SECTION code_user

PUBLIC _setup_step_ovl_entry

EXTERN _setup_choice
EXTERN _setup_focus_choice
EXTERN _setup_focus_board_theme
EXTERN _setup_game_focus
EXTERN _setup_defined_mask
EXTERN _setup_visible_mask
EXTERN _setup_cursor
EXTERN _setup_room_editing
EXTERN _setup_edit_row
EXTERN _setup_port_text
EXTERN _setup_config_dirty
EXTERN _setup_action_focus
EXTERN _setup_edit_backup
EXTERN _netchesszx_mqtt_code
EXTERN _netchesszx_direct_host
EXTERN _netchesszx_direct_port
EXTERN _spectrum_gui_edit_bind
EXTERN _spectrum_gui_edit_key
EXTERN _spectrum_gui_edit_hide
EXTERN _spectrum_append_u16
EXTERN _netchess_mqtt_session_parse_u16_token
EXTERN _edit_buf
EXTERN _edit_max

CTX             EQU 0x5FF8
CTX_KEY         EQU CTX + 0
CTX_FORCE_LO    EQU CTX + 1
CTX_FORCE_HI    EQU CTX + 2
CTX_CLEAR_FROM  EQU CTX + 3
CTX_ACTION      EQU CTX + 4
CTX_NOTICE      EQU CTX + 5
CTX_FLAGS       EQU CTX + 6

; Selection and edit-result paths are mutually exclusive within one entry.
SU_MARKS_CONFIG EQU CTX_NOTICE

KEY_UP          EQU 0x81
KEY_DOWN        EQU 0x82
KEY_LEFT        EQU 0x83
KEY_RIGHT       EQU 0x84
KEY_HOME        EQU 0x88
KEY_END         EQU 0x89
KEY_CANCEL      EQU 0x8a
SETUP_CLEAR_NONE EQU 0xff
SETUP_CLEAR_ENDPOINT EQU 0xfe

FLAG_RENDER     EQU 0x01
FLAG_PAINT      EQU 0x02
FLAG_EDIT       EQU 0x04
FLAG_SUPPRESS   EQU 0x08
FLAG_TIME_UI    EQU 0x10
FLAG_ACTION_UI  EQU 0x20
ACTION_SET      EQU 2
NOTICE_SELECT   EQU 1
NOTICE_BAD_IP   EQU 3
NOTICE_BAD_ROOM EQU 4
NOTICE_BAD_PORT EQU 5

ROW_GAME        EQU 0
ROW_LINK1       EQU 1
ROW_LINK2       EQU 2
ROW_TIME        EQU 4
ROW_SIDE        EQU 5
ROW_BOARD       EQU 6
ROW_SET         EQU 7
ROW_HINTS       EQU 8
ROW_ACTION      EQU 9

    DEFB 2
    DW _setup_step_ovl_entry
    DW _setup_compute_visible_ovl_entry

_setup_compute_visible_ovl_entry:
    push de
    ld h, d
    ld l, e
    ld c, (hl)
    inc hl
    ld b, (hl)
    call su_compute_visible
    pop de
    ld a, l
    ld (de), a
    inc de
    ld a, h
    ld (de), a
    ret

_setup_step_ovl_entry:
    call su_clear_ctx
    ld a, (_setup_room_editing)
    or a
    jp nz, su_edit_key
    ld a, (CTX_KEY)
    cp KEY_UP
    jr z, su_nav_prev
    cp KEY_DOWN
    jr z, su_nav_next
    cp KEY_LEFT
    jr z, su_focus
    cp KEY_RIGHT
    jr z, su_focus
    cp 32
    jp z, su_select
    cp 13
    jp z, su_select
    jr su_ret1

; A = flag bits; ORs into CTX_FLAGS then falls through to return 1.
su_or_flags:
    ld hl, CTX_FLAGS
    or (hl)
    ld (hl), a
su_ret1:
    ld l, 1
    ret

su_clear_ctx:
    xor a
    ld hl, CTX_FORCE_LO
    ld b, 6
su_clear_loop:
    ld (hl), a
    inc hl
    djnz su_clear_loop
    dec a
    ld (CTX_CLEAR_FROM), a
    ret

su_nav_prev:
    xor a
    jr su_nav_go
su_nav_next:
    ld a, 1
su_nav_go:
    ld b, a
    ld a, (_setup_cursor)
    ld (su_scratch), a
    ld a, b
    call su_step_row
    ld (_setup_cursor), a
    cp ROW_ACTION
    jr nz, su_nav_paint
    ld a, (_setup_config_dirty)
    xor 1
    ld (_setup_action_focus), a
su_nav_paint:
    ld a, (su_scratch)
    call su_mark_paint_row
    ld a, (su_scratch)
    cp ROW_BOARD
    jr nz, su_nav_next_mark
    ld a, (_setup_game_focus)
    ld (_setup_focus_board_theme), a
su_nav_next_mark:
    ld a, (_setup_cursor)
    call su_mark_paint_row
    jr su_ret1

su_focus:
    ld a, (_setup_cursor)
    cp 2
    jr nc, su_focus_later
    ld e, a
    ld d, 0
    ld hl, _setup_focus_choice
    add hl, de
    jr su_focus_toggle
su_focus_later:
    cp ROW_TIME
    jr nc, su_focus_option
    ; JOIN/DIRECT exposes IP and PORT as the paired endpoint controls: either
    ; horizontal key traverses the matching opposite control.
    ld hl, (_setup_choice)
    ld a, l
    dec a
    or h
    jp nz, su_ret1
    ld a, (_setup_cursor)
    xor 3
    jp su_nav_go
su_focus_option:
    cp ROW_SIDE
    jr z, su_focus_2
    cp ROW_BOARD
    jr z, su_focus_board
    cp ROW_SET
    jr z, su_focus_set
    cp ROW_HINTS
    jr z, su_focus_3
    jp su_ret1
su_focus_2:
    ld hl, _setup_focus_choice + 2
    jr su_focus_toggle
su_focus_3:
    ld hl, _setup_focus_choice + 3
su_focus_toggle:
    ld a, (hl)
    xor 1
    ld (hl), a
    jr su_focus_paint
su_focus_board:
    ld hl, _setup_focus_board_theme
    ld b, 5
    call su_cycle_choice
    ; Preview focused themes immediately; leaving BOARD restores the committed one.
    ld hl, CTX_FLAGS
    set 3, (hl)
    jr su_focus_paint
su_focus_set:
    ld hl, _setup_focus_choice + 4
    ld b, 3
    call su_cycle_choice
su_focus_paint:
    ld a, (_setup_cursor)
    jp su_mark_paint_row

; A = logical row.  Keep selective rendering and the two cold painters separate.
su_mark_paint_row:
    push af
    call su_row_bit_bc
    ld hl, (CTX_FORCE_LO)
    ld a, l
    or c
    ld l, a
    ld a, h
    or b
    ld h, a
    ld (CTX_FORCE_LO), hl
    pop af
    ld d, a
    cp ROW_TIME
    ld a, FLAG_TIME_UI
    jr z, su_mark_paint
    ld a, d
    cp ROW_ACTION
    ld a, FLAG_ACTION_UI
    jr z, su_mark_paint
    ld a, FLAG_PAINT
su_mark_paint:
    jp su_or_flags

su_cycle_choice:
    ld a, (CTX_KEY)
    cp KEY_LEFT
    jr z, su_cycle_left
    ld a, (hl)
    inc a
    cp b
    jr c, su_cycle_store
    xor a
    jr su_cycle_store
su_cycle_left:
    ld a, (hl)
    dec a
    cp b
    jr c, su_cycle_store
    ld a, b
    dec a
su_cycle_store:
    ld (hl), a
    ret

su_select:
    ld a, (_setup_cursor)
    or a
    jp z, su_select_game
    cp 1
    jp z, su_select_link
    cp 4
    jp c, su_select_endpoint
    cp ROW_SIDE
    jr z, su_select_side
    cp ROW_BOARD
    jr z, su_select_board
    cp ROW_SET
    jr z, su_select_set
    cp ROW_HINTS
    jr z, su_select_hints
    jp su_ret1

su_select_side:
    ld a, (_setup_focus_choice + 2)
    ld (_setup_choice + 2), a
    ld a, ROW_SIDE
su_define_game_row_and_finish:
    call su_set_defined_row
    jp su_after_game_select

su_define_row_and_finish:
    call su_set_defined_row
    jp su_after_select

su_select_board:
    ld a, (_setup_focus_board_theme)
    ld (_setup_game_focus), a
    ld a, ROW_BOARD
    call su_set_defined_row
    jp su_after_game_select

su_select_set:
    ld a, ACTION_SET
    ld (CTX_ACTION), a
    jp su_ret1

su_select_hints:
    ld a, (_setup_focus_choice + 3)
    ld (_setup_choice + 3), a
    ld a, FLAG_ACTION_UI
    call su_or_flags
    ld a, ROW_HINTS
    jp su_define_game_row_and_finish

su_select_endpoint:
    call su_room_editable
    jr z, su_endpoint_define
    call su_begin_room_edit
    ld a, (_setup_cursor)
    call su_mark_paint_row
    ld a, FLAG_SUPPRESS
    jp su_or_flags
su_endpoint_define:
    ld a, (_setup_cursor)
    jp su_define_row_and_finish

su_select_game:
    ld a, (_setup_focus_choice)
    ld hl, _setup_choice
    ld d, 1
    call su_select_primary
    jp z, su_select_unchanged
    ld a, b
    or a
    jp z, su_after_select
    call su_prepare_complete_connection
    jp nz, su_after_select
    ld hl, _setup_defined_mask
    ld a, (hl)
    and 1
    ld (hl), a
    inc hl
    ld (hl), 0
    ld a, 1
    ld (CTX_CLEAR_FROM), a
    jp su_after_select

su_select_link:
    ld a, (_setup_focus_choice + 1)
    ld hl, _setup_choice + 1
    ld d, 2
    call su_select_primary
    jp z, su_select_unchanged
    ld a, b
    or a
    jr z, su_link_auto_room
    call su_prepare_complete_connection
    jr z, su_link_incomplete
    jp su_after_select
su_link_incomplete:
    ld hl, _setup_defined_mask
    ld a, (hl)
    and 3
    ld (hl), a
    inc hl
    ld (hl), 0
    ld a, 2
    ld (CTX_CLEAR_FROM), a
su_link_auto_room:
    ld hl, (_setup_choice)
    ld a, h
    or l
    jp nz, su_after_select
    ld a, 2
    ld (CTX_CLEAR_FROM), a
    jp su_define_row_and_finish

; A = focused choice, D = defined bit, HL = stored choice.
; Returns B = value changed, C = row was undefined, Z only when unchanged.
su_select_primary:
    ld e, a
    ld a, (_setup_defined_mask)
    and d
    ld c, 0
    jr nz, su_sp_had
    inc c
su_sp_had:
    ld a, (hl)
    cp e
    ld b, 0
    jr z, su_sp_same
    inc b
su_sp_same:
    ld (hl), e
    ld a, (_setup_defined_mask)
    or d
    ld (_setup_defined_mask), a
    ld a, b
    or c
    ret z
    push bc
    call su_update_room
    pop bc
    ld a, b
    or c
    ret

su_select_unchanged:
    ld a, (_setup_cursor)
    ld (su_scratch), a
    ld a, 1
    call su_step_row
    ld (_setup_cursor), a
    ld a, (su_scratch)
    call su_mark_paint_row
    ld a, (_setup_cursor)
    call su_mark_paint_row
    ld a, FLAG_SUPPRESS
    jp su_or_flags

; A complete menu remains an editor when GAME/LINK changes. Invalidate only
; the endpoint, then immediately restore it when stored values are valid.
su_prepare_complete_connection:
    ld hl, _setup_defined_mask + 1
    bit 0, (hl)
    ret z
    dec hl
    ld a, (hl)
    and 0xf3
    ld (hl), a
    ld a, SETUP_CLEAR_ENDPOINT
    ld (CTX_CLEAR_FROM), a
    call su_endpoint_valid
    jr z, su_pcc_done
    call su_set_defined_rows_2_3
su_pcc_done:
    or 1
    ret

su_after_select:
    ld a, 1
    jr su_after_select_dirty
su_after_game_select:
    xor a
su_after_select_dirty:
    ld (SU_MARKS_CONFIG), a
    ld bc, (_setup_defined_mask)
    call su_compute_visible
    ld (su_scratch), hl
    ld a, (CTX_CLEAR_FROM)
    cp SETUP_CLEAR_ENDPOINT
    jr nz, su_as_endpoint_progress
    ; Follow the same visibility filter as normal navigation. CREATE/DIRECT
    ; therefore skips the fixed local IP and lands on PORT.
    ld a, 1
    call su_step_row
    jr su_as_store_next
su_as_endpoint_progress:
    cp 2
    jr nz, su_as_clear_visible
su_as_endpoint_next:
    ld a, (_setup_defined_mask)
    and 0x0c
    cp 0x0c
    ld a, 4
    jr z, su_as_store_next
    ld hl, (_setup_choice)
    ld a, l
    or a
    jr nz, su_as_clear_visible
    or h
    jr z, su_as_host_direct
    call su_set_defined_rows_2_3
    ld a, 4
    jr su_as_store_next
su_as_host_direct:
    ld a, 3
    jr su_as_store_next
su_as_clear_visible:
    ld a, (CTX_CLEAR_FROM)
    cp SETUP_CLEAR_NONE
    jr z, su_as_first_new
    push af
    call su_row_bit_bc
    ld hl, (su_scratch)
    ld a, l
    and c
    jr nz, su_as_use_clear_pop
    ld a, h
    and b
    jr nz, su_as_use_clear_pop
    pop af
    jr su_as_first_new
su_as_use_clear_pop:
    pop af
    jr su_as_store_next
su_as_first_new:
    ld hl, (su_scratch)
    ld de, (_setup_visible_mask)
    ld a, e
    cpl
    and l
    ld l, a
    ld a, d
    cpl
    and h
    ld h, a
    call su_first_row
    cp SETUP_CLEAR_NONE
    jr nz, su_as_store_next
    ld a, 1
    call su_step_row
su_as_store_next:
    ld (_setup_cursor), a
    call su_room_editable
    jr z, su_as_flags
    ld a, (hl)
    or a
    call z, su_begin_room_edit
su_as_flags:
    call su_mark_dirty
    ld a, (SU_MARKS_CONFIG)
    or a
    ld a, FLAG_ACTION_UI
    jr z, su_as_ui
    ld a, FLAG_TIME_UI | FLAG_ACTION_UI
su_as_ui:
    call su_or_flags
su_as_not_config:
    ld a, (CTX_CLEAR_FROM)
    call su_write_force_from_row
    ld a, FLAG_RENDER | FLAG_SUPPRESS
    jp su_or_flags

su_mark_dirty:
    ld a, 1
    ld (_setup_config_dirty), a
    xor a
    ld (_setup_action_focus), a
    ret

su_edit_key:
    ld a, (CTX_KEY)
    cp KEY_CANCEL
    jp z, su_edit_cancel
    cp 8
    jr z, su_edit_apply
    cp 13
    jp z, su_edit_confirm
    cp 32
    jp z, su_edit_confirm
    cp KEY_LEFT
    jr c, su_edit_char
    cp KEY_END + 1
    jr c, su_edit_apply
su_edit_char:
    call su_room_char
    or a
    jp z, su_ret1
su_edit_apply:
    ld l, a
    call _spectrum_gui_edit_key
    ld a, l
    or a
    jp z, su_ret1
    ld a, FLAG_EDIT
    jp su_or_flags

su_edit_cancel:
    call _spectrum_gui_edit_hide
    xor a
    ld (_setup_room_editing), a
    call su_restore_edit
    ld a, NOTICE_SELECT
    ld (CTX_NOTICE), a
    ld a, (_setup_edit_row)
    call su_mark_paint_row
    ld a, FLAG_EDIT | FLAG_SUPPRESS
    jp su_or_flags

su_edit_confirm:
    ld hl, (_edit_buf)
    ld a, (hl)
    or a
    jr nz, su_ec_nonempty
    ld a, (_setup_choice + 1)
    or a
    jr nz, su_ec_bad_room
    ld a, (_edit_max)
    cp 5
    jr z, su_ec_bad_port
    jr su_ec_bad_ip
su_ec_nonempty:
    ld a, (_setup_choice + 1)
    or a
    jr z, su_ec_direct
    call su_validate_room
    jr nz, su_ec_not_direct
su_ec_bad_room:
    ld a, NOTICE_BAD_ROOM
    jr su_ec_bad
su_ec_direct:
    ld a, (_edit_max)
    cp 5
    jr z, su_ec_check_port
    call su_validate_ip
    jr z, su_ec_bad_ip
    jr su_ec_not_direct
su_ec_bad_ip:
    ld a, NOTICE_BAD_IP
    jr su_ec_bad
su_ec_check_port:
    call su_parse_port
    jr nz, su_ec_not_direct
su_ec_bad_port:
    ld a, NOTICE_BAD_PORT
su_ec_bad:
    ld (CTX_NOTICE), a
    call su_restore_edit
    call su_begin_room_edit
    jp su_ret1
su_ec_not_direct:
    call _spectrum_gui_edit_hide
    xor a
    ld (_setup_room_editing), a
    ld a, (_setup_edit_row)
    call su_set_defined_row
    ld a, (_setup_choice + 1)
    or a
    jr z, su_ec_after_port
    ld a, 3
    call su_set_defined_row
su_ec_after_port:
    ld bc, (_setup_defined_mask)
    call su_compute_visible
    ld (_setup_visible_mask), hl
    ld a, 1
    call su_step_row
    ld (_setup_cursor), a
    call su_mark_dirty
    ld a, FLAG_TIME_UI | FLAG_ACTION_UI
    call su_or_flags
    ld a, (_setup_edit_row)
    call su_write_row_bit_to_force
    ld a, NOTICE_SELECT
    ld (CTX_NOTICE), a
    ld a, FLAG_RENDER | FLAG_SUPPRESS
    jp su_or_flags

su_update_room:
    ld a, (_setup_choice + 1)
    or a
    ret z
    ld a, (_setup_choice)
    or a
    jr z, su_ur_generate
    call su_validate_room
    ret nz
    ld hl, _netchesszx_mqtt_code
    ld (hl), 'M'
    inc hl
    ld (hl), 'S'
    inc hl
    ld (hl), 0
    ret
su_ur_generate:
    ld de, _netchesszx_mqtt_code
    ld a, 'M'
    ld (de), a
    inc de
    ld a, 'S'
    ld (de), a
    inc de
    ld hl, (0x5c78)
    ld a, h
    or l
    jr nz, su_ur_seed_done
    ld hl, 0x5a3c
su_ur_seed_done:
    ld b, 2
su_ur_byte:
    ld a, h
    rrca
    rrca
    rrca
    rrca
    call su_ur_store_hex
    ld a, h
    call su_ur_store_hex
    ld h, l
    djnz su_ur_byte
    xor a
    ld (de), a
    ret
su_ur_store_hex:
    and 0x0f
    add a, '0'
    cp '9' + 1
    jr c, su_hex_store
    add a, 7
su_hex_store:
    ld (de), a
    inc de
    ret

su_begin_room_edit:
    ld a, (_setup_cursor)
    ld (_setup_edit_row), a
    call su_backup_edit
    ld a, 1
    ld (_setup_room_editing), a
    ld a, (_setup_choice + 1)
    or a
    jr nz, su_bre_mqtt
    ld a, (_setup_cursor)
    cp 3
    jr nz, su_bre_direct_ip
su_bre_direct_port:
    ld a, 5
    ld hl, _setup_port_text
    jr su_bre_bind
su_bre_direct_ip:
    ld a, 15
    ld hl, _netchesszx_direct_host
    jr su_bre_bind
su_bre_mqtt:
    ld a, 4
    ld hl, _netchesszx_mqtt_code + 2
su_bre_bind:
    ld (_edit_max), a
    call _spectrum_gui_edit_bind
    ; Explicit selection adds PAINT; auto-entry already performs full RENDER.
    ld a, FLAG_EDIT
    ld hl, CTX_FLAGS
    or (hl)
    ld (hl), a
    ret

su_room_editable:
    ld a, (_setup_cursor)
    cp 3
    jr z, su_re_port
    sub 2
    jr nz, su_re_false
    ld hl, _netchesszx_mqtt_code + 2
    ld a, (_setup_choice + 1)
    or a
    ret nz
    ld hl, _netchesszx_direct_host
    ld a, (_setup_choice)
    or a
    ret
su_re_port:
    ld hl, _setup_port_text
    ld a, (_setup_choice + 1)
    or a
    jr nz, su_re_false
    inc a
    ret
su_re_false:
    xor a
    ret

su_endpoint_valid:
    ld a, (_setup_choice + 1)
    or a
    jp nz, su_validate_room
    ld hl, (_netchesszx_direct_port)
    ld a, h
    or l
    ret z
    ld a, (_setup_choice)
    or a
    jp nz, su_validate_ip
    inc a
    ret

su_room_char:
    cp '0'
    jr c, su_rc_not_digit
    cp '9' + 1
    ret c
su_rc_not_digit:
    ld c, a
    ld a, (_setup_choice + 1)
    or a
    jr nz, su_rc_mqtt
    ld a, (_edit_max)
    cp 5
    jr z, su_rc_bad
    ld a, c
    and 0xfd
    cp ','
    jr z, su_rc_dot
    ld a, c
    sub ':'
    cp 2
    jr c, su_rc_dot
su_rc_bad:
    xor a
    ret
su_rc_dot:
    ld a, '.'
    ret
su_rc_mqtt:
    ld a, c
    and 0xdf
    cp 'A'
    jr c, su_rc_bad
    cp 'F' + 1
    jr nc, su_rc_bad
    ret

; Exactly MS followed by four hexadecimal digits and NUL.
su_validate_room:
    ld hl, _netchesszx_mqtt_code
    ld a, (hl)
    cp 'M'
    jr nz, su_vr_fail
    inc hl
    ld a, (hl)
    cp 'S'
    jr nz, su_vr_fail
    inc hl
    ld b, 4
su_vr_hex:
    ld a, (hl)
    cp '0'
    jr c, su_vr_fail
    cp '9' + 1
    jr c, su_vr_next
    cp 'A'
    jr c, su_vr_fail
    cp 'F' + 1
    jr nc, su_vr_fail
su_vr_next:
    inc hl
    djnz su_vr_hex
    ld a, (hl)
    or a
    jr nz, su_vr_fail
    inc a
    ret
su_vr_fail:
    xor a
    ret

su_compute_visible:
    ld de, 1
    bit 0, c
    jr z, su_cv_done
    set 1, e
    bit 1, c
    jr z, su_cv_done
    set 2, e
    set 3, e
su_cv_after_link:
    bit 0, b
    jr z, su_cv_progressive
    ld de, 0x01df
    ld a, (_setup_choice)
    or a
    jr nz, su_cv_check_action
    set 5, e
    jr su_cv_check_action
su_cv_progressive:
su_cv_after_room:
    bit 3, c
    jr z, su_cv_done
    set 4, e
    bit 4, c
    jr z, su_cv_done
    ld a, (_setup_choice)
    or a
    jr z, su_cv_host_side
    set 6, e
    jr su_cv_after_side
su_cv_host_side:
    set 5, e
    bit 5, c
    jr z, su_cv_done
    set 6, e
su_cv_after_side:
    bit 6, c
    jr z, su_cv_done
    set 7, e
    bit 7, c
    jr z, su_cv_done
    set 0, d
su_cv_check_action:
    bit 0, b
    jr z, su_cv_done
    ld hl, (_setup_choice)
    ld a, l
    or a
    jr nz, su_cv_join_ready
    or h
    jr z, su_cv_host_direct_ready
    ld a, c
    cp 0xff
    jr z, su_cv_action
    jr su_cv_done
su_cv_host_direct_ready:
    ld a, c
    and 0xfb
    cp 0xfb
    jr z, su_cv_action
    jr su_cv_done
su_cv_join_ready:
    ld a, c
    and 0xdf
    cp 0xdf
    jr nz, su_cv_done
    ld a, (_setup_choice + 1)
    or a
    jr nz, su_cv_action
    ld a, (_netchesszx_direct_host)
    or a
    jr z, su_cv_done
su_cv_action:
    set 1, d
su_cv_done:
    ld h, d
    ld l, e
    ret

su_step_row:
    or a
    jr z, su_step_prev
    ld a, (_setup_cursor)
    inc a
su_step_next_loop:
    cp 10
    jr nc, su_step_restore
    call su_visible_a
    ld a, e
    ret nz
    inc a
    jr su_step_next_loop
su_step_prev:
    ld a, (_setup_cursor)
    or a
    jr z, su_step_restore
    dec a
su_step_prev_loop:
    call su_visible_a
    ld a, e
    ret nz
    or a
    jr z, su_step_restore
    dec a
    jr su_step_prev_loop
su_step_restore:
    ld a, (_setup_cursor)
    ret

su_visible_a:
    ld e, a
su_va_mask:
    ld a, e
    cp 2
    jr nz, su_va_check_port
    ld a, (_setup_choice + 1)
    or a
    jr nz, su_va_bit_restore
    ld a, (_setup_choice)
    or a
    jp z, su_re_false
su_va_bit_restore:
    ld a, e
su_va_check_port:
    cp 3
    jr nz, su_va_bit
    ld a, (_setup_choice + 1)
    or a
    jp nz, su_re_false
    ld a, e
su_va_bit:
    call su_row_bit_bc
    ld hl, (_setup_visible_mask)
    jr su_mask_has_bc

su_first_row:
    xor a
su_fr_loop:
    cp 10
    jr nc, su_fr_none
    ld e, a
    call su_row_bit_bc
    call su_mask_has_bc
    ld a, e
    ret nz
    inc a
    jr su_fr_loop
su_fr_none:
    ld a, SETUP_CLEAR_NONE
    ret

su_mask_has_bc:
    ld a, l
    and c
    ret nz
    ld a, h
    and b
    ret

su_row_bit_bc:
    ld bc, 1
    or a
    ret z
su_rbbc_shift:
    sla c
    rl b
    dec a
    jr nz, su_rbbc_shift
    ret

su_edit_target_hl_bc:
    ld a, (_setup_choice + 1)
    or a
    jr nz, su_edit_target_mqtt
    ld a, (_setup_edit_row)
    cp 3
    ld hl, _netchesszx_direct_host
    ld bc, 16
    ret nz
    ld hl, _setup_port_text
    ld bc, 6
    ret
su_edit_target_mqtt:
    ld hl, _netchesszx_mqtt_code
    ld bc, 17
    ret

su_backup_edit:
    call su_edit_target_hl_bc
    ld de, _setup_edit_backup
    ldir
    ret

su_restore_edit:
    call su_edit_target_hl_bc
    ex de, hl
    ld hl, _setup_edit_backup
    ldir
    ld a, (_setup_edit_row)
    cp 3
    ret nz
    jp su_parse_port

su_set_defined_rows_2_3:
    ld hl, _setup_defined_mask
    set 2, (hl)
    set 3, (hl)
    ret

su_set_defined_row:
    call su_row_bit_bc
    ld hl, _setup_defined_mask
    ld a, (hl)
    or c
    ld (hl), a
    inc hl
    ld a, (hl)
    or b
    ld (hl), a
    ret

su_write_force_from_row:
    cp SETUP_CLEAR_ENDPOINT
    jr nz, su_wff_not_endpoint
    ld bc, 4
    ld a, (_setup_visible_mask)
    cpl
    ld hl, su_scratch
    and (hl)
    and 0x20
    or c
    ld c, a
    jr su_wff_store
su_wff_not_endpoint:
    cp SETUP_CLEAR_NONE
    ret z
su_wff_row:
    call su_row_bit_bc
    dec bc
    ld a, c
    cpl
    ld c, a
    ld a, b
    cpl
    ld b, a
su_wff_store:
    ld (CTX_FORCE_LO), bc
    ret

su_write_row_bit_to_force:
    call su_row_bit_bc
    jr su_wff_store

su_validate_ip:
    ld hl, _netchesszx_direct_host
    ld b, 0
su_ip_segment:
    ld c, 0
    ld e, 0
su_ip_digit_loop:
    ld a, (hl)
    cp '0'
    jr c, su_ip_end_digits
    cp '9' + 1
    jr nc, su_ip_end_digits
    sub '0'
    ld (su_scratch), a
    inc c
    ld a, c
    cp 4
    jr nc, su_ip_fail
    ld a, e
    add a, a
    jr c, su_ip_fail
    ld d, a
    add a, a
    jr c, su_ip_fail
    add a, a
    jr c, su_ip_fail
    add a, d
    jr c, su_ip_fail
    ld d, a
    ld a, (su_scratch)
    add a, d
    jr c, su_ip_fail
    ld e, a
    inc hl
    jr su_ip_digit_loop
su_ip_end_digits:
    ld a, c
    or a
    jr z, su_ip_fail
    ld a, (hl)
    or a
    jr z, su_ip_end
    cp '.'
    jr nz, su_ip_fail
    ld a, b
    cp 3
    jr nc, su_ip_fail
    inc b
    inc hl
    jr su_ip_segment
su_ip_end:
    ld a, b
    cp 3
    jr nz, su_ip_fail
    inc a
    ret
su_ip_fail:
    xor a
    ret

su_parse_port:
    ld hl, su_scratch
    push hl
    ld hl, _setup_port_text
    push hl
    call _netchess_mqtt_session_parse_u16_token
    pop de
    pop de
    ld a, h
    or l
    ret z
    ld a, (hl)
    or a
    jr nz, su_pp_fail
    ld hl, (su_scratch)
    ld a, h
    or l
    ret z
    ld (_netchesszx_direct_port), hl
    ret
su_pp_fail:
    xor a
    ret

su_write_port_text:
    ld hl, (_netchesszx_direct_port)
    push hl
    ld hl, _setup_port_text
    push hl
    call _spectrum_append_u16
    ret


; Mutually exclusive visible-mask, IP-digit and parsed-port scratch.
su_scratch: DEFW 0
