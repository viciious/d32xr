| SEGA 32X support code for the 68000
| by Chilly Willy

        .text

        .equ DEFAULT_LINK_TIMEOUT, 0x3FFF

        .macro  sh2_wait
        move.w  #0x0003,0xA15102    /* assert CMD INT to both SH2s */
        move.w  #0xA55A,d2
99:
        cmp.w   0xA15120,d2         /* wait on handshake in COMM0 */
        bne.b   99b
98:
        cmp.w   0xA15124,d2         /* wait on handshake in COMM4 */
        bne.b   98b
        .endm

        .macro  sh2_cont
        move.w  #0xFFFE,d0          /* command = exit irq */
        move.w  d0,0xA15120
        move.w  d0,0xA15124
99:
        cmp.w   0xA15120,d0         /* wait on exit */
        beq.b   99b
98:
        cmp.w   0xA15124,d0         /* wait on exit */
        beq.b   98b
        .endm

        .macro  set_rv
        bset    #0,0xA15107         /* set RV */
        .endm

        .macro  clr_rv
        bclr    #0,0xA15107         /* clear RV */
        .endm

| 0x880800 - entry point for reset/cold-start

        .global _start
_start:

| Clear Work RAM
        moveq   #0,d0
        move.w  #0x3FFF,d1
        suba.l  a1,a1
1:
        move.l  d0,-(a1)
        dbra    d1,1b

| Copy initialized variables from ROM to Work RAM
        lea     __text_end,a0
        move.w  #__data_size,d0
        lsr.w   #1,d0
        subq.w  #1,d0
2:
        move.w  (a0)+,(a1)+
        dbra    d0,2b

        lea     __stack,sp              /* set stack pointer to top of Work RAM */

        bsr     init_hardware           /* initialize the console hardware */

        jsr     main                    /* call program main() */
3:
        stop    #0x2700
        bra.b   3b

        .align  64

| 0x880840 - 68000 General exception handler

        move.l  d0,-(sp)
        move.l  4(sp),d0            /* jump table return address */
        sub.w   #0x206,d0           /* 0 = BusError, 6 = AddrError, etc */

        /* call exception handler here */

        move.l  (sp)+,d0
        addq.l  #4,sp               /* pop jump table return address */
        rte

        .align  64

| 0x880880 - 68000 Level 4 interrupt handler - HBlank IRQ

        rte

        .align  64

| 0x8808C0 - 68000 Level 6 interrupt handler - VBlank IRQ

        move.l  d0,-(sp)
        move.l  vblank,d0
        beq.b   1f
        move.l  a0,-(sp)
        movea.l d0,a0
        jmp     (a0)
1:
        move.l  (sp)+,d0
        rte

        .align  64

| 0x880900 - 68000 Level 2 interrupt handler - EXT IRQ

        move.l  d0,-(sp)
        move.l  extint,d0
        beq.b   1f
        move.l  a0,-(sp)
        movea.l d0,a0
        jmp     (a0)
1:
        move.l  (sp)+,d0
        rte


        .align  64

| Initialize the MD side to a known state for the game

init_hardware:
        move.w  #0x8104,(0xC00004)      /* display off, vblank disabled */
        move.w  (0xC00004),d0           /* read VDP Status reg */

| init joyports
        move.b  #0x40,0xA10009
        move.b  #0x40,0xA1000B
        move.b  #0x40,0xA10003
        move.b  #0x40,0xA10005

| init vdp
        moveq   #0,d0                   /* full init */
        jsr     init_vdp

| setup default font
        jsr     load_font

| init controllers
        jsr     chk_ports

| allow the 68k to access the FM chip
        move.w  #0x0100,0xA11100        /* Z80 assert bus request */
        move.w  #0x0100,0xA11200        /* Z80 deassert reset */

        move.w  #0,0xA15104             /* set cart bank select */

| wait on Mars side
        move.b  #0,0xA15107             /* clear RV - allow SH2 to access ROM */
0:
        cmp.l   #0x4D5F4F4B,0xA15120    /* M_OK */
        bne.b   0b                      /* wait for master ok */
1:
        cmp.l   #0x535F4F4B,0xA15124    /* S_OK */
        bne.b   1b                      /* wait for slave ok */

        move.l  #vert_blank,vblank      /* set vertical blank interrupt handler */
        move.w  #0x8174,0xC00004        /* display on, vblank enabled */
        move.w  #0x2000,sr              /* enable interrupts */
        rts


| void write_byte(void *dst, unsigned char val)
        .global write_byte
write_byte:
        movea.l 4(sp),a0
        move.l  8(sp),d0
        move.b  d0,(a0)
        rts

| void write_word(void *dst, unsigned short val)
        .global write_word
write_word:
        movea.l 4(sp),a0
        move.l  8(sp),d0
        move.w  d0,(a0)
        rts

| void write_long(void *dst, unsigned int val)
        .global write_long
write_long:
        movea.l 4(sp),a0
        move.l  8(sp),d0
        move.l  d0,(a0)
        rts

| unsigned char read_byte(void *src)
        .global read_byte
read_byte:
        movea.l 4(sp),a0
        move.b  (a0),d0
        rts

| unsigned short read_word(void *src)
        .global read_word
read_word:
        movea.l 4(sp),a0
        move.w  (a0),d0
        rts

| unsigned int read_long(void *src)
        .global read_long
read_long:
        movea.l 4(sp),a0
        move.l  (a0),d0
        rts


        .data

| Put remaining code in data section to lower bus contention for the rom.

        .global do_main
do_main:
        move.b  #1,0xA15107         /* set RV */
        move.b  #2,0xA130F1         /* SRAM disabled, write protected */
        move.b  #0,0xA15107         /* clear RV */


| detect the Mega EverDrive
        move.w  #0x2700,sr          /* disable ints */
        move.b  #1,0xA15107         /* set RV */
        jsr     InitEverDrive
        move.w  d0,everdrive_ok
        move.b  #0,0xA15107         /* clear RV */
        move.w  #0x2000,sr          /* enable ints */

main_loop_start:
        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to MARS hw */
        move.l  #0,0xA15120         /* let Master SH2 run */

main_loop:
        move.w  #0x2700,sr          /* disable ints */
        bsr     bump_fm
        move.w  #0x2000,sr          /* enable ints */

        move.w  0xA15120,d0         /* get COMM0 */
        bne.b   handle_req

        move.w  0xA15124,d0         /* get COMM4 */
        bne.w   handle_sec_req

chk_hotplug:
        /* check hot-plug count */
        tst.b   hotplug_cnt
        bne.b   main_loop
        move.b  #60,hotplug_cnt

        move.w  0xA1512C,d0
        cmpi.w  #0xF001,d0
        beq.b   0f                  /* mouse in port 1, check port 2 */
        cmpi.w  #0xF000,d0
        beq.b   1f                  /* no pad in port 1, do hot-plug check */
0:
        tst.b   net_type
        bne.b   main_loop           /* networking enabled, ignore port 2 */
        move.w  0xA1512E,d0
        cmpi.w  #0xF001,d0
        beq.b   main_loop           /* mouse in port 2, exit */
        cmpi.w  #0xF000,d0
        bne.b   main_loop           /* pad in port 2, exit */
1:
        bsr     chk_ports
        bra.b   main_loop

| process request from Master SH2
handle_req:
        cmpi.w  #0x01FF,d0
        bls     read_sram
        cmpi.w  #0x02FF,d0
        bls     write_sram
        cmpi.w  #0x03FF,d0
        bls     start_music
        cmpi.w  #0x04FF,d0
        bls     stop_music
        cmpi.w  #0x05FF,d0
        bls     read_mouse
        cmpi.w  #0x06FF,d0
        bls     read_cdstate
        cmpi.w  #0x07FF,d0
        bls     set_usecd
        cmpi.w  #0x08FF,d0
        bls     set_crsr
        cmpi.w  #0x09FF,d0
        bls     get_crsr
        cmpi.w  #0x0AFF,d0
        bls     set_color
        cmpi.w  #0x0BFF,d0
        bls     get_color
        cmpi.w  #0x0CFF,d0
        bls     set_dbpal
        cmpi.w  #0x0DFF,d0
        bls     put_chr
        cmpi.w  #0x0EFF,d0
        bls     clear_a
        cmpi.w  #0x0FFF,d0
        bls     dbug_start
        cmpi.w  #0x10FF,d0
        bls     dbug_queue
        cmpi.w  #0x11FF,d0
        bls     dbug_end
        cmpi.w  #0x12FF,d0
        bls     net_getbyte
        cmpi.w  #0x13FF,d0
        bls     net_putbyte
        cmpi.w  #0x14FF,d0
        bls     net_setup
        cmpi.w  #0x15FF,d0
        bls     net_cleanup
        cmpi.w  #0x16FF,d0
        bls     set_bank_page
        cmpi.w  #0x17FF,d0
        bls     net_set_link_timeout
        cmpi.w  #0x18FF,d0
        bls     set_music_volume
        cmpi.w  #0x19FF,d0
        bls     ctl_md_vdp
        cmpi.w  #0x1AFF,d0
        bls     cpy_md_vram
| unknown command
        move.w  #0,0xA15120         /* done */
        bra     main_loop

| process request from Secondary SH2
handle_sec_req:
        cmpi.w  #0x15FF,d0
        bls     chk_hotplug
        cmpi.w  #0x16FF,d0
        bls     set_bank_page_sec
