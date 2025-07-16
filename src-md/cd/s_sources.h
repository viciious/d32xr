#ifndef _S_SOURCES_H
#define _S_SOURCES_H

#include <stdint.h>
#include <stdlib.h>

#include "pcm.h"
#include "adpcm.h"
#include "s_channels.h"
#include "s_buffers.h"

#define S_MAX_SOURCES 8

typedef struct
{
    sfx_buffer_t *buf;
    uint16_t freq;
    uint8_t channels[2];
    uint8_t num_channels;
    uint32_t data_pos;
    sfx_adpcm_t adpcm;
    uint8_t pan[2], env;
    int8_t backbuf;
    uint8_t autoloop;
    uint8_t paused;
    uint8_t eof;
    uint16_t rem;
    uint16_t bufpos[2];
    uint32_t painted;
} sfx_source_t;

#ifdef __cplusplus
extern "C" {
#endif

extern sfx_source_t s_sources[ S_MAX_SOURCES ];

void S_InitSources(void);
void S_StopSources(void);
int S_AllocSource(void);

void S_Src_Init(sfx_source_t *src);
void S_Src_Play(sfx_source_t *src, sfx_buffer_t *buf, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop);
void S_Src_Stop(sfx_source_t *src);
void S_Src_Paint(sfx_source_t *src);
void S_Src_Update(sfx_source_t *src, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop);
void S_Src_Rewind(sfx_source_t *src);
void S_Src_SetPause(sfx_source_t *src, uint8_t paused);
uint16_t S_Src_GetPosition(sfx_source_t *src);
void S_UpdateSourcesStatus(void);

#ifdef __cplusplus
}
#endif

#endif
