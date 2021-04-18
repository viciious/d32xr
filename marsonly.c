/* marsonly.c */

#include "doomdef.h"
#include "r_local.h"
#include "32x.h"

extern const int COLOR_WHITE;

const boolean	debugscreenstate = false;
boolean	debugscreenactive = false;

extern short* dc_colormaps;

pixel_t	*screens[2];	/* [SCREENWIDTH*SCREENHEIGHT];  */
int		*screenshade;	/* pixels for screen shifting */

int             joypad[32]; 
int             joystick1; 

int             samplecount;

int             soundbuffer[EXTERNALQUADS*16];

int		lastticcount = 0;
int		lasttics = 0;

int 	debugmode = 0;

extern int 	cy;
extern int tictics;

void ReadEEProm (void);

void Mars_ClearFrameBuffer(void);
void Mars_WaitFrameBuffersFlip(void);
void Mars_FlipFrameBuffers(boolean wait);
void Mars_Init(void);
void Mars_Slave(void);

void I_ClearWorkBuffer();

/* 
================ 
= 
= Mars_main  
= 
================ 
*/ 
 
int main(void)
{
/* clear screen */
	Mars_Init();

/* init vars  */

	I_Update();

/* */
/* load defaults */
/* */
	ReadEEProm();

/* */
/* start doom */
/* */
	D_DoomMain ();

	return 0;
}

void slave()
{
	Mars_Slave();
}

/*
==============================================================================

						DOOM INTERFACE CODE

==============================================================================
*/

static const byte font8[] =
{
   0,0,0,0,0,0,0,0,24,24,24,24,24,0,24,0,
   54,54,54,0,0,0,0,0,108,108,254,108,254,108,108,0,
   48,124,192,120,12,248,48,0,198,204,24,48,102,198,0,0,
   56,108,56,118,220,204,118,0,24,24,48,0,0,0,0,0,
   6,12,24,24,24,12,6,0,48,24,12,12,12,24,48,0,
   0,102,60,255,60,102,0,0,0,24,24,126,24,24,0,0,
   0,0,0,0,48,48,96,0,0,0,0,0,126,0,0,0,
   0,0,0,0,0,96,96,0,6,12,24,48,96,192,128,0,
   56,108,198,198,198,108,56,0,24,56,24,24,24,24,60,0,
   248,12,12,56,96,192,252,0,248,12,12,56,12,12,248,0,
   28,60,108,204,254,12,12,0,252,192,248,12,12,12,248,0,
   60,96,192,248,204,204,120,0,252,12,24,48,96,192,192,0,
   120,204,204,120,204,204,120,0,120,204,204,124,12,12,120,0,
   0,96,96,0,96,96,0,0,0,96,96,0,96,96,192,0,
   12,24,48,96,48,24,12,0,0,0,252,0,0,252,0,0,
   96,48,24,12,24,48,96,0,124,6,6,28,24,0,24,0,
   124,198,222,222,222,192,120,0,48,120,204,204,252,204,204,0,
   248,204,204,248,204,204,248,0,124,192,192,192,192,192,124,0,
   248,204,204,204,204,204,248,0,252,192,192,248,192,192,252,0,
   252,192,192,248,192,192,192,0,124,192,192,192,220,204,124,0,
   204,204,204,252,204,204,204,0,60,24,24,24,24,24,60,0,
   12,12,12,12,12,12,248,0,198,204,216,240,216,204,198,0,
   192,192,192,192,192,192,252,0,198,238,254,214,198,198,198,0,
   198,230,246,222,206,198,198,0,120,204,204,204,204,204,120,0,
   248,204,204,248,192,192,192,0,120,204,204,204,204,216,108,0,
   248,204,204,248,240,216,204,0,124,192,192,120,12,12,248,0,
   252,48,48,48,48,48,48,0,204,204,204,204,204,204,124,0,
   204,204,204,204,204,120,48,0,198,198,198,214,254,238,198,0,
   198,198,108,56,108,198,198,0,204,204,204,120,48,48,48,0,
   254,12,24,48,96,192,254,0,120,96,96,96,96,96,120,0,
   192,96,48,24,12,6,0,0,120,24,24,24,24,24,120,0,
   24,60,102,66,0,0,0,0,0,0,0,0,0,0,255,0,
   192,192,96,0,0,0,0,0,0,0,120,12,124,204,124,0,
   192,192,248,204,204,204,248,0,0,0,124,192,192,192,124,0,
   12,12,124,204,204,204,124,0,0,0,120,204,252,192,124,0,
   60,96,96,248,96,96,96,0,0,0,124,204,124,12,248,0,
   192,192,248,204,204,204,204,0,24,0,56,24,24,24,60,0,
   24,0,24,24,24,24,240,0,192,192,204,216,240,216,204,0,
   56,24,24,24,24,24,60,0,0,0,236,214,214,214,214,0,
   0,0,248,204,204,204,204,0,0,0,120,204,204,204,120,0,
   0,0,248,204,204,248,192,0,0,0,124,204,204,124,12,0,
   0,0,220,224,192,192,192,0,0,0,124,192,120,12,248,0,
   96,96,252,96,96,96,60,0,0,0,204,204,204,204,124,0,
   0,0,204,204,204,120,48,0,0,0,214,214,214,214,110,0,
   0,0,198,108,56,108,198,0,0,0,204,204,124,12,248,0,
   0,0,252,24,48,96,252,0,28,48,48,224,48,48,28,0,
   24,24,24,0,24,24,24,0,224,48,48,28,48,48,224,0,
   118,220,0,0,0,0,0,0,252,252,252,252,252,252,252,0
};

