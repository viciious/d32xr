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
#include "r_local.h"
#include "wadbase.h"

int COLOR_WHITE = 0x04;
int COLOR_BLACK = 0xF7;

short	*dc_colormaps;
const byte	*new_palette = NULL;

boolean	debugscreenactive = false;

int		lastticcount = 0;
int		lasttics = 0;
static int fpscount = 0;

int 	debugmode = 0;

extern int 	cy;
extern int tictics;

// framebuffer start is after line table AND a single blank line
static volatile pixel_t* framebuffer = &MARS_FRAMEBUFFER + 0x100;
static volatile pixel_t *framebufferend = &MARS_FRAMEBUFFER + 0x10000;

static jagobj_t* stbar;
static VINT stbar_height;

extern int t_ref_bsp[4], t_ref_prep[4], t_ref_segs[4], t_ref_planes[4], t_ref_sprites[4], t_ref_total[4];

static int Mars_ConvGamepadButtons(int ctrl)
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

	if (ctrl & SEGA_CTRL_MODE)
	{
		newc |= BT_MODE;
		if (ctrl & SEGA_CTRL_A)
			newc |= BT_A;
		if (ctrl & SEGA_CTRL_B)
			newc |= BT_B;
		if (ctrl & SEGA_CTRL_C)
			newc |= BT_C;

		if (ctrl & SEGA_CTRL_X)
			newc |= BT_X;
		if (ctrl & SEGA_CTRL_Y)
			newc |= BT_Y;
		if (ctrl & SEGA_CTRL_Z)
			newc |= BT_Z;
	}
	else
	{
		if (ctrl & SEGA_CTRL_A)
			newc |= BT_A | configuration[controltype][0];
		if (ctrl & SEGA_CTRL_B)
			newc |= BT_B | configuration[controltype][1];
		if (ctrl & SEGA_CTRL_C)
			newc |= BT_C | configuration[controltype][2] | BT_STRAFE;

		if (ctrl & SEGA_CTRL_X)
			newc |= BT_X | BT_PWEAPN;
		if (ctrl & SEGA_CTRL_Y)
			newc |= BT_Y | BT_NWEAPN;
		if (ctrl & SEGA_CTRL_Z)
			newc |= BT_Z | BT_AUTOMAP;
	}

	return newc;
}

// mostly exists to handle the 3-button controller situation
static int Mars_HandleStartHeld(unsigned *ctrl, const unsigned ctrl_start)
{
	int morebuttons = 0;
	boolean start = 0;
	static boolean prev_start = false;
	static int repeat = 0;
	static const int held_tics = 8;

	start = (*ctrl & ctrl_start) != 0;
	if (start ^ prev_start) {
		int prev_repeat = repeat;
		repeat = 0;

		// start button state changed
		if (prev_start) {
			prev_start = false;
			// quick key press and release
			if (prev_repeat < held_tics)
				return BT_START;

			// key held for a while and then released
			return 0;
		}

		prev_start = true;
	}

	if (!start) {
		return 0;
	}

	repeat++;
	if (repeat < held_tics) {
		// suppress action buttons
		*ctrl = *ctrl & ~(SEGA_CTRL_A | SEGA_CTRL_B | SEGA_CTRL_C);
		return 0;
	}

	if (*ctrl & SEGA_CTRL_A) {
		*ctrl = *ctrl & ~SEGA_CTRL_A;
		morebuttons |= BT_PWEAPN;
	}
	else if (*ctrl & SEGA_CTRL_B) {
		*ctrl = *ctrl & ~SEGA_CTRL_B;
		morebuttons |= BT_NWEAPN;
	}
	if (*ctrl & SEGA_CTRL_C) {
		*ctrl = *ctrl & ~SEGA_CTRL_C;
		morebuttons |= BT_AUTOMAP;
	}

	return morebuttons;
}

