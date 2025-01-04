#include <string.h>
#include "s_sources.h"
#include "s_channels.h"
#include "s_buffers.h"
#include "s_main.h"
#include "s_cd.h"
#include "s_spcm.h"

#define S_MEMBANK_SIZE 454*1024
#define S_MEMBANK_PTR s_membank
static uint8_t s_membank[S_MEMBANK_SIZE] = { 0 };

void S_Init(void)
{
    pcm_init ();
    
    adpcm_init();

    S_InitChannels();

    S_InitSources();

    S_InitBuffers(S_MEMBANK_PTR, S_MEMBANK_SIZE);
}

void S_Clear(void)
{
    S_StopSources();

    S_ClearChannels();

    S_StopSPCMTrack();
}

void S_Update(void)
{
    static int s_upd = 0;
    sfx_source_t *src;

    if (s_upd >= S_MAX_SOURCES) {
        s_upd = 0;
    }

    src = &s_sources[ s_upd++ ];
    S_Src_Paint(src);
}

void S_SetBufferData(uint16_t buf_id, uint8_t *data, uint32_t data_len)
{
    if (buf_id == 0 || buf_id > S_MAX_BUFFERS) {
        return;
    }
    S_Buf_SetData(&s_buffers[ buf_id - 1 ], data, data_len);
}

void S_CopyBufferData(uint16_t buf_id, const uint8_t *data, uint32_t data_len)
{
    if (buf_id == 0 || buf_id > S_MAX_BUFFERS) {
        return;
    }
    S_Buf_CopyData(&s_buffers[ buf_id - 1 ], data, data_len);
}

uint8_t S_PlaySource(uint8_t src_id, uint16_t buf_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop)
{
    sfx_source_t *src;
    sfx_buffer_t *buf;

    if (src_id == 255) {
        // find a free source
        src_id = S_AllocSource();
    }

    if (src_id == 0 || src_id > S_MAX_SOURCES) {
        return 0;
    }
    if (buf_id == 0 || buf_id > S_MAX_BUFFERS) {
        return 0;
    }

    src = &s_sources[ src_id - 1 ];
    buf = &s_buffers[ buf_id - 1 ];

    S_Src_Stop(src);

    S_Src_Play(src, buf, freq, pan, vol, autoloop);

    if (!src->buf) {
        // refused to start
        return 0;
    }

    return src_id;
}

void S_UpdateSource(uint8_t src_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop)
{
    sfx_source_t *src = &s_sources[ src_id - 1 ];

    if (src_id == 0 || src_id > S_MAX_SOURCES) {
        return;
    }
    S_Src_Update(src, freq, pan, vol, autoloop);
}

void S_RewindSource(uint8_t src_id)
{
    sfx_source_t *src = &s_sources[ src_id - 1 ];

    if (src_id == 0 || src_id > S_MAX_SOURCES) {
        return;
    }
    S_Src_Rewind(src);
}

void S_PUnPSource(uint8_t src_id, uint8_t pause)
{
    sfx_source_t *src = &s_sources[ src_id - 1 ];

    if (src_id == 0 || src_id > S_MAX_SOURCES) {
        return;
    }
    S_Src_SetPause(src, pause);
}

void S_StopSource(uint8_t src_id)
{
    sfx_source_t *src = &s_sources[ src_id - 1 ];

    if (src_id == 0 || src_id > S_MAX_SOURCES) {
        return;
    }
    S_Src_Stop( src );
}

uint16_t S_GetSourcePosition(uint8_t src_id)
{
    sfx_source_t *src = &s_sources[ src_id - 1 ];

    if (src_id == 0 || src_id > S_MAX_SOURCES) {
        return 0xffff;
    }
    return S_Src_GetPosition(src);
}

int S_LoadCDBuffers(uint16_t buf_id, int numsfx, const uint8_t *data)
{
    const char *name;
    const int32_t *offsetlen;

    if (buf_id == 0 || buf_id > S_MAX_BUFFERS) {
        return 0;
    }

    if (buf_id + numsfx > S_MAX_BUFFERS) {
        numsfx = S_MAX_BUFFERS - buf_id;
    }
    if (numsfx <= 0) {
        return 0;
    }

    name = (const char *)data;
    offsetlen = (void *)(((uintptr_t)name + mystrlen(name) + 1 + 3) & ~3);

    S_CD_LoadBuffers(&s_buffers[ buf_id - 1 ], numsfx, name, offsetlen);

    return 1;
}

int S_PlaySPCMTrack(const char *name, int repeat)
{
    return S_SCM_PlayTrack(name, repeat);
}

void S_StopSPCMTrack(void)
{
    S_SPCM_Suspend();
}

void S_PauseSPCMTrack(void)
{
    S_SPCM_Suspend();
}

void S_UnpauseSPCMTrack(void)
{
    S_SPCM_Unsuspend();
}

uint8_t *S_GetMemBankPtr(void)
{
    return s_membank;
}
