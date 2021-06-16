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

#ifndef _MARSHW_H
#define _MARSHW_H

#ifdef MARS

#include <stdint.h>
#include "32x.h"

void Mars_FlipFrameBuffers(char wait) __attribute__((noinline));
void Mars_WaitFrameBuffersFlip(void) __attribute__((noinline));
char Mars_FramebuffersFlipped(void) __attribute__((noinline));
void Mars_Init(void);
void Mars_UploadPalette(const uint8_t* palette);
int Mars_PollMouse(int port);
int Mars_ParseMousePacket(int mouse, int* pmx, int* pmy);

extern volatile unsigned short* mars_gamepadport;
extern char mars_mouseport;
extern volatile unsigned mars_vblank_count;
extern unsigned mars_frtc2msec_frac;

#define Mars_GetTicCount() (*(volatile int *)((intptr_t)&mars_vblank_count | 0x20000000))
int Mars_GetFRTCounter(void);

#define Mars_ClearCacheLine(addr) *(volatile int *)((addr) | 0x40000000) = 0
#define Mars_ClearCache() \
	do { \
		CacheControl(0); /* disable cache */ \
		CacheControl(SH2_CCTL_CP | SH2_CCTL_CE); /* purge and re-enable */ \
	} while (0)

#define Mars_ClearCacheLines(paddr,nl) \
	do { \
		intptr_t addr = (intptr_t)paddr; \
		int l; \
		for (l = 0; l < nl; l++) { \
			Mars_ClearCacheLine(addr); \
			addr += 16; \
		} \
	} while (0)

#endif 

#endif // _MARSHW_H
