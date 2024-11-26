! Original code by Chilly Willy

.section .sdata

.equ DOOMTLS_COLORMAP, 16

!=======================================================================
!Standard column draw functions use the following data sources:
!
! r4 = dc_x
! r5 = dc_yl
! r6 = dc_yh
! r7 = light
! @(0,r15) = frac
! @(4,r15) = fracstep
! @(8,r15) = dc_source
! @(12,r15) = dc_texheight
!=======================================================================


! Draw a vertical column of pixels from a projected wall texture.
! Source is the top of the column to scale.
! Low detail (doubl-wide pixels) mode.
!
!void I_Draw32XSkyColumnLow(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!                  fixed_t fracstep, inpixel_t *dc_source, int dc_y_offset)

        .align  4
        .global _I_Draw32XSkyColumnLowA
_I_Draw32XSkyColumnLowA:
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
        mov.l   r12,@-r15
        mov.l   r13,@-r15
!        mov.l   r14,@-r15       /* TESTING TESTING TESTING TESTING TESTING TESTING */

!        mov     #0,r14          /* TESTING TESTING TESTING TESTING TESTING TESTING */

        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        mov     r0,r7
        mov.l   draw_fb_2,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8
        add     r4,r8           /* fb += dc_x*2 */
        mov     r8,r12          /* store frame buffer offset without y offset */
        shll8   r5
        add     r5,r8
        shlr2   r5
        add     r5,r8           /* fb += (dc_yl*256 + dc_yl*64) */
        mov.l   @(20,r15),r2    /* frac */
        mov.l   @(24,r15),r3    /* fracstep */
        mov.l   @(28,r15),r5    /* dc_source */
        mov.l   @(32,r15),r4    /* y_offset */

        mov.l   draw_width_2,r1

        mov     #80,r9
        add     r4,r5           /* adjust sky position */
        sub     r4,r9

        swap.w  r2,r0           /* (frac >> 16) */

        /*
        r1 = screen width (320) (never changes)
        r4 = height mask (127) (never changes)
        r5 = starting y position for source (top of seg)
        r6 = row count down to zero (zero = bottom of seg)
        r8 = frame buffer offset
        r9 = pixel fetched from source

        r5 = start of seg
        r6 = cursor
        r10 = start of bottom fill
        r11 = end of seg
        r12 = frame buffer offset (without Y offset)
        */

        mov     r6,r10
        sub     r9,r10          /* Subtract area above seg from r10 */

        mov     #0,r9
        cmp/ge  r9,r4           /* Skip the top fill if it's not needed */
        bt/s    do_32xsky_middle_col_pre_loop
        nop



do_32xsky_top_col_pre_loop:
        mov.w   top_fill_color,r13
        
        /* test if count & 1 */
        neg     r4,r4
        add     r4,r5
        sub     r4,r6
!        mov     #1,r14          /* TESTING TESTING TESTING TESTING TESTING TESTING */
        shlr    r4
        movt    r9              /* 1 if count was odd */
        bt/s    do_32xsky_fill_top_col_loop_low_1px
        add     r9,r4

        .p2alignw 2, 0x0009
do_32xsky_fill_top_col_loop_low:
        mov.w   r13,@r12         /* *fb = dpix */ /* TODO: DLG: This will fail on real hardware at odd addresses. */
        add     r1,r12          /* fb += SCREENWIDTH */
do_32xsky_fill_top_col_loop_low_1px:
        dt      r4              /* count-- */
        mov.w   r13,@r12         /* *fb = dpix */ /* TODO: DLG: This will fail on real hardware at odd addresses. */
        bf/s    do_32xsky_fill_top_col_loop_low
        add     r1,r12          /* fb += SCREENWIDTH */

        mov     r12,r8


do_32xsky_middle_col_pre_loop:
        /* test if count & 1 */
        shlr    r10
        movt    r9
        add     r9,r10
        shlr    r6
        movt    r9              /* 1 if count was odd */
        bt/s    do_32xsky_middle_col_loop_low_1px
        add     r9,r6
        nop

        .p2alignw 2, 0x0009
do_32xsky_middle_col_loop_low:
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r0,r0
        mov.w   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        add     r3,r2           /* frac += fracstep */
        swap.w  r2,r0           /* (frac >> 16) */
        mov.w   r9,@r8          /* *fb = dpix */
        add     r1,r8           /* fb += SCREENWIDTH */
do_32xsky_middle_col_loop_low_1px:
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r0,r0
        mov.w   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        add     r3,r2           /* frac += fracstep */
        dt      r6              /* count-- */
        swap.w  r2,r0           /* (frac >> 16) */
        mov.w   r9,@r8          /* *fb = dpix */
        add     r1,r8           /* fb += SCREENWIDTH */

        mov     #0,r9
        cmp/eq  r9,r6
        bt/s    do_32xsky_done
        nop
        cmp/eq  r10,r6
        bf/s    do_32xsky_middle_col_loop_low
        nop



!        mov     #0,r9
!        cmp/eq  r9,r14
!        bt/s    do_32xsky_done
!        nop
!        mov     #2,r14

