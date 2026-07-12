/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2024 Victor Luchits, Derek John Evans, id Software and ZeniMax Media

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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include "32x.h"
#include "marshw.h"
#include "mars_newrb.h"
#include "roq.h"

#define RoQ_VID_BUF_SIZE        0xE000
#define RoQ_SND_BUF_SIZE        0x5000

#define RoQ_SAMPLE_MIN          2
#define RoQ_SAMPLE_MAX          1032
#define RoQ_SAMPLE_CENTER       (RoQ_SAMPLE_MAX-RoQ_SAMPLE_MIN)/2

#define RoQ_MAX_SAMPLES         632 // 35Hz

#define RoQ_MIXAHEAD_MSEC       267 // audio mixead in milliseconds

//#define RoQ_ATTR_SDRAM  __attribute__((section(".data"), aligned(16)))
#ifndef RoQ_ATTR_SDRAM
#define RoQ_ATTR_SDRAM 
#endif

static marsrbuf_t *schunks;

static unsigned *snd_samples[2];
static int8_t snd_flip = 0;
static int8_t snd_channels = 0;
static int16_t snd_samples_rem = 0;
static int16_t snd_lr[2];
static char snd_lock = 0;
static uint32_t snd_samples_count = 0;

static marsrbuf_t *vchunks;

/*
===============================================================================

						SOUND DMA CODE FOR THE SECONDARY SH-2

===============================================================================
*/

static void roq_snddma1_handler(void) RoQ_ATTR_SDRAM;

void Mars_Sec_RoQ_InitSound(int init) __attribute__((noinline));

static void roq_snddma_center(int f)
{
    int i;
    uint16_t *s;

    s = (uint16_t *)snd_samples[f];
    for (i = 0; i < RoQ_MAX_SAMPLES; i++)
        *s++ = RoQ_SAMPLE_CENTER;
    snd_channels = 1;
}

