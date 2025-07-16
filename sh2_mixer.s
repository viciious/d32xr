!  The MIT License (MIT)
!
!  Permission is hereby granted, free of charge, to any person obtaining a copy
!  of this software and associated documentation files (the "Software"), to deal
!  in the Software without restriction, including without limitation the rights
!  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
!  copies of the Software, and to permit persons to whom the Software is
!  furnished to do so, subject to the following conditions:
!
!  The above copyright notice and this permission notice shall be included in all
!  copies or substantial portions of the Software.
!
!  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
!  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
!  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
!  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
!  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
!  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
!  SOFTWARE.

.section .sdata

! void S_PaintChannel8(void *channel, int16_t *buffer, int32_t cnt, int32_t scale);
! On entry: r4 = channel pointer
!           r5 = buffer pointer
!           r6 = count (number of stereo 16-bit samples)
!           r7 = scale (global volume - possibly fading, 0 - 64)
        .align  4
        .global _S_PaintChannel8
_S_PaintChannel8:
        mov.l   r8,@-r15
        mov.l   r9,@-r15
        mov.l   r10,@-r15
        mov.l   r11,@-r15
        mov.l   r12,@-r15
        mov.l   r13,@-r15
        mov.l   r14,@-r15

        mov.l   @r4,r8          /* data pointer */
        mov.l   @(4,r4),r9      /* position */
        mov.l   @(8,r4),r10     /* increment */
        mov.l   @(12,r4),r11    /* length */
        mov.l   @(16,r4),r12    /* loop_length */
        mov.w   @(24,r4),r0     /* volume:pan */

        /* calculate left/right volumes from volume, pan, and scale */
        swap.b  r0,r13
        extu.b  r13,r13         /* ch_vol */
        extu.b  r0,r0           /* pan */

        /* LINEAR_CROSSFADE */
        mulu.w  r7,r13
        mov     #0xFF,r1
        extu.b  r1,r1
        sub     r0,r1           /* 255 - pan */
        sts     macl,r7         /* ch_vol * scale */

        mulu.w  r0,r7
        /* process one sample -- begin */
        mov     r9,r0
        shlr8   r0
        shll2   r0
        shlr8   r0
        sts     macl,r14        /* pan * ch_vol * scale */
        mov.b   @(r0,r8),r0

        mulu.w  r1,r7
        shlr8   r14
!       shlr2   r14
        shlr2   r14             /* right volume = pan * ch_vol * scale / 64 / 64 */
        sts     macl,r13        /* (255 - pan) * ch_vol * scale */
        shlr8   r13
!       shlr2   r13
        shlr2   r13             /* left volume = (255 - pan) * ch_vol * scale / 64 / 64 */

        /* mix r6 stereo samples */
        .p2alignw 1, 0x0009
mix_loop:
        mov.l   @r5,r1

        /* process one sample -- cont */
        extu.b  r0,r3
        add     #-128,r3
        shll8   r3

        /* scale sample for left output */
        muls.w  r3,r13
        add     r10,r9          /* position += increment */
        sts     macl,r0

        /* scale sample for right output */
        muls.w  r3,r14

        /* scale sample for left output -- cont */
        shlr16  r0
        shll16  r0
        add     r0,r1

        /* scale sample for right output -- cont */
        sts     macl,r0
        shlr16  r0
        add     r0,r1

        /* advance position and check for loop */
        mov.l   r1,@r5
        add     #4,r5
mix_chk:
        cmp/hs  r11,r9
        bt/s    mix_wrap                /* position >= length */
mix_next:
        /* next sample */
        /* process one sample -- begin */
        mov     r9,r0
        shlr8   r0
        shll2   r0
        shlr8   r0
        mov.b   @(r0,r8),r0
        dt      r6
        bf      mix_loop
        bra     mix_exit
        nop

mix_wrap:
        /* check if loop sample */
        mov     r12,r0
        cmp/eq  #0,r0
        bf/s    mix_chk                 /* loop sample */
        sub     r12,r9                  /* position -= loop_length */
        /* sample done playing */

