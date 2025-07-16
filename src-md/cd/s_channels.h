#ifndef _S_CHANNELS_H
#define _S_CHANNELS_H

#include <stdint.h>
#include <stdlib.h>

#ifndef S_MAX_CHANNELS
#define S_MAX_CHANNELS 6
#endif

#define CHBUF_SHIFT 9
#define CHBUF_SIZE (1<<CHBUF_SHIFT)
#define CHBUF_POS(b) ((b)<<CHBUF_SHIFT)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t id;         // 0 for dummy channel
    uint8_t realid;     // hardware channel id
    uint16_t freq;
    uint8_t pan, env;
    int8_t backbuf;
} sfx_channel_t;

extern sfx_channel_t s_channels[ S_MAX_CHANNELS+1 ]; // 0 is a dummy channel

void S_InitChannels(void);
void S_ClearChannels(void);

void S_Chan_Init(sfx_channel_t *src);
void S_Chan_Clear(sfx_channel_t *src);
void S_Chan_Paint(sfx_channel_t *src);
void S_Chan_Update(sfx_channel_t *chan);
uint16_t S_Chan_GetPosition(sfx_channel_t *src);
int8_t S_Chan_BackBuffer(sfx_channel_t *chan);
int8_t S_Chan_StartBlock(sfx_channel_t *chan);
uint8_t S_Chan_MidiPan(uint8_t pan);

int S_AllocChannel(void);

#ifdef __cplusplus
}
#endif

#endif
