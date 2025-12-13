! Original code by Chilly Willy

.section .sdata

.equ DOOMTLS_COLORMAP, 16

! Draw a vertical column of pixels from a projected wall texture.
! Source is the top of the column to scale.
!
!void I_Draw4bColumnLow(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!                  fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)

        .align  4
        .global _I_Draw4bColumnLowA
_I_Draw4bColumnLowA:
	add	#1,r6
0:
        cmp/ge  r6,r5
        bf/s    1f
        sub     r5,r6           /* count = dc_yh - dc_yl */

        /* dc_yl >= dc_yh, exit */
        rts
        nop
1:
        mov.l   r8,@-r15
        mov.l   r9,@-r15
        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        shlr2   r7              /* light >>= 3 */
        shlr    r7
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8           /* fb += dc_x */
        add     r4,r8           /* fb += dc_x */
        shll8   r5
        add     r5,r8
        shlr2   r5
        add     r5,r8           /* fb += (dc_yl*256 + dc_yl*64) */
        mov.l   @(8,r15),r2     /* frac */
        mov.l   @(12,r15),r3     /* fracstep */
        mov.l   @(16,r15),r5    /* dc_source */
        mov.l   @(20,r15),r4
        mov.l   draw_width,r1
        add     #-1,r4          /* heightmask = texheight - 1 */
        sub     r1,r8           /* fb -= SCREENWIDTH */
        swap.w  r2,r0           /* (frac >> 16) */

        /* test if count & 1 */
        shlr    r6
        movt    r9              /* 1 if count was odd */
        bt/s    do_col4b_loop_odd
        add     r9,r6

        .p2alignw 1, 0x0009
do_col4b_loop:
        and     r4,r0           /* (frac >> 16) & heightmask */
        shlr    r0              /* convert nibble to byte */
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r3,r2           /* frac += fracstep */
        # byte to nibble
        # note that the upper and lower nibbles are pre-swapped
        # in the texture data to avoid doing more address math here
        bt/s    do_col4b_loop_noshift
        add     r1,r8           /* fb += SCREENWIDTH */
        shlr2   r0
        shlr2   r0
do_col4b_loop_noshift:
        and     #15,r0
        add     r0,r0
        mov.w   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        swap.w  r2,r0           /* (frac >> 16) */
        mov.w   r9,@r8          /* *fb = dpix */

do_col4b_loop_odd:
        and     r4,r0           /* (frac >> 16) & heightmask */
        shlr    r0              /* convert nibble to byte */
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r3,r2           /* frac += fracstep */
        # byte to nibble
        # note that the upper and lower nibbles are pre-swapped
        # in the texture data to avoid doing more address math here
        bt/s    do_col4b_loop_odd_noshift
        add     r1,r8           /* fb += SCREENWIDTH */
        shlr2   r0
        shlr2   r0
do_col4b_loop_odd_noshift:
        and     #15,r0
        add     r0,r0
        mov.w   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        dt      r6
        mov.w   r9,@r8          /* *fb = dpix */

        bf/s    do_col4b_loop
        swap.w  r2,r0           /* (frac >> 16) */

        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8

! Draw a vertical column of pixels from a projected wall texture.
! Non-power of 2 texture height.
!
!void I_Draw4bColumnNPO2(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!                      fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)
        .align 2
        .global _I_Draw4bColumnNPo2LowA
_I_Draw4bColumnNPo2LowA:
	add	#1,r6
0:
        cmp/ge  r6,r5
        bf/s    1f
        sub     r5,r6           /* count = dc_yh - dc_yl */

        /* dc_yl >= dc_yh, exit */
        rts
        nop
1:
        mov.l   r8,@-r15
        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        shlr2   r7              /* light >>= 3 */
        shlr    r7        
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8           /* fb += dc_x */
        add     r4,r8           /* fb += dc_x */
        shll8   r5
        add     r5,r8
        shlr2   r5
        add     r5,r8           /* fb += (dc_yl*256 + dc_yl*64) */
        mov.l   @(4,r15),r2     /* frac */
        mov.l   @(8,r15),r3     /* fracstep */
        mov.l   @(12,r15),r5    /* dc_source */
        mov.l   @(16,r15),r4
        shll16  r4              /* heightmask = texheight << FRACBITS */

        mov     #0,r0
        cmp/ge  r0,r2
        bt      3f
        /* if (frac < 0) */
2:
        add     r4,r2           /* frac += heightmask */
        cmp/ge  r0,r2
        bf      2b
        bt      5f
3:
        cmp/ge  r4,r2
        bf      5f
        /* if (frac >= heightmask) */
4:
        sub     r4,r2           /* frac -= heightmask */
        cmp/ge  r4,r2
        bt      4b
5:
        mov.l   draw_width,r1
        sub     r1,r8           /* fb -= SCREENWIDTH */
        mov     r2,r0

        /* test if count & 1 */
        shlr    r6
        movt    r0              /* 1 if count was odd */
        bt/s    do_cnp4b_loop_odd
        add     r0,r6

        .p2alignw 1, 0x0009
do_cnp4b_loop:
        shlr16  r0              /* frac >> 16 */
        shlr    r0              /* convert nibble to byte */
        mov.b   @(r0,r5),r0     /* pix = dc_source[frac >> 16] */
        add     r3,r2           /* frac += fracstep */
        # byte to nibble
        # note that the upper and lower nibbles are pre-swapped
        # in the texture data to avoid doing more address math here
        bt/s    do_cnp4b_loop_noshift
        add     r1,r8           /* fb += SCREENWIDTH */
        shlr2   r0
        shlr2   r0
do_cnp4b_loop_noshift:
        and     #15,r0
        add     r0,r0
        mov.w   @(r0,r7),r0     /* dpix = dc_colormap[pix] */
        cmp/ge  r4,r2
        bf      1f
        /* if (frac >= heightmask) */
        sub     r4,r2           /* frac -= heightmask */
1:
        mov.w   r0,@r8          /* *fb = dpix */
        mov     r2,r0

do_cnp4b_loop_odd:
        shlr16  r0              /* frac >> 16 */
        shlr    r0              /* convert nibble to byte */
        mov.b   @(r0,r5),r0     /* pix = dc_source[frac >> 16] */
        # byte to nibble
        # note that the upper and lower nibbles are pre-swapped
        # in the texture data to avoid doing more address math here
        add     r3,r2           /* frac += fracstep */
        bt/s    do_cnp4b_loop_odd_noshift
        add     r1,r8           /* fb += SCREENWIDTH */
        shlr2   r0
        shlr2   r0
do_cnp4b_loop_odd_noshift:
        and     #15,r0
        add     r0,r0
        mov.w   @(r0,r7),r0     /* dpix = dc_colormap[pix] */
        cmp/ge  r4,r2
        bf      2f
        /* if (frac >= heightmask) */
        sub     r4,r2           /* frac -= heightmask */
2:
        mov.w   r0,@r8          /* *fb = dpix */
        dt      r6              /* count-- */
        bf/s    do_cnp4b_loop
        mov     r2,r0

        rts
        mov.l   @r15+,r8

        .align  2
draw_fb:
        .long   _viewportbuffer
draw_width:
        .long   320
draw_height:
        .long   _viewportHeight
