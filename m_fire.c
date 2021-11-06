/*
  Victor Luchits, Samuel Villarreal and Fabien Sanglard

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

#include "doomdef.h"
#include "mars.h"

// based on work by Samuel Villarreal and Fabien Sanglard

typedef struct {
	unsigned char* firePal;
	int firePalCols;
	char* firePix;
	unsigned char *rndtable;
	int rndindex;
	jagobj_t* titlepic;
	int solid_fire_height;
	int bottom_pos;
} m_fire_t;

#define FIRE_WIDTH		320
#define FIRE_HEIGHT		72

#define FIRE_STOP_TICON 470

const unsigned char fireRGBs[] =
{
		0x00, 0x00, 0x00,
		0x1F, 0x07, 0x07,
		//0x2F, 0x0F, 0x07,
		0x47, 0x0F, 0x07,
		0x57, 0x17, 0x07,
		//0x67, 0x1F, 0x07,
		0x77, 0x1F, 0x07,
		//0x8F, 0x27, 0x07,
		0x9F, 0x2F, 0x07,
		0xAF, 0x3F, 0x07,
		0xBF, 0x47, 0x07,
		//0xC7, 0x47, 0x07,
		0xDF, 0x4F, 0x07,
		0xDF, 0x57, 0x07,
		0xDF, 0x57, 0x07,
		//0xD7, 0x5F, 0x07,
		0xD7, 0x5F, 0x07,
		0xD7, 0x67, 0x0F,
		0xCF, 0x6F, 0x0F,
		//0xCF, 0x77, 0x0F,
		0xCF, 0x7F, 0x0F,
		0xCF, 0x87, 0x17,
		//0xC7, 0x87, 0x17,
		0xC7, 0x8F, 0x17,
		0xC7, 0x97, 0x1F,
		0xBF, 0x9F, 0x1F,
		//0xBF, 0x9F, 0x1F,
		0xBF, 0xA7, 0x27,
		0xBF, 0xA7, 0x27,
		//0xBF, 0xAF, 0x2F,
		0xB7, 0xAF, 0x2F,
		//0xB7, 0xB7, 0x2F,
		0xB7, 0xB7, 0x37,
		//0xCF, 0xCF, 0x6F,
		0xDF, 0xDF, 0x9F,
		0xEF, 0xEF, 0xC7,
		0xFF, 0xFF, 0xFF
};

static m_fire_t *m_fire;

static inline void M_SpreadFire(int src) ATTR_OPTIMIZE_EXTREME;
static inline void M_StopFire(void) ATTR_OPTIMIZE_EXTREME;

static inline int M_FireRand(void)
{
	int idx;
	idx = (m_fire->rndindex + 1) & 0xff;
	m_fire->rndindex = idx;
	return m_fire->rndtable[idx];
}

static inline void M_SpreadFire(int src)
{
	char newval = 0;
	char* firePix = m_fire->firePix;
	char pixel = firePix[src];
	int dst = src;

	if (pixel > 0) {
		int randIdx = M_FireRand();
		dst = src - randIdx + 1;
		newval = pixel - (randIdx & 1);
	}

	dst -= FIRE_WIDTH;
	firePix[dst] = newval;
}

static inline void M_StopFire(void)
{
	int x, y;
	char* firePix = m_fire->firePix;

	for (y = FIRE_HEIGHT - 1; y > FIRE_HEIGHT - 8; y--) {
		int* row = (int *)&firePix[y * FIRE_WIDTH];
		for (x = 0; x < FIRE_WIDTH; x += 4) {
			union {
				int i;
				char b[4];
			} bs;

			bs.i = *row;
			if (bs.i != 0)
			{
				int j;
				for (j = 0; j < 4; j++)
				{
					int c = bs.b[j] - M_FireRand();
					bs.b[j] = c <= 0 ? 0 : c;
				}

				*row = bs.i;
			}

			row++;
		}
	}
}

void Mars_Sec_M_AnimateFire(void)
{
	int start;

	Mars_ClearCache();

	start = I_GetTime();
	while (MARS_SYS_COMM4 == 8)
	{
		int x, y;

		for (x = 0; x < FIRE_WIDTH; x += 4) {
			int from0 = x + FIRE_WIDTH;
			int from1 = x + FIRE_WIDTH + 1;
			int from2 = x + FIRE_WIDTH + 2;
			int from3 = x + FIRE_WIDTH + 3;

			// y = 1
			M_SpreadFire(from0);
			from0 += FIRE_WIDTH;
			M_SpreadFire(from1);
			from1 += FIRE_WIDTH;
			M_SpreadFire(from2);
			from2 += FIRE_WIDTH;
			M_SpreadFire(from3);
			from3 += FIRE_WIDTH;

			for (y = 2; y < FIRE_HEIGHT; y++) {
				M_SpreadFire(from0);
				from0 += FIRE_WIDTH;
				M_SpreadFire(from1);
				from1 += FIRE_WIDTH;
				M_SpreadFire(from2);
				from2 += FIRE_WIDTH;
				M_SpreadFire(from3);
				from3 += FIRE_WIDTH;
			}
		}

		if (I_GetTime() - start > FIRE_STOP_TICON)
		{
			M_StopFire();
		}
	}

	Mars_ClearCache();
}

/* 
================ 
= 
= I_InitMenuFire  
=
================ 
*/ 
void I_InitMenuFire(jagobj_t *titlepic)
{
	int i, j;
	const byte* doompalette;

	doompalette = W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));

	m_fire = Z_Malloc(sizeof(*m_fire), PU_STATIC, NULL);

	m_fire->firePalCols = sizeof(fireRGBs) / 3;
	m_fire->firePal = Z_Malloc(sizeof(*m_fire->firePal) * m_fire->firePalCols, PU_STATIC, NULL);

	m_fire->firePix = Z_Malloc(sizeof(*m_fire->firePix) * FIRE_WIDTH * (FIRE_HEIGHT + 1), PU_STATIC, NULL);
	m_fire->firePix += FIRE_WIDTH;
	D_memset(m_fire->firePix, 0, sizeof(*m_fire->firePix) * FIRE_WIDTH * FIRE_HEIGHT);

	m_fire->rndindex = 0;
	m_fire->rndtable = Z_Malloc(sizeof(*m_fire->rndtable) * 256, PU_STATIC, NULL);

	m_fire->titlepic = titlepic;

	m_fire->solid_fire_height = 24;
	m_fire->bottom_pos = /*I_FrameBufferHeight()*/224 - m_fire->solid_fire_height;

	char* dest = m_fire->firePix + FIRE_WIDTH * (FIRE_HEIGHT - 1);
	for (j = 0; j < FIRE_WIDTH; j++)
		*dest++ = m_fire->firePalCols - 1;

	// find the best matches for fire RGB colors in the palette
	unsigned char* firePal = m_fire->firePal;
	for (i = 0; i < m_fire->firePalCols; i++)
	{
		const byte* pal = doompalette;
		byte ar = fireRGBs[i * 3 + 0], ag = fireRGBs[i * 3 + 1], ab = fireRGBs[i * 3 + 2];
		int best, bestdist;

		best = 0;
		bestdist = 0xFFFFFFF;

		for (j = 0; j < 256; j++) {
			byte r = *pal++, g = *pal++, b = *pal++;
			int dist = (ar - r) * (ar - r) + (ag - g) * (ag - g) + (ab - b) * (ab - b);
			if (dist < bestdist) {
				best = j;
				bestdist = dist;
			}
		}

		firePal[i] = best;
	}

	for (i = 0; i < 256; i++)
		m_fire->rndtable[i] = M_Random() & 3;

	if (titlepic != NULL)
	{
		for (i = 0; i < 2; i++)
		{
			short* lines = Mars_FrameBufferLines();

			DrawJagobj2(titlepic, 0, 0, 0, 0, 0, m_fire->bottom_pos - FIRE_HEIGHT, I_FrameBuffer());

			for (j = 0; j < m_fire->bottom_pos - FIRE_HEIGHT; j++)
				lines[j] = 0 * 320 / 2 + 0x100;

			UpdateBuffer();
		}
	}

	MARS_SYS_COMM4 = 8;
}

