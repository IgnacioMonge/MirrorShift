set_square_attr_2x2:                  call next_extension_restore
                                      jp next_ext_set_square_attr_2x2
_spectrum_next_sprites_hide_all:     call next_extension_restore
                                     jp next_ext_sprites_hide_all
draw_char64_pixels_at_tmp:           call next_extension_restore
                                     jp next_ext_draw_char64
_spectrum_key_edit_pressed:          call next_extension_restore
                                     jp next_ext_key_edit_pressed
_spectrum_input_flush_until_release: call next_extension_restore
                                     jp next_ext_input_flush
_spectrum_input_suppress_until_release: call next_extension_restore
                                     jp next_ext_input_suppress
_spectrum_input_frame_tick:          call next_extension_restore
                                     jp next_ext_input_frame_tick
_netchesszx_board_theme_apply:       call next_extension_restore
                                     jp next_ext_board_theme_apply
compute_screen_base:                 call next_extension_restore
                                     jp next_ext_compute_screen_base
compute_attr_base:                   call next_extension_restore
                                     jp next_ext_compute_attr_base
draw_one_board_square:               call next_extension_restore
                                     jp next_ext_draw_one_board_square
next_marker_set_hint_current_square: call next_extension_restore
                                     jp next_ext_marker_set_hint
next_marker_set_mark_current_square: call next_extension_restore
                                     jp next_ext_marker_set_mark
draw_square_mark:                    call next_extension_restore
                                     jp next_ext_draw_square_mark
next_draw_legal_hints:               call next_extension_restore
                                     jp next_ext_draw_legal_hints
clear_right_pixel_band_abs:          call next_extension_restore
                                     jp next_ext_clear_right_pixel_band
draw_ikkle_text_abs_y:               call next_extension_restore
                                     jp next_ext_draw_ikkle_text_abs_y
compute_pixel_base:                  call next_extension_restore
                                     jp next_ext_compute_pixel_base
pixel_down_hl:                       call next_extension_restore
                                     jp next_ext_pixel_down_hl
clear_text_row:                      call next_extension_restore
                                     jp next_ext_clear_text_row
draw_text64_line_attr_fast:          call next_extension_restore
                                     jp next_ext_draw_text64_line_attr
draw_char64_at_tmp:                  call next_extension_restore
                                     jp next_ext_draw_char64_at_tmp
draw_input_cursor_at_tmp:            call next_extension_restore
                                     jp next_ext_draw_input_cursor

_spectrum_render_piece_palette: call next_extension_restore
    jp next_ext_render_piece_palette
_spectrum_next_setup_pieces_use_final: call next_extension_restore
    jp next_ext_setup_pieces_use_final
next_square_sprite_slot: call next_extension_restore
    jp next_ext_square_sprite_slot
next_hide_hardware_sprite: call next_extension_restore
    jp next_ext_hide_hardware_sprite
next_draw_piece_sprite_16x16: call next_extension_restore
    jp next_ext_draw_piece_sprite
_spectrum_flip_blit_frame: call next_extension_restore
    jp next_ext_flip_blit_frame
_spectrum_animate_last_flip: call next_extension_restore
    jp next_ext_animate_last_flip
_spectrum_animate_set_morph: call next_extension_restore
    jp next_ext_animate_set_morph
_spectrum_animate_convergence: call next_extension_restore
    jp next_ext_animate_convergence
_spectrum_piece_masks_stash: call next_extension_restore
    jp next_ext_piece_masks_stash
screen_cell_for_tmpcol: call next_extension_restore
    jp next_ext_screen_cell_for_tmpcol
