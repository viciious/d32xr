
        .text

| Standard MegaCD Sub-CPU Program Header (copied to 0x6000)

SPHeader:
        .asciz  "MAIN-SUBCPU"
        .word   0x0001,0x0000
        .long   0x00000000
        .long   0x00000000
        .long   SPHeaderOffsets-SPHeader
        .long   0x00000000

SPHeaderOffsets:
        .word   SPInit-SPHeaderOffsets
        .word   SPMain-SPHeaderOffsets
        .word   SPInt2-SPHeaderOffsets
        .word   SPNull-SPHeaderOffsets
        .word   0x0000

| Sub-CPU Program Initialization (VBlank not enabled yet)

SPInit:
        move.b  #'I,0x800F.w            /* sub comm port = INITIALIZING */
        andi.b  #0xE2,0x8003.w          /* Priority Mode = off, 2M mode, Sub-CPU has access */
        bset    #2,0x8003.w             /* switch to 1M mode */

        jsr     0x5F3A.w                /* SPNull - returns boot loader globals */
        move.l  d0,global_vars

        rts

| Sub-CPU Program Main Entry Point (VBlank now enabled)

SPMain:
        move.w  #0x0081,d0              /* CDBSTAT */
        jsr     0x5F22.w                /* call CDBIOS function */
        move.w  0(a0),d0                /* BIOS status word */
        bmi.b   1f                      /* not ready */
        lsr.w   #8,d0
        cmpi.b  #0x40,d0
        beq.b   9f                      /* open */
        cmpi.b  #0x10,d0
        beq.b   9f                      /* no disc */
1:
| Initialize Drive
        lea     drive_init_parms(pc),a0
        move.w  #0x0010,d0              /* DRVINIT */
        jsr     0x5F22.w                /* call CDBIOS function */

        move.w  #0x0085,d0              /* BIOS_FDRSET - set audio volume */
        move.w  #0x8400.w,d1            /* master volume (0 to 1024) */
        jsr     0x5F22.w                /* call CDBIOS function */

        move.w  #0x0089,d0              /* CDCSTOP - stop reading data */
        jsr     0x5F22.w                /* call CDBIOS function */
9:
        move.b  #0,0x800F.w             /* sub comm port = READY */

| wait for command in main comm port
WaitCmd:
        tst.b   updates_suspend
        bne     WaitCmdPostUpdate

        jsr     S_Update
        
WaitCmdPostUpdate:
        tst.b   0x800E.w
        beq.b   WaitCmd
        cmpi.b  #'D,0x800E.w
        beq     GetDiscInfo
        cmpi.b  #'T,0x800E.w
        beq     GetTrackInfo
        cmpi.b  #'P,0x800E.w
        beq     PlayTrack
        cmpi.b  #'S,0x800E.w
        beq     StopPlaying
        cmpi.b  #'V,0x800E.w
        beq     SetVolume
        cmpi.b  #'Z,0x800E.w
        beq     PauseResume
        cmpi.b  #'C,0x800E.w
        beq     CheckDisc

        cmpi.b  #'I,0x800E.w
        beq     SfxInit
        cmpi.b  #'L,0x800E.w
        beq     SfxClear
        cmpi.b  #'B,0x800E.w
        beq     SfxCopyBuffer
        cmpi.b  #'A,0x800E.w
        beq     SfxPlaySource
        cmpi.b  #'N,0x800E.w
        beq     SfxPUnPSource
        cmpi.b  #'W,0x800E.w
        beq     SfxRewindSource
        cmpi.b  #'U,0x800E.w
        beq     SfxUpdateSource
        cmpi.b  #'O,0x800E.w
        beq     SfxStopSource
        cmpi.b  #'G,0x800E.w
        beq     SfxGetSourcePosition
        cmpi.b  #'E,0x800E.w
        beq     SfxSuspendUpdates

        move.b  #'E,0x800F.w            /* sub comm port = ERROR */
WaitAck:
        tst.b   0x800E.w
        bne.b   WaitAck                 /* wait for result acknowledged */
        move.b  #0,0x800F.w             /* sub comm port = READY */
        bra.w   WaitCmd

GetDiscInfo:
        move.w  #0x0081,d0              /* CDBSTAT */
        jsr     0x5F22.w                /* call CDBIOS function */
        move.w  0(a0),0x8020.w          /* BIOS status word */
        move.w  16(a0),0x8022.w         /* First song number, Last song number */
        move.w  18(a0),0x8024.w         /* Drive version, Flag */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

