! Original code by Chilly Willy

.data

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
        mov.l   draw_cmap,r0
        mov.l   @r0,r0
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

        .p2alignw 2, 0x0009
do_col_loop_low:
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r3,r2           /* frac += fracstep */
        dt      r6              /* count-- */
        add     r0,r0
        mov.w   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
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
        mov.l   draw_cmap,r0
        mov.l   @r0,r0
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
!       fixed_t fracstep, inpixel_t *dc_source, int dc_texheight, int* pfuzzpos)

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
        mov.l   draw_cmap,r0
        mov.l   r8,@-r15
        mov.l   @r0,r0
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
        mov.l   @(20,r15),r2    /* pfuzzpos */
        mov.l   @r2,r3          /* fuzzpos */
        sub     r1,r8           /* fb -= SCREENWIDTH */

        .p2alignw 2, 0x0009
do_fuzz_col_loop_low:
        mov     r3,r0
        and     #63,r0          /* fuzzpos &= FUZZMASK */
        add     r0,r0
        mov.w   @(r0,r5),r0     /* pix = fuzztable[fuzzpos] */
        add     r1,r8           /* fb += SCREENWIDTH */
        add     r0,r0
        mov.b   @(r0,r8),r0     /* pix = dest[pix] */
        add     #1,r3
        add     r0,r0
        dt      r6              /* count-- */
        mov.w   @(r0,r7),r4     /* dpix = dc_colormap[pix] */
        bf/s    do_fuzz_col_loop_low
        mov.w   r4,@r8          /* *fb = dpix */

        mov.l   r3,@r2
        rts
        mov.l   @r15+,r8

! Draw a horizontal row of pixels from a projected flat (floor/ceiling) texture.
! Low detail (doubl-wide pixels) mode.
!
!void I_DrawSpanLow(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
!                fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep,
!                inpixel_t *ds_source)

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
        mov.l   r13,@-r15
        mov.l   draw_cmap,r0
        mov.l   @r0,r0
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
        mov.l   @(24,r15),r2    /* xfrac */
        mov.l   @(28,r15),r4    /* yfrac */
        mov.l   @(32,r15),r3    /* xstep */
        mov.l   @(36,r15),r5    /* ystep */
        mov.l   @(40,r15),r9    /* ds_source */
        mov.l   draw_flat_ymask,r11

        /* test if dst & 2 == 2 */
        mov     r8,r0
        tst     #2,r0
        bt      begin_span_low_loop

        /* draw 1px so that dst & 1 == 0 afterwards */
        swap.w  r2,r0           /* (xfrac >> 16) */
        and     #63,r0          /* (xfrac >> 16) & 63 */
        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */

        .p2alignw 2, 0x0009
draw_span_low_1px:
        mov.b   @(r0,r9),r0     /* pix = ds_source[spot] */
        add     r3,r2           /* xfrac += xstep */
        add     r5,r4           /* yfrac += ystep */
        add     r0,r0
        mov.w   @(r0,r7),r0     /* dpix = ds_colormap[pix] */
        dt      r6              /* count-- */
        mov.w   r0,@r8          /* *fb = dpix */
        bt/s    exit_span_low_loop
        add     #2,r8           /* fb++ */

begin_span_low_loop:
        swap.w  r2,r0           /* (xfrac >> 16) */
        and     #63,r0          /* (xfrac >> 16) & 63 */
        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */

        /* test if count == 1 */
        mov     #1,r12
        cmp/eq  r12,r6
        bt/s    draw_span_low_1px
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */

        /* count = count / 2 */
        shlr    r6
        movt    r12             /* 1 if count was odd */

        .p2alignw 2, 0x0009
