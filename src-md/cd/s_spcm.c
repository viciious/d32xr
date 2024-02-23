#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "hw_md.h"
#include "cdfh.h"
#include "pcm.h"
#include "s_channels.h"

#define SPCM_SAMPLE_RATE       21500

#define SPCM_LEFT_CHANNEL_ID   (S_MAX_CHANNELS)

#define SPCM_BUF_NUM_SECTORS   5
#define SPCM_BUF_SIZE          (SPCM_BUF_NUM_SECTORS*2048)
#define SPCM_NUM_BUFFERS       5

#define SPCM_TIMER_BPM         3600 /* 60 Hz */
#define SPCM_WAIT_TICS         23 /* 23*1000/60 = ~383 ms */

// start at 12KiB offset in PCM RAM
#define SPCM_LEFT_CHAN_SOFFSET  0x3000

enum
{
    SPCM_STATE_PLAYING,
    SPCM_STATE_WAIT_DATA,
    SPCM_STATE_WAIT_BUF,
    SPCM_STATE_PAINT,
    SPCM_STATE_STOPPED,
};

typedef struct
{
    int block;
    int start_block;
    int final_block;
    unsigned data_tic;
    uint16_t startpos;
    uint16_t looppos;
    uint8_t num_channels;
    uint8_t env;
    uint8_t frontbuf;
    uint8_t chan_id;
    uint8_t playing;
    uint8_t sector_num, sector_cnt;
    uint8_t state;
    uint8_t repeat;
} s_spcm_t;

static s_spcm_t track = { 0 };

static volatile unsigned spcm_tic = 0;

// buffer currently being played back by hardware
int8_t S_SPCM_FrontBuffer(s_spcm_t *spcm) {
    volatile uint8_t *ptr = PCM_RAMPTR + ((spcm->chan_id) << 2);
    uint16_t hi = *(ptr + 2); // MSB of PCM RAM location
    uint16_t lo = *(ptr + 0); // MSB of PCM RAM location

    uint16_t pos = (hi << 8) | lo;
    pos = (pos - SPCM_LEFT_CHAN_SOFFSET) / SPCM_BUF_SIZE;
    if (pos > SPCM_NUM_BUFFERS-1)
        pos = SPCM_NUM_BUFFERS-1;

    return pos;
}

void S_SPCM_UpdateChannel(s_spcm_t *spcm)
{
    int chan_id;

    chan_id = spcm->chan_id;

    // update channel parameters on the ricoh chip
    pcm_set_ctrl(0xC0 + chan_id);

    if (!spcm->playing) {
        pcm_set_env(0);
        return;
    }

    if (!pcm_is_off(chan_id)) {
        pcm_set_env(spcm->env);
        return;
    }

    // kick off playback
    pcm_set_env(spcm->env);

    pcm_set_freq(SPCM_SAMPLE_RATE);

    PCM_PAN = 0xff;
    pcm_delay();

    pcm_set_start(spcm->startpos>>8, 0);

    pcm_set_loop(spcm->looppos);

    pcm_set_on(chan_id);
}

void S_SPCM_BeginRead(s_spcm_t *spcm)
{
    int cnt;
    
    cnt = SPCM_BUF_NUM_SECTORS;
    if (cnt + spcm->block > spcm->final_block)
        cnt = spcm->final_block - spcm->block;

    begin_read_cd(spcm->block, cnt);
    spcm->sector_num = 0;
    spcm->sector_cnt = cnt;
}

static int S_SPCM_DMA(s_spcm_t *spcm)
{
    int doff, woff;
    uint8_t *pcm;
    int offset;

    offset = spcm->frontbuf * SPCM_BUF_SIZE;
    offset += spcm->sector_num * 2048;

    doff = SPCM_LEFT_CHAN_SOFFSET + offset;
    woff = doff;
    woff &= 0x0FFF;
    pcm = (uint8_t *)(woff << 1);

    pcm_set_ctrl(0x80 + (doff >> 12)); // make sure PCM chip is ON to write wave memory, and set wave bank
    return read_cd_pcm(pcm);
}