static void roq_snddma1_load_samples(void)
{
    int i;
    int num_samples;
    uint8_t *buf_start;
    uint16_t *s = (uint16_t *)snd_samples[snd_flip];

    for (i = 0; i < RoQ_MAX_SAMPLES; )
    {
        int j, l;
        int8_t *b;

        if (!snd_samples_rem)
        {
            int chunk_id;
            uint8_t *header;

            buf_start = (uint8_t *)ringbuf_ralloc(schunks, 9);
            if (!buf_start) {
                // no data yet
                break;
            }

            header = buf_start;

            Mars_ClearCacheLines(header, 2);

            chunk_id = (header[0]) | (header[1] << 8);
            if (chunk_id != RoQ_SOUND_MONO && chunk_id != RoQ_SOUND_STEREO)
            {
                header++;
                chunk_id = (header[0]) | (header[1] << 8);
            }
            header += 2;

            if (chunk_id != RoQ_SOUND_MONO && chunk_id != RoQ_SOUND_STEREO)
            {
                ringbuf_rcommit(schunks, header - buf_start);
                break;
            }

            snd_channels = (chunk_id & 1) + 1;
            snd_samples_rem = (header[0]) | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
            header += 4;

            if (snd_channels == 1)
            {
                int init_sample = (int16_t)((header[0]) | (header[1] << 8));
                init_sample += 32768;
                snd_lr[0] = init_sample;
            }
            else
            {
                int init_sample;
                snd_samples_rem /= 2;

                init_sample = (int16_t)((header[1] << 8));
                init_sample += 32768;
                snd_lr[0] = init_sample;

                init_sample = (int16_t)((header[0] << 8));
                init_sample += 32768;
                snd_lr[1] = init_sample;
            }
            header += 2;

            ringbuf_rcommit(schunks, header - buf_start);
        }

        num_samples = snd_samples_rem;
        if (num_samples > RoQ_MAX_SAMPLES - i)
            num_samples = RoQ_MAX_SAMPLES - i;

        l = num_samples * snd_channels;
        buf_start = ringbuf_ralloc(schunks, l);
        b = (int8_t *)buf_start;

        Mars_ClearCacheLines(b, ((unsigned)l >> 4) + 2);

        if (snd_channels == 1)
        {
            int l_newval = (uint16_t)snd_lr[0];
            int c_hi = 0;
            __asm volatile("mov #1, %0\n\tshll16 %0\n\t" : "=&r"(c_hi) );

            for (j = 0; j < num_samples; j++)
            {
                int v;

                v = *b & 127;
#if 0
                // gcc uses a mul.l here, which has a slightly higher latency cycles value
                v *= v;
                if (*b++ & 128) v = -v;
#else
                __asm volatile(
                    "mulu.w %0,%0\n\t"
                    "cmp/pz %1\n\t"
                    "bt/s 1f\n\t"
                    "sts macl,%0\n\t"
                    "neg %0,%0\n\t"
                    "1:\n\t"
                     : "+r"(v) : "r"(*b++));
#endif
                l_newval += v;
                if (l_newval & c_hi) l_newval = c_hi-1;
                else if (l_newval < 0) l_newval = 0;

                *s++ = RoQ_SAMPLE_MIN + ((unsigned)l_newval >> 6);
            }

            snd_lr[0] = l_newval;
        }
        else
        {
            int l_newval = (uint16_t)snd_lr[0];
            int r_newval = (uint16_t)snd_lr[1];
            int c_hi = 0;
            __asm volatile("mov #1, %0\n\tshll16 %0\n\t" : "=&r"(c_hi) );

            for (j = 0; j < num_samples; j++)
            {
                int v;

                v = *b & 127;
#if 0
                v *= v;
                if (*b++ & 128) v = -v;
#else
                __asm volatile(
                    "mulu.w %0,%0\n\t"
                    "cmp/pz %1\n\t"
                    "bt/s 1f\n\t"
                    "sts macl,%0\n\t"
                    "neg %0,%0\n\t"
                    "1:\n\t"
                     : "+r"(v) : "r"(*b++));
#endif

                l_newval += v;
                if (l_newval & c_hi) l_newval = c_hi-1;
                else if (l_newval < 0) l_newval = 0;

                *s++ = RoQ_SAMPLE_MIN + ((unsigned)l_newval >> 6);

                v = *b & 127;
#if 0
                v *= v;
                if (*b++ & 128) v = -v;
#else
                __asm volatile(
                    "mulu.w %0,%0\n\t"
                    "cmp/pz %1\n\t"
                    "bt/s 1f\n\t"
                    "sts macl,%0\n\t"
                    "neg %0,%0\n\t"
                    "1:\n\t"
                     : "+r"(v) : "r"(*b++));
#endif

                r_newval += v;
                if (r_newval & c_hi) r_newval = c_hi-1;
                else if (r_newval < 0) r_newval = 0;

                *s++ = RoQ_SAMPLE_MIN + ((unsigned)r_newval >> 6);
            }

            snd_lr[0] = l_newval;
            snd_lr[1] = r_newval;
        }

        snd_samples_rem -= num_samples;

        if (!snd_samples_rem) {
            // done with the current chunk
            // request a new one on the next iteration
            ringbuf_rcommit(schunks, (l + ((intptr_t)b&1)));
        } else {
            ringbuf_rcommit(schunks, l);
        }

        i += num_samples;
    }

    while (atomic_flag_test_and_set(&snd_lock));
    snd_samples_count += i;
    atomic_flag_clear(&snd_lock);

    if (snd_channels == 1)
    {
        int l_lastval = RoQ_SAMPLE_MIN + ((uint16_t)snd_lr[0] >> 6);
        for (; i < RoQ_MAX_SAMPLES; i++)
            *s++ = l_lastval;
    }
    else
    {
        int l_lastval = RoQ_SAMPLE_MIN + ((uint16_t)snd_lr[0] >> 6);
        int r_lastval = RoQ_SAMPLE_MIN + ((uint16_t)snd_lr[1] >> 6);
        for (; i < RoQ_MAX_SAMPLES; i++)
        {
            *s++ = l_lastval;
            *s++ = r_lastval;
        }
    }
}

