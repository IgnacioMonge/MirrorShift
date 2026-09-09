"""Run production multi-piece shimmer: Z80 bitplanes and Next palette lifecycle."""
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools import build_checker_sets
from tests.tools.test_board_animation import classic_piece_sets

REFLECTION_BYTES = build_checker_sets.REFLECTION_SET_BYTES


def run():
    work = ROOT / 'build/piece-shine'
    work.mkdir(parents=True, exist_ok=True)
    gui = (ROOT / 'src/spectrum/ui/gui.c').read_text()
    code = gui.split('static void piece_shine_reset_wait(void)\n{', 1)[1].split('\n#endif', 1)[0]
    header = '''#include <stdint.h>
#include <assert.h>
#include <string.h>
#define MS_SIDE_A 0
#define MS_CELL_A 'A'
#define MS_CELL_B 'B'
#define SPECTRUM_NEXT_PIECE_PALETTE_NORMAL 0
#define SPECTRUM_NEXT_PIECE_PALETTE_ENTRY 4
#define SPECTRUM_NEXT_PIECE_PALETTE_EXIT 12
static uint8_t piece_shine_targets[3], piece_shine_count, piece_shine_wait, piece_shine_frames;
static uint8_t board_pieces_visible=1, about_visible, active_coord_valid, active_coord_square;
static uint8_t game_timer_active=1, menu_visible, clock_frames, side, over, entropy, flipped;
static char gui_live_board[64];
static unsigned calls[16];
static uint8_t palettes[64];
static uint8_t piece_shine_entropy(void) {return entropy;}
static uint8_t ms_rules_side(void) {return side;}
static uint8_t ms_rules_is_over(void) {return over;}
static uint8_t display_row(uint8_t r) {return flipped?r:7-r;}
static uint8_t display_col(uint8_t c) {return flipped?7-c:c;}
static void spectrum_render_piece_palette(char *s) {
 unsigned sq=(flipped?s[0]:7-s[0])*8+(flipped?7-s[1]:s[1]);
 palettes[sq]=s[2]; calls[(unsigned)s[2]]++;
}
static void piece_shine_reset_wait(void) {
'''
    checks = '''
int main(void) {
 for (flipped=0;flipped<2;flipped++) for (side=0;side<2;side++)
 for (unsigned n=0;n<=5;n++) for (entropy=0;entropy<64;entropy++) {
  memset(gui_live_board,'.',64); memset(calls,0,sizeof calls); memset(palettes,0,64);
  for(unsigned j=0;j<n;j++) gui_live_board[j*13]='A'+side;
  gui_live_board[3]='A'+(side^1);
  active_coord_valid=1; active_coord_square=0;
  unsigned want=n>0?n-1:0; if(want>3)want=3;
  piece_shine_wait=1; piece_shine_tick();
  assert(piece_shine_count==want && calls[4]==want);
  for(unsigned j=0;j<3;j++)piece_shine_tick();
  assert(calls[12]==0);
  piece_shine_tick(); assert(calls[12]==want);
  for(unsigned j=0;j<4;j++)piece_shine_tick();
  assert(piece_shine_count==0 && calls[0]==want);
  assert(palettes[0]==0 && palettes[3]==0);
  for(unsigned j=0;j<64;j++)assert(palettes[j]==0);
  for(unsigned reason=0;reason<3;reason++) {
   piece_shine_wait=1; piece_shine_tick();
   if(reason==0)menu_visible=1;
   if(reason==1)side^=1;
   if(reason==2)over=1;
   piece_shine_tick(); assert(piece_shine_count==0);
   for(unsigned j=0;j<64;j++)assert(palettes[j]==0);
   menu_visible=over=0; if(reason==1)side^=1;
  }
 }
 return 0;
}
'''
    (work/'next.c').write_text(header+code+checks)
    subprocess.run([shutil.which('gcc'), '-std=c99', '-Wall', '-Werror', str(work/'next.c'), '-o', str(work/'next.exe')],check=True)
    subprocess.run([str(work/'next.exe')],check=True)
    stub = (ROOT/'tests/spectrum/test_board_animation_vector.asm').read_text()
    prefix = stub.split('test_start:',1)[0]+'EXTERN _board_piece_reflection_ovl_entry\n'
    tail = 'test_apply:'+stub.split('test_apply:',1)[1]
    tail = tail.replace('_ms_rules_side:\n_ms_rules_is_over:', '_ms_rules_side:\n    ld a, (test_side)\n    ld l, a\n    ret\n_ms_rules_is_over:')
    tail += '\ntest_side: defb 0\n'
    sets = classic_piece_sets(ROOT)
    reflections = (ROOT/'assets/spectrum/checker_reflections.bin').read_bytes()
    for set_index in range(3):
      for flipped in (0,1):
       for side in (0,1):
        for count in (0,1,2,3,4):
          board = bytearray(b'.'*64)
          for sq in (0,13,26,39)[:count]: board[sq]=65+side
          board[3]=65+(side^1)
          # Cursor excludes logical zero; remaining targets cover both parities.
          targets = list((0,13,26,39)[:count])[1:]
          init = f'''test_start:
    ld sp, 0xff00
    di
    ld hl, 0xa000
    ld (log_ptr), hl
    ld hl, board_data
    ld de, 0x5f60
    ld bc, 64
    ldir
    ld hl, mask_data
    ld de, 0x662b
    ld bc, 96
    ldir
    xor a
    ld (0x5c78), a
    ld (0x5c79), a
    ld (tmp_char), a
    ld a, {set_index}
    ld (_netchesszx_piece_set_index), a
    ld a, {flipped}
    ld (_spectrum_gui_board_flipped), a
    ld a, {side}
    ld (test_side), a
    ld a, {0 if flipped else 7}
    ld (_spectrum_gui_active_coord_row), a
    ld a, {7 if flipped else 0}
    ld (_spectrum_gui_active_coord_col), a
    ld a, 1
    ld (_spectrum_gui_active_coord_valid), a
    ld (morph_mode), a
    call test_reflect
    ld hl, (log_ptr)
    ld (0x7002), hl
    jp 0
test_reflect:
    ld ix, 0x1234
    ld iy, 0x5c3a
    call _board_piece_reflection_ovl_entry
    jp test_registers
board_data: defb {','.join(map(str,board))}
mask_data: defb {','.join(map(str,sets[set_index]+reflections[set_index*REFLECTION_BYTES:(set_index+1)*REFLECTION_BYTES]))}
'''
          (work/'vector.asm').write_text(prefix+init+tail)
          for defines in ([], ['-DNETCHESSZX_SPECTRANEXT']):
            subprocess.run([shutil.which('z80asm'),'-b',*defines,'-r0x8000','-O=.','-o=shine.bin', 'vector.asm',str(ROOT/'asm/overlay/board/entry_board.asm'),str(ROOT/'assets/spectrum/checker_pieces_16x16.asm')],cwd=work,check=True,capture_output=True)
            subprocess.run([shutil.which('z88dk-ticks'),'-mz80','-l','0x8000','-pc','8000','-end','0','-counter','10000000','-output','shine.ram','shine.bin'],cwd=work,check=True,capture_output=True)
            ram=(work/'shine.ram').read_bytes(); end=int.from_bytes(ram[0x7002:0x7004],'little')
            expected=bytearray()
            deltas=build_checker_sets.unpack_reflection(reflections[set_index*REFLECTION_BYTES:(set_index+1)*REFLECTION_BYTES],set_index)
            for delta in [*deltas, set()]:
              for sq in targets:
                row,col=(sq//8,7-sq%8) if flipped else (7-sq//8,sq%8)
                physical=side^((row+col)&1)^1
                mask=bytearray(sets[set_index][physical*32:(physical+1)*32])
                for x,y in delta:
                  bit=1<<(7-x%8); offset=y*2+x//8
                  if physical:mask[offset]&=255^bit
                  else:mask[offset]|=bit
                expected+=b'F'+bytes((0,row,col,65+side))+mask
            # Interleave exactly one UART-serviced wait after each of seven frames.
            if targets:
              stride=37*len(targets)
              expected=b''.join(expected[i*stride:(i+1)*stride]+(b'DUTU' if i<7 else b'') for i in range(8))
            assert ram[0x7000]==0, 'IX/IY/SP'
            actual=ram[0xa000:end]
            assert actual==expected,(set_index,flipped,side,count,len(actual),len(expected),next(((i,a,b)for i,(a,b)in enumerate(zip(actual,expected))if a!=b),None))
    print('[OK] Next palette lifecycle and 120 Z80 reflection cases (Classic/SpectraNext): three sets, sides, orientations, 0..3 targets, exact bitplanes/restoration/cadence, IX/IY/SP')

if __name__ == '__main__':
    run()
