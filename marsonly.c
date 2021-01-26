/* marsonly.c */

#include "doomdef.h"
#include "r_local.h"

extern const int COLOR_WHITE;

const boolean	debugscreenstate = false;
boolean	debugareaactive = false;

extern short* dc_colormaps;

pixel_t	*screens[2];	/* [SCREENWIDTH*SCREENHEIGHT];  */
int		*screenshade;	/* pixels for screen shifting */

int             joypad[32]; 
int             joystick1; 

int             samplecount;

static volatile pixel_t* viewportbuffer;

int             soundbuffer[EXTERNALQUADS*16];

int		lastticcount = 0;
int		lasttics = 0;

static int 	debugmode = 0;

extern int 	cy;

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

	dest = (byte *)(I_FrameBuffer() + 320) + (y * 16) * 320/2 + x;

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

	debugareaactive = true;

	I_ClearWorkBuffer ();
	I_Print8 (0,25,errormessage);
	I_Update ();

	while (1)
	;
} 

/*
==============================================================================

						TEXTURE MAPPING CODE

==============================================================================
*/

/* 
================== 
= 
= I_DrawColumn 
= 
= Source is the top of the column to scale 
= 
================== 
*/ 
 
static void I_DrawColumnPO2 (int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_, 
	fixed_t fracstep, inpixel_t *dc_source, int dc_texheight)
{ 
	int			count, heightmask;
	volatile pixel_t	*dest;
	inpixel_t 		c;
	short 			*dc_colormap;
	unsigned		frac;

	count = dc_yh - dc_yl; 
	if (count <= 0)
		return; 
				 
#ifdef RANGECHECK 
	if ((unsigned)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT) 
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x); 
#endif 
 
	frac = frac_;
	heightmask = dc_texheight - 1;
	dest = viewportbuffer + dc_yl*320/2 + dc_x;
	dc_colormap = dc_colormaps + (((255-light)>>3)&31)*256;

	do
	{ 
		c = dc_source[(frac>>FRACBITS)&heightmask];
		*dest = dc_colormap[c];
		dest += 320/2; 
		frac += fracstep; 
	} while (count--); 
} 


//
// CALICO: the Jag blitter could wrap around textures of arbitrary height, so
// we need to do the "tutti frutti" fix here. Carmack didn't bother fixing
// this for the NeXT "simulator" build of the game.
//
void I_DrawColumnNPO2(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{
	int			count, heightmask;
	volatile pixel_t 	*dest;
	inpixel_t 		c;
	short 			*dc_colormap;
	unsigned 		frac;

	count = dc_yh - dc_yl;
	if (count <= 0)
		return;

#ifdef RANGECHECK 
	if ((unsigned)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif 

	heightmask = dc_texheight << FRACBITS;
	if (frac_ < 0)
		while ((frac_ += heightmask) < 0);
	else
	{
		while (frac_ >= heightmask)
			frac_ -= heightmask;
	}
	frac = frac_;

	dest = viewportbuffer + dc_yl*320/2 + dc_x;
	dc_colormap = dc_colormaps + (((255-light)>>3)&31)*256;

	do
	{
		c = dc_source[frac >> FRACBITS];
		*dest = dc_colormap[c];
		dest += 320/2;
		if ((frac += fracstep) >= heightmask)
			frac -= heightmask;
	} while (count--);
}

void I_DrawColumn(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac, fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{
	if (debugmode == 2)
		return;
	if (dc_texheight & (dc_texheight - 1)) // height is not a power-of-2?
		I_DrawColumnNPO2(dc_x, dc_yl, dc_yh, light, frac, fracstep, dc_source, dc_texheight);
	else
		I_DrawColumnPO2(dc_x, dc_yl, dc_yh, light, frac, fracstep, dc_source, dc_texheight);
}

/* 
================ 
= 
= I_DrawSpan 
= 
================ 
*/ 
 
void I_DrawSpan (int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac, fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t *ds_source) 
{ 
	unsigned		xfrac, yfrac;
	volatile pixel_t	*dest;
	int			count, spot;
	inpixel_t 		c;
	short 			*dc_colormap;

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=SCREENWIDTH  
	|| (unsigned)ds_y>SCREENHEIGHT) 
		I_Error ("R_DrawSpan: %i to %i at %i",ds_x1,ds_x2,ds_y); 
#endif 
	 
	if (debugmode == 2)
		return;

	xfrac = ds_xfrac; 
	yfrac = ds_yfrac; 
	 
	dest = viewportbuffer + ds_y*320/2 + ds_x1;
	count = ds_x2 - ds_x1;
	dc_colormap = dc_colormaps + (((255-light)>>3)&31)*256;

	do 
	{ 
		spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63); 
		c = ds_source[spot];
		*dest++ = dc_colormap[c];
		xfrac += ds_xstep; 
		yfrac += ds_ystep; 
	} while (count--); 
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
extern int t_ref_bsp, ref_prep, t_ref_segs, t_ref_planes, t_ref_sprites, t_ref_total;

void I_Update (void) 
{
	int ticcount;
	char buf[32];

	if (debugmode != 0)
	{
		int zmem = Z_FreeMemory(mainzone);

		D_snprintf(buf, sizeof(buf), "bsp: %d", t_ref_bsp);
		I_Print8(200, 6, buf);
		D_snprintf(buf, sizeof(buf), "segs: %d", t_ref_segs);
		I_Print8(200, 7, buf);
		D_snprintf(buf, sizeof(buf), "planes: %d", t_ref_planes);
		I_Print8(200, 8, buf);
		D_snprintf(buf, sizeof(buf), "sprites: %d", t_ref_sprites);
		I_Print8(200, 9, buf);
		D_snprintf(buf, sizeof(buf), "total: %d", t_ref_total);
		I_Print8(200, 10, buf);
		D_snprintf(buf, sizeof(buf), "zfree: %d.%d", zmem/1024, (zmem - (zmem/1024)*1024)/100);
		I_Print8(200, 11, buf);

	}

	// clear the visible part of the workbuffer
//	if (!debugareaactive)
//		I_ClearWorkBuffer();

	Mars_FlipFrameBuffers(true);

/* */
/* wait until on the third tic after last display */
/* */
	do
	{
		ticcount = I_GetTime();
	} while (ticcount-lastticcount < 3);

	lasttics = ticcount - lastticcount;
	lastticcount = ticcount;

	viewportbuffer = (volatile pixel_t *)I_ViewportBuffer();

	cy = 1;

	if ((I_ReadControls() & (BT_A|BT_C)) == (BT_A|BT_C))
	{
		static int lastdebugtic = 0;
		if (ticcount > lastdebugtic + 20)
		{
			debugmode = (debugmode + 1) % 3;
			lastdebugtic = ticcount;
		}
	}
	if (debugmode == 2)
		I_ClearFrameBuffer();
}