| unknown command
        move.w  #0,0xA15124         /* done */
        bra     main_loop

read_sram:
        move.w  #0x2700,sr          /* disable ints */
        moveq   #0,d0
        move.w  0xA15122,d0         /* COMM2 holds offset */
        moveq   #0,d1
        move.w  everdrive_ok,d1
        andi.l  #0x0100,d1
        beq.w   2f                  /* not everdrive with extended SSF */
1:
        sh2_wait
        set_rv
        lea     0x40000,a0          /* use the upper 256K of page 31 */
        move.w  #0x801F, 0xA130F0   /* map page 31 to bank 0 */
        move.b  0(a0,d0.l),d1       /* read SRAM */
        move.w  #0x8000, 0xA130F0   /* map page 0 to bank 0 */
        clr_rv
        move.w  d1,0xA15122         /* COMM2 holds return byte */
        bra     3f
2:
        add.l   d0,d0
        lea     0x200000,a0
        sh2_wait
        set_rv
        move.b  #3,0xA130F1         /* SRAM enabled, write protected */
        move.b  1(a0,d0.l),d1       /* read SRAM */
        move.b  #2,0xA130F1         /* SRAM disabled, write protected */
        clr_rv
        move.w  d1,0xA15122         /* COMM2 holds return byte */
3:
        sh2_cont
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop

write_sram:
        move.w  #0x2700,sr          /* disable ints */
        moveq   #0,d1
        move.w  everdrive_ok,d1
        andi.l  #0x0100,d1
        beq.w   2f                  /* not everdrive with extended SSF */
1:
        move.w  0xA15122,d1         /* COMM2 holds offset */
        sh2_wait
        set_rv
        lea     0x40000,a0          /* use the upper 256K of page 31 */
        move.w  #0xA01F, 0xA130F0   /* map page 31 to bank 0, enable the write bit */
        move.b  d0,0(a0,d1.l)       /* write SRAM */
        move.w  #0x8000, 0xA130F0   /* map page 0 to bank 0 */
        clr_rv
        bra     3f
2:
        move.w  0xA15122,d1         /* COMM2 holds offset */
        add.l   d1,d1
        lea     0x200000,a0
        sh2_wait
        set_rv
        move.b  #1,0xA130F1         /* SRAM enabled, write enabled */
        move.b  d0,1(a0,d1.l)       /* write SRAM */
        move.b  #2,0xA130F1         /* SRAM disabled, write protected */
        clr_rv
3:
        sh2_cont
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop

set_rom_bank:
        move.l  a0,d3
        swap    d3
        lsr.w   #4,d3
        andi.w  #3,d3
        move.w  d3,0xA15104         /* set cart bank select */
        move.l  a0,d3
        andi.l  #0x0FFFFF,d3
        ori.l   #0x900000,d3
        movea.l d3,a1
        rts

start_music:
        tst.w   use_cd
        bne     start_cd
        
        /* start VGM */
        clr.l   vgm_ptr
        cmpi.w  #0x0300,d0
        beq.b   0f

        /* fetch VGM length */
        lea     vgm_size,a0
        move.w  0xA15122,0(a0)
        move.w  #0,0xA15120         /* done with upper word */
20:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x0302,d0
        bne.b   20b
        move.w  0xA15122,2(a0)
        move.w  #0,0xA15120         /* done with lower word */

        /* fetch VGM offset */
        lea     vgm_ptr,a0
21:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x0303,d0
        bne.b   21b
        move.w  0xA15122,0(a0)
        move.w  #0,0xA15120         /* done with upper word */
22:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x0304,d0
        bne.b   22b
        move.w  0xA15122,2(a0)
        move.w  #0,0xA15120         /* done with lower word */

23:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x0300,d0
        bne.b   23b

0:
        /* set VGM pointer and init VGM player */
        move.w  0xA15122,d0          /* COMM2 = index | repeat flag */
        move.w  #0x8000,d1
        and.w   d0,d1                /* repeat flag */
        eor.w   d1,d0                /* clear flag from index */
        move.w  d1,fm_rep            /* repeat flag */
        move.w  d0,fm_idx            /* index 1 to N */
        move.w  #0,0xA15104          /* set cart bank select */
        move.l  #0,a0
        move.l  vgm_ptr,d0           /* set VGM offset */
        beq.b   9f

        move.l  d0,a0
        bsr     set_rom_bank
        move.l  a1,fm_ptr

        jsr     vgm_setup

        clr.w   fm_smpl
        clr.w   fm_tick
        bsr     fm_init             /* initial YM2612 */
        bsr     fm_setup            /* initial VGM player */

        move.w  #0,0xA15120         /* done */
        bra     main_loop
9:
        clr.l   fm_ptr
        clr.w   fm_idx              /* not playing VGM */

        move.w  #0,0xA15120         /* done */
        bra     main_loop

start_cd:
        tst.w   megasd_ok
        beq     9f                  /* couldn't find a MegaSD */

        move.w  0xA15122,d0          /* COMM2 = index | repeat flag */
        move.w  #0x8000,d1
        and.w   d0,d1                /* repeat flag */
        eor.w   d1,d0                /* clear flag from index */
        lsr.w   #7,d1
        lsr.w   #8,d1

        move.l  d1,-(sp)            /* push the looping flag */
        move.l  d0,-(sp)            /* push the cdtrack */
        jsr     MegaSD_PlayCDTrack
        lea     8(sp),sp            /* clear the stack */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

9:
        tst.w   cd_ok
        beq     2f                  /* couldn't init cd */
        tst.b   cd_ok
        bne.b   0f                  /* disc found - try to play track */
        clr.w   number_tracks
        /* check for CD */
10:
        move.b  0xA1200F,d1
        bne.b   10b                 /* wait until Sub-CPU is ready to receive command */
        move.b  #'D,0xA1200E        /* set main comm port to GetDiskInfo command */
11:
        move.b  0xA1200F,d0
        beq.b   11b                 /* wait for acknowledge byte in sub comm port */
        move.b  #0x00,0xA1200E      /* acknowledge receipt of command result */

        cmpi.b  #'D,d0
        bne.b   2f                  /* couldn't get disk info */
        move.w  0xA12020,d0         /* BIOS status */
        cmpi.w  #0x1000,d0
        bhs.b   2f                  /* open, busy, or no disc */
        move.b  #1,cd_ok            /* we have a disc - try to play track */

        move.w  0xA12022,d0         /* first and last track */
        moveq   #0,d1
        move.b  d0,d1
        move.w  d1,last_track
        lsr.w   #8,d0
        move.w  d0,first_track
        sub.w   d0,d1
        addq.w  #1,d1
        move.w  d1,number_tracks
0:
        move.b  0xA1200F,d1
        bne.b   0b                  /* wait until Sub-CPU is ready to receive command */

        move.w  0xA15122,d0         /* COMM2 = index | repeat flag */
        move.w  #0x8000,d1
        and.w   d0,d1               /* repeat flag */
        eor.w   d1,d0               /* clear flag from index */
        lsr.w   #7,d1
        lsr.w   #8,d1

        move.b  d1,0xA12012         /* repeat flag */
        move.w  d0,0xA12010         /* track number */
        move.b  #'P,0xA1200E        /* set main comm port to PlayTrack command */
1:
        move.b  0xA1200F,d0
        beq.b   1b                  /* wait for acknowledge byte in sub comm port */
        move.b  #0x00,0xA1200E      /* acknowledge receipt of command result */
2:
        move.w  #0,0xA15120         /* done */
        bra     main_loop

stop_music:
        tst.w    use_cd
        bne.b    stop_cd

        /* stop VGM */
        clr.w   fm_idx          /* stop music */
        clr.w   fm_rep
        clr.w   fm_smpl
        clr.w   fm_tick
        clr.l   fm_ptr
        bsr     fm_init

        move.w  #0,0xA15120         /* done */
        bra     main_loop

stop_cd:
        tst.w   megasd_ok
        beq     3f                  /* couldn't find a MegaSD */

        jsr     MegaSD_PauseCD
        move.w  #0,0xA15120         /* done */
        bra     main_loop

3:
        tst.w   cd_ok
        beq.b   2f
0:
        move.b  0xA1200F,d1
        bne.b   0b                  /* wait until Sub-CPU is ready to receive command */

        move.b  #'S,0xA1200E        /* set main comm port to StopPlayback command */
1:
        move.b  0xA1200F,d0
        beq.b   1b                  /* wait for acknowledge byte in sub comm port */
        move.b  #0x00,0xA1200E      /* acknowledge receipt of command result */
2:
        move.w  #0,0xA15120         /* done */
        bra     main_loop

read_mouse:
        tst.b   d0
        bne.b   1f                  /* skip port 1 */

        move.w  0xA1512C,d0
        cmpi.w  #0xF001,d0
        bne.b   1f                  /* no mouse in port 1 */
        lea     0xA10003,a0
        bsr     get_mky
        cmpi.l  #-1,d0
        beq.b   no_mky1
        bset    #31,d0
        move.w  d0,0xA15122
        swap    d0
        move.w  d0,0xA15120
0:
        move.w  0xA15120,d0
        bne.b   0b                  /* wait for SH2 to read mouse value */
        bra     main_loop
