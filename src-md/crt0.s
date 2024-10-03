| SEGA 32X support code for the 68000
| by Chilly Willy

        .text

        .equ DEFAULT_LINK_TIMEOUT, 0x3FFF

        /* Z80 local mem variable offsets */
        .equ FM_IDX,    0x40        /* music track index */
        .equ FM_CSM,    0x41        /* music CSM mode */
        .equ FM_SMPL,   0x42        /* music delay in samples (low) */
        .equ FM_SMPH,   0x43        /* music delay in samples (high) */
        .equ FM_TICL,   0x44        /* music timer ticks (low) */
        .equ FM_TICH,   0x45        /* music timer ticks (high) */
        .equ FM_TTTL,   0x46        /* music time to tick (low) */
        .equ FM_TTTH,   0x47        /* music time to tick (high) */
        .equ FM_BUFCSM, 0x48        /* music buffer reads requested */
        .equ FM_BUFGEN, 0x49        /* music buffer reads available */
        .equ FM_RQPND,  0x4A        /* 68000 request pending */
        .equ FM_RQARG,  0x4B        /* 68000 request arg */
        .equ FM_START,  0x4C        /* start offset in sram (for starting/looping vgm) */
        .equ CRITICAL,  0x4F        /* Z80 critical section flag */

        .equ REQ_ACT,   0xFFEF      /* Request 68000 Action */

        .equ STRM_KON,    0xFFF0
        .equ STRM_ID,     0xFFF1
        .equ STRM_CHIP,   0xFFF2
        .equ STRM_LMODE,  0xFFF3
        .equ STRM_SSIZE,  0xFFF4
        .equ STRM_SBASE,  0xFFF5
        .equ STRM_FREQH,  0xFFF6
        .equ STRM_FREQL,  0xFFF7
        .equ STRM_OFFHH,  0xFFF8
        .equ STRM_OFFHL,  0xFFF9
        .equ STRM_OFFLH,  0xFFFA
        .equ STRM_OFFLL,  0xFFFB
        .equ STRM_LENHH,  0xFFFC
        .equ STRM_LENHL,  0xFFFD
        .equ STRM_LENLH,  0xFFFE
        .equ STRM_LENLL,  0xFFFF

        .equ RF5C68_ENV,  0xFFE0
        .equ RF5C68_PAN,  0xFFE1
        .equ RF5C68_FSH,  0xFFE2
        .equ RF5C68_FSL,  0xFFE3
        .equ RF5C68_LAH,  0xFFE4
        .equ RF5C68_LAL,  0xFFE5
        .equ RF5C68_STA,  0xFFE6
        .equ RF5C68_CTL,  0xFFE7
        .equ RF5C68_CHN,  0xFFE8

        .equ STRM_ID,     0xFFF1
        .equ STRM_CHIP,   0xFFF2
        .equ STRM_LMODE,  0xFFF3
        .equ STRM_SSIZE,  0xFFF4
        .equ STRM_SBASE,  0xFFF5
        .equ STRM_FREQH,  0xFFF6
        .equ STRM_FREQL,  0xFFF7
        .equ STRM_OFFHH,  0xFFF8
        .equ STRM_OFFHL,  0xFFF9
        .equ STRM_OFFLH,  0xFFFA
        .equ STRM_OFFLL,  0xFFFB
        .equ STRM_LENHH,  0xFFFC
        .equ STRM_LENHL,  0xFFFD
        .equ STRM_LENLH,  0xFFFE
        .equ STRM_LENLL,  0xFFFF

        .equ MARS_FRAMEBUFFER, 0x840200 /* 32X frame buffer */
        .equ MARS_PWM_CTRL,    0xA15130
        .equ MARS_PWM_CYCLE,   0xA15132
        .equ MARS_PWM_MONO,    0xA15138

        .equ COL_STORE, 0x600800    /* WORD RAM + 2K */

        .macro  z80rd adr, dst
        move.b  0xA00000+\adr,\dst
        .endm

        .macro  z80wr adr, src
        move.b  \src,0xA00000+\adr
        .endm

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

        .macro  mute_pcm
        clr.w   pcm_env             /* mute PCM samples */
        clr.w   dac_len
        |tst.w   dac_len             /* drain the PCM buffer */
        |bne.w   main_loop
        |jsr     vgm_stop_samples
        .endm

        .macro  unmute_pcm
        move.w  #0xFFFF,pcm_env
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

|        lea     __stack,sp              /* set stack pointer to top of Work RAM */
        lea     0x00FFFFE0,sp           /* top of Work RAM - space for Z80 comm */

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

| init MD VDP
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
        move.w  #0x7FFF,d1              /* 32K - 1 words */
0:
        move.w  d0,(a1)                 /* clear VRAM */
        dbra    d1,0b

| The VDP state at this point is: Display disabled, ints disabled, Name Tbl A at 0xC000,
| Name Tbl B at 0xE000, Name Tbl W at 0xB000, Sprite Attr Tbl at 0xA800, HScroll Tbl at 0xAC00,
| H40 V28 mode, and Scroll size is 64x32.

| Clear CRAM
        move.l  #0x81048F01,(a0)        /* set reg 1 and reg 15 */
        move.l  #0xC0000000,(a0)        /* write CRAM address 0 */
        moveq   #31,d3
1:
        move.l  d0,(a1)
        dbra    d3,1b

| Clear VSRAM
        move.l  #0x40000010,(a0)         /* write VSRAM address 0 */
        moveq   #19,d4
2:
        move.l  d0,(a1)
        dbra    d4,2b

        jsr     load_font

| set the default palette for text
        move.l  #0xC0000000,(a0)        /* write CRAM address 0 */
        move.l  #0x00000CCC,(a1)        /* entry 0 (black) and 1 (lt gray) */
        move.l  #0xC0200000,(a0)        /* write CRAM address 32 */
        move.l  #0x000000A0,(a1)        /* entry 16 (black) and 17 (green) */
        move.l  #0xC0400000,(a0)        /* write CRAM address 64 */
        move.l  #0x0000000A,(a1)        /* entry 32 (black) and 33 (red) */

| init controllers
        jsr     chk_ports

| setup Z80 for FM music
        move.w  #0x0100,0xA11100        /* Z80 assert bus request */
        move.w  #0x0100,0xA11200        /* Z80 deassert reset */
3:
        move.w  0xA11100,d0
        andi.w  #0x0100,d0
        bne.b   3b                      /* wait for bus acquired */

        jsr     rst_ym2612              /* reset FM chip */

        moveq   #0,d0
        lea     0x00FFFFE0,a0           /* top of stack */
        moveq   #7,d1
4:
        move.l  d0,(a0)+                /* clear work ram used by FM driver */
        dbra    d1,4b

        lea     0xA00000,a0
        move.w  #0x1FFF,d1
