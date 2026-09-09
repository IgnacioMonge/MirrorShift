#ifndef MIRRORSHIFT_SPECTRANEXT_LOWRAM_H
#define MIRRORSHIFT_SPECTRANEXT_LOWRAM_H

#include "spectrum/lowram_map.h"

/* Cartridge XFS compatibility state in the reclaimed printer buffer. */
/* Leave room for CRT BSS_UNINITIALIZED after bss_user (saved BASIC SP). */
#define NETCHESSZX_LOWRAM_XFS_STATE_ADDR 0x5b60
#define NETCHESSZX_LOWRAM_XFS_STATE_END 0x5b74
#define NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_ADDR 0x662b
#define NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_SIZE 256u

/* DIRECT keeps one unparsed cartridge recv between cold-overlay calls. */
#define NETCHESSZX_LOWRAM_DIRECT_PENDING_CURSOR_ADDR 0x636d
#define NETCHESSZX_LOWRAM_DIRECT_PENDING_LENGTH_ADDR 0x636e
#define NETCHESSZX_LOWRAM_DIRECT_STAGE_ADDR 0x636f
#define NETCHESSZX_LOWRAM_DIRECT_STAGE_SIZE 160u

#if NETCHESSZX_LOWRAM_DIRECT_STAGE_ADDR != \
    (NETCHESSZX_LOWRAM_DIRECT_PENDING_LENGTH_ADDR + 1u)
#error "DIRECT stage must follow its pending state"
#endif
#if (NETCHESSZX_LOWRAM_DIRECT_STAGE_ADDR + \
     NETCHESSZX_LOWRAM_DIRECT_STAGE_SIZE) > 0x658bu
#error "DIRECT stage overlaps MQTT packet/payload scratch"
#endif

/* XFS directory scans alias BOARD's live masks and reflection stream.  The
   generic adapter preserves the complete active bundle around READDIR and
   freplace parent scans. */
#define NETCHESSZX_LOWRAM_PIECE_MASKS_BACKUP_ADDR 0x676bu
#define NETCHESSZX_LOWRAM_PIECE_MASKS_BACKUP_SIZE 96u

#if NETCHESSZX_LOWRAM_PIECE_MASKS_BACKUP_SIZE != \
        NETCHESSZX_LOWRAM_PIECE_BUNDLE_SIZE
#error "XFS backup must preserve the complete piece bundle"
#endif

#if NETCHESSZX_LOWRAM_XFS_STATE_END > 0x5c00
#error "low-RAM XFS state overlaps ROM sysvars"
#endif
#if NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_ADDR != 0x662b
#error "XFS directory scratch must preserve the Mirror Reversi piece bundle"
#endif
#if NETCHESSZX_LOWRAM_PIECE_MASKS_BACKUP_ADDR != 0x676bu
#error "XFS piece bundle backup must remain outside directory scratch"
#endif
#if (NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_ADDR + \
     NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_SIZE) != \
    NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR
#error "XFS directory scratch must end at overlay scratch"
#endif
#if (NETCHESSZX_LOWRAM_PIECE_MASKS_BACKUP_ADDR + \
     NETCHESSZX_LOWRAM_PIECE_MASKS_BACKUP_SIZE) > \
    NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_END
#error "XFS mask backup exceeds overlay scratch"
#endif

#endif
