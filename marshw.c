/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2021 Victor Luchits, Joseph Fenton

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

#include "marshw.h"

static volatile uint16_t mars_activescreen = 0;

static char mars_gamepadport[MARS_MAX_CONTROLLERS];
static char mars_mouseport;
static volatile uint16_t mars_controlval[2];

volatile unsigned mars_vblank_count = 0;
volatile unsigned mars_pwdt_ovf_count = 0;
volatile unsigned mars_swdt_ovf_count = 0;
unsigned mars_frtc2msec_frac = 0;

static const uint8_t* mars_newpalette = NULL;

int16_t mars_requested_lines = 224;
uint16_t mars_framebuffer_height = 224;

uint16_t mars_cd_ok = 0;
uint16_t mars_num_cd_tracks = 0;

uint16_t mars_refresh_hz = 0;

char *pri_dma_dest = NULL;
int pri_dma_read = 0;
void *pri_dma_arg = NULL;

const int NTSC_CLOCK_SPEED = 23011360; // HZ
const int PAL_CLOCK_SPEED = 22801467; // HZ

static volatile int16_t mars_brightness = 0;

void pri_vbi_handler(void) MARS_ATTR_DATA_CACHE_ALIGN;
void pri_cmd_handler(void) MARS_ATTR_DATA_CACHE_ALIGN;
void sec_dma1_handler(void) MARS_ATTR_DATA_CACHE_ALIGN;
void sec_cmd_handler(void) MARS_ATTR_DATA_CACHE_ALIGN;

static void intr_handler_stub(void) MARS_ATTR_DATA_CACHE_ALIGN;
static void intr_handler_stub(void) {}

static void *pri_dreqdma_handler_default(void *cbarg, void *dest, int length, int dmaarg) MARS_ATTR_DATA_CACHE_ALIGN;
static void *pri_dreqdma_handler_default(void *cbarg, void *dest, int length, int dmaarg) {
	dest = pri_dma_dest;
	pri_dma_dest += length;
	pri_dma_read += length;
	return dest;
}

static void (*pri_cmd_cb)(void) = &intr_handler_stub;
static void *(*pri_dreqdma_cb)(void*, void *, int, int) = pri_dreqdma_handler_default;
static void (*sci_cmd_cb)(void) = &intr_handler_stub;
static void (*sci_dma1_cb)(void) = &intr_handler_stub;

static void Mars_HandleDMARequest(void);

static char Mars_UploadPalette(const uint8_t* palette) __attribute__((section(".sdata"), aligned(16), optimize("Os")));

#define MARS_ACTIVE_SCREEN (*(volatile uint16_t *)(((intptr_t)&mars_activescreen) | 0x20000000))

void Mars_WaitFrameBuffersFlip(void)
{
	while ((MARS_VDP_FBCTL & MARS_VDP_FS) != MARS_ACTIVE_SCREEN);
}

void Mars_FlipFrameBuffers(char wait)
{
	MARS_ACTIVE_SCREEN = !MARS_ACTIVE_SCREEN;
	MARS_VDP_FBCTL = MARS_ACTIVE_SCREEN;
	if (wait) Mars_WaitFrameBuffersFlip();
}

char Mars_FramebuffersFlipped(void)
{
	return (MARS_VDP_FBCTL & MARS_VDP_FS) == MARS_ACTIVE_SCREEN;
}

void Mars_InitLineTable(void)
{
	int j;
	int blank;
	int offset = 0; // 224p or 240p
	volatile unsigned short* lines = &MARS_FRAMEBUFFER;

	// initialize the lines section of the framebuffer

	if (mars_requested_lines == -240)
	{
		// letterboxed 240p
		offset = (240 - 224) / 2;
	}

	for (j = 0; j < mars_framebuffer_height; j++)
		lines[offset+j] = j * 320 / 2 + 0x100;

	blank = j * 320 / 2;

	// set the rest of the line table to a blank line
	for (; j < 256; j++)
		lines[offset+j] = blank + 0x100;

	for (j = 0; j < offset; j++)
		lines[j] = blank + 0x100;

	// make sure blank line is clear
	for (j = blank; j < (blank + 160); j++)
		lines[j] = 0;
}

void Mars_SetBrightness(int16_t brightness)
{
	mars_brightness = brightness;
}