1:
        move.w  0xA1512E,d0
        cmpi.w  #0xF001,d0
        bne.b   no_mouse            /* no mouse in port 2 */
        lea     0xA10005,a0
        bsr     get_mky
        cmpi.l  #-1,d0
        beq.b   no_mky2
        bset    #31,d0
        move.w  d0,0xA15122
        swap    d0
        move.w  d0,0xA15120
2:
        move.w  0xA15120,d0
        bne.b   2b                  /* wait for SH2 to read mouse value */
        bra     main_loop

no_mky1:
        move.w  #0xF000,0xA1512C    /* nothing in port 1 */
        bra.b   no_mouse
no_mky2:
       move.w   #0xF000,0xA1512E    /* nothing in port 2 */
no_mouse:
        moveq   #-1,d0              /* no mouse */
        move.w  d0,0xA15122
        swap    d0
        move.w  d0,0xA15120
4:
        move.w  0xA15120,d0
        bne.b   4b                  /* wait for SH2 to read mouse value */
        bra     main_loop


read_cdstate:
        tst.w   megasd_ok
        beq     9f                  /* couldn't find a MegaSD or CD audio tracks */

        move.w  megasd_num_cdtracks,d0
        lsl.l   #2,d0
        or.w    megasd_ok,d0

        move.w  d0,0xA15122
        move.w  #0,0xA15120         /* done */
        bra     main_loop

9:
        tst.w   cd_ok
        beq     0f                  /* couldn't init cd */
        tst.b   cd_ok
        bne.b   0f                  /* disc found - return state */
        clr.w   number_tracks
        /* check for disc */
10:
        move.b  0xA1200F,d1
        bne.b   10b                 /* wait until Sub-CPU is ready to receive command */
        move.b  #'D,0xA1200E        /* set main comm port to GetDiskInfo command */
11:
        move.b  0xA1200F,d0
        beq.b   11b                 /* wait for acknowledge byte in sub comm port */
        move.b  #0x00,0xA1200E      /* acknowledge receipt of command result */

        cmpi.b  #'D,d0
        bne.b   0f                  /* couldn't get disk info */
        move.w  0xA12020,d0         /* BIOS status */
        cmpi.w  #0x1000,d0
        bhs.b   0f                  /* open, busy, or no disc */
        move.b  #1,cd_ok            /* we have a disc - get state info */

        move.w  0xA12022,d0         /* first and last track */
        moveq   #0,d1
        move.b  d0,d1
        move.w  d1,last_track
        lsr.w   #8,d0
        move.w  d0,first_track
        sub.w   d0,d1
        addq.w  #1,d1
        move.w  d1,number_tracks
0:
        move.w  number_tracks,d0
        lsl.l   #2,d0
        or.w    cd_ok,d0
        move.w  d0,0xA15122
        move.w  #0,0xA15120         /* done */
        bra     main_loop

set_usecd:
        move.w  0xA15122,use_cd     /* COMM2 holds the new value */
        move.w  #0,0xA15120         /* done */
        bra     main_loop


| network support functions

ext_serial:
        move.w  #0x2700,sr          /* disable ints */
        move.l  d1,-(sp)
        lea     net_rdbuf,a0
        move.w  net_wbix,d1

        btst    #2,0xA10019         /* check RERR on serial control register of joypad port 2 */
        bne.b   1f                  /* error */        
        btst    #1,0xA10019         /* check RRDY on serial control register of joypad port 2 */
        beq.b   3f                  /* no byte available, ignore int */
        moveq   #0,d0               /* clear serial status byte */
        move.b  0xA10017,d0         /* received byte */
        bra.b   2f
1:
        move.w  #0xFF04,d0          /* serial status and received byte = 0xFF04 (serial read error) */
2:
        move.w  d0,0(a0,d1.w)       /* write status:data to buffer */
        addq.w  #2,d1
        andi.w  #30,d1
        move.w  d1,net_wbix         /* update buffer write index */
3:
        move.l  (sp)+,d1
        movea.l (sp)+,a0
        move.l  (sp)+,d0
        rte

ext_link:
        move.w  #0x2700,sr          /* disable ints */
        btst    #6,0xA10005         /* check TH asserted */
        bne.b   2f                  /* no, extraneous ext int */

        move.l  d1,-(sp)
        lea     net_rdbuf,a0
        move.w  net_wbix,d1

        move.b  0xA10005,d0         /* read nibble from other console */
        move.b  #0x00,0xA10005      /* assert handshake (TR low) */

        move.b  d0,0(a0,d1.w)       /* write port byte to buffer - one nibble of data */
        addq.w  #1,d1
        andi.w  #31,d1
        move.w  d1,net_wbix         /* update buffer write index */
        move.w  net_link_timeout,d1
0:
        nop
        nop
        btst    #6,0xA10005         /* check TH deasserted */
        bne.b   1f                  /* handshake */
        dbra    d2,0b
        bra.b   3f                  /* timeout err */
1:
        move.b  #0x20,0xA10005      /* deassert handshake (TR high) */
        move.l  (sp)+,d1
2:
        movea.l (sp)+,a0
        move.l  (sp)+,d0
        rte
3:
        /* timeout during handshake - shut down link net */
        clr.b   net_type
        clr.l   extint
        move.w  #0x8B00,0xC00004    /* reg 11 = /IE2 (no EXT INT), full scroll */
        move.b  #0x40,0xA1000B      /* port 2 to neutral setting */
        nop
        nop
        move.b  #0x40,0xA10005
        move.l  (sp)+,d1
        movea.l (sp)+,a0
        move.l  (sp)+,d0
        rte

init_serial:
        move.w  #0xF000,0xA1512E    /* port 1 ID = SEGA_CTRL_NONE */
        move.b  #0x10,0xA1000B      /* all pins inputs except TL (Pin 6) */
        nop
        nop
        move.b  #0x38,0xA10019      /* 4800 Baud 8-N-1, RDRDY INT allowed */
        clr.w   net_rbix
        clr.w   net_wbix
        move.l  #ext_serial,extint  /* serial read data ready handler */
        move.w  #0x8B08,0xC00004    /* reg 11 = IE2 (enable EXT INT), full scroll */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

init_link:
        move.w  #0xF000,0xA1512E    /* port 1 ID = SEGA_CTRL_NONE */
        move.b  #0x00,0xA10019      /* no serial */
        nop
        nop
        move.b  #0xA0,0xA1000B      /* all pins inputs except TR, TH INT allowed */
        nop
        nop
        move.b  #0x20,0xA10005      /* TR set */
        clr.w   net_rbix
        clr.w   net_wbix
        move.l  #ext_link,extint    /* TH INT handler */
        move.w  #0x8B08,0xC00004    /* reg 11 = IE2 (enable EXT INT), full scroll */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

read_serial:
        move.w  #0x2700,sr          /* disable ints */
        lea     net_rdbuf,a0
        move.w  net_rbix,d1
        cmp.w   net_wbix,d1
        beq.b   1f                  /* no data in buffer */

        move.w  0(a0,d1.w),d0       /* received status:data */
        addq.w  #2,d1
        andi.w  #30,d1
        move.w  d1,net_rbix         /* update buffer read index */
        bra.b   2f
1:
        move.w  #0xFF00,d0          /* set status:data to 0xFF00 (no data available) */
2:
        move.w  d0,0xA15122         /* return received status:data in COMM2 */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop

read_link:
        move.w  #0x2700,sr          /* disable ints */
        btst    #0,net_wbix+1       /* check for even index */
        bne.b   1f                  /* in middle of collecting nibbles */

        lea     net_rdbuf,a0
        move.w  net_rbix,d1
        cmp.w   net_wbix,d1
        beq.b   1f                  /* no data in buffer */

        move.w  0(a0,d1.w),d0       /* data nibbles */
        andi.w  #0x0F0F,d0
        lsl.b   #4,d0
        lsr.w   #4,d0
        addq.w  #2,d1
        andi.w  #30,d1
        move.w  d1,net_rbix         /* update buffer read index */
        bra.b   2f
1:
        move.w  #0xFF00,d0          /* set status:data to 0xFF00 (no data available) */
2:
        move.w  d0,0xA15122         /* return received byte and status in COMM2 */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop

write_serial:
        move.w  #0x2700,sr          /* disable ints */
        move.w  net_link_timeout,d1
0:
        bsr     bump_fm
        btst    #0,0xA10019         /* ok to transmit? */
        beq.b   1f                  /* yes */
        dbra    d1,0b               /* wait until ok to transmit */
        bra.b   2f                  /* timeout err */
1:
        move.b  d0,0xA10015         /* Send byte */
        move.w  #0,0xA15122         /* okay */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop
2:
        move.w  net_link_timeout,0xA15122    /* timeout */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop

write_link:
        move.w  #0x2700,sr          /* disable ints */
        move.b  #0x2F,0xA1000B      /* only TL and TH in, TH INT not allowed */

        move.b  d0,d1               /* save lsn */
        lsr.b   #4,d0               /* msn */
        ori.b   #0x20,d0            /* set TR line */
        move.b  d0,0xA10005         /* set nibble out */
        nop
        nop
        andi.b  #0x0F,d0            /* clr TR line */
        move.b  d0,0xA10005         /* assert TH of other console */

        /* wait on handshake */
        move.w  net_link_timeout,d2
0:
        bsr     bump_fm
        btst    #6,0xA10005         /* check for TH low (handshake) */
        beq.b   1f                  /* handshake */
        dbra    d2,0b
        bra.w   9f                  /* timeout err */