static void roq_snddma1_pwmctrl(void)
{
    if (MARS_VDP_DISPMODE & MARS_NTSC_FORMAT)
        MARS_PWM_CYCLE = (((23011361 << 1) / (RoQ_SAMPLE_RATE) + 1) >> 1) + 1; // for NTSC clock
    else
        MARS_PWM_CYCLE = (((22801467 << 1) / (RoQ_SAMPLE_RATE) + 1) >> 1) + 1; // for PAL clock
    MARS_PWM_CTRL = 0x0185; // TM = 1, RTP, RMD = right, LMD = left    
}

static void roq_snddma1_startdma(void)
{
    SH2_DMA_SAR1 = (uintptr_t)snd_samples[snd_flip];
    SH2_DMA_TCR1 = RoQ_MAX_SAMPLES;

    if (snd_channels == 2)
    {
        SH2_DMA_DAR1 = (intptr_t)&MARS_PWM_LEFT; // storing a long here will set left and right
        SH2_DMA_CHCR1 = 0x18e5; // dest fixed, src incr, size long, ext req, dack mem to dev, dack hi, dack edge, dreq rising edge, cycle-steal, dual addr, intr enabled, clear TE, dma enabled
    }
    else
    {
        SH2_DMA_DAR1 = (intptr_t)&MARS_PWM_MONO; // storing a word here will set the MONO channel
        SH2_DMA_CHCR1 = 0x14e5; // dest fixed, src incr, size word, ext req, dack mem to dev, dack hi, dack edge, dreq rising edge, cycle-steal, dual addr, intr enabled, clear TE, dma enabled
    }

    snd_flip = (snd_flip + 1) % 2;
}

static void roq_snddma1_handler(void)
{
    SH2_DMA_CHCR1; // read TE
    SH2_DMA_CHCR1 = 0; // clear TE

    if (!snd_channels)
        return;

    roq_snddma1_startdma();

    roq_snddma1_load_samples();
}

static void roq_snddma1_kickstart(void)
{
    snd_samples_rem = 0;

    snd_flip = 0;
    roq_snddma1_load_samples();

    snd_flip = 1;
    roq_snddma1_load_samples();

    snd_flip = 0;
    roq_snddma1_startdma();
}

void Mars_Sec_RoQ_InitSound(int init)
{
    int i;

#ifdef DISABLE_CDFS
    return;
#endif
    Mars_ClearCache();

    if (init == 2)
    {
        Mars_InitPWM(RoQ_SAMPLE_RATE, RoQ_SAMPLE_MIN, RoQ_SAMPLE_MAX);
        return;
    }

    snd_lr[0] = snd_lr[1] = 0;
    snd_samples_count = 0;

    for (i = 0; i < 2; i++)
    {
        roq_snddma_center(i);
    }

    if (!init)
    {
        SH2_DMA_CHCR1; // read TE
        SH2_DMA_CHCR1 = 0; // clear TE
        snd_channels = 0;
        return;
    }

    // init DMA
    SH2_DMA_DAR1 = (intptr_t)&MARS_PWM_MONO; // storing a word here will use the MONO channel
    SH2_DMA_TCR1 = 0;
    SH2_DMA_CHCR1 = 0;
    SH2_DMA_DRCR1 = 0;

    roq_snddma1_pwmctrl();

    Mars_SetSecDMA1Callback(&roq_snddma1_handler);

    roq_snddma1_kickstart();
}

/*
===============================================================================

						RoQ SUPPORT CODE FOR THE MAIN SH-2

===============================================================================
*/