void S_SPCM_UpdateTrack(s_spcm_t *spcm)
{
    extern uint16_t CDA_VOLUME;

    if (spcm->num_channels == 0) {
        return;
    }

    spcm->env = (CDA_VOLUME > 1020 ? 1020 : CDA_VOLUME) / 4;
    S_SPCM_UpdateChannel(spcm);

    switch (spcm->state)
    {
    case SPCM_STATE_PLAYING:
        if (!spcm->playing) {
            pcm_set_off(spcm->chan_id);
            spcm->state = SPCM_STATE_STOPPED;
            return;
        }

        S_SPCM_BeginRead(spcm);
        spcm->frontbuf = S_SPCM_FrontBuffer(spcm);
        spcm->data_tic = spcm_tic;
        spcm->state = SPCM_STATE_WAIT_DATA;
        break;

    case SPCM_STATE_WAIT_DATA:
        if (spcm_tic < spcm->data_tic + SPCM_WAIT_TICS) {
            break;
        }

        spcm->state = SPCM_STATE_WAIT_BUF;
        break;

    case SPCM_STATE_WAIT_BUF:
        if (spcm->playing) {
            if (S_SPCM_FrontBuffer(spcm) == spcm->frontbuf) {
                break;
            }
        }

        spcm->state = SPCM_STATE_PAINT;
        break;

    case SPCM_STATE_PAINT:
        if (!S_SPCM_DMA(spcm))
            break;

        spcm->block++;
        if (spcm->block > spcm->final_block)
        {
            if (spcm->repeat)
            {
                spcm->block = spcm->start_block;
                spcm->state = SPCM_STATE_PLAYING;
            }
            else
            {
                spcm->playing = 0;
            }
        }
        else if (++spcm->sector_num == spcm->sector_cnt)
        {
            spcm->state = SPCM_STATE_PLAYING;
        }
        break;

    default:
        break;
    }
}

void S_SPCM_Suspend(void)
{
    s_spcm_t *spcm = &track;

    if (!spcm->playing) {
        return;
    }
    if (spcm->state == SPCM_STATE_STOPPED) {
        return;
    }
    if (spcm->num_channels == 0) {
        return;
    }

    spcm->playing = 0;
    while (spcm->state != SPCM_STATE_STOPPED) {
        S_SPCM_UpdateTrack(spcm);
    }

    pcm_load_zero(SPCM_LEFT_CHAN_SOFFSET, SPCM_BUF_SIZE*SPCM_NUM_BUFFERS);
}

void S_SPCM_Unsuspend(void)
{
    s_spcm_t *spcm = &track;

    if (spcm->playing) {
        return;
    }
    if (spcm->state != SPCM_STATE_STOPPED) {
        return;
    }
    if (spcm->num_channels == 0) {
        return;
    }

    spcm->playing = 1;
    spcm->state = SPCM_STATE_PLAYING;

    pcm_load_zero(SPCM_LEFT_CHAN_SOFFSET, SPCM_BUF_SIZE*SPCM_NUM_BUFFERS);
    pcm_loop_markers(SPCM_LEFT_CHAN_SOFFSET + SPCM_BUF_SIZE*SPCM_NUM_BUFFERS);

    // start the playback, otherwise DMA won't work
    S_SPCM_UpdateTrack(spcm);
}

void S_SPCM_Update(void)
{
    S_SPCM_UpdateTrack(&track);
}

void S_SPCM_Tick(void)
{
    spcm_tic++;
}

int S_SCM_PlayTrack(const char *name, int repeat)
{
    int32_t length, offset;
    int64_t lo;
    extern uint8_t DISC_BUFFER[2048];
    uint8_t *header = DISC_BUFFER;
    s_spcm_t *spcm = &track; 

    lo = open_file(name);
    if (lo < 0)
        return lo;
    offset = lo & 0x7fffffff;
    length = (lo >> 32) & 0x7fffffff;

    read_sectors(header, offset, 1);

    spcm->num_channels = 1;
    spcm->chan_id = SPCM_LEFT_CHANNEL_ID;
    spcm->env = 255;
    spcm->start_block = offset + 1;
    spcm->block = spcm->start_block;
    spcm->final_block = spcm->start_block + (length>>11) - 2;
    spcm->frontbuf = 0xff;
    spcm->sector_cnt = 0;
    spcm->sector_num = 0;
    spcm->playing = 0;
    spcm->state = SPCM_STATE_STOPPED;
    spcm->startpos = SPCM_LEFT_CHAN_SOFFSET;
    spcm->looppos = SPCM_LEFT_CHAN_SOFFSET;
    spcm->repeat = repeat;

    pcm_set_timer(SPCM_TIMER_BPM); // 60Hz

    pcm_start_timer(S_SPCM_Tick);

    S_SPCM_Unsuspend();

    return spcm->num_channels;
}

void S_SPCM_StopTrack(void)
{
    S_SPCM_Suspend();
}
