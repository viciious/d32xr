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
#include <stddef.h>
#include <stdio.h>
#include "32x.h"

#define MARS_ATTR_DATA_CACHE_ALIGN __attribute__((section(".sdata"), aligned(16), optimize("O1")))

#define MARS_MAX_CONTROLLERS 2

void Mars_FlipFrameBuffers(char wait);
void Mars_WaitFrameBuffersFlip(void);
char Mars_FramebuffersFlipped(void);
void Mars_Init(void);
void Mars_InitVideo(int lines);
void Mars_InitLineTable(void);
void Mars_SetBrightness(int16_t brightness);
int Mars_BackBuffer(void);
void Mars_SetPalette(const uint8_t *palette);
int Mars_PollMouse(void);
int Mars_ParseMousePacket(int mouse, int* pmx, int* pmy);

extern volatile unsigned mars_vblank_count;
extern volatile unsigned mars_pwdt_ovf_count;
extern volatile unsigned mars_swdt_ovf_count;
extern unsigned mars_frtc2msec_frac;
extern uint16_t mars_cd_ok;
extern uint16_t mars_num_cd_tracks;
extern uint16_t mars_framebuffer_height;

extern uint16_t mars_refresh_hz;

void Mars_UpdateCD(void);
void Mars_UseCD(int usecd);

void Mars_PlayTrack(char usecd, int playtrack, const char *name, int offset, int length, char looping);
void Mars_StopTrack(void);
void Mars_SetMusicVolume(uint8_t volume);

#define Mars_GetTicCount() mars_vblank_count
int Mars_GetWDTCount(void);

#define Mars_ClearCacheLine(addr) *(volatile uintptr_t *)(((uintptr_t)addr) | 0x40000000) = 0
#define Mars_ClearCache() \
	do { \
		CacheControl(0); /* disable cache */ \
		CacheControl(SH2_CCTL_CP | SH2_CCTL_CE); /* purge and re-enable */ \
	} while (0)

#define Mars_ClearCacheLines(paddr,nl) \
	do { \
		uintptr_t addr = ((uintptr_t)(paddr)) | 0x40000000; \
		uint32_t l; \
		for (l = 0; l < nl; l++) { \
			*(volatile uintptr_t *)addr = 0; \
			addr += 16; \
		} \
	} while (0)

#endif

uint16_t *Mars_FrameBufferLines(void);

#define Mars_IsPAL() ((MARS_VDP_DISPMODE & MARS_NTSC_FORMAT) == 0)

#define Mars_RefreshHZ() (mars_refresh_hz)

// If you intend to use the two functions below, beware
// that they communicate with the 68000, which in turn
// sets the RV bit to 1. The consequence is that your
// program mustn't attempt reading from ROM, while these
// functions are executing. That includes DMA, code and
// interrupt handlers on both CPUs.

void Mars_ReadSRAM(uint8_t * buffer, int offset, int len) MARS_ATTR_DATA_CACHE_ALIGN;
void Mars_WriteSRAM(const uint8_t * buffer, int offset, int len) MARS_ATTR_DATA_CACHE_ALIGN;

void Mars_WaitTicks(int ticks);

// MD network functions
int Mars_GetNetByte(int wait);
int Mars_PutNetByte(int val);
void Mars_SetupNet(int type);
void Mars_CleanupNet(void);
void Mars_SetNetLinkTimeout(int timeout);

// MD video debug functions
void Mars_SetMDCrsr(int x, int y);
void Mars_GetMDCrsr(int *x, int *y);
void Mars_SetMDColor(int fc, int bc);
void Mars_GetMDColor(int *fc, int *bc);
void Mars_SetMDPal(int cpsel);
void Mars_MDPutChar(char chr);
void Mars_ClearNTA(void);
void Mars_MDPutString(char *str);

void Mars_SetBankPage(int bank, int page) MARS_ATTR_DATA_CACHE_ALIGN;
void Mars_SetBankPageSec(int bank, int page) MARS_ATTR_DATA_CACHE_ALIGN;
int Mars_ReadController(int port);

int Mars_ROMSize(void);

void Mars_CtlMDVDP(int sel);

void Mars_StoreWordColumnInMDVRAM(int c);
// both offset and length are in words, not in bytes
void Mars_LoadWordColumnFromMDVRAM(int c, int offset, int len);
void Mars_SwapWordColumnWithMDVRAM(int c);

int Mars_OpenCDFileByName(const char *name, int *poffset);
void Mars_OpenCDFileByOffset(int length, int offset);
int Mars_ReadCDFile(int length);
int Mars_SeekCDFile(int offset, int whence);
void *Mars_GetCDFileBuffer(void) __attribute__((noinline));

void Mars_Finish(void) MARS_ATTR_DATA_CACHE_ALIGN;

void Mars_MCDLoadSfx(uint16_t id, void *data, uint32_t data_len);
void Mars_MCDPlaySfx(uint8_t src_id, uint16_t buf_id, uint8_t pan, uint8_t vol, uint16_t freq);
int Mars_MCDGetSfxPlaybackStatus(void);
void Mars_MCDClearSfx(void);
void Mars_MCDUpdateSfx(uint8_t src_id, uint8_t pan, uint8_t vol, uint16_t freq);
void Mars_MCDStopSfx(uint8_t src_id);
void Mars_MCDFlushSfx(void);
void Mars_MCDLoadSfxFileOfs(uint16_t start_id, int numsfx, const char *name, int *offsetlen);
int Mars_MCDReadDirectory(const char *path);
void Mars_MCDResumeSPCMTrack(void);
void Mars_MCDOpenTray(void);

// copies bytes from the framebuffer into AUX storage area on the MD
void Mars_StoreAuxBytes(int numbytes);
// copies bytes from AUX storage area on the MD to the framebuffer
void *Mars_LoadAuxBytes(int numbytes);

void Mars_SetPriCmdCallback(void (*cb)(void));
void Mars_SetPriDreqDMACallback(void *(*cb)(void *, void *, int , int ), void *arg);

void Mars_SetSecCmdCallback(void (*cb)(void));
void Mars_SetSecDMA1Callback(void (*cb)(void));

enum {
	DEBUG_FPSCOUNT,
	DEBUG_LASTTICS,
	DEBUG_GAMEMSEC,
	DEBUG_BSPMSEC,
	DEBUG_SEGSMSEC,
	DEBUG_SEGSCOUNT,
	DEBUG_PLANESMSEC,
	DEBUG_PLANESCOUNT,
	DEBUG_SPRITESMSEC,
	DEBUG_SPRITESCOUNT,
	DEBUG_REFMSEC,
	DEBUG_DRAWMSEC,
	DEBUG_TOTALMSEC,
};

#endif // _MARSHW_H
