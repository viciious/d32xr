/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2021 Victor Luchits

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

#ifndef _MARS_H
#define _MARS_H

#ifdef MARS

#include "marshw.h"

enum
{
	MARS_SECCMD_NONE,

	MARS_SECCMD_CLEAR_CACHE,
	MARS_SECCMD_BREAK,

	MARS_SECCMD_R_WALL_PREP_NODRAW,
	MARS_SECCMD_R_WALL_PREP,
	MARS_SECCMD_R_DRAW_PLANES,
	MARS_SECCMD_R_DRAW_SPRITES,

	MARS_SECCMD_M_ANIMATE_FIRE,

	MARS_SECCMD_S_INIT_DMA,

	MARS_SECCMD_AM_DRAW,

	MARS_SECCMD_P_SIGHT_CHECKS,

	MARS_SECCMD_MELT_DO_WIPE,

	MARS_SECCMD_S_INIT_ROQ_DMA,

	MARS_SECCMD_NUMCMDS
};

void Mars_Secondary(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

void Mars_Sec_R_Setup(void);
void Mars_Sec_R_WallPrep(void) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_R_SegCommands(void) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_R_DrawPlanes(void) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_R_PreDrawPlanes(void) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_R_DrawSprites(int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_R_DrawPSprites(int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_P_CheckSights(void) ATTR_DATA_CACHE_ALIGN;
void Mars_Sec_wipe_doMelt(void);

void Mars_Sec_M_AnimateFire(void) ATTR_OPTIMIZE_EXTREME;
void Mars_Sec_InitSoundDMA(int initfull);
void Mars_Sec_ReadSoundCmds(void);

void Mars_Sec_RoQ_InitSound(int init) __attribute__((noinline));
int Mars_PlayRoQ(const char *fn, void *mem, size_t size, int allowpause, void (*secsnd)(int init));

void Mars_Sec_AM_Drawer(void);

#define Mars_R_SecWait() do { while (MARS_SYS_COMM4 != MARS_SECCMD_NONE); } while(0)

static inline void Mars_CommSlaveClearCache(void)
{
	Mars_R_SecWait();
	MARS_SYS_COMM4 = MARS_SECCMD_CLEAR_CACHE;
}

// r_phase2
static inline void Mars_R_BeginWallPrep(boolean draw)
{
	Mars_R_SecWait();
	if (!draw)
		return;
	MARS_SYS_COMM6 = 0; // (next seg)<<8|last unready seg
	MARS_SYS_COMM4 = draw ? MARS_SECCMD_R_WALL_PREP : MARS_SECCMD_R_WALL_PREP_NODRAW;
}

static inline void Mars_R_WallNext(void)
{
	*(volatile uint8_t *)&MARS_SYS_COMM6 = *(volatile uint8_t *)&MARS_SYS_COMM6 + 1;
}

static inline void Mars_R_EndWallPrep(void)
{
	*(volatile int8_t *)&MARS_SYS_COMM6 = -2;
}

// r_phase7
static inline void Mars_R_BeginDrawPlanes(void)
{
	Mars_R_SecWait();
	MARS_SYS_COMM6 = 0; // next visplane
	MARS_SYS_COMM4 = MARS_SECCMD_R_DRAW_PLANES;
}

static inline void Mars_R_EndDrawPlanes(void)
{
}

// r_phase8
static inline void Mars_R_BeginDrawSprites(int sprscreenhalf)
{
	Mars_R_SecWait();
	MARS_SYS_COMM6 = sprscreenhalf;
	MARS_SYS_COMM4 = MARS_SECCMD_R_DRAW_SPRITES;
}

static inline void Mars_R_EndDrawSprites(void)
{
}

static inline void Mars_M_BeginDrawFire(void)
{
	Mars_R_SecWait();
	MARS_SYS_COMM4 = MARS_SECCMD_M_ANIMATE_FIRE;
}

static inline void Mars_M_EndDrawFire(void)
{
	MARS_SYS_COMM4 = MARS_SECCMD_BREAK;
	Mars_R_SecWait();
}

static inline void Mars_InitSoundDMA(int initfull)
{
	Mars_R_SecWait();
	MARS_SYS_COMM6 = initfull;
	MARS_SYS_COMM4 = MARS_SECCMD_S_INIT_DMA;
	Mars_R_SecWait();
}

static inline void Mars_InitRoQSoundDMA(int init)
{
	Mars_R_SecWait();
	MARS_SYS_COMM6 = init;
	MARS_SYS_COMM4 = MARS_SECCMD_S_INIT_ROQ_DMA;
}

static inline void Mars_AM_BeginDrawer(void)
{
	Mars_R_SecWait();
	MARS_SYS_COMM4 = MARS_SECCMD_AM_DRAW;
}

static inline void Mars_AM_EndDrawer(void)
{
	Mars_R_SecWait();
}

static inline void Mars_P_BeginCheckSights(void)
{
	while (MARS_SYS_COMM4 != 0) {};
	MARS_SYS_COMM6 = 0;
	MARS_SYS_COMM4 = MARS_SECCMD_P_SIGHT_CHECKS;
}

static inline void Mars_P_EndCheckSights(void)
{
	while (MARS_SYS_COMM4 != 0);
}

static inline void Mars_melt_BeginWipe(void)
{
	while (MARS_SYS_COMM4 != 0) {};
	MARS_SYS_COMM6 = 0;
	MARS_SYS_COMM4 = MARS_SECCMD_MELT_DO_WIPE;
}

static inline void Mars_melt_EndWipe(void)
{
	while (MARS_SYS_COMM4 != 0);
}

#endif 

#endif // _MARS_H