static void roq_init_video(roq_info *ri)
{
    int i, j;
    int stretch, pitch, header, footer, stretch_height;
    unsigned short* lines = (unsigned short *) &MARS_FRAMEBUFFER;
    short *framebuffer;

    framebuffer = (short *)&MARS_FRAMEBUFFER;
    framebuffer += 0x100;

    stretch = 1;
    pitch = ri->canvas_pitch;
    stretch_height = ri->display_height;
    header = (224 - stretch_height) / 2;
    footer = header + stretch_height;

    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 0x10000; j++)
            framebuffer[j] = 0;

        for (j = 0; j < 256; j++)
        {
            if (j < header)
                lines[j] = stretch_height * pitch + 0x100;
            else if (j < footer)
                lines[j] = ((j-header)/stretch) * pitch + 0x100;
            else
                lines[j] = stretch_height * pitch + 0x100;
        }

        Mars_FlipFrameBuffers(1);
    }

    while ((MARS_SYS_INTMSK & MARS_SH2_ACCESS_VDP) == 0);
    MARS_VDP_DISPMODE = MARS_VDP_MODE_32K;
}

static void roq_dma_alloc(roq_file *fp)
{
    int chunk_id;
    int chunk_size;
    marsrbuf_t *rb;

    chunk_size = fp->rem_chunk_size;
    chunk_id = fp->chunk_id;
 
    if (!chunk_size) {
        return;
    }

    rb = vchunks;
    switch (chunk_id)
    {
    case RoQ_SOUND_MONO:
    case RoQ_SOUND_STEREO:
        rb = schunks;
    case RoQ_INFO:
    case RoQ_QUAD_CODEBOOK:
    case RoQ_QUAD_VQ:
    case RoQ_SIGNAGURE:
        if (!fp->dma_dest) {
            fp->dma_dest = ringbuf_walloc(rb, (chunk_size+15)&~7);
            if (!fp->dma_dest) {
                fp->oom = 1;
                fp->dma_target = NULL;
                return;
            }
            fp->oom = 0;
            fp->dma_target = rb;
        }
        break;
    default:
        fp->dma_dest = fp->backupdma_dest;
        fp->dma_target = NULL;
        break;
    }
}

static void roq_dma_done(roq_file *fp)
{
}

static void *roq_dma_dest(roq_file *fp, void *dest, int length, int dmaarg)
{
    int chunk_id;
    unsigned chunk_size;

    chunk_size = ((unsigned)dmaarg >> 16) & 0xffff;
    chunk_id = ((unsigned)dmaarg & 0xffff) >> 1;

    fp->chunk_id = chunk_id;
    fp->dma_length = length;

    if (fp->dma_dest != NULL) {
        return (void *)fp->dma_dest;
    }

    fp->rem_chunk_size = (chunk_size + 8 + 1) & ~1;
    fp->dma_length = 0;
    return NULL;
}

static void roq_request(roq_file* fp, int sync)
{
    // wait for ongoing transfer to finish
    while ((MARS_SYS_COMM8 & MARS_ROQFL_REQ) != 0);

    // EOF is reached and there's no data left
    if ((MARS_SYS_COMM8 & (MARS_ROQFL_EOF|MARS_ROQFL_NOD)) == (MARS_ROQFL_EOF|MARS_ROQFL_NOD))
        fp->eof = 1;

    if (fp->eof)
        return;

    if (fp->dma_length != 0) {
        if (fp->dma_target != NULL) {
            ringbuf_wcommit((marsrbuf_t *)fp->dma_target, fp->dma_length);
            fp->dma_dest += fp->dma_length;
        }
        if (fp->rem_chunk_size <= fp->dma_length)
        {
            fp->rem_chunk_size = 0;
            fp->dma_dest = NULL;
            fp->dma_target = NULL;
        }
        else
            fp->rem_chunk_size -= fp->dma_length;
        fp->dma_length = 0;
    }

    if (!fp->dma_dest) {
        roq_dma_alloc(fp);
    }

    // request a new chunk
    MARS_SYS_COMM8 |= MARS_ROQFL_REQ;
    if (sync) {
        while ((MARS_SYS_COMM8 & MARS_ROQFL_REQ) != 0);
    }
}

