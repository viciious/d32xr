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
} m_fire_t;

#define FIRE_WIDTH 320
#define FIRE_HEIGHT 80

const unsigned char fireRGBs[] =
{
		0x00, 0x00, 0x00,
		0x1F, 0x07, 0x07,
		0x2F, 0x0F, 0x07,
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
		0xCF, 0xCF, 0x6F,
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

		if (I_GetTime() - start > 360)
		{
			//M_StopFire();
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
void I_InitMenuFire(void)
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
int I_DrawMenuFire(void)
{
	int x, y;
	pixel_t *dest = I_FrameBuffer() + 320 / 2 * (224 - FIRE_HEIGHT);
	char* firePix = m_fire->firePix;
	unsigned char* firePal = m_fire->firePal;

	// draw the fire at the bottom
	char* row = (char*)((intptr_t)firePix | 0x20000000);
	for (y = 0; y < FIRE_HEIGHT; y++) {
		for (x = 0; x < FIRE_WIDTH; x += 2) {
			int p1 = *row++;
			int p2 = *row++;
			int p;

			p1 = firePal[p1];
			p2 = firePal[p2];
			p = (p1 << 8) | p2;

			*dest++ = p;
		}
	}

	return FIRE_HEIGHT;
}