1:
        ori.b   #0x20,d0            /* set TR line */
        move.b  d0,0xA10005         /* deassert TH of other console */
        move.w  net_link_timeout,d2
2:
        nop
        nop
        btst    #6,0xA10005         /* wait for TH high (handshake done) */
        bne.b   3f                  /* handshake */
        dbra    d2,2b
        bra.w   9f                  /* timeout err */
3:
        moveq   #0x0F,d0
        and.b   d1,d0               /* lsn */
        ori.b   #0x20,d0            /* set TR line */
        move.b  d0,0xA10005         /* set nibble out */
        nop
        nop
        andi.b  #0x0F,d0            /* clr TR line */
        move.b  d0,0xA10005         /* assert TH of other console */

        /* wait on handshake */
4:
        bsr     bump_fm
        btst    #6,0xA10005         /* check for TH low (handshake) */
        bne.b   4b

        ori.b   #0x20,d0            /* set TR line */
        move.b  d0,0xA10005         /* deassert TH of other console */
5:
        nop
        nop
        btst    #6,0xA10005         /* wait for TH high (handshake done) */
        beq.b   5b

        move.b  #0x20,0xA10005      /* TR set */
        nop
        nop
        move.b  #0xA0,0xA1000B      /* all pins inputs except TR, TH INT allowed */
        move.w  #0,0xA15122         /* okay */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop
9:
        move.b  #0x20,0xA10005      /* TR set */
        nop
        nop
        move.b  #0xA0,0xA1000B      /* all pins inputs except TR, TH INT allowed */
        move.w  #0xFFFF,0xA15122    /* timeout */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop


net_getbyte:
        tst.b   net_type
        bmi.w   read_serial
        bne.w   read_link
        move.w  #0xFF00,0xA15122    /* nothing */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

net_putbyte:
        tst.b   net_type
        bmi.w   write_serial
        bne.w   write_link
        move.w  #0xFFFF,0xA15122    /* timeout */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

net_setup:
        move.b  d0,net_type
        tst.b   net_type
        bmi.w   init_serial
        bne.w   init_link
        move.w  #0,0xA15120         /* done */
        bra     main_loop

net_cleanup:
        clr.b   net_type
        clr.l   extint
        move.w  #0x8B00,0xC00004    /* reg 11 = /IE2 (no EXT INT), full scroll */
        move.b  #0x00,0xA10019      /* no serial */
        nop
        nop
        move.b  #0x40,0xA1000B      /* port 2 to neutral setting */
        nop
        nop
        move.b  #0x40,0xA10005
        move.w  #0,0xA15120         /* done */
        bra     main_loop


| video debug functions

set_crsr:
        move.w  0xA15122,d0         /* cursor y<<6 | x */
        move.w  d0,d1
        andi.w  #0x1F,d0
        move.w  d0,crsr_y
        lsr.l   #6,d1
        move.w  d1,crsr_x
        move.w  #0,0xA15120         /* done */
        bra     main_loop

get_crsr:
        move.w  crsr_y,d0           /* y coord */
        lsl.w   #6,d0
        or.w    crsr_x,d0           /* cursor y<<6 | x */
        move.w  d0,0xA15122
        move.w  #0,0xA15120         /* done */
        bra     main_loop

set_color:
        /* the foreground color is in the LS nibble of COMM0 */
        move.w  0xA15120,d0
        andi.l  #0x000F,d0
        move    d0,d1
        lsl.w   #4,d0
        or      d1,d0
        move.w  d0,d1
        lsl.w   #8,d0
        or      d1,d0
        move.w  d0,d1
        swap    d1
        move.w  d0,d1
        move.l  d1,fg_color

        /* the background color is in the second LS nibble of COMM0 */
        move.w  0xA15120,d0
        lsr.w   #4,d0
        andi.l  #0x000F,d0
        move    d0,d1
        lsl.w   #4,d0
        or      d1,d0
        move.w  d0,d1
        lsl.w   #8,d0
        or      d1,d0
        move.w  d0,d1
        swap    d1
        move.w  d0,d1
        move.l  d1,bg_color

        bsr     load_font
        move.w  #0,0xA15120         /* done */
        bra     main_loop

get_color:
        move.w  fg_color,d0
        andi.w  #0x000F,d0
        move    d0,d1
        move.w  bg_color,d0
        andi.w  #0x000F,d0
        lsl     #4,d0
        or      d1,d0
        move.w  d0,0xA15122
        move.w  #0,0xA15120         /* done */
        bra     main_loop

set_dbpal:
        andi.w  #0x0003,d0
        moveq   #13,d1
        lsl.w   d1,d0
        move.w  d0,dbug_color       /* palette select = N * 0x2000 */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

put_chr:
        andi.w  #0x00FF,d0          /* character */
        subi.b  #0x20,d0            /* font starts at space */
        or.w    dbug_color,d0       /* OR with color palette */
        lea     0xC00000,a0
        move.w  #0x8F02,4(a0)       /* set INC to 2 */
        moveq   #0,d1
        move.w  crsr_y,d1           /* y coord */
        lsl.w   #6,d1
        or.w    crsr_x,d1           /* cursor y<<6 | x */
        add.w   d1,d1               /* pattern names are words */
        swap    d1
        ori.l   #0x40000003,d1      /* OR cursor with VDP write VRAM at 0xC000 (scroll plane A) */
        move.l  d1,4(a0)            /* write VRAM at location of cursor in plane A */
        move.w  d0,(a0)             /* set pattern name for character */
        addq.w  #1,crsr_x           /* increment x cursor coord */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

clear_a:
        moveq   #0,d0
        lea     0xC00000,a0
        move.w  #0x8F02,4(a0)       /* set INC to 2 */
        move.l  #0x40000003,d1      /* VDP write VRAM at 0xC000 (scroll plane A) */
        move.l  d1,4(a0)            /* write VRAM at plane A start */
        move.w  #64*32-1,d1
0:
        move.w  d0,(a0)             /* clear name pattern */
        dbra    d1,0b
        move.w  #0,0xA15120         /* done */
        bra     main_loop

dbug_start:
        clr.w   dbq_ix              /* no entries in queue */
        move.w  #0,0xA15120         /* done */
        bra     main_loop

dbug_queue:
        move.w  dbq_ix,d1
        cmpi.w  #64,d1
        beq.b   0f                  /* queue full, ignore */
        lea     dbq_id,a1
        move.b  d0,0(a1,d1.w)       /* queue id */
        add.w   d1,d1
        add.w   d1,d1
        lea     dbq_val,a1
        move.w  0xA15122,2(a1,d1.w) /* queue value */
        addq.w  #1,dbq_ix
0:
        move.w  #0,0xA15120         /* done */
        bra     main_loop

dbug_end:
        move.w  #0,0xA15120         /* release SH2 now */

| process queue
        moveq   #0,d2
        move.w  dbq_ix,d3
        beq     main_loop           /* nothing to do */
        subq.w  #1,d3               /* for dbra */
        lea     dbq_id,a2
        lea     dbq_val,a3
        lea     dbug_list,a4
        lea     dbug_tmp,a5
0:
        moveq   #0,d4
        move.b  0(a2,d2.w),d4       /* id */
        move.w  d2,d0
        add.w   d0,d0
        add.w   d0,d0
        move.l  0(a3,d0.w),d0      /* value */
        add.w   d4,d4
        add.w   d4,d4
        movea.l 0(a4,d4.w),a0       /* debug list entry for id */
        move.w  (a0)+,crsr_x
        move.w  (a0)+,crsr_y
        movea.l (a0),a0             /* address of format string */

        move.l  d0,-(sp)            /* push the value */
        move.l  a0,-(sp)            /* push the format string pointer */
        move.l  a5,-(sp)            /* push the output buffer pointer */
        jsr     xvprintf            /* print to output buffer */
        lea     12(sp),sp           /* clear the stack */

| convert dbug_tmp to name patterns
        moveq   #0,d1
        move.w  crsr_y,d1           /* y coord */
        lsl.w   #6,d1
        or.w    crsr_x,d1           /* cursor y<<6 | x */
        add.w   d1,d1               /* pattern names are words */
        swap    d1
        ori.l   #0x40000003,d1      /* OR cursor with VDP write VRAM at 0xC000 (scroll plane A) */
        lea     0xC00000,a1
        move.l  d1,4(a1)            /* write VRAM at location of cursor in plane A */
        moveq   #0,d0
        movea.l a5,a0
1:
        move.b  (a0)+,d0
        beq.b   2f                  /* end of string */
        subi.b  #0x20,d0            /* font starts at space */
        or.w    dbug_color,d0       /* OR with color palette */
        move.w  d0,(a1)
        bra.b   1b
2:
        move.w  #0x2700,sr          /* disable ints */
        bsr     bump_fm
        move.w  #0x2000,sr          /* enable ints */
        addq.w  #1,d2
        dbra    d3,0b
        bra     main_loop

set_bank_page:
        move.w  d0,d1
        andi.l  #0x07,d0            /* bank number */
        lsr.w   #3,d1               /* page number */
        andi.l  #0x1f,d1
        lea     0xA130F0,a0
        add.l   d0,d0
        lea     1(a0,d0.l),a0
        move.w  #0x2700,sr          /* disable ints */
        sh2_wait
        set_rv
        move.b  d1,(a0)
        clr_rv
        sh2_cont
        move.w  #0x2000,sr          /* enable ints */
        move.w  #0,0xA15120         /* release SH2 now */
        bra     main_loop

