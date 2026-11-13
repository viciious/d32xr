/*
  The MIT License (MIT)

  Copyright (c) Chilly Willy

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

#ifndef __32X_H__
#define __32X_H__

#define MARS_SYS_DMA_RV     0x0001
#define MARS_SYS_DMA_68S    0x0004
#define MARS_SYS_DMA_EMPTY  0x4000
#define MARS_SYS_DMA_FULL   0x8000

#define MARS_SH2_ACCESS_VDP 0x8000
#define MARS_68K_ACCESS_VDP 0x0000

#define MARS_PAL_FORMAT     0x0000
#define MARS_NTSC_FORMAT    0x8000

#define MARS_VDP_PRIO_68K   0x0000
#define MARS_VDP_PRIO_32X   0x0080

#define MARS_224_LINES      0x0000
#define MARS_240_LINES      0x0040

#define MARS_VDP_MODE_OFF   0x0000
#define MARS_VDP_MODE_256   0x0001
#define MARS_VDP_MODE_32K   0x0002
#define MARS_VDP_MODE_RLE   0x0003

#define MARS_VDP_VBLK       0x8000
#define MARS_VDP_HBLK       0x4000
#define MARS_VDP_PEN        0x2000
#define MARS_VDP_FEN        0x0002
#define MARS_VDP_FS         0x0001

#define SH2_CCTL_CP         0x10
#define SH2_CCTL_TW         0x08
#define SH2_CCTL_CE         0x01

#define SH2_DMA_CHCR_DM_F     (0<<14)  // Fixed destination address
#define SH2_DMA_CHCR_DM_INC   (1<<14)  // Destination address is incremented 
#define SH2_DMA_CHCR_DM_DEC   (2<<14)  // Destination address is decremented

#define SH2_DMA_CHCR_SM_F     (0<<12)  // Fixed source address
#define SH2_DMA_CHCR_SM_INC   (1<<12)  // Source address is incremented
#define SH2_DMA_CHCR_SM_DEC   (2<<12)  // Source address is decremented

#define SH2_DMA_CHCR_TS_BU    (0<<10)  // Byte unit
#define SH2_DMA_CHCR_TS_WU    (1<<10)  // Word (2-byte) unit
#define SH2_DMA_CHCR_TS_LWU   (2<<10)  // Longword (4-byte) unit
#define SH2_DMA_CHCR_TS_16BU  (3<<10)  // 16-byte unit (4 longword transfers)

#define SH2_DMA_CHCR_AR_MRM   (0<<9)   // Module request mode
#define SH2_DMA_CHCR_AR_ARM   (1<<9)   // Auto-request mode

#define SH2_DMA_CHCR_AM_RC    (0<<8)   // DACK output in read cycle/transfer from memory to device
#define SH2_DMA_CHCR_AM_WC    (1<<8)   // DACK output in write cycle/transfer from device to memory

#define SH2_DMA_CHCR_AL_AL    (0<<7)   // DACK is an active-low signal
#define SH2_DMA_CHCR_AL_AH    (1<<7)   // DACK is an active-high signal

#define SH2_DMA_CHCR_DS_LEEVL (0<<6)  // Detected by level
#define SH2_DMA_CHCR_DS_EDGE  (1<<6)  // Detected by edge

#define SH2_DMA_CHCR_DL_AL    (0<<5)   // When DS is 0, DREQ is detected by low level; when DS is 1, DREQ is detected by fall
#define SH2_DMA_CHCR_DL_AH    (1<<5)   // When DS is 0, DREQ is detected by high level; when DS is 1, DREQ is detected by rise

#define SH2_DMA_CHCR_TB_CS    (0<<4)   // Cycle-steal mode
#define SH2_DMA_CHCR_TB_BM    (1<<4)   // Burst mode

#define SH2_DMA_CHCR_TA_DA    (0<<3)   // Dual address mode
#define SH2_DMA_CHCR_TA_SA    (1<<3)   // Single address mode

#define SH2_DMA_CHCR_IE       (1<<2)   // Interrupt enabled

#define SH2_DMA_CHCR_TE       (1<<1)   // DMA has ended normally (by TCR = 0)

#define SH2_DMA_CHCR_DE       (1<<0)   // DMA transfer enabled

#define SEGA_CTRL_UP        0x0001
#define SEGA_CTRL_DOWN      0x0002
#define SEGA_CTRL_LEFT      0x0004
#define SEGA_CTRL_RIGHT     0x0008
#define SEGA_CTRL_B         0x0010
#define SEGA_CTRL_C         0x0020
#define SEGA_CTRL_A         0x0040
#define SEGA_CTRL_START     0x0080
#define SEGA_CTRL_Z         0x0100
#define SEGA_CTRL_Y         0x0200
#define SEGA_CTRL_X         0x0400
#define SEGA_CTRL_MODE      0x0800

#define SEGA_CTRL_TYPE      0xF000
#define SEGA_CTRL_THREE     0x0000
#define SEGA_CTRL_SIX       0x1000
#define SEGA_CTRL_NONE      0xF000

#define SEGA_CTRL_LMB       0x00010000
#define SEGA_CTRL_RMB       0x00020000
#define SEGA_CTRL_MMB       0x00040000
#define SEGA_CTRL_STARTMB   0x00080000

#define SH2_ClearCacheLine(addr) *(volatile unsigned long *)((uintptr_t)sh2_assoc_purge + (uintptr_t)(addr)) = 0
#define SH2_ClearCache() \
	do { \
		CacheControl(0); /* disable cache */ \
		CacheControl(SH2_CCTL_CP | SH2_CCTL_CE); /* purge and re-enable */ \
	} while (0)

