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

#define MARS_CRAM           (*(volatile unsigned short *)0x20004200)
#define MARS_FRAMEBUFFER    (*(volatile unsigned short *)0x24000000)
#define MARS_OVERWRITE_IMG  (*(volatile unsigned short *)0x24020000)
#define MARS_SDRAM          (*(volatile unsigned short *)0x26000000)

#define MARS_SYS_INTMSK     (*(volatile unsigned short *)0x20004000)
#define MARS_SYS_DMACTR     (*(volatile unsigned short *)0x20004006)
#define MARS_SYS_DMASAR     (*(volatile unsigned long *)0x20004008)
#define MARS_SYS_DMADAR     (*(volatile unsigned long *)0x2000400C)
#define MARS_SYS_DMALEN     (*(volatile unsigned short *)0x20004010)
#define MARS_SYS_DMAFIFO    (*(volatile unsigned short *)0x20004012)
#define MARS_SYS_VRESI_CLR  (*(volatile unsigned short *)0x20004014)
#define MARS_SYS_VINT_CLR   (*(volatile unsigned short *)0x20004016)
#define MARS_SYS_HINT_CLR   (*(volatile unsigned short *)0x20004018)
#define MARS_SYS_CMDI_CLR   (*(volatile unsigned short *)0x2000401A)
#define MARS_SYS_PWMI_CLR   (*(volatile unsigned short *)0x2000401C)
#define MARS_SYS_COMM0      (*(volatile unsigned short *)0x20004020) /* Master SH2 communication */
#define MARS_SYS_COMM2      (*(volatile unsigned short *)0x20004022)
#define MARS_SYS_COMM4      (*(volatile unsigned short *)0x20004024) /* Slave SH2 communication */
#define MARS_SYS_COMM6      (*(volatile unsigned short *)0x20004026)
#define MARS_SYS_COMM8      (*(volatile unsigned short *)0x20004028) /* unused */
#define MARS_SYS_COMM10     (*(volatile unsigned short *)0x2000402A) /* unused */
#define MARS_SYS_COMM12     (*(volatile unsigned short *)0x2000402C) /* unused */
#define MARS_SYS_COMM14     (*(volatile unsigned short *)0x2000402E) /* unused */

#define MARS_SYS_DMA_RV     0x0001
#define MARS_SYS_DMA_68S    0x0004
#define MARS_SYS_DMA_EMPTY  0x4000
#define MARS_SYS_DMA_FULL   0x8000

#define MARS_PWM_CTRL       (*(volatile unsigned short *)0x20004030)
#define MARS_PWM_CYCLE      (*(volatile unsigned short *)0x20004032)
#define MARS_PWM_LEFT       (*(volatile unsigned short *)0x20004034)
#define MARS_PWM_RIGHT      (*(volatile unsigned short *)0x20004036)
#define MARS_PWM_MONO       (*(volatile unsigned short *)0x20004038)
#define MARS_PWM_STEREO     MARS_PWM_LEFT

#define MARS_VDP_DISPMODE   (*(volatile unsigned short *)0x20004100)
#define MARS_VDP_FILLEN     (*(volatile unsigned short *)0x20004104)
#define MARS_VDP_FILADR     (*(volatile unsigned short *)0x20004106)
#define MARS_VDP_FILDAT     (*(volatile unsigned short *)0x20004108)
#define MARS_VDP_FBCTL      (*(volatile unsigned short *)0x2000410A)

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

#define SH2_FRT_TIER        (*(volatile unsigned char *)0xFFFFFE10)
#define SH2_FRT_FTCSR       (*(volatile unsigned char *)0xFFFFFE11)
#define SH2_FRT_FRCH        (*(volatile unsigned char *)0xFFFFFE12)
#define SH2_FRT_FRCL        (*(volatile unsigned char *)0xFFFFFE13)
#define SH2_FRT_OCRH        (*(volatile unsigned char *)0xFFFFFE14)
#define SH2_FRT_OCRL        (*(volatile unsigned char *)0xFFFFFE15)
#define SH2_FRT_TCR         (*(volatile unsigned char *)0xFFFFFE16)
#define SH2_FRT_TOCR        (*(volatile unsigned char *)0xFFFFFE17)
#define SH2_FRT_ICRH        (*(volatile unsigned char *)0xFFFFFE18)
#define SH2_FRT_ICRL        (*(volatile unsigned char *)0xFFFFFE19)

#define SH2_WDT_RTCSR       (*(volatile unsigned char *)0xFFFFFE80)
#define SH2_WDT_RTCNT       (*(volatile unsigned char *)0xFFFFFE81)
#define SH2_WDT_RRSTCSR     (*(volatile unsigned char *)0xFFFFFE83)
#define SH2_WDT_WTCSR_TCNT  (*(volatile unsigned short *)0xFFFFFE80)
#define SH2_WDT_WRWOVF_RST  (*(volatile unsigned short *)0xFFFFFE82)
#define SH2_WDT_VCR         (*(volatile unsigned short *)0xFFFFFEE4)

#define SH2_DMA_SAR0        (*(volatile unsigned long *)0xFFFFFF80)
#define SH2_DMA_DAR0        (*(volatile unsigned long *)0xFFFFFF84)
#define SH2_DMA_TCR0        (*(volatile unsigned long *)0xFFFFFF88)
#define SH2_DMA_CHCR0       (*(volatile unsigned long *)0xFFFFFF8C)
#define SH2_DMA_VCR0        (*(volatile unsigned long *)0xFFFFFFA0)
#define SH2_DMA_DRCR0       (*(volatile unsigned char *)0xFFFFFE71)

#define SH2_DMA_SAR1        (*(volatile unsigned long *)0xFFFFFF90)
#define SH2_DMA_DAR1        (*(volatile unsigned long *)0xFFFFFF94)
#define SH2_DMA_TCR1        (*(volatile unsigned long *)0xFFFFFF98)
#define SH2_DMA_CHCR1       (*(volatile unsigned long *)0xFFFFFF9C)
#define SH2_DMA_VCR1        (*(volatile unsigned long *)0xFFFFFFA8)
#define SH2_DMA_DRCR1       (*(volatile unsigned char *)0xFFFFFE72)

#define SH2_DMA_DMAOR       (*(volatile unsigned long *)0xFFFFFFB0)

#define SH2_INT_ICR         (*(volatile unsigned short *)0xFFFFFEE0)
#define SH2_INT_IPRA        (*(volatile unsigned short *)0xFFFFFEE2)
#define SH2_INT_IPRB        (*(volatile unsigned short *)0xFFFFFE60)
#define SH2_INT_VCRA        (*(volatile unsigned short *)0xFFFFFE62)
#define SH2_INT_VCRB        (*(volatile unsigned short *)0xFFFFFE64)
#define SH2_INT_VCRC        (*(volatile unsigned short *)0xFFFFFE66)
#define SH2_INT_VCRD        (*(volatile unsigned short *)0xFFFFFE68)
#define SH2_INT_VCRWDT      (*(volatile unsigned short *)0xFFFFFEE4)
#define SH2_INT_VCRDIV      (*(volatile unsigned long *)0xFFFFFF0C)

#define SH2_DIVU_DVSR       (*(volatile long *)0xFFFFFF00)
#define SH2_DIVU_DVDNT      (*(volatile long *)0xFFFFFF04)
#define SH2_DIVU_DVDNTH     (*(volatile long *)0xFFFFFF10)
#define SH2_DIVU_DVDNTL     (*(volatile long *)0xFFFFFF14)

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