/*
================
=
= I_StopMenuFire
=
================
*/
void I_StopMenuFire(void)
{
	MARS_SYS_COMM4 = 9;
	while (MARS_SYS_COMM4 != 0) {}

	Z_Free(m_fire->rndtable);
	Z_Free(m_fire->firePix - FIRE_WIDTH);
	Z_Free(m_fire->firePal);
	Z_Free(m_fire);

	Mars_ClearCache();
}

/*
================
=
= I_DrawMenuFire
=
================
*/
void I_DrawMenuFire(void)
{
	int x, y;
	char* firePix = m_fire->firePix;
	jagobj_t* titlepic = m_fire->titlepic;
	unsigned char* firePal = m_fire->firePal;
	unsigned* row;
	const int pic_startpos = -20;
	const int pic_cutoff = 16;
	const int fh = FIRE_HEIGHT;
	const int solid_fire_height = m_fire->solid_fire_height;
	const int bottom_pos = m_fire->bottom_pos;
	unsigned* dest = (unsigned*)(I_OverwriteBuffer() + 320 / 2 * (bottom_pos + solid_fire_height - FIRE_HEIGHT));

	// scroll the title pic from bottom to top

	// unroll the hidden part as the picture moves
	int pos = (ticon + pic_startpos) / 2;
	if (pos >= fh && pos <= bottom_pos+2)
	{
		int j;
		int limit = pos > bottom_pos ? 0 : bottom_pos - pos;
		short* lines = Mars_FrameBufferLines();
		for (j = limit; j < bottom_pos - fh; j++)
			lines[j] = (j - limit) * 320 / 2 + 0x100;
	}

	if (titlepic != NULL)
	{
		// clear the framebuffer underneath the fire where the title pic is 
		// no longer draw and thus can no longer as serve as the background
		if (ticon >= FIRE_STOP_TICON)
		{
			row = (unsigned*)(I_FrameBuffer() + 320 / 2 * (200 - pic_cutoff));
			for (y = 200 - pic_cutoff; y <= bottom_pos; y++)
			{
				for (x = 0; x < 320 / 4; x += 4)
					*row++ = 0, *row++ = 0, *row++ = 0, *row++ = 0;
			}
		}

		// draw the clipped title pic
		// avoid drawing the part that's hidden by the fire animation at
		// the bottom of the screen. the upper part must mesh together
		// with the part that is being unfolded using the line table
		int y = bottom_pos - (pos < fh ? pos : fh);
		int src_y = pos > fh ? ((pos >= bottom_pos ? bottom_pos : pos) - fh) : 0;
		if (pos >= solid_fire_height)
			DrawJagobj2(titlepic, 0, y, 0, src_y, 0, (pos >= 200 ? 0 : pos) - pic_cutoff, I_FrameBuffer());
	}

	// draw the fire at the bottom
	Mars_ClearCache();

	row = (unsigned*)((intptr_t)firePix);
	for (y = 0; y < FIRE_HEIGHT; y++) {
		for (x = 0; x < FIRE_WIDTH; x += 4) {
			unsigned p = *row;
			unsigned p1 = p & 255; p >>= 8;
			unsigned p2 = p & 255; p >>= 8;
			unsigned p3 = p & 255; p >>= 8;
			unsigned p4 = p & 255; p >>= 8;

			p1 = firePal[p1];
			p2 = firePal[p2];
			p3 = firePal[p3];
			p4 = firePal[p4];
			p = (p4 << 24) | (p3 << 16) | (p2 << 8) | p1;

			*dest++ = p;
			row++;
		}

		if (y == FIRE_HEIGHT - solid_fire_height)
		{
			size_t offs = dest - (unsigned*)I_OverwriteBuffer();
			dest = (unsigned*)I_FrameBuffer() + offs;
		}
	}
}