mix_exit:
        mov.l   r9,@(4,r4)              /* update position field */
        mov.l   @r15+,r14
        mov.l   @r15+,r13
        mov.l   @r15+,r12
        mov.l   @r15+,r11
        mov.l   @r15+,r10
        mov.l   @r15+,r9
        mov.l   @r15+,r8
        rts
        nop

! void S_PaintChannel4IMA(void *channel, int16_t *buffer, int32_t cnt, int32_t scale);
! On entry: r4 = channel pointer
!           r5 = buffer pointer
!           r6 = count (number of stereo 16-bit samples / 2)
!           r7 = scale (global volume - possibly fading, 0 - 64)
        .align  4
        .global _S_PaintChannel4IMA
_S_PaintChannel4IMA:
        mov.l   r8,@-r15
        mov.l   r9,@-r15
        mov.l   r10,@-r15
        mov.l   r11,@-r15
        mov.l   r12,@-r15
        mov.l   r13,@-r15
        mov.l   r14,@-r15
 
        mov.l   @r4,r8          /* data pointer */
        mov.l   @(4,r4),r9      /* position */
        mov.l   @(8,r4),r10     /* increment (should be <= 1.0 for adpcm) */
        mov.l   @(12,r4),r11    /* length */
        mov.l   @(16,r4),r12    /* step_index:predictor */
        mov.w   @(24,r4),r0     /* volume:pan */

        mov     r12,r2          /* save step_index */
        shlr16  r2

        /* calculate left/right volumes from volume, pan, and scale */
        swap.b  r0,r13
        extu.b  r13,r13         /* ch_vol */
        extu.b  r0,r0           /* pan */
 
        /* LINEAR_CROSSFADE */
        mulu.w  r7,r13
        mov     #0xFF,r1
        extu.b  r1,r1
        sub     r0,r1           /* 255 - pan */
        sts     macl,r7         /* ch_vol * scale */

        mulu.w  r0,r7
        mov.l   @(20,r4),r0     /* prev_pos */
        sts     macl,r14        /* pan * ch_vol * scale */

        mulu.w  r1,r7
        shlr8   r14
!       shlr2   r14
        shlr2   r14             /* right volume = pan * ch_vol * scale / 64 / 64 */
        sts     macl,r13        /* (255 - pan) * ch_vol * scale */
        shlr8   r13
!       shlr2   r13
        shlr2   r13             /* left volume = (255 - pan) * ch_vol * scale / 64 / 64 */

        cmp/eq  #-1,r0
        bf/s    mix4_loop
        mov     r0,r7
        bra     mix4_gets
        mov     #1,r7

        /* mix r6 stereo samples */
        .p2alignw 1, 0x0009
mix4_loop:
        /* process one sample */
        mov.l   _step_table,r1
        mov     r2,r0
        mov.w   @(r0,r1),r1     /* step = step_table[step_index] */

        mov     r9,r0
        shlr8   r0
        shll2   r0
        shlr8   r0              /* sample position in nibbles */
        cmp/eq  r0,r7           /* prev_pos */
        bt/s    mix4_gets       /* haven't advanced a sample yet */
        mov     r0,r7           /* update prev_pos and calculate new adpcm sample */
 
        shlr    r0              /* one more because samples are nibbles, T set if msn */
        bf/s    0f              /* T not set, lsn */
        mov.b   @(r0,r8),r0
        shlr2   r0
        shlr2   r0
0:
        tst     #8,r0           /* (nibble & 8) */
        and     #0x07,r0        /* nibble &= 7 */
        /* diff = (2 * nibble + 1) * step / 8 */
        mov     r0,r3
        add     r3,r3           /* 2 * nibble */
        add     #1,r3
        muls.w  r1,r3
        exts.w  r12,r12         /* 32-bit predictor */
        mov     #-128,r1
        sts     macl,r3         /* diff */
        /* if (nibble & 8) step = -step */
        bt/s    1f
        shar    r3              /* diff / 2 */
        neg     r3,r3
