#ifndef _ADPCM_H
#define _ADPCM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//#define ADPCM_USE_SLOW_DECODERS

enum
{
    ADPCM_CODEC_NONE,
    ADPCM_CODEC_IMA,
    ADPCM_CODEC_SB4,
    ADPCM_NUM_CODECS
};

typedef struct
{
    uint8_t *data;
    int16_t index;
    uint16_t value;
    uint8_t *data_end;
    uint32_t remaining_bytes;
    uint16_t block_size;
    uint8_t codec;
    uint8_t nibble;
} sfx_adpcm_t;

typedef uint16_t (*sfx_adpcm_dec_t)(sfx_adpcm_t *, uint8_t *, uint16_t);

/* from pcm.c */
extern void adpcm_init(void);
extern uint16_t adpcm_load_samples(sfx_adpcm_t *adpcm, uint16_t start, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif
