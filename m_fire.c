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

#define FIRE_STOP_TICON 60

static jagobj_t *intro_titlepic;

void Mars_Sec_M_AnimateFire(void)
{
	int start;

	Mars_ClearCache();

	start = I_GetTime();
	while (MARS_SYS_COMM4 == MARS_SECCMD_M_ANIMATE_FIRE)
	{
		int duration = I_GetTime() - start;
		if (duration > FIRE_STOP_TICON - 20)
		{
			int palIndex = duration - (FIRE_STOP_TICON - 20);
			palIndex /= 4;
			if (palIndex > 5)
				palIndex = 5;

			const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
			I_SetPalette(dc_playpals+palIndex*768);
		}
		else if (duration < 20)
		{
			int palIndex = 10 - (duration / 4);

			const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
			I_SetPalette(dc_playpals+palIndex*768);
		}
		else if (duration < 22)
		{
			const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
			I_SetPalette(dc_playpals);
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
	int i;

	intro_titlepic = titlepic;
	const int titlepic_pos_x = (320 - titlepic->width) / 2;

	if (titlepic != NULL)
	{
		for (i = 0; i < 2; i++)
		{
			DrawJagobj2(titlepic, titlepic_pos_x, 16, 0, 0, titlepic->width, titlepic->height, I_FrameBuffer());
			UpdateBuffer();
		}
	}

	Mars_M_BeginDrawFire();
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
	Mars_M_EndDrawFire();

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
	const int y = 16;
	jagobj_t* titlepic = intro_titlepic;
	const int titlepic_pos_x = (320 - titlepic->width) / 2;

	// scroll the title pic from bottom to top
/*
	if (m_fire->start_song)
	{
		m_fire->start_song = 0;
		S_StartSong(gameinfo.titleMus, 0, cdtrack_title);
	}
*/
	if (titlepic != NULL)
	{
		DrawJagobj2(titlepic, titlepic_pos_x, y, 0, 0, 0, titlepic->height, I_FrameBuffer());
	}

	// draw the fire at the bottom
	Mars_ClearCache();
}
