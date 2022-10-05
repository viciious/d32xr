| SEGA 32X VGM Player
| by Chilly Willy
|
| uses work ram buffer to hold decompressed vgm commands
| pcm data is separate from the vgm commands

        .equ    buf_size,0x8000    /* must match LZSS_BUF_SIZE */
        .equ    vgm_ahead,0x200    /* must match VGM_READAHEAD */

        .macro  fetch_vgm reg
        cmpa.l  a3,a6           /* check for wrap around */
        bcs.b   9f
        suba.l  a2,a6           /* wrap */
        subi.l  #buf_size,vgm_chk
        jsr     bump_vgm
9:
        move.b  (a6)+,\reg
        .endm

        .macro  fetch_pcm reg
        cmpa.w  #0,a5
        beq.b   8f
        move.b  (a5)+,\reg
        bra.b   9f
8:
        move.b  #0x80,\reg      /* silence if pcm ptr isn't set */
9:
        .endm

        .data

        .align  2

        .global fm_init
fm_init:
| reset YM2612
        lea     FMReset(pc),a1
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

        .global fm_newvgmptr
fm_newvgmptr:
        movea.l vgm_ptr,a6      /* decompress buffer */
        move.l  a6,vgm_chk
        addi.l  #vgm_ahead,vgm_chk    /* vgm_setup reads this for us */
        move.l  a6,fm_cur       /* current vgm ptr */
        lea     buf_size,a2     /* wrap length for buffer */
        lea     0(a6,a2.l),a3   /* buffer limit */
        clr.b   pcm_en          /* pcm dac not yet enabled */
        rts

        .global fm_setup
fm_setup:
        tst.l   fm_ptr
        beq     stop            /* no vgm pointer */

        tst.l   pcm_baseoffs
        beq.b   4f

        move.l  vgm_ptr,d3
        move    d3,d0
        add.l   pcm_baseoffs,d0
        move.l  d0,vgm_ptr
        jsr     fm_newvgmptr
        bsr     fm_play
        move.l  d3,vgm_ptr

4:
        jsr     fm_newvgmptr

        lea     0x1C(a6),a6     /* loop offset */
        fetch_vgm d0
        fetch_vgm d1
        fetch_vgm d2
        fetch_vgm d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d0,d3
        tst.l   d3
        beq.b   0f              /* no loop offset, use default song start */
        addi.l  #0x1C,d3
        bra.b   1f
0:
        moveq   #0x40,d3
1:
        move.l  d3,fm_loop      /* loop offset */

        movea.l fm_cur,a6
        lea     8(a6),a6        /* version */
        fetch_vgm d0
        fetch_vgm d1
        fetch_vgm d2
        fetch_vgm d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d0,d3
        cmpi.l  #0x150,d3       /* >= v1.50? */
        bcs     2f              /* no, default to song start of offset 0x40 */

        movea.l fm_cur,a6
        lea     0x34(a6),a6     /* VGM data offset */
        fetch_vgm d0
        fetch_vgm d1
        fetch_vgm d2
        fetch_vgm d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d0,d3
        tst.l   d3
        beq.b   2f              /* not set - use 0x40 */
        addi.l  #0x34,d3
        bra.b   3f
2:
        moveq   #0x40,d3
3:
        add.l   d3,fm_cur       /* start of song data */
        move.b  #0x15,fm_csm
        move.l  pcm_base,pcm_cur /* start of pcm data */
        rts


| main player entry point - process next VGM command

        .global fm_play
fm_play:
        lea     buf_size,a2     /* wrap length for buffer */
        movea.l vgm_ptr,a3
        adda.l  a2,a3           /* buffer limit */
        movea.l fm_cur,a6       /* vgm ptr */

        move.l  vgm_chk,d0
        sub.l   a6,d0
        cmpi.l  #vgm_ahead,d0
        bgt.b   0f
        jsr     bump_vgm
0:
        moveq   #0,d0
        fetch_vgm d0            /* Read one byte from the VGM data */

        add.w   d0,d0
        move.w  cmd_table(pc,d0.w),d1
        jmp     cmd_table(pc,d1.w)      /* dispatch command */

