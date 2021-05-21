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


#include "32x.h"
#include "doomdef.h"
#include "mars.h"
#include "mars_ringbuf.h"
#include "r_local.h"
#include "wadbase.h"

const int COLOR_WHITE = 0x04;

int		activescreen = 0;
short	*dc_colormaps;

unsigned vblank_count = 0;
unsigned frt_ovf_count = 0;
unsigned frtc2msec_frac = 0;

const int NTSC_CLOCK_SPEED = 23011360; // HZ
const int PAL_CLOCK_SPEED  = 22801467; // HZ

extern int 	debugmode;

// framebuffer start is after line table AND a single blank line
static volatile pixel_t* framebuffer = &MARS_FRAMEBUFFER + 0x100 + 160;
static volatile pixel_t *framebufferend = &MARS_FRAMEBUFFER + 0x10000;

void Mars_ClearFrameBuffer(void)
{
	int *p = (int *)framebuffer;
	int *p_end = (int *)(framebuffer + 320*224 / 2);
	while (p < p_end)
		*p++ = 0;
}

inline void Mars_WaitFrameBuffersFlip(void)
{
	while ((MARS_VDP_FBCTL & MARS_VDP_FS) != activescreen);
}

inline void Mars_FlipFrameBuffers(boolean wait)
{
	activescreen = !activescreen;
	MARS_VDP_FBCTL = activescreen;
	if (wait) Mars_WaitFrameBuffersFlip();
}

void Mars_Init(void)
{
	int i, j;
	volatile unsigned short *lines = &MARS_FRAMEBUFFER;
	volatile unsigned short *palette;
	boolean NTSC;

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
	frtc2msec_frac = 128.0f * 1000.0f / (NTSC ? NTSC_CLOCK_SPEED : PAL_CLOCK_SPEED) * 65536.f;

	activescreen = MARS_VDP_FBCTL;

	Mars_FlipFrameBuffers(true);

	for (i = 0; i < 2; i++)
	{
		// initialize the lines section of the framebuffer
		for (j = 0; j < 224; j++)
			lines[j] = j * 320 / 2 + 0x100;
		// set the rest of the line table to a blank line
		for (j = 224; j < 256; j++)
			lines[j] = 0x100;
		// make sure blank line is clear
		for (j = 256; j < (256 + 160); j++)
			lines[j] = 0;

		Mars_ClearFrameBuffer();

		Mars_FlipFrameBuffers(true);
	}

	ticrate = NTSC ? 4 : 3;

	/* set a two color palette */
	palette = &MARS_CRAM;
	for (i = 0; i < 256; i++)
		palette[i] = 0;
	palette[COLOR_WHITE] = 0x7fff;
}

int Mars_ToDoomControls(int ctrl)
{
	int newc = 0;

	if (ctrl & SEGA_CTRL_LEFT)
		newc |= BT_LEFT;
	if (ctrl & SEGA_CTRL_RIGHT)
		newc |= BT_RIGHT;
	if (ctrl & SEGA_CTRL_UP)
		newc |= BT_UP;
	if (ctrl & SEGA_CTRL_DOWN)
		newc |= BT_DOWN;

	switch (ctrl & (SEGA_CTRL_START | SEGA_CTRL_A)) {
	case SEGA_CTRL_START | SEGA_CTRL_A:
		newc |= BT_9;
		break;
	case SEGA_CTRL_START:
		newc |= BT_OPTION;
		break;
	case SEGA_CTRL_A:
		newc |= BT_A;
		break;
	}

	if (ctrl & SEGA_CTRL_B)
		newc |= BT_B;
	if (ctrl & SEGA_CTRL_C)
		newc |= BT_C;
	if (ctrl & SEGA_CTRL_X)
		newc |= BT_PWEAPN;
	if (ctrl & SEGA_CTRL_Y)
		newc |= BT_NWEAPN;
	if (ctrl & SEGA_CTRL_Z)
		newc |= BT_9;

	if (ctrl & SEGA_CTRL_MODE)
		newc |= BT_STAR;

	return newc;
}

void Mars_Slave(void)
{
	while (1)
	{
		int cmd;
		while ((cmd = MARS_SYS_COMM4) == 0);

		switch (cmd) {
		case 1:
			Mars_Slave_R_SegCommands();
			break;
		case 2:
			break;
		case 3:
			Mars_ClearCache();
			break;
		case 4:
			Mars_Slave_R_DrawPlanes();
			break;
		case 5:
			Mars_Slave_R_DrawSprites();
			break;
		case 6:
			Mars_Slave_R_OpenPlanes();
			break;
		case 7:
			break;
		case 8:
			Mars_Slave_M_AnimateFire();
			break;
		case 9:
			break;
		case 10:
			Mars_Slave_InitSoundDMA();
			break;
		default:
			break;
		}

		MARS_SYS_COMM4 = 0;
	}
}