5:
        move.b  d0,(a0)+                /* clear Z80 sram */
        dbra    d1,5b

        lea     z80_vgm_start,a0
        lea     z80_vgm_end,a1
        lea     0xA00000,a2
6:
        move.b  (a0)+,(a2)+             /* copy Z80 driver */
        cmpa.l  a0,a1
        bne.b   6b

        move.w  #0x0000,0xA11200        /* Z80 assert reset */

        move.w  #0x0000,0xA11100        /* Z80 deassert bus request */
7:
|        move.w  0xA11100,d0
|        andi.w  #0x0100,d0
|        beq.b   7b                      /* wait for bus released */

        move.w  #0x0100,0xA11200        /* Z80 deassert reset - run driver */

| wait on Mars side
        move.w  #0,0xA15104             /* set cart bank select */
        move.b  #0,0xA15107             /* clear RV - allow SH2 to access ROM */
8:
        cmp.l   #0x4D5F4F4B,0xA15120    /* M_OK */
        bne.b   8b                      /* wait for master ok */
9:
        cmp.l   #0x535F4F4B,0xA15124    /* S_OK */
        bne.b   9b                      /* wait for slave ok */

        move.l  #vert_blank,vblank      /* set vertical blank interrupt handler */
        move.w  #0x8174,0xC00004        /* display on, vblank enabled */
        move.w  #0x2000,sr              /* enable interrupts */
        rts


        .data

| Put remaining code in data section to lower bus contention for the rom.

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

        .global do_main
do_main:
| make sure save ram is disabled
        move.w  #0x2700,sr          /* disable ints */
        set_rv
        cmpi.w  #2,megasd_ok
        beq.b   1f
        tst.w   everdrive_ok
        bne.b   2f
| assume standard mapper save ram
        move.b  #2,0xA130F1         /* SRAM disabled, write protected */
        bra.b   3f
1:
        move.b  bnk7_save,0xA130FF  /* MegaSD => restore bank 7 */
        move.w  #0x0002,0xA130F0    /* standard mapping, sram disabled */
        bra.b   3f
2:
        move.w  #0x8000,0xA130F0    /* MED/MSD => map bank 0 to page 0, write disabled */
3:
        clr_rv
        move.w  #0x2000,sr          /* enable ints */

main_loop_start:
        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to MARS hw */
        move.l  #0,0xA15120         /* let Master SH2 run */

main_loop:
        tst.w   dac_len
        beq.b   main_loop_chk_ctrl  /* done/none, silence */

        move.l  dac_samples,a0
        move.w  dac_center,d1
        move.w  dac_len,d2
        move.w  pcm_env,d3
0:
        tst.w   MARS_PWM_MONO       /* check mono reg status */
        bmi.b   01f                 /* full */

        move.b  (a0)+,d0            /* fetch dac sample */
        eori.b  #0x80,d0            /* unsigned to signed */
        ext.w   d0                  /* sign extend to word */
|       add.w   d0,d0               /* *2 */
|       add.w   d0,d0               /* *4 */
        and.w   d3,d0
        add.w   d1,d0
        move.w  d0,MARS_PWM_MONO

        subq.w  #1,d2
        bne.b   0b                  /* not done */
        move.w  dac_center,MARS_PWM_MONO /* silence */
01:
        move.w  d2,dac_len
        move.l  a0,dac_samples

main_loop_chk_ctrl:
        tst.b   need_ctrl_int
        beq.b   main_loop_bump_fm
        move.b  #0,need_ctrl_int
        /* send controller values to primary sh2 */
        move.w  #0x0001,0xA15102    /* assert CMD INT to primary SH2 */
10:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0xA55A,d0
        bne.b   10b
        move.w  ctrl1_val,0xA15122  /* controller 1 value in COMM2 */
        move.w  #0xFF00,0xA15120
11:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0xFF01,d0
        bne.b   11b
        move.w  ctrl2_val,0xA15122  /* controller 2 value in COMM2 */
        move.w  #0xFF02,0xA15120
20:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0xA55A,d0
        bne.b   20b
        /* done */
        move.w  #0x5AA5,0xA15120
30:
        cmpi.w  #0x5AA5,0xA15120
        beq.b   30b

main_loop_bump_fm:
        move.b  REQ_ACT.w,d0
        cmpi.b  #0x03,d0
        beq.b   0f                 /* stream start */
        cmpi.b  #0x04,d0
        beq.b   0f                 /* stream stop */
        cmpi.b  #0x06,d0
        beq.b   0f                 /* RF5C68 register write */
        tst.b   need_bump_fm
        beq.b   main_loop_handle_req
0:
        move.b  #0,need_bump_fm
        bsr     bump_fm
        jsr     scd_flush_cmd_queue

main_loop_handle_req:
        moveq   #0,d0
        move.w  0xA15120,d0         /* get COMM0 */
        bne.b   handle_req

        move.w  0xA15124,d0         /* get COMM4 */
        bne.w   handle_sec_req

        /* check hot-plug count */
        tst.b   hotplug_cnt
        bne.w   main_loop
        move.b  #60,hotplug_cnt

        move.w  ctrl1_val,d0
        cmpi.w  #0xF001,d0
        beq.b   0f                  /* mouse in port 1, check port 2 */
        cmpi.w  #0xF000,d0
        beq.b   1f                  /* no pad in port 1, do hot-plug check */
0:
        tst.b   net_type
        bne.w   main_loop           /* networking enabled, ignore port 2 */
        move.w  ctrl2_val,d0
        cmpi.w  #0xF001,d0
        beq.w   main_loop           /* mouse in port 2, exit */
        cmpi.w  #0xF000,d0
        bne.w   main_loop           /* pad in port 2, exit */
1:
        bsr     chk_ports
        bra.w   main_loop

| process request from Master SH2
handle_req:
        moveq   #0,d1
        move.w  d0,-(sp)
        move.b  (sp)+,d1
        add.w   d1,d1
        move.w  prireqtbl(pc,d1.w),d1
        jmp     prireqtbl(pc,d1.w)
