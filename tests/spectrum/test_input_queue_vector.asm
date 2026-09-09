SECTION code_user

PUBLIC test_start

EXTERN spectrum_input_queue_clear
EXTERN spectrum_input_queue_latch
EXTERN _spectrum_key_poll

TEST_RESULT EQU 0x7000

test_start:
    ld sp, 0xff00
    xor a
    ld (TEST_RESULT), a
    call spectrum_input_queue_clear

    ld a, 'a'
    call spectrum_input_queue_latch
    ld a, 'b'
    call spectrum_input_queue_latch
    ld a, 'a'
    call test_expect_a
    jr nz, test_done
    ld a, 'b'
    call test_expect_a
    jr nz, test_done
    xor a
    call test_expect_a
    jr nz, test_done

    ld a, 'a'
    call spectrum_input_queue_latch
    ld a, 'b'
    call spectrum_input_queue_latch
    ld a, 'c'
    call spectrum_input_queue_latch
    ld a, 'a'
    call test_expect_a
    jr nz, test_done
    ld a, 'b'
    call test_expect_a
    jr nz, test_done
    xor a
    call test_expect_a
    jr nz, test_done

    ld a, 'a'
    call spectrum_input_queue_latch
    ld a, 'b'
    call spectrum_input_queue_latch
    call spectrum_input_queue_clear
    xor a
    call test_expect_a
    jr nz, test_done

    ld a, 'a'
    call spectrum_input_queue_latch
    ld a, 0x8a
    call spectrum_input_queue_latch
    ld a, 'b'
    call spectrum_input_queue_latch
    ld a, 0x8a
    call test_expect_a
    jr nz, test_done
    xor a
    call test_expect_a
    jr nz, test_done

    ld a, 'a'
    call spectrum_input_queue_latch
    ld a, 0x90
    call spectrum_input_queue_latch
    ld a, 'b'
    call spectrum_input_queue_latch
    ld a, 0x90
    call test_expect_a
    jr nz, test_done
    xor a
    call test_expect_a

test_done:
    ld (TEST_RESULT), a
    jp 0

; A = expected queued event. Returns NZ on mismatch.
test_expect_a:
    ld b, a
    call _spectrum_key_poll
    ld a, l
    cp b
    ret z
    ld a, 1
    or a
    ret
