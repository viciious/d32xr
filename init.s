.include "/home/scott/Jaguar/include/jaguar.inc"

	.extern	_Jag68k_main
	.extern	_readylist_p
	.extern _displaylist_p
	.extern	_isrvmode
	.extern	_joystick1
	.extern	_ticcount
	.extern	_joypad

	.text
reset:
;/* Run the GPU/BLIT interface in CORRECT mode ALWAYS */
	move.l   #$00070007,G_END
	move.l   #$00050005,D_END

;/* At this point we dont know what state the video is in */
;/* It may be active or not and may be using an interrupt or not */
;/* Since we may not turn video off we use the following procedure */
;/* Disable VI by setting to a VERY large number. */
;/*      The existing screen will fail to be refreshed so all bit maps vanish */
;/* Set up an object list consisting of a stop object. */
;/* Clear the spot the screen will be */
;/* Set up the desired object list */
;/* Set up an interrupt and start */
;/* Set up the size of borders */
;/* Set VMODE to the desired resolution and color model */

	move.l   ENDRAM,a7            ;/* Put the stack at the top of DRAM */
	move.w   #$FFFF,VI              ;/* Guarantee NO VI interrupts */

;/* point the object processor at a stop object until told otherwise */
	move.l   #_stopobj,d5
	swap    d5
	move.l   d5,OLP

	jsr     IntInit
	jsr     VideoIni

;/*	move.w   #0x8C1,VMODE            // Set 16 bit CRY;/* 256 overscanned */
;move.w   #0xC1+(7<<9),VMODE     ;/* Set 16 bit CRY;/* 160 overscanned */
	move.w   #$EC1,VMODE     ;/* Set 16 bit CRY;/* 160 overscanned */

	jmp     _Jag68k_main

	;illegal


.bss
.dphrase
_listbuffer:	ds.l  40
_listbuffer1:	ds.l  40
_listbuffer2:	ds.l  40
_stopobj:	ds.l  2

.globl _listbuffer,_listbuffer1,_listbuffer2,_stopobj


.text
;/*========================================================================= */
;/* vdinit.s */
;/*========================================================================= */
;/* The size of the horizontal and vertical active areas */
;/* are based on a given center position and then built */
;/* as offsets of half the size off of these. */

;/* In the horizontal direction this needs to take into */
;/* account the variable resolution that is possible. */

;/* THESE ARE THE NTSC DEFINITIONS */
ntsc_width      =       1409
ntsc_hmid       =       823

ntsc_height     =       241
ntsc_vmid       =       266

;/* THESE ARE THE PAL DEFINITIONS */
pal_width       =       1381
pal_hmid        =       843

pal_height      =       287
pal_vmid        =       322


VideoIni:
;/* Check if NTSC or PAL */
;/* For now assume NTSC */

	movem.l  d0-d6,-(sp)

	move.w	CONFIG,d0
	and.w	#$10,d0
	beq	ispal

	move.w	#ntsc_hmid,d2
	move.w	#ntsc_width,d0

	move.w	#ntsc_vmid,d6
	move.w	#ntsc_height,d4

	bra	doit

ispal:
	move.w	#pal_hmid,d2
	move.w	#pal_width,d0

	move.w	#pal_vmid,d6
	move.w	#pal_height,d4

doit:
	move.w   d0,_video_width
	move.w   d4,_video_height

	move.w   d0,d1
	asr.l    #1,d1                   ;/* Max width/2 */

	sub.w    d1,d2                   ;/* middle-width/2 */
	add.w    #4,d2                   ;/* (middle-width/2)+4 */
	
	sub.w    #1,d1                   ;/* Width/2-1 */
	or.w     #$400,d1               ;/* (Width/2-1)+0x400 */

	move.w   d1,_a_hde
	move.w   d1,HDE

	move.w   d2,_a_hdb
	move.w   d2,HDB1
	move.w   d2,HDB2

	move.w   d6,d5
	sub.w    d4,d5                   ;/* already in half lines */
	move.w   d5,_a_vdb

	add.w    d4,d6
	move.w   d6,_a_vde

	move.w   _a_vdb,VDB
	move.w   #$FFFF,VDE

;/* Also lets set up some default colors */

	move.w   #0,BG
	move.l   #0,BORD1
;/*      move.w   #$f0ff,BG */
;/*      move.l   #$ffffffff,BORD1 */

	movem.l  (sp)+,d0-d6
	rts

.bss
.phrase

_video_height:	.ds.w	1
_a_vdb:		.ds.w	1
_a_vde:		.ds.w	1
_video_width:	.ds.w	1
_a_hdb:		.ds.w	1
_a_hde:		.ds.w	1

.globl _video_height, _video_width, _a_vdb, _a_vde, _a_hdb, _a_hde

.text


;/*========================================================================== */
;/*
;====================
;=
;= CopyObjList
;=
;====================
;*/

_CopyObjList:	
	move.w   #0,$f00026     ;/* restart object processor, just in case */
	