#define SH2_ClearCacheLines(paddr,nl) \
	do { \
		volatile unsigned long *addr = (void *)((uintptr_t)sh2_assoc_purge + (uintptr_t)(paddr)); \
		int l; \
		for (l = 0; l < (int)nl; l++) { \
			*addr = 0; \
			addr += 4; \
		} \
	} while (0)

#define SH2_Delay64() do { __asm volatile \
        ( \
            "mov #1, r0\n\t" \
            "shll8 r0\n\t" \
            "shlr2 r0\n\t" \
            "1:\n\t" \
            "dt r0\n\t" \
            "bf/s 1b\n\t" \
            "nop\n\t" \
            : : : "r0" \
        ); \
    } while(0)

#define SH2_Delay512() do { __asm volatile \
        ( \
            "mov #1, r0\n\t" \
            "shll8 r0\n\t" \
            "1:\n\t" \
            "nop\n\t" \
            "dt r0\n\t" \
            "bf/s 1b\n\t" \
            "nop\n\t" \
            : : : "r0" \
        ); \
    } while(0)

typedef struct {
	unsigned short intmsk;
	unsigned short standby_changer; // Use with system (Boot ROM); application access is prohibited.
	unsigned short hcount;
	unsigned short dmactr;
	unsigned long dmasar;
	unsigned long dmadar;
	unsigned short dmalen;
	unsigned short dmafifo;
	unsigned short vresi_clr;
	unsigned short vint_clr;
	unsigned short hint_clr;
	unsigned short cmdi_clr;
	unsigned short pwmi_clr;
} mars_sys_t;

typedef struct {
	short comm0;
	short comm2;
	short comm4;
	short comm6;
	short comm8;
	short comm10;
	short comm12;
	short comm14;
} mars_sys_comm_t;

typedef struct {
	unsigned short ctrl;
	unsigned short cycle;
	union {
		unsigned short left;
		unsigned short stereo;
	};
	unsigned short right;
	unsigned short mono;
} mars_pwm_t;

typedef struct {
	unsigned short dispmode;
	unsigned short screenshift;
	unsigned short fillen;
	unsigned short filadr;
	unsigned short fildat;
	unsigned short fbctl;
} mars_vdp_t;