cmd_table:
        /* 00 - 0F */
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        /* 10 - 1F */
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        /* 20 - 2F */
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        .word   reserved_0 - cmd_table
        /* 30 - 3F */
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        /* 40 - 4F */
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table
        .word   reserved_1 - cmd_table /* Game Gear PSG stereo, write dd to port 0x06 */
        /* 50 - 5F */
        .word   write_psg - cmd_table  /* PSG (SN76489/SN76496) write value dd */
        .word   reserved_2 - cmd_table /* YM2413, write value dd to register aa */
        .word   write_fm0 - cmd_table  /* YM2612 port 0, write value dd to register aa */
        .word   write_fm1 - cmd_table  /* YM2612 port 1, write value dd to register aa */
        .word   reserved_2 - cmd_table /* YM2151, write value dd to register aa */
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        /* 60 - 6F */
        .word   reserved_2 - cmd_table
        .word   wait_nnnn - cmd_table  /* Wait n samples, n can range from 0 to 65535 */
        .word   wait_735 - cmd_table   /* Wait 735 samples (60th of a second) */
        .word   wait_882 - cmd_table   /* Wait 882 samples (50th of a second) */
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   end_data - cmd_table   /* end of sound data */
        .word   data_block - cmd_table /* data block: 0x67 0x66 tt ss ss ss ss (data) */
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        /* 70 - 7F wait n+1 samples */
        .word   wait_1 - cmd_table
        .word   wait_2 - cmd_table
        .word   wait_3 - cmd_table
        .word   wait_4 - cmd_table
        .word   wait_5 - cmd_table
        .word   wait_6 - cmd_table
        .word   wait_7 - cmd_table
        .word   wait_8 - cmd_table
        .word   wait_9 - cmd_table
        .word   wait_10 - cmd_table
        .word   wait_11 - cmd_table
        .word   wait_12 - cmd_table
        .word   wait_13 - cmd_table
        .word   wait_14 - cmd_table
        .word   wait_15 - cmd_table
        .word   wait_16 - cmd_table
        /* 80 - 8F YM2612 port 0 address 2A write from the data bank, then wait n samples */
        .word   write_pcm_wait_0 - cmd_table
        .word   write_pcm_wait_1 - cmd_table
        .word   write_pcm_wait_2 - cmd_table
        .word   write_pcm_wait_3 - cmd_table
        .word   write_pcm_wait_4 - cmd_table
        .word   write_pcm_wait_5 - cmd_table
        .word   write_pcm_wait_6 - cmd_table
        .word   write_pcm_wait_7 - cmd_table
        .word   write_pcm_wait_8 - cmd_table
        .word   write_pcm_wait_9 - cmd_table
        .word   write_pcm_wait_10 - cmd_table
        .word   write_pcm_wait_11 - cmd_table
        .word   write_pcm_wait_12 - cmd_table
        .word   write_pcm_wait_13 - cmd_table
        .word   write_pcm_wait_14 - cmd_table
        .word   write_pcm_wait_15 - cmd_table
        /* 90 - 9F */
        .word   stream_setup_ctrl - cmd_table
        .word   stream_set_data - cmd_table
        .word   stream_set_freq - cmd_table
        .word   stream_start - cmd_table
        .word   stream_stop - cmd_table
        .word   stream_start_fast - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        /* A0 - AF */
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        /* B0 - BF */
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        .word   reserved_2 - cmd_table
        /* C0 - CF */
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        /* D0 - DF */
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        .word   reserved_3 - cmd_table
        /* E0 - EF */
        .word   seek_pcm - cmd_table   /* seek to offset dddddddd in PCM data bank */
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        /* F0 - FF */
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table
        .word   reserved_4 - cmd_table

reserved_0:
        move.l  a6,fm_cur       /* update vgm ptr */
        rts                     /* command has no args */

reserved_1:
        addq.l  #1,a6
        move.l  a6,fm_cur       /* update vgm ptr */
        rts                     /* command has one arg */

reserved_2:
        addq.l  #2,a6
        move.l  a6,fm_cur       /* update vgm ptr */
        rts                     /* command has two args */

reserved_3:
        addq.l  #3,a6
        move.l  a6,fm_cur       /* update vgm ptr */
        rts                     /* command has three args */

reserved_4:
        addq.l  #4,a6
        move.l  a6,fm_cur       /* update vgm ptr */
        rts                     /* command has four args */

write_psg:
        fetch_vgm d0
        move.b  d0,0xC00011
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