set_bank_page_sec:
        move.w  d0,d1
        andi.l  #0x07,d0            /* bank number */
        lsr.w   #3,d1               /* page number */
        andi.l  #0x1f,d1
        lea     0xA130F0,a0
        add.l   d0,d0
        lea     1(a0,d0.l),a0
        move.w  #0x2700,sr          /* disable ints */
        sh2_wait
        set_rv
        move.b  d1,(a0)
        clr_rv
        sh2_cont
        move.w  #0x2000,sr          /* enable ints */
        move.w  #0x1000,0xA15124    /* release SH2 now */
        bra     main_loop

net_set_link_timeout:
        move.w  0xA15122,net_link_timeout
        move.w  #0,0xA15120         /* release SH2 now */
        bra     main_loop

set_music_volume:
        tst.w   megasd_ok
        beq     1f                  /* couldn't find a MegaSD */

        move    #0xFF,d1
        and.w   d1,d0

        move.l  d0,-(sp)            /* push the volume */
        jsr     MegaSD_SetCDVolume
        lea     4(sp),sp            /* clear the stack */
1:
        tst.w   cd_ok
        beq.b   4f                  /* couldn't init cd */
        tst.b   cd_ok
        beq.b   4f                  /* disc not found */

        move.w  #0xFF,d1
        and.w   d0,d1
        addq.w  #1,d1               /* volume (1 to 256) */
        add.w   d1,d1
        add.w   d1,d1               /* volume * 4 (4 to 1024) */
2:
        move.b  0xA1200F,d0
        bne     4b                  /* wait until Sub-CPU is ready to receive command */

        move.w  d1,0xA12010         /* cd volume */
        move.b  #'V,0xA1200E        /* set main comm port to SetVolume command */
3:
        move.b  0xA1200F,d0
        beq.b   3b                  /* wait for acknowledge byte in sub comm port */
        move.b  #0x00,0xA1200E      /* acknowledge receipt of command result */
4:
        move.w  #0,0xA15120         /* done */
        bra     main_loop

ctl_md_vdp:
        andi.w  #255, d0
        move.w  d0, init_vdp_latch

        tst.b   d0
        bne.b   1f                  /* re-init vdp and vram */

        move.w  #0x8134,0xC00004    /* display off, vblank enabled, V28 mode */
1:
        move.w  #0,0xA15120         /* done */
        bra     main_loop

cpy_md_vram:
        move.w  0xA15100,d1
        eor.w   #0x8000,d1
        move.w  d1,0xA15100         /* unset FM - disallow SH2 access to FB */

        lea     0xA15120,a1         /* 32x comm0 port */
        lea     0xA15122,a2         /* 32x comm2 port */
        moveq   #0,d1
        move.w  (a2),d1             /* word offset into vram */
        add.l   d1,d1

        lea     0x840200,a2         /* frame buffer */
        lea     0(a2,d1.l),a2

        cmpi.b  #1,d0
        beq.w   5f                  /* copy from vram */
        cmpi.b  #2,d0
        beq     10f                 /* swap with vram */

        /* COPY TO VRAM */
        cmpi.l  #280,d1             /* vram or wram? */
        bhs.b   2f                  /* wram */

        lea     0xC00000,a0         /* vdp data port */
        move.w  #0x8F02,4(a0)       /* set INC to 2 */

        #mulu.w  #224,d1
        lsl.w   #5,d1
        move    d1,d2
        add     d2,d2
        add     d2,d1
        add     d2,d2
        add     d2,d1

        lsl.l   #2,d1               /* get top two bits of offset into high word */
        lsr.w   #2,d1
        ori.w   #0x4000,d1          /* write VRAM */
        swap    d1
        move.l  d1,4(a0)            /* cmd port <- write VRAM at offset */
        move.w  #27,d0
1:
        move.w  (a2),(a0)           /* next word */
        move.w  320(a2),(a0)        /* next word */
        move.w  640(a2),(a0)        /* next word */
        move.w  960(a2),(a0)        /* next word */
        move.w  1280(a2),(a0)       /* next word */
        move.w  1600(a2),(a0)       /* next word */
        move.w  1920(a2),(a0)       /* next word */
        move.w  2240(a2),(a0)       /* next word */
        lea     2560(a2),a2
        dbra    d0,1b
        bra     4f                  /* done */
2:
        subi.l  #280,d1
        #mulu.w  #224,d1
        lsl.w   #5,d1
        move    d1,d2
        add     d2,d2
        add     d2,d1
        add     d2,d2
        add     d2,d1

        lea     col_store,a0
        lea     0(a0,d1.l),a0
        move.w  #27,d0
3:
        move.w  (a2),(a0)+          /* next word */
        move.w  320(a2),(a0)+       /* next word */
        move.w  640(a2),(a0)+       /* next word */
        move.w  960(a2),(a0)+       /* next word */
        move.w  1280(a2),(a0)+      /* next word */
        move.w  1600(a2),(a0)+      /* next word */
        move.w  1920(a2),(a0)+      /* next word */
        move.w  2240(a2),(a0)+      /* next word */
        lea     2560(a2),a2
        dbra    d0,3b
4:
        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        move.w  #0,0xA15120         /* done */
        bra     main_loop

5:
        /* COPY FROM VRAM */
        cmpi.l  #280,d1             /* vram or wram? */
        bhs.w   7f                  /* wram */

        #mulu.w  #224,d1
        lsl.w   #5,d1
        move    d1,d2
        add     d2,d2
        add     d2,d1
        add     d2,d2
        add     d2,d1

        lea     0xC00000,a0         /* vdp data port */
        move.w  #0x8F02,4(a0)       /* set INC to 2 */

        move.w  #0x9A00, (a1)
        moveq   #0,d0
51:
        move.w  (a1), d0
        cmpi.w  #0x1A01,d0
        bne.b   51b
        move.w  2(a1), d0

        andi.w  #0xFF,d0            /* column offset in words */
        add.l   d0,d0
        add.l   d0,d1

        #mulu.w  #160,d0
        lsl.l   #5,d0
        move.l  d0,d2
        add.l   d2,d2
        add.l   d2,d2
        add.l   d2,d0

        lea     0(a2,d0.l),a2

        lsl.l   #2,d1               /* get top two bits of offset into high word */
        lsr.w   #2,d1
        swap    d1
        move.l  d1,4(a0)            /* cmd port <- read VRAM from offset */

        move.w  2(a1), d0           /* length in words */
        lsr.w   #8,d0
        andi.w  #255,d0
        subq.w  #1,d0
        cmpi.w  #223,d0
        bne.b   6f

        move.w  #27,d0
60:
        move.w  (a0),(a2)           /* next word */
        move.w  (a0),320(a2)        /* next word */
        move.w  (a0),640(a2)        /* next word */
        move.w  (a0),960(a2)        /* next word */
        move.w  (a0),1280(a2)       /* next word */
        move.w  (a0),1600(a2)       /* next word */
        move.w  (a0),1920(a2)       /* next word */
        move.w  (a0),2240(a2)       /* next word */
        lea     2560(a2),a2
        dbra    d0,60b
        bra     9f                  /* done */
6:
        move.w  (a0), (a2)
        lea     320(a2),a2
        dbra    d0,6b
        bra     9f                  /* done */
7:
        subi.l  #280,d1
        #mulu.w  #224,d1
        lsl.w   #5,d1
        move    d1,d2
        lsl.w   #1,d2
        add     d2,d1
        lsl.w   #1,d2
        add     d2,d1

        move.w  #0x9A00, (a1)
        moveq   #0,d0
71:
        move.w  (a1), d0
        cmpi.w  #0x1A01,d0
        bne.b   71b
        move.w  2(a1), d0

        andi.w  #0xFF,d0            /* column offset in words */
        add.l   d0,d0
        add.l   d0,d1

        #mulu.w  #160,d0
        lsl.l   #5,d0
        move    d0,d2
        lsl.l   #2,d2
        add.l   d2,d0

        lea     0(a2,d0.l),a2

        lea     col_store,a0
        lea     0(a0,d1.l),a0

        move.w  2(a1), d0           /* length in words */
        lsr.w   #8,d0
        andi.w  #255,d0
        subq.w  #1,d0
        cmpi.w  #223,d0
        bne.b   8f

        move.w  #27,d0
80:
        move.w  (a0)+,(a2)          /* next word */
        move.w  (a0)+,320(a2)       /* next word */
        move.w  (a0)+,640(a2)       /* next word */
        move.w  (a0)+,960(a2)       /* next word */
        move.w  (a0)+,1280(a2)      /* next word */
        move.w  (a0)+,1600(a2)      /* next word */
        move.w  (a0)+,1920(a2)      /* next word */
        move.w  (a0)+,2240(a2)      /* next word */
        lea     2560(a2),a2
        dbra    d0,80b
        bra     9f                  /* done */
8:
        move.w  (a0)+, (a2)
        lea     320(a2),a2
        dbra    d0,8b
9:
        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        move.w  #0,0xA15120         /* done */
        bra     main_loop