//
// Print a debug message.
// CALICO: Rewritten to expand 1-bit graphics
//
void I_Print8(int x, int y, const char* string)
{
	int c;
	const byte* source;
	volatile byte *dest;

	if (y > 224 / 8)
		return;

	dest = (byte *)(I_FrameBuffer() + 320) + (y * 8) * 320 + x;

	while ((c = *string++) && x < 320-8)
	{
		int i, b;
		volatile byte * d;

		if (c < 32 || c >= 128)
			continue;

		source = font8 + ((c - 32) << 3);

		d = dest;
		for (i = 0; i < 7; i++)
		{
			byte s = *source++;
			for (b = 0; b < 8; b++)
			{
				if (s & (1 << (7 - b)))
					*(d + b) = COLOR_WHITE;

			}
			d += 320;
		}

		dest+=8;
		++x;
	}
}

/*
================ 
=
= I_Error
=
================
*/

static char errormessage[80];

void I_Error (char *error, ...) 
{
	va_list ap;

	va_start(ap, error);
	D_vsnprintf(errormessage, sizeof(errormessage), error, ap);
	va_end(ap);

	I_ClearWorkBuffer ();
	I_Print8 (0,25,errormessage);
	I_Update ();

	while (1)
	;
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
extern int t_ref_bsp, t_ref_prep, t_ref_segs, t_ref_planes, t_ref_sprites, t_ref_total, t_ref_wait;

void I_Update (void) 
{
	int sec;
	int ticcount;
	char buf[32];
	static int fpscount = 0;
	static int prevsec = 0;

	if (debugmode != 0)
	{
		int line = 5;

		D_snprintf(buf, sizeof(buf), "fps  :%d", fpscount);
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "ticks:%d/%d/%d", tictics, t_ref_total, lasttics);
		I_Print8(200, line++, buf);

		line++;

		D_snprintf(buf, sizeof(buf), "bsp  :%d", t_ref_bsp);
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "segs :%d %02d", t_ref_segs, lastwallcmd - viswalls);
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "plns :%d %02d", t_ref_planes, lastvisplane - visplanes - 1);
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "sprts:%d %02d", t_ref_sprites, lastsprite_p - vissprites);
		I_Print8(200, line++, buf);
	}

	Mars_FlipFrameBuffers(false);

/* */
/* wait until on the third tic after last display */
/* */
	do
	{
		ticcount = I_GetTime();
	} while (ticcount-lastticcount < 2);

	lasttics = ticcount - lastticcount;
	lastticcount = ticcount;

	sec = ticcount / (ticrate == 4 ? 60 : 50); // FIXME: add proper NTSC vs PAL rate detection
	if (sec != prevsec) {
		static int prevsecframe;
		fpscount = (framecount - prevsecframe) / (sec - prevsec);
		prevsec = sec;
		prevsecframe = framecount;
	}

	cy = 1;

	if ((I_ReadControls() & BT_STAR) == BT_STAR)
	{
		static int lastdebugtic = 0;
		if (ticcount > lastdebugtic + 20)
		{
			debugmode = (debugmode + 1) % 3;
			lastdebugtic = ticcount;
		}
	}

	debugscreenactive = debugmode != 0;
}

