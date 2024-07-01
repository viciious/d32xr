! Original code by Chilly Willy

.section .sdata

.equ DOOMTLS_COLORMAP, 16
.equ DOOMTLS_FUZZPOS,  20

! Draw a vertical column of pixels from a projected wall texture.
! Source is the top of the column to scale.
! Low detail (doubl-wide pixels) mode.
!
!void I_DrawColumnLow(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!                  fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)

        .align  4
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
        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        add     r7,r7
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8
        add     r4,r8           /* fb += dc_x*2 */
        shll8   r5
        add     r5,r8
        shlr2   r5
        add     r5,r8           /* fb += (dc_yl*256 + dc_yl*64) */
        mov.l   @(8,r15),r2     /* frac */
        mov.l   @(12,r15),r3    /* fracstep */
        mov.l   @(16,r15),r5    /* dc_source */
        mov.l   @(20,r15),r4
        mov.l   draw_width,r1
        add     #-1,r4          /* heightmask = texheight - 1 */
        swap.w  r2,r0           /* (frac >> 16) */
        and     r4,r0           /* (frac >> 16) & heightmask */

        /* test if count & 1 */
        shlr    r6
        movt    r9              /* 1 if count was odd */
        bt/s    do_col_loop_low_1px
        add     r9,r6

        .p2alignw 2, 0x0009
do_col_loop_low:
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r0,r0
        mov.w   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        add     r3,r2           /* frac += fracstep */
        swap.w  r2,r0           /* (frac >> 16) */
        and     r4,r0           /* (frac >> 16) & heightmask */
        mov.w   r9,@r8          /* *fb = dpix */
        add     r1,r8           /* fb += SCREENWIDTH */
do_col_loop_low_1px:
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r0,r0
        mov.w   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        add     r3,r2           /* frac += fracstep */
        dt      r6              /* count-- */
        swap.w  r2,r0           /* (frac >> 16) */
        mov.w   r9,@r8          /* *fb = dpix */
        and     r4,r0           /* (frac >> 16) & heightmask */
        bf/s    do_col_loop_low
        add     r1,r8           /* fb += SCREENWIDTH */

        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8

! Draw a vertical column of pixels from a projected wall texture.
! Non-power of 2 texture height.
! Low detail (doubl-wide pixels) mode.
!
!void I_DrawColumnNPO2Low(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!                      fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)

        .align  4
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
        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        add     r7,r7
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8
        add     r4,r8           /* fb += dc_x*2 */
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

        .p2alignw 2, 0x0009
do_cnp_loop_low:
        mov     r2,r0
        shlr16  r0              /* frac >> 16 */
        mov.b   @(r0,r5),r0     /* pix = dc_source[frac >> 16] */
        add     r1,r8           /* fb += SCREENWIDTH */
        add     r3,r2           /* frac += fracstep */
        cmp/ge  r4,r2
        add     r0,r0
        bf/s    1f
        mov.w   @(r0,r7),r0     /* dpix = dc_colormap[pix] */
        /* if (frac >= heightmask) */
        sub     r4,r2           /* frac -= heightmask */
1:
        dt      r6              /* count-- */
        bf/s    do_cnp_loop_low
        mov.w   r0,@r8          /* *fb = dpix */

        rts
        mov.l   @r15+,r8

! Draw a vertical column of distorted background pixels.
! Source is the top of the column to scale.
! Low detail (doubl-wide pixels) mode.
!
!void I_DrawFuzzColumnLow(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!       fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)

        .align  4
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
        add     r7,r7
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8
        add     r4,r8           /* fb += dc_x*2 */
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
do_fuzz_col_loop_low:
        mov.w   @(r0,r5),r0     /* pix = fuzztable[fuzzpos] */
        dt      r6              /* count-- */
        mov.b   @(r0,r8),r0     /* pix = dest[pix] */
        add     r0,r0
        mov.w   @(r0,r7),r4     /* dpix = dc_colormap[pix] */
        add     #2,r3
        mov     r3,r0
        and     #126,r0         /* fuzzpos &= FUZZMASK */
        mov.w   r4,@r8          /* *fb = dpix */
        bf/s    do_fuzz_col_loop_low
        add     r1,r8           /* fb += SCREENWIDTH */

        shlr    r3
        mov     r3,r0
        mov.l   r0,@(DOOMTLS_FUZZPOS, gbr)
        rts
        mov.l   @r15+,r8

! Draw a horizontal row of pixels from a projected flat (floor/ceiling) texture.
! Low detail (doubl-wide pixels) mode.
!
!void I_DrawSpanLow(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
!                fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep,
!                inpixel_t *ds_source, int ds_height)

        .align  4
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
        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        add     r7,r7
        add     r0,r7           /* ds_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r5,r8
        add     r5,r8           /* fb += ds_x1*2 */
        shll8   r4
        add     r4,r8
        shlr2   r4
        add     r4,r8           /* fb += (ds_y*256 + ds_y*64) */
        mov.l   @(20,r15),r2    /* xfrac */
        mov.l   @(24,r15),r4    /* yfrac */
        mov.l   @(28,r15),r3    /* xstep */
        mov.l   @(32,r15),r5    /* ystep */
        mov.l   @(36,r15),r9    /* ds_source */
        mov.l   @(40,r15),r12   /* ds_height */

        mov     r12,r11
        dt      r11
        mulu.w  r12,r11         /* (ds_height-1) * ds_height */
        dt      r12             /* (ds_height-1) */
        sts     macl,r11        /* draw_flat_ymask */

        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        swap.w  r2,r0           /* (xfrac >> 16) */
        and     r12,r0          /* (xfrac >> 16) & 63 */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */

        /* test if count & 1 */
        shlr    r6
        movt    r1              /* 1 if count was odd */
        bt/s    do_span_low_loop_1px
        add     r1,r6

        .p2alignw 2, 0x0009
do_span_low_loop:
        mov.b   @(r0,r9),r0     /* pix = ds_source[spot] */
        add     r3,r2           /* xfrac += xstep */
        add     r5,r4           /* yfrac += ystep */
        add     r0,r0
        mov.w   @(r0,r7),r10    /* dpix = ds_colormap[pix] */
        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        swap.w  r2,r0           /* (xfrac >> 16) */
        and     r12,r0          /* (xfrac >> 16) & 63 */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */
        mov.w   r10,@r8         /* *fb = dpix */
        add     #2,r8           /* fb++ */

do_span_low_loop_1px:
        mov.b   @(r0,r9),r0     /* pix = ds_source[spot] */
        add     r3,r2           /* xfrac += xstep */
        add     r5,r4           /* yfrac += ystep */
        add     r0,r0
        mov.w   @(r0,r7),r10    /* dpix = ds_colormap[pix] */
        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        swap.w  r2,r0           /* (xfrac >> 16) */
        and     r12,r0          /* (xfrac >> 16) & 63 */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */
        mov.w   r10,@r8         /* *fb = dpix */

        dt      r6              /* count-- */
        bf/s    do_span_low_loop
        add     #2,r8           /* fb++ */

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
draw_flat_ymask:
        .long   4032
draw_fuzzoffset:
        .long   _fuzzoffset
