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
#include "r_local.h"
#include "wadbase.h"

const int COLOR_WHITE = 0x04;

static int		activescreen = 0;

short		*dc_colormaps;

static volatile pixel_t	*framebuffer = &MARS_FRAMEBUFFER + 0x100;
static volatile pixel_t *framebufferend = &MARS_FRAMEBUFFER + 0x10000;

/*
====================
=
= Mars_ClearFrameBuffer
=
====================
*/
void Mars_ClearFrameBuffer(void)
{
	int *p = (int *)framebuffer;
	int *p_end = (int *)(framebuffer + 320*240/2);
	while (p < p_end)
		*p++ = 0;
}

/*
================
=
= Mars_WaitFrameBuffersFlip
=
================
*/
void Mars_WaitFrameBuffersFlip(void)
{
	while ((MARS_VDP_FBCTL & MARS_VDP_FS) != activescreen);
}

/*
================
=
= Mars_FlipFrameBuffers
=
================
*/
void Mars_FlipFrameBuffers(boolean wait)
{
	activescreen = !activescreen;
	MARS_VDP_FBCTL = activescreen;
	if (wait) Mars_WaitFrameBuffersFlip();
}

/*
================
=
= Mars_Init
=
================
*/
void Mars_Init(void)
{
	int i, j;
	volatile unsigned short *lines = &MARS_FRAMEBUFFER;
	volatile unsigned short *palette;

	while ((MARS_SYS_INTMSK & MARS_SH2_ACCESS_VDP) == 0);

	MARS_VDP_DISPMODE = MARS_224_LINES | MARS_VDP_MODE_256;

	activescreen = MARS_VDP_FBCTL;

	Mars_FlipFrameBuffers(true);

	for (i = 0; i < 2; i++)
	{
		/* initialize the lines section of the framebuffer */
		for (j = 0; j < 256; j++)
			lines[j] = j * 320 / 2 + 0x100;

		Mars_ClearFrameBuffer();

		Mars_FlipFrameBuffers(true);
	}

	ticrate = (MARS_VDP_DISPMODE & MARS_NTSC_FORMAT) ? 4 : 3;

	/* set a two color palette */
	palette = &MARS_CRAM;
	for (i = 0; i < 256; i++)
		palette[i] = 0;
	palette[COLOR_WHITE] = 0x7fff;
}

/*
====================
=
= Mars_ToDoomControls
=
====================
*/
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
	if (ctrl & SEGA_CTRL_Z)
		newc |= BT_9;

	if (ctrl & SEGA_CTRL_MODE)
		newc |= BT_STAR;

	return newc;
}

/*
================
=
= Mars_Slave
=
================
*/

void Mars_Slave(void)
{
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
	int	i, j;
	volatile unsigned short *palette = &MARS_CRAM;
	byte    *doompalette;
	byte 	*doomcolormap;

	doompalette = I_TempBuffer();
	W_ReadLump (W_GetNumForName("PLAYPALS"), doompalette);

	for (i=0 ; i<256 ; i++) {
		byte r = *doompalette++;
		byte g = *doompalette++;
		byte b = *doompalette++;
		unsigned short b1 = ((b >> 3) & 0x1f) << 10;
		unsigned short g1 = ((g >> 3) & 0x1f) << 5;
		unsigned short r1 = ((r >> 3) & 0x1f) << 0;
		palette[i] = r1 | g1 | b1;
	}

	doomcolormap = I_TempBuffer();
	W_ReadLump (W_GetNumForName("COLORMAPS"), doomcolormap);
	Z_Malloc(64*256, PU_STATIC, &dc_colormaps);

	for(j = 0; j < 32; j++) {
		byte *sl1 = &doomcolormap[j*512];
		byte *sl2 = &doomcolormap[j*512+256];
		byte *dl1 = (byte *)&dc_colormaps[j*256];
		byte *dl2 = (byte *)&dc_colormaps[j*256+128];
		D_memcpy(dl1, sl2, 256);
		D_memcpy(dl2, sl1, 256);
	}
}


void I_DrawSbar (void)
{
}

boolean	I_RefreshCompleted (void)
{
	return true;
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
 
static char zone[0x29000];
byte *I_ZoneBase (int *size)
{
	*size = sizeof(zone);
	return (byte *)zone;
}

/*
====================
=
= I_ReadControls
=
====================
*/
int I_ReadControls(void)
{
	int ctrl = consoleplayer == 0 ? MARS_SYS_COMM8 : MARS_SYS_COMM10;
	return Mars_ToDoomControls(ctrl);
}

/*
====================
=
= I_GetTime
=
====================
*/
int	I_GetTime (void)
{
	return MARS_SYS_COMM12;
}

/*
====================
=
= I_TempBuffer
=
= return a pointer to a 52k or so temp work buffer for level setup uses
= (non-displayed frame buffer)
=
====================
*/
byte	*I_TempBuffer (void)
{
	int *p = (int*)I_WorkBuffer();
	int *p_end = (int*)framebufferend;

	// clear the buffer so the fact that 32x ignores 0-byte writes goes unnoticed
	// the buffer cannot be re-used without clearing it again though
	while (p < p_end)
		*p++ = 0;

	return I_WorkBuffer();
}

byte 	*I_WorkBuffer (void)
{
	return (byte *)(I_ViewportBuffer() + 320 * SCREENHEIGHT / 2);
}

/*
====================
=
= I_FrameBuffer
=
====================
*/
pixel_t	*I_FrameBuffer (void)
{
	return (pixel_t *)framebuffer;
}

pixel_t	*I_ViewportBuffer (void)
{
	volatile pixel_t *viewportbuffer = framebuffer;
#if SCREENWIDTH != 160
	viewportbuffer += (224-SCREENHEIGHT)*320/4+(320-SCREENWIDTH*2)/4;
#endif
	return (pixel_t *)viewportbuffer;
}

/*
====================
=
= I_ClearFrameBuffer
=
====================
*/
void 	I_ClearFrameBuffer (void)
{
	Mars_ClearFrameBuffer();
}

void I_ClearWorkBuffer(void)
{
	int *p = (int *)I_WorkBuffer();
	int *p_end = (int *)framebufferend;
	while (p < p_end)
		*p++ = 0;
}

void DoubleBufferSetup (void)
{
}

void EraseBlock (int x, int y, int width, int height)
{
}

void DrawJagobj (jagobj_t *jo, int x, int y)
{
}

void UpdateBuffer (void) {
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

