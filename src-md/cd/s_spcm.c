#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "hw_md.h"
#include "cdfh.h"
#include "pcm.h"
#include "s_channels.h"
#include "s_spcm.h"

/* page 9 of https://segaretro.org/images/2/2d/MCDHardware_Manual_PCM_Sound_Source.pdf */
/* or page 55 of https://segaretro.org/images/2/2e/Sega-CD_Technical_Bulletins.pdf */
#define SPCM_RF5C164_BASEFREQ   32604 /* should not be modified */

#define SPCM_RF5C164_DEFAULT_INCREMENT  0x0800 /* equivalent to the sample rate of 32604 Hz */

//#define SPCM_SAMPLE_RATE      (SPCM_RF5C164_INCREMENT * SPCM_RF5C164_BASEFREQ / 2048) /* 24453 */

#define SPCM_BUF_MIN_SECTORS    3 /* the playback buffer needs to be this many sectors before a new CD read is started */
#define SPCM_BUF_NUM_SECTORS    12

// start at 10KiB offset in PCM RAM - must be changed if S_MAX_CHANNELS is greater than 6!
#define SPCM_CHAN_BUF_SIZE      (SPCM_BUF_NUM_SECTORS*2048)  /* 12*2048*1000/27500 = ~744ms */
#define SPCM_LEFT_CHANNEL_ID    (S_MAX_CHANNELS)
#define SPCM_LEFT_CHAN_SOFFSET  0x2800
#define SPCM_RIGHT_CHAN_SOFFSET (SPCM_LEFT_CHAN_SOFFSET+SPCM_CHAN_BUF_SIZE+2048)
#define SPCM_RIGHT_CHANNEL_ID   (SPCM_LEFT_CHANNEL_ID+1)

#define SPCM_MAX_INIT_WAIT_TICS 130 // x7.8ms = ~1s seconds
#define SPCM_MAX_PLAY_WAIT_TICS 260 // x7.8ms = ~2s seconds

#define SPCM_AUX_BUFFER_SECTORS 4
#define SPCM_AUX_BUFFER_CHUNK   512 // 2048/512x7.8ms = 31,2ms
#define SPCM_AUX_BUFFER_MASK    (SPCM_AUX_BUFFER_SECTORS-1)

enum
{
    SPCM_STATE_INIT,

    SPCM_STATE_PREPAINT,
    SPCM_STATE_START, // must be SPCM_STATE_PREPAINT+1

    SPCM_STATE_PAINT,
    SPCM_STATE_PLAYING, // must be SPCM_STATE_PAINT+1

    SPCM_STATE_STOPPING,
    SPCM_STATE_STOPPED,

    SPCM_STATE_RESUME
};

typedef struct
{
    int block, next_block;
    int start_block;
    int final_block;
    struct {
        int paint_sector;
        int painted_sectors_int;
        uint16_t paint_offset;
        uint16_t painted_sectors_frac;
        uint16_t lastpos;
        uint8_t paint_chan;
    } mix;
    volatile uint32_t tics;
    volatile uint32_t lastdmatic;
    uint16_t increment;
    uint8_t num_channels;
    uint8_t chan_sectors;
    uint16_t chan_samples;
    uint8_t env;
    struct {
        uint8_t id;
        uint8_t pan;
        uint16_t startpos;
    } chan[2];
    volatile uint8_t playing;
    uint8_t sector_cnt;
    uint8_t ramsector_wcnt, ramsector_rcnt;
    uint16_t ramsector_roffset;
    volatile uint8_t state;
    uint8_t repeat, quickrepeat;
    int16_t maxwait;
} s_spcm_t;

static s_spcm_t track = { 0 };
static uint8_t safeguard[0x10000 - (SPCM_RIGHT_CHAN_SOFFSET+SPCM_CHAN_BUF_SIZE+2048)] __attribute__((unused));

void S_SPCM_Update(s_spcm_t *spcm);

// buffer position currently being played back by hardware
static uint16_t S_SPCM_MixerPos_(s_spcm_t *spcm, int chan) {
    volatile uint8_t *ptr = PCM_RAMPTR + ((spcm->chan[chan].id) << 2);
    uint16_t hi = *(ptr + 2); // MSB of PCM RAM location
    uint16_t lo = *(ptr + 0); // LSB of PCM RAM location
    return ((hi << 8) | lo) - spcm->chan[chan].startpos;
}

