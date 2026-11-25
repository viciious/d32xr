! Sprite column/post loop for R_DrawVisSprite (r_phase8.c)
!
!void I_DrawSpriteColsA(int x, int stopx, unsigned short *spropening,
!    drawsprargs_t *drawargs);
!
! From drawargs, the prologue sets up in the local frame:
!   sprtop    = centerYFrac - FixedMul(FROMHEIGHT16(texturemid), spryscale)
!   light     = abs(colormap)
!
! for each column x in  [x, stopx) it reads the sprite opening, walks the
! patch posts and calls dcol(x, top, bottom, light, frac, iscale, src, 128).
!
! Register use (callee-saved, live across the drawer call):
!   r8  = drawargs       r9  = x
!   r10 = xfrac          r11 = &spropening[x] (post-incremented)
!   r12 = current post   r13 = topclip        r14 = bottomclip
!
! The drawers only read their stack arguments, so iscale and the texture
! height (128) are stored once; each post writes frac and src only.

.section .sdata

! drawsprargs_t fields, relative to r8
.equ DA_XFRAC,       0
.equ DA_FRACSTEP,    4
.equ DA_SPRYSCALE,   8
.equ DA_PATCH,      12
.equ DA_PIXELS,     16
.equ DA_DCOL,       20
.equ DA_TEXTUREMID, 24
.equ DA_COLORMAP,   26
.equ DA_WIDTHMASK,  28
.equ DA_MASKEDCOL,  32
.equ DA_SCALEFRAC,  36
.equ DA_SCALESTEP,  40

