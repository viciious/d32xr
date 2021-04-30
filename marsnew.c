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

//#define USE_C_DRAW

const int COLOR_WHITE = 0x04;

int		activescreen = 0;
short	*dc_colormaps;

unsigned	vblank_count = 0;
unsigned	frt_ovf_count = 0;
static float frt_counter2msec = 0;

const int NTSC_CLOCK_SPEED = 23011360; // HZ
const int PAL_CLOCK_SPEED  = 22801467; // HZ

extern int 	debugmode;

static volatile pixel_t	*framebuffer = &MARS_FRAMEBUFFER + 0x100;
static volatile pixel_t *framebufferend = &MARS_FRAMEBUFFER + 0x10000;

#ifdef USE_C_DRAW
#define I_DrawColumnPO2A I_DrawColumnPO2C
#define I_DrawColumnNPO2A I_DrawColumnNPO2C
#define I_DrawSpanA I_DrawSpanC
#else
void I_DrawColumnPO2A(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_, fixed_t fracstep, inpixel_t* dc_source, int dc_texheight);
void I_DrawColumnNPO2A(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_, fixed_t fracstep, inpixel_t* dc_source, int dc_texheight);
void I_DrawSpanA(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac, fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source);
#endif

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
	frt_counter2msec = 128.0f * 1000.0f / (NTSC ? NTSC_CLOCK_SPEED : PAL_CLOCK_SPEED);

	activescreen = MARS_VDP_FBCTL;

	Mars_FlipFrameBuffers(true);

	for (i = 0; i < 2; i++)
	{
		// initialize the lines section of the framebuffer
		for (j = 0; j < 224; j++)
			lines[j] = j * 320 / 2 + 0x100;

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
		while ((cmd = MARS_SYS_COMM4) == 0) {
			Mars_Slave_ReadSoundCmds();
		}

		switch (cmd) {
		case 1:
			Mars_Slave_R_SegCommands();
			break;
		case 2:
			Mars_Slave_R_PrepWalls();
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
	return (int)((float)c * frt_counter2msec);
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
	return (byte *)(I_ViewportBuffer() + 320 * screenHeight / 2);
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
	if (debugmode == 2)
		Mars_ClearFrameBuffer();
}

void I_ClearWorkBuffer(void)
{
	int *p = (int *)I_WorkBuffer();
	int *p_end = (int *)framebufferend;
	while (p < p_end)
		*p++ = 0;
}

/*
==============================================================================

						TEXTURE MAPPING CODE

==============================================================================
*/

#ifdef USE_C_DRAW

static void I_DrawColumnPO2C(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight) ATTR_DATA_CACHE_ALIGN;
static void I_DrawColumnNPO2C(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight) ATTR_DATA_CACHE_ALIGN;
static void I_DrawSpanC(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source) ATTR_DATA_CACHE_ALIGN;

static void I_DrawColumnPO2C(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{
	unsigned	heightmask;
	volatile pixel_t* dest;
	short* dc_colormap;
	unsigned	frac;
	unsigned    count, n;

	if (dc_yl > dc_yh)
		return;

	frac = frac_;
	heightmask = dc_texheight - 1;
	dest = I_ViewportBuffer() + dc_yl * 320 / 2 + dc_x;
	dc_colormap = dc_colormaps + light;

#define DO_PIXEL() do { \
		*dest = dc_colormap[dc_source[(frac >> FRACBITS) & heightmask]]; \
		dest += 320/2; \
		frac += fracstep; \
	} while (0)

	count = dc_yh - dc_yl + 1;
	n = (count + 7) >> 3;

	switch (count & 7)
	{
	case 0: do { DO_PIXEL();
	case 7:      DO_PIXEL();
	case 6:      DO_PIXEL();
	case 5:      DO_PIXEL();
	case 4:      DO_PIXEL();
	case 3:      DO_PIXEL();
	case 2:      DO_PIXEL();
	case 1:      DO_PIXEL();
	} while (--n > 0);
	}

#undef DO_PIXEL
}

//
// CALICO: the Jag blitter could wrap around textures of arbitrary height, so
// we need to do the "tutti frutti" fix here. Carmack didn't bother fixing
// this for the NeXT "simulator" build of the game.
//
static void I_DrawColumnNPO2C(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{
	unsigned	heightmask;
	volatile pixel_t* dest;
	short* dc_colormap;
	unsigned    count, n;
	unsigned 	frac;

	if (dc_yl > dc_yh)
		return;

	heightmask = dc_texheight << FRACBITS;
	if (frac_ < 0)
		while ((frac_ += heightmask) < 0);
	else
	{
		while (frac_ >= heightmask)
			frac_ -= heightmask;
	}
	frac = frac_;

	dest = I_ViewportBuffer() + dc_yl * 320 / 2 + dc_x;
	dc_colormap = dc_colormaps + light;

	count = dc_yh - dc_yl + 1;
	n = (count + 7) >> 3;

#define DO_PIXEL() do { \
		*dest = dc_colormap[dc_source[frac >> FRACBITS]]; \
		dest += 320/2; \
		if ((frac += fracstep) >= heightmask) \
			frac -= heightmask; \
	} while (0)

	switch (count & 7)
	{
	case 0: do { DO_PIXEL();
	case 7:      DO_PIXEL();
	case 6:      DO_PIXEL();
	case 5:      DO_PIXEL();
	case 4:      DO_PIXEL();
	case 3:      DO_PIXEL();
	case 2:      DO_PIXEL();
	case 1:      DO_PIXEL();
	} while (--n > 0);
	}

#undef DO_PIXEL
}

static void I_DrawSpanC(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac, 
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source)
{
	unsigned xfrac, yfrac;
	volatile pixel_t* dest;
	int		spot;
	unsigned count, n;
	short* dc_colormap;

	count = ds_x2 - ds_x1 + 1;
	xfrac = ds_xfrac, yfrac = ds_yfrac;

	dest = I_ViewportBuffer() + ds_y * 320 / 2 + ds_x1;
	dc_colormap = dc_colormaps + light;

#define DO_PIXEL() do { \
		spot = ((yfrac >> (16 - 6)) & (63 * 64)) + ((xfrac >> 16) & 63); \
		*dest++ = dc_colormap[ds_source[spot]]; \
		xfrac += ds_xstep, yfrac += ds_ystep; \
	} while(0)

	n = (count + 7) >> 3;
	switch (count & 7)
	{
	case 0: do { DO_PIXEL();
	case 7:      DO_PIXEL();
	case 6:      DO_PIXEL();
	case 5:      DO_PIXEL();
	case 4:      DO_PIXEL();
	case 3:      DO_PIXEL();
	case 2:      DO_PIXEL();
	case 1:      DO_PIXEL();
	} while (--n > 0);
	}

#undef DO_PIXEL
}

#endif

/*
==================
=
= I_DrawColumn
=
= Source is the top of the column to scale
=
==================
*/
void I_DrawColumn(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac, fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{
#ifdef RANGECHECK 
	if ((unsigned)dc_x >= screenWidth || dc_yl < 0 || dc_yh >= screenHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (debugmode == 2)
		return;

	if (dc_texheight & (dc_texheight - 1)) // height is not a power-of-2?
		I_DrawColumnNPO2A(dc_x, dc_yl, dc_yh, light, frac, fracstep, dc_source, dc_texheight);
	else
		I_DrawColumnPO2A(dc_x, dc_yl, dc_yh, light, frac, fracstep, dc_source, dc_texheight);
}

/*
================
=
= I_DrawSpan
=
================
*/
void I_DrawSpan(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac, fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source)
{
#ifdef RANGECHECK 
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2 >= screenWidth
		|| (unsigned)ds_y>screenHeight)
		I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif 

	if (debugmode == 2)
		return;

	I_DrawSpanA(ds_y, ds_x1, ds_x2, light, ds_xfrac, ds_yfrac, ds_xstep, ds_ystep, ds_source);
}

/*=========================================================================== */

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
