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
| Written by: Victor Luchits, 2023-08
| Based on prior work by Mikael Kalms

| void adpcm_load_bytes_fast_sb4(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t len);
|------------------------------------------------------------------------------------
| This will decode a number of samples from the input stream, to the output stream.
|
| The input should be length/2 bytes large.
|
.global adpcm_load_bytes_fast_sb4
adpcm_load_bytes_fast_sb4:
		movem.l	d2-d6/a2-a4,-(sp)

		move.l  sp@(36),a0  | adpcm state pointer
		move.l	sp@(40),a1  | write pointer
		move.l  sp@(44),d0  | length

		move.l	d0,d6
		lsr.l	#1,d6
		subq.l	#1,d6

		move.l	0(a0),a3  | read pointer
		moveq	#0,d2
		move.w	4(a0),d2  | index
		moveq	#0,d5
		move.w	6(a0),d5  | value

		lea	adpcm_sb4_steps_indices,a2
		lea pcm_u8_to_sm_lut,a4

		moveq   #0,d1
		move.l  #255,d4
sampleSB4Pair:

		move.b	(a3)+,d1		| 8
		move.w	d1,d0			| 4

		and.b	#0x0F,d0		| 8
		andi.b  #0xF0,d1		| 8

		lsl.b   #4,d0			| 14

								| = 42

firstSB4Sample:
		add.b	d0,d2			| 4

		move.l  (a2,d2.w),d2    | 18	| load delta+index
		move.l  d2,d3           | 4
		swap    d3              | 4		| update delta

		add.w	d3,d5			| 4
		spl		d3				| 4
		ext.w	d3				| 4
		and.w	d3,d5			| 4		| d5 == 0 if d5 < 0
		cmp.w	d4,d5			| 4		| d4 == 255
		bls.s	clampSB40Done	| 8 or 12
		move.w	d4,d5
clampSB40Done:
		move.b  (a4,d5.w),(a1)	| 22

secondSB4Sample:
		add.b	d1,d2			| 4

		move.l  (a2,d2.w),d2    | 18	| load delta+index
		move.l  d2,d3           | 4
		swap    d3              | 4		| update delta

		add.w   d3,d5			| 4
		spl	    d3				| 4
		ext.w	d3				| 4
		and.w	d3,d5			| 4		| d5 == 0 if d5 < 0
		cmp.w	d4,d5			| 4		| d4 == 255
		bls.s	clampSB41Done	| 8 or 12
		move.w	d4,d5
clampSB41Done:
		move.b  (a4,d5.w),2(a1) | 22

		addq	#4,a1			| 8

								| = 164 approx

		dbf	d6, sampleSB4Pair	| 12

		move.l	a3,0(a0)
		move.w	d2,4(a0)
		move.w	d5,6(a0)

		movem.l	(sp)+,d2-d6/a2-a4
		
		rts
