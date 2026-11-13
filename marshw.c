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
static unsigned mars_frtc2msec_frac = 0;

static volatile char mars_have_newpalette = 0;
__attribute__((aligned(16)))
static volatile int16_t mars_newpalette[256];

int16_t mars_requested_lines = 224;
uint16_t mars_framebuffer_height = 224;

uint16_t mars_cd_ok = 0;
uint16_t mars_num_cd_tracks = 0;

uint16_t mars_refresh_hz = 0;

char *pri_dma_dest = NULL;
int pri_dma_length = 0;
void *pri_dma_arg = NULL;

const int NTSC_CLOCK_SPEED = 23011360; // HZ
const int PAL_CLOCK_SPEED = 22801467; // HZ

static char *mars_stringbuffer = (char *)(mars_framebuffer + 0x100);

void pri_vbi_handler(void) MARS_ATTR_DATA_CACHE_ALIGN;
void pri_cmd_handler(void) MARS_ATTR_DATA_CACHE_ALIGN;
void sec_dma1_handler(void) MARS_ATTR_DATA_CACHE_ALIGN;
void sec_cmd_handler(void) MARS_ATTR_DATA_CACHE_ALIGN;

static void intr_handler_stub(void) MARS_ATTR_DATA_CACHE_ALIGN;
static void intr_handler_stub(void) {}

static void *pri_dreqdma_handler_default(void *cbarg, void *dest, int length, int dmaarg) {
	return pri_dma_dest;
}

static void pri_dreqdmadone_handler_default(void *cbarg) {
	pri_dma_dest += pri_dma_length;
}

static void (*pri_cmd_cb)(void) = &intr_handler_stub;
static void *(*pri_dreqdma_cb)(void*, void *, int, int) = pri_dreqdma_handler_default;
static void (*pri_dreqdmadone_cb)(void*) = pri_dreqdmadone_handler_default;
static void (*sci_cmd_cb)(void) = &intr_handler_stub;
static void (*sci_dma1_cb)(void) = &intr_handler_stub;

static void Mars_HandleBeginDMARequest(void) MARS_ATTR_DATA_CACHE_ALIGN;
static void Mars_HandleEndDMARequest(void) MARS_ATTR_DATA_CACHE_ALIGN;

static char Mars_UploadPalette(void) MARS_ATTR_DATA_CACHE_ALIGN;

#define MARS_ACTIVE_SCREEN (*(volatile uint16_t *)(((intptr_t)&mars_activescreen) | 0x20000000))

int Mars_FRTCounter2Msec(int c)
{
	return (c * mars_frtc2msec_frac) >> 16;
}

void Mars_WaitFrameBuffersFlip(void)
{
	while ((mars_vdp->fbctl & MARS_VDP_FS) != MARS_ACTIVE_SCREEN) {
		SH2_Delay512();
	}
}

void Mars_FlipFrameBuffers(char wait)
{
	MARS_ACTIVE_SCREEN = !MARS_ACTIVE_SCREEN;
	mars_vdp->fbctl = MARS_ACTIVE_SCREEN;
	if (wait) Mars_WaitFrameBuffersFlip();
}

char Mars_FramebuffersFlipped(void)
{
	return (mars_vdp->fbctl & MARS_VDP_FS) == MARS_ACTIVE_SCREEN;
}