void Mars_UploadPalette(const byte *palette)
{
	int	i;
	volatile unsigned short *cram = &MARS_CRAM;

	while ((MARS_SYS_INTMSK & MARS_SH2_ACCESS_VDP) == 0);

	for (i=0 ; i<256 ; i++) {
		byte r = *palette++;
		byte g = *palette++;
		byte b = *palette++;
		unsigned short b1 = ((b >> 3) & 0x1f) << 10;
		unsigned short g1 = ((g >> 3) & 0x1f) << 5;
		unsigned short r1 = ((r >> 3) & 0x1f) << 0;
		cram[i] = r1 | g1 | b1;
	}
}

void I_SetPalette(const byte *palette)
{
	Mars_UploadPalette(palette);
}

/* 
================ 
= 
= I_Init  
=
= Called after all other subsystems have been started
================ 
*/ 
 
void I_Init (void) 
{	
	int	i;
	const byte	*doompalette;
	const byte 	*doomcolormap;

	doompalette = W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
	I_SetPalette(doompalette);

	doomcolormap = W_POINTLUMPNUM(W_GetNumForName("COLORMAPS"));
	dc_colormaps = Z_Malloc(64*256, PU_STATIC, 0);

	for(i = 0; i < 32; i++) {
		const byte *sl1 = &doomcolormap[i*512];
		const byte *sl2 = &doomcolormap[i*512+256];
		byte *dl1 = (byte *)&dc_colormaps[i*256];
		byte *dl2 = (byte *)&dc_colormaps[i*256+128];
		D_memcpy(dl1, sl2, 256);
		D_memcpy(dl2, sl1, 256);
	}
}

void I_DrawSbar (void)
{
}

boolean	I_RefreshCompleted (void)
{
	return (MARS_VDP_FBCTL & MARS_VDP_FS) == activescreen;
}

boolean	I_RefreshLatched (void)
{
	return true;
}


/* 
==================== 
= 
= I_WadBase  
=
= Return a pointer to the wadfile.  In a cart environment this will
= just be a pointer to rom.  In a simulator, load the file from disk.
==================== 
*/ 
 
byte *I_WadBase (void)
{
	return wadBase; 
}


/* 
==================== 
= 
= I_ZoneBase  
=
= Return the location and size of the heap for dynamic memory allocation
==================== 
*/ 
 
static char zone[0x30000] __attribute__((aligned(16)));
byte *I_ZoneBase (int *size)
{
	*size = sizeof(zone);
	return (byte *)zone;
}

int I_ReadControls(void)
{
	int ctrl = consoleplayer == 0 ? MARS_SYS_COMM8 : MARS_SYS_COMM10;
	return Mars_ToDoomControls(ctrl);
}

int	I_GetTime (void)
{
	return *(int *)((intptr_t)&vblank_count | 0x20000000);
}

int I_GetFRTCounter(void)
{
	unsigned cnt = (SH2_FRT_FRCH << 8) | SH2_FRT_FRCL;
	return (int)((frt_ovf_count << 16) | cnt);
}

int I_FRTCounter2Msec(int c)
{
	return FixedMul((unsigned)c << FRACBITS, frtc2msec_frac)>>FRACBITS;
}

/*
====================
=
= I_TempBuffer
=
= return a pointer to a 64k or so temp work buffer for level setup uses
= (non-displayed frame buffer)
=
====================
*/
byte	*I_TempBuffer (void)
{
	byte *w = I_WorkBuffer();
	int *p, *p_end = (int*)framebufferend;

	// clear the buffer so the fact that 32x ignores 0-byte writes goes unnoticed
	// the buffer cannot be re-used without clearing it again though
	for (p = (int*)w; p < p_end; p++)
		*p = 0;

	return w;
}

byte 	*I_WorkBuffer (void)
{
	while (!I_RefreshCompleted());
	return (byte *)(framebuffer + 320 / 2 * 224);
}

pixel_t	*I_FrameBuffer (void)
{
	return (pixel_t *)framebuffer;
}

pixel_t	*I_ViewportBuffer (void)
{
	volatile pixel_t *viewportbuffer = framebuffer;
	if (screenWidth < 160)
		viewportbuffer += (224-screenHeight)*320/4+(320-screenWidth*2)/4;
	return (pixel_t *)viewportbuffer;
}

void I_ClearFrameBuffer (void)
{
	Mars_ClearFrameBuffer();
}

void I_DebugScreen(void)
{
	if (debugmode == 3)
		Mars_ClearFrameBuffer();
}

void I_ClearWorkBuffer(void)
{
	int *p = (int *)I_WorkBuffer();
	int *p_end = (int *)framebufferend;
	while (p < p_end)
		*p++ = 0;
}

/*=========================================================================== */

void DoubleBufferSetup (void)
{
	int i;

	for (i = 0; i < 2; i++) {
		while (!I_RefreshCompleted())
			;

		I_ClearFrameBuffer();

		UpdateBuffer();
	}

	while (!I_RefreshCompleted())
		;
}

void UpdateBuffer (void) {
	I_Update();
	while (!I_RefreshCompleted())
		;
}

void ReadEEProm (void)
{
	maxlevel = 24;
}

void WriteEEProm (void)
{
	maxlevel = 24;
}

unsigned I_NetTransfer (unsigned ctrl)
{
	return 0;
}

void I_NetSetup (void)
{
}
