#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "hw_md.h"
#include "cdfh.h"
#include "pcm.h"
#include "s_channels.h"

/* page 9 of https://segaretro.org/images/2/2d/MCDHardware_Manual_PCM_Sound_Source.pdf */
/* or page 55 of https://segaretro.org/images/2/2e/Sega-CD_Technical_Bulletins.pdf */
#define SPCM_RF5C164_BASEFREQ   32604 /* should not be modified */

#define SPCM_RF5C164_INCREMENT  0x0800 /* equivalent to the sample rate of 32604 Hz */

//#define SPCM_SAMPLE_RATE      (SPCM_RF5C164_INCREMENT * SPCM_RF5C164_BASEFREQ / 2048) /* 32604 */

#define SPCM_BUF_MIN_SECTORS    4 /* wait this many sectors in the playback buffer before starting a new read */
#define SPCM_BUF_NUM_SECTORS    12
#define SPCM_BUF_SIZE           (SPCM_BUF_NUM_SECTORS*2048)  /* 13*2048*1000/32604 = ~816ms */
#define SPCM_NUM_BUFFERS        1

// start at 10KiB offset in PCM RAM - must be changed if S_MAX_CHANNELS is greater than 6!
#define SPCM_CHAN_BUF_SIZE      (SPCM_NUM_BUFFERS*SPCM_BUF_SIZE)
#define SPCM_LEFT_CHANNEL_ID    (S_MAX_CHANNELS)
#define SPCM_LEFT_CHAN_SOFFSET  0x2800
#define SPCM_RIGHT_CHAN_SOFFSET (SPCM_LEFT_CHAN_SOFFSET+SPCM_CHAN_BUF_SIZE+2048)
#define SPCM_RIGHT_CHANNEL_ID   (SPCM_LEFT_CHANNEL_ID+1)

#define SPCM_MAX_WAIT_TICS      200 // 3.3s on NTSC, 4s on PAL

enum
{
    SPCM_STATE_INIT,
    SPCM_STATE_PREPAINT,
    SPCM_STATE_START,

    SPCM_STATE_PAINT,
    SPCM_STATE_PLAYING,

    SPCM_STATE_STOPPED,
    SPCM_STATE_RESUME
};

typedef struct
{
    int block;
    int start_block;
    int final_block;
    volatile uint32_t tics;
    volatile uint32_t lastdmatic;
    uint8_t num_channels;
    uint8_t env;
    struct {
        uint8_t id;
        uint8_t pan;
        uint16_t startpos;
        uint16_t looppos;
        uint16_t lastmixpos;
    } chan[2];
    volatile uint8_t playing;
    uint8_t sector_num, sector_cnt;
    volatile uint8_t state;
    uint8_t repeat;
} s_spcm_t;

static s_spcm_t track = { 0 };
static uint8_t safeguard[0x10000 - (SPCM_RIGHT_CHAN_SOFFSET+SPCM_CHAN_BUF_SIZE+2048)] __attribute__((unused));

// buffer position currently being played back by hardware
static uint16_t S_SPCM_MixerPos(s_spcm_t *spcm, int chan) {
    volatile uint8_t *ptr = PCM_RAMPTR + ((spcm->chan[chan].id) << 2);
    uint16_t hi = *(ptr + 2); // MSB of PCM RAM location
    uint16_t lo = *(ptr + 0); // MSB of PCM RAM location
    return ((hi << 8) | lo) - spcm->chan[chan].startpos;
}

void S_SPCM_UpdateChannel(s_spcm_t *spcm, int chan)
{
    int chan_id;

    chan_id = spcm->chan[chan].id;

    // update channel parameters on the ricoh chip
    pcm_set_ctrl(0xC0 + chan_id);

    if (!spcm->playing) {
        pcm_set_env(0);
        return;
    }

    pcm_set_env(spcm->env);

    if (!pcm_is_off(chan_id)) {
        return;
    }

    // kick off playback
    PCM_FDL = (SPCM_RF5C164_INCREMENT >> 0) & 0xff;
    pcm_delay();
    PCM_FDH = (SPCM_RF5C164_INCREMENT >> 8) & 0xff;
    pcm_delay();

    PCM_PAN = spcm->chan[chan].pan;
    pcm_delay();

    pcm_set_start(spcm->chan[chan].startpos>>8, 0);

    pcm_set_loop(spcm->chan[chan].looppos);
}