10:
        /* SWAP WITH VRAM */
        cmpi.l  #280,d1             /* vram or wram? */
        bhs     12f                 /* wram */

        lea     0xC00000,a0         /* vdp data port */
        move.w  #0x8F02,4(a0)       /* set INC to 2 */

        #mulu.w  #224,d1
        lsl.w   #5,d1
        move    d1,d2
        add     d2,d2
        add     d2,d1
        add     d2,d2
        add     d2,d1

        lsl.l   #2,d1               /* get top two bits of offset into high word */
        lsr.w   #2,d1
        move.l  d1,d2
        ori.w   #0x4000,d2          /* write VRAM */
        swap    d1
        swap    d2
        move.l  d1,4(a0)            /* cmd port <- read VRAM at offset */
        move.w  #27,d0
        lea     col_store+20*224*2,a1
11:
        /* vram to swap buffer */
        move.w  (a0),(a1)+          /* next word */
        move.w  (a0),(a1)+          /* next word */
        move.w  (a0),(a1)+          /* next word */
        move.w  (a0),(a1)+          /* next word */
        move.w  (a0),(a1)+          /* next word */
        move.w  (a0),(a1)+          /* next word */
        move.w  (a0),(a1)+          /* next word */
        move.w  (a0),(a1)+          /* next word */
        dbra    d0,11b

        move.l  d2,4(a0)            /* cmd port <- write VRAM at offset */
        move.w  #27,d0
111:
        /* screen to vram */
        move.w  (a2),(a0)           /* next word */
        move.w  320(a2),(a0)        /* next word */
        move.w  640(a2),(a0)        /* next word */
        move.w  960(a2),(a0)        /* next word */
        move.w  1280(a2),(a0)       /* next word */
        move.w  1600(a2),(a0)       /* next word */
        move.w  1920(a2),(a0)       /* next word */
        move.w  2240(a2),(a0)       /* next word */
        lea     2560(a2),a2
        dbra    d0,111b

        move.w  #27,d0
        lea     -224*2(a1),a1
        adda.l  #-320*224,a2
112:
        /* swap buffer to screen */
        move.w  (a1)+,(a2)          /* next word */
        move.w  (a1)+,320(a2)       /* next word */
        move.w  (a1)+,640(a2)       /* next word */
        move.w  (a1)+,960(a2)       /* next word */
        move.w  (a1)+,1280(a2)      /* next word */
        move.w  (a1)+,1600(a2)      /* next word */
        move.w  (a1)+,1920(a2)      /* next word */
        move.w  (a1)+,2240(a2)      /* next word */
        lea     2560(a2),a2
        dbra    d0,112b
        bra     14f                 /* done */
12:
        subi.l  #280,d1
        #mulu.w  #224,d1
        lsl.w   #5,d1
        move    d1,d2
        add     d2,d2
        add     d2,d1
        add     d2,d2
        add     d2,d1

        lea     col_store,a0
        lea     0(a0,d1.l),a0
        move.w  #27,d0
        lea     col_store+20*224*2,a1
13:
        /* wram to swap buffer */
        move.w  (a0)+,(a1)+         /* next word */
        move.w  (a0)+,(a1)+         /* next word */
        move.w  (a0)+,(a1)+         /* next word */
        move.w  (a0)+,(a1)+         /* next word */
        move.w  (a0)+,(a1)+         /* next word */
        move.w  (a0)+,(a1)+         /* next word */
        move.w  (a0)+,(a1)+         /* next word */
        move.w  (a0)+,(a1)+         /* next word */
        dbra    d0,13b

        lea     -224*2(a0),a0
        move.w  #27,d0
131:
        /* screen to wram */
        move.w  (a2),(a0)+          /* next word */
        move.w  320(a2),(a0)+       /* next word */
        move.w  640(a2),(a0)+       /* next word */
        move.w  960(a2),(a0)+       /* next word */
        move.w  1280(a2),(a0)+      /* next word */
        move.w  1600(a2),(a0)+      /* next word */
        move.w  1920(a2),(a0)+      /* next word */
        move.w  2240(a2),(a0)+      /* next word */
        lea     2560(a2),a2
        dbra    d0,131b

        move.w  #27,d0
        lea     -224*2(a1),a1
        adda.l  #-320*224,a2
132:
        /* swap buffer to screen */
        move.w  (a1)+,(a2)          /* next word */
        move.w  (a1)+,320(a2)       /* next word */
        move.w  (a1)+,640(a2)       /* next word */
        move.w  (a1)+,960(a2)       /* next word */
        move.w  (a1)+,1280(a2)      /* next word */
        move.w  (a1)+,1600(a2)      /* next word */
        move.w  (a1)+,1920(a2)      /* next word */
        move.w  (a1)+,2240(a2)      /* next word */
        lea     2560(a2),a2
        dbra    d0,132b
14:
        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        move.w  #0,0xA15120         /* done */
        bra     main_loop

| set standard mapper registers to default mapping

reset_banks:
        move.w  #0x2700,sr          /* disable ints */
        move.b  #1,0xA15107         /* set RV */
        move.b  #1,0xA130F3         /* bank for 0x080000-0x0FFFFF */
        move.b  #2,0xA130F5         /* bank for 0x100000-0x17FFFF */
        move.b  #3,0xA130F7         /* bank for 0x180000-0x1FFFFF */
        move.b  #4,0xA130F9         /* bank for 0x200000-0x27FFFF */
        move.b  #5,0xA130FB         /* bank for 0x280000-0x2FFFFF */
        move.b  #6,0xA130FD         /* bank for 0x300000-0x37FFFF */
        move.b  #7,0xA130FF         /* bank for 0x380000-0x3FFFFF */
        move.b  #0,0xA15107         /* clear RV */
        move.w  #0x2000,sr          /* enable ints */
        rts


| init MD VDP

init_vdp:
        move.w  d0,d2
        lea     0xC00004,a0
        lea     0xC00000,a1
        move.w  #0x8004,(a0) /* reg 0 = /IE1 (no HBL INT), /M3 (enable read H/V cnt) */
        move.w  #0x8114,(a0) /* reg 1 = /DISP (display off), /IE0 (no VBL INT), M1 (DMA enabled), /M2 (V28 mode) */
        move.w  #0x8230,(a0) /* reg 2 = Name Tbl A = 0xC000 */
        move.w  #0x832C,(a0) /* reg 3 = Name Tbl W = 0xB000 */
        move.w  #0x8407,(a0) /* reg 4 = Name Tbl B = 0xE000 */
        move.w  #0x8554,(a0) /* reg 5 = Sprite Attr Tbl = 0xA800 */
        move.w  #0x8600,(a0) /* reg 6 = always 0 */
        move.w  #0x8700,(a0) /* reg 7 = BG color */
        move.w  #0x8800,(a0) /* reg 8 = always 0 */
        move.w  #0x8900,(a0) /* reg 9 = always 0 */
        move.w  #0x8A00,(a0) /* reg 10 = HINT = 0 */
        move.w  #0x8B00,(a0) /* reg 11 = /IE2 (no EXT INT), full scroll */
        move.w  #0x8C81,(a0) /* reg 12 = H40 mode, no lace, no shadow/hilite */
        move.w  #0x8D2B,(a0) /* reg 13 = HScroll Tbl = 0xAC00 */
        move.w  #0x8E00,(a0) /* reg 14 = always 0 */
        move.w  #0x8F01,(a0) /* reg 15 = data INC = 1 */
        move.w  #0x9001,(a0) /* reg 16 = Scroll Size = 64x32 */
        move.w  #0x9100,(a0) /* reg 17 = W Pos H = left */
        move.w  #0x9200,(a0) /* reg 18 = W Pos V = top */

| clear VRAM
        move.w  #0x8F02,(a0)            /* set INC to 2 */
        move.l  #0x40000000,(a0)        /* write VRAM address 0 */
        moveq   #0,d0
        move.w  #0x07FF,d1              /* 2K - 1 tiles */
        tst.w   d2
        beq.b   0f
        moveq   #0,d1                   /* 1 tile */
0:
        move.l  d0,(a1)                 /* clear VRAM */
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        dbra    d1,0b

        tst.w   d2
        beq.b   9f

        move.l  #0x60000002,(a0)        /* write VRAM address 0xA800 */
        moveq   #0,d0
        move.w  #0x02BF,d1              /* 704 - 1 tiles */
8:
        move.l  d0,(a1)                 /* clear VRAM */
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        move.l  d0,(a1)
        dbra    d1,8b
        bra.b   3f

| The VDP state at this point is: Display disabled, ints disabled, Name Tbl A at 0xC000,
| Name Tbl B at 0xE000, Name Tbl W at 0xB000, Sprite Attr Tbl at 0xA800, HScroll Tbl at 0xAC00,
| H40 V28 mode, and Scroll size is 64x32.

9:
| Clear CRAM
        move.l  #0x81048F02,(a0)        /* set reg 1 and reg 15 */
        move.l  #0xC0000000,(a0)        /* write CRAM address 0 */
        moveq   #31,d1
1:
        move.l  d0,(a1)
        dbra    d1,1b

| Clear VSRAM
        move.l  #0x40000010,(a0)         /* write VSRAM address 0 */
        moveq   #19,d1
2:
        move.l  d0,(a1)
        dbra    d1,2b