void Mars_InitLineTable(void)
{
	int j;
	int blank;
	int offset = 0; // 224p or 240p
	unsigned short* lines = (unsigned short *)mars_framebuffer;

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

void Mars_SetPalette(const uint8_t *palette)
{
	int	i;

	for (i = 0; i < 256; i++) {
		unsigned r = 1 + *palette++;
		unsigned g = 1 + *palette++;
		unsigned b = 1 + *palette++;

		if (r > 255) r = 255;
		if (g > 255) g = 255;
		if (b > 255) b = 255;

		b = ((b >> 3) & 0x1f) << 10;
		g = ((g >> 3) & 0x1f) << 5;
		r = ((r >> 3) & 0x1f) << 0;

		mars_newpalette[i] = r | g | b;
	}

	mars_have_newpalette = 1;
}

int Mars_BackBuffer(void) {
	return MARS_ACTIVE_SCREEN;
}

static char Mars_UploadPalette(void)
{
	int i;
	volatile int* newpal = (volatile int *)mars_newpalette;
	volatile int* cram = (volatile int *)mars_cram;

	if ((mars_sys->intmsk & MARS_SH2_ACCESS_VDP) == 0)
		return 0;
	for (i = 0; i < 128; i += 4)
	{
		cram[i+0] = newpal[i+0];
		cram[i+1] = newpal[i+1];
		cram[i+2] = newpal[i+2];
		cram[i+3] = newpal[i+3];
	}
	return 1;
}

int Mars_PollMouse(void)
{
	unsigned int mouse1, mouse2;
	int port = mars_mouseport;

	if (port < 0)
		return -1;

	while (mars_sys_comm->comm0); // wait until 68000 has responded to any earlier requests
	mars_sys_comm->comm0 = 0x0500 | port; // tells 68000 to read mouse
	while (mars_sys_comm->comm0 == (0x0500 | port)); // wait for mouse value

	mouse1 = (uint16_t)mars_sys_comm->comm0;
	mouse2 = (uint16_t)mars_sys_comm->comm2;
	mars_sys_comm->comm0 = 0; // tells 68000 we got the mouse value

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
	unsigned int cnt = sh2_wdt->rtcnt;
	return (int)((mars_pwdt_ovf_count << 8) | cnt);
}

void Mars_InitVideo(int lines)
{
	int i;
	char NTSC;
	int mars_lines = lines == 240 || lines == -240 ? MARS_240_LINES : MARS_224_LINES;

	while ((mars_sys->intmsk & MARS_SH2_ACCESS_VDP) == 0);

	mars_vdp->dispmode = mars_lines | MARS_VDP_MODE_256;
	NTSC = (mars_vdp->dispmode & MARS_NTSC_FORMAT) != 0;

	// change 4096.0f to something else if WDT TCSR is changed!
	mars_frtc2msec_frac = 4096.0f * 1000.0f / (NTSC ? NTSC_CLOCK_SPEED : PAL_CLOCK_SPEED) * 65536.0f;

	mars_refresh_hz = NTSC ? 60 : 50;
	mars_requested_lines = lines;
	mars_framebuffer_height = lines == 240 ? 240 : 224;
	MARS_ACTIVE_SCREEN = mars_vdp->fbctl;

	Mars_FlipFrameBuffers(1);

	for (i = 0; i < 2; i++)
	{
		int* p, * p_end;

		Mars_InitLineTable();

		p = (int*)(mars_framebuffer + 0x100);
		p_end = (int*)p + 320 / 4 * mars_framebuffer_height;
		do {
			*p = 0;
		} while (++p < p_end);

		Mars_FlipFrameBuffers(1);
	}
}

void Mars_Init(void)
{
	int i;

	/* no controllers or mouse by default */
	for (i = 0; i < MARS_MAX_CONTROLLERS; i++)
		mars_gamepadport[i] = -1;
	mars_mouseport = -1;

	// init DMA channel 0 & 1
	sh2_dma->sar0 = (int)&mars_sys->dmafifo;
	sh2_dma->dar0 = 0;
	sh2_dma->tcr0 = 0;
	sh2_dma->chcr0 = 0;
	sh2_dma_drcr->drcr0 = 0;
	sh2_dma->sar1 = 0;
	sh2_dma->dar1 = 0;
	sh2_dma->tcr1 = 0;
	sh2_dma->chcr1 = 0;
	sh2_dma_drcr->drcr1 = 0;
	sh2_dma->dmaor = 1; // enable DMA transfers on all channels

	sh2_wdt->wtcsr_tcnt = 0xA518; /* WDT TCSR = clr OVF, IT mode, timer off, clksel = Fs/2 */

	/* init hires timer system */
	*sh2_int_vcrwdt = (65<<8) | (*sh2_int_vcrwdt & 0x00FF); // set exception vector for WDT
	*sh2_int_ipra = (*sh2_int_ipra & 0xFF0F) | 0x0020; // set WDT INT to priority 2

	Mars_UpdateCD();

	if (mars_cd_ok & 0x1)
	{
		/* if the CD is present, give it seconds to init */
		Mars_WaitTicks(180);
	}

	Mars_SetMusicVolume(255);
}

void Mars_InitPWM(int sample_rate, int min_sample, int max_sample)
{
    uint16_t sample, ix;
	int centre_sample = (max_sample-min_sample)/2;

	// init the sound hardware
	mars_pwm->mono = 1;
	mars_pwm->mono = 1;
	mars_pwm->mono = 1;
	if (mars_vdp->dispmode & MARS_NTSC_FORMAT)
		mars_pwm->cycle = (((23011361 << 1) / (sample_rate)+1) >> 1) + 1; // for NTSC clock
	else
		mars_pwm->cycle = (((22801467 << 1) / (sample_rate)+1) >> 1) + 1; // for PAL clock
	mars_pwm->ctrl = 0x0185; // TM = 1, RTP, RMD = right, LMD = left

	sample = min_sample;

	// ramp up to SAMPLE_CENTER to avoid click in audio (real 32X)
	while (sample < centre_sample)
	{
		for (ix = 0; ix < (sample_rate * 2) / (centre_sample - min_sample); ix++)
		{
			while (mars_pwm->mono & 0x8000); // wait while full
			mars_pwm->mono = sample;
		}
		sample++;
	}
}

uint16_t* Mars_FrameBufferLines(void)
{
	uint16_t* lines = (uint16_t*)mars_framebuffer;
	if (mars_requested_lines == -240)
		lines += (240 - 224) / 2;
	return lines;
}

void Mars_ReadSRAM(uint8_t * buffer, int offset, int len)
{
	uint8_t *ptr = buffer;

	while (mars_sys_comm->comm0);
	while (len-- > 0) {
		mars_sys_comm->comm2 = offset++;
		mars_sys_comm->comm0 = 0x0100;    /* Read SRAM */
		while (mars_sys_comm->comm0);
		*ptr++ = mars_sys_comm->comm2 & 0x00FF;
	}
}

void Mars_WriteSRAM(const uint8_t* buffer, int offset, int len)
{
	const uint8_t *ptr = buffer;

	while (mars_sys_comm->comm0);
	while (len-- > 0) {
		mars_sys_comm->comm2 = offset++;
		mars_sys_comm->comm0 = 0x0200 | *ptr++;    /* Write SRAM */
		while (mars_sys_comm->comm0);
	}
}

void Mars_SetStringBufferOffset(int offset)
{
	mars_stringbuffer = (char *)(mars_framebuffer + 0x100) + ((offset + 3)&~3);
}

char *Mars_StringToFramebuffer(const char *str)
{
	short *fb = (short *)(mars_framebuffer + 0x100);
	char *buf = mars_stringbuffer;
	int *lnbuf = (int *)(mars_framebuffer + 0x0F0); // 16 unused words of the line table

	if (!*str)
		return (char *)buf;

	do {
		*buf++ = *str++;
	} while (*str);

	*(int16_t *)((uintptr_t)buf & ~1) = 0;
	*(buf-1) = *(str-1);
	*lnbuf = (uintptr_t)mars_stringbuffer - (uintptr_t)fb;

	return (char *)buf;
}

void Mars_UpdateCD(void)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x0600;
	while (mars_sys_comm->comm0);
	mars_cd_ok = mars_sys_comm->comm2;
	mars_num_cd_tracks = (mars_cd_ok >> 2) & 63;
	mars_cd_ok = mars_cd_ok & 0x3;
}