void Mars_SetPalette(const uint8_t *palette)
{
	mars_newpalette = palette;
}

int Mars_BackBuffer(void) {
	return MARS_ACTIVE_SCREEN;
}

static char Mars_UploadPalette(const uint8_t* palette)
{
	int	i;
	unsigned short* cram = (unsigned short *)&MARS_CRAM;
	int br = mars_brightness;

	if ((MARS_SYS_INTMSK & MARS_SH2_ACCESS_VDP) == 0)
		return 0;

	for (i = 0; i < 256; i++) {
		int r = br + *palette++;
		int g = br + *palette++;
		int b = br + *palette++;

		if (r > 255) r = 255;
		if (g > 255) g = 255;
		if (b > 255) b = 255;

		unsigned b1 = b;
		unsigned g1 = g;
		unsigned r1 = r;

		b1 = ((b1 >> 3) & 0x1f) << 10;
		g1 = ((g1 >> 3) & 0x1f) << 5;
		r1 = ((r1 >> 3) & 0x1f) << 0;

		cram[i] = 0x8000 | r1 | g1 | b1;
	}

	return 1;
}

int Mars_PollMouse(void)
{
	unsigned int mouse1, mouse2;
	int port = mars_mouseport;

	if (port < 0)
		return -1;

	while (MARS_SYS_COMM0); // wait until 68000 has responded to any earlier requests
	MARS_SYS_COMM0 = 0x0500 | port; // tells 68000 to read mouse
	while (MARS_SYS_COMM0 == (0x0500 | port)); // wait for mouse value

	mouse1 = MARS_SYS_COMM0;
	mouse2 = MARS_SYS_COMM2;
	MARS_SYS_COMM0 = 0; // tells 68000 we got the mouse value

	return (int)((mouse1 << 16) | mouse2);
}

int Mars_ParseMousePacket(int mouse, int* pmx, int* pmy)
{
	int mx, my;

	// (YO XO YS XS S  M  R  L  X7 X6 X5 X4 X3 X2 X1 X0 Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0)

	mx = ((unsigned)mouse >> 8) & 0xFF;
	// check overflow
	if (mouse & 0x00400000)
		mx = (mouse & 0x00100000) ? -256 : 256;
	else if (mouse & 0x00100000)
		mx |= 0xFFFFFF00;

	my = mouse & 0xFF;
	// check overflow
	if (mouse & 0x00800000)
		my = (mouse & 0x00200000) ? -256 : 256;
	else if (mouse & 0x00200000)
		my |= 0xFFFFFF00;

	*pmx = mx;
	*pmy = my;

	return mouse;
}

int Mars_GetWDTCount(void)
{
	unsigned int cnt = SH2_WDT_RTCNT;
	return (int)((mars_pwdt_ovf_count << 8) | cnt);
}

void Mars_InitVideo(int lines)
{
	int i;
	char NTSC;
	int mars_lines = lines == 240 || lines == -240 ? MARS_240_LINES : MARS_224_LINES;

	while ((MARS_SYS_INTMSK & MARS_SH2_ACCESS_VDP) == 0);

	MARS_VDP_DISPMODE = mars_lines | MARS_VDP_MODE_256;
	NTSC = (MARS_VDP_DISPMODE & MARS_NTSC_FORMAT) != 0;

	// change 4096.0f to something else if WDT TCSR is changed!
	mars_frtc2msec_frac = 4096.0f * 1000.0f / (NTSC ? NTSC_CLOCK_SPEED : PAL_CLOCK_SPEED) * 65536.0f;

	mars_refresh_hz = NTSC ? 60 : 50;
	mars_requested_lines = lines;
	mars_framebuffer_height = lines == 240 ? 240 : 224;
	MARS_ACTIVE_SCREEN = MARS_VDP_FBCTL;

	Mars_FlipFrameBuffers(1);

	for (i = 0; i < 2; i++)
	{
		int* p, * p_end;

		Mars_InitLineTable();

		p = (int*)(&MARS_FRAMEBUFFER + 0x100);
		p_end = (int*)p + 320 / 4 * mars_framebuffer_height;
		do {
			*p = 0;
		} while (++p < p_end);

		Mars_FlipFrameBuffers(1);
	}

	Mars_SetMDColor(1, 0);
}

