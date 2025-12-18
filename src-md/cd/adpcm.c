#include <stdint.h>
#include "pcm.h"
#include "adpcm.h"

#ifndef likely
#define likely(x)       __builtin_expect(!!(x),1)
#endif
#ifndef unlikely
#define unlikely(x)     __builtin_expect(!!(x),0)
#endif

const uint16_t adpcm_ima_steps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};

int16_t adpcm_ima_indices[89*16];

int32_t adpcm_ima_deltas[89*16];

// As per https://github.com/joncampbell123/dosbox-x/blob/master/src/hardware/sblaster.cpp
__attribute__((aligned(4))) int16_t adpcm_voc2_steps_indices[6*4*2] = {
    0, 0, 1, 4, 0, 0, -1, 4,
    1, 252, 3, 4, -1, 252, -3, 4, 2, 252, 6, 4, -2, 252, -6, 4,
    4, 252, 12, 4, -4, 252, -12, 4, 8, 252, 24, 4, -8, 252, -24, 4,
    16, 252, 48, 0, -16, 252, -48, 0
};

//#define ADPCM_USE_SLOW_DECODERS

#ifdef ADPCM_USE_SLOW_DECODERS
static void adcpm_load_bytes_slow_ima(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t len);
#endif
void adpcm_load_bytes_fast_ima(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t len);

#ifdef ADPCM_USE_SLOW_DECODERS
static void adcpm_load_bytes_slow_voc(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t len);
#endif
void adpcm_load_bytes_fast_voc2(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t len);

#define ADPCM_READ_IMA_NIBBLE(index,nibble,val,wptr) do { \
        uint8_t input_ = nibble; \
        uint8_t input2 = input_ + input_; \
        int16_t ind = (index) + input2; \
        \
        index = *((uint16_t *)((uint8_t *)adpcm_ima_indices + ind)); \
        \
        int32_t delta = *((int32_t *)((uint8_t *)&adpcm_ima_deltas[0] + ind + ind)); \
        int32_t newval = delta + (int32_t)(val); \
        if (unlikely(newval < 0)) newval = 0; \
        if (unlikely(newval > 65535)) newval = 65535; \
        val = newval/* - 32768*/; \
        \
        uint8_t tempub; \
        __asm volatile("move.w %1,-(%%sp)\n\tmoveq #0,%0\n\tmove.b (%%sp)+,%0\n\tmove.b (%2,%0.w),(%3)" : "=&d"(tempub) : "d"(val), "a"(pcm_u8_to_sm_lut), "a"(wptr)); \
        /*tempub = val >> 8;*/ \
        /**(wptr) = pcm_u8_to_sm_lut[tempub];*/ \
    } while(0)

static void adcpm_load_nibble_ima(sfx_adpcm_t *adpcm, uint8_t *wptr)
{
    uint8_t input = *adpcm->data;
    int32_t value = adpcm->value;
    int16_t index = adpcm->index;

    if (adpcm->nibble) {
        input >>= 4;
    } else {
        input &= 15;
    }

    ADPCM_READ_IMA_NIBBLE(index, input, value, wptr);

    adpcm->index = index;
    adpcm->value = value;
    adpcm->data += adpcm->nibble;
    adpcm->nibble ^= 1;
}

#ifdef ADPCM_USE_SLOW_DECODERS
static void adcpm_load_bytes_slow_ima(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t wblen)
{
    int32_t value = adpcm->value;
    int16_t index = adpcm->index;
    
    wblen >>= 1;
    do {
        uint8_t input = *adpcm->data++;
        ADPCM_READ_IMA_NIBBLE(index, input & 15, value, wptr);
        ADPCM_READ_IMA_NIBBLE(index, input >> 4, value, wptr + 2);
        wptr += 4;
    } while (--wblen);

    adpcm->index = index;
    adpcm->value = value;
}
#endif

static void adcpm_load_bytes_ima(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t wblen)
{
#ifdef ADPCM_USE_SLOW_DECODERS
    adcpm_load_bytes_slow_ima(adpcm, wptr, wblen);
#else
    adpcm_load_bytes_fast_ima(adpcm, wptr, wblen);
#endif
}

static int adcpm_advance_block_ima(sfx_adpcm_t *adpcm, int bias)
{
    // advance to next block
    uint8_t *block = adpcm->data_end;
    int block_size = adpcm->block_size;

    if (block_size > adpcm->remaining_bytes)
        block_size = adpcm->remaining_bytes;
    if (block_size < 4)
        return 0; // EOF

    adpcm->value = (int16_t)(((int16_t)block[1] << 8) | block[0] << 0) + bias;
    adpcm->index = (uint16_t)block[2];
    adpcm->data = block + 3;
    adpcm->data_end = block + block_size;
    adpcm->remaining_bytes -= block_size;
    adpcm->nibble = 0;
    return 1;
}

