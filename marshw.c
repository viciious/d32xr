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

volatile unsigned short* mars_gamepadport, * mars_gamepadport2;
char mars_mouseport;

volatile unsigned mars_controls, mars_controls2;

volatile unsigned mars_vblank_count = 0;
volatile unsigned mars_pwdt_ovf_count = 0;
volatile unsigned mars_swdt_ovf_count = 0;
unsigned mars_frtc2msec_frac = 0;
const uint8_t* mars_newpalette = NULL;

int16_t mars_requested_lines = 224;
uint16_t mars_framebuffer_height = 224;

uint16_t mars_cd_ok = 0;
uint16_t mars_num_cd_tracks = 0;

uint16_t mars_refresh_hz = 0;

const int NTSC_CLOCK_SPEED = 23011360; // HZ
const int PAL_CLOCK_SPEED = 22801467; // HZ

void pri_vbi_handler(void) __attribute__((section(".data"), aligned(16)));

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

void Mars_InitLineTable(void)
{
	int j;
	int blank;
	int offset = 0; // 224p or 240p
	volatile unsigned short* lines = &MARS_FRAMEBUFFER;

	// initialize the lines section of the framebuffer

	if (mars_requested_lines == -240)
	{
		// letterboxed 240p
		offset = (240 - 224) / 2;
	}

	for (j = 0; j < mars_framebuffer_height; j++)
		lines[offset+j] = j * 320 / 2 + 0x100;

	blank = j * 320 / 2;

	// set the rest of the line table to a blank line
	for (; j < 256; j++)
		lines[offset+j] = blank + 0x100;

	for (j = 0; j < offset; j++)
		lines[j] = blank + 0x100;

	// make sure blank line is clear
	for (j = blank; j < (blank + 160); j++)
		lines[j] = 0;
}

char Mars_UploadPalette(const uint8_t* palette)
{
	int	i;
	volatile unsigned short* cram = &MARS_CRAM;

	if ((MARS_SYS_INTMSK & MARS_SH2_ACCESS_VDP) == 0)
		return 0;

	for (i = 0; i < 256; i++) {
		uint8_t r = *palette++;
		uint8_t g = *palette++;
		uint8_t b = *palette++;
		unsigned short b1 = ((b >> 3) & 0x1f) << 10;
		unsigned short g1 = ((g >> 3) & 0x1f) << 5;
		unsigned short r1 = ((r >> 3) & 0x1f) << 0;
		cram[i] = r1 | g1 | b1;
	}

	return 1;
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
	unsigned int cnt = SH2_WDT_RTCNT;
	return (int)((mars_pwdt_ovf_count << 8) | cnt);
}

void Mars_InitVideo(int lines)
{
	int i;
	char NTSC;
	int mars_lines = lines == 240 || lines == -240 ? MARS_240_LINES : MARS_224_LINES;

	while ((MARS_SYS_INTMSK & MARS_SH2_ACCESS_VDP) == 0);

	MARS_VDP_DISPMODE = mars_lines | MARS_VDP_MODE_256;
	NTSC = (MARS_VDP_DISPMODE & MARS_NTSC_FORMAT) != 0;

	// change 4096.0f to something else if WDT TCSR is changed!
	mars_frtc2msec_frac = 4096.0f * 1000.0f / (NTSC ? NTSC_CLOCK_SPEED : PAL_CLOCK_SPEED) * 65536.0f;

	mars_refresh_hz = NTSC ? 60 : 50;
	mars_requested_lines = lines;
	mars_framebuffer_height = lines == 240 ? 240 : 224;
	mars_activescreen = MARS_VDP_FBCTL;

	Mars_FlipFrameBuffers(1);

	for (i = 0; i < 2; i++)
	{
		volatile int* p, * p_end;

		Mars_InitLineTable();

		p = (int*)(&MARS_FRAMEBUFFER + 0x100);
		p_end = (int*)p + 320 / 4 * mars_framebuffer_height;
		do {
			*p = 0;
		} while (++p < p_end);

		Mars_FlipFrameBuffers(1);
	}
}

void Mars_Init(void)
{
	Mars_InitVideo(224);

	SH2_WDT_WTCSR_TCNT = 0xA518; /* WDT TCSR = clr OVF, IT mode, timer off, clksel = Fs/2 */

	/* init hires timer system */
	SH2_WDT_VCR = (65<<8) | (SH2_WDT_VCR & 0x00FF); // set exception vector for WDT
	SH2_INT_IPRA = (SH2_INT_IPRA & 0xFF0F) | 0x0020; // set WDT INT to priority 2

	MARS_SYS_COMM4 = 0;

	/* detect input devices */
	mars_mouseport = -1;
	mars_gamepadport = &MARS_SYS_COMM8;
	mars_gamepadport2 = &MARS_SYS_COMM10;

	/* values set by the m68k on startup */
	if (MARS_SYS_COMM10 == 0xF001)
	{
		mars_mouseport = 1;
		mars_gamepadport = &MARS_SYS_COMM8;
		mars_gamepadport2 = NULL;
	}
	else if (MARS_SYS_COMM8 == 0xF001)
	{
		mars_mouseport = 0;
		mars_gamepadport = &MARS_SYS_COMM10;
		mars_gamepadport2 = NULL;
	}

	mars_controls = 0;
	mars_controls2 = 0;

	Mars_UpdateCD();

	if (mars_cd_ok)
	{
		/* give the CD three seconds to init */
		unsigned wait = mars_vblank_count + 180;
		while (mars_vblank_count <= wait) ;
	}
}