void Mars_UseCD(int usecd)
{
	while (mars_sys_comm->comm0);

	if (!mars_cd_ok)
		return;

	mars_sys_comm->comm2 = usecd & 1;
	mars_sys_comm->comm0 = 0x0700;
	while (mars_sys_comm->comm0);
}

void Mars_PlayTrack(char use_cda, int playtrack, const char *name, int offset, int length, char looping)
{
	char *ptr;
	int send_fn = !use_cda && name && name[0] ? 0x1 : 0;
	int use_cdf = send_fn && offset < 0 ? 0x2 : 0;

	Mars_UseCD(use_cda);

	if (send_fn)
	{
		ptr = Mars_StringToFramebuffer(name);
		ptr = (void*)(((uintptr_t)ptr + 1 + 3) & ~3);
	}

	if (!use_cda)
	{
		*(int *)&mars_sys_comm->comm8 = offset;
		*(int *)&mars_sys_comm->comm12 = length;
	}

	mars_sys_comm->comm2 = playtrack | (looping ? 0x8000 : 0x0000);
	mars_sys_comm->comm0 = 0x0300 | use_cdf | send_fn; /* start music */
	while (mars_sys_comm->comm0);
}

void Mars_MCDResumeSPCMTrack(void)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x2C00;
	while (mars_sys_comm->comm0);
}

