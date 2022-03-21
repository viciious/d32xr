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

typedef struct {
	VINT repeat;
	boolean prev_state;
} btnstate_t;

int COLOR_WHITE = 0x04;
int COLOR_BLACK = 0xF7;

short	*dc_colormaps;
const byte	*new_palette = NULL;

boolean	debugscreenactive = false;
boolean	debugscreenupdate = false;

int		lastticcount = 0;
int		lasttics = 0;
static int fpscount = 0;

int 	debugmode = 0;
VINT	strafebtns = 0;

extern int 	cy;
extern int tictics, drawtics;

// framebuffer start is after line table AND a single blank line
static volatile pixel_t* framebuffer = &MARS_FRAMEBUFFER + 0x100;
static volatile pixel_t *framebufferend = &MARS_FRAMEBUFFER + 0x10000;

static jagobj_t* jo_stbar;
static VINT jo_stbar_height;

extern int t_ref_bsp[4], t_ref_prep[4], t_ref_segs[4], t_ref_planes[4], t_ref_sprites[4], t_ref_total[4];

void I_ClearFrameBuffer(void) ATTR_DATA_CACHE_ALIGN;

static int Mars_ConvGamepadButtons(int ctrl)
{
	int newc = 0;
	int	alwrun;

	if (ctrl & SEGA_CTRL_UP)
		newc |= BT_UP;
	if (ctrl & SEGA_CTRL_DOWN)
		newc |= BT_DOWN;
	if (ctrl & SEGA_CTRL_LEFT)
		newc |= BT_LEFT;
	if (ctrl & SEGA_CTRL_RIGHT)
		newc |= BT_RIGHT;
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

	if (ctrl & SEGA_CTRL_MODE)
	{
		newc |= BT_MODE;
	}
	else
	{
		if (strafebtns)
		{
			if (ctrl & SEGA_CTRL_A)
				newc |= configuration[controltype][0];
			if (ctrl & SEGA_CTRL_B)
				newc |= configuration[controltype][1];

			switch (strafebtns)
			{
			default:
			case 1:
				if (ctrl & SEGA_CTRL_C)
					newc |= configuration[controltype][2];

				if (ctrl & SEGA_CTRL_X)
					newc |= BT_NWEAPN;
				if (ctrl & SEGA_CTRL_Y)
					newc |= BT_STRAFELEFT;
				if (ctrl & SEGA_CTRL_Z)
					newc |= BT_STRAFERIGHT;
				break;
			case 2:
				if (ctrl & SEGA_CTRL_C)
					newc |= BT_STRAFERIGHT;

				if (ctrl & SEGA_CTRL_X)
					newc |= BT_NWEAPN;
				if (ctrl & SEGA_CTRL_Y)
					newc |= configuration[controltype][2];
				if (ctrl & SEGA_CTRL_Z)
					newc |= BT_STRAFELEFT;
				break;
			case 3:
				if (ctrl & SEGA_CTRL_C)
					newc |= configuration[controltype][2];

				if (ctrl & SEGA_CTRL_X)
					newc |= BT_STRAFELEFT;
				if (ctrl & SEGA_CTRL_Y)
					newc |= BT_NWEAPN;
				if (ctrl & SEGA_CTRL_Z)
					newc |= BT_STRAFERIGHT;
				break;
			}
		}
		else
		{
			if (ctrl & SEGA_CTRL_A)
				newc |= configuration[controltype][0];
			if (ctrl & SEGA_CTRL_B)
				newc |= configuration[controltype][1];
			if (ctrl & SEGA_CTRL_C)
				newc |= configuration[controltype][2];

			if (ctrl & SEGA_CTRL_X)
				newc |= BT_PWEAPN;
			if (ctrl & SEGA_CTRL_Y)
				newc |= BT_NWEAPN;
			if (ctrl & SEGA_CTRL_Z)
				newc |= BT_AUTOMAP;

			if (newc & BT_USE)
				newc |= BT_STRAFE;
		}
	}

	alwrun = alwaysrun ? BT_SPEED : 0;
	if (demoplayback || demorecording)
		alwrun = 0;
	newc ^= alwrun;

	if ((newc & (BT_UP | BT_DOWN | BT_SPEED)) == BT_SPEED && !alwrun)
		newc |= BT_FASTTURN;

	return newc;
}

