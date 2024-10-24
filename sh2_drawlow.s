! Original code by Chilly Willy

.section .sdata

.equ DOOMTLS_COLORMAP, 16
.equ DOOMTLS_FUZZPOS,  20

! Draw a vertical column of pixels from a projected wall texture.
! Source is the top of the column to scale.
!
!void I_DrawColumn(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!                  fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)
        .align 2
        .global _I_DrawColumnLowA
_I_DrawColumnLowA:

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
        mov.l   r10,@-r15
        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8           /* fb += dc_x */
        add     r4,r8           /* fb += dc_x */
        shll8   r5
        add     r5,r8
        shlr2   r5
        add     r5,r8           /* fb += (dc_yl*256 + dc_yl*64) */
        mov.l   @(12,r15),r2     /* frac */
        mov.l   @(16,r15),r3    /* fracstep */
        mov.l   @(20,r15),r5    /* dc_source */
        mov.l   @(24,r15),r4
        mov.l   draw_width,r1
        add     #-1,r4          /* heightmask = texheight - 1 */

        swap.w  r2,r0           /* (frac >> 16) */
        and     r4,r0           /* (frac >> 16) & heightmask */

        /* test if count & 1 */
        shlr    r6
        movt    r9              /* 1 if count was odd */
        bt/s    do_col_loop_1px
        add     r9,r6

        .p2alignw 2, 0x0009
do_col_loop:
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r3,r2           /* frac += fracstep */
        mov.b   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        swap.w  r2,r0           /* (frac >> 16) */
        extu.b  r9,r9
        swap.b  r9,r10
        or      r10,r9
        mov.w   r9,@r8          /* *fb = dpix */
        add     r1,r8           /* fb += SCREENWIDTH */
        and     r4,r0           /* (frac >> 16) & heightmask */
do_col_loop_1px:
        add     r3,r2           /* frac += fracstep */
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        dt      r6              /* count-- */
        mov.b   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        swap.w  r2,r0           /* (frac >> 16) */
        and     r4,r0           /* (frac >> 16) & heightmask */
        extu.b  r9,r9
        swap.b  r9,r10
        or      r10,r9
        mov.w   r9,@r8          /* *fb = dpix */
        bf/s    do_col_loop
        add     r1,r8           /* fb += SCREENWIDTH */

        mov.l   @r15+,r10
        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8

! Draw a vertical column of pixels from a projected wall texture.
! Non-power of 2 texture height.
!
!void I_DrawColumnNPO2(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!                      fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)
        .align 2
        .global _I_DrawColumnNPo2LowA
_I_DrawColumnNPo2LowA:

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

        .p2alignw 2, 0x0009
do_cnp_loop:
        mov     r2,r0
        shlr16  r0              /* frac >> 16 */
        mov.b   @(r0,r5),r0     /* pix = dc_source[frac >> 16] */
        cmp/ge  r4,r2
        mov.b   @(r0,r7),r0     /* dpix = dc_colormap[pix] */
        bf/s    1f
        add     r3,r2           /* frac += fracstep */
        /* if (frac >= heightmask) */
        sub     r4,r2           /* frac -= heightmask */
1:
        dt      r6              /* count-- */
        extu.b  r0,r0
        swap.b  r0,r9
        or      r9,r0
        mov.w   r0,@r8          /* *fb = dpix */
        bf/s    do_cnp_loop
        add     r1,r8           /* fb += SCREENWIDTH */

        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8

! Draw a vertical column of distorted background pixels.
! Source is the top of the column to scale.
! Low detail (doubl-wide pixels) mode.
!
!void I_DrawFuzzColumn(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!       fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)
        .align 2
        .global _I_DrawFuzzColumnLowA
_I_DrawFuzzColumnLowA:

        mov     #0,r0
        cmp/eq  r0,r5
        bf      0f
        mov     #1,r5
0:
        mov.l   draw_height,r0
        mov.w   @r0,r0
        add     #-2,r0
        cmp/gt  r0,r6
        bf      1f
        mov     r0,r6
1:
	add	#1,r6
2:
        cmp/ge  r6,r5
        bf/s    3f
        sub     r5,r6           /* count = dc_yh - dc_yl */

        /* dc_yl >= dc_yh, exit */
        rts
        nop
3:
        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        mov.l   r8,@-r15
        mov.l   r9,@-r15
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8           /* fb += dc_x */
        add     r4,r8           /* fb += dc_x */
        shll8   r5
        add     r5,r8
        shlr2   r5
        add     r5,r8           /* fb += (dc_yl*256 + dc_yl*64) */
        mov.l   draw_fuzzoffset,r5
        mov.l   draw_width,r1
        mov.l   @(DOOMTLS_FUZZPOS, gbr),r0 /* pfuzzpos */
        add     r0,r0
        mov     r0,r3
        and     #126,r0         /* fuzzpos &= FUZZMASK */

        .p2alignw 2, 0x0009