1:
        shar    r3              /* diff / 4 */
        shar    r3              /* diff / 8 */
        add     r3,r12          /* predictor += diff */

        /* clamp to -32768:32767 */
        shll8   r1
        cmp/ge  r1,r12
        bf/s    2f              /* sample < -32768 */
        not     r1,r1
        cmp/gt  r1,r12
        bt/s    3f              /* sample > 32767 */
4:
        /* step_index += index_table[nibble] */
        mov.l   _index_table,r1
        shll2   r2              /* index *= 4 */
        add     r2,r0
        mov.b   @(r0,r1),r2

        .p2alignw 1, 0x0009
mix4_gets:
        /* get sample */

        /* scale sample for left output */
        muls.w  r12,r13
        mov.l   @r5,r1
        add     r10,r9          /* position += increment */
        sts     macl,r0

        /* scale sample for right output */
        muls.w  r12,r14

        /* scale sample for left output -- cont */
        shlr16  r0
        shll16  r0
        mov     r0,r3

        /* scale sample for right output -- cont */
        sts     macl,r0
        shlr16  r0
        add     r0,r3

        /* write left and right output */
        add     r3,r1
        cmp/hs  r11,r9
        mov.l   r1,@r5

        extu.b  r2,r2                   /* index = (uint8_t)index */

        /* advance position */

        bt/s    mix4_exit               /* position >= length */

        /* next sample */
        dt      r6
        bf/s    mix4_loop
        add     #4,r5

        /* not done with the whole file, so update step_index : predictor */ 
        extu.w  r12,r12                 /* clear upper word for step_index */
        shll16  r2
        or      r2,r12                  /* r12 is step_index : predictor again */
        mov.l   r12,@(16,r4)            /* update step_index : predictor */

mix4_exit:
        mov.l   r7,@(20,r4)
        mov.l   r9,@(4,r4)              /* update position field */
        mov.l   @r15+,r14
        mov.l   @r15+,r13
        mov.l   @r15+,r12
        mov.l   @r15+,r11
        mov.l   @r15+,r10
        mov.l   @r15+,r9
        mov.l   @r15+,r8
        rts
        mov     r6,r0
2:
        not     r1,r1
        bra     4b
        mov     r1,r12          /* clamp sample to -32768 */
3:
        bra     4b
        mov     r1,r12          /* clamp sample to  32767 */
        bra     mix4_gets
        nop

! void S_PaintChannel4IMA2x(void *channel, int16_t *buffer, int32_t cnt, int32_t scale);
! On entry: r4 = channel pointer
!           r5 = buffer pointer
!           r6 = count (number of stereo 16-bit samples / 2)
!           r7 = scale (global volume - possibly fading, 0 - 64)
        .align  4
        .global _S_PaintChannel4IMA2x
_S_PaintChannel4IMA2x:
        mov.l   r8,@-r15
        mov.l   r9,@-r15
        mov.l   r10,@-r15
        mov.l   r11,@-r15
        mov.l   r12,@-r15
        mov.l   r13,@-r15
        mov.l   r14,@-r15
 
        mov.l   @r4,r8          /* data pointer */
        mov.l   @(4,r4),r9      /* position */
        mov.l   @(8,r4),r10     /* increment (should be <= 1.0 for adpcm) */
        mov.l   @(12,r4),r11    /* length */
        mov.l   @(16,r4),r12    /* step_index:predictor */
        mov.w   @(24,r4),r0     /* volume:pan */

        mov     r12,r2          /* save step_index */
        shlr16  r2

        add     r10,r10         /* increment *= 2 */

        /* calculate left/right volumes from volume, pan, and scale */
        swap.b  r0,r13
        extu.b  r13,r13         /* ch_vol */
        extu.b  r0,r0           /* pan */
 
        /* LINEAR_CROSSFADE */
        mulu.w  r7,r13
        mov     #0xFF,r1
        extu.b  r1,r1
        sub     r0,r1           /* 255 - pan */
        sts     macl,r7         /* ch_vol * scale */

        mulu.w  r0,r7
        mov.l   @(20,r4),r0     /* prev_pos */
        sts     macl,r14        /* pan * ch_vol * scale */

        mulu.w  r1,r7
        shlr8   r14