;/* point the object processor at a stop object until copy is done */
	move.l   #_stopobj,d5
	swap    d5
	move.l   d5,OLP

reread:
	move.l   _readylist_p,a0
	cmp.l	_readylist_p,a0		;/* make sure its the entire word */
	bne	reread
	
	move.l   a0,_displaylist_p       ;/* let the program know this is being used */
	move.l   #_listbuffer,a1         ;/* copy to here */
	
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+
	move.l   (a0)+,(a1)+

;/* start the object processor at the newly copied list */
	move.l   #_listbuffer,d5
	swap    d5
	move.l   d5,OLP
	
	rts
		


;/*========================================================================= */
;/* intserv.s */
;/*========================================================================= */
.data

enabled_ints:   .long   0       ;/* The IRQ-enabled bits in INT1 */

.text

;USER0   =       4*0x40

;/* This will set up the VI (Vertical line counter Interrupt) */
;/* and the PIT interrupt handler */

IntInit:

	move.l   #Frame,USER0            ;/* Set up the vector */

	move.w   _a_vde,d0
	or.w     #1,d0
	move.w   d0,VI                   ;/* Get the maximum VBLANK time */

	move.w   #1, enabled_ints
	move.w   #1, INT1

	move.w   sr,d0
	and.w    #$f8ff,d0              ;/* Lower the 68K IPL */
	move.w   d0,sr

	rts

;/*======================================================================== */
;/* */
;/* FRAME INTERRUPT */
;/* */
;/*======================================================================== */

_bogusvmode:	.long	0

Frame:
	link    a6, #0
	movem.l  d0-d5/a0-a5,-(sp)

;/*move.w   #0xC1+(7<<9),VMODE           ;/* Set 16 bit CRY/* 160 overscanned */

	jsr		_CopyObjList

	move.l	_isrvmode,d0	;/* set VMODE for top part of screen */
	move.w   d0,VMODE	;/* can be reset by GPU interrupt */

;/* */
;/*  This monsterous ugly code lifted almost directly from demo.s which */
;/*  came with the jaguar development code examples. */
;/* */

;/* read player 1 joypad */

	move.l	#$f0fffffc,d1	  ;/* d1 = Joypad data mask */
	moveq.l	#-1,d2		;/* d2 = Cumulative joypad reading */

	move.w	#$81fe,JOYSTICK
	move.l	JOYSTICK,d0	;/* Read joypad, pause button, A button */
	or.l	d1,d0		  	;/* Mask off unused bits */
	ror.l	#4,d0
	and.l	d0,d2		;/* d2 = xxAPxxxx RLDUxxxx xxxxxxxx xxxxxxxx */
	move.w	#$81fd,JOYSTICK
	move.l	JOYSTICK,d0	;/* Read *741 keys, B button */
	or.l	d1,d0		  	;/* Mask off unused bits */
	ror.l	#8,d0
	and.l	d0,d2		;/* d2 = xxAPxxBx RLDU741* xxxxxxxx xxxxxxxx */
	move.w	#$81fb,JOYSTICK
	move.l	JOYSTICK,d0	;/* Read 2580 keys, C button */
	or.l	d1,d0		  	;/* Mask off unused bits */
	rol.l	#6,d0
	rol.l	#6,d0
	and.l	d0,d2		;/* d2 = xxAPxxBx RLDU741* xxCxxxxx 2580xxxx */
	move.w	#$81f7,JOYSTICK
	move.l	JOYSTICK,d0	;/* Read 369# keys, Option button */
	or.l	d1,d0		 ;/* Mask off unused bits */
	rol.l	#8,d0
	and.l	d0,d2		;/* d2 = xxAPxxBx RLDU741* xxCxxxOx 2580369# */
				;/*              (inputs active low) */

	moveq.l	#-1,d1
	eor.l	d2,d1		;/* d1 = xxAPxxBx RLDU741* xxCxxxOx 2580369# */
				;/*           (now inputs active high) */
;/*===================== */
	cmp.l	#$10001,d1	;/* # + * reset */
	bne	notreset
;/* throw a reset returnpoint over the old returnpoint */
	move.l	#reset,6(a6)
	
notreset:

	move.l	d1,_joystick1		;/* joystick1 = joystickbuts */
	
	move.l	_ticcount,d0
	and.l	#31,d0
	lsl.l	#2,d0
	move.l	d0,a0
	add.l	#_joypad,a0
	move.l	d1,(a0)		;/* joypad[ticcount&31] = joystickbits */


;/* */
;/* other VI stuff */
;/* */
	addq.l	#1,_ticcount
	move.w	#$100, d0

;/*========================================================================== */
;/* */
;/* done with interrupts */
;/* */
;/*========================================================================== */

;0:

	or.w     enabled_ints, d0
	move.w   d0, INT1
	move.w   #0, INT2
	movem.l  (sp)+,d0-d5/a0-a5
	unlk    a6
	rte

	.dphrase
	.data
	.dphrase