do_fuzz_col_loop:
        mov.w   @(r0,r5),r0     /* pix = fuzztable[fuzzpos] */
        add     #2,r3
        mov.b   @(r0,r8),r0     /* pix = dest[pix] */
        dt      r6              /* count-- */
        mov.b   @(r0,r7),r4     /* dpix = dc_colormap[pix] */
        mov     r3,r0
        and     #126,r0         /* fuzzpos &= FUZZMASK */
        extu.b  r4,r4
        swap.b  r4,r9
        or      r9,r4
        mov.w   r4,@r8          /* *fb = dpix */
        bf/s    do_fuzz_col_loop
        add     r1,r8           /* fb += SCREENWIDTH */

        shlr    r3
        mov     r3,r0
        mov.l   r0,@(DOOMTLS_FUZZPOS, gbr)
        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8

! Draw a horizontal row of pixels from a projected flat (floor/ceiling) texture.
!
!void I_DrawSpan(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
!                fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep,
!                inpixel_t *ds_source, int ds_height)
        .align 2
        .global _I_DrawSpanLowA
_I_DrawSpanLowA:

	add	#1,r6
0:
        cmp/ge  r6,r5
        bf/s    1f
        sub     r5,r6           /* count = ds_x2 - ds_x1 */

        /* dc_yl >= dc_yh, exit */
        rts
        nop

1:
        mov.l   r8,@-r15
        mov.l   r9,@-r15
        mov.l   r10,@-r15
        mov.l   r11,@-r15
        mov.l   r12,@-r15
        mov.l   r13,@-r15
        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        add     r0,r7           /* ds_colormap = colormap + light */
        mov.l   draw_fb,r8

        mov.l   @r8,r8          /* frame buffer start */
        add     r5,r8
        add     r5,r8
        add     r6,r8
        add     r6,r8
        shll8   r4
        add     r4,r8
        shlr2   r4
        add     r4,r8           /* fb += (ds_y*256 + ds_y*64) */

        mov.l   @(40,r15),r9    /* ds_source */
        mov.l   @(44,r15),r11   /* ds_height */

        mov     r11,r12
        dt      r12             /* (ds_height-1) */
        mulu.w  r12,r11         /* (ds_height-1) * ds_height */

        mov.l   @(24,r15),r2    /* xfrac */
        mov.l   @(32,r15),r3    /* xstep */

        sts     macl,r11        /* draw_flat_ymask */

        dmuls.l r3,r6

        mov.l   @(28,r15),r4    /* yfrac */
        mov.l   @(36,r15),r5    /* ystep */

        sts     macl,r10
        add     r10,r2          /* xfrac += xstep * count */

        dmuls.l r5,r6

        swap.w  r2,r0           /* (xfrac >> 16) */
        and     r12,r0          /* (xfrac >> 16) & 63 */

        sts     macl,r10
        add     r10,r4          /* yfrac += ystep * count */

        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */

        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */

        /* test if count & 1 */
        shlr    r6
        movt    r1              /* 1 if count was odd */
        bt/s    do_span_loop_1px
        add     r1,r6

        .p2alignw 2, 0x0009
do_span_loop:
        mov.b   @(r0,r9),r0     /* pix = ds_source[spot] */
        sub     r3,r2           /* xfrac -= xstep */
        sub     r5,r4           /* yfrac -= ystep */
        swap.w  r4,r1           /* (yfrac >> 16) */
        mov.b   @(r0,r7),r10    /* dpix = ds_colormap[pix] */
        swap.w  r2,r0           /* (xfrac >> 16) */
        and     r12,r0          /* (xfrac >> 16) & 63 */
        extu.b  r10,r10
        swap.b  r10,r13
        or      r13,r10
        mov.w   r10,@-r8        /* *--fb = dpix */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */
do_span_loop_1px:
        dt      r6              /* count-- */
        mov.b   @(r0,r9),r0     /* pix = ds_source[spot] */
        sub     r3,r2           /* xfrac -= xstep */
        sub     r5,r4           /* yfrac -= ystep */
        swap.w  r4,r1           /* (yfrac >> 16) */
        mov.b   @(r0,r7),r10    /* dpix = ds_colormap[pix] */
        swap.w  r2,r0           /* (xfrac >> 16) */
        and     r12,r0          /* (xfrac >> 16) & 63 */
        extu.b  r10,r10
        swap.b  r10,r13
        or      r13,r10
        mov.w   r10,@-r8        /* *--fb = dpix */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        bf/s    do_span_loop
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */

        mov.l   @r15+,r13
        mov.l   @r15+,r12
        mov.l   @r15+,r11
        mov.l   @r15+,r10
        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8

        .align  2
draw_fb:
        .long   _viewportbuffer
draw_width:
        .long   320
draw_height:
        .long   _viewportHeight
draw_fuzzoffset:
        .long   _fuzzoffset