write_fm0:
        fetch_vgm d0
        move.b  d0,0xA04000
        move.b  d0,d1
        fetch_vgm d0
        cmpi.b  #0x27,d1
        bne.b   0f              /* not setting timer command/status reg */
        andi.b  #0x40,d0        /* we need the CSM Mode bit */
        ori.b   #0x15,d0
        move.b  d0,fm_csm       /* save for timer updates */
0:
        move.b  d0,0xA04001
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

write_fm1:
        fetch_vgm d0
        move.b  d0,0xA04002
        fetch_vgm d0
        move.b  d0,0xA04003
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_1:
        move.w  #1,fm_smpl      /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_2:
        move.w  #2,fm_smpl      /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_3:
        move.w  #3,fm_smpl      /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_4:
        move.w  #4,fm_smpl      /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_5:
        move.w  #5,fm_smpl      /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_6:
        move.w  #6,fm_smpl      /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_7:
        move.w  #7,fm_smpl      /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_8:
        move.w  #8,fm_smpl      /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_9:
        move.w  #9,fm_smpl      /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_10:
        move.w  #10,fm_smpl     /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_11:
        move.w  #11,fm_smpl     /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_12:
        move.w  #12,fm_smpl     /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_13:
        move.w  #13,fm_smpl     /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_14:
        move.w  #14,fm_smpl     /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_15:
        move.w  #15,fm_smpl     /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_16:
        move.w  #16,fm_smpl     /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_735:
        move.w  #735,fm_smpl    /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_882:
        move.w  #882,fm_smpl    /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

wait_nnnn:
        fetch_vgm d0
        fetch_vgm d1
        move.b  d1,-(sp)
        move.w  (sp)+,d1        /* shift left 8 */
        move.b  d0,d1
        move.w  d1,fm_smpl      /* set vgm wait count */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

write_pcm_wait_0:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

write_pcm_wait_1:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_1

write_pcm_wait_2:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_2

write_pcm_wait_3:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_3

write_pcm_wait_4:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_4

write_pcm_wait_5:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_5

write_pcm_wait_6:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_6

write_pcm_wait_7:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_7

write_pcm_wait_8:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_8

write_pcm_wait_9:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_9

write_pcm_wait_10:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_10

write_pcm_wait_11:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_11

write_pcm_wait_12:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_12

write_pcm_wait_13:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_13

write_pcm_wait_14:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_14

write_pcm_wait_15:
        movea.l pcm_cur,a5      /* pcm ptr */
        move.b  #0x2A,0xA04000
        fetch_pcm d0
        move.b  d0,0xA04001
        move.l  a5,pcm_cur      /* update pcm ptr */
        bra     wait_15

| Some notes for streams on the MD
|   There is only one YM2612, and thus only one DAC,
|   and thus only one stream. We can probably just
|   ignore it. We are also assuming an optimized VGM
|   with only one data block using stream command 93
|   to play samples.
|
|   We currently assume the length mode is 01, and
|   hence the number of samples to play at the set
|   frequency.

stream_setup_ctrl:
        fetch_vgm d0            /* stream ID */
        fetch_vgm d1            /* chip type */
        fetch_vgm d2            /* port */
        fetch_vgm d3            /* command/control value */

        move.b  d0,strm_id
        move.b  d1,strm_chip    /* should be 0x00 for YM2612 */
        move.b  d2,strm_port    /* should be 0x00 for YM2612 */
        move.b  d3,strm_cmd     /* should be 0x2A for YM2612 */

        move.l  a6,fm_cur       /* update vgm ptr */
        rts

stream_set_data:
        fetch_vgm d0            /* stream ID */
        fetch_vgm d1            /* data bank ID */
        fetch_vgm d2            /* step size */
        fetch_vgm d3            /* step base */

        move.b  d0,strm_id
        move.b  d2,strm_ssize
        move.b  d3,strm_sbase

        move.l  a6,fm_cur       /* update vgm ptr */
        rts

stream_set_freq:
        fetch_vgm d0            /* stream ID */
        move.b  d0,strm_id

        fetch_vgm d0            /* frequency */
        fetch_vgm d1
        fetch_vgm d2
        fetch_vgm d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d0,d3
        move.l  d3,strm_freq

        move.l  a6,fm_cur       /* update vgm ptr */
        rts

stream_start:
        fetch_vgm d0            /* stream ID */
        move.b  d0,strm_id

        fetch_vgm d0            /* data start offset (-1 == ignore) */
        fetch_vgm d1
        fetch_vgm d2
        fetch_vgm d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d0,d3
        cmpi.l  #-1,d3
        beq.b   0f              /* ignore start offset, leave at current position */
        addq    #7,d3           /* skip over block command to pcm data */
        move.l  d3,strm_offs