!        .p2alignw 2, 0x0009
!test_01:
!        nop
!        nop
!        bra     test_01
!        nop



do_32xsky_bottom_col_pre_loop:
        mov.w   bottom_fill_color,r13
!        mov     r6,r9   /* TESTING TESTING TESTING TESTING TESTING TESTING */
!        mov     r8,r10  /* TESTING TESTING TESTING TESTING TESTING TESTING */

        .p2alignw 2, 0x0009
do_32xsky_fill_bottom_col_loop_low:
        mov.w   r13,@r8          /* *fb = dpix */ /* TODO: DLG: This will fail on real hardware at odd addresses. */
        add     r1,r8           /* fb += SCREENWIDTH */
do_32xsky_fill_bottom_col_loop_low_1px:
        dt      r6              /* count-- */
        mov.w   r13,@r8          /* *fb = dpix */ /* TODO: DLG: This will fail on real hardware at odd addresses. */
        bf/s    do_32xsky_fill_bottom_col_loop_low
        add     r1,r8           /* fb += SCREENWIDTH */



        .p2alignw 2, 0x0009
do_32xsky_done:
!        mov.l   @r15+,r14       /* TESTING TESTING TESTING TESTING TESTING TESTING */
        mov.l   @r15+,r13
        mov.l   @r15+,r12
        mov.l   @r15+,r10
        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8



        .align  4
top_fill_color:
        !.short  0x8888
        .short  0x8989
bottom_fill_color:
        !.short  0x8E8E
        .short  0x8F8F
draw_fb_2:
        .long   _viewportbuffer
draw_width_2:
        .long   320


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

do_col_pre_loop:
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
        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        add     r7,r7
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
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
        bt/s    do_col_loop_1px
        add     r9,r6

        .p2alignw 2, 0x0009
do_col_loop:
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r0,r0
        mov.w   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        add     r3,r2           /* frac += fracstep */
        swap.w  r2,r0           /* (frac >> 16) */
        and     r4,r0           /* (frac >> 16) & heightmask */
        mov.b   r9,@r8          /* *fb = dpix */
        add     r1,r8           /* fb += SCREENWIDTH */
do_col_loop_1px:
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r0,r0
        mov.w   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        add     r3,r2           /* frac += fracstep */
        dt      r6              /* count-- */
        swap.w  r2,r0           /* (frac >> 16) */
        mov.b   r9,@r8          /* *fb = dpix */
        and     r4,r0           /* (frac >> 16) & heightmask */
        bf/s    do_col_loop
        add     r1,r8           /* fb += SCREENWIDTH */

        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8

! Draw a vertical column of pixels from a projected wall texture UPSIDE DOWN.
! Source is the top of the column to scale.
!
!void I_DrawColumnFlipped(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac,
!                  fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)

        .align  4
        .global _I_DrawColumnFlippedA
_I_DrawColumnFlippedA:
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
        bt/s    do_col_flipped_loop_1px
        add     r9,r6

        .p2alignw 2, 0x0009
do_col_flipped_loop:
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r0,r0
        mov.w   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        add     r3,r2           /* frac += fracstep */
        swap.w  r2,r0           /* (frac >> 16) */
        and     r4,r0           /* (frac >> 16) & heightmask */
        mov.b   r9,@r8          /* *fb = dpix */
        sub     r1,r8           /* fb += SCREENWIDTH */
do_col_flipped_loop_1px:
        mov.b   @(r0,r5),r0     /* pix = dc_source[(frac >> 16) & heightmask] */
        add     r0,r0
        mov.w   @(r0,r7),r9     /* dpix = dc_colormap[pix] */
        add     r3,r2           /* frac += fracstep */
        dt      r6              /* count-- */
        swap.w  r2,r0           /* (frac >> 16) */
        mov.b   r9,@r8          /* *fb = dpix */
        and     r4,r0           /* (frac >> 16) & heightmask */
        bf/s    do_col_flipped_loop
        sub     r1,r8           /* fb += SCREENWIDTH */

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
        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        add     r7,r7
        add     r0,r7           /* dc_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
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
do_cnp_loop:
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
        bf/s    do_cnp_loop
        mov.b   r0,@r8          /* *fb = dpix */

        rts
        mov.l   @r15+,r8

! Draw a horizontal row of pixels from a projected flat (floor/ceiling) texture.
!
!void I_DrawSpan(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
!                fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep,
!                inpixel_t *ds_source, int ds_height)

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
        mov.l   @(DOOMTLS_COLORMAP, gbr),r0
        add     r0,r7           /* ds_colormap = colormap + light */
        mov.l   draw_fb,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r5,r8
        shll8   r4
        add     r4,r8
        shlr2   r4
        add     r4,r8           /* fb += (ds_y*256 + ds_y*64) */
        mov.l   @(40,r15),r11   /* ds_height */
        mov.l   @(20,r15),r2    /* xfrac */
        mov.l   @(24,r15),r4    /* yfrac */
        mov.l   @(28,r15),r3    /* xstep */
        mov.l   @(32,r15),r5    /* ystep */
        mov.l   @(36,r15),r9    /* ds_source */

        mov     r11,r12
        dt      r12             /* (ds_height-1) */
        mulu.w  r12,r11         /* (ds_height-1) * ds_height */

        swap.w  r2,r0           /* (xfrac >> 16) */
        and     r12,r0          /* (xfrac >> 16) & 63 */
        sts     macl,r11        /* draw_flat_ymask */

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
        add     r3,r2           /* xfrac += xstep */
        add     r5,r4           /* yfrac += ystep */
        swap.w  r4,r1           /* (yfrac >> 16) */
        mov.b   @(r0,r7),r10    /* dpix = ds_colormap[pix] */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        swap.w  r2,r0           /* (xfrac >> 16) */
        and     r12,r0          /* (xfrac >> 16) & 63 */
        mov.b   r10,@r8         /* *fb = dpix */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */
        add     #1,r8           /* fb++ */