| unknown command
no_cmd:
        move.w  #0,0xA15120         /* done */
        bra     main_loop

        .align  2
 prireqtbl:
        dc.w    no_cmd - prireqtbl
        dc.w    read_sram - prireqtbl
        dc.w    write_sram - prireqtbl
        dc.w    start_music - prireqtbl
        dc.w    stop_music - prireqtbl
        dc.w    read_mouse - prireqtbl
        dc.w    read_cdstate - prireqtbl
        dc.w    set_usecd - prireqtbl
        dc.w    set_crsr - prireqtbl
        dc.w    get_crsr - prireqtbl
        dc.w    set_color - prireqtbl
        dc.w    get_color - prireqtbl
        dc.w    set_dbpal - prireqtbl
        dc.w    put_chr - prireqtbl
        dc.w    clear_a - prireqtbl
        dc.w    no_cmd - prireqtbl
        dc.w    no_cmd - prireqtbl
        dc.w    no_cmd - prireqtbl
        dc.w    net_getbyte - prireqtbl
        dc.w    net_putbyte - prireqtbl
        dc.w    net_setup - prireqtbl
        dc.w    net_cleanup - prireqtbl
        dc.w    set_bank_page - prireqtbl
        dc.w    net_set_link_timeout - prireqtbl
        dc.w    set_music_volume - prireqtbl
        dc.w    ctl_md_vdp - prireqtbl
        dc.w    cpy_md_vram - prireqtbl
        dc.w    cpy_md_vram - prireqtbl
        dc.w    cpy_md_vram - prireqtbl
        dc.w    load_sfx - prireqtbl
        dc.w    play_sfx - prireqtbl
        dc.w    get_sfx_status - prireqtbl
        dc.w    sfx_clear - prireqtbl
        dc.w    update_sfx - prireqtbl
        dc.w    stop_sfx - prireqtbl
        dc.w    flush_sfx - prireqtbl
        dc.w    store_bytes - prireqtbl
        dc.w    load_bytes - prireqtbl
        dc.w    open_cd_file_by_name - prireqtbl
        dc.w    open_cd_file_by_handle - prireqtbl
        dc.w    read_cd_file - prireqtbl
        dc.w    seek_cd_file - prireqtbl
        dc.w    load_sfx_cd_fileofs - prireqtbl
        dc.w    read_cd_directory - prireqtbl
        dc.w    resume_spcm_track - prireqtbl

| process request from Secondary SH2
handle_sec_req:
        cmpi.w  #0x1600,d0
        bls     main_loop
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
|        cmpi.w  #2,megasd_ok
|        beq.b   rd_msd_sram
        tst.w   everdrive_ok
        bne.w   rd_med_sram
| assume standard mapper save ram
        add.l   d0,d0
        lea     0x200000,a0         /* use standard mapper save ram base */
        sh2_wait
        set_rv
        move.b  #3,0xA130F1         /* SRAM enabled, write protected */
        move.b  1(a0,d0.l),d1       /* read SRAM from ODD bytes */
        move.b  #2,0xA130F1         /* SRAM disabled, write protected */
        bra.w   exit_rd_sram

rd_msd_sram:
        add.l   d0,d0
        lea     0x380000,a0         /* use the lower 32K of bank 7 */
        sh2_wait
        set_rv
        move.b  #0x80,0xA130FF      /* bank 7 is save ram */
        move.b  1(a0,d0.l),d1       /* read SRAM from ODD bytes */
        move.b  bnk7_save,0xA130FF  /* restore bank 7 */
        bra.w   exit_rd_sram

rd_med_sram:
        add.l   d0,d0
        tst.b   everdrive_ok
        bne.b   1f                  /* v1 or v2a MED */
        lea     0x040000,a0         /* use the upper 256K of page 31 */
        sh2_wait
        set_rv
        move.w  #0x801F,0xA130F0    /* map bank 0 to page 31, disable write */
        move.b  1(a0,d0.l),d1       /* read SRAM */
        move.w  #0x8000,0xA130F0    /* map bank 0 to page 0, disable write */
        bra.b   exit_rd_sram
1:
        lea     0x000000,a0         /* use the lower 32K of page 28 */
        sh2_wait
        set_rv
        move.w  #0x801C,0xA130F0    /* map bank 0 to page 28, disable write */
        move.b  1(a0,d0.l),d1       /* read SRAM */
        move.w  #0x8000,0xA130F0    /* map bank 0 to page 0, disable write */

exit_rd_sram:
        clr_rv
        sh2_cont
        move.w  d1,0xA15122         /* COMM2 holds return byte */
        move.w  #0,0xA15120         /* done */
        move.w  #0x2000,sr          /* enable ints */
        bra     main_loop

write_sram:
        move.w  #0x2700,sr          /* disable ints */
        moveq   #0,d1
        move.w  0xA15122,d1         /* COMM2 holds offset */
|        cmpi.w  #2,megasd_ok
|        beq.b   wr_msd_sram
        tst.w   everdrive_ok
        bne.w   wr_med_sram
| assume standard mapper save ram
        add.l   d1,d1
        lea     0x200000,a0         /* use standard mapper save ram base */
        sh2_wait
        set_rv
        move.b  #1,0xA130F1         /* SRAM enabled, write enabled */
        move.b  d0,1(a0,d1.l)       /* write SRAM to ODD bytes */
        move.b  #2,0xA130F1         /* SRAM disabled, write protected */
        bra.w   exit_wr_sram

wr_msd_sram:
        add.l   d1,d1
        lea     0x380000,a0         /* use the lower 32K of bank 7 */
        sh2_wait
        set_rv
        move.w  #0xA000,0xA130F0    /* enable write */
        move.b  #0x80,0xA130FF      /* bank 7 is save ram */
        move.b  d0,1(a0,d1.l)       /* write SRAM to ODD bytes */
        move.b  bnk7_save,0xA130FF  /* restore bank 7 */
        move.w  #0x8000,0xA130F0    /* disable write */
        bra.w   exit_wr_sram

wr_med_sram:
        add.l   d1,d1
        tst.b   everdrive_ok
        bne.b   1f                  /* v1 or v2a MED */
        lea     0x040000,a0         /* use the upper 256K of page 31 */
        sh2_wait
        set_rv
        move.w  #0xA01F,0xA130F0    /* map bank 0 to page 31, enable write */
        move.b  d0,1(a0,d1.l)       /* write SRAM */
        move.w  #0x8000,0xA130F0    /* map bank 0 to page 0, disable write */
        bra.b   exit_wr_sram
1:
        lea     0x000000,a0         /* use the lower 32K of page 28 */
        sh2_wait
        set_rv
        move.w  #0xA01C,0xA130F0    /* map bank 0 to page 28, enable write */
        move.b  d0,1(a0,d1.l)       /* write SRAM */
        move.w  #0x8000,0xA130F0    /* map bank 0 to page 0, disable write */

exit_wr_sram:
        clr_rv
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

        clr.l   vgm_ptr

        /* init VGM player */
        move.w  0xA15122,d0         /* COMM2 = index | repeat flag */
        move.w  #0x8000,d1
        and.w   d0,d1               /* repeat flag */
        eor.w   d1,d0               /* clear flag from index */
        move.w  d1,fm_rep           /* repeat flag */
        move.w  d0,fm_idx           /* index 1 to N */
        move.w  0xA15120,d0         /* COMM0 */

        /* fetch VGM offset */
        lea     vgm_ptr,a0
        move.l  0xA15128,(a0)       /* offset in COMM8 */
        /* fetch VGM length */
        lea     vgm_size,a0
        move.l  0xA1512C,(a0)       /* length in COMM12 */

        /* start VGM */
        btst    #0,d0               /* check if we read from CD */
        beq.b   0f

        move.w  0xA15100,d0
        eor.w   #0x8000,d0
        move.w  d0,0xA15100         /* unset FM - disallow SH2 access to FB */

        /* copy frame buffer string to local buffer */
        lea     MARS_FRAMEBUFFER,a0         /* frame buffer */
        lea     vgm_lzss_buf,a1
