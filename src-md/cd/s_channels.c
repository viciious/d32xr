#include "s_channels.h"
#include "pcm.h"

sfx_channel_t s_channels[ S_MAX_CHANNELS+1 ] = { { 0 } }; // 0 is a dummy channel

// PCM RAM location for the channel
volatile uint8_t *S_Chan_RAMPtr(sfx_channel_t *chan) {
    if (chan->id == 0) {
        return PCM_RAMPTR;
    }    
    return PCM_RAMPTR + ((chan->realid) << 2);
}

// buffer currently being played back by hardware
int8_t S_Chan_FrontBuffer(sfx_channel_t *chan) {
    if (chan->id == 0) {
        return 0;
    }
    if (pcm_is_off((chan->realid))) {
        return 1;
    }
    volatile uint8_t *ptr = S_Chan_RAMPtr(chan);
    uint16_t lo = *(ptr + 0); // LSB of PCM RAM location
    uint16_t hi = *(ptr + 2); // MSB of PCM RAM location
    if (chan->realid & 1)
    {
        if (CHBUF_SHIFT <= 8)
            return (lo & (1<<(CHBUF_SHIFT-1))) == 0;
        return (hi & (1<<(CHBUF_SHIFT-8))) == 0;
    }
    else
    {
        if (CHBUF_SHIFT <= 8)
            return (lo & (1<<(CHBUF_SHIFT-1))) != 0;
        return (hi & (1<<(CHBUF_SHIFT-8))) != 0;
    }
}

// buffer we can paint to
int8_t S_Chan_BackBuffer(sfx_channel_t *chan) {
    return S_Chan_FrontBuffer(chan) ^ 1;
}

int8_t S_Chan_StartBlock(sfx_channel_t *chan) {
    if (chan->id == 0) {
        return 0;
    }    
    return (chan->realid + chan->realid + chan->realid); // * 3
}

int8_t S_Chan_LoopBlock(sfx_channel_t *chan) {
    return S_Chan_StartBlock(chan) + 2; // double buffered
}

// the location that the channel is playing
uint16_t S_Chan_GetPosition(sfx_channel_t *chan)
{
    volatile uint8_t *ptr = S_Chan_RAMPtr(chan);
    uint16_t hi = *(ptr + 2);
    uint16_t lo = *(ptr    );
    return (hi << 8) | lo;
}

void S_Chan_Clear(sfx_channel_t *chan)
{
    if (chan->id == 0) {
        return;
    }
    chan->freq = 0;
    pcm_set_off(chan->realid);
}

void S_Chan_Init(sfx_channel_t *chan)
{
    if (chan->id == 0) {
        return;
    }    
    chan->freq = 0;
    pcm_loop_markers(CHBUF_POS(S_Chan_LoopBlock(chan)));
}

uint8_t S_Chan_MidiPan(uint8_t pan)
{
    uint8_t left, right;

    if (pan == 0) {
        // full left
        left = 0b00001111;
        right = 0;
    } else if (pan == 255) {
        // no panning
        left = 0b00001111;
        right = 0b11110000;
    } else {
        right = pan & 0xf0;
        left = (256 - pan) >> 4;
    }

    return right | left;
}

void S_Chan_Update(sfx_channel_t *chan)
{
    uint8_t startblock = S_Chan_StartBlock(chan);
    uint16_t startpos = CHBUF_POS(startblock); 

    if (chan->id == 0 || chan->freq == 0) {
        return;
    }

    // update channel parameters on the ricoh chip
    pcm_set_ctrl(0xC0 + chan->realid);

    if (!pcm_is_off(chan->realid)) {
        // keep playing, only update the volume and panning
        pcm_set_freq(chan->freq);

        pcm_set_env(chan->env);

        PCM_PAN = chan->pan;
        pcm_delay();
        return;
    }

    // kick off playback
    pcm_set_env(chan->env);

    pcm_set_freq(chan->freq);

    PCM_PAN = chan->pan;
    pcm_delay();

    pcm_set_start(startpos>>8, 0);

    pcm_set_loop(startpos);

    pcm_set_on(chan->realid);
}

int S_AllocChannel(void)
{
    int i;
    sfx_channel_t *chan;

    for (i = 0; i < S_MAX_CHANNELS; i++) {
        chan = &s_channels[ i + 1 ];
        if (chan->freq) {
            continue;
        }
        return i + 1;
    }
    return 0;
}

void S_InitChannels(void)
{
    int i;

    pcm_init();

    s_channels[0].id = 0;
    s_channels[0].realid = 0;

    for (i = 0; i < S_MAX_CHANNELS; i++) {
        s_channels[i+1].id = i+1;
        s_channels[i+1].realid = i;
        S_Chan_Init(&s_channels[ i+1 ]);
    }
}

void S_ClearChannels(void)
{
    int i;
    for (i = 0; i < S_MAX_CHANNELS; i++) {
        S_Chan_Clear(&s_channels[ i+1 ]);
    }    
}