void S_SPCM_BeginRead(s_spcm_t *spcm)
{
    int cnt;
    
    cnt = SPCM_BUF_NUM_SECTORS*2;
    if (cnt + spcm->block > spcm->final_block + 1)
        cnt = spcm->final_block - spcm->block + 1;

    begin_read_cd(spcm->block, cnt);
    spcm->sector_num = 0;
    spcm->sector_cnt = cnt;
    spcm->lastdmatic = spcm->tics;
}

static int S_SPCM_DMA(s_spcm_t *spcm, uint16_t doff, uint16_t offset)
{
    int woff;
    uint8_t *pcm;

    doff += offset;
    woff = doff;
    woff &= 0x0FFF;
    pcm = (uint8_t *)(woff << 1);

    pcm_set_ctrl(0x80 + (doff >> 12)); // make sure PCM chip is ON to write wave memory, and set wave bank
    return dma_cd_sector_pcm(pcm);
}

void S_SPCM_UpdateTrack(s_spcm_t *spcm)
{
    volatile int i;
    int next_state;
    extern volatile uint16_t CDA_VOLUME;

    if (spcm->num_channels == 0) {
        return;
    }

    spcm->env = (CDA_VOLUME >= 1020 ? 255 : CDA_VOLUME / 4);
 
    for (i = 0; i < 2; i++) {
        S_SPCM_UpdateChannel(spcm, i);
    }

    switch (spcm->state)
    {
    case SPCM_STATE_INIT:
        S_SPCM_BeginRead(spcm);
        spcm->state = SPCM_STATE_PREPAINT;
        break;

    case SPCM_STATE_START:
        spcm->playing = 1;

        for (i = 0; i < 2; i++) {
            S_SPCM_UpdateChannel(spcm, i);
        }

        for (i = 0; i < 2; i++) {
            pcm_loop_markers(spcm->chan[i].startpos + SPCM_CHAN_BUF_SIZE);
        }
        for (i = 0; i < 2; i++) {
            pcm_set_on(spcm->chan[i].id);
        }
        spcm->state = SPCM_STATE_PLAYING;
        break;

    case SPCM_STATE_PLAYING:
        if (!spcm->playing) {
            for (i = 0; i < 2; i++) {
                pcm_set_off(spcm->chan[i].id);
            }
            spcm->state = SPCM_STATE_STOPPED;
            break;
        }
    case SPCM_STATE_RESUME:
        // start the playback, otherwise DMA won't work
        if (pcm_is_off(spcm->chan[0].id)) {
            for (i = 0; i < 2; i++) {
                pcm_load_zero(spcm->chan[i].startpos, SPCM_CHAN_BUF_SIZE);
                pcm_loop_markers(spcm->chan[i].startpos + SPCM_CHAN_BUF_SIZE);
            }

            for (i = 0; i < 2; i++) {
                pcm_set_on(spcm->chan[i].id);
            }
        }

        for (i = 0; i < 2; i++) {
            spcm->chan[i].lastmixpos = S_SPCM_MixerPos(spcm, i);
        }
        for (i = 0; i < 2; i++) {
            if (spcm->chan[i].lastmixpos < SPCM_BUF_MIN_SECTORS * 2048) {
                return;
            }
        }

        S_SPCM_BeginRead(spcm);
        spcm->state = SPCM_STATE_PAINT;
        break;

    case SPCM_STATE_PAINT:
    case SPCM_STATE_PREPAINT:
        if (!spcm->sector_cnt) {
            goto done;
        }

        while (1) {
            uint16_t mixpos;
            uint16_t chan, offset;

            chan = spcm->sector_num & 1;
            offset = (spcm->sector_num/2) * 2048;

            switch (spcm->state) {
            case SPCM_STATE_PREPAINT:
                next_state = SPCM_STATE_START;
                break;
            case SPCM_STATE_PAINT:
                next_state = SPCM_STATE_PLAYING;

                mixpos = S_SPCM_MixerPos(spcm, 0);
                if (spcm->chan[0].lastmixpos < mixpos)
                {
                    spcm->chan[0].lastmixpos = mixpos;
                    if (mixpos < offset + 2048) {
                        return;
                    }
                }
                else
                {
                    // playback buffer has wrapped around, it's safe to DMA
                }
            }

            if (!S_SPCM_DMA(spcm, spcm->chan[chan].startpos, offset))
                break;

skipblock:
            spcm->lastdmatic = spcm->tics;
            if (++spcm->block > spcm->final_block) {
done:
                if (spcm->repeat) {
                    spcm->block = spcm->start_block;
                    spcm->state = next_state;
                    break;
                }
                else {
                    spcm->playing = 0;
                    spcm->state = SPCM_STATE_PLAYING;
                    break;
                }
            }
            else if (++spcm->sector_num >= spcm->sector_cnt) {
                spcm->state = next_state;
                break;
            }
        }

        if (spcm->tics - spcm->lastdmatic > SPCM_MAX_WAIT_TICS) {
            goto skipblock;
        }
        break;
    default:
        break;
    }
}

