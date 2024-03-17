    .text

| MIT License
|
| Copyright (c) 2023 Mikael Kalms
| Copyright (c) 2023 Victor Luchits

| Permission is hereby granted, free of charge, to any person obtaining a copy
| of this software and associated documentation files (the "Software"), to deal
| in the Software without restriction, including without limitation the rights
| to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
| copies of the Software, and to permit persons to whom the Software is
| furnished to do so, subject to the following conditions:

| The above copyright notice and this permission notice shall be included in all
| copies or substantial portions of the Software.

| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
| IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
| FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
| AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
| LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
| OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
| SOFTWARE.

| IMA ADPCM decode routine optimized for 68000
| It takes in a mono datastream encoded to 4-bit entries, and outputs a stream of signed 8-bit or 16-bit samples.
|
| Written by: Mikael Kalms, 2009-09-26 / 2017-10-24
| Modified by: Victor Luchits, 2023-08

| void adocm_load_bytes_fast_ima(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t len);
|------------------------------------------------------------------------------------
| This will decode a number of samples from the input stream, to the output stream.
|
| The input should be length/2 bytes large.
|
.global adpcm_load_bytes_fast_ima
adpcm_load_bytes_fast_ima:
		movem.l	d2-d7/a2-a5,-(sp)

		move.l  sp@(44),a0  | adpcm state pointer
		move.l	sp@(48),a1  | write pointer
		move.l  sp@(52),d0  | length

		move.l	d0,d7
		lsr.l	#1,d7
		subq.l	#1,d7

		move.l	0(a0),a5  | read pointer
		moveq	#0,d2
		move.w	4(a0),d2  | index
		moveq   #0,d6
		move.w	6(a0),d6  | value

		lea	adpcm_ima_indices,a3
		lea	adpcm_ima_deltas,a2
		lea pcm_u8_to_sm_lut,a4

		moveq	#0,d0
		moveq   #0,d1
		moveq   #0,d3

		move    #0x0F,d4
		move    #0xF0,d5
 
samplePair:

		move.b	(a5)+,d1		| 8
		move.b	d1,d0			| 4

		and		d4,d0		    | 4
		and 	d5,d1		    | 4

		lsr.b	#3,d1			| 12
		add.b	d0,d0			| 4

								| = 36

firstSample:
		move.w  d2,d3			| 4
		add.w   d0,d3			| 4
		move.w	(a3,d3.w),d2	| 14	| update index

		add.w   d3,d3			| 4
		add.l	(a2,d3.w),d6	| 18
		| The minimum negative step is +-61436:
		| +-(32767/8 + 32767 + 32767/2 + 32767/4). Values in ranges 
		| [-61436,-1] and [0x10000,0x1ffff] are all going to have
		| bit 16 set to 1
		btst.l  #16,d6			| 6
		beq.s	clamp0Done		| 10 or 16
		spl		d3				| 4
		ext.w	d3				| 4     | d3 is now either 0 or 0xffff
		move.l  d3,d6			| 4
clamp0Done:
		move.w	d6,-(sp)		| 8
		move.b	(sp)+,d0		| 8
		move.b  (a4,d0.w),(a1)  | 18

secondSample:
		move.w  d2,d3			| 4
		add.w   d1,d3			| 4
		move.w	(a3,d3.w),d2	| 14	| update index

		add.w   d3,d3			| 4
		add.l	(a2,d3.w),d6	| 16
		btst.l  #16,d6			| 6
		beq.s	clamp1Done		| 10 or 16
		spl		d3				| 4
		ext.w	d3				| 4     | d3 is now either 0 or 0xffff
		move.l  d3,d6			| 4
clamp1Done:
		move.w	d6,-(sp)		| 8
		move.b	(sp)+,d0		| 8
		move.b  (a4,d0.w),2(a1) | 22

		addq    #4,a1			| 8

								| = 208-228

		dbf	d7, samplePair		| 12

		move.l	a5,0(a0)
		move.w	d2,4(a0)
		move.w	d6,6(a0)

		movem.l	(sp)+,d2-d7/a2-a5
		
		rts