static int Mars_ConvMouseButtons(int mouse)
{
	int ctrl = 0;
	if (mouse & SEGA_CTRL_LMB)
	{
		ctrl |= BT_ATTACK; // L -> B
		ctrl |= BT_LMBTN;
	}
	if (mouse & SEGA_CTRL_RMB)
	{
		ctrl |= BT_USE; // R -> C
		ctrl |= BT_RMBTN;
	}
	if (mouse & SEGA_CTRL_MMB)
	{
		ctrl |= BT_NWEAPN; // M -> Y
		ctrl |= BT_MMBTN;
	}
	if (mouse & SEGA_CTRL_STARTMB)
	{
		//ctrl |= BT_START;
	}
	return ctrl;
}

void Mars_Slave(void)
{
	// init DMA
	SH2_DMA_SAR0 = 0;
	SH2_DMA_DAR0 = 0;
	SH2_DMA_TCR0 = 0;
	SH2_DMA_CHCR0 = 0;
	SH2_DMA_DRCR0 = 0;
	SH2_DMA_SAR1 = 0;
	SH2_DMA_DAR1 = 0;
	SH2_DMA_TCR1 = 0;
	SH2_DMA_CHCR1 = 0;
	SH2_DMA_DRCR1 = 0;
	SH2_DMA_DMAOR = 1; // enable DMA

	SH2_DMA_VCR1 = 72; // set exception vector for DMA channel 1
	SH2_INT_IPRA = (SH2_INT_IPRA & 0xF0FF) | 0x0F00; // set DMA INT to priority 15

	SetSH2SR(2); // allow ints

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
			Mars_Slave_R_WallPrep();
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
		case 11:
			Mars_Slave_StopSoundMixer();
			break;
		case 12:
			Mars_Slave_StartSoundMixer();
			break;
		default:
			break;
		}

		MARS_SYS_COMM4 = 0;
	}
}

int Mars_FRTCounter2Msec(int c)
{
	return (c * mars_frtc2msec_frac) >> FRACBITS;
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
	unsigned minr;
	const byte	*doompalette;
	const byte 	*doomcolormap;

	doompalette = W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
	I_SetPalette(doompalette);

	// look up palette indices for black and white colors
	// if the black color isn't present, use the darkest one
	minr = 255;
	for (i = 1; i < 256; i++)
	{
		unsigned r = doompalette[i * 3 + 0];
		unsigned g = doompalette[i * 3 + 1];
		unsigned b = doompalette[i * 3 + 2];
		if (r != g || r != b) {
			continue;
		}
		if (r <= minr) {
			minr = r;
			COLOR_BLACK = i;
		}
		if (r == 255) {
			COLOR_WHITE = i;
		}
	}

	doomcolormap = W_POINTLUMPNUM(W_GetNumForName("COLORMAPS"));
	dc_colormaps = Z_Malloc(64*256, PU_STATIC, 0);

	for(i = 0; i < 32; i++) {
		const byte *sl1 = &doomcolormap[i*512];
		const byte *sl2 = &doomcolormap[i*512+256];
		byte *dl1 = (byte *)&dc_colormaps[i*256];
		byte *dl2 = (byte *)&dc_colormaps[i*256+128];
		D_memcpy(dl1, sl2, 256);
		D_memcpy(dl2, sl1, 256);

		for (j = 0; j < 512; j+=2) {
			if (dl1[j] == 0)
				dl1[j] = COLOR_BLACK;
			if (dl1[j+1] == 0)
				dl1[j+1] = COLOR_BLACK;
		}
	}

	i = W_CheckNumForName("STBAR");
	if (i != -1)
	{
		stbar = (jagobj_t*)W_POINTLUMPNUM(i);
		stbar_height = BIGSHORT(stbar->height);
	}
	else
	{
		stbar = NULL;
		stbar_height = 0;
	}
}

void I_SetPalette(const byte* palette)
{
	mars_newpalette = palette;
}

boolean	I_RefreshCompleted (void)
{
	return Mars_FramebuffersFlipped();
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
	int ctrl;
	unsigned val;

	val = mars_controls;
	mars_controls = 0;

	ctrl = 0;
	ctrl |= Mars_HandleStartHeld(&val, SEGA_CTRL_START);
	ctrl |= Mars_ConvGamepadButtons(val);
	ctrl |= Mars_ConvGamepadButtons(val);
	return ctrl;
}