| set the default palette for text
        move.l  #0xC0000000,(a0)        /* write CRAM address 0 */
        move.l  #0x00000CCC,(a1)        /* entry 0 (black) and 1 (lt gray) */
        move.l  #0xC0200000,(a0)        /* write CRAM address 32 */
        move.l  #0x000000A0,(a1)        /* entry 16 (black) and 17 (green) */
        move.l  #0xC0400000,(a0)        /* write CRAM address 64 */
        move.l  #0x0000000A,(a1)        /* entry 32 (black) and 33 (red) */
3:
        move.w  #0x8174,0xC00004        /* display on, vblank enabled, V28 mode */
        rts


| load font tile data

load_font:
        lea     0xC00004,a0         /* VDP cmd/sts reg */
        lea     0xC00000,a1         /* VDP data reg */
        move.w  #0x8F02,(a0)        /* INC = 2 */
        move.l  #0x40000000,(a0)    /* write VRAM address 0 */
        lea     font_data,a2
        move.w  #0x6B*8-1,d2
0:
        move.l  (a2)+,d0            /* font fg mask */
        move.l  d0,d1
        not.l   d1                  /* font bg mask */
        and.l   fg_color,d0         /* set font fg color */
        and.l   bg_color,d1         /* set font bg color */
        or.l    d1,d0
        move.l  d0,(a1)             /* set tile line */
        dbra    d2,0b
        rts


| Bump the FM player to keep the music going

bump_fm:
        tst.w   fm_idx
        beq     bump_exit2          /* VGM not playing */

        movem.l d0-d7/a0-a6,-(sp)
        tst.w   fm_smpl
        bne.b   1f                  /* waiting, check timer overflow */
0:
        /* process more VGM commands */
        bsr     fm_play
        tst.w   fm_idx
        beq     bump_exit           /* no longer playing */
        tst.w   fm_smpl
        beq.b   0b                  /* no delay */
        bra.b   2f                  /* new delay, set Timer A */
1:
        move.b  0xA04000,d0         /* YM2612 status */
        btst    #7,d0
        bne     bump_exit           /* busy */
        btst    #0,d0               /* Timer A overflow flag */
        beq     bump_exit           /* still waiting on Timer A */
        /* overflow */
        move.w  fm_tick,d0
        sub.w   fm_ttt,d0
        bne.b   3f                  /* longer delay, set Timer A */
        clr.w   fm_smpl             /* delay done, process commands */
        clr.w   fm_tick
        clr.w   fm_ttt
        bra.b   0b
2:
        /* set Timer A for delay */
        moveq   #0,d0
        move.w  fm_smpl,d0
        lsr.w   #2,d0
        add.w   fm_smpl,d0          /* ticks ~= 1.25 * # samples */
        bcc.b   3f
        ori.w   #0xFFFF,d0          /* saturate ticks to word */
3:
        move.w  d0,fm_tick
        cmpi.w  #1024,d0            /* max ticks for Timer A */
        bls.b   4f
        move.w  #1024,d0
4:
        move.w  d0,fm_ttt
        move.w  #1024,d1
        sub.w   d0,d1               /* Timer A count value */
        move.w  d1,d0
        andi.w  #3,d1
        lsr.w   #2,d0
        move.b  fm_csm,d2           /* timer control + CSM Mode */
        andi.b  #0xFE,d2
        move.b  #0x27,0xA04000
        nop
        nop
        nop
        move.b  d2,0xA04001         /* reset Timer A flag */
        nop
        nop
        nop
        move.b  #0x24,0xA04000
        nop
        nop
        nop
        move.b  d0,0xA04001         /* set Timer A msbs */
        nop
        nop
        nop
        move.b  #0x25,0xA04000
        nop
        nop
        nop
        move.b  d1,0xA04001         /* set Timer A lsbs */
        nop
        nop
        nop
        move.b  #0x27,0xA04000
        nop
        nop
        nop
        ori.b   #0x01,d2
        move.b  d2,0xA04001         /* enable Timer A, start Timer A */
bump_exit:
        movem.l (sp)+,d0-d7/a0-a6
bump_exit2:
        rts


| Vertical Blank handler

vert_blank:
        move.l  d1,-(sp)
        move.l  d2,-(sp)
        move.l  a2,-(sp)

        /* read controllers */
        move.w  0xA1512C,d0
        andi.w  #0xF000,d0
        cmpi.w  #0xF000,d0
        beq.b   0f               /* no pad in port 1 (or mouse) */
        lea     0xA10003,a0
        bsr     get_pad
        move.w  d2,0xA1512C      /* controller 1 current value */
0:
        tst.b   net_type
        bne.b   1f               /* networking enabled, ignore port 2 */
        move.w  0xA1512E,d0
        andi.w  #0xF000,d0
        cmpi.w  #0xF000,d0
        beq.b   1f               /* no pad in port 2 (or mouse) */
        lea     0xA10005,a0
        bsr     get_pad
        move.w  d2,0xA1512E     /* controller 2 current value */
1:
        /* if SCD present, generate IRQ 2 */
        tst.w   gen_lvl2
        beq.b   2f
        lea     0xA12000,a0
        move.w  (a0),d0
        ori.w   #0x0100,d0
        move.w  d0,(a0)
2:
        tst.b   hotplug_cnt
        beq.b   3f
        subq.b  #1,hotplug_cnt
3:
        move.w  init_vdp_latch,d0
        move.w  #-1,init_vdp_latch

        cmpi.w  #1,d0
        bne.b   4f                  /* re-init vdp and vram */

        bsr     init_vdp
        bsr     bump_fm
        bsr     load_font
        move.w  #0x8174,0xC00004    /* display on, vblank enabled, V28 mode */
4:
        move.l  (sp)+,a2
        move.l  (sp)+,d2
        move.l  (sp)+,d1
        movea.l (sp)+,a0
        move.l  (sp)+,d0
        rte

| get current pad value
| entry: a0 = pad control port
| exit:  d2 = pad value (0 0 0 1 M X Y Z S A C B R L D U) or (0 0 0 0 0 0 0 0 S A C B R L D U)
get_pad:
        bsr.b   get_input       /* - 0 s a 0 0 d u - 1 c b r l d u */
        move.w  d0,d1
        andi.w  #0x0C00,d0
        bne.b   no_pad
        bsr.b   get_input       /* - 0 s a 0 0 d u - 1 c b r l d u */
        bsr.b   get_input       /* - 0 s a 0 0 0 0 - 1 c b m x y z */
        move.w  d0,d2
        bsr.b   get_input       /* - 0 s a 1 1 1 1 - 1 c b r l d u */
        andi.w  #0x0F00,d0      /* 0 0 0 0 1 1 1 1 0 0 0 0 0 0 0 0 */
        cmpi.w  #0x0F00,d0
        beq.b   common          /* six button pad */
        move.w  #0x010F,d2      /* three button pad */
common:
        lsl.b   #4,d2           /* - 0 s a 0 0 0 0 m x y z 0 0 0 0 */
        lsl.w   #4,d2           /* 0 0 0 0 m x y z 0 0 0 0 0 0 0 0 */
        andi.w  #0x303F,d1      /* 0 0 s a 0 0 0 0 0 0 c b r l d u */
        move.b  d1,d2           /* 0 0 0 0 m x y z 0 0 c b r l d u */
        lsr.w   #6,d1           /* 0 0 0 0 0 0 0 0 s a 0 0 0 0 0 0 */
        or.w    d1,d2           /* 0 0 0 0 m x y z s a c b r l d u */
        eori.w  #0x1FFF,d2      /* 0 0 0 1 M X Y Z S A C B R L D U */
        rts

no_pad:
        move.w  #0xF000,d2
        rts

| read single phase from controller
get_input:
        move.b  #0x00,(a0)
        nop
        nop
        move.b  (a0),d0
        move.b  #0x40,(a0)
        lsl.w   #8,d0
        move.b  (a0),d0
        rts

| get current mouse value
| entry: a0 = mouse control port
| exit:  d0 = mouse value (0  0  0  0  0  0  0  0  YO XO YS XS S  M  R  L  X7 X6 X5 X4 X3 X2 X1 X0 Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0) or -2 (timeout) or -1 (no mouse)
get_mky:
        move.w  sr,d2
        move.w  #0x2700,sr      /* disable ints */

        move.b  #0x60,6(a0)     /* set direction bits */
        nop
        nop
        move.b  #0x60,(a0)      /* first phase of mouse packet */
        nop
        nop
0:
        btst    #4,(a0)
        beq.b   0b              /* wait on handshake */
        move.b  (a0),d0
        andi.b  #15,d0
        bne     mky_err         /* not 0 means not mouse */

        move.b  #0x20,(a0)      /* next phase */
        move.w  #254,d1         /* number retries before timeout */
1:
        btst    #4,(a0)
        bne.b   2f              /* handshake */
        dbra    d1,1b
        bra     timeout_err
2:
        move.b  (a0),d0
        andi.b  #15,d0
        move.b  #0,(a0)         /* next phase */
        cmpi.b  #11,d0
        bne     mky_err         /* not 11 means not mouse */
3:
        btst    #4,(a0)
        beq.b   4f              /* handshake */
        dbra    d1,3b
        bra     timeout_err
4:
        move.b  (a0),d0         /* specs say should be 15 */
        nop
        nop
        move.b  #0x20,(a0)      /* next phase */
        nop
        nop
5:
        btst    #4,(a0)
        bne.b   6f
        dbra    d1,5b
        bra     timeout_err
