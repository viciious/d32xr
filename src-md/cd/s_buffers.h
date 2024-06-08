#ifndef _S_BUFFERS_H
#define _S_BUFFERS_H

#include <stdint.h>
#include "adpcm.h"

#define S_MAX_BUFFERS 128

enum
{
    S_FORMAT_NONE,
    S_FORMAT_RAW_U8,
    S_FORMAT_WAV_ADPCM,
    S_FORMAT_RAW_SPCM
};

typedef struct
{
    uint8_t *data, *buf;
    uint32_t data_len, size;
    uint16_t freq;
    uint8_t num_channels;
    uint8_t format;
    uint8_t adpcm_codec;
    uint16_t adpcm_block_size;
} sfx_buffer_t;

extern sfx_buffer_t s_buffers [ S_MAX_BUFFERS ];

uint8_t *S_Buf_Alloc(uint32_t data_len);

void S_InitBuffers(uint8_t *start_addr, uint32_t size);
void S_ClearBuffersMem(void);

void S_Buf_SetData(sfx_buffer_t *buf, uint8_t *data, uint32_t data_len);
void S_Buf_CopyData(sfx_buffer_t *buf, const uint8_t *data, uint32_t data_len);

#endif