static uint16_t adcpm_decode_ima(sfx_adpcm_t *adpcm, uint8_t *wptr, uint16_t wblen)
{
    uint32_t len, rem;
    uint16_t owblen = wblen;

check:
    if (!wblen) {
        return owblen - wblen;
    }

    if (adpcm->data >= adpcm->data_end) {
        // advance to the next block
        if (!adcpm_advance_block_ima(adpcm, 32768)) {
            return owblen - wblen;
        }

        if (adpcm->index < 0)  adpcm->index = 0;
        if (adpcm->index > 88) adpcm->index = 88;
        adpcm->index *= 32;

        // output of initial predictor
        *wptr = pcm_u8_to_sm_lut[(uint16_t)adpcm->value>>8];
        wptr += 2;
        wblen--;
    }

    // output the trailing nibble first, which enables
    // us to operate on pairs of samples in the main loop
    if (wblen > 0 && adpcm->nibble) {
        adcpm_load_nibble_ima(adpcm, wptr);
        wptr += 2;
        wblen--;
        goto check;  // we may have just hit the end pointer
    }

    // the number of bytes we need to read from the stream
    len = wblen >> 1;
    rem = wblen & 1; // if 1 - we need to read a nibble at the end
    if (len >= adpcm->data_end - adpcm->data) {
        len = adpcm->data_end - adpcm->data;
        rem = 0;
    }

    if (len > 0) {
        // output two samples per iteration
        len <<= 1;
        adcpm_load_bytes_ima(adpcm, wptr, len);
        wptr += (len << 1);
        wblen -= len;
    }

    if (rem) {
        adcpm_load_nibble_ima(adpcm, wptr);
        wptr += 2;
        wblen--;
    }

    return owblen - wblen;
}

#define ADPCM_READ_CL2_NIBBLE(code,value,index,wptr) do { \
    int16_t ind = (code) + (index); \
    int32_t delta = *((int32_t *)((uint8_t *)adpcm_voc2_steps_indices + ind)); \
    (index) = ((index) + (delta&0xffff)) & 0x3FC; \
    \
    int16_t s = (value) + (delta>>16); \
    if (unlikely(s < 0)) s = 0; \
    if (unlikely(s > 255)) s = 255; \
    \
    (value) = s; \
    *(wptr) = pcm_u8_to_sm_lut[s];\
} while(0)

static void adcpm_load_nibbles_voc(sfx_adpcm_t *adpcm, uint8_t *wptr, int count)
{
    int16_t value = adpcm->value;
    int16_t index = adpcm->index;

    do {
        uint8_t shift = (3 - adpcm->nibble) << 1;
        uint8_t input = (*adpcm->data >> shift) & 0x3;
        if (++adpcm->nibble > 3) {
            adpcm->data++;
            adpcm->nibble = 0;
        }
        ADPCM_READ_CL2_NIBBLE(input*4, value, index, wptr);
        wptr += 2;
    } while (--count);

    adpcm->index = index;
    adpcm->value = value;
}

#ifdef ADPCM_USE_SLOW_DECODERS
static void adcpm_load_bytes_slow_voc(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t wblen)
{
    int16_t value = adpcm->value;
    int16_t index = adpcm->index;
    
    wblen >>= 2;
    do {
        uint8_t input = *adpcm->data++;
        ADPCM_READ_CL2_NIBBLE((input >> 4) & 0xC, value, index, wptr);
        ADPCM_READ_CL2_NIBBLE((input >> 2) & 0xC, value, index, wptr+2);
        ADPCM_READ_CL2_NIBBLE((input >> 0) & 0xC, value, index, wptr+4);
        ADPCM_READ_CL2_NIBBLE((input << 2) & 0xC, value, index, wptr+6);
        wptr += 8;
    } while (--wblen);

    adpcm->index = index;
    adpcm->value = value;
}
#endif

static void adcpm_load_bytes_voc(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t wblen)
{
#ifdef ADPCM_USE_SLOW_DECODERS
    adcpm_load_bytes_slow_voc(adpcm, wptr, wblen);
#else
    adpcm_load_bytes_fast_voc2(adpcm, wptr, wblen);
#endif
}

static int adcpm_advance_block_voc(sfx_adpcm_t *adpcm)
{
    // advance to next block
    uint8_t *block = adpcm->data_end;
    int block_size = 0;

    if (adpcm->remaining_bytes < 4) {
        return 0; // EOF
    }

    switch (block[0]) {
        case 1:
            block_size = block[1] | (block[2] << 8) | (block[3] << 16);
            if (!block_size) {
                return 0; // EOF
            }

            // header
            block += 4;
            // freq divisor
            block++;
            // codec id
            block++;

            adpcm->value = *block++;
            adpcm->index = 0;
            adpcm->data = block;
            adpcm->data_end = block + block_size - 3;
            adpcm->remaining_bytes -= (block_size + 4);
            adpcm->nibble = 0;
            return 1;
    }

    return 0;
}

