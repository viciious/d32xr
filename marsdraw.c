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
#include "lzss.h"

/*
==============================================================================

						TEXTURE MAPPING CODE

==============================================================================
*/

void I_DrawFuzzColumnLow(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight) ATTR_DATA_CACHE_ALIGN;
void I_DrawFuzzColumn(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight) ATTR_DATA_CACHE_ALIGN;

#ifdef USE_C_DRAW

void I_DrawColumnLowC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight) ATTR_DATA_CACHE_ALIGN;
void I_DrawColumnNPo2LowC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight) ATTR_DATA_CACHE_ALIGN;
void I_DrawSpanLowC(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source, int dc_texheight) ATTR_DATA_CACHE_ALIGN;

void I_DrawColumnC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight) ATTR_DATA_CACHE_ALIGN;
void I_DrawColumnNPo2C(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight) ATTR_DATA_CACHE_ALIGN;
void I_DrawSpanC(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source, int dc_texheight) ATTR_DATA_CACHE_ALIGN;

/*
==================
=
= I_DrawColumn
=
= Source is the top of the column to scale
=
==================
*/

void I_DrawColumnLowC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{
	unsigned	heightmask;
	pixel_t* dest;
	int16_t* dc_colormap;
	unsigned	frac;
	unsigned    count, n;
	int deststep;

#ifdef RANGECHECK
	if (dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (dc_yl > dc_yh)
		return;

	frac = frac_;
	heightmask = dc_texheight - 1;
	dest = viewportbuffer + dc_yl * 320 / 2 + dc_x;
	dc_colormap = (int16_t *)dc_colormaps + light;
	__asm volatile("mov %1,%0\n\t" : "=&r" (deststep) : "r"(320/2));

#define DO_PIXEL() do { \
		*dest = dc_colormap[dc_source[(frac >> FRACBITS) & heightmask]]; \
		dest += deststep; \
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
void I_DrawColumnNPo2LowC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{
	unsigned	heightmask;
	pixel_t* dest;
	int16_t* dc_colormap;
	unsigned    count, n;
	unsigned 	frac;
	int deststep;

#ifdef RANGECHECK
	if (dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (dc_yl > dc_yh)
		return;

	heightmask = dc_texheight << FRACBITS;
	if (frac_ < 0)
		while ((frac_ += heightmask) < 0);
	else
	{
		while ((unsigned)frac_ >= heightmask)
			frac_ -= heightmask;
	}
	frac = frac_;

	dest = viewportbuffer + dc_yl * 320 / 2 + dc_x;
	dc_colormap = (int16_t *)dc_colormaps + light;
	__asm volatile("mov %1,%0\n\t" : "=&r" (deststep) : "r"(320/2));

	count = dc_yh - dc_yl + 1;
	n = (count + 7) >> 3;

#define DO_PIXEL() do { \
		*dest = dc_colormap[dc_source[frac >> FRACBITS]]; \
		dest += deststep; \
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

/*
================
=
= I_DrawSpan
=
================
*/
void I_DrawSpanLowC(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source, int dc_texheight)
{
	unsigned xfrac, yfrac;
	pixel_t* dest;
	int		spot;
	unsigned count, n;
	int16_t* dc_colormap;
	unsigned xmask, ymask;

#ifdef RANGECHECK
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2 >= viewportWidth || ds_y>viewportHeight)
		I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif 

	count = ds_x2 - ds_x1 + 1;
	xfrac = ds_xfrac, yfrac = ds_yfrac;

	xmask = dc_texheight - 1;
	ymask = (dc_texheight-1)*dc_texheight;

	dest = viewportbuffer + ds_y * 320 / 2 + ds_x1;
	dc_colormap = (int16_t *)dc_colormaps + light;

#define DO_PIXEL() do { \
		spot = ((yfrac >> 16) & ymask) + ((xfrac >> 16) & xmask); \
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

void I_DrawColumnC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{
	unsigned	heightmask;
	int8_t *dest;
	int8_t *dc_colormap;
	unsigned	frac;
	unsigned    count, n;
	int deststep;

#ifdef RANGECHECK
	if (dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (dc_yl > dc_yh)
		return;

	frac = frac_;
	heightmask = dc_texheight - 1;
	dest = (int8_t *)viewportbuffer + dc_yl * 320 + dc_x;
	dc_colormap = (int8_t *)(dc_colormaps + light);
	__asm volatile("mov %1,%0\n\t" : "=&r" (deststep) : "r"(320));

#define DO_PIXEL() do { \
		*dest = dc_colormap[(int8_t)dc_source[(frac >> FRACBITS) & heightmask]] & 0xff; \
		dest += deststep; \
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

void I_DrawColumnNPo2C(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{
	unsigned	heightmask;
	int8_t *dest;
	int8_t *dc_colormap;
	unsigned    count, n;
	unsigned 	frac;
	int deststep;

#ifdef RANGECHECK
	if (dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (dc_yl > dc_yh)
		return;

	heightmask = dc_texheight << FRACBITS;
	if (frac_ < 0)
		while ((frac_ += heightmask) < 0);
	else
	{
		while ((unsigned)frac_ >= heightmask)
			frac_ -= heightmask;
	}
	frac = frac_;

	dest = (int8_t *)viewportbuffer + dc_yl * 320 + dc_x;
	dc_colormap = (int8_t *)(dc_colormaps + light);
	__asm volatile("mov %1,%0\n\t" : "=&r" (deststep) : "r"(320));

	count = dc_yh - dc_yl + 1;
	n = (count + 7) >> 3;

#define DO_PIXEL() do { \
		*dest = dc_colormap[(int8_t)dc_source[frac >> FRACBITS]] & 0xff; \
		dest += deststep; \
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

void I_DrawSpanC(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source, int dc_texheight)
{
	unsigned xfrac, yfrac;
	int8_t  *dest;
	int		spot;
	unsigned count, n;
	int8_t* dc_colormap;
	unsigned xmask, ymask;

#ifdef RANGECHECK
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2 >= viewportWidth || ds_y>viewportHeight)
		I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif 

	count = ds_x2 - ds_x1 + 1;
	xfrac = ds_xfrac, yfrac = ds_yfrac;

	xmask = dc_texheight - 1;
	ymask = (dc_texheight-1)*dc_texheight;

	dest = (int8_t *)viewportbuffer + ds_y * 320 + ds_x1;
	dc_colormap = (int8_t *)(dc_colormaps + light);

#define DO_PIXEL() do { \
		spot = ((yfrac >> 16) & ymask) + ((xfrac >> 16) & xmask); \
		*dest++ = dc_colormap[(int8_t)ds_source[spot]] & 0xff; \
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

//
// Spectre/Invisibility.
//

void I_DrawFuzzColumnLowC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{
	int16_t *dest;
	int16_t *dc_colormap;
	unsigned	frac;
	unsigned    count, n;
	int8_t *bfuzzoffset;
	int fuzzpos;
	int deststep;

	I_GetThreadLocalVar(DOOMTLS_FUZZPOS, fuzzpos);
	fuzzpos = fuzzpos * 2;

	if (!dc_yl)
		dc_yl = 1;
	if (dc_yh == viewportHeight - 1)
		dc_yh = viewportHeight - 2;

#ifdef RANGECHECK
	if (dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (dc_yl > dc_yh)
		return;

	frac = frac_;
	dest = (int16_t *)(viewportbuffer + dc_yl * 320 / 2 + dc_x);
	dc_colormap = (int16_t *)dc_colormaps + light;
	bfuzzoffset = (int8_t *)fuzzoffset;
	__asm volatile("mov %1,%0\n\t" : "=&r" (deststep) : "r"(320/2));

#define DO_PIXEL() do { \
		int offset = *(int16_t *)(bfuzzoffset + (fuzzpos & FUZZMASK*2)); \
		*dest = dc_colormap[*((int8_t *)dest + offset)]; \
		fuzzpos += 2; \
		dest += deststep; \
		frac += fracstep; \
	} while (0)

	count = dc_yh - dc_yl + 1;
	n = (count + 3) >> 2;

	switch (count & 3)
	{
	case 0: do { DO_PIXEL();
	case 3:      DO_PIXEL();
	case 2:      DO_PIXEL();
	case 1:      DO_PIXEL();
	} while (--n > 0);
	}

#undef DO_PIXEL

	I_SetThreadLocalVar(DOOMTLS_FUZZPOS, fuzzpos / 2);
}

void I_DrawFuzzColumnC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{
	int8_t * dest;
	int8_t* dc_colormap;
	unsigned	frac;
	unsigned    count, n;
	int8_t *bfuzzoffset;
	int	fuzzpos;
	int deststep;

	I_GetThreadLocalVar(DOOMTLS_FUZZPOS, fuzzpos); 
	fuzzpos = fuzzpos * 2;

	if (!dc_yl)
		dc_yl = 1;
	if (dc_yh == viewportHeight - 1)
		dc_yh = viewportHeight - 2;

#ifdef RANGECHECK
	if (dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (dc_yl > dc_yh)
		return;

	frac = frac_;
	dest = (int8_t *)viewportbuffer + dc_yl * 320 + dc_x;
	dc_colormap = (int8_t *)(dc_colormaps + light);
	bfuzzoffset = (int8_t *)fuzzoffset;
	__asm volatile("mov %1,%0\n\t" : "=&r" (deststep) : "r"(320));

#define DO_PIXEL() do { \
		int offset = *(int16_t *)(bfuzzoffset + (fuzzpos & FUZZMASK*2)); \
		*dest = dc_colormap[dest[offset]]; \
		dest += deststep; \
		frac += fracstep; \
	} while (0)

	count = dc_yh - dc_yl + 1;
	n = (count + 3) >> 2;

	switch (count & 3)
	{
	case 0: do { DO_PIXEL();
	case 3:      DO_PIXEL();
	case 2:      DO_PIXEL();
	case 1:      DO_PIXEL();
	} while (--n > 0);
	}

#undef DO_PIXEL

	I_SetThreadLocalVar(DOOMTLS_FUZZPOS, fuzzpos / 2);
}

/*
================
=
= I_DrawSpanPotatoLow
=
================
*/
void I_DrawSpanPotatoLow(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source, int dc_texheight)
{
	pixel_t* dest, pix;
	unsigned count;
	int8_t *dc_colormap;

#ifdef RANGECHECK
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2 >= viewportWidth || ds_y>viewportHeight)
		I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif

	if (ds_x2 < ds_x1)
		return;

	count = ds_x2 - ds_x1 + 1;

	dest = viewportbuffer + ds_y * 320 / 2 + ds_x1;
	dc_colormap = (int8_t *)dc_colormaps + light;
	pix = (uint8_t)dc_colormap[(int8_t)ds_source[513]];
	pix = ((uint8_t)pix << 8) | pix;

	do {
		*dest++ = pix;
	} while (--count > 0);
}

/*
================
=
= I_DrawSpanPotato
=
================
*/
void I_DrawSpanPotato(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source, int dc_texheight)
{
	int8_t *udest, upix;
	unsigned count, scount;
	int8_t* dc_colormap;

#ifdef RANGECHECK
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2 >= viewportWidth || ds_y>viewportHeight)
		I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif

	if (ds_x2 < ds_x1)
		return;

	I_GetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormap);

	count = ds_x2 - ds_x1 + 1;
	udest = (int8_t *)viewportbuffer + ds_y * 320 + ds_x1;
	dc_colormap = (int8_t *)(dc_colormap + light);
	upix = dc_colormap[(int8_t)ds_source[513]];

	if (ds_x1 & 1) {
		*udest++ = upix;
		count--;
	}

	scount = count >> 1;
	if (scount > 0)
	{
		pixel_t spix = (upix << 8) | (uint8_t)upix;
		pixel_t *sdest = (pixel_t*)udest;

		do {
			*sdest++ = spix;
		} while (--scount > 0);

		udest = (int8_t*)sdest;
	}

	if (count & 1) {
		*udest = upix;
	}
}

void I_DrawColumnNoDraw(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight)
{

}

void I_DrawSpanNoDraw(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source, int dc_texheight)
{

}

/*
=============
=
= DrawJagobjLump
=
=============
*/

void DrawJagobjLump(int lumpnum, int x, int y, int* ow, int* oh)
{
	lzss_state_t gfx_lzss;
	uint8_t lzss_buf[LZSS_BUF_SIZE];
	byte* lump;
	jagobj_t* jo;
	int width, height;

	if (lumpnum < 0)
		return;

	lump = W_POINTLUMPNUM(lumpnum);
	if (!(W_GetNameForNum(lumpnum)[0] & 0x80))
	{
		// uncompressed
		jo = (jagobj_t*)lump;
		if (ow) *ow = BIGSHORT(jo->width);
		if (oh) *oh = BIGSHORT(jo->height);
		DrawJagobj((void*)lump, x, y);
		return;
	}

	lzss_setup(&gfx_lzss, lump, lzss_buf, LZSS_BUF_SIZE);
	if (lzss_read(&gfx_lzss, 16) != 16)
		return;

	jo = (jagobj_t*)gfx_lzss.buf;
	width = BIGSHORT(jo->width);
	height = BIGSHORT(jo->height);

	if (ow) *ow = width;
	if (oh) *oh = height;

	if (x + width > 320)
		width = 320 - x;
	if (y + height > mars_framebuffer_height)
		height = mars_framebuffer_height - y;

	if (width < 1 || height < 1)
		return;
	{
		byte* dest;
		byte* source;
		byte* fb;
		pixel_t* ob;
		unsigned p;

		source = gfx_lzss.buf;
		p = 16;

		fb = (byte*)I_FrameBuffer();
		ob = I_OverwriteBuffer();

		dest = fb + y * 320 + x;
		for (; height; height--)
		{
			int i;

			lzss_read(&gfx_lzss, width);

			i = 0;
			if (p + width > LZSS_BUF_SIZE) {
				int rem = LZSS_BUF_SIZE - p;
				for (; i < rem; i++)
					dest[i] = source[p++];
				p = 0;
			}

			if ((i & 1) == 0 && (width & 1) == 0 && (p & 1) == 0 && (x & 1) == 0)
			{
				int j = i;
				pixel_t* dest2 = ob + ((dest - fb + i) >> 1);
				pixel_t* source2 = (pixel_t*)&source[p];
				for (; i < width; i += 2)
					*dest2++ = *source2++;
				p += width - j;
			}
			else
			{
				for (; i < width; i++)
					dest[i] = source[p++];
			}

			dest += 320;
		}
	}
}

/*
=============
=
= DrawTiledBackground
=
=============
*/
void DrawTiledBackground2(int flat)
{
	int			y, yt;
	const int	w = 64, h = 64;
	const int	hw = w / 2;
	const int xtiles = (320 + w - 1) / w;
	const int ytiles = (224 + h - 1) / h;
	pixel_t* bdest;
	const pixel_t* bsrc;

	if (debugmode == DEBUGMODE_NODRAW)
		return;
	if (flat <= 0)
		return;

	bsrc = (const pixel_t*)W_POINTLUMPNUM(flat);
	bdest = I_FrameBuffer();

	y = 0;
	for (yt = 0; yt < ytiles; yt++)
	{
		int y1;
		const pixel_t* source = bsrc;

		for (y1 = 0; y1 < 64; y1++)
		{
			int xt;

			for (xt = 0; xt < xtiles; xt++) {
				int x;
				for (x = 0; x < hw; x++)
					*bdest++ = source[x];
			}

			y++;
			source += hw;
			if (y == mars_framebuffer_height)
				return;
		}
	}
}

void DrawTiledBackground(void)
{
	if (gameinfo.borderFlatNum <= 0)
	{
		I_ClearFrameBuffer();
		return;
	}
	DrawTiledBackground2(gameinfo.borderFlatNum);
}

void EraseBlock(int x, int y, int width, int height)
{
}

void DrawJagobj2(jagobj_t* jo, int x, int y, 
	int src_x, int src_y, int src_w, int src_h, pixel_t *fb)
{
	int		srcx, srcy;
	int		width, height, depth, flags, index, hw;
	int		rowsize, inc;
	byte	*dest, *source;

	rowsize = BIGSHORT(jo->width);
	width = BIGSHORT(jo->width);
	height = BIGSHORT(jo->height);
	depth = BIGSHORT(jo->depth);
	flags = BIGSHORT(jo->flags);
	index = BIGSHORT(jo->index);

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

	if (y < 0)
	{
		srcy = -y;
		height += y;
		y = 0;
	}
	srcy += src_y;

	if (x + width > 320)
		width = 320 - x;
	if (y + height > mars_framebuffer_height)
		height = mars_framebuffer_height - y;
	inc = rowsize - width;

	if (width < 1 || height < 1)
		return;

	hw = width >> 1;

	if (depth == 2)
	{
		inc >>= 1;
		srcx >>= 1;
		rowsize >>= 1;
		index = (index << 1) + (flags & 2 ? 1 : 0);
	}

	dest = (byte*)fb + y * 320 + x;
	source = jo->data + srcx + srcy * rowsize;

	if (depth == 2)
	{
		if (((intptr_t)dest & 1) == 0 && hw >= 1)
		{
			pixel_t* dest2 = (pixel_t *)dest;
			index = ((unsigned)index << 8) | index;

			for (; height; height--)
			{
				int n = (hw + 3) >> 2;
				switch (hw & 3)
				{
				case 0: do { *dest2++ = index + (((((*source >>  4) & 0xF) << 8) | ((*source >> 0) & 0xF)) << 1), source++;
				case 3:      *dest2++ = index + (((((*source >>  4) & 0xF) << 8) | ((*source >> 0) & 0xF)) << 1), source++;
				case 2:      *dest2++ = index + (((((*source >>  4) & 0xF) << 8) | ((*source >> 0) & 0xF)) << 1), source++;
				case 1:      *dest2++ = index + (((((*source >>  4) & 0xF) << 8) | ((*source >> 0) & 0xF)) << 1), source++;
				} while (--n > 0);
				}

				source += inc;
				dest2 += 160 - hw;
			}

			return;
		}

		for (; height; height--)
		{
			int n = (width + 3) >> 2;
			switch (width & 3)
			{
			case 0: do { *dest++ = index + (((*source >> 4) & 0xF) << 1);
			case 3:      *dest++ = index + (((*source >> 0) & 0xF) << 1), source++;
			case 2:      *dest++ = index + (((*source >> 4) & 0xF) << 1);
			case 1:      *dest++ = index + (((*source >> 0) & 0xF) << 1), source++;
			} while (--n > 0);
			}
			source += inc;
			dest += 320 - width;
		}
		return;
	}

	if ((x & 1) == 0 && (width & 1) == 0)
	{
		pixel_t* dest2 = (pixel_t*)dest, * source2 = (pixel_t*)source;

		inc >>= 1;
		for (; height; height--)
		{
			int n = (hw + 3) >> 2;
			switch (hw & 3)
			{
			case 0: do { *dest2++ = *source2++;
			case 3:      *dest2++ = *source2++;
			case 2:      *dest2++ = *source2++;
			case 1:      *dest2++ = *source2++;
			} while (--n > 0);
			}
			source2 += inc;
			dest2 += 160 - hw;
		}

		return;
	}

	for (; height; height--)
	{
		int n = (width + 3) >> 2;
		switch (width & 3)
		{
		case 0: do { *dest++ = *source++;
		case 3:      *dest++ = *source++;
		case 2:      *dest++ = *source++;
		case 1:      *dest++ = *source++;
		} while (--n > 0);
		}
		source += rowsize - width;
		dest += 320 - width;
	}
}

void DrawJagobj(jagobj_t* jo, int x, int y)
{
	DrawJagobj2(jo, x, y, 0, 0, 0, 0, I_OverwriteBuffer());
}

void DrawFillRect(int x, int y, int w, int h, int c)
{
	int i;

	if (x + w >= 320)
		w = 320 - x;
	if (y + h >= mars_framebuffer_height)
		h = mars_framebuffer_height - y;

	if (w == 0 || h == 0)
		return;

	c = (c << 8) | c;
	int hw = w >> 1;

	pixel_t* dest = I_FrameBuffer() + y * 160 + (x>>1);
	for (i = 0; i < h; i++)
	{
		int n = (hw + 3) >> 2;
		pixel_t* dest2 = dest;

		switch (hw & 3)
		{
		case 0: do { *dest2++ = c;
		case 3:      *dest2++ = c;
		case 2:      *dest2++ = c;
		case 1:      *dest2++ = c;
		} while (--n > 0);
		}

		dest += 160;
	}
}