!       shlr2   r14
        shlr2   r14             /* right volume = pan * ch_vol * scale / 64 / 64 */
        sts     macl,r13        /* (255 - pan) * ch_vol * scale */
        shlr8   r13
!       shlr2   r13
        shlr2   r13             /* left volume = (255 - pan) * ch_vol * scale / 64 / 64 */

        cmp/eq  #-1,r0
        bf/s    mix4_loop2x
        mov     r0,r7
        bra     mix4_gets2x
        mov     #1,r7

        /* mix r6 stereo samples */
        .p2alignw 1, 0x0009
mix4_loop2x:
        /* process one sample */
        mov.l   _step_table,r1
        mov     r2,r0           /* step_index */
        mov.w   @(r0,r1),r1     /* step = step_table[step_index] */

        mov     r9,r0
        shlr8   r0
        shll2   r0
        shlr8   r0              /* sample position in nibbles */
        cmp/eq  r0,r7           /* prev_pos */
        bt/s    mix4_gets2x     /* haven't advanced a sample yet */
        mov     r0,r7           /* update prev_pos and calculate new adpcm sample */
 
        shlr    r0              /* one more because samples are nibbles, T set if msn */
        bf/s    0f              /* T not set, lsn */
        mov.b   @(r0,r8),r0
        shlr2   r0
        shlr2   r0
0:
        tst     #8,r0           /* (nibble & 8) */
        and     #0x07,r0        /* nibble &= 7 */
        /* diff = (2 * nibble + 1) * step / 8 */
        mov     r0,r3
        add     r3,r3           /* 2 * nibble */
        add     #1,r3
        muls.w  r1,r3
        exts.w  r12,r12         /* 32-bit predictor */
        mov     #-128,r1
        sts     macl,r3         /* diff */
        /* if (nibble & 8) step = -step */
        bt/s    1f
        shar    r3              /* diff / 2 */
        neg     r3,r3
1:
        shar    r3              /* diff / 4 */
        shar    r3              /* diff / 8 */
        add     r3,r12          /* predictor += diff */

        /* clamp to -32768:32767 */
        shll8   r1
        cmp/ge  r1,r12
        bf/s    2f              /* sample < -32768 */
        not     r1,r1
        cmp/gt  r1,r12
        bt/s    3f              /* sample > 32767 */
4:
        /* step_index += index_table[nibble] */
        mov.l   _index_table,r1
        shll2   r2              /* index *= 4 */
        add     r2,r0
        mov.b   @(r0,r1),r2

        .p2alignw 1, 0x0009
mix4_gets2x:
        /* get sample */

        /* scale sample for left output */
        muls.w  r12,r13
        mov.l   @r5,r1
        dt      r6
        add     r10,r9          /* position += increment */
        sts     macl,r0

        /* scale sample for right output */
        muls.w  r12,r14

        /* scale sample for left output -- cont */
        shlr16  r0
        shll16  r0
        mov     r0,r3

        /* scale sample for right output -- cont */
        sts     macl,r0
        shlr16  r0
        add     r0,r3

        /* write left and right output */
        add     r3,r1
        mov.l   r1,@r5

        mov.l   @(4,r5),r1
        cmp/hs  r11,r9
        add     r3,r1
        mov.l   r1,@(4,r5)

        extu.b  r2,r2                   /* index = (uint8_t)index */

        /* advance position */

        bt/s    mix4_exit2x             /* position >= length */

        /* next sample */
        dt      r6
        bf/s    mix4_loop2x
        add     #8,r5

        /* not done with the whole file, so update step_index : predictor */ 
        extu.w  r12,r12                 /* clear upper word for step_index */
        shll16  r2
        or      r2,r12                  /* r12 is step_index : predictor again */
        mov.l   r12,@(16,r4)            /* update step_index : predictor */