GetTrackInfo:
        move.w  0x8010.w,d1             /* track number */
        move.w  #0x0083,d0              /* CDBTOCREAD */
        jsr     0x5F22.w                /* call CDBIOS function */
        move.l  d0,0x8020.w             /* MMSSFFTN */
        move.b  d1,0x8024.w             /* track type */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

PlayTrack:
        move.w  #0x0002,d0              /* MSCSTOP - stop playing */
        jsr     0x5F22.w                /* call CDBIOS function */

        move.w  0x8010.w,d1             /* track number */
        move.w  #0x0011,d0              /* MSCPLAY - play from track on */
        move.b  0x8012.w,d2             /* flag */
        bmi.b   2f
        beq.b   1f
        move.w  #0x0013,d0              /* MSCPLAYR - play with repeat */
        bra.b   2f
1:
        move.w  #0x0012,d0              /* MSCPLAY1 - play once */
2:
        lea     track_number(pc),a0
        move.w  d1,(a0)
        jsr     0x5F22.w                /* call CDBIOS function */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

StopPlaying:
        move.w  #0x0002,d0              /* MSCSTOP - stop playing */
        jsr     0x5F22.w                /* call CDBIOS function */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

SetVolume:
        move.w  #0x0085,d0              /* BIOS_FDRSET - set audio volume */
        move.w  0x8010.w,d1             /* cd volume (0 to 1024) */
        jsr     0x5F22.w                /* call CDBIOS function */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

PauseResume:
        move.w  #0x0081,d0              /* CDBSTAT */
        jsr     0x5F22.w                /* call CDBIOS function */
        move.b  (a0),d0
        cmpi.b  #1,d0
        beq.b   1f                      /* playing - pause playback */
        cmpi.b  #5,d0
        beq.b   2f                      /* paused - resume playback */

        move.b  #'E,0x800F.w            /* sub comm port = ERROR */
        bra     WaitAck
1:
        move.w  #0x0003,d0              /* MSCPAUSEON - pause playback */
        jsr     0x5F22.w                /* call CDBIOS function */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck
2:
        move.w  #0x0004,d0              /* MSCPAUSEOFF - resume playback */
        jsr     0x5F22.w                /* call CDBIOS function */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

CheckDisc:
        lea     drive_init_parms(pc),a0
        move.w  #0x0010,d0              /* DRVINIT */
        jsr     0x5F22.w                /* call CDBIOS function */

        move.w  #0x0089,d0              /* CDCSTOP - stop reading data */
        jsr     0x5F22.w                /* call CDBIOS function */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

SfxInit:
        jsr     S_Init
        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

SfxClear:
        jsr     S_Clear
        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

SfxCopyBuffer:
        jsr     switch_banks
        move.l  0x8018.w,d0             /* length */
        move.l  d0,-(sp)
        move.l  0x8014.w,d0             /* address in RAM */
        move.l  d0,-(sp)
        moveq   #0,d0
        move.w  0x8010.w,d0             /* buffer id */
        move.l  d0,-(sp)
        move.b  #'D,0x800F.w            /* sub comm port = DONE */
SfxCopyBufferWaitAck:
        tst.b   0x800E.w
        bne.b   SfxCopyBufferWaitAck    /* wait for result acknowledged */
        move.b  #0,0x800F.w             /* sub comm port = READY */
        jsr     S_CopyBufferData        /* copy the buffer data in the background */
        lea     12(sp),sp               /* clear the stack */
        bra.w   WaitCmd

SfxPlaySource:
| uint8_t S_PlaySource(uint8_t src_id, uint16_t buf_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop);
        moveq   #0,d0

        move.b  0x801b.w,d0
        move.l  d0,-(sp)                /* autoloop */
        move.b  0x8019.w,d0
        move.l  d0,-(sp)                /* vol */

        move.b  0x8017.w,d0
        move.l  d0,-(sp)                /* pan */
        move.w  0x8014.w,d0
        move.l  d0,-(sp)                /* freq */

        move.w  0x8012.w,d0
        move.l  d0,-(sp)                /* buf_id */
        move.w  0x8010.w,d0
        move.l  d0,-(sp)                /* src_id */

        jsr     S_PlaySource
        lea     24(sp),sp               /* clear the stack */

        move.b  d0,0x8020.w             /* src_id */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