typedef struct {
	unsigned char tier;
	unsigned char ftcsr;
	unsigned char frch;
	unsigned char frcl;
	unsigned char ocrh;
	unsigned char ocrl;
	unsigned char tcr;
	unsigned char tocr;
	unsigned char icrh;
	unsigned char icrl;
} sh2_frt_t;

typedef struct {
    // Write by word access. It cannot be written by byte or longword access.
    // Read by byte access. The correct value cannot be read by word or longword access
	union {
		struct {
			unsigned char rtcsr;
			unsigned char rtcnt;
		};
		unsigned short wtcsr_tcnt;
	};

    // Write by word access. It cannot be written by byte or longword access.
    // Read by byte access. The correct value cannot be read by word or longword access
    union {
		struct {
			unsigned char unused;
			unsigned char rrstcsr;
		};
		unsigned short wrwovf_rst;
	};
} sh2_wdt_t;

typedef struct {
	unsigned long sar0;
	unsigned long dar0;
	unsigned long tcr0;
	unsigned long chcr0;
	unsigned long sar1;
	unsigned long dar1;
	unsigned long tcr1;
	unsigned long chcr1;
	unsigned long vcr0;
	unsigned long unused0;
	unsigned long vcr1;
	unsigned long unused1;
	unsigned long dmaor;
} sh2_dma_t;

typedef struct {
	unsigned char drcr0;
	unsigned char drcr1;
} sh2_dma_drcr_t;

typedef struct {
	unsigned short a, b, c, d;
} sh2_vcr_t;

typedef struct {
	long dvsr;
	long dvdnt;
	long dvcr;
    long vcrdiv;
	long dvdnth;
	long dvdntl;
} sh2_divu_t;

static short volatile * const mars_cram =
	(short volatile *)0x20004200;
static short volatile * const mars_framebuffer =
	(short volatile *)0x24000000;
static short volatile * const mars_overwrite_img =
	(short volatile *)0x24020000;
static short volatile * const mars_sdram =
	(short volatile *)0x26000000;

static mars_sys_t volatile * const mars_sys =
	(mars_sys_t *)0x20004000;
static mars_sys_comm_t volatile * const mars_sys_comm =
	(mars_sys_comm_t *)0x20004020;
static mars_pwm_t volatile * const mars_pwm =
	(mars_pwm_t *)0x20004030;
static mars_vdp_t volatile * const mars_vdp =
	(mars_vdp_t *)0x20004100;

static sh2_frt_t volatile * const sh2_frt =
	(sh2_frt_t *)0xFFFFFE10;
static sh2_wdt_t volatile * const sh2_wdt =
	(sh2_wdt_t *)0xFFFFFE80;
static sh2_dma_t volatile * const sh2_dma =
	(sh2_dma_t *)0xFFFFFF80;
static sh2_dma_drcr_t volatile * const sh2_dma_drcr =
	(sh2_dma_drcr_t *)0xFFFFFE71;
static volatile unsigned short * const sh2_int_icr =
	(volatile unsigned short *)0xFFFFFEE0;
static volatile unsigned short * const sh2_int_ipra =
	(volatile unsigned short *)0xFFFFFEE2;
static volatile unsigned short * const sh2_int_vcrwdt =
	(volatile unsigned short *)0xFFFFFEE4;
static volatile unsigned short * const sh2_int_iprb =
	(volatile unsigned short *)0xFFFFFE60;
static sh2_vcr_t volatile * const sh2_vcr =
	(sh2_vcr_t *)0xFFFFFE62;
static sh2_divu_t volatile * const sh2_divu =
	(sh2_divu_t *)0xFFFFFF00;

static volatile unsigned long * const sh2_assoc_purge =
	(volatile unsigned long *)0x40000000; // associative purge area address
static volatile unsigned char * const sh2_direct_rw =
	(volatile unsigned char *)0xC0000000; // cache data array

#ifdef __cplusplus
extern "C" {
#endif


/* global functions in crt0.s */
extern int SetSH2SR(int level);
extern void fast_memcpy(void* dst, const void* src, int len);
extern void CacheControl(int mode);

#ifdef __cplusplus
}
#endif

#endif