mix4_exit2x:
        mov.l   r7,@(20,r4)
        mov.l   r9,@(4,r4)              /* update position field */
        mov.l   @r15+,r14
        mov.l   @r15+,r13
        mov.l   @r15+,r12
        mov.l   @r15+,r11
        mov.l   @r15+,r10
        mov.l   @r15+,r9
        mov.l   @r15+,r8
        rts
        mov     r6,r0
2:
        not     r1,r1
        bra     4b
        mov     r1,r12          /* clamp sample to -32768 */
3:
        bra     4b
        mov     r1,r12          /* clamp sample to  32767 */
        bra     mix4_gets2x
        nop

        .align  4

_step_table:
        .long   step_table
_index_table:
        .long   index_table
 
step_table:
        .word   7, 8, 9, 10, 11, 12, 13, 14
        .word   16, 17, 19, 21, 23, 25, 28, 31
        .word   34, 37, 41, 45, 50, 55, 60, 66
        .word   73, 80, 88, 97, 107, 118, 130, 143
        .word   157, 173, 190, 209, 230, 253, 279, 307
        .word   337, 371, 408, 449, 494, 544, 598, 658
        .word   724, 796, 876, 963, 1060, 1166, 1282, 1411
        .word   1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024
        .word   3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484
        .word   7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899
        .word   15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794
        .word   32767