do_span_low_loop:
        mov.b   @(r0,r9),r0     /* pix = ds_source[spot] */
        add     r3,r2           /* xfrac += xstep */
        add     r5,r4           /* yfrac += ystep */
        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        add     r0,r0
        mov.w   @(r0,r7),r13    /* dpix = ds_colormap[pix] */

        swap.w  r2,r0           /* (xfrac >> 16) */
        and     #63,r0          /* (xfrac >> 16) & 63 */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */
        mov.b   @(r0,r9),r0     /* pix = ds_source[spot] */
        add     r3,r2           /* xfrac += xstep */
        add     r5,r4           /* yfrac += ystep */
        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        add     r0,r0
        mov.w   @(r0,r7),r10    /* dpix = ds_colormap[pix] */

        swap.w  r2,r0           /* (xfrac >> 16) */
        and     #63,r0          /* (xfrac >> 16) & 63 */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */

        dt      r6              /* count-- */

        shll16  r13
        extu.w  r10,r10
        or      r13,r10
        mov.l   r10,@r8         /* *fb = dpix */
        bf/s    do_span_low_loop
        add     #4,r8           /* fb += 2 */

        /* test if r12 is 0, draw additional pixel if it's not */
        tst     r12,r12
        bf/s    draw_span_low_1px   /* if r12 == 1, draw 1px */
        mov     #1,r6

exit_span_low_loop:
        mov.l   @r15+,r13
        mov.l   @r15+,r12
        mov.l   @r15+,r11
        mov.l   @r15+,r10
        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8

! Draw a vertical column of pixels from a projected wall texture.
! Source is the top of the column to scale.
!
!void I_DrawColumn(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!                  fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)

        .align  4
        .global _I_DrawColumnA
_I_DrawColumnA:
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
        mov.l   draw_cmap,r0
        mov.l   @r0,r0
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8           /* fb += dc_x */
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

        .p2alignw 2, 0x0009
do_col_loop:
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r3,r2           /* frac += fracstep */
        mov.b   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        dt      r6              /* count-- */
        swap.w  r2,r0           /* (frac >> 16) */
        and     r4,r0           /* (frac >> 16) & heightmask */
        mov.b   r9,@r8          /* *fb = dpix */
        bf/s    do_col_loop
        add     r1,r8           /* fb += SCREENWIDTH */

        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8

! Draw a vertical column of pixels from a projected wall texture.
! Non-power of 2 texture height.
!
!void I_DrawColumnNPO2(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!                      fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)

        .align  4
        .global _I_DrawColumnNPo2A
_I_DrawColumnNPo2A:
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
        mov.l   draw_cmap,r0
        mov.l   @r0,r0
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
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

        .p2alignw 2, 0x0009
do_cnp_loop:
        mov     r2,r0
        shlr16  r0              /* frac >> 16 */
        mov.b   @(r0,r5),r0     /* pix = dc_source[frac >> 16] */
        add     r3,r2           /* frac += fracstep */
        cmp/ge  r4,r2
        bf/s    1f
        mov.b   @(r0,r7),r0     /* dpix = dc_colormap[pix] */
        /* if (frac >= heightmask) */
        sub     r4,r2           /* frac -= heightmask */
1:
        add     r1,r8           /* fb += SCREENWIDTH */
        dt      r6              /* count-- */
        bf/s    do_cnp_loop
        mov.b   r0,@r8          /* *fb = dpix */

        rts
        mov.l   @r15+,r8


! Draw a vertical column of distorted background pixels.
! Source is the top of the column to scale.
! Low detail (doubl-wide pixels) mode.
!
!void I_DrawFuzzColumn(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!       fixed_t fracstep, inpixel_t *dc_source, int dc_texheight, int* pfuzzpos)

        .align  4
        .global _I_DrawFuzzColumnA
_I_DrawFuzzColumnA:
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
        mov.l   draw_cmap,r0
        mov.l   r8,@-r15
        mov.l   @r0,r0
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8           /* fb += dc_x */
        shll8   r5
        add     r5,r8
        shlr2   r5
        add     r5,r8           /* fb += (dc_yl*256 + dc_yl*64) */
        mov.l   draw_fuzzoffset,r5
        mov.l   draw_width,r1
        mov.l   @(20,r15),r2    /* pfuzzpos */
        mov.l   @r2,r3          /* fuzzpos */
        sub     r1,r8           /* fb -= SCREENWIDTH */

        .p2alignw 2, 0x0009