00:
        move.b  (a0)+,(a1)+
        bne.b   00b

        move.w  0xA15120,d0         /* COMM0 */
        btst    #1,d0               /* check if we read SPCM */
        beq.b   01f

        /* we read SPCM from CD */
        moveq   #0,d1
        tst.w   fm_rep
        beq.b   03f
        moveq   #1,d1
03:
        move.l  d1,-(sp)
        lea     vgm_lzss_buf,a1
        move.l  a1,-(sp)
        move.w  #0x2700,sr          /* disable ints */
        jsr     scd_play_spcm_track
        move.w  #0x2000,sr          /* enable ints */
        lea     8(sp),sp

        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        move.w  #0,0xA15120         /* done */

        bra     main_loop

01:
        /* CD VGM playback */
        move.l  0xA1512C,d1         /* length in COMM12 */
        move.l  d1,-(sp)
        move.l  0xA15128,d1         /* offset in COMM8 */
        move.l  d1,-(sp)

        /* we read VGM from CD */
        lea     vgm_lzss_buf,a1
        move.l  a1,-(sp)
        jsr     vgm_cache_scd
        lea     12(sp),sp
        move.l  d0,a1

        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        bra     02f

0:
        /* ROM VGM playback */
        move.w  #0,0xA15104         /* set cart bank select */
        move.l  #0,a0
        move.l  vgm_ptr,d0          /* set VGM offset */
        beq     9f

        move.l  d0,a0
        bsr     set_rom_bank
02:
        move.w  #0,0xA15120         /* done */

        move.l  a1,-(sp)            /* MD lump ptr */
        jsr     vgm_setup           /* setup lzss, set pcm_baseoffs, set vgm_ptr, read first block */
        lea     4(sp),sp

        /* start VGM on Z80 */
        move.w  #0x0100,0xA11100    /* Z80 assert bus request */
        move.w  #0x0100,0xA11200    /* Z80 deassert reset */
1:
        move.w  0xA11100,d0
        andi.w  #0x0100,d0
        bne.b   1b                  /* wait for bus acquired */

        jsr     rst_ym2612

        moveq   #0,d0
        lea     0x00FFFFE0,a0       /* top of stack */
        moveq   #7,d1
2:
        move.l  d0,(a0)+            /* clear work ram used by FM driver */
        dbra    d1,2b

        lea     z80_vgm_start,a0
        lea     z80_vgm_end,a1
        lea     0xA00000,a2
3:
        move.b  (a0)+,(a2)+         /* copy Z80 driver */
        cmpa.l  a0,a1
        bne.b   3b

| FM setup
        movea.l vgm_ptr,a6
        lea     8(a6),a6            /* version */
        move.b  (a6)+,d0
        move.b  (a6)+,d1
        move.b  (a6)+,d2
        move.b  (a6)+,d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d0,d3
        cmpi.l  #0x150,d3           /* >= v1.50? */
        bcs     2f                  /* no, default to song start of
offset 0x40 */

        movea.l vgm_ptr,a6
        lea     0x34(a6),a6         /* VGM data offset */
        move.b  (a6)+,d0
        move.b  (a6)+,d1
        move.b  (a6)+,d2
        move.b  (a6)+,d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d0,d3
        tst.l   d3
        beq.b   2f                  /* not set - use 0x40 */
        addi.l  #0x34,d3
        bra.b   3f
2:
        moveq   #0x40,d3
3:
        move.l  d3,fm_start         /* start of song data */

        movea.l vgm_ptr,a6
        lea     0x1C(a6),a6         /* loop offset */
| get vgm loop offset
        move.b  (a6)+,d0
        move.b  (a6)+,d1
        move.b  (a6)+,d2
        move.b  (a6)+,d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3            /* shift left 8 */
        move.b  d0,d3
        tst.l   d3
        beq.b   0f                  /* no loop offset, use default song start */
        addi.l  #0x1C,d3
        bra.b   1f
0:
        move.l  fm_start,d3
1:
        move.l  d3,fm_loop          /* loop offset */

        move.l  fm_start, d3

        unmute_pcm

        z80wr   FM_START,d3         /* start song => FX_START = start offset */
        lsr.w   #8,d3
        z80wr   FM_START+1,d3

        move.w  fm_idx,d0
        z80wr   FM_IDX,d0           /* set FM_IDX */

        jsr     vgm_read
        move.w  #6, preread_cnt     /* preread blocks to fill buffer */

        z80wr   FM_BUFCSM,#0
        z80wr   FM_BUFGEN,#2
        z80wr   FM_RQPND,#0

        movea.l vgm_ptr,a0
        lea     0xA01000,a1
        move.w  #1023,d1
4:
        move.b  (a0)+,(a1)+         /* copy vgm data from decompressor buffer to sram */
        dbra    d1,4b

        move.w  #0x0400,offs68k
        move.w  #0x0400,offsz80

        move.w  #0x0000,0xA11200    /* Z80 assert reset */
        move.w  #0x0000,0xA11100    /* Z80 deassert bus request */
        move.w  #0x0100,0xA11200    /* Z80 deassert reset - run driver */

        bra     main_loop
9:
        clr.w   fm_idx              /* not playing VGM */

        bra     main_loop

start_cd:
        tst.w   megasd_num_cdtracks
        beq     9f                   /* no MD+ tracks */

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
        tst.w   use_cd
        bne.w   stop_cd

        move.w  #0x2700,sr          /* disable ints */
        jsr     scd_stop_spcm_track
        move.w  #0x2000,sr          /* enable ints */

        /* stop VGM */
        move.w  #0x0100,0xA11100    /* Z80 assert bus request */
        move.w  #0x0100,0xA11200    /* Z80 deassert reset */
0:
        move.w  0xA11100,d0
        andi.w  #0x0100,d0
        bne.b   0b                  /* wait for bus acquired */

        jsr     rst_ym2612          /* reset FM chip */
        jsr     vgm_stop_samples

        moveq   #0,d0
        lea     0x00FFFFE0,a0       /* top of stack */
        moveq   #7,d1
1:
        move.l  d0,(a0)+            /* clear work ram used by FM driver */
        dbra    d1,1b

|        lea     z80_vgm_start,a0
|        lea     z80_vgm_end,a1
|        lea     0xA00000,a2
2:
|        move.b  (a0)+,(a2)+         /* copy Z80 driver */
|        cmpa.l  a0,a1
|        bne.b   2b

        z80wr   FM_IDX,#0           /* no music playing */
        z80wr   FM_RQPND,#0         /* no requests pending */
        z80wr   CRITICAL,#0         /* not in a critical section */

        move.w  #0x0000,0xA11200    /* Z80 assert reset */

        move.w  #0x0000,0xA11100    /* Z80 deassert bus request */