0:
        fetch_vgm d0            /* length mode */
        move.b  d0,strm_lmode

        fetch_vgm d0            /* data length */
        fetch_vgm d1
        fetch_vgm d2
        fetch_vgm d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d0,d3
        move.l  d3,strm_len

        /* send stream start to primary sh2 */
        move.w  #0x0001,0xA15102    /* assert CMD INT to primary SH2 */
10:
        move.w  0xA15120,d0     /* wait on handshake in COMM0 */
        cmpi.w  #0xA55A,d0
        bne.b   10b

        move.l  strm_offs,d0
        add.l   pcm_baseoffs,d0
|        move.w  d0,0xA1512C     /* offs 0 to 15 */
        swap    d0
        move.l  strm_len,d3
|        move.w  d3,0xA1512E     /* len 0 to 15 */
        swap    d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3        /* shift left 8 len 16 to 23 */
        move.b  d0,d3           /* offs 16 to 23 */
        move.w  d3,0xA15122     /* len_hi:off_hi */
        move.l  strm_freq,d0
        move.w  d0,0xA15120     /* set frequency and start stream */
20:
        move.w  0xA15120,d0     /* wait on handshake in COMM0 */
        cmpi.w  #0,d0
        bne.b   20b

        move.l  strm_offs,d0
        add.l   pcm_baseoffs,d0
        move.w  d0,0xA15122     /* offs 0 to 15 */

        move.l  strm_len,d3
        lsl.l   #1,d3
        add.l   #1,d3
        move.w  d3,0xA15120     /* len 0 to 14 */

30:
        move.w  0xA15120,d0     /* wait on handshake in COMM0 */
        cmpi.w  #0xA55A,d0
        bne.b   30b

        /* done */
        move.w  #0x5AA5,0xA15120
40:
        cmpi.w  #0x5AA5, 0xA15120
        beq.b   40b

        /* pcm stream started on 32X */
|        move.l  strm_offs,d0
|        add.l   strm_len,d0
|        move.l  d0,strm_offs

        move.l  a6,fm_cur       /* update vgm ptr */
        rts

stream_stop:
        fetch_vgm d0            /* stream ID (0xFF == stop all streams) */

        /* send stream stop to primary sh2 */
        move.w  #0x0001,0xA15102    /* assert CMD INT to primary SH2 */
10:
        move.w  0xA15120,d0     /* wait on handshake in COMM0 */
        cmpi.w  #0xA55A,d0
        bne.b   10b
        move.w  #0xFFFF,0xA15120    /* stop pcm channel */
20:
        move.w  0xA15120,d0     /* wait on handshake in COMM0 */
        cmpi.w  #0xA55A,d0
        bne.b   20b
        /* done */
        move.w  #0x5AA5,0xA15120
30:
        cmpi.w  #0x5AA5, 0xA15120
        beq.b   30b

        move.l  a6,fm_cur       /* update vgm ptr */
        rts

stream_start_fast:
        fetch_vgm d0            /* stream ID */

        fetch_vgm d1            /* block ID */
        fetch_vgm d2
        move.b  d2,-(sp)
        move.w  (sp)+,d2        /* shift left 8 */
        move.b  d1,d2

        fetch_vgm d3            /* flags (0x01 == loop, 0x10 == reverse) */

        move.l  a6,fm_cur       /* update vgm ptr */
        rts

seek_pcm:
        fetch_vgm d0
        fetch_vgm d1
        fetch_vgm d2
        fetch_vgm d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d0,d3
        add.l   pcm_base,d3     /* new pcm ptr */
        move.l  d3,pcm_cur

        tst.b   pcm_en
        bne.b   0f
        move.b  #0x2B,0xA04000
        nop
        nop
        nop
        move.b  #0x80,0xA04001  /* enable DAC */
        nop
        nop
        nop
        move.b  #0x2A,0xA04000
        nop
        nop
        nop
        move.b  #0x80,0xA04001  /* silence */
        move.b  #1,pcm_en
0:
        move.l  a6,fm_cur       /* update vgm ptr */
        rts