int I_ReadMouse(int* pmx, int *pmy)
{
	int mval, ctrl;
	static int oldmval = 0;
	unsigned val;

	*pmx = *pmy = 0;
	if (!mousepresent)
		return 0;

	mval = Mars_PollMouse(mars_mouseport);
	switch (mval)
	{
	case -2:
		// timeout - return old buttons and no deltas
		mval = oldmval & 0x00F70000;
		break;
	case -1:
		// no mouse
		mousepresent = false;
		oldmval = 0;
		return 0;
	default:
		oldmval = mval;
		break;
	}

	val = Mars_ParseMousePacket(mval, pmx, pmy);

	ctrl = 0;
	//ctrl |= Mars_HandleStartHeld(&val, SEGA_CTRL_STARTMB);
	ctrl |= Mars_ConvMouseButtons(val);
	return ctrl;
}

int	I_GetTime (void)
{
	return Mars_GetTicCount();
}

int I_GetFRTCounter(void)
{
	return Mars_GetFRTCounter();
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
	byte *w = (byte *)framebuffer;
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
	return (byte *)(framebuffer + 320 / 2 * (224+1)); // +1 for the blank line
}

pixel_t	*I_FrameBuffer (void)
{
	return (pixel_t *)framebuffer;
}

pixel_t* I_OverwriteBuffer(void)
{
	return (pixel_t*)&MARS_OVERWRITE_IMG + 0x100;
}

int I_ViewportYPos(void)
{
	if (viewportWidth < 160)
		return (224 - stbar_height - viewportHeight) / 2;
	if (viewportWidth == 160)
		return (224 - stbar_height - viewportHeight);
	return (224 - stbar_height - viewportHeight) / 2;
}

pixel_t	*I_ViewportBuffer (void)
{
	volatile pixel_t *viewportbuffer = framebuffer;
	if (viewportWidth <= 160)
		viewportbuffer += I_ViewportYPos() * 320 / 2 + (320 - viewportWidth * 2) / 4;
	else
		viewportbuffer += I_ViewportYPos() * 320 / 2 + (320 - viewportWidth) / 4;
	return (pixel_t *)viewportbuffer;
}

void I_ClearFrameBuffer (void)
{
	int* p = (int*)framebuffer;
	int* p_end = (int*)(framebuffer + 320 / 2 * (224+1));
	while (p < p_end)
		*p++ = 0;
}

void I_DebugScreen(void)
{
	int i;
	char buf[32];

	if (debugmode == 1)
	{
		D_snprintf(buf, sizeof(buf), "fps:%2d", fpscount);
		I_Print8(200, 5, buf);
	}
	else if (debugmode > 1)
	{
		int line = 5;
		unsigned t_ref_bsp_avg = 0;
		unsigned t_ref_segs_avg = 0;
		unsigned t_ref_planes_avg = 0;
		unsigned t_ref_sprites_avg = 0;
		unsigned t_ref_total_avg = 0;

		for (i = 0; i < 4; i++)
		{
			t_ref_bsp_avg += t_ref_bsp[i];
			t_ref_segs_avg += t_ref_segs[i];
			t_ref_planes_avg += t_ref_planes[i];
			t_ref_sprites_avg += t_ref_sprites[i];
			t_ref_total_avg += t_ref_total[i];
		}
		t_ref_bsp_avg >>= 2;
		t_ref_segs_avg >>= 2;
		t_ref_planes_avg >>= 2;
		t_ref_sprites_avg >>= 2;
		t_ref_total_avg >>= 2;

		D_snprintf(buf, sizeof(buf), "fps:%2d", fpscount);
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "tcs:%d", lasttics);
		I_Print8(200, line++, buf);

		line++;

		D_snprintf(buf, sizeof(buf), "g:%2d", Mars_FRTCounter2Msec(tictics));
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "b:%2d", Mars_FRTCounter2Msec(t_ref_bsp_avg));
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "w:%2d %2d", Mars_FRTCounter2Msec(t_ref_segs_avg), lastwallcmd - viswalls);
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "p:%2d %2d", Mars_FRTCounter2Msec(t_ref_planes_avg), lastvisplane - visplanes - 1);
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "s:%2d %2d", Mars_FRTCounter2Msec(t_ref_sprites_avg), vissprite_p - vissprites);
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "t:%2d", Mars_FRTCounter2Msec(t_ref_total_avg));
		I_Print8(200, line++, buf);
	}
}