SfxPUnPSource:
| void S_PUnPSource(uint8_t src_id, uint8_t pause);
        moveq   #0,d0

        move.b  0x8013.w,d0
        move.l  d0,-(sp)                /* paused */
        move.w  0x8010.w,d0
        move.l  d0,-(sp)                /* src_id */

        jsr     S_PUnPSource
        lea     8(sp),sp                /* clear the stack */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

SfxRewindSource:
| void S_RewindSource(uint8_t src_id);
        moveq   #0,d0

        move.w  0x8010.w,d0
        move.l  d0,-(sp)                /* src_id */

        jsr     S_RewindSource
        lea     4(sp),sp                /* clear the stack */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

SfxStopSource:
| void S_StopSource(uint8_t src_id);
        moveq   #0,d0

        move.w  0x8010.w,d0
        move.l  d0,-(sp)                /* src_id */

        jsr     S_StopSource
        lea     4(sp),sp                /* clear the stack */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

SfxUpdateSource:
| void S_UpdateSource(uint8_t src_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop);
        moveq   #0,d0

        move.b  0x801b.w,d0
        move.l  d0,-(sp)                /* autoloop */
        move.b  0x8019.w,d0
        move.l  d0,-(sp)                /* vol */

        move.b  0x8017.w,d0
        move.l  d0,-(sp)                /* pan */
        move.w  0x8014.w,d0
        move.l  d0,-(sp)                /* freq */

        move.w  0x8010.w,d0
        move.l  d0,-(sp)                /* src_id */

        jsr     S_UpdateSource
        lea     20(sp),sp               /* clear the stack */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

SfxGetSourcePosition:
| void S_GetSourcePosition(uint8_t src_id);
        moveq   #0,d0
        move.w  0x8010.w,d0
        move.l  d0,-(sp)                /* src_id */

        jsr     S_GetSourcePosition
        lea     4(sp),sp                /* clear the stack */

        move.w  d0,0x8020.w             /* position */

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

SfxSuspendUpdates:
        move.b  0x8010.w,updates_suspend

        move.b  #'D,0x800F.w            /* sub comm port = DONE */
        bra     WaitAck

| void switch_banks(void);
| Switch 1M Banks
        .global switch_banks
switch_banks:
        bchg    #0,0x8003.w             /* switch banks */
0:
        btst    #1,0x8003.w
        bne.b   0b                      /* bank switch not finished */
        rts

| Sub-CPU Program VBlank (INT02) Service Handler

SPInt2:
        rts

| Sub-CPU program Reserved Function

SPNull:
        rts

|-----------------------------------------------------------------------|
|                            Sub-CPU code                               |
|-----------------------------------------------------------------------|

| Global Variable offsets - must match boot loader
        .equ    VBLANK_HANDLER, 0
        .equ    VBLANK_PARAM,   4
        .equ    INIT_CD,        8
        .equ    READ_CD,        12
        .equ    SET_CWD,        16
        .equ    FIRST_DIR_SEC,  20
        .equ    NEXT_DIR_SEC,   24
        .equ    FIND_DIR_ENTRY, 28
        .equ    NEXT_DIR_ENTRY, 32
        .equ    LOAD_FILE,      36
        .equ    DISC_TYPE,      40
        .equ    DIR_ENTRY,      42
        .equ    CWD_OFFSET,     44
        .equ    CWD_LENGTH,     48
        .equ    CURR_OFFSET,    52
        .equ    CURR_LENGTH,    56
        .equ    ROOT_OFFSET,    60
        .equ    ROOT_LENGTH,    64
        .equ    DENTRY_OFFSET,  68
        .equ    DENTRY_LENGTH,  72
        .equ    DENTRY_FLAGS,   76
        .equ    DENTRY_NAME,    78
        .equ    TEMP_NAME,      78+256
        .equ    SIZE_GLOBALVARS,78+256+256

| Disc Read Buffer
        .equ    DISC_BUFFER, 0x6800

| Program Load Buffer
        .equ    LOAD_BUFFER, 0x8000