void Mars_MCDOpenTray(void)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x2D00;
	while (mars_sys_comm->comm0);
}

void Mars_MCDLoadSfx(uint16_t id, void *data, uint32_t data_len)
{
	int i;
	uint16_t s[4];

	while (mars_sys_comm->comm0);

	s[0] = (uintptr_t)data_len>>16, s[1] = (uintptr_t)data_len&0xffff;
	s[2] = (uintptr_t)data>>16, s[3] = (uintptr_t)data&0xffff;

	for (i = 0; i < 4; i++) {
		mars_sys_comm->comm2 = s[i];
		mars_sys_comm->comm0 = 0x1D01+i;
		while (mars_sys_comm->comm0);
	}

	mars_sys_comm->comm2 = id;
	mars_sys_comm->comm0 = 0x1D00; /* load sfx */
	while (mars_sys_comm->comm0);
}

void Mars_MCDPlaySfx(uint8_t src_id, uint16_t buf_id, uint8_t pan, uint8_t vol, uint16_t freq)
{
	if (src_id == 0)
		return;

	while (mars_sys_comm->comm0);

	mars_sys_comm->comm2 = (pan<<8)|vol;
	mars_sys_comm->comm8 = freq;
	mars_sys_comm->comm10 = buf_id;
	mars_sys_comm->comm0 = 0x1E00|src_id;

	while (mars_sys_comm->comm0);
}

int Mars_MCDGetSfxPlaybackStatus(void)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x1F00;
	while (mars_sys_comm->comm0);
	return mars_sys_comm->comm2;
}

void Mars_MCDClearSfx(void)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x2000;
	while (mars_sys_comm->comm0);
}

void Mars_MCDUpdateSfx(uint8_t src_id, uint8_t pan, uint8_t vol, uint16_t freq)
{
	if (src_id == 0)
		return;

	while (mars_sys_comm->comm0);
	mars_sys_comm->comm2 = (pan<<8)|vol;
	mars_sys_comm->comm8 = freq;
	mars_sys_comm->comm0 = 0x2100|src_id;
	while (mars_sys_comm->comm0);
}

void Mars_MCDStopSfx(uint8_t src_id)
{
	if (src_id == 0)
		return;
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x2200|src_id;
	while (mars_sys_comm->comm0);
}

void Mars_MCDFlushSfx(void)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x2300;
	while (mars_sys_comm->comm0);
}

void Mars_StopTrack(void)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x0400; /* stop music */
	while (mars_sys_comm->comm0);
}

void Mars_SetMusicVolume(uint8_t volume)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x1800|volume;
	while (mars_sys_comm->comm0);
}

void Mars_WaitTicks(int ticks)
{
	unsigned ticend = mars_vblank_count + ticks;
	while (mars_vblank_count < ticend);
}

int Mars_GetMusicStatus(void)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x1100;
	while (mars_sys_comm->comm0);
	return mars_sys_comm->comm2;
}

/*
 *  MD network functions
 */

static inline unsigned short GetNetByte(void)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x1200;	/* get a byte from the network */
	while (mars_sys_comm->comm0);
	return *(volatile unsigned short *)&mars_sys_comm->comm2;		/* status:byte */
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
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x1300 | (val & 0x00FF);	/* send a byte to the network */
	while (mars_sys_comm->comm0);
	return (*(volatile unsigned short *)&mars_sys_comm->comm2 == 0xFFFF) ? -1 : 0;
}

void Mars_SetupNet(int type)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x1400 | (type & 255);		/* init joyport 2 for networking */
	while (mars_sys_comm->comm0);
}

void Mars_CleanupNet(void)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x1500;		/* cleanup networking */
	while (mars_sys_comm->comm0);
}

void Mars_SetNetLinkTimeout(int timeout)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm2 = timeout;
	mars_sys_comm->comm0 = 0x1700;
	while (mars_sys_comm->comm0);
}