static void roq_get_chunk(roq_file* fp)
{
    uint8_t *header;
    uint8_t *chunk;
    unsigned chunk_size = 0;
    unsigned pad;

    // the initial header is strictly 8 bytes
    // other chunks may have a padding byte at the start
get_header:
    header = ringbuf_ralloc(vchunks, fp->bof ? 8 : 9);

    if (!header) {
        if (fp->eof) {
            fp->data = NULL;
            return;
        }
        roq_request(fp, 1);
        goto get_header;
    }

    Mars_ClearCacheLines(header, 2);

    pad = 0;
    if (header[1] != 0x10) {
        // if this is a valid chunk, the second byte must be a 0x10
        pad++;
        header++;
    }

    chunk_size = (header[2]) | (header[3] << 8) | (header[4] << 16) | (header[5] << 24);

    if (fp->bof) {
        // must be the file header
        chunk = header;
        chunk_size = 0;
    }
    else {
        while ((chunk = ringbuf_ralloc(vchunks, (chunk_size + 8 + 1) & ~1)) == NULL && !fp->eof) {
            roq_request(fp, 1);
        }
        chunk += pad;
    }

    if (fp->eof) {
        fp->data = NULL;
        return;
    }

    if (fp->clear_cache)
        Mars_ClearCacheLines(chunk + 8, (chunk_size >> 4) + 2);

    fp->clear_cache = 1;
    fp->data = chunk;
    fp->data_length = ((intptr_t)(header + 8 + chunk_size + 1) & ~1) - (intptr_t)header;
}

static void roq_return_chunk(roq_file* fp)
{
    int size;

    if (!fp->data) {
        return;
    }

    size = fp->data_length;
    size = (size + 1) & ~1;

    ringbuf_rcommit(vchunks, size);
    fp->bof = 0;
    fp->oom = 0;
}

static void roq_lazybuffer(roq_file* fp)
{
    if ((MARS_SYS_COMM8 & MARS_ROQFL_REQ) == 0 && !fp->oom && !fp->eof) {
        if (ringbuf_nfree(schunks) > 0x400 && ringbuf_nfree(vchunks) > 0x400) {
            roq_request(fp, 0);
        }
    }
}

static void roq_waitflip(roq_info* ri) {
    Mars_WaitFrameBuffersFlip();
}

static int roq_copyscreen(roq_info* ri, char sync, int yfrom, int yto)
{
    roq_file* fp = ri->fp;

    if (yto > ri->display_height)
        yto = ri->display_height;
    if (yto > yfrom)
    {
        if (yfrom > 0) 
        {
            if (sync)
            {
                while (!(SH2_DMA_CHCR1 & SH2_DMA_CHCR_TE)) {
                    roq_lazybuffer(fp);
                } // wait on TE
            }
            else
            {
                if (!(SH2_DMA_CHCR1 & SH2_DMA_CHCR_TE)) {
                    roq_lazybuffer(fp);
                    return 0;
                }
            }
            SH2_DMA_CHCR1 = 0; // clear TE
        }

        // start DMA
        SH2_DMA_SAR1 = (uint32_t)(ri->canvas + ri->canvas_pitch*yfrom);
        SH2_DMA_DAR1 = (uint32_t)(ri->canvascopy + ri->canvas_pitch*yfrom);
        // xfer count (4 * # of 16 byte units)
        SH2_DMA_TCR1 = (((((int16_t)(yto - yfrom - 1)*ri->canvas_pitch + ri->width)) >> 3) << 2);
        SH2_DMA_CHCR1 = SH2_DMA_CHCR_DM_INC|SH2_DMA_CHCR_SM_INC|SH2_DMA_CHCR_TS_16BU|SH2_DMA_CHCR_AR_ARM|SH2_DMA_CHCR_TB_CS|SH2_DMA_CHCR_DS_EDGE|SH2_DMA_CHCR_AL_AH|SH2_DMA_CHCR_DL_AH|SH2_DMA_CHCR_TA_DA|SH2_DMA_CHCR_DE;
    }

    if (sync)
    {
        while (!(SH2_DMA_CHCR1 & SH2_DMA_CHCR_TE)) {
            roq_lazybuffer(fp);
        } // wait on TE
        SH2_DMA_CHCR1 = 0; // clear TE
    }

    roq_lazybuffer(fp);
    return !sync;
}