void Mars_Init(void)
{
	int i;

	/* no controllers or mouse by default */
	for (i = 0; i < MARS_MAX_CONTROLLERS; i++)
		mars_gamepadport[i] = -1;
	mars_mouseport = -1;

	// init DMA channel 0 & 1
	SH2_DMA_SAR0 = (int)&MARS_SYS_DMAFIFO;
	SH2_DMA_DAR0 = 0;
	SH2_DMA_TCR0 = 0;
	SH2_DMA_CHCR0 = 0;
	SH2_DMA_DRCR0 = 0;
	SH2_DMA_SAR1 = 0;
	SH2_DMA_DAR1 = 0;
	SH2_DMA_TCR1 = 0;
	SH2_DMA_CHCR1 = 0;
	SH2_DMA_DRCR1 = 0;
	SH2_DMA_DMAOR = 1; // enable DMA transfers on all channels

	SH2_WDT_WTCSR_TCNT = 0xA518; /* WDT TCSR = clr OVF, IT mode, timer off, clksel = Fs/2 */

	/* init hires timer system */
	SH2_WDT_VCR = (65<<8) | (SH2_WDT_VCR & 0x00FF); // set exception vector for WDT
	SH2_INT_IPRA = (SH2_INT_IPRA & 0xFF0F) | 0x0020; // set WDT INT to priority 2

	Mars_UpdateCD();

	if (mars_cd_ok & 0x1)
	{
		/* if the CD is present, give it seconds to init */
		Mars_WaitTicks(180);
	}

	Mars_SetMusicVolume(255);
}

uint16_t* Mars_FrameBufferLines(void)
{
	uint16_t* lines = (uint16_t*)&MARS_FRAMEBUFFER;
	if (mars_requested_lines == -240)
		lines += (240 - 224) / 2;
	return lines;
}

void Mars_ReadSRAM(uint8_t * buffer, int offset, int len)
{
	uint8_t *ptr = buffer;

	while (MARS_SYS_COMM0);
	while (len-- > 0) {
		MARS_SYS_COMM2 = offset++;
		MARS_SYS_COMM0 = 0x0100;    /* Read SRAM */
		while (MARS_SYS_COMM0);
		*ptr++ = MARS_SYS_COMM2 & 0x00FF;
	}
}

void Mars_WriteSRAM(const uint8_t* buffer, int offset, int len)
{
	const uint8_t *ptr = buffer;

	while (MARS_SYS_COMM0);
	while (len-- > 0) {
		MARS_SYS_COMM2 = offset++;
		MARS_SYS_COMM0 = 0x0200 | *ptr++;    /* Write SRAM */
		while (MARS_SYS_COMM0);
	}
}

static char *Mars_StringToFramebuffer(const char *str)
{
	char *fb = (char *)(&MARS_FRAMEBUFFER + 0x100);

	if (!*str)
		return (char *)fb;

	do {
		*fb++ = *str++;
	} while (*str);

	*(int16_t *)((uintptr_t)fb & ~1) = 0;
	*(fb-1) = *(str-1);
	return (char *)fb;
}

void Mars_UpdateCD(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0600;
	while (MARS_SYS_COMM0);
	mars_cd_ok = MARS_SYS_COMM2;
	mars_num_cd_tracks = (mars_cd_ok >> 2) & 63;
	mars_cd_ok = mars_cd_ok & 0x3;
}

void Mars_UseCD(int usecd)
{
	while (MARS_SYS_COMM0);

	if (!mars_cd_ok)
		return;

	MARS_SYS_COMM2 = usecd & 1;
	MARS_SYS_COMM0 = 0x0700;
	while (MARS_SYS_COMM0);
}

void Mars_PlayTrack(char use_cda, int playtrack, const char *name, int offset, int length, char looping)
{
	int len;
	int backup[256];
	char *fb = (char *)(&MARS_FRAMEBUFFER + 0x100), *ptr;
	int send_fn = !use_cda && name ? 0x1 : 0;
	int use_cdf = send_fn && offset < 0 ? 0x2 : 0;

	Mars_UseCD(use_cda);

	if (send_fn)
	{
		ptr = Mars_StringToFramebuffer(name);
		ptr = (void*)(((uintptr_t)ptr + 1 + 3) & ~3);
		len = ptr - fb;
		fast_memcpy(backup, fb, (unsigned)len/4);
	}

	if (!use_cda)
	{
		*(int *)&MARS_SYS_COMM8 = offset;
		*(int *)&MARS_SYS_COMM12 = length;
	}

	MARS_SYS_COMM2 = playtrack | (looping ? 0x8000 : 0x0000);
	MARS_SYS_COMM0 = 0x0300 | use_cdf | send_fn; /* start music */
	while (MARS_SYS_COMM0);

	if (send_fn)
	{
		fast_memcpy(fb, backup, (unsigned)len/4);
	}
}

