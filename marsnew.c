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
#include "p_camera.h"
#include "p_local.h"
#include "r_local.h"
#include "wadbase.h"

#define LINK_TIMEOUT_SHORT 		0x3FF
#define LINK_TIMEOUT_LONG	 	0x3FFF

typedef struct {
	VINT repeat;
	VINT prev_state;
} btnstate_t;

// !!! if this is changed, it must be changed in asm too!
typedef void (*setbankpage_t)(int bank, int page);
typedef struct {
	uint16_t bank;
	uint16_t bankpage;
	setbankpage_t setbankpage;
	VINT *validcount;
	void *columncache;
	void *colormaps;
} mars_tls_t __attribute__((aligned(16))); // thread local storage

VINT COLOR_WHITE = 0x00;
VINT COLOR_BLACK = 0x1F;

int8_t	*dc_colormaps;

boolean	debugscreenactive = false;
boolean	debugscreenupdate = false;

int     ticcount = 0;
int		lastticcount = 0;
VINT		lasttics = 0;
static int8_t fpscount = 0;

VINT 	debugmode = DEBUGMODE_NONE;

extern VINT 	cy;
extern int tictics, drawtics, ticstart;

// framebuffer start is after line table AND a single blank line
static volatile pixel_t* framebuffer = &MARS_FRAMEBUFFER + 0x100;
static volatile pixel_t *framebufferend = &MARS_FRAMEBUFFER + 0x10000;

#define jo_stbar_height 22

extern int t_ref_bsp[4], t_ref_prep[4], t_ref_segs[4], t_ref_planes[4], t_ref_sprites[4], t_ref_total[4];

static volatile mars_tls_t mars_tls_pri, mars_tls_sec;
static uint32_t mars_rom_bsw_start = 0;

void I_ClearFrameBuffer(void) ATTR_DATA_CACHE_ALIGN;

int Mars_ConvGamepadButtons(int ctrl)
{
	unsigned newc = 0;

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
		if (ctrl & SEGA_CTRL_A)
			newc |= BT_FLIP; //configuration[controltype][0];
		if (ctrl & SEGA_CTRL_B)
			newc |= BT_JUMP; //configuration[controltype][1];
		if (ctrl & SEGA_CTRL_C)
			newc |= BT_SPIN; //configuration[controltype][2];

		if (ctrl & SEGA_CTRL_X)
			newc |= BT_CAMLEFT;
		if (ctrl & SEGA_CTRL_Y)
			newc |= BT_GASPEDAL;
		if (ctrl & SEGA_CTRL_Z)
			newc |= BT_CAMRIGHT;
	}

	return newc;
}

