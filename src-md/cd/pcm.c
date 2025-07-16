
#include <stdint.h>
#include <stddef.h>
#include "pcm.h"

#define BLK_SHIFT 8

uint8_t pcm_u8_to_sm_lut[256];

static uint8_t loop_markers[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static uint8_t ChanOff;
static uint8_t PcmCtrl;

static void pcm_cpy_mono(uint16_t doff, void *src, uint16_t len, uint8_t *conv)
{
    uint8_t *sptr = (uint8_t *)src;

    while (len > 0)
    {
        uint8_t *wptr = (uint8_t *)&PCM_WAVE;
        uint16_t woff = doff & 0x0FFF;
        uint16_t wblen = 0x1000 - woff;
        wptr += (woff << 1);

        pcm_set_ctrl(0x80 + (doff >> 12)); // make sure PCM chip is ON to write wave memory, and set wave bank

        if (wblen > len)
            wblen = len;
        doff += wblen;
        len -= wblen;

        do {
            uint8_t s = *sptr++;
            if (conv)
            {
                // convert from 8-bit unsigned samples to sign/magnitude samples
                s = conv[(uint8_t)s];
            }
            *wptr++ = s;
            wptr++;
        } while (--wblen > 0);
    }
}

static void pcm_cpy_stereo(uint16_t doff, void *src, uint16_t len, uint8_t *conv)
{
    uint8_t *sptr = (uint8_t *)src;

    while (len > 0)
    {
        uint8_t *wptr = (uint8_t *)&PCM_WAVE;
        uint16_t woff = doff & 0x0FFF;
        uint16_t wblen = 0x1000 - woff;
        wptr += (woff << 1);

        pcm_set_ctrl(0x80 + (doff >> 12)); // make sure PCM chip is ON to write wave memory, and set wave bank

        if (wblen > len)
            wblen = len;
        doff += wblen;
        len -= wblen;
        do {
            uint8_t s = *sptr;
            if (conv)
            {
                // convert from 8-bit unsigned samples to sign/magnitude samples
                s = conv[(uint8_t)s];
            }
            sptr += 2;
            *wptr++ = s;
            wptr++;
        } while (--wblen > 0);
    }
}

uint16_t pcm_load_samples(uint16_t start, uint8_t *samples, uint16_t length)
{
    uint16_t doff = start;
    uint16_t len = length;
    uint8_t *sptr = (uint8_t *)samples;

    while (len > 0)
    {
        uint8_t *wptr = (uint8_t *)&PCM_WAVE;
        uint16_t woff = doff & 0x0FFF;
        uint16_t wblen = 0x1000 - woff;
        wptr += (woff << 1);

        pcm_set_ctrl(0x80 + (doff >> 12)); // make sure PCM chip is ON to write wave memory, and set wave bank

        if (wblen > len)
            wblen = len;
        doff += wblen;
        len -= wblen;

        do {
            uint8_t s = *sptr++;
            *wptr++ = s;
            wptr++;
        } while (--wblen > 0);
    }

    return length;
}

uint16_t pcm_load_samples_u8(uint16_t start, uint8_t *samples, uint16_t length)
{
    pcm_cpy_mono(start, samples, length, pcm_u8_to_sm_lut);
    return length;
}

uint16_t pcm_load_stereo_samples_u8(uint16_t start, uint16_t start2, uint8_t *samples, uint16_t length)
{
    pcm_cpy_stereo(start, samples, length, pcm_u8_to_sm_lut);
    pcm_cpy_stereo(start2, samples+1, length, pcm_u8_to_sm_lut);
    return length;
}

void pcm_loop_markers(uint16_t start)
{
    pcm_cpy_mono(start, loop_markers, 32, NULL);
}

void pcm_load_zero(uint16_t start, uint16_t len)
{
    uint16_t doff = start;

    while (len > 0)
    {
        uint8_t *wptr = (uint8_t *)&PCM_WAVE;
        uint16_t woff = doff & 0x0FFF;
        uint16_t wblen = 0x1000 - woff;
        wptr += (woff << 1);

        pcm_set_ctrl(0x80 + (doff >> 12)); // make sure PCM chip is ON to write wave memory, and set wave bank

        if (wblen > len)
            wblen = len;
        doff += wblen;
        len -= wblen;
        while (wblen > 0)
        {
            *wptr++ = 0;
            wptr++;
            wblen--;
        }
    }
}

void pcm_init(void)
{
    int i;

    for (i = 0; i < 256; i++) {
        int32_t j = i - 128;
        j = j * PCM_U8_AMPLIFICATION; // amplify the volume
        pcm_u8_to_sm_lut[i] = pcm_s8_to_sm(j);
    }

    pcm_reset();
}

void pcm_reset(void)
{
    uint16_t i;

    ChanOff = 0xFF;
    PCM_ONOFF = 0xFF; // turn off all channels
    pcm_delay();

    for (i=0; i<8; i++)
    {
        pcm_set_ctrl(0xC0 + i); // turn on pcm chip and select channel

        PCM_ENV = 0x00; // channel env off
        pcm_delay();
        PCM_PAN = 0x00;
        pcm_delay();
        PCM_FDL = 0x00;
        pcm_delay();
        PCM_FDH = 0x00;
        pcm_delay();
        PCM_LSL = 0x00;
        pcm_delay();
        PCM_LSH = 0x00;
        pcm_delay();
        PCM_START = 0x00;
        pcm_delay();
    }
}

void pcm_set_ctrl(uint8_t val)
{
    PcmCtrl = val;
    PCM_CTRL = val;
    pcm_delay();
}

uint8_t pcm_get_ctrl(void)
{
    return PcmCtrl;
}

void pcm_set_off(uint8_t index)
{
    ChanOff |= (1 << index);
    PCM_ONOFF = ChanOff;
    pcm_delay();
}

void pcm_set_on(uint8_t index)
{
    ChanOff &= ~(1 << index);
    PCM_ONOFF = ChanOff;
    pcm_delay();
}

uint8_t pcm_is_off(uint8_t index)
{
    return (ChanOff & (1 << index)) != 0;
}

void pcm_set_start(uint8_t start, uint16_t offset)
{
    PCM_START = start + (offset >> BLK_SHIFT);
    pcm_delay();
}

void pcm_set_loop(uint16_t loopstart)
{
    PCM_LSL = loopstart & 0x00FF; // low byte
    pcm_delay();
    PCM_LSH = loopstart >> 8; // high byte
    pcm_delay();
}

void pcm_set_env(uint8_t vol)
{
    PCM_ENV = vol;
    pcm_delay();
}

void pcm_set_pan(uint8_t pan)
{
    PCM_PAN = pcm_lcf(pan);
    pcm_delay();
}