do_span_loop_1px:
        add     r3,r2           /* xfrac += xstep */
        mov.b   @(r0,r9),r0     /* pix = ds_source[spot] */
        add     r5,r4           /* yfrac += ystep */
        swap.w  r4,r1           /* (yfrac >> 16) */
        and     r11,r1          /* (yfrac >> 16) & 63*64 */
        mov.b   @(r0,r7),r10    /* dpix = ds_colormap[pix] */
        swap.w  r2,r0           /* (xfrac >> 16) */
        and     r12,r0          /* (xfrac >> 16) & 63 */
        or      r1,r0           /* spot = ((yfrac >> 16) & *64) | ((xfrac >> 16) & 63) */
        mov.b   r10,@r8         /* *fb = dpix */

        dt      r6              /* count-- */
        bf/s    do_span_loop
        add     #1,r8           /* fb++ */

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


! Clear a vertical column of pixels for the MD sky to show through.
!
!void I_DrawSkyColumn(int dc_x, int dc_yl, int dc_yh)

        .align  4
        .global _I_DrawSkyColumnA
_I_DrawSkyColumnA:
        cmp/ge  r6,r5
        bf/s    1f
        sub     r5,r6           /* count = dc_yh - dc_yl */

        /* dc_yl >= dc_yh, exit */
        rts
        nop
1:
        mov.l   r8,@-r15
        mov.l   r9,@-r15
        mov.l   draw_fb2,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8           /* fb += dc_x */
        shll8   r5
        add     r5,r8
        shlr2   r5
        add     r5,r8           /* fb += (dc_yl*256 + dc_yl*64) */
        mov.l   draw_width2,r1
        mov.w   thru_pal_index_double,r7     /* dpix = dc_colormap[pix] */

        /* test if count & 1 */
        shlr    r6
        movt    r9              /* 1 if count was odd */
        bt/s    do_sky_col_loop_1px
        add     r9,r6

        .p2alignw 2, 0x0009
do_sky_col_loop:
        mov.w   r7,@r8          /* *fb = dpix */ /* TODO: DLG: This will fail on real hardware at odd addresses. */
        add     r1,r8           /* fb += SCREENWIDTH */
do_sky_col_loop_1px:
        dt      r6              /* count-- */
        mov.w   r7,@r8          /* *fb = dpix */ /* TODO: DLG: This will fail on real hardware at odd addresses. */
        bf/s    do_sky_col_loop
        add     r1,r8           /* fb += SCREENWIDTH */

        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8


! Clear a vertical column of pixels for the MD sky to show through.
! Low detail (double-wide pixels) mode.
!
!void I_DrawSkyColumnLow(int dc_x, int dc_yl, int dc_yh)

        .align  4
        .global _I_DrawSkyColumnLowA
_I_DrawSkyColumnLowA:
        cmp/ge  r6,r5
        bf/s    1f
        sub     r5,r6           /* count = dc_yh - dc_yl */

        /* dc_yl >= dc_yh, exit */
        rts
        nop
1:
        mov.l   r8,@-r15
        mov.l   r9,@-r15
        mov.l   draw_fb2,r8
        mov.l   @r8,r8          /* frame buffer start */
        add     r4,r8
        add     r4,r8           /* fb += dc_x*2 */
        shll8   r5
        add     r5,r8
        shlr2   r5
        add     r5,r8           /* fb += (dc_yl*256 + dc_yl*64) */
        mov.l   draw_width2,r1
        mov.w   thru_pal_index_double,r7     /* dpix = dc_colormap[pix] */

        /* test if count & 1 */
        shlr    r6
        movt    r9              /* 1 if count was odd */
        bt/s    do_sky_col_loop_low_1px
        add     r9,r6

        .p2alignw 2, 0x0009
do_sky_col_loop_low:
        mov.w   r7,@r8          /* *fb = dpix */
        add     r1,r8           /* fb += SCREENWIDTH */
do_sky_col_loop_low_1px:
        dt      r6              /* count-- */
        mov.w   r7,@r8          /* *fb = dpix */
        bf/s    do_sky_col_loop_low
        add     r1,r8           /* fb += SCREENWIDTH */

        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8


        .align  2
draw_fb2:
        .long   _viewportbuffer
draw_width2:
        .long   320
thru_pal_index_double:
        .short  0xFCFC