static uint16_t S_SPCM_MixerPos(s_spcm_t *spcm, int chan) {
    volatile uint16_t mixpos;

    while (1) {
        mixpos = S_SPCM_MixerPos_(spcm, chan);
        if (S_SPCM_MixerPos_(spcm, chan) == mixpos) {
            break;
        }
    }

    return mixpos;
}

static int S_SPCM_PaintedSectors(s_spcm_t *spcm) {
    uint16_t mixpos;

    mixpos = S_SPCM_MixerPos(spcm, 0);
    if (mixpos >= spcm->chan_samples) {
        mixpos = 0;
    }

    if (spcm->mix.lastpos > mixpos) {
        // wrapped
        spcm->mix.painted_sectors_int += spcm->chan_sectors;
    }
    spcm->mix.lastpos = mixpos;
    spcm->mix.painted_sectors_frac = mixpos >> 11;

    return spcm->mix.painted_sectors_int + spcm->mix.painted_sectors_frac;
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
    PCM_FDL = (spcm->increment >> 0) & 0xff;
    pcm_delay();
    PCM_FDH = (spcm->increment >> 8) & 0xff;
    pcm_delay();

    PCM_PAN = spcm->chan[chan].pan;
    pcm_delay();

    pcm_set_start(spcm->chan[chan].startpos>>8, 0);

    pcm_set_loop(spcm->chan[chan].startpos);
}

static void S_SPCM_BeginRead(s_spcm_t *spcm)
{
    int cnt;

    // a continuous mono stream or interleaved stereo
    cnt = SPCM_BUF_NUM_SECTORS * 2;
    if (cnt + spcm->block > spcm->final_block)
        cnt = spcm->final_block - spcm->block;

    spcm->ramsector_wcnt = spcm->ramsector_rcnt = 0;
    spcm->ramsector_roffset = 0;
    spcm->sector_cnt = cnt;
    spcm->lastdmatic = spcm->tics;
    spcm->next_block = spcm->block + cnt;

    if (cnt > 0)
        begin_read_cd(spcm->block, cnt);
}

