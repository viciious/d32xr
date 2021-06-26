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

short	*dc_colormaps;
const byte	*new_palette = NULL;

boolean	debugscreenactive = false;

int		lastticcount = 0;
int		lasttics = 0;

int 	debugmode = 1;


extern int 	cy;
extern int tictics;

// framebuffer start is after line table AND a single blank line
static volatile pixel_t* framebuffer = &MARS_FRAMEBUFFER + 0x100 + 160;
static volatile pixel_t *framebufferend = &MARS_FRAMEBUFFER + 0x10000;

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
		ctrl |= BT_OPTION; // S -> S
	}
	return ctrl;
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

int Mars_FRTCounter2Msec(int c)
{
	return FixedMul((unsigned)c << FRACBITS, mars_frtc2msec_frac) >> FRACBITS;
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

void I_SetPalette(const byte* palette)
{
	mars_newpalette = palette;
}

void I_DrawSbar (void)
{
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
	unsigned ctrls = mars_controls;
	mars_controls = 0;
	return Mars_ConvGamepadButtons(ctrls);
}

int I_ReadMouse(int* pmx, int *pmy)
{
	int val;
	static int oldval = 0;

	*pmx = *pmy = 0;
	if (!mousepresent)
		return 0;

	val = Mars_PollMouse(mars_mouseport);
	switch (val)
	{
	case -2:
		// timeout - return old buttons and no deltas
		val = oldval & 0x00F70000;
		break;
	case -1:
		// no mouse
		mousepresent = false;
		oldval = 0;
		return 0;
	default:
		oldval = val;
		break;
	}

	val = Mars_ParseMousePacket(val, pmx, pmy);
	return Mars_ConvMouseButtons(val);
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
	return (byte *)(framebuffer + 320 / 2 * 223);
}

pixel_t	*I_FrameBuffer (void)
{
	return (pixel_t *)framebuffer;
}

pixel_t* I_OverwriteBuffer(void)
{
	return (pixel_t*)&MARS_OVERWRITE_IMG + 0x100 + 160;
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
	int* p = (int*)framebuffer;
	int* p_end = (int*)(framebuffer + 320 * 223 / 2);
	while (p < p_end)
		*p++ = 0;
}

void I_DebugScreen(void)
{
	if (debugmode == 3)
		I_ClearFrameBuffer();
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
extern int t_ref_bsp[4], t_ref_prep[4], t_ref_segs[4], t_ref_planes[4], t_ref_sprites[4], t_ref_total;

void I_Update(void)
{
	int i;
	int sec;
	int ticcount;
	char buf[32];
	static int fpscount = 0;
	static int prevsec = 0;
	static int framenum = 0;
	boolean NTSC = (MARS_VDP_DISPMODE & MARS_NTSC_FORMAT) != 0;
	const int ticwait = (demoplayback ? 3 : 2); // demos were recorded at 15-20fps
	const int refreshHZ = (NTSC ? 60 : 50);

	if ((ticbuttons[consoleplayer] & BT_STAR) && !(oldticbuttons[consoleplayer] & BT_STAR))
	{
		extern int clearscreen;
		debugmode = (debugmode + 1) % 4;
		clearscreen = 2;
	}
	debugscreenactive = debugmode != 0;

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

		for (i = 0; i < 4; i++)
		{
			t_ref_bsp_avg += t_ref_bsp[i];
			t_ref_segs_avg += t_ref_segs[i];
			t_ref_planes_avg += t_ref_planes[i];
			t_ref_sprites_avg += t_ref_sprites[i];
		}
		t_ref_bsp_avg >>= 2;
		t_ref_segs_avg >>= 2;
		t_ref_planes_avg >>= 2;
		t_ref_sprites_avg >>= 2;

		D_snprintf(buf, sizeof(buf), "fps:%2d", fpscount);
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "tic:%d/%d", t_ref_total, lasttics);
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
		D_snprintf(buf, sizeof(buf), "s:%2d %2d", Mars_FRTCounter2Msec(t_ref_sprites_avg), lastsprite_p - vissprites);
		I_Print8(200, line++, buf);
	}

	Mars_FlipFrameBuffers(false);

	/* */
	/* wait until on the third tic after last display */
	/* */
	do
	{
		ticcount = I_GetTime();
	} while (ticcount - lastticcount < ticwait);

	lasttics = ticcount - lastticcount;
	lastticcount = ticcount;

	sec = ticcount / refreshHZ; // FIXME: add proper NTSC vs PAL rate detection
	if (sec != prevsec) {
		static int prevsecframe;
		fpscount = (framenum - prevsecframe) / (sec - prevsec);
		prevsec = sec;
		prevsecframe = framenum;
	}
	framenum++;

	cy = 1;
}

void DoubleBufferSetup (void)
{
	int i;

	while (!I_RefreshCompleted());

	for (i = 0; i < 2; i++) {
		I_ClearFrameBuffer();
		Mars_FlipFrameBuffers(true);
	}
}

void UpdateBuffer (void) {
	Mars_FlipFrameBuffers(true);
}

void ReadEEProm (void)
{
	maxlevel = 24;
}

void WriteEEProm (void)
{
	maxlevel = 24;
}

/* =========================================================================== */
/* SERIAL NETWORKING                                                           */
/* =========================================================================== */

#define PACKET_SIZE 6            // size of packet in bytes

int GetSerialChar (void)
{
	unsigned 	status, byte;

	MARS_SYS_COMM0 = 0x0700;			  // Issue command to M68K to attempt to receive a byte.
	while (MARS_SYS_COMM0 != 0);		// Wait for command to complete

	status = (MARS_SYS_COMM2 >> 8) & 0xff;
	byte = (MARS_SYS_COMM2) & 0xff;

	if (status == 0xff) { return -1; } else { return byte; }
}