3:
|        move.w  0xA11100,d0
|        andi.w  #0x0100,d0
|        beq.b   3b                  /* wait for bus released */

        move.w  #0x0100,0xA11200    /* Z80 deassert reset - run driver */

        clr.w   fm_idx              /* stop music */
        clr.w   fm_rep
        clr.w   fm_stream

        move.w  #0,0xA15120         /* done */
        bra     main_loop

stop_cd:
        tst.w   megasd_num_cdtracks
        beq     3f                  /* no MD+ */

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

        move.w  ctrl1_val,d0
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
        move.w  ctrl2_val,d0
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
        move.w  #0xF000,ctrl1_val    /* nothing in port 1 */
        bra.b   no_mouse
no_mky2:
       move.w   #0xF000,ctrl2_val    /* nothing in port 2 */
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
        tst.w   megasd_num_cdtracks
        beq     9f                  /* no MD+ tracks */

        move.w  megasd_num_cdtracks,d0
        lsl.l   #2,d0

        move.w  megasd_ok,d1
        or.w    cd_ok,d1
        andi.w  #3,d1
        or.w    d1,d0

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
        dbra    d1,0b
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
        move.w  #0xF000,ctrl2_val    /* port 1 ID = SEGA_CTRL_NONE */
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
        move.w  #0xF000,ctrl2_val    /* port 1 ID = SEGA_CTRL_NONE */
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

set_bank_page:
        move.w  d0,d1
        andi.l  #0x07,d0            /* bank number */
        lsr.w   #3,d1               /* page number */
        andi.l  #0x1f,d1
        cmpi.b  #7,d0
        bne.b   1f
        move.b  d1,bnk7_save
1:
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
        cmpi.b  #7,d0
        bne.b   1f
        move.b  d1,bnk7_save
1:
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
        tst.w   megasd_num_cdtracks
        beq     1f                  /* no MD+ */

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

        move.w  0xA15100,d0
        eor.w   #0x8000,d0
        move.w  d0,0xA15100         /* unset FM - disallow SH2 access to FB */

        move.w  0xA15180,d0
        andi.w  #0xFF7F,d0
        move.w  d0,0xA15180         /* set MD priority */
        tst.b   d0
        bne.b   1f                  /* re-init vdp and vram */

|       move.w  #0x8134,0xC00004    /* display off, vblank enabled, V28 mode */
        move.w  0xA15180,d0
        ori.w   #0x0080,d0
        move.w  d0,0xA15180         /* set 32X priority */
1:
        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        move.w  #0,0xA15120         /* done */
        bra     main_loop

cpy_md_vram:
        mute_pcm

        move.w  0xA15100,d1
        eor.w   #0x8000,d1
        move.w  d1,0xA15100         /* unset FM - disallow SH2 access to FB */

        lea     0xA15120,a1         /* 32x comm0 port */

        moveq   #0,d1
        move.b  d0,d1               /* column number */
        add.w   d1,d1

        lea     MARS_FRAMEBUFFER,a2         /* frame buffer */
        lea     0(a2,d1.l),a2

        cmpi.w  #0x1C00,d0
        bhs.w   10f                 /* swap with vram */
        cmpi.w  #0x1B00,d0
        bhs.w   5f                  /* copy from vram */

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

        moveq   #0,d0
        move.l  d0,-(sp)
        jsr     scd_switch_to_bank
        lea     4(sp),sp

        lea     COL_STORE,a0
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

        unmute_pcm

        move.w  #0,0xA15120         /* done */
        bra     main_loop

5:
        moveq   #0,d0
        move.w  2(a1), d0

        andi.w  #0xFF,d0            /* column offset in words */
        add.l   d0,d0

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
        add.l   d0,d1

        #mulu.w  #160,d0
        lsl.l   #5,d0
        move    d0,d2
        lsl.l   #2,d2
        add.l   d2,d0

        lea     0(a2,d0.l),a2

        moveq   #0,d0
        move.l  d0,-(sp)
        jsr     scd_switch_to_bank
        lea     4(sp),sp

        lea     COL_STORE,a0
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

        unmute_pcm

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
        lea     col_swap,a1
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

        moveq   #0,d0
        move.l  d0,-(sp)
        jsr     scd_switch_to_bank
        lea     4(sp),sp

        lea     COL_STORE,a0
        lea     0(a0,d1.l),a0
        move.w  #27,d0
        lea     col_swap,a1
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

        unmute_pcm

        move.w  #0,0xA15120         /* done */
        bra     main_loop

load_sfx:
        /* fetch sample length */
        move.w  0xA15122,d2
        swap.w  d2
        move.w  #0,0xA15120         /* done with upper word */
20:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x1D02,d0
        bne.b   20b
        move.w  0xA15122,d2
        move.w  #0,0xA15120         /* done with lower word */
21:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x1D03,d0
        bne.b   21b

        /* fetch sample address */
        move.w  0xA15122,d1
        swap.w  d1
        move.w  #0,0xA15120         /* done with upper word */
22:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x1D04,d0
        bne.b   22b
        move.w  0xA15122,d1
        move.w  #0,0xA15120         /* done with lower word */

23:
        move.w  0xA15120,d0         /* wait on handshake in COMM0 */
        cmpi.w  #0x1D00,d0
        bne.b   23b

        /* set buffer id */
        moveq   #0,d0
        move.w  0xA15122,d0         /* COMM2 = buffer id */

        move.l  d1,a0
        bsr     set_rom_bank
        move.l  a1,d1

        move.l  d2,-(sp)
        move.l  d1,-(sp)
        move.l  d0,-(sp)

        move.w  #0,0xA15120         /* done */

        jsr     scd_upload_buf
        lea     12(sp),sp

        bra     main_loop

play_sfx:
        /* set source id */
        moveq   #0,d0
        moveq   #0,d1
        moveq   #0,d2

        move.b  0xA15121,d0         /* LB of COMM0 = src id */
        move.w  0xA15122,d2         /* pan|vol */
        move.w  0xA15128,d1         /* freq */

        move.w  d2,d3
        andi.l  #255,d3
        lsr.w   #8,d2

        move.l  #0,-(sp)            /* autoloop */
        move.l  d3,-(sp)            /* vol */
        move.l  d2,-(sp)            /* pan */ 
        move.l  d1,-(sp)
        move.w  0xA1512A,d1         /* buf_id */
        move.l  d1,-(sp)
        move.l  d0,-(sp)            /* src id */

        move.w  #0,0xA15120         /* done */

        jsr     scd_queue_play_src
        lea     24(sp),sp

        bra     main_loop