void S_SPCM_Suspend(void)
{
    uint32_t waitstart;
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
    waitstart = spcm->tics;
    while (spcm->state != SPCM_STATE_STOPPED) {
        if (spcm->tics - waitstart > SPCM_MAX_WAIT_TICS) {
            // don't wait indefinitely
            break;
        }
        S_SPCM_UpdateTrack(spcm);
    }
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
    spcm->state = SPCM_STATE_RESUME;
}

void S_SPCM_Update(void)
{
    int oldctl;
    s_spcm_t *spcm = &track;

    if (spcm->num_channels == 0) {
        return;
    }

    spcm->tics++;

    if (!spcm->playing) {
        return;
    }

    // preserve and restore global PCM control state
    oldctl = pcm_get_ctrl();

    S_SPCM_UpdateTrack(spcm);

    pcm_set_ctrl(oldctl);
}

int S_SCM_PlayTrack(const char *name, int repeat)
{
    int32_t length, offset;
    int64_t lo;
    extern uint8_t DISC_BUFFER[2048];
    uint8_t *header = DISC_BUFFER;
    s_spcm_t *spcm = &track;
    uint32_t waitstart;

    lo = open_file(name);
    if (lo < 0)
        return lo;
    offset = lo & 0x7fffffff;
    length = (lo >> 32) & 0x7fffffff;

    read_sectors(header, offset, 1);

    spcm->num_channels = 1;
    spcm->env = 255;
    spcm->start_block = offset + 1;
    spcm->block = spcm->start_block;
    spcm->final_block = offset + (length>>11) - 2; // minus the header and last padding sector
    spcm->sector_cnt = 0;
    spcm->sector_num = 0;
    spcm->playing = 0;
    spcm->chan[0].id = SPCM_LEFT_CHANNEL_ID;
    spcm->chan[0].startpos = SPCM_LEFT_CHAN_SOFFSET;
    spcm->chan[0].looppos = SPCM_LEFT_CHAN_SOFFSET;
    spcm->chan[0].pan = 0b00001111;
    spcm->chan[0].lastmixpos = 0;
    spcm->chan[1].id = SPCM_RIGHT_CHANNEL_ID;
    spcm->chan[1].startpos = SPCM_RIGHT_CHAN_SOFFSET;
    spcm->chan[1].looppos = SPCM_RIGHT_CHAN_SOFFSET;
    spcm->chan[1].pan = 0b11110000;
    spcm->chan[1].lastmixpos = 0;
    spcm->repeat = repeat;
    spcm->tics = 0;
    spcm->lastdmatic = 0;
    spcm->state = SPCM_STATE_INIT;

    waitstart = spcm->tics;
    while (spcm->state != SPCM_STATE_PLAYING) {
        if (spcm->tics - waitstart > SPCM_MAX_WAIT_TICS) {
            // don't wait indefinitely
            break;
        }
        S_SPCM_UpdateTrack(spcm);
    }

    return spcm->num_channels;
}

void S_SPCM_StopTrack(void)
{
    s_spcm_t *spcm = &track; 
    S_SPCM_Suspend();
    spcm->num_channels = 0;
}