void Mars_MCDResumeSPCMTrack(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x2C00;
	while (MARS_SYS_COMM0);
}

void Mars_MCDOpenTray(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x2D00;
	while (MARS_SYS_COMM0);
}

void Mars_MCDLoadSfx(uint16_t id, void *data, uint32_t data_len)
{
	int i;
	uint16_t s[4];

	while (MARS_SYS_COMM0);

	s[0] = (uintptr_t)data_len>>16, s[1] = (uintptr_t)data_len&0xffff;
	s[2] = (uintptr_t)data>>16, s[3] = (uintptr_t)data&0xffff;

	for (i = 0; i < 4; i++) {
		MARS_SYS_COMM2 = s[i];
		MARS_SYS_COMM0 = 0x1D01+i;
		while (MARS_SYS_COMM0);
	}

	MARS_SYS_COMM2 = id;
	MARS_SYS_COMM0 = 0x1D00; /* load sfx */
	while (MARS_SYS_COMM0);
}

void Mars_MCDPlaySfx(uint8_t src_id, uint16_t buf_id, uint8_t pan, uint8_t vol, uint16_t freq)
{
	if (src_id == 0)
		return;

	while (MARS_SYS_COMM0);

	MARS_SYS_COMM2 = (pan<<8)|vol;
	MARS_SYS_COMM8 = freq;
	MARS_SYS_COMM10 = buf_id;
	MARS_SYS_COMM0 = 0x1E00|src_id;

	while (MARS_SYS_COMM0);
}

int Mars_MCDGetSfxPlaybackStatus(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x1F00;
	while (MARS_SYS_COMM0);
	return MARS_SYS_COMM2;
}

void Mars_MCDClearSfx(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x2000;
	while (MARS_SYS_COMM0);
}

void Mars_MCDUpdateSfx(uint8_t src_id, uint8_t pan, uint8_t vol, uint16_t freq)
{
	if (src_id == 0)
		return;

	while (MARS_SYS_COMM0);
	MARS_SYS_COMM2 = (pan<<8)|vol;
	MARS_SYS_COMM8 = freq;
	MARS_SYS_COMM0 = 0x2100|src_id;
	while (MARS_SYS_COMM0);
}

void Mars_MCDStopSfx(uint8_t src_id)
{
	if (src_id == 0)
		return;
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x2200|src_id;
	while (MARS_SYS_COMM0);
}

void Mars_MCDFlushSfx(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x2300;
	while (MARS_SYS_COMM0);
}

void Mars_StopTrack(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0400; /* stop music */
	while (MARS_SYS_COMM0);
}

void Mars_SetMusicVolume(uint8_t volume)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x1800|volume;
	while (MARS_SYS_COMM0);
}

void Mars_WaitTicks(int ticks)
{
	unsigned ticend = mars_vblank_count + ticks;
	while (mars_vblank_count < ticend);
}

/*
 *  MD network functions
 */

static inline unsigned short GetNetByte(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x1200;	/* get a byte from the network */
	while (MARS_SYS_COMM0);
	return MARS_SYS_COMM2;		/* status:byte */
}

/*
 *  Get a byte from the network. The number of ticks to wait for a byte
 *  is passed in. A wait time of 0 means return immediately. The return
 *  value is -2 for a timeout/no bytes are waiting, -1 if a network error
 *  occurred, and 0 to 255 for a received byte.
 */
int Mars_GetNetByte(int wait)
{
	unsigned short ret;
	unsigned ticend;

	if (!wait)
	{
		/* no wait - return a value immediately */
		ret = GetNetByte();
		return (ret == 0xFF00) ? -2 : (ret & 0xFF00) ? -1 : (int)(ret & 0x00FF);
	}

	/* quick check for byte in rec buffer */
	ret = GetNetByte();
	if (ret != 0xFF00)
		return (ret & 0xFF00) ? -1 : (int)(ret & 0x00FF);

	/* nothing waiting - do timeout loop */
	ticend = mars_vblank_count + wait;
	while (mars_vblank_count < ticend)
	{
		ret = GetNetByte();
		if (ret == 0xFF00)
			continue;	/* no bytes waiting */
		/* GetNetByte returned a byte or a net error */
		return (ret & 0xFF00) ? -1 : (int)(ret & 0x00FF);
	}
	return -2;	/* timeout */
}