.text
.global index_table
index_table:
        .byte   0, 0, 0, 0, 4, 8, 12, 16
        .byte   0, 0, 0, 0, 6, 10, 14, 18
        .byte   2, 2, 2, 2, 8, 12, 16, 20
        .byte   4, 4, 4, 4, 10, 14, 18, 22
        .byte   6, 6, 6, 6, 12, 16, 20, 24
        .byte   8, 8, 8, 8, 14, 18, 22, 26
        .byte   10, 10, 10, 10, 16, 20, 24, 28
        .byte   12, 12, 12, 12, 18, 22, 26, 30
        .byte   14, 14, 14, 14, 20, 24, 28, 32
        .byte   16, 16, 16, 16, 22, 26, 30, 34
        .byte   18, 18, 18, 18, 24, 28, 32, 36
        .byte   20, 20, 20, 20, 26, 30, 34, 38
        .byte   22, 22, 22, 22, 28, 32, 36, 40
        .byte   24, 24, 24, 24, 30, 34, 38, 42
        .byte   26, 26, 26, 26, 32, 36, 40, 44
        .byte   28, 28, 28, 28, 34, 38, 42, 46
        .byte   30, 30, 30, 30, 36, 40, 44, 48
        .byte   32, 32, 32, 32, 38, 42, 46, 50
        .byte   34, 34, 34, 34, 40, 44, 48, 52
        .byte   36, 36, 36, 36, 42, 46, 50, 54
        .byte   38, 38, 38, 38, 44, 48, 52, 56
        .byte   40, 40, 40, 40, 46, 50, 54, 58
        .byte   42, 42, 42, 42, 48, 52, 56, 60
        .byte   44, 44, 44, 44, 50, 54, 58, 62
        .byte   46, 46, 46, 46, 52, 56, 60, 64
        .byte   48, 48, 48, 48, 54, 58, 62, 66
        .byte   50, 50, 50, 50, 56, 60, 64, 68
        .byte   52, 52, 52, 52, 58, 62, 66, 70
        .byte   54, 54, 54, 54, 60, 64, 68, 72
        .byte   56, 56, 56, 56, 62, 66, 70, 74
        .byte   58, 58, 58, 58, 64, 68, 72, 76
        .byte   60, 60, 60, 60, 66, 70, 74, 78
        .byte   62, 62, 62, 62, 68, 72, 76, 80
        .byte   64, 64, 64, 64, 70, 74, 78, 82
        .byte   66, 66, 66, 66, 72, 76, 80, 84
        .byte   68, 68, 68, 68, 74, 78, 82, 86
        .byte   70, 70, 70, 70, 76, 80, 84, 88
        .byte   72, 72, 72, 72, 78, 82, 86, 90
        .byte   74, 74, 74, 74, 80, 84, 88, 92
        .byte   76, 76, 76, 76, 82, 86, 90, 94
        .byte   78, 78, 78, 78, 84, 88, 92, 96
        .byte   80, 80, 80, 80, 86, 90, 94, 98
        .byte   82, 82, 82, 82, 88, 92, 96, 100
        .byte   84, 84, 84, 84, 90, 94, 98, 102
        .byte   86, 86, 86, 86, 92, 96, 100, 104
        .byte   88, 88, 88, 88, 94, 98, 102, 106
        .byte   90, 90, 90, 90, 96, 100, 104, 108
        .byte   92, 92, 92, 92, 98, 102, 106, 110
        .byte   94, 94, 94, 94, 100, 104, 108, 112
        .byte   96, 96, 96, 96, 102, 106, 110, 114
        .byte   98, 98, 98, 98, 104, 108, 112, 116
        .byte   100, 100, 100, 100, 106, 110, 114, 118
        .byte   102, 102, 102, 102, 108, 112, 116, 120
        .byte   104, 104, 104, 104, 110, 114, 118, 122
        .byte   106, 106, 106, 106, 112, 116, 120, 124
        .byte   108, 108, 108, 108, 114, 118, 122, 126
        .byte   110, 110, 110, 110, 116, 120, 124, 128
        .byte   112, 112, 112, 112, 118, 122, 126, 130
        .byte   114, 114, 114, 114, 120, 124, 128, 132
        .byte   116, 116, 116, 116, 122, 126, 130, 134
        .byte   118, 118, 118, 118, 124, 128, 132, 136
        .byte   120, 120, 120, 120, 126, 130, 134, 138
        .byte   122, 122, 122, 122, 128, 132, 136, 140
        .byte   124, 124, 124, 124, 130, 134, 138, 142
        .byte   126, 126, 126, 126, 132, 136, 140, 144
        .byte   128, 128, 128, 128, 134, 138, 142, 146
        .byte   130, 130, 130, 130, 136, 140, 144, 148
        .byte   132, 132, 132, 132, 138, 142, 146, 150
        .byte   134, 134, 134, 134, 140, 144, 148, 152
        .byte   136, 136, 136, 136, 142, 146, 150, 154
        .byte   138, 138, 138, 138, 144, 148, 152, 156
        .byte   140, 140, 140, 140, 146, 150, 154, 158
        .byte   142, 142, 142, 142, 148, 152, 156, 160
        .byte   144, 144, 144, 144, 150, 154, 158, 162
        .byte   146, 146, 146, 146, 152, 156, 160, 164
        .byte   148, 148, 148, 148, 154, 158, 162, 166
        .byte   150, 150, 150, 150, 156, 160, 164, 168
        .byte   152, 152, 152, 152, 158, 162, 166, 170
        .byte   154, 154, 154, 154, 160, 164, 168, 172
        .byte   156, 156, 156, 156, 162, 166, 170, 174
        .byte   158, 158, 158, 158, 164, 168, 172, 176
        .byte   160, 160, 160, 160, 166, 170, 174, 176
        .byte   162, 162, 162, 162, 168, 172, 176, 176
        .byte   164, 164, 164, 164, 170, 174, 176, 176
        .byte   166, 166, 166, 166, 172, 176, 176, 176
        .byte   168, 168, 168, 168, 174, 176, 176, 176
        .byte   170, 170, 170, 170, 176, 176, 176, 176
        .byte   172, 172, 172, 172, 176, 176, 176, 176
        .byte   174, 174, 174, 174, 176, 176, 176, 176