get_sfx_status:
        jsr     scd_get_playback_status

        move.w  d0,0xA15122         /* state */
        move.w  #0,0xA15120         /* done */

        bra     main_loop

sfx_clear:
        jsr     scd_queue_clear_pcm

        move.w  #0,0xA15120         /* done */

        bra     main_loop

update_sfx:
        /* set source id */
        moveq   #0,d0
        moveq   #0,d2

        move.b  0xA15121,d0         /* LB of COMM0 = src id */
        move.w  0xA15122,d2         /* pan|vol */
        move.w  0xA15128,d1         /* freq */

        move.w  d2,d3
        andi.l  #255,d3
        lsr.w   #8,d2

        move.l  #0,-(sp)            /* autoloop */
        move.l  d3,-(sp)            /* vol */
        move.l  d2,-(sp)            /* pan */ 
        move.l  d1,-(sp)            /* freq */
        move.l  d0,-(sp)            /* src id */

        move.w  #0,0xA15120         /* done */

        jsr     scd_queue_update_src
        lea     20(sp),sp

        bra     main_loop

stop_sfx:
        /* set source id */
        moveq   #0,d0
        move.b  0xA15121,d0         /* LB of COMM0 = src id */

        move.w  #0,0xA15120         /* done */

        move.l  d0,-(sp)            /* src id */
        jsr     scd_queue_stop_src
        lea     4(sp),sp

        bra     main_loop

flush_sfx:
        move.w  #0,0xA15120         /* done */

        jsr     scd_flush_cmd_queue

        move.b  #1,need_bump_fm
        bra     main_loop

store_bytes:
        moveq   #0,d1
        move.w  0xA15122,d1         /* COMM2 = length */

        lea     MARS_FRAMEBUFFER,a2
        lea     aux_store,a1
        
        move.l  d1,-(sp)            /* length */
        move.l  a2,-(sp)            /* source */
        move.l  a1,-(sp)            /* destination */

        move.w  0xA15100,d0
        eor.w   #0x8000,d0
        move.w  d0,0xA15100         /* unset FM - disallow SH2 access to FB */

        jsr     memcpy
        lea     12(sp),sp           /* clear the stack */

        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        move.w  #0,0xA15120         /* done */
        bra     main_loop

load_bytes:
        moveq   #0,d1
        move.w  0xA15122,d1         /* COMM2 = length */

        lea     MARS_FRAMEBUFFER,a1
        lea     aux_store,a2

        move.l  d1,-(sp)            /* length */
        move.l  a2,-(sp)            /* source */
        move.l  a1,-(sp)            /* destination */

        move.w  0xA15100,d0
        eor.w   #0x8000,d0
        move.w  d0,0xA15100         /* unset FM - disallow SH2 access to FB */

        jsr     memcpy
        lea     12(sp),sp           /* clear the stack */

        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        move.w  #0,0xA15120         /* done */
        bra     main_loop

open_cd_file_by_name:
        mute_pcm
        jsr vgm_stop_samples

        move.w  0xA15100,d0
        eor.w   #0x8000,d0
        move.w  d0,0xA15100         /* unset FM - disallow SH2 access to FB */

        lea     MARS_FRAMEBUFFER,a1 /* frame buffer */
        move.l  a1,-(sp)            /* string pointer */
        jsr     scd_open_gfile_by_name
        lea     4(sp),sp            /* clear the stack */
        move.l  d0,0xA15128         /* length => COMM8 */
        move.l  d1,0xA1512C         /* offset => COMM12 */

        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        unmute_pcm

        move.w  #0,0xA15120         /* done */
        bra     main_loop

open_cd_file_by_handle:
        move.l  0xA15128,d0         /* length => COMM8 */
        move.l  0xA1512C,d1         /* offset => COMM12 */

        move.l  d1,-(sp)
        move.l  d0,-(sp)
        jsr     scd_open_gfile_by_length_offset
        lea     8(sp),sp            /* clear the stack */

        move.w  #0,0xA15120         /* done */
        bra     main_loop

read_cd_file:
        mute_pcm
        jsr vgm_stop_samples

        move.w  0xA15100,d0
        eor.w   #0x8000,d0
        move.w  d0,0xA15100         /* unset FM - disallow SH2 access to FB */

        move.l  0xA15128,d0         /* length in COMM8 */
        move.l  d0,-(sp)
        lea     MARS_FRAMEBUFFER,a1
        move.l  a1,-(sp)            /* destination pointer */
        jsr     scd_read_gfile
        lea     8(sp),sp            /* clear the stack */
        move.l  d0,0xA15128         /* length => COMM8 */

        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        unmute_pcm

        move.w  #0,0xA15120         /* done */
        bra     main_loop

seek_cd_file:
        moveq   #0,d0
        move.w  0xA15122,d0         /* whence in COMM2 */
        move.l  d0,-(sp)
        move.l  0xA15128,d0         /* offset in COMM8 */
        move.l  d0,-(sp)
        jsr     scd_seek_gfile
        lea     8(sp),sp            /* clear the stack */
        move.l  d0,0xA15128         /* position => COMM8 */

        move.w  #0,0xA15120         /* done */
        bra     main_loop

load_sfx_cd_fileofs:
        lea     MARS_FRAMEBUFFER,a1 /* frame buffer */
        move.l  a1,-(sp)            /* file name + offsets */

        moveq   #0,d0
        move.b  0xA15121,d0         /* LSB of COMM0 = num sfx */
        move.l  d0,-(sp)

        /* set buffer id */
        move.w  0xA15122,d0         /* COMM2 = start buffer id */
        move.l  d0,-(sp)

        move.w  0xA15100,d0
        eor.w   #0x8000,d0
        move.w  d0,0xA15100         /* unset FM - disallow SH2 access to FB */

        jsr     scd_upload_buf_fileofs
        lea     16(sp),sp

        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        move.w  #0,0xA15120         /* done */

        bra     main_loop

read_cd_directory:
        move.w  0xA15100,d0
        eor.w   #0x8000,d0
        move.w  d0,0xA15100         /* unset FM - disallow SH2 access to FB */

        lea     MARS_FRAMEBUFFER,a1
        move.l  a1,-(sp)            /* path and destination buffer */
        jsr     scd_read_directory
        lea     4(sp),sp            /* clear the stack */
        move.l  d0,0xA15128         /* length => COMM8 */
        move.l  d1,0xA1512C         /* num entries(result) => COMM12 */

        move.w  0xA15100,d0
        or.w    #0x8000,d0
        move.w  d0,0xA15100         /* set FM - allow SH2 access to FB */

        move.w  #0,0xA15120         /* done */
        bra     main_loop

resume_spcm_track:
        move.w  #0x2700,sr          /* disable ints */
        jsr     scd_resume_spcm_track
        move.w  #0x2000,sr          /* enable ints */
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
|        lea     font_data,a2
        move.w  #0x6B*8-1,d2
