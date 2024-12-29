
    /* PCM channel increment */
    .equ    PCM_FDL, 0xFF0005
    .equ    PCM_FDH, 0xFF0007

    /* General use timer */
    .equ    TIMER,    0x8030
    .equ    INT_MASK, 0x8032
    .equ    _LEVEL3,  0x5F82        /* TIMER INTERRUPT jump vector */


    .text
    .align  2


| uint8_t pcm_lcf(uint8_t pan);
| Convert pan setting to simple linear crossfade volume settings
    .global pcm_lcf
pcm_lcf:
    move.l  4(sp),d0                /* pan */
    addq.b  #8,d0
    bcc.b   0f
    move.l  4(sp),d0
0:
    andi.w  #0x00F0,d0              /* right pan volume nibble */

    move.l  4(sp),d1
    not.b   d1
    addq.b  #8,d1
    bcc.b   1f
    move.l  4(sp),d1
    not.b   d1
1:
    lsr.b   #4,d1                   /* left pan volume nibble */
    or.b    d1,d0
    rts


| void pcm_delay(void);
| Delay after writing PCM register
    .global pcm_delay
pcm_delay:
    moveq   #20,d1
0:
    dbra    d1,0b
    rts


| void pcm_set_period(uint32_t period);
    .global pcm_set_period
pcm_set_period:
    move.l  4(sp),d1
    cmpi.l  #4,d1
    blo.b   0f                      /* saturate increment to 65535 */
    addq.l  #1,d1
    move.l  #446304,d0
    divu.w  d1,d0
    lsr.w   #1,d0                   /* incr = (446304 / Period + 1) >> 1 */
    bra.b   1f
0:
    move.w  #65535,d0
1:
    move.b  d0,PCM_FDL.l
    bsr.b   pcm_delay
    lsr.w   #8,d0
    move.b  d0,PCM_FDH.l
    bra.b   pcm_delay


| void pcm_set_freq(uint32_t freq);
    .global pcm_set_freq
pcm_set_freq:
    move.l  4(sp),d0
    cmpi.l  #1041648,d0
    bhs.b   0f                      /* saturate increment to 65535 */
    lsl.l   #8,d0
    lsl.l   #3,d0                   /* shift freq for fixed point result */
    move.w  #32552,d1
    divu.w  d1,d0                   /* incr = (freq << 11) / 32552 */
    bra.b   1f
0:
    move.w  #65535,d0
1:
    move.b  d0,PCM_FDL.l
    bsr.b   pcm_delay
    lsr.w   #8,d0
    move.b  d0,PCM_FDH.l
    bra.b   pcm_delay


| void pcm_stop_timer(void);
    .global pcm_stop_timer
pcm_stop_timer:
    move    #0x2700,sr              /* disable interrupts */

    move.w  #0,TIMER.w              /* stop General Timer */
    move.w  INT_MASK.w,d0
    andi.w  #0xFFF7,d0
    move.w  d0,INT_MASK.w           /* disable General Timer interrupt */

    move    #0x2000,sr              /* enable interrupts */
    rts


| void pcm_start_timer(void (*callback)(void *), void *);
    .global pcm_start_timer
pcm_start_timer:
    move    #0x2700,sr              /* disable interrupts */

    move.l  4(sp),int3_callback     /* set callback vector */
    move.l  8(sp),int3_callback_arg /* set callback argument */

    move.w  #0x4EF9,_LEVEL3.w
    move.l  #timer_int,_LEVEL3+2.w  /* set level 3 int vector for timer */

    move.w  #255,TIMER.w            /* 255 clocks at 30.72 microseconds ~ 7.8ms */
    move.w  INT_MASK.w,d0
    ori.w   #0x0008,d0
    move.w  d0,INT_MASK.w           /* enable General Timer interrupt */

    move    #0x2000,sr              /* enable interrupts */
    rts


timer_int:
    movem.l d0-d1/a0-a1,-(sp)
    movea.l int3_callback,a1
    move.l  int3_callback_arg,-(sp)
    jsr     (a1)
    lea     4(sp),sp
    movem.l (sp)+,d0-d1/a0-a1
    rte


    .data


    .align  4

int3_callback:
    .long   0
int3_callback_arg:
    .long   0