void I_ClearWorkBuffer(void)
{
	int *p = (int *)I_WorkBuffer();
	int *p_end = (int *)framebufferend;
	while (p < p_end)
		*p++ = 0;
}

/*=========================================================================== */

/*
====================
=
= I_Update
=
= Display the current framebuffer
= If < 1/15th second has passed since the last display, busy wait.
= 15 fps is the maximum frame rate, and any faster displays will
= only look ragged.
=
= When displaying the automap, use full resolution, otherwise use
= wide pixels
====================
*/
void I_Update(void)
{
	int ticcount;
	static int prevsecticcount = 0;
	static int framenum = 0;
	boolean NTSC = (MARS_VDP_DISPMODE & MARS_NTSC_FORMAT) != 0;
	const int refreshHZ = (NTSC ? 60 : 50);

	if (ticsperframe < MINTICSPERFRAME)
		ticsperframe = MINTICSPERFRAME;
	else if (ticsperframe > MAXTICSPERFRAME)
		ticsperframe = MAXTICSPERFRAME;

	if (players[consoleplayer].automapflags & AF_OPTIONSACTIVE)
		if ((ticrealbuttons & BT_MODE) && !(oldticrealbuttons & BT_MODE))
		{
			int prevdebugmode = debugmode;
			debugmode = (debugmode + 1) % 5;
			if (prevdebugmode == 4 || debugmode == 4)
			{
				R_ClearTexCache(&r_flatscache);
				R_ClearTexCache(&r_wallscache);
			}
			clearscreen = 2;
		}

	Mars_FlipFrameBuffers(false);

	/* */
	/* wait until on the third tic after last display */
	/* */
	const int ticwait = (demoplayback ? 3 : ticsperframe); // demos were recorded at 15-20fps
	do
	{
		ticcount = I_GetTime();
	} while (ticcount - lastticcount < ticwait);

	lasttics = ticcount - lastticcount;
	lastticcount = ticcount;

	if (ticcount > prevsecticcount + refreshHZ) {
		static int prevsecframe;
		fpscount = (framenum - prevsecframe) * refreshHZ / (ticcount - prevsecticcount);
		prevsecticcount = ticcount;
		prevsecframe = framenum;
	}
	framenum++;
	debugscreenactive = debugmode != 0;

	cy = 1;
}

void DoubleBufferSetup (void)
{
	int i;

	while (!I_RefreshCompleted())
		;

	for (i = 0; i < 2; i++) {
		I_ClearFrameBuffer();
		Mars_FlipFrameBuffers(true);
	}
}

void UpdateBuffer (void) {
	Mars_FlipFrameBuffers(true);
}

unsigned I_NetTransfer (unsigned ctrl)
{
	return 0;
}

void I_NetSetup (void)
{
}

void I_DrawSbar(void)
{
	if (!stbar)
		return;

	const pixel_t* source = (pixel_t*)stbar->data;
	int width = BIGSHORT(stbar->width);
	int height = BIGSHORT(stbar->height);
	int halfwidth = (unsigned)width >> 1;
	pixel_t* dest;

	dest = (pixel_t*)I_FrameBuffer() + (224 - height) * 320 / 2;
	for (; height; height--)
	{
		int i;
		for (i = 0; i < halfwidth; i++)
			dest[i] = source[i];
		source += halfwidth;
		dest += 320 / 2;
	}
}
