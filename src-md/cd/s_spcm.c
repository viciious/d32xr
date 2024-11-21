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

#define SPCM_BUF_MIN_SECTORS    4 /* the playback buffer needs to be this many sectors before a new CD read is started */
#define SPCM_BUF_NUM_SECTORS    12

// start at 10KiB offset in PCM RAM - must be changed if S_MAX_CHANNELS is greater than 6!
#define SPCM_CHAN_BUF_SIZE      (SPCM_BUF_NUM_SECTORS*2048)  /* 12*2048*1000/24453 = ~1005ms */
#define SPCM_LEFT_CHANNEL_ID    (S_MAX_CHANNELS)
#define SPCM_LEFT_CHAN_SOFFSET  0x2800
#define SPCM_RIGHT_CHAN_SOFFSET (SPCM_LEFT_CHAN_SOFFSET+SPCM_CHAN_BUF_SIZE+2048)
#define SPCM_RIGHT_CHANNEL_ID   (SPCM_LEFT_CHANNEL_ID+1)

#define SPCM_MAX_WAIT_TICS      400 // x7.8ms = ~3s seconds

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
    int block;
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
    uint8_t env;
    struct {
        uint8_t id;
        uint8_t pan;
        uint16_t startpos;
        uint16_t endpos;
        uint16_t looppos;
    } chan[2];
    volatile uint8_t playing;
    uint8_t sector_cnt;
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

static int S_SPCM_PaintedSectors(s_spcm_t *spcm) {
    uint16_t mixpos;

    mixpos = S_SPCM_MixerPos(spcm, 0);
    if (mixpos >= SPCM_CHAN_BUF_SIZE) {
        mixpos = SPCM_CHAN_BUF_SIZE - 1;
    }
    if (spcm->mix.lastpos > mixpos) {
        // wrapped
        spcm->mix.painted_sectors_int += SPCM_BUF_NUM_SECTORS;
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

    pcm_set_loop(spcm->chan[chan].looppos);
}

static void S_SPCM_BeginRead(s_spcm_t *spcm)
{
    int cnt;
    
    cnt = SPCM_BUF_NUM_SECTORS;
    if (spcm->num_channels == 2)
        cnt += cnt;
    if (cnt + spcm->block > spcm->final_block)
        cnt = spcm->final_block - spcm->block;

    spcm->sector_cnt = cnt;
    spcm->lastdmatic = spcm->tics;

    if (cnt > 0)
        begin_read_cd(spcm->block, cnt);
}

static void S_SPCM_Preseek(s_spcm_t *spcm)
{
    seek_cd(spcm->block);
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
    int i;
    uint16_t chan, offset;
    int painted_sectors;
    extern uint16_t CDA_VOLUME;

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
            pcm_loop_markers(spcm->chan[i].endpos);
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
                pcm_load_zero(spcm->chan[i].startpos, spcm->chan[i].endpos - spcm->chan[i].startpos);
                pcm_loop_markers(spcm->chan[i].endpos);
            }

            for (i = 0; i < spcm->num_channels; i++) {
                pcm_set_on(spcm->chan[i].id);
            }
        }

        if (painted_sectors < spcm->mix.paint_sector + SPCM_BUF_MIN_SECTORS) {
            S_SPCM_Preseek(spcm);
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
            chan = spcm->mix.paint_chan;
            offset = spcm->mix.paint_offset;

            if (chan == 0) {
                switch (spcm->state) {
                case SPCM_STATE_PAINT:
                    painted_sectors = S_SPCM_PaintedSectors(spcm);
                    if (painted_sectors <= spcm->mix.paint_sector) {
                        return;
                    }
                }
            }

            if (!S_SPCM_DMA(spcm, spcm->chan[chan].startpos, offset)) {
                break;
            }

            spcm->mix.paint_chan++;
            spcm->mix.paint_chan &= (spcm->num_channels-1);

            if (spcm->mix.paint_chan == 0) {
                spcm->mix.paint_sector++;
                spcm->mix.paint_offset += 2048;
                if (spcm->mix.paint_offset >= SPCM_CHAN_BUF_SIZE) {
                    spcm->mix.paint_offset = 0;
                }
            }

skipblock:
            spcm->lastdmatic = spcm->tics;
            if (++spcm->block > spcm->final_block) {
done:
                if (spcm->repeat) {
                    spcm->block = spcm->start_block;
                    spcm->state++;
                    break;
                }
                else {
                    spcm->sector_cnt = SPCM_BUF_NUM_SECTORS;
                    spcm->state = SPCM_STATE_STOPPING;
                    break;
                }
            }
            else if (--spcm->sector_cnt == 0) {
                spcm->state++;
                break;
            }
        }

        if (spcm->tics - spcm->lastdmatic > SPCM_MAX_WAIT_TICS) {
            goto skipblock;
        }
        break;

    case SPCM_STATE_STOPPING:
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
        if (spcm->mix.paint_offset >= SPCM_CHAN_BUF_SIZE) {
            spcm->mix.paint_offset = 0;
        }

        if (--spcm->sector_cnt == 0) {
            for (i = 0; i < spcm->num_channels; i++) {
                pcm_set_off(spcm->chan[i].id);
            }
            spcm->playing = 0;
            spcm->state = SPCM_STATE_STOPPED;
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

    spcm->playing = 1;
    spcm->state = SPCM_STATE_RESUME;

    pcm_start_timer(S_SPCM_Update);
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

    lo = open_file(name);
    if (lo < 0)
        return lo;
    offset = lo & 0x7fffffff;
    length = (lo >> 32) & 0x7fffffff;

    read_sectors(header, offset, 1);

    memset(spcm, 0, sizeof(*spcm));
    spcm->start_block = offset + 1;
    spcm->block = spcm->start_block;
    spcm->final_block = offset + (length>>11) - 1; // ignore the last padding sector
    spcm->chan[0].id = SPCM_LEFT_CHANNEL_ID;
    spcm->chan[0].startpos = SPCM_LEFT_CHAN_SOFFSET;
    spcm->chan[0].endpos = SPCM_LEFT_CHAN_SOFFSET + SPCM_CHAN_BUF_SIZE;
    spcm->chan[0].looppos = SPCM_LEFT_CHAN_SOFFSET;
    spcm->chan[0].pan = 0b00001111;
    spcm->chan[1].id = SPCM_RIGHT_CHANNEL_ID;
    spcm->chan[1].startpos = SPCM_RIGHT_CHAN_SOFFSET;
    spcm->chan[1].looppos = SPCM_RIGHT_CHAN_SOFFSET;
    spcm->chan[1].endpos = SPCM_RIGHT_CHAN_SOFFSET + SPCM_CHAN_BUF_SIZE;
    spcm->chan[1].pan = 0b11110000;
    spcm->repeat = repeat;
    spcm->state = SPCM_STATE_INIT;
    spcm->increment = (header[6] << 8) | header[7];
    spcm->num_channels = header[8];
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

    pcm_start_timer(S_SPCM_Update);

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