void Mars_MDSetFontPalette(const uint8_t* palette)
{
	int	i;
	short *buf = (short *)(mars_framebuffer + 0x0F0); // 16 unused words of the line table

	while (mars_sys_comm->comm0) ;

	for (i = 0; i < 16; i++) {
		int r = *palette++;
		int g = *palette++;
		int b = *palette++;

		b = (((unsigned)(b) >> 5) & 0x7) << 9;
		g = (((unsigned)(g) >> 5) & 0x7) << 5;
		r = (((unsigned)(r) >> 5) & 0x7) << 1;

		buf[i] = r | g | b;

	}

    mars_sys_comm->comm0 = 0x0B00;
    while (mars_sys_comm->comm0) ;
}

void Mars_MDLoadFont(int xtiles, int ytiles, short *data)
{
	int *buf = (int *)(mars_framebuffer + 0x0F0); // 16 unused words of the line table
	short *fb = (short *)(mars_framebuffer + 0x100);

	if (data < fb || data >= mars_overwrite_img) {
		return;
	}

	while (mars_sys_comm->comm0) ;
	mars_sys_comm->comm2 = ((ytiles & 0xff) << 8) | (xtiles & 0xff);
	*buf = (uintptr_t)data - (uintptr_t)fb;
    mars_sys_comm->comm0 = 0x0C00;
    while (mars_sys_comm->comm0) ;
}

void Mars_MDPutString(int x, int y, const char *str)
{
	if (!str || !*str) {
		return;
	}
	while (mars_sys_comm->comm0) ;
	mars_sys_comm->comm2 = (y << 6) | x;
	Mars_StringToFramebuffer(str);
	mars_sys_comm->comm0 = 0x0D00|1; // force the line to be cleared
	while (mars_sys_comm->comm0) ;
}

void Mars_SetBankPage(int bank, int page)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x1600 | (page<<3) | bank;
	while (mars_sys_comm->comm0);
}

void Mars_SetBankPageSec(int bank, int page)
{
	volatile unsigned short bcomm4 = mars_sys_comm->comm4;

	mars_sys_comm->comm4 = 0x1600 | (page<<3) | bank;
	while (mars_sys_comm->comm4 != 0x1000);

	mars_sys_comm->comm4 = bcomm4;
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
		while (*(volatile unsigned short *)&mars_sys_comm->comm0 != ctrl_wait);

		int val = *(volatile unsigned short *)&mars_sys_comm->comm2;
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

		mars_sys_comm->comm0 = ++ctrl_wait;
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

void Mars_SetVDPPri(int sel)
{
	if (sel) {
		mars_vdp->dispmode |= MARS_VDP_PRIO_32X;
	} else {
		mars_vdp->dispmode &= ~MARS_VDP_PRIO_32X;
	}
}

void Mars_InitMDVDP(void)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm0 = 0x1900 | (1);
	while (mars_sys_comm->comm0);
}

void Mars_StoreWordColumnInMDVRAM(int c, int count)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm2 = count;
	mars_sys_comm->comm0 = 0x1A00|c;		/* sel = to VRAM, column in LB of comm0, start move */
}

void Mars_LoadWordColumnFromMDVRAM(int c, const uint16_t *length_offsets, int count)
{
	int i;
	int offset;
	volatile short *line_table =
		(volatile short *)(mars_framebuffer + 0x0F0);

	for (offset = 0; offset < count; offset += 16) {
		int bcount = count - offset;
		int hascols = 0;

		if (bcount > 16)
			bcount = 16;

		for (i = 0; i < bcount; i++) {
			if (length_offsets[offset + i] != 0) {
				hascols = 1;
				break;
			}
		}
		if (!hascols)
			continue;

		while (mars_sys_comm->comm0 != 0);
		for (i = 0; i < bcount; i++) {
			line_table[i] = length_offsets[offset + i];
		}
		mars_sys_comm->comm2 = bcount;
		mars_sys_comm->comm0 = 0x1B00 | (c + offset);
	}

	while (mars_sys_comm->comm0 != 0);
}