! locals, relative to r15 (0..15 are the drawer's stack args)
.equ LO_ISCALE,    4                    /* the drawer's iscale argument */
.equ LO_STOPX,    16
.equ LO_FRACBASE, 20
.equ LO_SPRTOP,   24
.equ LO_LIGHT,    28
.equ FRAMESIZE,   32

        .align  4
        .global _I_DrawSpriteColsA
_I_DrawSpriteColsA:
        mov.l   @(DA_SPRYSCALE,r7),r2
        mov     #-1,r1
        shll8   r1              /* r1 = divide unit, 0xFFFFFF00 */
        mov     #1,r0
        mov.l   r2,@r1          /* divisor = spryscale */
        mov.l   r0,@(16,r1)     /* dividend = FRACUNIT << 16 */
        mov     #0,r0
        mov.l   r0,@(20,r1)     /* start the divide */
        mov.l   r8,@-r15
        mov.l   r9,@-r15
        mov.l   r10,@-r15
        mov.l   r11,@-r15
        mov.l   r12,@-r15
        mov.l   r13,@-r15
        mov.l   r14,@-r15
        sts.l   pr,@-r15
        mov     r7,r8
        add     #-FRAMESIZE,r15

        mov.w   @(DA_TEXTUREMID,r8),r0
        mov     r4,r9
        shll8   r0
        shll2   r0
        shll2   r0              /* FROMHEIGHT16(texturemid) */
        dmuls.l r0,r2           /* FixedMul(.., spryscale) */
        mov.w   @(DA_COLORMAP,r8),r0
        mov.l   @(DA_XFRAC,r8),r10
        mov.l   r0,@(LO_LIGHT,r15) /* light = colormap */
        sts     mach,r2
        sts     macl,r0
        xtrct   r2,r0
        mov.l   centerYFrac,r2
        mov.l   @r2,r2
        sub     r0,r2           /* sprtop = centerYFrac - .. */
        mov.l   r2,@(LO_SPRTOP,r15)
        mov     #-128,r0
        neg     r0,r0
        mov.l   r0,@(12,r15)    /* texture height 128 */
        mov     r4,r11
        add     r11,r11
        add     r6,r11          /* &spropening[x] */
        mov.l   @(20,r1),r0     /* iscale, waits for the divide */
        neg     r2,r2
        dmuls.l r2,r0           /* fracbase = FixedMul(-sprtop, iscale) */
        mov.l   r5,@(LO_STOPX,r15)
        mov.l   r0,@(LO_ISCALE,r15)
        sts     mach,r1
        sts     macl,r0
        xtrct   r1,r0
        bra     col
        mov.l   r0,@(LO_FRACBASE,r15)

        .p2alignw 2, 0x0009
        /* column fully covered, empty, or done */
nextcol:
        mov.l   @(LO_STOPX,r15),r0
        add     #1,r9
        cmp/ge  r0,r9
        bt      exit

col:
        mov.w   @r11+,r0        /* opening: topclip << 8 | bottom */
        swap.w  r10,r1          /* xfrac >> 16 in the low half */
        mov.l   @(DA_FRACSTEP,r8),r2
        extu.w  r0,r13
        extu.b  r0,r14
        shlr8   r13             /* topclip */
        add     #-1,r14         /* bottomclip */
        cmp/gt  r14,r13
        bt/s    nextcol         /* column fully covered */
        add     r2,r10          /* xfrac += fracstep */

        mov.l   @(DA_PATCH,r8),r12
        exts.w  r1,r0           /* colnum = xfrac >> 16 */
        add     r0,r0
        add     r12,r0
        mov.w   @(6,r0),r0      /* columnofs[colnum], bit 15 = inline */
        mov     #-128,r3        /* for inline; fills the load-use slot */
        cmp/pz  r0
        bf/s    inline          /* inline column: out of line */
        add     r0,r12          /* first post */
        mov.b   @r12,r0
        cmp/eq  #-1,r0
        bt      nextcol         /* empty column */

        /* first or next post of a regular column */
dataofs:
        mov.b   @(2,r12),r0     /* dataofs, may be at an odd address */
        extu.b  r0,r3
        mov.b   @(3,r12),r0
        shll8   r3
        extu.b  r0,r0
        or      r0,r3
        mov.l   @(DA_PIXELS,r8),r0
        add     r0,r3           /* src */

        /* r3 = src, r12 = post */
post:
        mov.l   @(DA_SPRYSCALE,r8),r2
        mov.b   @r12,r1
        mov.l   @(LO_SPRTOP,r15),r5
        extu.b  r1,r1           /* topdelta */
        mov.b   @(1,r12),r0
        mul.l   r1,r2           /* topdelta * spryscale */
        and     #127,r0
        add     #1,r0           /* length + 1 */
        sts     macl,r6
        mul.l   r0,r2           /* (length + 1) * spryscale */
        add     r6,r5           /* top (16.16) */
        add     #-1,r5
        mov     r5,r6           /* bottom - 1, before the length */
        swap.w  r5,r5
        exts.w  r5,r5
        add     #1,r5           /* top = (top + FRACUNIT-1) >> 16 */
        sts     macl,r0
        add     r0,r6
        swap.w  r6,r6
        exts.w  r6,r6           /* bottom = (bottom - 1) >> 16 */

        /* posts go from top to bottom: once one starts below the
           opening, so do all the ones after it */
        cmp/gt  r14,r5
        bt/s    nextcol
        cmp/gt  r14,r6

        bf/s    1f
        cmp/gt  r5,r13
        mov     r14,r6          /* clip to bottom */
1:
        bf      2f
        mov     r13,r5          /* clip to top */
2:
        cmp/gt  r6,r5
        bt      postend         /* nothing left after clipping */

        /* frac = fracbase + top * iscale - (topdelta << 16), at least 0 */
        mov.l   @(LO_ISCALE,r15),r2
        mul.l   r5,r2
        mov.l   @(LO_FRACBASE,r15),r0
        shll16  r1
        mov.l   r3,@(8,r15)     /* src */
        sub     r1,r0
        mov.l   @(LO_LIGHT,r15),r7
        mov     r9,r4           /* x */
        sts     macl,r1
        add     r1,r0
        mov.l   @(DA_DCOL,r8),r2
        cmp/pz  r0              /* frac < 0 ? 0 : frac, no branch */
        movt    r1
        neg     r1,r1
        and     r1,r0
        jsr     @r2
        mov.l   r0,@r15         /* frac */

postend:
        mov.b   @(1,r12),r0
        cmp/pz  r0
        bt/s    dataofs         /* not the last post */
        add     #4,r12
        bra     nextcol         /* longword aligned target; the delay */
                                /* slot is the .p2alignw nop or inline's */
                                /* first insn (shll8 r3), both harmless */

        /* an inlined column: one post, pixels right after the header.
           columnofs was sign-extended, so the header is at r12 + 0x8000 */
        .p2alignw 2, 0x0009
inline:
        shll8   r3              /* -0x8000 (r3 = -128 from the column) */
        sub     r3,r12
        mov.b   @r12,r0
        cmp/eq  #-1,r0
        bt      nextcol         /* empty column */
        mov     r12,r3
        bra     post
        add     #2,r3

exit:
        add     #FRAMESIZE,r15
        lds.l   @r15+,pr
        mov.l   @r15+,r14
        mov.l   @r15+,r13
        mov.l   @r15+,r12
        mov.l   @r15+,r11
        mov.l   @r15+,r10
        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8

        .align  2
centerYFrac:
        .long   _centerYFrac

! Masked seg column/post loop for R_DrawMaskedSegRange (r_phase8.c)
!
!void I_DrawMaskedColsA(int x, int stopx, unsigned short *spropening,
!    drawsprargs_t *drawargs);
!
! Like I_DrawSpriteColsA, but the scale changes per column: for each column
! x in [x, stopx) with maskedcol[x] != OPENMARK it sets maskedcol[x] to
! OPENMARK, reads the opening, and with spryscale = scalefrac
!   iscale   = 0xFFFFFFFF / spryscale (divider, overlapped)
!   sprtop   = centerYFrac - FixedMul(FROMHEIGHT16(texturemid), spryscale)
!   fracbase = FixedMul(-sprtop, iscale)
!   light    = maskedcol[x] & OPENMARK
! walks the posts of column maskedcol[x] & widthmask and calls
! dcol(x, top, bottom, light, frac, iscale, src, 128).
!
! Register use (callee-saved, live across the drawer call):
!   r8  = drawargs       r9  = x
!   r10 = scalefrac      r11 = &spropening[x] (post-incremented)
!   r12 = current post   r13 = topclip        r14 = bottomclip

! locals, relative to r15 (0..15 are the drawer's stack args)
.equ ML_ISCALE,     4                   /* the drawer's iscale argument */
.equ ML_STOPX,     16
.equ ML_FRACBASE,  20
.equ ML_SPRTOP,    24
.equ ML_LIGHT,     28
.equ ML_SPRYSCALE, 32
.equ ML_MASKDELTA, 36                   /* maskedcol - spropening - 2, bytes */
.equ MFRAMESIZE,   40

        .align  4
        .global _I_DrawMaskedColsA
_I_DrawMaskedColsA:
        mov.l   r8,@-r15
        mov.l   r9,@-r15
        mov.l   r10,@-r15
        mov.l   r11,@-r15
        mov.l   r12,@-r15
        mov.l   r13,@-r15
        mov.l   r14,@-r15
        sts.l   pr,@-r15
        mov     r7,r8
        add     #-MFRAMESIZE,r15

        mov     #-128,r0
        neg     r0,r0
        mov.l   r0,@(12,r15)    /* texture height 128 */
        mov     r4,r9
        mov     r4,r11
        add     r11,r11
        add     r6,r11          /* &spropening[x] */
        mov.l   @(DA_MASKEDCOL,r8),r0
        add     #-2,r0
        sub     r6,r0           /* maskedcol[x] is @(r0,r11) after the opening read */
        mov.l   r0,@(ML_MASKDELTA,r15)
        mov.l   r5,@(ML_STOPX,r15)
        bra     mcol
        mov.l   @(DA_SCALEFRAC,r8),r10

        /* here so that the bt at the end of a column reaches it */
mexit:
        add     #MFRAMESIZE,r15
        lds.l   @r15+,pr
        mov.l   @r15+,r14
        mov.l   @r15+,r13
        mov.l   @r15+,r12
        mov.l   @r15+,r11
        mov.l   @r15+,r10
        mov.l   @r15+,r9
        rts
        mov.l   @r15+,r8

        .p2alignw 2, 0x0009
        /* column already drawn, fully covered, empty, or done */
mnextcol:
        mov.l   @(ML_STOPX,r15),r0
        add     #1,r9
        cmp/ge  r0,r9
        bt      mexit

mcol:
        mov.w   @r11+,r0        /* opening: topclip << 8 | bottom */
        mov     r10,r3          /* spryscale = scale of this column */
        mov.l   @(DA_SCALESTEP,r8),r2
        extu.w  r0,r13
        extu.b  r0,r14
        mov.l   @(ML_MASKDELTA,r15),r0
        add     r2,r10          /* scalefrac += scalestep */
        mov     #-1,r4
        mov.w   @(r0,r11),r1    /* maskedcol[x]: light << 8 | column */
        shll8   r4              /* 0xFFFFFF00: OPENMARK sign-extended, and */
                                /* the divide unit */
        shlr8   r13             /* topclip */
        cmp/eq  r4,r1
        bt/s    mnextcol        /* already drawn */
        add     #-1,r14         /* bottomclip */
        mov.w   r4,@(r0,r11)    /* maskedcol[x] = OPENMARK */
        cmp/gt  r14,r13
        bt      mnextcol        /* column fully covered */

        /* iscale = 0xFFFFFFFF / spryscale, overlapped with the lookups */
        mov     #0,r0
        mov.l   r3,@r4          /* divisor = spryscale */
        mov.l   r0,@(16,r4)     /* dividend high = 0 */
        mov     #-1,r0
        mov.l   r0,@(20,r4)     /* dividend low = -1, start the divide */
        extu.w  r1,r5
        and     r4,r5           /* light = maskedcol[x] & OPENMARK */
        mov.l   r5,@(ML_LIGHT,r15)
        mov.l   r3,@(ML_SPRYSCALE,r15)
        mov.w   @(DA_TEXTUREMID,r8),r0
        shll8   r0
        shll2   r0
        shll2   r0              /* FROMHEIGHT16(texturemid) */
        dmuls.l r0,r3           /* FixedMul(.., spryscale) */
        mov.w   @(DA_WIDTHMASK,r8),r0
        mov.l   @(DA_PATCH,r8),r12
        and     r1,r0           /* colnum */
        add     r0,r0
        add     r12,r0
        sts     mach,r6
        sts     macl,r7
        mov.w   @(6,r0),r0      /* columnofs[colnum], bit 15 = inline */
        xtrct   r6,r7
        mov.l   mcenterYFrac,r6
        mov.l   @r6,r6
        sub     r7,r6           /* sprtop = centerYFrac - .. */
        mov.l   r6,@(ML_SPRTOP,r15)
        neg     r6,r6
        mov.l   @(20,r4),r2     /* iscale, waits for the divide */
        dmuls.l r6,r2           /* fracbase = FixedMul(-sprtop, iscale) */
        mov.l   r2,@(ML_ISCALE,r15)
        cmp/pz  r0
        sts     mach,r6
        sts     macl,r7
        xtrct   r6,r7
        mov.l   r7,@(ML_FRACBASE,r15)
        bf/s    minline         /* inline column: out of line */
        add     r0,r12          /* first post */
        mov.b   @r12,r0
        cmp/eq  #-1,r0
        bt      mnextcol        /* empty column */

        /* first or next post of a regular column */
mdataofs:
        mov.b   @(2,r12),r0     /* dataofs, may be at an odd address */
        extu.b  r0,r3
        mov.b   @(3,r12),r0
        shll8   r3
        extu.b  r0,r0
        or      r0,r3
        mov.l   @(DA_PIXELS,r8),r0
        add     r0,r3           /* src */

        /* r3 = src, r12 = post */
mpost:
        mov.l   @(ML_SPRYSCALE,r15),r2
        mov.b   @r12,r1
        mov.l   @(ML_SPRTOP,r15),r5
        extu.b  r1,r1           /* topdelta */
        mov.b   @(1,r12),r0
        mul.l   r1,r2           /* topdelta * spryscale */
        and     #127,r0
        add     #1,r0           /* length + 1 */
        sts     macl,r6
        mul.l   r0,r2           /* (length + 1) * spryscale */
        add     r6,r5           /* top (16.16) */
        add     #-1,r5
        mov     r5,r6           /* bottom - 1, before the length */
        swap.w  r5,r5
        exts.w  r5,r5
        add     #1,r5           /* top = (top + FRACUNIT-1) >> 16 */
        sts     macl,r0
        add     r0,r6
        swap.w  r6,r6
        exts.w  r6,r6           /* bottom = (bottom - 1) >> 16 */

        /* posts go from top to bottom: once one starts below the
           opening, so do all the ones after it */
        cmp/gt  r14,r5
        bt/s    mnextcol
        cmp/gt  r14,r6

        bf/s    1f
        cmp/gt  r5,r13
        mov     r14,r6          /* clip to bottom */
1:
        bf      2f
        mov     r13,r5          /* clip to top */
2:
        cmp/gt  r6,r5
        bt      mpostend        /* nothing left after clipping */

        /* frac = fracbase + top * iscale - (topdelta << 16), at least 0 */
        mov.l   @(ML_ISCALE,r15),r2
        mul.l   r5,r2
        mov.l   @(ML_FRACBASE,r15),r0
        shll16  r1
        mov.l   r3,@(8,r15)     /* src */
        sub     r1,r0
        mov.l   @(ML_LIGHT,r15),r7
        mov     r9,r4           /* x */
        sts     macl,r1
        add     r1,r0
        mov.l   @(DA_DCOL,r8),r2
        cmp/pz  r0              /* frac < 0 ? 0 : frac, no branch */
        movt    r1
        neg     r1,r1
        and     r1,r0
        jsr     @r2
        mov.l   r0,@r15         /* frac */

mpostend:
        mov.b   @(1,r12),r0
        cmp/pz  r0
        bt/s    mdataofs        /* not the last post */
        add     #4,r12
        bra     mnextcol        /* longword aligned target; the delay */
                                /* slot is the .p2alignw nop or minline's */
                                /* first insn, both harmless here */

        /* an inlined column: one post, pixels right after the header.
           columnofs was sign-extended, so the header is at r12 + 0x8000 */
        .p2alignw 2, 0x0009
minline:
        mov     #-128,r0
        shll8   r0              /* -0x8000 */
        sub     r0,r12
        mov.b   @r12,r0
        cmp/eq  #-1,r0
        bf      3f
        bra     mnextcol        /* empty column (out of bt range); the */
3:                              /* delay slot is the mov below, harmless */
        mov     r12,r3
        bra     mpost
        add     #2,r3

        .align  2
mcenterYFrac:
        .long   _centerYFrac
