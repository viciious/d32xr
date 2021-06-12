/* marsonly.c */

#include "doomdef.h"
#include "r_local.h"
#include "32x.h"

extern const int COLOR_WHITE;

boolean	debugscreenactive = false;

extern short* dc_colormaps;

int		lastticcount = 0;
int		lasttics = 0;

int 	debugmode = 0;

extern int 	cy;
extern int tictics;

void ReadEEProm (void);

void Mars_FlipFrameBuffers(boolean wait);
void Mars_Init(void);
void Mars_Slave(void);

void I_ClearWorkBuffer();
int I_FRTCounter2Msec(int c);

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
extern int t_ref_bsp[4], t_ref_prep[4], t_ref_segs[4], t_ref_planes[4], t_ref_sprites[4], t_ref_total;

void I_Update (void) 
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

		D_snprintf(buf, sizeof(buf), "g:%2d", I_FRTCounter2Msec(tictics));
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "b:%2d", I_FRTCounter2Msec(t_ref_bsp_avg));
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "w:%2d %2d", I_FRTCounter2Msec(t_ref_segs_avg), lastwallcmd - viswalls);
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "p:%2d %2d", I_FRTCounter2Msec(t_ref_planes_avg), lastvisplane - visplanes - 1);
		I_Print8(200, line++, buf);
		D_snprintf(buf, sizeof(buf), "s:%2d %2d", I_FRTCounter2Msec(t_ref_sprites_avg), lastsprite_p - vissprites);
		I_Print8(200, line++, buf);
	}

	Mars_FlipFrameBuffers(false);

/* */
/* wait until on the third tic after last display */
/* */
	do
	{
		ticcount = I_GetTime();
	} while (ticcount-lastticcount < ticwait);

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

void EraseBlock(int x, int y, int width, int height)
{
}

void DrawJagobj2(jagobj_t* jo, int x, int y, int src_x, int src_y, int src_w, int src_h)
{
	int		srcx, srcy;
	int		width, height;
	int		rowsize;

	rowsize = BIGSHORT(jo->width);
	width = BIGSHORT(jo->width);
	height = BIGSHORT(jo->height);

	if (src_w > 0)
		width = src_w;
	else if (src_w < 0)
		width += src_w;

	if (src_h > 0)
		height = src_h;
	else if (src_h < 0)
		height += src_h;

	srcx = 0;
	srcy = 0;

	if (x < 0)
	{
		width += x;
		srcx = -x;
		x = 0;
	}
	srcx += src_x;
	width -= src_x;

	if (y < 0)
	{
		srcy = -y;
		height += y;
		y = 0;
	}

	srcy += src_y;
	height -= src_y;

	if (x + width > 320)
		width = 320 - x;
	if (y + height > 200)
		height = 200 - y;

	if (width < 1 || height < 1)
		return;
	{
		byte* dest;
		byte* source;
		unsigned i;

		source = jo->data + srcx + srcy * rowsize;

		if ((x & 1) == 0 && (width & 1) == 0)
		{
			unsigned hw = (unsigned)width >> 1;
			unsigned hx = (unsigned)x >> 1;
			unsigned hr = (unsigned)rowsize >> 1;

			pixel_t *dest2 = I_OverwriteBuffer() + y * 160 + hx;
			pixel_t *source2 = (pixel_t*)source;

			for (; height; height--)
			{
				for (i = 0; i < hw; i++)
					dest2[i] = source2[i];
				source2 += hr;
				dest2 += 160;
			}
			return;
		}

		dest = (byte*)I_FrameBuffer() + y * 320 + x;
		for (; height; height--)
		{
			for (i = 0; i < width; i++)
				dest[i] = source[i];
			source += rowsize;
			dest += 320;
		}
	}
}

void DrawJagobj(jagobj_t* jo, int x, int y)
{
	DrawJagobj2(jo, x, y, 0, 0, 0, 0);
}
