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

extern short* dc_colormaps;

void I_DrawFuzzColumnLow(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int* pfuzzpos) ATTR_DATA_CACHE_ALIGN;
void I_DrawFuzzColumn(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int* pfuzzpos) ATTR_DATA_CACHE_ALIGN;

#ifdef USE_C_DRAW

void I_DrawColumnLowC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int *fuzzpos) ATTR_DATA_CACHE_ALIGN;
void I_DrawColumnNPo2LowC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int *fuzzpos) ATTR_DATA_CACHE_ALIGN;
void I_DrawSpanLowC(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source) ATTR_DATA_CACHE_ALIGN;

void I_DrawColumnC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int* fuzzpos) ATTR_DATA_CACHE_ALIGN;
void I_DrawColumnNPo2C(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int* fuzzpos) ATTR_DATA_CACHE_ALIGN;
void I_DrawSpanC(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source) ATTR_DATA_CACHE_ALIGN;

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
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int* fuzzpos)
{
	unsigned	heightmask;
	pixel_t* dest;
	short* dc_colormap;
	unsigned	frac;
	unsigned    count, n;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

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
void I_DrawColumnNPo2LowC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int* fuzzpos)
{
	unsigned	heightmask;
	pixel_t* dest;
	short* dc_colormap;
	unsigned    count, n;
	unsigned 	frac;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (debugmode == 3)
		return;
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

/*
================
=
= I_DrawSpan
=
================
*/
void I_DrawSpanLowC(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source)
{
	unsigned xfrac, yfrac;
	pixel_t* dest;
	int		spot;
	unsigned count, n;
	short* dc_colormap;

#ifdef RANGECHECK
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2 >= viewportWidth
		|| (unsigned)ds_y>viewportHeight)
		I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif 
	if (debugmode == 3)
		return;

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

void I_DrawColumnC(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int* fuzzpos)
{
	unsigned	heightmask;
	byte * dest;
	short* dc_colormap;
	unsigned	frac;
	unsigned    count, n;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (debugmode == 3)
		return;
	if (dc_yl > dc_yh)
		return;

	frac = frac_;
	heightmask = dc_texheight - 1;
	dest = (byte *)I_ViewportBuffer() + dc_yl * 320 + dc_x;
	dc_colormap = dc_colormaps + light;

#define DO_PIXEL() do { \
		*dest = dc_colormap[dc_source[(frac >> FRACBITS) & heightmask]] & 0xff; \
		dest += 320; \
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
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int* fuzzpos)
{
	unsigned	heightmask;
	byte * dest;
	short* dc_colormap;
	unsigned    count, n;
	unsigned 	frac;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (debugmode == 3)
		return;
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

	dest = (byte *)I_ViewportBuffer() + dc_yl * 320 + dc_x;
	dc_colormap = dc_colormaps + light;

	count = dc_yh - dc_yl + 1;
	n = (count + 7) >> 3;

#define DO_PIXEL() do { \
		*dest = dc_colormap[dc_source[frac >> FRACBITS]] & 0xff; \
		dest += 320; \
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
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source)
{
	unsigned xfrac, yfrac;
	byte *dest;
	int		spot;
	unsigned count, n;
	short* dc_colormap;

#ifdef RANGECHECK
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2 >= viewportWidth
		|| (unsigned)ds_y>viewportHeight)
		I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif 
	if (debugmode == 3)
		return;

	count = ds_x2 - ds_x1 + 1;
	xfrac = ds_xfrac, yfrac = ds_yfrac;

	dest = (byte*)I_ViewportBuffer() + ds_y * 320 + ds_x1;
	dc_colormap = dc_colormaps + light;

#define DO_PIXEL() do { \
		spot = ((yfrac >> (16 - 6)) & (63 * 64)) + ((xfrac >> 16) & 63); \
		*dest++ = dc_colormap[ds_source[spot]] & 0xff; \
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

void I_DrawFuzzColumnLow(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int* pfuzzpos)
{
	pixel_t* dest;
	short* dc_colormap;
	unsigned	frac;
	unsigned    count, n;
	int	fuzzpos = *pfuzzpos;

	if (!dc_yl)
		dc_yl = 1;
	if (dc_yh == viewportHeight - 1)
		dc_yh = viewportHeight - 2;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (dc_yl > dc_yh)
		return;

	frac = frac_;
	dest = I_ViewportBuffer() + dc_yl * 320 / 2 + dc_x;
	dc_colormap = dc_colormaps + 6 * 256;

#define DO_PIXEL() do { \
		*dest = dc_colormap[dest[fuzzoffset[fuzzpos]] & 0xff]; \
		if (++fuzzpos == FUZZTABLE) fuzzpos = 0; \
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

	*pfuzzpos = fuzzpos;
}

void I_DrawFuzzColumn(int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac_,
	fixed_t fracstep, inpixel_t* dc_source, int dc_texheight, int* pfuzzpos)
{
	byte * dest;
	short* dc_colormap;
	unsigned	frac;
	unsigned    count, n;
	int	fuzzpos = *pfuzzpos;

	if (!dc_yl)
		dc_yl = 1;
	if (dc_yh == viewportHeight - 1)
		dc_yh = viewportHeight - 2;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= viewportWidth || dc_yl < 0 || dc_yh >= viewportHeight)
		I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (debugmode == 3)
		return;
	if (dc_yl > dc_yh)
		return;

	frac = frac_;
	dest = (byte *)I_ViewportBuffer() + dc_yl * 320 + dc_x;
	dc_colormap = dc_colormaps + 6 * 256;

#define DO_PIXEL() do { \
		*dest = dc_colormap[dest[fuzzoffset[fuzzpos]]] & 0xff; \
		if (++fuzzpos == FUZZTABLE) fuzzpos = 0; \
		dest += 320; \
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

	*pfuzzpos = fuzzpos;
}