void Mars_SwapWordColumnWithMDVRAM(int c, int count)
{
    while (mars_sys_comm->comm0);
    mars_sys_comm->comm2 = count;
    mars_sys_comm->comm0 = 0x1C00|c;        /* sel = swap with VRAM, column in LB of comm0, start move */
}

int Mars_OpenCDFileByName(const char *name, int *poffset)
{
	if (!*name) {
		return -1;
	}

	if (!mars_cd_ok) {
		return -1;
	}

	while (mars_sys_comm->comm0) {}
	Mars_StringToFramebuffer(name);
    mars_sys_comm->comm0 = 0x2600;
    while (mars_sys_comm->comm0) {}

	if (poffset)
		*poffset = *(int *)&mars_sys_comm->comm12;
	return *(int *)&mars_sys_comm->comm8;
}

void Mars_OpenCDFileByOffset(int length, int offset)
{
	if (length < 0)
		return;

	if (!mars_cd_ok) {
		return;
	}

    while (mars_sys_comm->comm0) {}
	*(int *)&mars_sys_comm->comm8 = length;
	*(int *)&mars_sys_comm->comm12 = offset;

    mars_sys_comm->comm0 = 0x2700;
    while (mars_sys_comm->comm0) {}
}

int Mars_ReadCDFile(int length)
{
	int words = length / 2 + 1;
	short *fb = (short *)(mars_framebuffer + 0x100);

	while (mars_sys_comm->comm0) {}

	do {
		*fb++ = 0;
	} while (--words);

	*(int *)&mars_sys_comm->comm8 = length;
	mars_sys_comm->comm0 = 0x2800;

	while (mars_sys_comm->comm0) {}
	return *(int *)&mars_sys_comm->comm8;
}

static void Mars_HandleBeginDMARequest(void)
{
	int j, l;
	int cmd, arg;
	void *dest;
	int chcr = SH2_DMA_CHCR_DM_INC|SH2_DMA_CHCR_TS_WU|SH2_DMA_CHCR_AL_AH|SH2_DMA_CHCR_DS_EDGE|SH2_DMA_CHCR_DL_AH;

	cmd = ++mars_sys_comm->comm0;
	while (mars_sys_comm->comm0 == cmd);

	l = mars_sys_comm->comm2; // length in words
	arg = (((((mars_sys->dmadar>>1)&0xffff)<<16)))|(((mars_sys->dmasar>>1)&0xffff));

	dest = pri_dreqdma_cb(pri_dma_arg, NULL, l*2, arg);

	if (!dest) {
		// deny
		++mars_sys_comm->comm2;
		cmd = ++mars_sys_comm->comm0;
		while (mars_sys_comm->comm0 == cmd);
		return;
	}

	cmd = ++mars_sys_comm->comm0;
	while (mars_sys_comm->comm0 == cmd);

	while (!(mars_sys->dmactr & MARS_SYS_DMA_68S)) ; // wait for SH DREQ to start

	j = (l + 3) & 0xFFFC; // # words to DMA
	if (j == 0)
		j = 0x10000; // 0 => 64K words

	pri_dma_dest = dest;
	pri_dma_length = l*2;

	sh2_dma->chcr0; // read TE
	sh2_dma->chcr0 = chcr; // clear TE
	sh2_dma->dar0 = 0x20000000 | ((uintptr_t)dest);
	sh2_dma->tcr0 = j;

	sh2_dma->chcr0 = chcr|SH2_DMA_CHCR_DE;
}

static void Mars_HandleEndDMARequest(void)
{
	int flag;
	int chcr = SH2_DMA_CHCR_DM_INC|SH2_DMA_CHCR_TS_WU|SH2_DMA_CHCR_AL_AH|SH2_DMA_CHCR_DS_EDGE|SH2_DMA_CHCR_DL_AH;

	while (!(sh2_dma->chcr0 & SH2_DMA_CHCR_TE)) {
	} // wait on TE
	sh2_dma->chcr0 = chcr; // clear DMA TE

	// wait for an ack
	flag = mars_sys_comm->comm0 + 2;
	mars_sys_comm->comm0 = flag;
	while (mars_sys_comm->comm0 == flag);

	pri_dreqdmadone_cb(pri_dma_arg);
}