static uint16_t adcpm_decode_voc(sfx_adpcm_t *adpcm, uint8_t *wptr, uint16_t wblen)
{
    uint32_t len, rem;
    uint16_t owblen = wblen;

check:
    if (!wblen) {
        return owblen - wblen;
    }

    if (adpcm->data >= adpcm->data_end) {
        // advance to the next block
        if (!adcpm_advance_block_voc(adpcm)) {
            return owblen - wblen;
        }

        // output of initial predictor
        *wptr = pcm_u8_to_sm_lut[(uint8_t)adpcm->value];
        wptr += 2;
        wblen--;
    }

    // output the trailing nibble first, which enables
    // us to operate on pairs of samples in the main loop
    if (wblen > 0 && adpcm->nibble) {
        int count = 4 - adpcm->nibble;
        if (count > wblen)
            count = wblen;
        adcpm_load_nibbles_voc(adpcm, wptr, count);
        wptr += (count << 1);
        wblen -= count;
        goto check;  // we may have just hit the end pointer
    }

    // the number of bytes we need to read from the stream
    len = wblen >> 2;
    rem = wblen & 3; // if not 0 - we need to read nibbles at the end
    if (len >= adpcm->data_end - adpcm->data) {
        len = adpcm->data_end - adpcm->data;
        rem = 0;
    }

    if (len > 0) {
        len <<= 2;
        adcpm_load_bytes_voc(adpcm, wptr, len);
        wptr += (len << 1);
        wblen -= len;
    }

    if (rem) {
        adcpm_load_nibbles_voc(adpcm, wptr, rem);
        wptr += (rem << 1);
        wblen -= rem;
    }

    return owblen - wblen;
}

sfx_adpcm_dec_t adpcm_decoder(sfx_adpcm_t *adpcm)
{
    switch (adpcm->codec) {
        case ADPCM_CODEC_CLVOC:
            return adcpm_decode_voc;
        case ADPCM_CODEC_IMA:
        default:
            return adcpm_decode_ima;
    }
}

uint16_t adpcm_load_samples(sfx_adpcm_t *adpcm, uint16_t doff, uint16_t len)
{
    uint16_t written = 0;
    sfx_adpcm_dec_t decode = adpcm_decoder(adpcm);

    while (len > 0)
    {
        uint16_t wr;
        uint8_t *wptr = (uint8_t *)&PCM_WAVE;
        uint16_t woff = doff & 0x0FFF;
        uint16_t wblen = 0x1000 - woff;
        wptr += (woff << 1);

        pcm_set_ctrl(0x80 + (doff >> 12)); // make sure PCM chip is ON to write wave memory, and set wave bank

        if (wblen > len)
            wblen = len;
        doff += wblen;
        len -= wblen;
        while (wblen > 0) {
            wr = decode(adpcm, wptr, wblen);
            if (!wr) {
                break;
            }
            wblen -= wr;
            wptr += (wr<<1);
            written += wr;
        }
    }

    return written;
}

static void adpcm_init_ima(void)
{
    int i, j;
    int16_t indices[16];

    for (i = 0; i < 8; i++) {
        indices[i] = (i < 4) ? -1 : 2 * (i - 3);
        indices[i+8] = indices[i];
    }

    for (i = 0; i < 89; i++) {
        for (j = 0; j < 16; j++) {
            uint32_t step = adpcm_ima_steps[i];
            int32_t delta = step >> 3;
            if (j & 4) delta += step;
            if (j & 2) delta += step>>1;
            if (j & 1) delta += step>>2;
            if (j & 8) delta = -delta;
            adpcm_ima_deltas[i*16+j] = delta;
        }
    }

    for (i = 0; i < 89; i++) {
        for (j = 0; j < 16; j++) {
            int newindex = i + indices[j];
            if (newindex < 0) {
                newindex = 0;
            }
            if (newindex > 88) {
                newindex = 88;
            }
            adpcm_ima_indices[i*16+j] = newindex << 5;
        }
    }
}

static void adpcm_init_voc(void)
{
    int i;

    if (adpcm_voc2_steps_indices[3] != 4) {
        // prevent multiple invocations
        return;
    }
    for (i = 0; i < 24; i++) {
        adpcm_voc2_steps_indices[i*2+1] *= 4;
    }
}

void adpcm_init(void)
{
    adpcm_init_ima();
    adpcm_init_voc();
}