// mostly exists to handle the 3-button controller situation
static int Mars_HandleStartHeld(unsigned *ctrl, const unsigned ctrl_start, btnstate_t *startbtn)
{
	int morebuttons = 0;
	boolean start = 0;
	static const int held_tics = 16;

	start = (*ctrl & ctrl_start) != 0;
	if (start ^ startbtn->prev_state) {
		int prev_repeat = startbtn->repeat;
		startbtn->repeat = 0;

		// start button state changed
		if (startbtn->prev_state) {
			startbtn->prev_state = false;
			// quick key press and release
			if (prev_repeat < held_tics)
				return BT_START;

			// key held for a while and then released
			return 0;
		}

		startbtn->prev_state = true;
	}

	if (!start) {
		return 0;
	}

	startbtn->repeat++;
	if (startbtn->repeat < 2) {
		// suppress action buttons
		//*ctrl = *ctrl & ~(SEGA_CTRL_A | SEGA_CTRL_B | SEGA_CTRL_C);
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

	if (morebuttons)
	{
		startbtn->repeat = held_tics;
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

void Mars_Secondary(void)
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
	SH2_DMA_DMAOR = 1; 	// enable DMA

	SH2_DMA_VCR1 = 66; 	// set exception vector for DMA channel 1
	SH2_INT_IPRA = (SH2_INT_IPRA & 0xF0FF) | 0x0400; // set DMA INT to priority 4

	SetSH2SR(1); 		// allow ints

	while (1)
	{
		int cmd;
		while ((cmd = MARS_SYS_COMM4) == 0);

		switch (cmd) {
		case 1:
			Mars_ClearCache();
			break;
		case 2:
			Mars_Sec_R_Setup();
			break;
		case 3:
			Mars_Sec_R_WallPrep();
			break;
		case 4:
			Mars_Sec_R_WallPrep();
			Mars_Sec_R_SegCommands();
			break;
		case 5:
			Mars_Sec_R_DrawPlanes();
			break;
		case 6:
			Mars_Sec_R_DrawSprites((int *)(*(uintptr_t *)&MARS_SYS_COMM12), MARS_SYS_COMM6);
			break;
		case 7:
			Mars_Sec_R_DrawPSprites();
			break;
		case 8:
			Mars_Sec_M_AnimateFire();
			break;
		case 9:
			// break current command
			break;
		case 10:
			Mars_Sec_InitSoundDMA();
			break;
		case 11:
			Mars_Sec_StopSoundMixer();
			break;
		case 12:
			Mars_Sec_StartSoundMixer();
			break;
		case 13:
			break;
		case 14:
			Mars_Sec_AM_Drawer();
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
	int	i;
	unsigned minr, maxr;
	const byte	*doompalette;

	Mars_SetBrightness(1);

	doompalette = W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
	I_SetPalette(doompalette);

	// look up palette indices for black and white colors
	// if the black color isn't present, use the darkest one
	minr = 255;
	maxr = 0;
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
		if (r > maxr) {
			maxr = r;
			COLOR_WHITE = i;
		}
	}

	dc_colormaps = Z_Malloc(33*512, PU_STATIC, 0);

	I_InitColormap();

	i = W_CheckNumForName("STBAR");
	if (i != -1)
	{
		jo_stbar = (jagobj_t*)W_POINTLUMPNUM(i);
		jo_stbar_height = BIGSHORT(jo_stbar->height);
	}
	else
	{
		jo_stbar = NULL;
		jo_stbar_height = 0;
	}
}

void I_InitColormap(void)
{
	int	i, j;
	int l, s;
	byte* doomcolormap;

	if (!dc_colormaps)
		return;

	l = -1;
	if (colormapopt == 1)
		l = W_CheckNumForName("COLORMA1");
	if (l < 0)
		l = W_CheckNumForName("COLORMAP");

	doomcolormap = I_WorkBuffer();
	D_memset(doomcolormap, 0, 512 * 33);
	s = W_ReadLump(l, doomcolormap);
	s /= 512;

	for (i = 0; i < s; i++) {
		const byte* sl1 = &doomcolormap[i * 512];
		const byte* sl2 = &doomcolormap[i * 512 + 256];
		byte* dl1 = (byte*)&dc_colormaps[i * 256];
		byte* dl2 = (byte*)&dc_colormaps[i * 256 + 128];
		D_memcpy(dl1, sl2, 256);
		D_memcpy(dl2, sl1, 256);

		for (j = 0; j < 512; j += 2) {
			if (dl1[j] == 0)
				dl1[j] = COLOR_BLACK;
			if (dl1[j + 1] == 0)
				dl1[j + 1] = COLOR_BLACK;
		}
	}

	if (s < 33)
		D_memcpy(dc_colormaps + 256 * 32, dc_colormaps, 512);
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
= I_RemapLumpPtr
====================
*/

void* I_RemapLumpPtr(void *ptr)
{
	static int8_t curbank7page = 7;

	if ((uintptr_t)ptr >= 0x02380000)
	{
		void* newptr = (void*)(((uintptr_t)ptr & 0x0007FFFF) + 0x02380000);

		int page = (((uintptr_t)ptr - 0x02000000) >> 19);
		if (curbank7page != page)
		{
			S_Clear();
			Mars_SetBankPage(7, page);
			Mars_ClearCache();
			Mars_CommSlaveClearCache();
			curbank7page = page;
		}

		ptr = newptr;
	}

	return ptr;
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

static int I_ReadControls_(volatile unsigned *c, btnstate_t* startbtn)
{
	int ctrl;
	unsigned val;

	val = *c;
	*c = 0;

	ctrl = 0;
	ctrl |= Mars_HandleStartHeld(&val, SEGA_CTRL_START, startbtn);
	ctrl |= Mars_ConvGamepadButtons(val);
	return ctrl;
}

int I_ReadControls(void)
{
	static btnstate_t startbtn = { 0 };
	return I_ReadControls_(&mars_controls, &startbtn);
}

int I_ReadControls2(void)
{
	static btnstate_t startbtn2 = { 0 };
	return I_ReadControls_(&mars_controls2, &startbtn2);
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
byte *I_TempBuffer (void)
{
	byte *w = (byte *)framebuffer;
	int *p, *p_end = (int*)framebufferend;

	// clear the buffer so the fact that 32x ignores 0-byte writes goes unnoticed
	// the buffer cannot be re-used without clearing it again though
	for (p = (int*)w; p < p_end; p++)
		*p = 0;

	return w;
}

byte *I_WorkBuffer (void)
{
	while (!I_RefreshCompleted());
	return (byte *)(framebuffer + 320 / 2 * (I_FrameBufferHeight() +1)); // +1 for the blank line
}

pixel_t	*I_FrameBuffer (void)
{
	return (pixel_t *)framebuffer;
}

int I_FrameBufferHeight(void) {
	return mars_framebuffer_height;
}

int I_IsPAL(void) {
	return Mars_IsPAL();
}

pixel_t* I_OverwriteBuffer(void)
{
	return (pixel_t*)&MARS_OVERWRITE_IMG + 0x100;
}

int I_ViewportYPos(void)
{
	const int fbh = I_FrameBufferHeight();

	if (splitscreen)
		return (fbh - viewportHeight) / 2;

	if (viewportWidth < 160)
		return (fbh - jo_stbar_height - viewportHeight) / 2;
	if (viewportWidth == 160 && lowResMode)
		return (fbh - jo_stbar_height - viewportHeight);
	return (fbh - jo_stbar_height - viewportHeight) / 2;
}

pixel_t	*I_ViewportBuffer (void)
{
	int x = 0;
	pixel_t *vb = (pixel_t * )framebuffer;
	
	if (splitscreen)
	{
		x = vd.displayplayer ? 160 : 0;
	}
	else
	{
		if (viewportWidth <= 160)
			x = (320 - viewportWidth * 2) / 2;
		else
			x = (320 - viewportWidth) / 2;
	}

	vb += I_ViewportYPos() * 320 / 2 + x / 2;
	return (pixel_t *)vb;
}

void I_ClearFrameBuffer (void)
{
	int* p = (int*)framebuffer;
	int* p_end = (int*)(framebuffer + 320 / 2 * (I_FrameBufferHeight()+1));
	while (p < p_end)
		*p++ = 0;
}

void I_DebugScreen(void)
{
	int i;

	if (!debugscreenupdate)
		return;

	if (debugmode == 1)
	{
		Mars_DebugStart();
		Mars_DebugQueue(DEBUG_FPSCOUNT, fpscount);
		Mars_DebugEnd();
	}
	else if (debugmode > 1)
	{
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

		Mars_DebugStart();

		Mars_DebugQueue(DEBUG_FPSCOUNT, fpscount);
		Mars_DebugQueue(DEBUG_LASTTICS, lasttics);
		Mars_DebugQueue(DEBUG_GAMEMSEC, Mars_FRTCounter2Msec(tictics));

		Mars_DebugQueue(DEBUG_BSPMSEC, Mars_FRTCounter2Msec(t_ref_bsp_avg));

		Mars_DebugQueue(DEBUG_SEGSMSEC, Mars_FRTCounter2Msec(t_ref_segs_avg));
		Mars_DebugQueue(DEBUG_SEGSCOUNT, lastwallcmd - viswalls);

		Mars_DebugQueue(DEBUG_PLANESMSEC, Mars_FRTCounter2Msec(t_ref_planes_avg));
		Mars_DebugQueue(DEBUG_PLANESCOUNT, lastvisplane - visplanes - 1);

		Mars_DebugQueue(DEBUG_SPRITESMSEC, Mars_FRTCounter2Msec(t_ref_sprites_avg));
		Mars_DebugQueue(DEBUG_SPRITESCOUNT, vissprite_p - vissprites);

		Mars_DebugQueue(DEBUG_REFMSEC, Mars_FRTCounter2Msec(t_ref_total_avg));

		Mars_DebugQueue(DEBUG_DRAWMSEC, Mars_FRTCounter2Msec(drawtics));

		Mars_DebugEnd();
	}

	debugscreenupdate = false;
}

void I_ClearWorkBuffer(void)
{
	int *p = (int *)I_WorkBuffer();
	int *p_end = (int *)framebufferend;
	while (p < p_end)
		*p++ = 0;
}

void I_ResetLineTable(void)
{
	Mars_InitLineTable();
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
	const int refreshHZ = Mars_RefreshHZ();

	if (ticsperframe < MINTICSPERFRAME)
		ticsperframe = MINTICSPERFRAME;
	else if (ticsperframe > MAXTICSPERFRAME)
		ticsperframe = MAXTICSPERFRAME;

	if (players[consoleplayer].automapflags & AF_OPTIONSACTIVE)
		if ((ticrealbuttons & BT_MODE) && !(oldticrealbuttons & BT_MODE))
		{
			int prevdebugmode = debugmode;

			debugmode = (debugmode + 1) % 5;
			debugscreenupdate = true;

			if (prevdebugmode == 4 || debugmode == 4)
			{
				R_ClearTexCache(&r_texcache);
			}

			if (prevdebugmode == 0)
			{
				SH2_WDT_WTCSR_TCNT = 0x5A00; /* WDT TCNT = 0 */
				SH2_WDT_WTCSR_TCNT = 0xA53E; /* WDT TCSR = clr OVF, IT mode, timer on, clksel = Fs/4096 */
			}
			else if (!debugmode)
			{
				Mars_ClearNTA();
				SH2_WDT_WTCSR_TCNT = 0xA518; /* WDT TCSR = clr OVF, IT mode, timer off, clksel = Fs/2 */
			}

			clearscreen = 2;
		}

	Mars_FlipFrameBuffers(false);

	/* */
	/* wait until on the third tic after last display */
	/* */
	const int ticwait = (demoplayback || demorecording ? 4 : ticsperframe); // demos were recorded at 15-20fps
	do
	{
		ticcount = I_GetTime();
	} while (ticcount - lastticcount < ticwait);

	lasttics = ticcount - lastticcount;
	lastticcount = ticcount;
	if (lasttics > 99) lasttics = 99;

	if (ticcount > prevsecticcount + refreshHZ) {
		static int prevsecframe;
		fpscount = (framenum - prevsecframe) * refreshHZ / (ticcount - prevsecticcount);
		debugscreenupdate = true;
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


/*
 *  Network support functions
 */

static void Player0Setup (void)
{
	int		val, idbyte, buttons;

	I_Print8(104,5,"Player 0 setup");
	I_Update();
	consoleplayer = 0;	/* we are player 0 */
	idbyte = startmap + 24*startskill + 128*(starttype == gt_deathmatch);
	
	do
	{
		/* wait until rec 0x22 byte from other side or player aborts */
		buttons = I_ReadControls();
		if (buttons & BT_START)
		{
			starttype = gt_single;
			return;	/* abort */
		}

		val = Mars_GetNetByte(1);
		if (val == 0x22)
			return;	/* ready to go! */

		Mars_PutNetByte(idbyte);	/* ignore timeout */
	} while (1);
}

static void Player1Setup (void)
{	
	int		val, oldval, buttons;

	I_Print8(104,5,"Player 1 setup");
	I_Update();
	consoleplayer = 1;	/* we are player 1 */
	oldval = 999;

	do
	{
		/* start game after two identical id bytes unless user aborts  */
		buttons = I_ReadControls();
		if (buttons & BT_START)
		{
			starttype = gt_single;
			return;	/* abort */
		}

		val = Mars_GetNetByte(1);
		if (val < 0)
			continue;
		if (val == oldval)
			break;
		oldval = val;
	} while (1);

	starttype = (val & 128) ? gt_deathmatch : gt_coop;
	startskill = (val & 127) / 24;
	startmap = (val & 127) % 24;

	Mars_PutNetByte(0x22);	/* send an acknowledge byte */
	Mars_PutNetByte(0x22);	/* send another acknowledge byte */
}

void I_NetSetup (void)
{
	int		listen1, listen2;

	while (!I_RefreshCompleted());

	Mars_SetupNet(1); /* 0 = no net, 1 = system link cable, -1 = serial cable */

	I_Print8(64,1,"Attempting to connect..."); 
	I_Print8(80,3,"Press start to abort");
//	I_Update();

	while (Mars_GetNetByte(0) != -2) ;  /* flush network buffer */
	Mars_WaitTicks(5);

	listen1 = Mars_GetNetByte(0);
	listen2 = Mars_GetNetByte(0);
	if (listen1 < 0 && listen2 < 0)
		Player0Setup();	/* bytes not waiting - we are player 0 */
	else
		Player1Setup();	/* bytes waiting, we are player 1 */

	Mars_WaitTicks(5);

	while (Mars_GetNetByte(0) != -2); /* flush network buffer */
}

void I_NetStop(void)
{
	Mars_CleanupNet();
}

#define PACKET_SIZE 6
void G_PlayerReborn (int player);

unsigned I_NetTransfer (unsigned buttons)
{
	int			i, val;
	unsigned	inbytes[PACKET_SIZE];
	unsigned	outbytes[PACKET_SIZE];
	unsigned	consistency;

	consistency = players[0].mo->x ^ players[0].mo->y ^ players[0].mo->z ^ players[1].mo->x ^ players[1].mo->y ^ players[1].mo->z;
	consistency = (consistency>>8) ^ consistency ^ (consistency>>16);

	outbytes[0] = (buttons>>24) & 0xff;
	outbytes[1] = (buttons>>16) & 0xff;
	outbytes[2] = (buttons>>8) & 0xff;
	outbytes[3] = (buttons) & 0xff;
	outbytes[4] = consistency & 0xff;
	outbytes[5] = vblsinframe;

	if (consoleplayer)
	{
		/* player 1 waits before sending */
		for (i=0; i<=PACKET_SIZE-1; i++)
		{
			val = Mars_GetNetByte(120);
			if (val < 0)
				goto reconnect;
			inbytes[i] = val;
			Mars_PutNetByte(outbytes[i]);
		}
		vblsinframe = inbytes[5];	/* take gamevbls from other player */
	}
	else
	{
		/* player 0 sends first */
		for (i=0; i<=PACKET_SIZE-1; i++)
		{
			Mars_PutNetByte(outbytes[i]);
			val = Mars_GetNetByte(120);
			if (val < 0)
				goto reconnect;
			inbytes[i] = val;
		}
	}
	
	if (inbytes[4] != outbytes[4])
	{
		/* consistency error */
		I_Print8(108,23,"Network Error"); 
		I_Update();
		while (Mars_GetNetByte(0) != -2); /* flush network buffer */
		Mars_WaitTicks(200);
		goto reconnect;
	}

	val = (inbytes[0]<<24) + (inbytes[1]<<16) + (inbytes[2]<<8) + inbytes[3];
	return val;
	
reconnect:
	/*
	 *  Player 1 must delay so that Player 0 finds no waiting data during
	 *  setup and stays Player 0.
	 */
	if (consoleplayer)
		Mars_WaitTicks(60);
	I_NetSetup();
	G_PlayerReborn(0);
	G_PlayerReborn(1);
	gameaction = ga_warped;
	ticbuttons[0] = ticbuttons[1] = oldticbuttons[0] = oldticbuttons[1] = 0;
	return 0;
}


void I_DrawSbar(void)
{
	int i, p;
	int y[MAXPLAYERS];

	if (!jo_stbar)
		return;

	int width = BIGSHORT(jo_stbar->width);
	int height = BIGSHORT(jo_stbar->height);
	int halfwidth = (unsigned)width >> 1;

	y[0] = I_FrameBufferHeight() - height;
	y[1] = 0;

	p = 1;
	p += splitscreen ? 1 : 0;
	for (i = 0; i < p; i++) {
		int h;
		const pixel_t* source = (pixel_t*)jo_stbar->data;
		pixel_t* dest = (pixel_t*)I_FrameBuffer() + y[i] * 320 / 2;

		for (h = height; h; h--)
		{
			int i;
			for (i = 0; i < halfwidth; i++)
				dest[i] = source[i];
			source += halfwidth;
			dest += 320 / 2;
		}
	}
}