int Mars_SeekCDFile(int offset, int whence)
{
	while (mars_sys_comm->comm0) {}
	*(int *)&mars_sys_comm->comm8 = offset;
	mars_sys_comm->comm2 = whence;
	mars_sys_comm->comm0 = 0x2900;

	while (mars_sys_comm->comm0) {}
	return *(int *)&mars_sys_comm->comm8;
}

void *Mars_GetCDFileBuffer(void)
{
	return (void *)(mars_framebuffer + 0x100);
}

void Mars_MCDLoadSfxFileOfs(uint16_t start_id, int numsfx, const char *name, int *offsetlen)
{
	void *ptr;

	if (!*name) {
		return;
	}

	while (mars_sys_comm->comm0);

	ptr = Mars_StringToFramebuffer(name);
	ptr = (void*)(((uintptr_t)ptr + 1 + 3) & ~3);
	fast_memcpy(ptr, offsetlen, (unsigned)numsfx*2*sizeof(*offsetlen)/4);

	mars_sys_comm->comm2 = start_id;
	mars_sys_comm->comm0 = 0x2A00|numsfx; /* load sfx */

	while (mars_sys_comm->comm0);
}

int Mars_MCDReadDirectory(const char *path, char **pbuf)
{
	if (!*path) {
		return -1;
	}
	if (!mars_cd_ok) {
		return -1;
	}

	while (mars_sys_comm->comm0);

	Mars_StringToFramebuffer(path);
	mars_sys_comm->comm0 = 0x2B00; /* read directory */
	while (mars_sys_comm->comm0);
	*pbuf = mars_stringbuffer;
	return *(int *)&mars_sys_comm->comm12;
}

int Mars_MCDBeginRoQStream(const char *file)
{
	int length, offset;

    length = Mars_OpenCDFileByName(file, &offset);
    if (length < 0) {
        return -1;
    }

	mars_sys_comm->comm8 = 0x1000;
    mars_sys_comm->comm0 = 0x2E00;
	return length;
}

void Mars_MCDSopRoQStream(void)
{
    while (mars_sys_comm->comm8 & MARS_ROQFL_REQ) {
		;
	}
	mars_sys_comm->comm8 |= MARS_ROQFL_STP; // tell the 68k to stop streaming

	while (mars_sys_comm->comm0) {
		;
	}
}

void Mars_StoreAuxBytes(int length)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm2 = length;
	mars_sys_comm->comm0 = 0x2400;
	while (mars_sys_comm->comm0);
}

void *Mars_LoadAuxBytes(int length)
{
	while (mars_sys_comm->comm0);
	mars_sys_comm->comm2 = length;
	mars_sys_comm->comm0 = 0x2500;
	while (mars_sys_comm->comm0);
	return (void *)(mars_framebuffer + 0x100);
}

void Mars_Finish(void)
{
	while (mars_sys_comm->comm0 != 0);
}

void Mars_SetPriCmdCallback(void (*cb)(void))
{
	pri_cmd_cb = cb;
}

void Mars_SetPriDreqDMACallbacks(void *(*cb)(void *, void *, int , int), void (*cbdone)(void *), void *arg)
{
	pri_dma_arg = arg;
	pri_dreqdma_cb = cb == NULL ? pri_dreqdma_handler_default : cb;
	pri_dreqdmadone_cb = cbdone == NULL ? pri_dreqdmadone_handler_default : cbdone;
}

void Mars_SetSecCmdCallback(void (*cb)(void))
{
	sci_cmd_cb = cb;
}

void Mars_SetSecDMA1Callback(void (*cb)(void))
{
	sci_dma1_cb = cb;
}

/* ======================== INTERRUPT HANDLERS ======================== */

void pri_vbi_handler(void)
{
	mars_vblank_count++;

	if (mars_have_newpalette)
	{
		if (Mars_UploadPalette())
			mars_have_newpalette = 0;
	}
}

void pri_cmd_handler(void)
{
	switch (*(volatile unsigned short *)&mars_sys_comm->comm0)
	{
		case 0xFF00:
			Mars_DetectInputDevices();
			break;
		case 0xFF10:
			Mars_HandleBeginDMARequest();
			break;
		case 0xFF20:
			Mars_HandleEndDMARequest();
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
	sh2_dma->chcr1; // read TE
	sh2_dma->chcr1 = 0; // clear TE

	sci_dma1_cb();
}