6:
        move.b  (a0),d0         /* specs say should be 15 */
        nop
        nop
        move.b  #0,(a0)         /* next phase */
        moveq   #0,d0           /* clear reg to hold packet */
        nop
7:
        btst    #4,(a0)
        beq.b   8f              /* handshake */
        dbra    d1,7b
        bra     timeout_err
8:
        move.b  (a0),d0         /* YO XO YS XS */
        move.b  #0x20,(a0)      /* next phase */
        lsl.w   #8,d0           /* save nibble */
9:
        btst    #4,(a0)
        bne.b   10f             /* handshake */
        dbra    d1,9b
        bra     timeout_err
10:
        move.b  (a0),d0         /* S  M  R  L */
        move.b  #0,(a0)         /* next phase */
        lsl.b   #4,d0           /* YO XO YS XS S  M  R  L  0  0  0  0 */
        lsl.l   #4,d0           /* YO XO YS XS S  M  R  L  0  0  0  0  0  0  0  0 */
11:
        btst    #4,(a0)
        beq.b   12f             /* handshake */
        dbra    d1,11b
        bra     timeout_err
12:
        move.b  (a0),d0         /* X7 X6 X5 X4 */
        move.b  #0x20,(a0)      /* next phase */
        lsl.b   #4,d0           /* YO XO YS XS S  M  R  L  X7 X6 X5 X4 0  0  0  0 */
        lsl.l   #4,d0           /* YO XO YS XS S  M  R  L  X7 X6 X5 X4 0  0  0  0  0  0  0  0 */
13:
        btst    #4,(a0)
        bne.b   14f             /* handshake */
        dbra    d1,13b
        bra     timeout_err
14:
        move.b  (a0),d0         /* X3 X2 X1 X0 */
        move.b  #0,(a0)         /* next phase */
        lsl.b   #4,d0           /* YO XO YS XS S  M  R  L  X7 X6 X5 X4 X3 X2 X1 X0 0  0  0  0 */
        lsl.l   #4,d0           /* YO XO YS XS S  M  R  L  X7 X6 X5 X4 X3 X2 X1 X0 0  0  0  0  0  0  0  0 */
15:
        btst    #4,(a0)
        beq.b   16f             /* handshake */
        dbra    d1,15b
        bra     timeout_err
16:
        move.b  (a0),d0         /* Y7 Y6 Y5 Y4 */
        move.b  #0x20,(a0)      /* next phase */
        lsl.b   #4,d0           /* YO XO YS XS S  M  R  L  X7 X6 X5 X4 X3 X2 X1 X0 Y7 Y6 Y5 Y4 0  0  0  0 */
        lsl.l   #4,d0           /* YO XO YS XS S  M  R  L  X7 X6 X5 X4 X3 X2 X1 X0 Y7 Y6 Y5 Y4 0  0  0  0  0  0  0  0*/
17:
        btst    #4,(a0)
        beq.b   18f             /* handshake */
        dbra    d1,17b
        bra     timeout_err
18:
        move.b  (a0),d0         /* Y3 Y2 Y1 Y0 */
        move.b  #0x60,(a0)      /* first phase */
        lsl.b   #4,d0           /* YO XO YS XS S  M  R  L  X7 X6 X5 X4 X3 X2 X1 X0 Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 0  0  0  0 */
        lsr.l   #4,d0           /* YO XO YS XS S  M  R  L  X7 X6 X5 X4 X3 X2 X1 X0 Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */
19:
        btst    #4,(a0)
        beq.b   19b             /* wait on handshake */

        move.w  d2,sr           /* restore int status */
        rts

timeout_err:
        move.b  #0x60,(a0)      /* first phase */
        nop
        nop
0:
        btst    #4,(a0)
        beq.b   0b              /* wait on handshake */

        move.w  d2,sr           /* restore int status */
        moveq   #-2,d0
        rts

mky_err:
        move.b  #0x40,6(a0)     /* set direction bits */
        nop
        nop
        move.b  #0x40,(a0)

        move.w  d2,sr           /* restore int status */
        moveq   #-1,d0
        rts


| get_port: returns ID bits of controller pointed to by a0 in d0
get_port:
        move.b  (a0),d0
        move.b  #0x00,(a0)
        moveq   #12,d1
        and.b   d0,d1
        sne     d2
        andi.b  #8,d2
        andi.b  #3,d0
        sne     d0
        andi.b  #4,d0
        or.b    d0,d2

        move.b  (a0),d0
        move.b  #0x40,(a0)
        moveq   #12,d1
        and.b   d0,d1
        sne     d1
        andi.b  #2,d1
        or.b    d1,d2
        andi.b  #3,d0
        sne     d0
        andi.b  #1,d0
        or.b    d0,d2

        move.w  #0xF001,d0
        cmpi.b  #3,d2
        beq.b   0f                       /* mouse in port */
        move.w  #0xF000,d0               /* no pad in port */
        cmpi.b  #0x0D,d2
        bne.b   0f
        moveq   #0,d0                    /* pad in port */
0:
        rts

| Check ports - sets controller word to 0xF001 if mouse, or 0 if not.
|   The next vblank will try to read a pad if 0, which will set the word
|   to 0xF000 if no pad found.
chk_ports:
        /* get ID port 1 */
        lea     0xA10003,a0
        bsr.b   get_port
        move.w  d0,0xA1512C             /* controller 1 */

        tst.b   net_type
        beq.b   0f                      /* ignore controller 2 when networking enabled */
        rts
0:
        /* get ID port 2 */
        lea     0xA10005,a0
        bsr.b   get_port
        move.w  d0,0xA1512E             /* controller 2 */
        rts


| Global variables for 68000

        .align  4

vblank:
        dc.l    0
extint:
        dc.l    0

        .global gen_lvl2
gen_lvl2:
        dc.w    0

        .global    use_cd
use_cd:
        dc.w    0

        .global cd_ok
cd_ok:
        dc.w    0
first_track:
        dc.w    0
last_track:
        dc.w    0
number_tracks:
        dc.w    0

        .global megasd_ok
megasd_ok:
        dc.w    0

        .global megasd_num_cdtracks
megasd_num_cdtracks:
        dc.w    0

        .global everdrive_ok
everdrive_ok:
        dc.w    0

init_vdp_latch:
        dc.w    -1

net_rbix:
        dc.w    0
net_wbix:
        dc.w    0
net_rdbuf:
        .space  32
net_link_timeout:
        dc.w    DEFAULT_LINK_TIMEOUT

    .global net_type
net_type:
        dc.b    0

hotplug_cnt:
        dc.b    0

        .align  4

fg_color:
        dc.l    0x11111111      /* default to color 1 for fg color */
bg_color:
        dc.l    0x00000000      /* default to color 0 for bg color */
crsr_x:
        dc.w    0
crsr_y:
        dc.w    0
dbug_color:
        dc.w    0


dbq_ix:
        dc.w    0

        .align  4
dbug_tmp:
        .space  64
dbq_id:
        .space  64
dbq_val:
        .space  64*4


        .text
        .align  4

dbug_list:
        dc.l    dbug_fpscount
        dc.l    dbug_lastticks
        dc.l    dbug_gameticks
        dc.l    dbug_bspmsec
        dc.l    dbug_segsmsec
        dc.l    dbug_segscount
        dc.l    dbug_planesmsec
        dc.l    dbug_planescount
        dc.l    dbug_spritesmsec
        dc.l    dbug_spritescount
        dc.l    dbug_refmsec
        dc.l    dbug_drawmsec

dbug_fpscount:
        dc.w    28,3
        dc.l    fpscount
dbug_lastticks:
        dc.w    28,4
        dc.l    lastticks
dbug_gameticks:
        dc.w    28,6
        dc.l    gameticks
dbug_bspmsec:
        dc.w    28,7
        dc.l    bspmsec
dbug_segsmsec:
        dc.w    28,8
        dc.l    segsmsec
dbug_segscount:
        dc.w    33,8
        dc.l    segscount
dbug_planesmsec:
        dc.w    28,9
        dc.l    planesmsec
dbug_planescount:
        dc.w    33,9
        dc.l    planescount
dbug_spritesmsec:
        dc.w    28,10
        dc.l    spritesmsec
dbug_spritescount:
        dc.w    33,10
        dc.l    spritescount
dbug_refmsec:
        dc.w    28,11
        dc.l    refmsec
dbug_drawmsec:
        dc.w    28,12
        dc.l    drawmsec

fpscount:
        .asciz  "fps:%2d"
        .align  2
lastticks:
        .asciz  "tcs:%2d"
        .align  2
gameticks:
        .asciz  "g:%2d"
        .align  2
bspmsec:
        .asciz  "b:%2d"
        .align  2
segsmsec:
        .asciz  "w:%2d"
        .align  2
segscount:
        .asciz  "%3d"
        .align  2
planesmsec:
        .asciz  "p:%2d"
        .align  2
planescount:
        .asciz  "%3d"
        .align  2
spritesmsec:
        .asciz  "s:%2d"
        .align  2
spritescount:
        .asciz  "%3d"
        .align  2
refmsec:
        .asciz  "r:%2d"
        .align  2
drawmsec:
        .asciz  "d:%2d"
        .align  2

        .align  4


        .bss
        .align  2
col_store:
        .space  21*224*2        /* 140 double-columns in vram, 20 in wram, 1 in wram for swap */