|0:
        move.l  #0,d0
|        move.l  (a2)+,d0            /* font fg mask */
        move.l  d0,d1
        not.l   d1                  /* font bg mask */
        and.l   fg_color,d0         /* set font fg color */
        and.l   bg_color,d1         /* set font bg color */
        or.l    d1,d0
        move.l  d0,(a1)             /* set tile line */
        dbra    d2,0b
        rts

| Bump the FM player to keep the music going

        .global bump_fm
bump_fm:
        tst.w   fm_idx
        beq.b   999f

        move.w  sr,-(sp)
        move.w  #0x2700,sr          /* disable ints */
        tst.b   REQ_ACT.w
        bmi.b   99f                 /* sram locked */
        bne.b   0f                  /* Z80 requesting action */
        tst.w   preread_cnt
        bne.b   0f                  /* buffer preread pending */
99:
        move.w  (sp)+,sr            /* restore int level */
999:
        rts

0:
        movem.l d0-d7/a0-a6,-(sp)
        move.w  #0x0100,0xA11100    /* Z80 assert bus request */
        move.w  #0x0100,0xA11200    /* Z80 deassert reset */
1:
        move.w  0xA11100,d0
        andi.w  #0x0100,d0
        bne.b   1b                  /* wait for bus acquired */

        z80rd   CRITICAL,d0
        bne     8f                  /* z80 critical section, just exit */

10:
        move.b  REQ_ACT.w,d0
        cmpi.b  #0x01,d0
        bne.w   5f                  /* not read buffer block */
11:
        /* check for space in Z80 sram buffer */
        moveq   #0,d0
        z80rd   FM_BUFGEN,d0
        moveq   #0,d1
        z80rd   FM_BUFCSM,d1
        sub.w   d1,d0
        bge.b   111f
        addi.w  #256,d0             /* how far ahead the decompressor is */
111:
        cmpi.w  #8,d0
        bge.w   7f                  /* no space in circular buffer */
        moveq   #8,d1
        sub.w   d0,d1
        ble.w   7f                  /* sanity check */
        subq.w  #1,d1
        move.w  d1,preread_cnt      /* try to fill buffer */

        /* read a buffer block */
        jsr     vgm_read
        move.w  d0,d4
        move.w  offs68k,d2
        move.w  offsz80,d3
        cmpi.w  #0,d4
        beq.b   13f                 /* EOF with no more data */

        /* read returned some or all requested data */
        z80rd   FM_BUFGEN,d0
        addq.b  #1,d0
        z80wr   FM_BUFGEN,d0

        /* copy buffer to z80 sram */
        movea.l vgm_ptr,a3
        lea     0xA01000,a4
        move.w  d4,d1
        subi.w  #1,d1
        lea     0(a3,d2.w),a3
        lea     0(a4,d3.w),a4
12:
        move.b  (a3)+,(a4)+
        dbra    d1,12b

        addi.w  #512,d2
        addi.w  #512,d3
        andi.w  #0x7FFF,d2          /* 32KB buffer */
        andi.w  #0x0FFF,d3          /* 4KB buffer */

        cmpi.w  #512,d4
        beq.b   14f                 /* got entire block, not EOF */
13:
        /* EOF reached */
        move.l  fm_loop,d0
        andi.w  #0x01FF,d0          /* 1 read buffer */
        add.w   d3,d0
        andi.w  #0x0FFF,d0          /* 4KB buffer */
        move.w  d0,fm_loop2         /* offset to z80 loop start */

        /* reset */
        jsr     vgm_reset           /* restart at start of compressed data */
        move.l  fm_loop,d2
        andi.l  #0xFFFFFE00,d2
        beq.b   14f
        move.l  d2,-(sp)
        jsr     vgm_read2
        addq.l  #4,sp
        move.w  d0,d2
        andi.w  #0x7FFF,d2          /* amount data pre-read (with wrap around) */
14:
        move.w  d2,offs68k
        move.w  d3,offsz80
        move.w  #7,preread_cnt      /* refill buffer if room */
        bra.w   7f

5:
        cmpi.b  #0x03,d0
        beq.b   51f                 /* stream start */
        cmpi.b  #0x04,d0
        beq.b   51f                 /* stream stop */
        cmpi.b  #0x06,d0
        beq.b   52f                 /* rf5c68 */

        cmpi.b  #0x02,d0
        bne.w   6f                  /* unknown request, ignore */

        /* music ended, check for loop */
        tst.w   fm_rep
        bne.b   50f                 /* repeat, loop vgm */
        /* no repeat, reset FM */
        jsr     rst_ym2612
        bra.w   7f

50:
        move.w  fm_loop2,d0
        z80wr   FM_START,d0
        lsr.w   #8,d0
        z80wr   FM_START+1,d0       /* music start = loop offset */
        move.w  fm_idx,d0
        z80wr   FM_IDX,d0           /* set FM_IDX to start music */
        bra.w   11b                 /* try to load a block */

51:
        move.b  d0,fm_stream
        move.w  STRM_FREQH.w,fm_stream_freq
        move.l  STRM_OFFHH.w,fm_stream_ofs
        move.l  STRM_LENHH.w,fm_stream_len
        bra.b   7f

52:
        move.b  d0,fm_stream
        move.w  RF5C68_FSH.w,rf5c68_freq_incr
        move.b  RF5C68_STA.w,rf5c68_start
        move.w  RF5C68_LAH.w,rf5c68_loop_ofs
        move.b  RF5C68_ENV.w,rf5c68_envelope
        move.b  RF5C68_CHN.w,rf5c68_chanmask
        move.b  RF5C68_CTL.w,rf5c68_chanctl
        move.b  RF5C68_PAN.w,rf5c68_pan
        bra.b   7f

6:
        /* check if need to preread a block */
        tst.w   preread_cnt
        beq.w   7f
        subq.w  #1,preread_cnt

        /* read a buffer */
        bra.w   11b

7:
        z80wr   FM_RQPND,#0         /* request handled */
        move.b  #0,REQ_ACT.w
8:
        move.w  #0x0000,0xA11100    /* Z80 deassert bus request */
9:
        movem.l (sp)+,d0-d7/a0-a6

        /* handle PCM data stream */
        tst.w   fm_stream
        beq.b   18f

        cmpi.b  #0x06,fm_stream
        beq.b   19f

        btst    #0,fm_stream
        beq.b   17f
16:
        /* set the stream pointer and start playback on the SegaCD */
        clr.w   fm_stream

        tst.w   pcm_env
        beq.b   18f                 /* PCM muted */

        moveq   #0,d1
        move.w  fm_stream_freq,d1
        move.l  d1,-(sp)
        move.l  fm_stream_len,-(sp)
        move.l  fm_stream_ofs,-(sp)
        jsr     vgm_play_dac_samples
        lea     12(sp),sp
        bra.b   18f
