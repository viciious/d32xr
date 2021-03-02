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

#include "32x.h"

void Mars_ClearFrameBuffer(void);
void Mars_FlipFrameBuffers(boolean wait);
void Mars_WaitFrameBuffersFlip(void);
void Mars_Init(void);
int Mars_ToDoomControls(int ctrl);
void Mars_Slave(void);
void Mars_UploadPalette(const byte* palette);
#define Mars_ClearCacheLine(addr) *(volatile int *)((addr) | 0x40000000) = 0

//static inline void Mars_ClearCacheLines(volatile void* paddr, volatile int lines) __attribute__((section(".data"), aligned(16)));
static inline void Mars_R_BeginComputeSeg(void)/* __attribute__((section(".data"), aligned(16)))*/;
static inline void Mars_R_EndComputeSeg(void)/* __attribute__((section(".data"), aligned(16)))*/;
static inline void Mars_R_BeginPrepWalls()/* __attribute__((section(".data"), aligned(16)))*/;
static inline void Mars_R_EndPrepWalls(void)/* __attribute__((section(".data"), aligned(16)))*/;

void Mars_Slave_R_SegCommands(void)/* __attribute__((section(".data"), aligned(16)))*/;
void Mars_Slave_R_PrepWalls(void)/* __attribute__((section(".data"), aligned(16)))*/;

void Mars_R_SegCommands(void)/* __attribute__((section(".data"), aligned(16)))*/;

//static inline void Mars_ClearCacheLines(volatile void* paddr, volatile int lines)
//{
//	volatile intptr_t addr = (volatile intptr_t)paddr;
//	while (lines--) {
//		Mars_ClearCacheLine(addr);
//		addr += 16;
//	}
//}

#define Mars_ClearCacheLines(paddr,nl) \
	do { \
		intptr_t addr = (intptr_t)paddr; \
		int l; \
		for (l = 0; l < nl; l++) { \
			Mars_ClearCacheLine(addr); \
			addr += 16; \
		} \
	} while (0)

static inline void Mars_R_BeginComputeSeg(void)
{
	while (MARS_SYS_COMM4 != 0) {};

	MARS_SYS_COMM4 = 1;
}

static inline void Mars_R_EndComputeSeg(void)
{
	while (MARS_SYS_COMM4 != 0);
}

static inline void Mars_R_BeginPrepWalls(void)
{
	while (MARS_SYS_COMM4 != 0) {};
	MARS_SYS_COMM4 = 2;
}

static inline void Mars_R_EndPrepWalls(void)
{
	while (MARS_SYS_COMM4 != 0);
}

#endif 

#endif // _MARS_H