/*
 *  Put a byte to the network. Returns -1 if timeout.
 */
int Mars_PutNetByte(int val)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x1300 | (val & 0x00FF);	/* send a byte to the network */
	while (MARS_SYS_COMM0);
	return (MARS_SYS_COMM2 == 0xFFFF) ? -1 : 0;
}

void Mars_SetupNet(int type)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x1400 | (type & 255);		/* init joyport 2 for networking */
	while (MARS_SYS_COMM0);
}

void Mars_CleanupNet(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x1500;		/* cleanup networking */
	while (MARS_SYS_COMM0);
}

void Mars_SetNetLinkTimeout(int timeout)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM2 = timeout;
	MARS_SYS_COMM0 = 0x1700;
	while (MARS_SYS_COMM0);
}


/*
 *  MD video debug functions
 */
void Mars_SetMDCrsr(int x, int y)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM2 = (x<<6)|y;
	MARS_SYS_COMM0 = 0x0800;			/* set current md cursor */
}

void Mars_GetMDCrsr(int *x, int *y)
{
	unsigned t;
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0900;			/* get current md cursor */
	while (MARS_SYS_COMM0);
	t = MARS_SYS_COMM2;
	*y = t & 31;
	*x = t >> 6;
}

void Mars_SetMDColor(int fc, int bc)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0A00 | (bc << 4) | fc;			/* set font fg and bg colors */
}

void Mars_GetMDColor(int *fc, int *bc)
{
	while (MARS_SYS_COMM0);
	for (MARS_SYS_COMM0 = 0x0B00; MARS_SYS_COMM0;);		/* get font fg and bg colors */
	*fc = (unsigned)(MARS_SYS_COMM2 >> 0) & 15;
	*bc = (unsigned)(MARS_SYS_COMM2 >> 4) & 15;
}

void Mars_SetMDPal(int cpsel)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0C00 | cpsel;	/* set palette select */
}

void Mars_MDPutChar(char chr)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0D00 | chr;		/* put char at current cursor pos */
}

void Mars_ClearNTA(void)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x0E00;			/* clear name table a */
}

void Mars_MDPutString(char *str)
{
	while (*str)
		Mars_MDPutChar(*str++);
}

void Mars_SetBankPage(int bank, int page)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x1600 | (page<<3) | bank;
	while (MARS_SYS_COMM0);
}

void Mars_SetBankPageSec(int bank, int page)
{
	volatile unsigned short bcomm4 = MARS_SYS_COMM4;

	MARS_SYS_COMM4 = 0x1600 | (page<<3) | bank;
	while (MARS_SYS_COMM4 != 0x1000);

	MARS_SYS_COMM4 = bcomm4;
}

int Mars_ROMSize(void)
{
	return *((volatile uint32_t *)0x020001a4) - *((volatile uint32_t *)0x020001a0) + 1;
}

void Mars_DetectInputDevices(void)
{
	unsigned i;
	unsigned ctrl_wait = 0xFF00;

	mars_mouseport = -1;
	for (i = 0; i < MARS_MAX_CONTROLLERS; i++)
		mars_gamepadport[i] = -1;

	for (i = 0; i < MARS_MAX_CONTROLLERS; i++)
	{
		/* wait on COMM0 */
		while (MARS_SYS_COMM0 != ctrl_wait);

		int val = MARS_SYS_COMM2;
		if (val == 0xF000)
		{
			mars_controlval[i] = 0;
		}
		else if (val == 0xF001)
		{
			mars_mouseport = i;
			mars_controlval[i] = 0;
		}
		else
		{
			mars_gamepadport[i] = i;
			mars_controlval[i] |= val;
		}

		MARS_SYS_COMM0 = ++ctrl_wait;
		++ctrl_wait;
	}

	/* swap controller 1 and 2 around if the former isn't present */
	if (mars_gamepadport[0] < 0 && mars_gamepadport[1] >= 0)
	{
		mars_gamepadport[0] = mars_gamepadport[1];
		mars_gamepadport[1] = -1;
	}
}