static int S_SPCM_PCM_DMA(s_spcm_t *spcm, uint16_t doff, uint16_t offset)
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
    int i;
    uint16_t chan, offset;
    int painted_sectors;
    int delay_sectors;
    extern uint16_t CDA_VOLUME;
    extern uint8_t DISC_BUFFER[2048*4];

    if (spcm->num_channels == 0) {
        return;
    }

    spcm->env = (CDA_VOLUME >= 1020 ? 255 : CDA_VOLUME / 4);
 
    for (i = 0; i < spcm->num_channels; i++) {
        S_SPCM_UpdateChannel(spcm, i);
    }

    painted_sectors = S_SPCM_PaintedSectors(spcm);

    switch (spcm->state)
    {
    case SPCM_STATE_INIT:
        S_SPCM_BeginRead(spcm);
        spcm->state = SPCM_STATE_PREPAINT;
        break;

    case SPCM_STATE_START:
        spcm->playing = 1;

        for (i = 0; i < spcm->num_channels; i++) {
            S_SPCM_UpdateChannel(spcm, i);
        }

        for (i = 0; i < spcm->num_channels; i++) {
            pcm_loop_markers(spcm->chan[i].startpos + spcm->chan_samples);
        }
        for (i = 0; i < spcm->num_channels; i++) {
            pcm_set_on(spcm->chan[i].id);
        }

        memset(&spcm->mix, 0, sizeof(spcm->mix));
        spcm->state = SPCM_STATE_PLAYING;
        break;

    case SPCM_STATE_PLAYING:
        if (!spcm->playing) {
            for (i = 0; i < spcm->num_channels; i++) {
                pcm_set_off(spcm->chan[i].id);
            }
            spcm->state = SPCM_STATE_STOPPED;
            break;
        }
    case SPCM_STATE_RESUME:
        // start the playback
        if (pcm_is_off(spcm->chan[0].id)) {
            for (i = 0; i < spcm->num_channels; i++) {
                pcm_load_zero(spcm->chan[i].startpos, spcm->chan_samples);
                pcm_loop_markers(spcm->chan[i].startpos + spcm->chan_samples);
            }
            for (i = 0; i < spcm->num_channels; i++) {
                pcm_set_on(spcm->chan[i].id);
            }
        }

        delay_sectors = SPCM_BUF_MIN_SECTORS;
        delay_sectors += (spcm->ramsector_wcnt > 2);
        delay_sectors += (spcm->ramsector_wcnt > 4);
        if (delay_sectors > SPCM_AUX_BUFFER_SECTORS)
            delay_sectors = SPCM_AUX_BUFFER_SECTORS;

        if (spcm->increment <= 0x600) // 24453 Hz
            delay_sectors++;
        if (spcm->increment <= 0x500) // 20378 Hz
            delay_sectors++;
        if (spcm->increment <= 0x400) // 16302 Hz
            delay_sectors++;

        if (spcm->num_channels == 1)
            delay_sectors += SPCM_BUF_NUM_SECTORS; // wait 1/2 of playback time

        if (painted_sectors < spcm->mix.paint_sector + delay_sectors) {
            seek_cd(spcm->block);
            return;
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
paint_loop:
            chan = spcm->mix.paint_chan;
            offset = spcm->mix.paint_offset;

            if (chan == 0 && spcm->playing) {
                switch (spcm->state) {
                case SPCM_STATE_PAINT:
                    painted_sectors = S_SPCM_PaintedSectors(spcm);
                    if (painted_sectors <= spcm->mix.paint_sector) {
                        // soak up to 4 sectors into PRG RAM to deal with data spikes
                        if (dma_cd_sector_prg(&DISC_BUFFER[(spcm->ramsector_wcnt&SPCM_AUX_BUFFER_MASK)*2048])) {
                            spcm->lastdmatic = spcm->tics;
                            spcm->ramsector_wcnt++;
                            goto paint_loop;
                        }
                        goto check_timeout;
                    }
                }
            }

            if (spcm->ramsector_rcnt < spcm->ramsector_wcnt) {
                const int chunk_size = SPCM_AUX_BUFFER_CHUNK;

                pcm_load_samples(spcm->chan[chan].startpos + offset + spcm->ramsector_roffset, 
                    &DISC_BUFFER[(spcm->ramsector_rcnt&SPCM_AUX_BUFFER_MASK)*2048 + spcm->ramsector_roffset], chunk_size);

                spcm->lastdmatic = spcm->tics;
                spcm->ramsector_roffset += chunk_size;
                if (spcm->ramsector_roffset < 2048) {
                    break;
                }
                spcm->ramsector_roffset = 0;
                spcm->ramsector_rcnt++;
            }
            else {
                if (!S_SPCM_PCM_DMA(spcm, spcm->chan[chan].startpos, offset)) {
                    break;
                }
            }

            spcm->mix.paint_chan++;
            spcm->mix.paint_chan &= (spcm->num_channels-1);

            if (spcm->mix.paint_chan == 0) {
                spcm->mix.paint_sector++;
                spcm->mix.paint_offset += 2048;
                if (spcm->mix.paint_offset >= spcm->chan_samples) {
                    spcm->mix.paint_offset = 0;
                }
            }

            spcm->lastdmatic = spcm->tics;
            if (++spcm->block >= spcm->final_block) {
done:
                if (spcm->repeat && spcm->quickrepeat) {
                    spcm->next_block = spcm->start_block;
                    spcm->block = spcm->start_block;
                    spcm->state++;
                    seek_cd(spcm->block);
                    break;
                }
                else {
                    spcm->sector_cnt = spcm->chan_sectors;
                    spcm->state = SPCM_STATE_STOPPING;
                    break;
                }
            }
            else if (--spcm->sector_cnt == 0) {
post_paint:
                spcm->lastdmatic = spcm->tics;
                spcm->block = spcm->next_block;
                spcm->maxwait = SPCM_MAX_PLAY_WAIT_TICS;
                spcm->state++;
                break;
            }
        }

check_timeout:
        if (spcm->tics - spcm->lastdmatic > spcm->maxwait) {
            for (i = 0; i < spcm->num_channels; i++) {
                pcm_set_off(spcm->chan[i].id);
            }
            memset(&spcm->mix, 0, sizeof(spcm->mix));
            goto post_paint;
        }
        break;

    case SPCM_STATE_STOPPING:
        if (!spcm->playing) {
            spcm->state = SPCM_STATE_PLAYING;
            break;
        }

        if (painted_sectors < spcm->mix.paint_sector + SPCM_BUF_MIN_SECTORS) {
            return;
        }

        offset = spcm->mix.paint_offset;
        painted_sectors = S_SPCM_PaintedSectors(spcm);
        if (painted_sectors <= spcm->mix.paint_sector) {
            return;
        }

        for (i = 0; i < spcm->num_channels; i++) {
            pcm_load_zero(spcm->chan[i].startpos + offset, 2048);
        }

        spcm->mix.paint_sector++;
        spcm->mix.paint_offset += 2048;
        if (spcm->mix.paint_offset >= spcm->chan_samples) {
            spcm->mix.paint_offset = 0;
        }

        if (--spcm->sector_cnt == 0) {
            for (i = 0; i < spcm->num_channels; i++) {
                pcm_set_off(spcm->chan[i].id);
            }
            memset(&spcm->mix, 0, sizeof(spcm->mix));
            if (spcm->repeat && spcm->playing)
            {
                spcm->block = spcm->start_block;
                spcm->state = SPCM_STATE_INIT;
                break;
            }
            spcm->playing = 0;
            spcm->state = spcm->repeat ? SPCM_STATE_INIT : SPCM_STATE_STOPPED;
            break;
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

    while (!spcm->playing) {
        if (spcm->state == SPCM_STATE_STOPPED) {
            return;
        }
    }

    spcm->playing = 0;
    while (spcm->state != SPCM_STATE_STOPPED) {
        S_SPCM_UpdateTrack(spcm);
    }

    pcm_stop_timer();
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

    memset(&spcm->mix, 0, sizeof(spcm->mix));
    spcm->maxwait = SPCM_MAX_INIT_WAIT_TICS;
    spcm->state = SPCM_STATE_RESUME;
    spcm->playing = 1;

    pcm_start_timer((void (*)(void *))S_SPCM_Update, spcm);
}

void S_SPCM_Update(s_spcm_t *spcm)
{
    int oldctl;

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
    extern uint8_t DISC_BUFFER[2048*4];
    uint8_t *header = DISC_BUFFER;
    s_spcm_t *spcm = &track;

    lo = open_file(name);
    if (lo < 0)
        return lo;
    offset = lo & 0x7fffffff;
    length = (lo >> 32) & 0x7fffffff;

    read_sectors(header, offset, 1);

    memset(spcm, 0, sizeof(*spcm));
    spcm->num_channels = header[8];
    spcm->start_block = offset + 1;
    spcm->next_block = spcm->start_block;
    spcm->block = spcm->start_block;
    spcm->final_block = offset + (length>>11) - 1; // ignore the last padding sector
    spcm->chan_sectors = SPCM_BUF_NUM_SECTORS;
    spcm->chan_samples = SPCM_CHAN_BUF_SIZE;
    spcm->chan[0].id = SPCM_LEFT_CHANNEL_ID;
    spcm->chan[0].startpos = SPCM_LEFT_CHAN_SOFFSET;
    spcm->chan[0].pan = 0b00001111;
    spcm->chan[1].id = SPCM_RIGHT_CHANNEL_ID;
    spcm->chan[1].startpos = SPCM_RIGHT_CHAN_SOFFSET;
    spcm->chan[1].pan = 0b11110000;
    spcm->repeat = repeat;
    spcm->state = SPCM_STATE_INIT;
    spcm->maxwait = SPCM_MAX_INIT_WAIT_TICS;
    spcm->increment = (header[6] << 8) | header[7];
    spcm->env = header[9];

    if (spcm->increment == 0) {
        spcm->increment = SPCM_RF5C164_DEFAULT_INCREMENT;
    }
    if (spcm->num_channels == 0) {
        spcm->num_channels = 2;
    }
    if (spcm->num_channels == 1) {
        spcm->chan[0].pan = 0b11111111;
    }
    if (spcm->env == 0) {
        spcm->env = 0xff;
    }
    if (spcm->num_channels == 1) {
        spcm->chan_sectors += spcm->chan_sectors;
        spcm->chan_samples += spcm->chan_samples;
    }

    pcm_start_timer((void (*)(void *))S_SPCM_Update, spcm);

    while (spcm->state != SPCM_STATE_PLAYING) {
        if (spcm->state == SPCM_STATE_STOPPED) {
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