/*
================
=
= I_DrawSpanPotatoLow
=
================
*/
void I_DrawSpanPotatoLow(int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac,
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source)
{
	pixel_t* dest, pix;
	unsigned count;
	short* dc_colormap;

#ifdef RANGECHECK
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2 >= viewportWidth
		|| (unsigned)ds_y>viewportHeight)
		I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif

	if (debugmode == 3)
		return;
	if (ds_x2 < ds_x1)
		return;

	count = ds_x2 - ds_x1 + 1;

	dest = I_ViewportBuffer() + ds_y * 320 / 2 + ds_x1;
	dc_colormap = dc_colormaps + light;
	pix = dc_colormap[ds_source[513]];

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
	fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t* ds_source)
{
	byte *udest, upix;
	unsigned count, scount;
	short* dc_colormap;

#ifdef RANGECHECK
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2 >= viewportWidth
		|| (unsigned)ds_y>viewportHeight)
		I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif

	if (ds_x2 < ds_x1)
		return;
	if (debugmode == 3)
		return;

	count = ds_x2 - ds_x1 + 1;

	udest = (byte *)I_ViewportBuffer() + ds_y * 320 + ds_x1;
	dc_colormap = dc_colormaps + light;
	upix = dc_colormap[ds_source[513]] & 0xff;

	if (ds_x1 & 1) {
		*udest++ = upix;
		count--;
	}

	scount = count >> 1;
	if (scount > 0)
	{
		pixel_t spix = (upix << 8) | upix;
		pixel_t *sdest = (pixel_t*)udest;

		do {
			*sdest++ = spix;
		} while (--scount > 0);

		udest = (byte*)sdest;
	}

	if (count & 1) {
		*udest = upix;
	}
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
	byte* lump;
	jagobj_t* jo;
	int width, height;

	if (debugmode == 3)
		return;
	if (lumpnum < 0)
		return;

	lump = W_POINTLUMPNUM(lumpnum);
	if (!(lumpinfo[lumpnum].name[0] & 0x80))
	{
		// uncompressed
		DrawJagobj((void*)lump, x, y);
		return;
	}

	lzss_setup(&gfx_lzss, lump);
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
void DrawTiledBackground(void)
{
	int			y, yt;
	const int	w = 64, h = 64;
	const int	hw = w / 2;
	const int xtiles = (320 + w - 1) / w;
	const int ytiles = (200 + h - 1) / h;
	pixel_t* bdest;
	const pixel_t* bsrc;

	if (debugmode == 3)
		return;
	if (gameinfo.borderFlat <= 0)
	{
		I_ClearFrameBuffer();
		return;
	}

	bsrc = (const pixel_t*)W_POINTLUMPNUM(gameinfo.borderFlat);
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


void EraseBlock(int x, int y, int width, int height)
{
}

void DrawJagobj2(jagobj_t* jo, int x, int y, 
	int src_x, int src_y, int src_w, int src_h, pixel_t *fb)
{
	int		srcx, srcy;
	int		width, height;
	int		rowsize;

	if (debugmode == 3)
		return;

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
	if (y + height > mars_framebuffer_height-1)
		height = mars_framebuffer_height-1 - y;

	if (width < 1 || height < 1)
		return;

	{
		byte* dest;
		byte* source;

		source = jo->data + srcx + srcy * rowsize;

		if ((x & 1) == 0 && (width & 1) == 0)
		{
			unsigned hw = (unsigned)width >> 1;
			unsigned hx = (unsigned)x >> 1;
			unsigned hr = (unsigned)rowsize >> 1;

			pixel_t* dest2 = fb + y * 160 + hx;
			pixel_t* source2 = (pixel_t*)source;

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

				source2 += hr - hw;
				dest2 += 160 - hw;
			}
			return;
		}

		dest = (byte*)fb + y * 320 + x;
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
}

void DrawJagobj(jagobj_t* jo, int x, int y)
{
	DrawJagobj2(jo, x, y, 0, 0, 0, 0, I_OverwriteBuffer());
}
