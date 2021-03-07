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
} m_fire_t;

#define FIRE_WIDTH 160
#define FIRE_HEIGHT 120

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
		0xC7, 0x47, 0x07,
		0xDF, 0x4F, 0x07,
		//0xDF, 0x57, 0x07,
		0xDF, 0x57, 0x07,
		//0xD7, 0x5F, 0x07,
		0xD7, 0x5F, 0x07,
		0xD7, 0x67, 0x0F,
		0xCF, 0x6F, 0x0F,
		0xCF, 0x77, 0x0F,
		0xCF, 0x7F, 0x0F,
		//0xCF, 0x87, 0x17,
		0xC7, 0x87, 0x17,
		//0xC7, 0x8F, 0x17,
		0xC7, 0x97, 0x1F,
		0xBF, 0x9F, 0x1F,
		0xBF, 0x9F, 0x1F,
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

static inline void M_SpreadFire(char* fire, int src)
{
	char newval = 0;
	char* firePix = m_fire->firePix;
	char pixel = firePix[src];
	int dst = src;

	if (pixel > 0) {
		int randIdx = M_Random() & 3;
		dst = src - randIdx + 1;
		randIdx &= 1;
		newval = pixel - randIdx;
	}

	dst -= FIRE_WIDTH;
	if (dst < 0)
		dst = 0;
	else if (dst >= FIRE_WIDTH * FIRE_HEIGHT)
		dst = FIRE_WIDTH * FIRE_HEIGHT - 1;
	firePix[dst] = newval;
}

void Mars_Slave_M_AnimateFire(void)
{
	int start;
	int lastticcount;
	char* firePix;

	Mars_ClearCache();

	firePix = m_fire->firePix;
	start = I_GetTime();
	lastticcount = start;
	while (MARS_SYS_COMM4 == 8)
	{
		int x, y;
		int ticcount;

		for (x = 0; x < FIRE_WIDTH; x++) {
			int from = x + FIRE_WIDTH;

			// y = 1
			M_SpreadFire(firePix, from);
			from += FIRE_WIDTH;

			// y = 2
			M_SpreadFire(firePix, from);
			from += FIRE_WIDTH;

			// y = 3
			M_SpreadFire(firePix, from);
			from += FIRE_WIDTH;

			for (y = 4; y < FIRE_HEIGHT; y++) {
				M_SpreadFire(firePix, from);
				from += FIRE_WIDTH;
			}
		}

		// Stop fire
		if (I_GetTime() - start > 350)
		{
			for (y = FIRE_HEIGHT - 1; y > FIRE_HEIGHT - 8; y--) {
				if (MARS_SYS_COMM4 != 8) break;

				char* row = &firePix[y * FIRE_WIDTH];
				for (x = 0; x < FIRE_WIDTH; x ++) {
					int c = *row;
					if (c > 0)
					{
						c -= (M_Random() & 3);
						*row = c <= 0 ? 0 : c;
					}
					row++;
				}
			}
		}

		do
		{
			ticcount = I_GetTime();
			if (MARS_SYS_COMM4 != 8) break;
		} while (ticcount - lastticcount < 2);

		lastticcount = ticcount;
	}
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

	m_fire->firePix = Z_Malloc(sizeof(*m_fire->firePix) * FIRE_WIDTH * FIRE_HEIGHT, PU_STATIC, NULL);
	D_memset(m_fire->firePix, 0, sizeof(*m_fire->firePix) * FIRE_WIDTH * FIRE_HEIGHT);

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

	Z_Free(m_fire->firePix);
	Z_Free(m_fire->firePal);
	Z_Free(m_fire);
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
	pixel_t *p = (pixel_t*)I_FrameBuffer();
	pixel_t *dest = p + 320 / 2 * (224 - FIRE_HEIGHT);
	char* firePix = m_fire->firePix;
	unsigned char* firePal = m_fire->firePal;

	// clear the upper part
	while (p < dest)
		*p++ = 0;

	// draw the fire at the bottom
	char* row = (char*)((intptr_t)firePix | 0x20000000);
	for (y = 0; y < FIRE_HEIGHT; y++) {
		for (x = 0; x < FIRE_WIDTH; x++) {
			int p = *row++;
			if (p > 0) {
				p = firePal[p], p = p | (p << 8);
			} else {
				p = 0;
			}
			*dest++ = p;
		}

		dest += (320 / 2 - FIRE_WIDTH);
	}
}
