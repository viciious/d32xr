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

.data

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
        mov.w   @(20,r4),r0     /* volume:pan */

        /* calculate left/right volumes from volume, pan, and scale */
        mov     r0,r13
        shlr8   r13
        extu.b  r13,r13         /* ch_vol */
        mov     r13,r14
        extu.b  r0,r0           /* pan */

        /* LINEAR_CROSSFADE */
        mov     #0xFF,r1
        extu.b  r1,r1
        sub     r0,r1           /* 255 - pan */
        mulu.w  r0,r14
        sts     macl,r0         /* pan * ch_vol */
        mul.l   r0,r7
        sts     macl,r14        /* pan * ch_vol * scale */
        shlr8   r14
!       shlr2   r14
        shlr2   r14             /* right volume = pan * ch_vol * scale / 64 / 64 */

        mulu.w  r1,r13
        sts     macl,r0         /* (255 - pan) * ch_vol */
        mul.l   r0,r7
        sts     macl,r13        /* (255 - pan) * ch_vol * scale */
        shlr8   r13
!       shlr2   r13
        shlr2   r13             /* left volume = (255 - pan) * ch_vol * scale / 64 / 64 */

        /* mix r6 stereo samples */
mix_loop:
        /* process one sample */
        mov     r9,r0
        shlr8   r0
        shll2   r0
        shlr8   r0
        mov.b   @(r0,r8),r0
        extu.b  r0,r3
        add     #-128,r3

        /* scale sample for left output */
        muls.w  r3,r13
        mov.l   @r5,r1
        sts     macl,r0
        shlr8   r0
        exts.w  r0,r0

        /* scale sample for right output */
        muls.w  r3,r14

        /* scale sample for left output -- cont */
        add     r0,r1
        swap.w  r1,r1

        /* scale sample for right output -- cont */
        sts     macl,r0
        shlr8   r0
        exts.w  r0,r0
        add     r0,r1

        mov.l   r1,@r5
        add     #4,r5

        /* advance position and check for loop */
        add     r10,r9                  /* position += increment */
mix_chk:
        cmp/hs  r11,r9
        bt      mix_wrap                /* position >= length */
mix_next:
        /* next sample */
        dt      r6
        bf      mix_loop
        bra     mix_exit
        mov.l   r9,@(4,r4)              /* update position field */

mix_wrap:
        /* check if loop sample */
        mov     r12,r0
        cmp/eq  #0,r0
        bf/s    mix_chk                 /* loop sample */
        sub     r12,r9                  /* position -= loop_length */
        /* sample done playing */
        mov.l   r12,@r4                 /* clear data pointer field */

mix_exit:
        mov.l   @r15+,r14
        mov.l   @r15+,r13
        mov.l   @r15+,r12
        mov.l   @r15+,r11
        mov.l   @r15+,r10
        mov.l   @r15+,r9
        mov.l   @r15+,r8
        rts
        nop