int Mars_ReadController(int ctrl)
{
	int val;
	int port;

	if (ctrl < 0 || ctrl >= MARS_MAX_CONTROLLERS)
		return -1;

	port = mars_gamepadport[ctrl];
	if (port < 0)
		return -1;

	val = mars_controlval[port];
	mars_controlval[port] = 0;
	return val;
}

void Mars_CtlMDVDP(int sel)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x1900 | (sel & 0x00FF);
	while (MARS_SYS_COMM0);
}

void Mars_StoreWordColumnInMDVRAM(int c)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM0 = 0x1A00|c;		/* sel = to VRAM, column in LB of comm0, start move */
}

void Mars_LoadWordColumnFromMDVRAM(int c, int offset, int len)
{
	while (MARS_SYS_COMM0 != 0);
	MARS_SYS_COMM2 = (((uint16_t)len)<<8) | offset;  /* (length<<8)|offset */
	MARS_SYS_COMM0 = 0x1B00|c;		/* sel = to VRAM, column in LB of comm0, start move */
}

void Mars_SwapWordColumnWithMDVRAM(int c)
{
    while (MARS_SYS_COMM0);
    MARS_SYS_COMM0 = 0x1C00|c;        /* sel = swap with VRAM, column in LB of comm0, start move */
}

int Mars_OpenCDFileByName(const char *name, int *poffset)
{
	int len;
	int backup[256];
	char *fb = (char *)(&MARS_FRAMEBUFFER + 0x100), *ptr;

	if (!*name) {
		return -1;
	}

    while (MARS_SYS_COMM0) {}

	ptr = Mars_StringToFramebuffer(name);
	ptr = (void*)(((uintptr_t)ptr + 1 + 3) & ~3);
	len = ptr - fb;
	fast_memcpy(backup, fb, (unsigned)len/4);

    MARS_SYS_COMM0 = 0x2600;

    while (MARS_SYS_COMM0) {}

	fast_memcpy(fb, backup, (unsigned)len/4);

	if (poffset)
		*poffset = *(int *)&MARS_SYS_COMM12;
	return *(int *)&MARS_SYS_COMM8;
}

void Mars_OpenCDFileByOffset(int length, int offset)
{
	if (length < 0)
		return;

    while (MARS_SYS_COMM0) {}
	*(int *)&MARS_SYS_COMM8 = length;
	*(int *)&MARS_SYS_COMM12 = offset;

    MARS_SYS_COMM0 = 0x2700;
    while (MARS_SYS_COMM0) {}
}

int Mars_ReadCDFile(int length)
{
	int words = length / 2 + 1;
	short *fb = (short *)(&MARS_FRAMEBUFFER + 0x100);

	while (MARS_SYS_COMM0) {}

	do {
		*fb++ = 0;
	} while (--words);

	*(int *)&MARS_SYS_COMM8 = length;
	MARS_SYS_COMM0 = 0x2800;

	while (MARS_SYS_COMM0) {}
	return *(int *)&MARS_SYS_COMM8;
}

static void Mars_HandleDMARequest(void)
{
	int j, l;
	int cmd, arg;
	void *dest;
	int chcr = SH2_DMA_CHCR_DM_INC|SH2_DMA_CHCR_TS_WU|SH2_DMA_CHCR_AL_AH|SH2_DMA_CHCR_DS_EDGE|SH2_DMA_CHCR_DL_AH;

	cmd = ++MARS_SYS_COMM0;
	// wait for LW of arg
	while (MARS_SYS_COMM0 == cmd);
	arg = MARS_SYS_COMM2;

	cmd = ++MARS_SYS_COMM0;
	// wait for HW of arg
	while (MARS_SYS_COMM0 == cmd);
	arg = (MARS_SYS_COMM2 << 16) | arg;

	while (!(MARS_SYS_DMACTR & MARS_SYS_DMA_68S)) ; // wait for SH DREQ to start

	l = MARS_SYS_COMM2; // # length in words
	if (l == 0)
		l = 0x10000; // 0 => 64K words

	j = (l + 3) & 0xFFFC; // # words to DMA
	if (j == 0)
		j = 0x10000; // 0 => 64K words

	l += l; // bytes
	dest = pri_dreqdma_cb(pri_dma_arg, (void*)MARS_SYS_DMADAR, l, arg);

	SH2_DMA_CHCR0; // read TE
	SH2_DMA_CHCR0 = chcr; // clear TE
	SH2_DMA_DAR0 = 0x20000000 | ((uintptr_t)dest & ~1);
	SH2_DMA_TCR0 = j;

	SH2_DMA_CHCR0 = chcr|SH2_DMA_CHCR_DE;

	++MARS_SYS_COMM0; // SH2 DMA started
	while (!(SH2_DMA_CHCR0 & SH2_DMA_CHCR_TE)) ; // wait on TE
	SH2_DMA_CHCR0 = chcr; // clear DMA TE
}

