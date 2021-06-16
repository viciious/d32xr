/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2021 Victor Luchits, Derek John Evans, id Software and ZeniMax Media

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "marshw.h"

static int mars_activescreen = 0;

volatile unsigned short* mars_gamepadport;
char mars_mouseport;

volatile unsigned mars_vblank_count = 0;
volatile unsigned mars_frt_ovf_count = 0;
unsigned mars_frtc2msec_frac = 0;
const uint8_t* mars_newpalette = NULL;

const int NTSC_CLOCK_SPEED = 23011360; // HZ
const int PAL_CLOCK_SPEED = 22801467; // HZ

void Mars_WaitFrameBuffersFlip(void)
{
	while ((MARS_VDP_FBCTL & MARS_VDP_FS) != mars_activescreen);
}

void Mars_FlipFrameBuffers(char wait)
{
	mars_activescreen = !mars_activescreen;
	MARS_VDP_FBCTL = mars_activescreen;
	if (wait) Mars_WaitFrameBuffersFlip();
}

char Mars_FramebuffersFlipped(void)
{
	return (MARS_VDP_FBCTL & MARS_VDP_FS) == mars_activescreen;
}

void Mars_InitLineTable(volatile unsigned short* lines)
{
	int j;

	// initialize the lines section of the framebuffer
	for (j = 0; j < 224; j++)
		lines[j] = j * 320 / 2 + 0x100;
	// set the rest of the line table to a blank line
	for (j = 224; j < 256; j++)
		lines[j] = 0x100;
	// make sure blank line is clear
	for (j = 256; j < (256 + 160); j++)
		lines[j] = 0;
}

void Mars_UploadPalette(const uint8_t* palette)
{
	int	i;
	volatile unsigned short* cram = &MARS_CRAM;

	while ((MARS_SYS_INTMSK & MARS_SH2_ACCESS_VDP) == 0);

	for (i = 0; i < 256; i++) {
		uint8_t r = *palette++;
		uint8_t g = *palette++;
		uint8_t b = *palette++;
		unsigned short b1 = ((b >> 3) & 0x1f) << 10;
		unsigned short g1 = ((g >> 3) & 0x1f) << 5;
		unsigned short r1 = ((r >> 3) & 0x1f) << 0;
		cram[i] = r1 | g1 | b1;
	}
}

int Mars_PollMouse(int port)
{
	unsigned int mouse1, mouse2;

	while (MARS_SYS_COMM0); // wait until 68000 has responded to any earlier requests
	MARS_SYS_COMM0 = 0x0500 | port; // tells 68000 to read mouse
	while (MARS_SYS_COMM0 == (0x0500 | port)); // wait for mouse value

	mouse1 = MARS_SYS_COMM0;
	mouse2 = MARS_SYS_COMM2;
	MARS_SYS_COMM0 = 0; // tells 68000 we got the mouse value

	return (int)((mouse1 << 16) | mouse2);
}

int Mars_ParseMousePacket(int mouse, int* pmx, int* pmy)
{
	int mx, my;

	// (YO XO YS XS S  M  R  L  X7 X6 X5 X4 X3 X2 X1 X0 Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0)

	mx = ((unsigned)mouse >> 8) & 0xFF;
	// check overflow
	if (mouse & 0x00400000)
		mx = (mouse & 0x00100000) ? -256 : 256;
	else if (mouse & 0x00100000)
		mx |= 0xFFFFFF00;

	my = mouse & 0xFF;
	// check overflow
	if (mouse & 0x00800000)
		my = (mouse & 0x00200000) ? -256 : 256;
	else if (mouse & 0x00200000)
		my |= 0xFFFFFF00;

	*pmx = mx;
	*pmy = my;

	return mouse;
}

int Mars_GetFRTCounter(void)
{
	unsigned cnt = (SH2_FRT_FRCH << 8);
	cnt |= SH2_FRT_FRCL;
	return (int)((mars_frt_ovf_count << 16) | cnt);
}

void Mars_Init(void)
{
	int i;
	char NTSC;

	while ((MARS_SYS_INTMSK & MARS_SH2_ACCESS_VDP) == 0);

	MARS_VDP_DISPMODE = MARS_224_LINES | MARS_VDP_MODE_256;
	NTSC = (MARS_VDP_DISPMODE & MARS_NTSC_FORMAT) != 0;

	/* init hires timer system */
	SH2_FRT_TCR = 2;									/* TCR set to count at SYSCLK/128 */
	SH2_FRT_FRCH = 0;
	SH2_FRT_FRCL = 0;
	SH2_INT_IPRB = (SH2_INT_IPRB & 0xF0FF) | 0x0E00; 	/* set FRT INT to priority 14 */
	SH2_INT_VCRD = 72 << 8; 							/* set exception vector for FRT overflow */
	SH2_FRT_FTCSR = 0;									/* clear any int status */
	SH2_FRT_TIER = 3;									/* enable overflow interrupt */

	MARS_SYS_COMM4 = 0;

	// change 128.0f to something else if SH2_FRT_TCR is changed!
	mars_frtc2msec_frac = 128.0f * 1000.0f / (NTSC ? NTSC_CLOCK_SPEED : PAL_CLOCK_SPEED) * 65536.f;

	mars_activescreen = MARS_VDP_FBCTL;

	Mars_FlipFrameBuffers(1);

	for (i = 0; i < 2; i++)
	{
		volatile int* p, * p_end;

		Mars_InitLineTable(&MARS_FRAMEBUFFER);

		p = (int*)(&MARS_FRAMEBUFFER + 0x100);
		p_end = (int*)p + 320 / 4 * 224;
		do {
			*p = 0;
		} while (++p < p_end);

		Mars_FlipFrameBuffers(1);
	}

	/* detect input devices */
	mars_mouseport = -1;
	mars_gamepadport = &MARS_SYS_COMM8;

	/* values set by the m68k on startup */
	if (MARS_SYS_COMM10 == 0xF001)
	{
		mars_mouseport = 1;
		mars_gamepadport = &MARS_SYS_COMM8;
	}
	else if (MARS_SYS_COMM8 == 0xF001)
	{
		mars_mouseport = 0;
		mars_gamepadport = &MARS_SYS_COMM10;
	}
}

void master_vbi_handler(void)
{
	mars_vblank_count++;
	if (mars_newpalette)
	{
		Mars_UploadPalette(mars_newpalette);
		mars_newpalette = NULL;
	}
}