short* Mars_FrameBufferLines(void)
{
	short* lines = (short*)&MARS_FRAMEBUFFER;
	if (mars_requested_lines == -240)
		lines += (240 - 224) / 2;
	return lines;
}

void pri_vbi_handler(void)
{
	mars_vblank_count++;
	if (mars_newpalette)
	{
		if (Mars_UploadPalette(mars_newpalette))
			mars_newpalette = NULL;
	}

	mars_controls |= *mars_gamepadport;
	if (mars_gamepadport2)
		mars_controls2 |= *mars_gamepadport2;
}

void Mars_ReadSRAM(uint8_t * buffer, int offset, int len)
{
	uint8_t *ptr = buffer;

	while (MARS_SYS_COMM0);
	while (len-- > 0) {
		MARS_SYS_COMM2 = offset++;
		MARS_SYS_COMM0 = 0x0100;    /* Read SRAM */
		while (MARS_SYS_COMM0);
		*ptr++ = MARS_SYS_COMM2 & 0x00FF;
	}
}

void Mars_WriteSRAM(const uint8_t* buffer, int offset, int len)
{
	const uint8_t *ptr = buffer;

	while (MARS_SYS_COMM0);
	while (len-- > 0) {
		MARS_SYS_COMM2 = offset++;
		MARS_SYS_COMM0 = 0x0200 | *ptr++;    /* Write SRAM */
		while (MARS_SYS_COMM0);
	}
}

void Mars_UpdateCD(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0600;
	while (MARS_SYS_COMM0);
	mars_cd_ok = MARS_SYS_COMM2;
	mars_num_cd_tracks = MARS_SYS_COMM12;
}

void Mars_UseCD(int usecd)
{
	if (!mars_cd_ok)
		return;

	while (MARS_SYS_COMM0);
	MARS_SYS_COMM2 = usecd & 1;
	MARS_SYS_COMM0 = 0x0700;
	while (MARS_SYS_COMM0);
}

void Mars_PlayTrack(char usecd, int playtrack, void *vgmptr, char looping)
{
	Mars_UseCD(usecd);

	while (MARS_SYS_COMM0);

	if (usecd)
	{
		MARS_SYS_COMM2 = looping;
		MARS_SYS_COMM12 = playtrack;
		MARS_SYS_COMM0 = 0x0300; /* start music */
	}
	else
	{
		MARS_SYS_COMM2 = playtrack | (looping ? 0x8000 : 0x0000);
		*(volatile intptr_t*)&MARS_SYS_COMM12 = (intptr_t)vgmptr;
		MARS_SYS_COMM0 = 0x0300; /* start music */
	}
}

void Mars_StopTrack(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0400; /* stop music */
}


// MD video debug functions

void Mars_SetMDCrsr(int x, int y)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM12 = x;
	MARS_SYS_COMM14 = y;
	MARS_SYS_COMM0 = 0x0800;			/* set current md cursor */
}

void Mars_GetMDCrsr(int *x, int *y)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0900;			/* get current md cursor */
	while (MARS_SYS_COMM0);
	*x = (int)MARS_SYS_COMM12;
	*y = (int)MARS_SYS_COMM14;
}

void Mars_SetMDColor(int fc, int bc)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM12 = fc | (fc << 4) | (fc << 8) | (fc << 12);
	MARS_SYS_COMM14 = bc | (bc << 4) | (bc << 8) | (bc << 12);
	MARS_SYS_COMM0 = 0x0A00;			/* set font fg and bg colors */
}

void Mars_GetMDColor(int *fc, int *bc)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0B00;			/* get font fg and bg colors */
	while (MARS_SYS_COMM0);
	*fc = (int)MARS_SYS_COMM12;
	*bc = (int)MARS_SYS_COMM14;
}

void Mars_SetMDPal(int cpsel)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0C00 | cpsel;	/* set palette select */
}

void Mars_MDPutChar(char chr)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0D00 | chr;		/* put char at current cursor pos */
}

void Mars_ClearNTA(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0E00;			/* clear name table a */
}

void Mars_MDPutString(char *str)
{
	while (*str)
		Mars_MDPutChar(*str++);
}


void Mars_DebugStart(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0F00;			/* start debug queue */
}

void Mars_DebugQueue(int id, int val)
{
	while (MARS_SYS_COMM0);
	*(volatile intptr_t *)&MARS_SYS_COMM12 = val;
	MARS_SYS_COMM0 = 0x1000 | id;		/* queue debug entry */
}

void Mars_DebugEnd(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x1100;			/* end debug queue and display */
}