| ISO directory offsets (big-endian where applicable)
        .equ    RECORD_LENGTH,  0
        .equ    EXTENT,         6
        .equ    FILE_LENGTH,    14
        .equ    FILE_FLAGS,     25
        .equ    FILE_NAME_LEN,  32
        .equ    FILE_NAME,      33

| Primary Volume Descriptor offset
        .equ    PVD_ROOT, 0x9C

| CDFS Error codes
        .equ    ERR_READ_FAILED,    -2
        .equ    ERR_NO_PVD,         -3
        .equ    ERR_NO_MORE_ENTRIES,-4
        .equ    ERR_BAD_ENTRY,      -5
        .equ    ERR_NAME_NOT_FOUND, -6
        .equ    ERR_NO_DISC,        -7

| XCMD Error codes
        .equ    ERR_UNKNOWN_CMD,    -1
        .equ    ERR_CMDDONE_TIMEOUT,-2
        .equ    ERR_CMDACK_TIMEOUT, -3

|-----------------------------------------------------------------------
| Global functions
|-----------------------------------------------------------------------

| int init_cd(void);
        .global init_cd
init_cd:
        movem.l d2-d7/a2-a6,-(sp)
        movea.l global_vars,a6
        lea     iso_pvd_magic,a5
        movea.l INIT_CD(a6),a2
        jsr     (a2)
        movem.l (sp)+,d2-d7/a2-a6
        rts

| int read_cd(int lba, int len, void *buffer);
        .global read_cd
read_cd:
        move.l  4(sp),d0                /* lba */
        move.l  8(sp),d1                /* length */
        movea.l 12(sp),a0               /* buffer */
        movem.l d2-d7/a2-a6,-(sp)
        movea.l global_vars,a6
        movea.l READ_CD(a6),a2
        jsr     (a2)
        movem.l (sp)+,d2-d7/a2-a6
        rts

| int set_cwd(char *path);
        .global set_cwd
set_cwd:
        movea.l 4(sp),a0                /* path */
        movem.l d2-d7/a2-a6,-(sp)
        movea.l global_vars,a6
        movea.l SET_CWD(a6),a2
        jsr     (a2)
        movem.l (sp)+,d2-d7/a2-a6
        rts

| int first_dir_sec(void);
        .global first_dir_sec
first_dir_sec:
        movem.l d2-d7/a2-a6,-(sp)
        movea.l global_vars,a6
        movea.l FIRST_DIR_SEC(a6),a2
        jsr     (a2)
        movem.l (sp)+,d2-d7/a2-a6
        rts

| int next_dir_sec(void);
        .global next_dir_sec
next_dir_sec:
        movem.l d2-d7/a2-a6,-(sp)
        movea.l global_vars,a6
        movea.l NEXT_DIR_SEC(a6),a2
        jsr     (a2)
        movem.l (sp)+,d2-d7/a2-a6
        rts

| int find_dir_entry(char *name);
        .global find_dir_entry
find_dir_entry:
        movea.l 4(sp),a0
        movem.l d2-d7/a2-a6,-(sp)
        movea.l global_vars,a6
        movea.l FIND_DIR_ENTRY(a6),a2
        jsr     (a2)
        movem.l (sp)+,d2-d7/a2-a6
        rts

| int next_dir_entry(void)
        .global next_dir_entry
next_dir_entry:
        movem.l d2-d7/a2-a6,-(sp)
        movea.l global_vars,a6
        movea.l NEXT_DIR_ENTRY(a6),a2
        jsr     (a2)
        movem.l (sp)+,d2-d7/a2-a6
        rts

| int load_file(char *filename, void *buffer);
        .global load_file
load_file:
        movea.l 4(sp),a0                /* filename */
        movea.l 8(sp),a1                /* buffer */
        movem.l d2-d7/a2-a6,-(sp)
        movea.l global_vars,a6
        movea.l LOAD_FILE(a6),a2
        jsr     (a2)
        movem.l (sp)+,d2-d7/a2-a6
        rts

    .align  4

int3_callback:
    .long   0

int3_cntr:
    .word   0

| Sub-CPU variables

        .align  2
drive_init_parms:
        .byte   0x01, 0xFF              /* first track (1), last track (all) */

track_number:
        .word   0

updates_suspend:
        .byte   0

        .data

        .align  4

        .global global_vars
global_vars:
        .long   0

iso_pvd_magic:
        .asciz  "\1CD001\1"

        .global _start
_start:
