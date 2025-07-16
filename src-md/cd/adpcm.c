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

int32_t adpcm_sb4_steps_indices[16*4]; // deltas interleaved with indices

#ifdef ADPCM_USE_SLOW_DECODERS
static void adcpm_load_bytes_slow_ima(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t len);
#endif
void adpcm_load_bytes_fast_ima(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t len);

#ifdef ADPCM_ENABLE_SB4
#ifdef ADPCM_USE_SLOW_DECODERS
static void adcpm_load_bytes_slow_sb4(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t len);
#endif
void adpcm_load_bytes_fast_sb4(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t len);
#endif

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

static void adcpm_load_byte_ima(sfx_adpcm_t *adpcm, uint8_t *wptr)
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

static int adcpm_advance_block(sfx_adpcm_t *adpcm, int bias)
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
        if (!adcpm_advance_block(adpcm, 32768)) {
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
        adcpm_load_byte_ima(adpcm, wptr);
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
        adcpm_load_byte_ima(adpcm, wptr);
        wptr += 2;
        wblen--;
    }

    return owblen - wblen;
}

#ifdef ADPCM_ENABLE_SB4

#define ADPCM_READ_SB4_NIBBLE(code,value,index,wptr) do { \
        int16_t s; \
        int16_t ind = (uint16_t)(code)*16+index; \
        \
        int16_t tempsw; \
        int16_t *delta_index = ((int16_t *)((uint8_t *)&adpcm_sb4_steps_indices[0] + ind)); \
        s = (value) + delta_index[0]; \
        __asm volatile("spl %0\n\text.w %0\n\t" : "=&d"(tempsw)); \
        s &= tempsw; \
        if (unlikely(s > 255)) s = 255; \
        \
        (value) = s; \
        index = delta_index[1]; \
        *(wptr) = pcm_u8_to_sm_lut[(uint8_t)s];\
    } while(0)

static void adcpm_load_byte_sb4(sfx_adpcm_t *adpcm, uint8_t *wptr)
{
    uint8_t input = *adpcm->data;
    int16_t value = adpcm->value;
    int16_t index = adpcm->index;

    if (adpcm->nibble) {
        input >>= 4;
    } else {
        input &= 15;
    }

    ADPCM_READ_SB4_NIBBLE(input, value, index, wptr);

    adpcm->index = index;
    adpcm->value = value;
    adpcm->data += adpcm->nibble;
    adpcm->nibble ^= 1;
}

#ifdef ADPCM_USE_SLOW_DECODERS
static void adcpm_load_bytes_slow_sb4(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t wblen)
{
    int16_t value = adpcm->value;
    int16_t index = adpcm->index;
    
    wblen >>= 1;
    do {
        uint8_t input = *adpcm->data++;
        ADPCM_READ_SB4_NIBBLE(input & 15, value, index, wptr);
        ADPCM_READ_SB4_NIBBLE(input >> 4, value, index, wptr+2);
        wptr += 4;
    } while (--wblen);

    adpcm->index = index;
    adpcm->value = value;    
}
#endif

static void adcpm_load_bytes_sb4(sfx_adpcm_t *adpcm, uint8_t *wptr, uint32_t wblen)
{
#ifdef ADPCM_USE_SLOW_DECODERS
    adcpm_load_bytes_slow_sb4(adpcm, wptr, wblen);
#else
    adpcm_load_bytes_fast_sb4(adpcm, wptr, wblen);
#endif
}

static uint16_t adcpm_decode_sb4(sfx_adpcm_t *adpcm, uint8_t *wptr, uint16_t wblen)
{
    uint32_t len, rem;
    uint16_t owblen = wblen;

check:
    if (!wblen) {
        return owblen - wblen;
    }

    if (adpcm->data >= adpcm->data_end) {
        // advance to the next block
        if (!adcpm_advance_block(adpcm, 0)) {
            return owblen - wblen;
        }

        if (adpcm->index < 0)  adpcm->index = 0;
        if (adpcm->index > 3) adpcm->index = 3;
        adpcm->index <<= 2;

        // output of initial predictor
        *wptr = pcm_u8_to_sm_lut[(uint8_t)adpcm->value];
        wptr += 2;
        wblen--;
    }

    // output the trailing nibble first, which enables
    // us to operate on pairs of samples in the main loop
    if (wblen > 0 && adpcm->nibble) {
        adcpm_load_byte_sb4(adpcm, wptr);
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
        len <<= 1;
        adcpm_load_bytes_sb4(adpcm, wptr, len);
        wptr += (len << 1);
        wblen -= len;
    }

    if (rem) {
        adcpm_load_byte_sb4(adpcm, wptr);
        wptr += 2;
        wblen--;
    }

    return owblen - wblen;
}

#endif

sfx_adpcm_dec_t adpcm_decoder(sfx_adpcm_t *adpcm)
{
    switch (adpcm->codec) {
#ifdef ADPCM_ENABLE_SB4
        case ADPCM_CODEC_SB4:
            return adcpm_decode_sb4;
#endif
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

#ifdef ADPCM_ENABLE_SB4

// As per https://wiki.multimedia.cx/index.php/Creative_8_bits_ADPCM
static void adpcm_init_sb4(void)
{
    int i, j;

    for (i = 0; i < 16; i++) {
        for (j = 0; j < 4; j++) {
            int delta = (i & 0x7) << j;
            if (i & 0x8) {
                delta = -delta;
            }

            int value = i & 0x7;
            int newstep = j;
            if (value >= 5) {
                newstep++;
            } else if (value == 0) {
                newstep--;
            }
            if (newstep > 3) {
                newstep = 3;
            }
            if (newstep < 0) {
                newstep = 0;
            }
            adpcm_sb4_steps_indices[i*4+j] = (delta << 16) | (newstep << 2);
        }
    }
}

#endif

void adpcm_init(void)
{
    adpcm_init_ima();
#ifdef ADPCM_ENABLE_SB4
    adpcm_init_sb4();
#endif
}