/* =========================================================================== */

int WaitGetSerialChar (void)
{
	int		status;
	int		vblstop;
	int   ticcount;

	vblstop = (I_GetTime()) + 180;  // Timeout value for waiting for a response from other console	
	do
	{
		ticcount = I_GetTime();
		if (ticcount >= vblstop) { return -1; }		/* timeout */
		status = GetSerialChar();
	} while (status == -1);
	
	return status;
}

/* =========================================================================== */

void PutSerialChar (int data)
{
	MARS_SYS_COMM2 = data;          // Set COMM2 as data to send
	MARS_SYS_COMM0 = 0x0800;	  		// Issue transmit command
	while (MARS_SYS_COMM0 != 0);		// Wait for command to complete
}

/* =========================================================================== */

void wait (int tics)
{
	int		start;
	int   junk;

	start = I_GetTime();
	do { junk = I_GetTime(); } while (junk < start + tics);
}

/* =========================================================================== */

void Player0Setup (void)
{
	int		val;
	int		idbyte;
	int   buttons;
	
	consoleplayer = 0;
	idbyte = startmap + 24*startskill + 128*(starttype==2);
	
	do
	{ /* wait until we see a 0x22 byte from other side */
		buttons = I_ReadControls();
	  if (buttons & JP_OPTION) { starttype = gt_single; return; } /* abort */
		wait (1);
		val = GetSerialChar ();
		if (val == 0x22) { return; } /* ready to go! */
		PutSerialChar (idbyte);
	} while (1);
}

/* =========================================================================== */

void Player1Setup (void)
{	
	int	val, oldval;
	int   buttons;

	oldval = 999;
	consoleplayer = 1;	 /* we are player 1 */

	do
	{ /* wait for two identical id bytes, then start game */
		buttons = I_ReadControls();
	  if (buttons & JP_OPTION) { starttype = gt_single; return; } /* abort */
		val = GetSerialChar();
		if (val == -1) { continue; }
		if (val == oldval) { break; }
		oldval = val;
	} while (1);
	
	if (val > 128) { starttype = 2; val -= 128; } else { starttype = 1; }
	
	startskill = val/24;
	val %= 24;
	startmap = val;

	PutSerialChar(0x22);   /* send an acknowledge byte */
	PutSerialChar(0x22);
}

/* =========================================================================== */

void I_NetSetup (void)
{
	int	listen1, listen2;

	MARS_SYS_COMM0 = 0x0600;			  // Tell M68K to Initialize joypad port 2 in serial mode at 4800 Baud 8-N-1
	while (MARS_SYS_COMM0 != 0);		// Wait for command to complete

	I_Print8 (64,24,"Attempting to connect..."); 
  I_Print8 (80,26,"Press start to abort");
	I_Update ();

	GetSerialChar();
	GetSerialChar();
	wait (1);
	GetSerialChar();
	GetSerialChar();

	wait (4);
	
	listen1 = GetSerialChar(); /* if a character is allready waiting, we are player 1 */
	listen2 = GetSerialChar();
		
	if (listen1 == -1 && listen2 == -1) { Player0Setup(); } else { Player1Setup(); }

	wait (5);
	
	GetSerialChar();  /* flush out serial receive */
	GetSerialChar();
}

/* =========================================================================== */

void G_PlayerReborn (int player);

unsigned I_NetTransfer (unsigned buttons)
{
	int		i, val;
	byte	inbytes[PACKET_SIZE];
	byte	outbytes[PACKET_SIZE];
	byte	consistancy;

	consistancy = players[0].mo->x ^ players[0].mo->y ^ players[0].mo->z ^ players[1].mo->x ^ players[1].mo->y ^ players[1].mo->z;
	consistancy = (consistancy>>8) ^ consistancy ^ (consistancy>>16);

	outbytes[0] = buttons>>24;
	outbytes[1] = buttons>>16;
	outbytes[2] = buttons>>8;
	outbytes[3] = buttons;
	outbytes[4] = consistancy;
	outbytes[5] = vblsinframe;

	if (consoleplayer) /* player 1 waits before sending */
	{
		for (i=0 ; i<=PACKET_SIZE-1 ; i++)
		{
			val = WaitGetSerialChar (); if (val == -1) { goto reconnect; }
			inbytes[i] = val;
			PutSerialChar (outbytes[i]);
		}
		vblsinframe = inbytes[5];		/* take gamevbls from other player */
	}
	else /* player 0 sends first */
	{
		for (i=0 ; i<=PACKET_SIZE-1 ; i++)
		{
			PutSerialChar (outbytes[i]);
			val = WaitGetSerialChar (); if (val == -1) { goto reconnect; }
			inbytes[i] = val;
		}
	}
	
	if (inbytes[4] != outbytes[4]) /* check for consistancy error */
	{
		I_Print8 (108,23,"Network Error"); 
		I_Update ();
		GetSerialChar();  /* flush out serial receive */
		GetSerialChar();
		wait(200);
		goto reconnect;
	}

	val = (inbytes[0]<<24) + (inbytes[1]<<16) + (inbytes[2]<<8) + inbytes[3];
	return val;
	
reconnect:
	if (consoleplayer) { wait(15); }	/* Player 0 waits before proceeding */
	I_NetSetup();
	G_PlayerReborn(0);
	G_PlayerReborn(1);
	gameaction = ga_warped;
	ticbuttons[0] = ticbuttons[1] = oldticbuttons[0] = oldticbuttons[1] = 0;
	return 0;
}