data_block:
        fetch_vgm d0
        cmpi.b  #0x66,d0
        bne     stop            /* error in stream */
        fetch_vgm d4

        fetch_vgm d0
        fetch_vgm d1
        fetch_vgm d2
        fetch_vgm d3
        move.b  d3,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d2,d3
        swap    d3
        move.b  d1,-(sp)
        move.w  (sp)+,d3        /* shift left 8 */
        move.b  d0,d3           /* size of data block */

        move.l  a6,d1           /* current vgm real address */

        tst.b   d4
        bne.b   0f              /* not pcm */
        /* data block of pcm */
        move.l  d1,pcm_base
        move.l  pcm_base,pcm_cur /* curr pcm ptr = pcm buffer */

        tst.b   pcm_en
        bne.b   0f
        move.b  #0x2B,0xA04000
        nop
        nop
        nop
        move.b  #0x80,0xA04001  /* enable DAC */
        nop
        nop
        nop
        move.b  #0x2A,0xA04000
        nop
        nop
        nop
        move.b  #0x80,0xA04001  /* silence */
        move.b  #1,pcm_en
0:
        add.l   d1,d3           /* new vgm ptr, skip over data block */
        move.l  d1,fm_cur
        rts

end_data:
        tst.w   fm_rep
        beq     stop            /* no repeat, stop music */

        move.l  vgm_ptr,vgm_chk
        jsr     vgm_setup       /* restart at start of compressed data and fill buffer */
        addi.l  #vgm_ahead,vgm_chk  /* vgm_setup read this for us */

        move.l  fm_loop,d0      /* offset from data start to song start */
        add.l   vgm_ptr,d0
        move.l  d0,fm_cur
        move.l  pcm_base,pcm_cur
        clr.b   pcm_en

        move.b  #0x2B,0xA04000
        nop
        nop
        nop
        move.b  #0x80,0xA04001  /* enable DAC */
        nop
        nop
        nop
        move.b  #0x2A,0xA04000
        nop
        nop
        nop
        move.b  #0x80,0xA04001  /* silence */
        nop
        nop
        nop
        move.b  #0x2B,0xA04000
        nop
        nop
        nop
        move.b  #0x00,0xA04001  /* disable DAC */
        rts
stop:
        clr.w   fm_idx          /* stop music */
        clr.w   fm_rep
        clr.w   fm_smpl
        clr.w   fm_tick
        clr.w   fm_ttt
        clr.l   fm_ptr
        bra     fm_init


bump_vgm:
        movem.l d0-d4/a2-a3/a6,-(sp)
        jsr     vgm_read
        addi.l  #vgm_ahead,vgm_chk
        movem.l (sp)+,d0-d4/a2-a3/a6
        rts

bump_pcm:
        rts


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
        /* disable timer & set channel 6 to normal mode */
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

        .global    fm_ptr
fm_ptr:
        dc.l    0
        .global    fm_cnt
fm_cnt:
        dc.l    0
        .global    fm_loop
fm_loop:
        dc.l    0
fm_cur:
        dc.l    0
pcm_base:
        dc.l    0
        .global pcm_baseoffs
pcm_baseoffs:
        dc.l    0
pcm_cur:
        dc.l    0
vgm_chk:
        dc.l    0
pcm_chk:
        dc.l    0
        .global    vgm_ptr
vgm_ptr:
        dc.l    0
        .global    vgm_size
vgm_size:
        dc.l    0
        .global    fm_rep
fm_rep:
        dc.w    0
        .global    fm_idx
fm_idx:
        dc.w    0
        .global    fm_smpl
fm_smpl:
        dc.w    0
        .global    fm_tick
fm_tick:
        dc.w    0
        .global    fm_ttt
fm_ttt:
        dc.w    0
        .global    fm_csm
fm_csm:
        dc.b    0

pcm_en:
        dc.b    0


        .align  4

        .global     strm_freq
strm_freq:
        dc.l    0
        .global     strm_len
strm_len:
        dc.l    0
        .global     strm_offs
strm_offs:
        dc.l    1
        .global     strm_id
strm_id:
        dc.b    0
        .global     strm_chip
strm_chip:
        dc.b    0
        .global     strm_port
strm_port:
        dc.b    0
        .global     strm_cmd
strm_cmd:
        dc.b    0
        .global     strm_ssize
strm_ssize:
        dc.b    0
        .global     strm_sbase
strm_sbase:
        dc.b    0
        .global     strm_lmode
strm_lmode:
        dc.b    0

        .align  2