static int roq_open(const char *file, roq_file *fp, char *buf)
{
    int length;

    memset(fp, 0, sizeof(*fp));
    fp->bof = 1;
    fp->data = (unsigned char *)buf;
    fp->data_length = 0;

    Mars_ClearCache();

    Mars_SetPriDreqDMACallbacks((void *(*)(void *, void *, int , int))roq_dma_dest, (void (*)(void *))roq_dma_done, fp);

    length = Mars_MCDBeginRoQStream(file);
    if (length < 0) {
        Mars_SetPriDreqDMACallbacks(NULL, NULL, NULL);
        return -1;
    }

    return 0;
}

static void roq_close(roq_info *ri, void (*secsnd)(int init))
{
    ri->fp->eof = 1;

    secsnd(0);

    Mars_MCDSopRoQStream();

    while (MARS_SYS_COMM4 != 0); // wait for the slave

    Mars_SetPriDreqDMACallbacks(NULL, NULL, NULL);

    Mars_ClearCache();
}

int Mars_PlayRoQ(const char *fn, void *mem, size_t size, int allowpause, void (*secsnd)(int init))
{
    int displayrate;
    volatile short *framebuffer;
    char *viddata, *buf;
    uint8_t *snddata;
    roq_info *ri;
    roq_file fp;
    char paused = 0;
    int ctrl = 0, prev_ctrl = 0, ch_ctrl = 0;
    int framecount = 0;
    char needaudio = 1, haveaudio = 0;
    int starttics, exptics;
    marsrbuf_t _vchunks __attribute__((aligned(16)));
    marsrbuf_t _schunks __attribute__((aligned(16)));
    roq_cell cells_u[256];
    roq_qcell qcells_u[256];

#ifdef DISABLE_CDFS
    return 0;
#endif
    if (!allowpause && (Mars_ReadController(0) & SEGA_CTRL_START)) {
        return 0;
    }

    snd_channels = 0;
    snd_samples_count = 0;

    framebuffer = (short *)&MARS_FRAMEBUFFER;
    framebuffer += 0x100;

    ri = mem;
    ri = (void *)(((intptr_t)mem + 15) & ~15);
    memset(ri, 0, sizeof(*ri));
    ri->cells_u = cells_u;
    ri->qcells_u = qcells_u;
    buf = (char *)ri + sizeof(*ri);

    viddata = (void *)(((intptr_t)buf + 15) & ~15);
    buf = viddata + RoQ_VID_BUF_SIZE;

    snddata = (void *)(((intptr_t)buf + 1) & ~1);
    buf = (void *)(snddata + RoQ_SND_BUF_SIZE);

    ri->canvascopy = (void *)(((intptr_t)buf + 15) & ~15);
    buf = (void *)(ri->canvascopy + RoQ_MAX_CANVAS_SIZE);

    snd_samples[0] = (void *)(((intptr_t)buf + 15) & ~15);
    buf = (void *)(snd_samples[0] + RoQ_MAX_SAMPLES);

    snd_samples[1] = (void *)(((intptr_t)buf + 15) & ~15);
    buf = (void *)(snd_samples[1] + RoQ_MAX_SAMPLES);

    if (buf > (char *)mem + size) {
        return -1;
    }

    vchunks = &_vchunks;
    schunks = &_schunks;

    ringbuf_init(vchunks, viddata, RoQ_VID_BUF_SIZE, 0);

    ringbuf_init(schunks, snddata, RoQ_SND_BUF_SIZE, 1);

    secsnd(2);

    if (roq_open(fn, &fp, viddata) < 0) {
        return -1;
    }

    displayrate = MARS_VDP_DISPMODE & MARS_NTSC_FORMAT ? 60 : 50;

    roq_init(ri, &fp, roq_get_chunk, roq_return_chunk, displayrate, (short *)framebuffer);

	if (roq_read_info(ri->fp, ri)) {
        roq_close(ri, secsnd);
        return -1;
    }

    // donate free memory to the audio chunks buffer
    if (ri->canvas_pitch * ri->display_height < RoQ_MAX_CANVAS_SIZE)
    {
        short *oldp = ri->canvascopy;
        int snd_buf_size = RoQ_SND_BUF_SIZE;
        int shift = RoQ_MAX_CANVAS_SIZE - ri->canvas_pitch * ri->display_height;

        ri->canvascopy += shift;
        ri->canvascopy = (void *)((intptr_t)ri->canvascopy & ~15);
        shift = ri->canvascopy - oldp;

        snd_buf_size += shift * sizeof(short);
        if (snd_buf_size > 0xF000) {
            snd_buf_size = 0xF000;
        }

        ringbuf_setsize(schunks, snd_buf_size);
    }

    roq_init_video(ri);

    while (MARS_SYS_COMM4 != 0);

    Mars_FlipFrameBuffers(0);

    framecount = 0;
    exptics = 0;
    starttics = Mars_GetTicCount();

    while(1)
    {
        int ret;
        int waittics;

        prev_ctrl = ctrl;
        ctrl = Mars_ReadController(0);
        ch_ctrl = ctrl ^ prev_ctrl;

        if (framecount > 5) // ignore key presses for the first few frames
        {
            if (allowpause) {
                if (ch_ctrl & (SEGA_CTRL_A|SEGA_CTRL_B|SEGA_CTRL_C)) {
                    ri->fp->eof = 1;
                } else if (ctrl & SEGA_CTRL_START)  {
                    if (ch_ctrl & SEGA_CTRL_START) {
                        paused = !paused;
                    }
                }
            }
            else {
                if (ch_ctrl & (SEGA_CTRL_START|SEGA_CTRL_A|SEGA_CTRL_B|SEGA_CTRL_C)) {
                    ri->fp->eof = 1;
                }
            }
        }
        else
        {
            ctrl = prev_ctrl = 0;
        }

        Mars_ClearCache();
        ri->fp->clear_cache = 0;

        if (!paused) {
            int64_t sndtime = 0;

            framecount++;
            exptics = ((int64_t)ri->frametics * framecount) >> 16;

            ret = roq_read_frame(ri, 0, roq_waitflip, roq_copyscreen);

            if (ret <= 0) {
                roq_close(ri, secsnd);
                roq_init_video(ri);
                return 1;
            }

            if (needaudio && schunks->writepos)
            {
                needaudio = 0;
                haveaudio = 1;
                secsnd(1);
            }

            if (haveaudio)
            {
                uint32_t samplecount;

                // sync video to audio

                while (atomic_flag_test_and_set(&snd_lock));
                Mars_ClearCacheLines(&snd_samples_count, 1);
                samplecount = snd_samples_count;
                atomic_flag_clear(&snd_lock);

                SH2_DIVU_DVSR = RoQ_SAMPLE_RATE;
                SH2_DIVU_DVDNTH = samplecount >> 16;
                SH2_DIVU_DVDNTL = (int64_t)samplecount << 16;

                sndtime = SH2_DIVU_DVDNT;
                sndtime += (RoQ_MIXAHEAD_MSEC*65536/1000);
            }
            
            Mars_FlipFrameBuffers(0);

            if (haveaudio) {
                waittics = exptics - ((sndtime * ri->displayrate)>>16);
            } else {
                waittics = exptics - (Mars_GetTicCount() - starttics);
            }

            if (waittics < 0) {
                continue;
            }
        }
        else
        {
            waittics = (ri->frametics>>16);
        }

        waittics = (int)Mars_GetTicCount() + waittics;
        do {
            if (!paused) {
                roq_lazybuffer(ri->fp);
            }
        } while (waittics > (int)Mars_GetTicCount());
    }

    return 0;
}