do_fuzz_col_loop:
        mov     r3,r0
        and     #63,r0          /* fuzzpos &= FUZZMASK */
        add     r0,r0
        add     r1,r8           /* fb += SCREENWIDTH */
        mov.w   @(r0,r5),r0     /* pix = fuzztable[fuzzpos] */
        add     #1,r3
        mov.b   @(r0,r8),r0     /* pix = dest[pix] */
        dt      r6              /* count-- */
        mov.b   @(r0,r7),r4     /* dpix = dc_colormap[pix] */
        bf/s    do_fuzz_col_loop
        mov.b   r4,@r8          /* *fb = dpix */

        mov.l   r3,@r2
        rts
        mov.l   @r15+,r8

! Draw a horizontal row of pixels from a projected flat (floor/ceiling) texture.
!
!void I_DrawSpan(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
!                fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep,
!                inpixel_t *ds_source)

        .align  4
        .global _I_DrawSpanA
_I_DrawSpanA:
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
        mov.l   draw_cmap,r0
        mov.l   @r0,r0
        add     r0,r7           /* ds_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r5,r8
        shll8   r4
        add     r4,r8
        shlr2   r4
        add     r4,r8           /* fb += (ds_y*256 + ds_y*64) */
        mov.l   @(24,r15),r2    /* xfrac */
        mov.l   @(28,r15),r4    /* yfrac */
        mov.l   @(32,r15),r3    /* xstep */
        mov.l   @(36,r15),r5    /* ystep */
        mov.l   @(40,r15),r9    /* ds_source */
        mov.l   draw_flat_ymask,r11

        /* test if dst & 1 == 1 */
        mov     r8,r0
        tst     #1,r0
        bt      begin_span_loop

        /* draw 1px so that dst & 1 == 0 afterwards */
        swap.w  r2,r0           /* (xfrac >> 16) */
        and     #63,r0          /* (xfrac >> 16) & 63 */
        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */

        .p2alignw 2, 0x0009
draw_span_1px:
        mov.b   @(r0,r9),r0     /* pix = ds_source[spot] */
        add     r3,r2           /* xfrac += xstep */
        mov.b   @(r0,r7),r0     /* dpix = ds_colormap[pix] */
        add     r5,r4           /* yfrac += ystep */
        mov.b   r0,@r8          /* *fb = dpix */
        dt      r6              /* count-- */
        bt/s    exit_span_loop
        add     #1,r8           /* fb++ */

begin_span_loop:
        swap.w  r2,r0           /* (xfrac >> 16) */
        and     #63,r0          /* (xfrac >> 16) & 63 */
        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */

        /* test if count == 1 */
        mov     #1,r12
        cmp/eq  r12,r6
        bt/s    draw_span_1px
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */

        /* count = count / 2 */
        shlr    r6
        movt    r12             /* 1 if count was odd */

        .p2alignw 2, 0x0009
do_span_loop:
        mov.b   @(r0,r9),r0     /* pix = ds_source[spot] */
        add     r3,r2           /* xfrac += xstep */
        add     r5,r4           /* yfrac += ystep */
        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        dt      r6              /* count-- */
        mov.b   @(r0,r7),r13    /* dpix = ds_colormap[pix] */

        swap.w  r2,r0           /* (xfrac >> 16) */
        and     #63,r0          /* (xfrac >> 16) & 63 */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */
        mov.b   @(r0,r9),r0     /* pix = ds_source[spot] */
        add     r3,r2           /* xfrac += xstep */
        add     r5,r4           /* yfrac += ystep */
        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        shll8   r13
        mov.b   @(r0,r7),r10    /* dpix = ds_colormap[pix] */

        swap.w  r2,r0           /* (xfrac >> 16) */
        and     #63,r0          /* (xfrac >> 16) & 63 */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */

        extu.b  r10,r10
        or      r13,r10
        mov.w   r10,@r8         /* *fb = dpix */
        bf/s    do_span_loop
        add     #2,r8           /* fb += 2 */

        /* test if r12 is 0, draw additional pixel if it's not */
        tst     r12,r12
        bf/s    draw_span_1px   /* if r12 == 1, draw 1px */
        mov     #1,r6

exit_span_loop:
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
draw_cmap:
        .long   _dc_colormaps		/* cached */
/*      .long   colormap|0x20000000   */
draw_width:
        .long   320
draw_height:
        .long   _viewportHeight
draw_flat_ymask:
        .long   4032
draw_fuzzoffset:
        .long   _fuzzoffset