// mostly exists to handle the 3-button controller situation
static int Mars_HandleStartHeld(int *ctrl, const int ctrl_start, btnstate_t *startbtn)
{
	int morebuttons = 0;
	boolean start = 0;
	static const VINT held_tics = 16;

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
		morebuttons |= BT_CAMLEFT;
	}
	else if (*ctrl & SEGA_CTRL_B) {
		*ctrl = *ctrl & ~SEGA_CTRL_B;
		morebuttons |= BT_GASPEDAL;
	}
	if (*ctrl & SEGA_CTRL_C) {
		*ctrl = *ctrl & ~SEGA_CTRL_C;
		morebuttons |= BT_CAMRIGHT;
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
		ctrl |= BT_JUMP; // L -> B
		ctrl |= BT_LMBTN;
	}
	if (mouse & SEGA_CTRL_RMB)
	{
		ctrl |= BT_SPIN; // R -> C
		ctrl |= BT_RMBTN;
	}
	if (mouse & SEGA_CTRL_MMB)
	{
		ctrl |= BT_FLIP; // M -> Y
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
	// init thread-local storage
	__asm volatile("mov %0, r0\n\tldc r0,gbr" : : "rm"(&mars_tls_sec) : "r0", "gbr");

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

	while (1)
	{
		int cmd;
		extern VINT *validcount;

		while ((cmd = MARS_SYS_COMM4) == MARS_SECCMD_NONE);

		switch (cmd) {
		case MARS_SECCMD_CLEAR_CACHE:
			Mars_ClearCache();
			// FIXME: find a better place for this
			I_SetThreadLocalVar(DOOMTLS_VALIDCOUNT, validcount + numlines + 1);
			break;
		case MARS_SECCMD_BREAK:
			// break current command
			break;
		case MARS_SECCMD_R_WALL_PREP_NODRAW:
			Mars_Sec_R_Setup();
			Mars_Sec_R_WallPrep();
			break;
		case MARS_SECCMD_R_WALL_PREP:
			Mars_Sec_R_Setup();
			Mars_Sec_R_WallPrep();
			Mars_Sec_R_SegCommands();
			Mars_Sec_R_PreDrawPlanes();
			break;
		case MARS_SECCMD_R_DRAW_PLANES:
			Mars_Sec_R_DrawPlanes();
			break;
		case MARS_SECCMD_R_DRAW_SPRITES:
			Mars_Sec_R_DrawSprites(MARS_SYS_COMM6);
			break;
		case MARS_SECCMD_M_ANIMATE_FIRE:
			Mars_Sec_M_AnimateFire();
			break;
		case MARS_SECCMD_S_INIT_DMA:
			Mars_Sec_InitSoundDMA(MARS_SYS_COMM6);
			break;
		case MARS_SECCMD_P_SIGHT_CHECKS:
			Mars_Sec_P_CheckSights();
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

	// init thread-local storage
	mars_tls_pri.bank = 6;
	mars_tls_pri.bankpage = 6;
	mars_tls_pri.setbankpage = &Mars_SetBankPage;

	mars_tls_sec.bank = 7;
	mars_tls_sec.bankpage = 7;
	mars_tls_sec.setbankpage = &Mars_SetBankPageSec;

	// find the memory address in ROM that corresponds to bank 6 in case
	// the ROM is configured to use the Sega mapper, otherwise set the address
	// to some value beyond the ROM space
	mars_rom_bsw_start = Mars_ROMSize() > 0x400000 ? 0x02300000 : 0x02400000;

	__asm volatile("mov %0, r0\n\tldc r0,gbr" : : "rm"(&mars_tls_pri) : "r0", "gbr");

	Mars_SetBrightness(1);

	doompalette = W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
	I_SetPalette(doompalette+(10*768));

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

	Mars_CommSlaveClearCache();
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
	uintptr_t newptr = (uintptr_t)ptr;

	if (newptr >= mars_rom_bsw_start && newptr < 0x04000000)
	{
		unsigned page = (newptr - 0x02000000) >> 19;
		volatile unsigned bank, bankpage;
		void (*setbankpage)(int bank, int page);

		I_GetThreadLocalVar(DOOMTLS_BANKPAGE, bank);
		I_GetThreadLocalVar(DOOMTLS_SETBANKPAGEPTR, setbankpage);

		bankpage = bank & 0xffff;
		bank = bank >> 16;

		newptr = ((newptr & 0x0007FFFF) + 512*1024*bank + 0x02000000);
		if (page >= 6)
			newptr |= 0x20000000; // bypass cache

		if (bankpage != page)
		{
			setbankpage(bank, page);

			bank = (bank << 16) | page;
			I_SetThreadLocalVar(DOOMTLS_BANKPAGE, bank);
		}

		return (void *)newptr;
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
 
static char zone[0x33000] __attribute__((aligned(16)));
byte *I_ZoneBase (int *size)
{
	*size = sizeof(zone);
	return (byte *)zone;
}

static int I_ReadControls_(int port, btnstate_t* startbtn)
{
	int ctrl;
	int val;

	val = Mars_ReadController(port);
	if (val < 0)
		return 0;

	ctrl = 0;
	ctrl |= Mars_HandleStartHeld(&val, SEGA_CTRL_START, startbtn);
	ctrl |= Mars_ConvGamepadButtons(val);
	return ctrl;
}

int I_ReadControls(void)
{
	static btnstate_t startbtn = { 0 };
	return I_ReadControls_(0, &startbtn);
}

int I_ReadControls2(void)
{
	static btnstate_t startbtn2 = { 0 };
	return I_ReadControls_(1, &startbtn2);
}

int I_ReadMouse(int* pmx, int *pmy)
{
	int mval, ctrl;
	static int oldmval = 0;
	unsigned val;

	*pmx = *pmy = 0;

	mval = Mars_PollMouse();
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
		mousepresent = true;
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
	return Mars_GetWDTCount();
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

static byte *workbuf_high = NULL;

byte *I_WorkBuffer (void)
{
	while (!I_RefreshCompleted());
	if (workbuf_high == NULL)
		workbuf_high = (byte *)(framebuffer + 320 / 2 * (I_FrameBufferHeight() +1)); // +1 for the blank line
	return workbuf_high;
}

void I_FreeWorkBuffer(void)
{
	workbuf_high = NULL;
}

byte *I_AllocWorkBuffer(int size)
{
	byte *b = I_WorkBuffer();
	workbuf_high = b + size;
	return b;
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
	if ((viewportWidth == 160 && lowResMode) || viewportWidth == 320)
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
		*p++ = 0x1F1F1F1F; // Four bytes of black palette index
}

#ifdef BENCHMARK
static int last = 0;
static VINT numtimes = 0;
int benchcounter = 0;
#endif

void I_DebugScreen(void)
{
	int i;
	int x = 200;
	int line = 3;
	static char buf[19][16];

#ifdef BENCHMARK
	if (benchcounter == 0)
	{
		numtimes = 0;
		last = I_GetFRTCounter();
	}
#endif

	if (debugmode == DEBUGMODE_FPSCOUNT)
	{
		D_snprintf(buf[0], sizeof(buf[0]), "fps:%2d", fpscount);
		I_Print8(x, line++, buf[0]);
	}
	else if (debugmode > DEBUGMODE_FPSCOUNT)
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

		if (debugscreenupdate)
		{
			D_snprintf(buf[0], sizeof(buf[0]), "fps:%2d", fpscount);
			D_snprintf(buf[1], sizeof(buf[0]), "tcs:%d", lasttics);
			D_snprintf(buf[2], sizeof(buf[0]), "g:%2d", Mars_FRTCounter2Msec(tictics));
			D_snprintf(buf[3], sizeof(buf[0]), "b:%2d", Mars_FRTCounter2Msec(t_ref_bsp_avg));
			D_snprintf(buf[4], sizeof(buf[0]), "w:%2d %2d", Mars_FRTCounter2Msec(t_ref_segs_avg), vd.lastwallcmd - vd.viswalls);
			D_snprintf(buf[5], sizeof(buf[0]), "p:%2d %2d", Mars_FRTCounter2Msec(t_ref_planes_avg), vd.lastvisplane - vd.visplanes - 1);
			D_snprintf(buf[6], sizeof(buf[0]), "s:%2d %2d", Mars_FRTCounter2Msec(t_ref_sprites_avg), vd.vissprite_p - vd.vissprites);
			D_snprintf(buf[7], sizeof(buf[0]), "r:%2d", Mars_FRTCounter2Msec(t_ref_total_avg));
			D_snprintf(buf[8], sizeof(buf[0]), "d:%2d", Mars_FRTCounter2Msec(drawtics));
			D_snprintf(buf[9], sizeof(buf[0]), "t:%2d", Mars_FRTCounter2Msec(I_GetFRTCounter() - ticstart));
			D_snprintf(buf[10], sizeof(buf[0]), "mt:%d", thingmem);
			D_snprintf(buf[11], sizeof(buf[0]), "ml:%d", numlines * sizeof(line_t));
			D_snprintf(buf[12], sizeof(buf[0]), "msd:%d", numsides * sizeof(side_t));
			D_snprintf(buf[13], sizeof(buf[0]), "mss:%d", numsubsectors *sizeof(subsector_t));
			D_snprintf(buf[14], sizeof(buf[0]), "ms:%d", numsectors *sizeof(sector_t));
			D_snprintf(buf[15], sizeof(buf[0]), "scenm:%d", numscenerymobjs);
			D_snprintf(buf[16], sizeof(buf[0]), "ringm:%d", numringmobjs);
			D_snprintf(buf[17], sizeof(buf[0]), "statm:%d", numstaticmobjs);
			#ifdef BENCHMARK
			int newTime = I_GetFRTCounter();
			benchcounter += newTime - last;
			last = newTime;
			D_snprintf(buf[18], sizeof(buf[0]), "bch:%d, %d", benchcounter, numtimes++);
			#else
			D_snprintf(buf[18], sizeof(buf[0]), "regm:%d", numregmobjs);
			#endif
		}

        I_Print8(x, line++, buf[0]);
        I_Print8(x, line++, buf[1]);
        line++;
		for (i = 2; i < 19; i++)
	        I_Print8(x, line++, buf[i]);
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
	//int ticcount;
	static int prevsecticcount = 0;
	static VINT framenum = 0;
	const int refreshHZ = Mars_RefreshHZ();

	if (ticsperframe < MINTICSPERFRAME)
		ticsperframe = MINTICSPERFRAME;
	else if (ticsperframe > MAXTICSPERFRAME)
		ticsperframe = MAXTICSPERFRAME;

	if (optionsMenuOn)
		if ((ticrealbuttons & BT_MODE) && !(oldticrealbuttons & BT_MODE))
		{
			int prevdebugmode;

			prevdebugmode = debugmode;
			debugmode = (debugmode + 1) % DEBUGMODE_NUM_MODES;
			debugscreenupdate = true;

			if (prevdebugmode == DEBUGMODE_TEXCACHE 
				|| debugmode == DEBUGMODE_TEXCACHE
				|| prevdebugmode == DEBUGMODE_NOTEXCACHE 
				|| debugmode == DEBUGMODE_NOTEXCACHE)
			{
				R_ClearTexCache(&r_texcache);
			}

			R_SetDrawMode();

			clearscreen = 2;
		}

	/* */
	/* wait until on the third tic after last display */
	/* */
	const int ticwait = (titlescreen ? 4 : ticsperframe); // run title screen at 15 fps

	// Adjust sky position.
	unsigned short scroll_y_base = gamemapinfo.skyOffsetY;
	unsigned short scroll_y_offset = (vd.viewz >> 16);
	unsigned short scroll_y_pan = (vd.aimingangle >> 22);

	if (effects_enabled & EFFECTS_MASK_COPPER) {
		copper_color_index = (copper_vertical_offset
				- scroll_y_base - (scroll_y_offset >> (16-copper_vertical_rate)) - scroll_y_pan) & 511;
	}

#ifdef MDSKY
	if (sky_md_layer) {
		unsigned short scroll_x = (*((unsigned short *)&vd.viewangle) >> 6);

		if (extended_sky) {
			// Use this to scroll the sky 1280 pixels. Works well for 256-width skies.
			scroll_x += (scroll_x >> 2);
		}

		Mars_ScrollMDSky(scroll_x, scroll_y_base, scroll_y_offset, scroll_y_pan);
	}
#endif

	Mars_FlipFrameBuffers(false);
	do
	{
		ticcount = I_GetTime();
	} while (ticcount - lastticcount < ticwait);

	lasttics = ticcount - lastticcount;
	lastticcount = ticcount;
	if (lasttics > 99) lasttics = 99;

	framenum++;
	if (ticcount > prevsecticcount + refreshHZ) {
		fpscount = (framenum * refreshHZ) / (ticcount - prevsecticcount);
		debugscreenupdate = true;
		prevsecticcount = ticcount;
		framenum = 0;
	}

	debugscreenactive = debugmode != DEBUGMODE_NONE;

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

static int Net_SendPacket(int size, uint8_t *data)
{
	int i;

	for (i = 0; i < size; i++)
	{
		int res = Mars_PutNetByte(data[i]);
		if (res < 0)
			return res;
	}

	return 0;
}

static int Net_RecvPacket(int size, uint8_t *data)
{
	int i;

	for (i = 0; i < size; i++)
	{
		int val = Mars_GetNetByte(0);
		if (val == -2)
			break;
		if (val < 0)
			return val;
		data[i] = val;
	}

	return i;
}

#define CONN_MAGIC 0xAA /* 10101010 in binary */

static void Player0Setup (void)
{
	int		val, buttons;
	int 	stage, start;
	uint8_t idbyte[3];
	const int timeout = 300;

	I_Print8(104,5,"Player 0 setup");
	I_Update();

	idbyte[0] = 0xFF;
	idbyte[1] = startmap;
	idbyte[2] = 8*starttype;

	stage = 0;
	start = Mars_GetTicCount();
	do
	{
		int tics;

		/* wait until rec 0xAA byte from other side or player aborts */
		buttons = I_ReadControls();
		if (buttons & BT_START)
		{
			starttype = gt_single;
			return;	/* abort */
		}

		switch (stage)
		{
		case 0:
			val = Mars_GetNetByte(1);
			if (val == CONN_MAGIC)
				stage++;
			break;
		case 1:
			Net_SendPacket(3, idbyte); /* ignore timeout */
			stage++;
			break;
		case 2:
			val = Mars_GetNetByte(1);
			if (val == (CONN_MAGIC ^ 0xFF))
				return;	/* ready to go! */
			break;
		}

		tics = Mars_GetTicCount();
		if (tics - start > timeout)
		{
			/* timeout, restart the process */
			stage = 0;
			start = tics;

			while (Mars_GetNetByte(0) != -2); /* flush network buffer */
			Mars_WaitTicks(Mars_GetTicCount()&7);
		}
	} while (1);
}

static void Player1Setup (void)
{
	uint8_t	idbyte[3];
	int		val, buttons;

	I_Print8(104,5,"Player 1 setup");
	I_Update();

	do
	{
		buttons = I_ReadControls();
		if (buttons & BT_START)
		{
			starttype = gt_single;
			return;	/* abort */
		}

		Mars_PutNetByte(CONN_MAGIC);	/* send an acknowledge byte */

		Mars_WaitTicks(5);

		val = Net_RecvPacket(3, idbyte);
		if (val == 3 && idbyte[0] == 0xFF)
			break;

		Mars_WaitTicks(5);

		while (Mars_GetNetByte(0) != -2); /* flush network buffer */
		Mars_WaitTicks(Mars_GetTicCount()&7);
	} while (1);

	startmap = idbyte[1];
	starttype = idbyte[2] / 8;

	Mars_PutNetByte(CONN_MAGIC ^ 0xFF);	/* send an acknowledge byte */
	Mars_PutNetByte(CONN_MAGIC ^ 0xFF);	/* send another acknowledge byte */
}

void I_NetSetup (void)
{
	while (!I_RefreshCompleted());

	Mars_SetupNet(1); /* 0 = no net, 1 = system link cable, -1 = serial cable */

	Mars_SetNetLinkTimeout(LINK_TIMEOUT_SHORT);

	I_Print8(64,1,"Attempting to connect..."); 
	I_Print8(80,3,"Press start to abort");
//	I_Update();

	while (Mars_GetNetByte(0) != -2) ;  /* flush network buffer */
	Mars_WaitTicks(5);

	if (consoleplayer == 0)
		Player0Setup();
	else
		Player1Setup();

	if (starttype == gt_single)
	{
		I_NetStop();
		return;
	}

	Mars_SetNetLinkTimeout(LINK_TIMEOUT_LONG);

	Mars_WaitTicks(5);

	while (Mars_GetNetByte(0) != -2); /* flush network buffer */
}

void I_NetStop(void)
{
	consoleplayer = 0;
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
		while (!I_RefreshCompleted());
		I_Print8(108,23,"Connecting...");
		I_Update();
		while (Mars_GetNetByte(0) != -2); /* flush network buffer */
		Mars_CleanupNet();
		Mars_WaitTicks(32+(Mars_GetTicCount()&31));
		goto reconnect;
	}

	val = (inbytes[0]<<24) + (inbytes[1]<<16) + (inbytes[2]<<8) + inbytes[3];
	return val;
	
reconnect:
	I_NetSetup();
	G_PlayerReborn(0);
	G_PlayerReborn(1);
	gameaction = starttype == gt_single ? ga_startnew : ga_warped;
	ticbuttons[0] = ticbuttons[1] = oldticbuttons[0] = oldticbuttons[1] = 0;
	return 0;
}