17:
        /* stop playback */
        clr.w   fm_stream
        jsr     vgm_stop_samples
18:
        move.w  (sp)+,sr            /* restore int level */
        rts
19:
        clr.w   fm_stream

        cmpi.b  #0xFF,rf5c68_chanmask /* 0xFF -- all channels disabled */
        beq.b   20f

        tst.w   pcm_env
        beq.b   20f                 /* PCM muted */

        moveq   #0,d1
        move.b  rf5c68_envelope,d1
        swap.w  d1
        move.b  rf5c68_pan,d1
        move.l  d1,-(sp)
        moveq   #0,d1
        move.w  rf5c68_freq_incr,d1
        move.l  d1,-(sp)
        move.w  rf5c68_loop_ofs,d1
        move.l  d1,-(sp)
        move.w  rf5c68_start,d1
        move.l  d1,-(sp)
        moveq   #0,d1
        move.b  rf5c68_chanctl,d1
        andi.b  #0x7,d1
        move.l  d1,-(sp)
        jsr     vgm_play_rf5c68_samples
        lea     20(sp),sp
20:
        move.w  (sp)+,sr            /* restore int level */
        rts

| reset YM2612
rst_ym2612:
        lea     FMReset,a1
        lea     0xA00000,a0
        moveq   #6,d3
0:
        move.b  (a1)+,d0                /* FM reg */
        move.b  (a1)+,d1                /* FM data */
        moveq   #15,d2
1:
        move.b  d0,0x4000(a0)           /* FM reg */
        nop
        nop
        nop
        move.b  d1,0x4001(a0)           /* FM data */
        nop
        nop
        nop
        move.b  d0,0x4002(a0)           /* FM reg */
        nop
        nop
        nop
        move.b  d1,0x4003(a0)           /* FM data */
        addq.b  #1,d0
        dbra    d2,1b
        dbra    d3,0b

        move.w  #0x4000,d1
        moveq   #28,d2
2:
        move.b  (a1)+,d1                /* YM reg */
        move.b  (a1)+,0(a0,d1.w)        /* YM data */
        dbra    d2,2b

| reset PSG
        move.b  #0x9F,0xC00011
        move.b  #0xBF,0xC00011
        move.b  #0xDF,0xC00011
        move.b  #0xFF,0xC00011
        rts


| Vertical Blank handler

vert_blank:
        move.l  d1,-(sp)
        move.l  d2,-(sp)
        move.l  a2,-(sp)

        move.b  #1,need_bump_fm
        move.b  #1,need_ctrl_int

        /* read controllers */
        move.w  ctrl1_val,d0
        andi.w  #0xF000,d0
        cmpi.w  #0xF000,d0
        beq.b   0f               /* no pad in port 1 (or mouse) */
        lea     0xA10003,a0
        bsr     get_pad
        move.w  d2,ctrl1_val      /* controller 1 current value */
0:
        tst.b   net_type
        bne.b   1f               /* networking enabled, ignore port 2 */
        move.w  ctrl2_val,d0
        andi.w  #0xF000,d0
        cmpi.w  #0xF000,d0
        beq.b   1f               /* no pad in port 2 (or mouse) */
        lea     0xA10005,a0
        bsr     get_pad
        move.w  d2,ctrl2_val     /* controller 2 current value */
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

        bsr     bump_fm
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
        move.w  d0,ctrl1_val             /* controller 1 */

        tst.b   net_type
        beq.b   0f                      /* ignore controller 2 when networking enabled */
        rts
0:
        /* get ID port 2 */
        lea     0xA10005,a0
        bsr.b   get_port
        move.w  d0,ctrl2_val             /* controller 2 */
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

ctrl1_val:
        dc.w    0
ctrl2_val:
        dc.w    0

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

need_bump_fm:
        dc.b    1
need_ctrl_int:
        dc.b    0

bnk7_save:
        dc.b    7

        .align  4

        .global    fm_start
fm_start:
        dc.l    0
        .global    fm_loop
fm_loop:
        dc.l    0
        .global pcm_baseoffs
pcm_baseoffs:
        dc.l    0
        .global    vgm_ptr
vgm_ptr:
        dc.l    0
        .global    vgm_size
vgm_size:
        dc.l    0
        .global    fm_rep
dac_samples:
        .global    dac_samples
        dc.l    0
fm_rep:
        dc.w    0
        .global    fm_idx
fm_idx:
        dc.w    0
fm_stream:
        dc.w    0
fm_stream_freq:
        dc.w    0
fm_loop2:
        dc.w    0

dac_freq:
        .global    dac_freq
        dc.w    0
dac_len:
        .global    dac_len
        dc.w    0
dac_center:
        .global    dac_center
        dc.w    1
pcm_env:
        .global pcm_env
        dc.w    0xFFFF

rf5c68_start:
        dc.w    0
rf5c68_freq_incr:
        dc.w    0
rf5c68_loop_ofs:
        dc.w    0
rf5c68_envelope:
        dc.b    0
rf5c68_chanmask:
        dc.b    255
rf5c68_pan:
        dc.b    255

        .align  2
offs68k:
        dc.w    0
offsz80:
        dc.w    0
preread_cnt:
        dc.w    0

        .align  4

fm_stream_ofs:
        dc.l    0
fm_stream_len:
        dc.l    0

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
rf5c68_chanctl:
        dc.b    0

        .text
        .align  4

FMReset:
        /* block settings */
        .byte   0x30,0x00
        .byte   0x40,0x7F
        .byte   0x50,0x1F
        .byte   0x60,0x1F
        .byte   0x70,0x1F
        .byte   0x80,0xFF
        .byte   0x90,0x00

        /* disable LFO */
        .byte   0,0x22
        .byte   1,0x00
        /* disable timers & set channel 6 to normal mode */
        .byte   0,0x27
        .byte   1,0x00
        /* all KEY_OFF */
        .byte   0,0x28
        .byte   1,0x00
        .byte   1,0x04
        .byte   1,0x01
        .byte   1,0x05
        .byte   1,0x02
        .byte   1,0x06
        /* disable DAC */
        .byte   0,0x2B
        .byte   1,0x80
        .byte   0,0x2A
        .byte   1,0x80
        .byte   0,0x2B
        .byte   1,0x00
        /* turn off channels */
        .byte   0,0xB4
        .byte   1,0x00
        .byte   0,0xB5
        .byte   1,0x00
        .byte   0,0xB6
        .byte   1,0x00
        .byte   2,0xB4
        .byte   3,0x00
        .byte   2,0xB5
        .byte   3,0x00
        .byte   2,0xB6
        .byte   3,0x00

        .align  4

        .bss
        .align  2
col_swap:
        .space  1*224*2         /* 1 double-column in wram for swap */

        .align  16
aux_store:
        .global aux_store
        .space  16*1024