int Mars_SeekCDFile(int offset, int whence)
{
	while (MARS_SYS_COMM0) {}
	*(int *)&MARS_SYS_COMM8 = offset;
	MARS_SYS_COMM2 = whence;
	MARS_SYS_COMM0 = 0x2900;

	while (MARS_SYS_COMM0) {}
	return *(int *)&MARS_SYS_COMM8;
}

void *Mars_GetCDFileBuffer(void)
{
	return (void *)(&MARS_FRAMEBUFFER + 0x100);
}

void Mars_MCDLoadSfxFileOfs(uint16_t start_id, int numsfx, const char *name, int *offsetlen)
{
	void *ptr;

	if (!*name) {
		return;
	}

	while (MARS_SYS_COMM0);

	ptr = Mars_StringToFramebuffer(name);
	ptr = (void*)(((uintptr_t)ptr + 1 + 3) & ~3);
	fast_memcpy(ptr, offsetlen, (unsigned)numsfx*2*sizeof(*offsetlen)/4);

	MARS_SYS_COMM2 = start_id;
	MARS_SYS_COMM0 = 0x2A00|numsfx; /* load sfx */
}

int Mars_MCDReadDirectory(const char *path)
{
	if (!*path) {
		return -1;
	}

	while (MARS_SYS_COMM0);

	Mars_StringToFramebuffer(path);
	MARS_SYS_COMM0 = 0x2B00; /* read directory */
	while (MARS_SYS_COMM0);

	return *(int *)&MARS_SYS_COMM12;
}

void Mars_StoreAuxBytes(int length)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM2 = length;
	MARS_SYS_COMM0 = 0x2400;
	while (MARS_SYS_COMM0);
}

void *Mars_LoadAuxBytes(int length)
{
	while (MARS_SYS_COMM0);
	MARS_SYS_COMM2 = length;
	MARS_SYS_COMM0 = 0x2500;
	while (MARS_SYS_COMM0);
	return (void *)(&MARS_FRAMEBUFFER + 0x100);
}

void Mars_Finish(void)
{
	while (MARS_SYS_COMM0 != 0);
}

void Mars_SetPriCmdCallback(void (*cb)(void))
{
	pri_cmd_cb = cb;
}

void Mars_SetPriDreqDMACallback(void *(*cb)(void *, void *, int , int), void *arg)
{
	pri_dma_arg = arg;
	pri_dreqdma_cb = NULL ? pri_dreqdma_handler_default : cb;
}

void Mars_SetSecCmdCallback(void (*cb)(void))
{
	pri_cmd_cb = cb;
}

void Mars_SetSecDMA1Callback(void (*cb)(void))
{
	sci_dma1_cb = cb;
}

/* ======================== INTERRUPT HANDLERS ======================== */

void pri_vbi_handler(void)
{
	mars_vblank_count++;

	if (mars_newpalette)
	{
		if (Mars_UploadPalette(mars_newpalette))
			mars_newpalette = NULL;
	}
}

void pri_cmd_handler(void)
{
	switch (MARS_SYS_COMM0)
	{
		case 0xFF00:
			Mars_DetectInputDevices();
			break;
		case 0xFF10:
			Mars_HandleDMARequest();
			break;
		default:
			pri_cmd_cb();
			break;
	}
}

void sec_cmd_handler(void)
{
	sci_cmd_cb();
}

void sec_dma1_handler(void)
{
	SH2_DMA_CHCR1; // read TE
	SH2_DMA_CHCR1 = 0; // clear TE

	sci_dma1_cb();
}
