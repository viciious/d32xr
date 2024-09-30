#include <stdint.h>
#include <string.h>
#include "lzss.h"
#include "scd_pcm.h"

#define VGM_MCD_BUFFER_ID   126
#define VGM_MCD_SOURCE_ID   7

#define VGM_MCD_BUFFER_ID2  127
#define VGM_MCD_SOURCE_ID2  8

#define VGM_WORDRAM_OFS     0x3000
#define VGM_READAHEAD       0x200
#define VGM_LZSS_BUF_SIZE   0x8000
#define VGM_MAX_SIZE        0x18800

#define VGM_USE_PWM_FOR_DAC

#define VGM_MAX_RF5C68_LOOPS 10

#define MD_WORDRAM          (void*)0x600000
#define MCD_WORDRAM         (void*)0x0C0000

#define MD_WORDRAM_VGM_PTR  (MD_WORDRAM+VGM_WORDRAM_OFS)
#define MCD_WORDRAM_VGM_PTR (MCD_WORDRAM+VGM_WORDRAM_OFS)

#define MARS_PWM_CTRL       (*(volatile unsigned short *)0xA15130)
#define MARS_PWM_CYCLE      (*(volatile unsigned short *)0xA15132)
#define MARS_PWM_MONO       (*(volatile unsigned short *)0xA15138)

static lzss_state_t vgm_lzss = { 0 };
static int rf5c68_dataofs;
static int rf5c68_datalen;
static short rf5c68_num_loops;
static uint16_t rf5c68_loops[VGM_MAX_RF5C68_LOOPS];
static int vgm_word_ram_offs;

extern void *vgm_ptr;
extern int pcm_baseoffs;
extern int vgm_size;
extern uint16_t cd_ok;

__attribute__((aligned(4))) uint8_t vgm_lzss_buf[VGM_LZSS_BUF_SIZE];

void lzss_setup(lzss_state_t* lzss, uint8_t* base, uint8_t *buf, uint32_t buf_size) __attribute__((section(".data"), aligned(16)));
int lzss_read(lzss_state_t* lzss, uint16_t chunk) __attribute__((section(".data"), aligned(16)));
void lzss_reset(lzss_state_t* lzss) __attribute__((section(".data"), aligned(16)));

int vgm_setup(void* fm_ptr) __attribute__((section(".data"), aligned(16)));
int vgm_read(void) __attribute__((section(".data"), aligned(16)));
int vgm_read2(int length) __attribute__((section(".data"), aligned(16)));

extern int64_t scd_open_file(const char *name);
extern void scd_read_sectors(void *ptr, int lba, int len, void (*wait)(void));
extern void scd_switch_to_bank(int bank);

static int vmg_find_rf5c68_dataofs(const uint8_t* data, int *len)
{
    const uint8_t *data_start = data;

    *len = 0;
    if (!data)
        return 0;

    while (data[0] == 0x67) {
        // data block
        int data_len = (data[3] << 0) | (data[4] << 8) | (data[5] << 16) | (data[6] << 24);
        if (data[2] == 0xC0 || data[2] == 0xC1) {
            // RF5C68 or RF5C164 RAM Data
            *len = data_len;
            return data + 7 - data_start;
        }
        data += 7 + data_len;
    }

    return 0;
}

int vgm_setup(void* fm_ptr)
{
    int s;

    if (cd_ok && vgm_size < VGM_MAX_SIZE) {
        if (!(fm_ptr >= MD_WORDRAM_VGM_PTR && fm_ptr < MD_WORDRAM+0x20000)) {
            // precache the whole VGM file in word ram to reduce bus
            // contention when reading from ROM during the gameplay
            memcpy(MD_WORDRAM_VGM_PTR, fm_ptr, vgm_size);
            fm_ptr = MD_WORDRAM_VGM_PTR;
        }
    }

    vgm_word_ram_offs = VGM_WORDRAM_OFS;
    if ((fm_ptr >= MD_WORDRAM_VGM_PTR && fm_ptr < MD_WORDRAM+0x20000)) {
        // this is needed because fm_ptr may not exactly align with MD_WORDRAM_VGM_PTR
        // due to how vgm_cache_scd works (reads on sector offset)
        vgm_word_ram_offs += (fm_ptr - MD_WORDRAM_VGM_PTR);
    }

    lzss_setup(&vgm_lzss, fm_ptr, vgm_lzss_buf, VGM_LZSS_BUF_SIZE);

    s = lzss_compressed_size(&vgm_lzss);
    pcm_baseoffs = s < vgm_size ? s : 0;

    rf5c68_num_loops = 0;
    rf5c68_dataofs = 0;
    rf5c68_datalen = 0;
    if (pcm_baseoffs)
    {
        rf5c68_dataofs = vmg_find_rf5c68_dataofs((uint8_t *)fm_ptr + pcm_baseoffs, &rf5c68_datalen);
    }

    // convert RF5C68's signed PCM samples to uchar because that's what the driver expects
    if ((fm_ptr >= MD_WORDRAM_VGM_PTR && fm_ptr < MD_WORDRAM+0x20000) && rf5c68_datalen) {
        int i;
        char is_loop = 1;
        volatile uint8_t *start = (uint8_t*)fm_ptr + pcm_baseoffs + rf5c68_dataofs;
        volatile uint8_t *end = start + rf5c68_datalen;

        for (i = 0; i < 2; i++)
        {
            volatile uint8_t *pcm = start;

            scd_switch_to_bank(i);

            while (pcm < end) {
                int s = *pcm;
                if (s == 255)
                {
                    // it's a loop point, so store the offset
                    if (!i && !is_loop && rf5c68_num_loops < VGM_MAX_RF5C68_LOOPS)
                    {
                        is_loop = 1;
                        rf5c68_loops[rf5c68_num_loops++] = pcm - start;
                    }
                }
                else
                {
                    is_loop = 0;
                }
                *pcm++ = (s & 128 ? s & ~128 : -s) + 128;
            }
        }
    }

    if (rf5c68_dataofs) {
        rf5c68_dataofs += pcm_baseoffs;
    }

    vgm_ptr = vgm_lzss_buf;

    return vgm_read();
}

void vgm_reset(void)
{
    lzss_reset(&vgm_lzss);
    vgm_ptr = vgm_lzss_buf;
}

int vgm_read(void)
{
    return lzss_read(&vgm_lzss, VGM_READAHEAD);
}

int vgm_read2(int length)
{
    return lzss_read(&vgm_lzss, length);
}

void *vgm_cache_scd(const char *name, int offset, int length)
{
    int64_t lo;
    int blk, blks;
    int flength, foffset;
    uint8_t *ptr;

    lo = scd_open_file(name);
    if (lo < 0)
        return NULL;
    flength = lo >> 32;
    if (flength < 0)
        return NULL;
    foffset = lo & 0xffffffff;

    blk = offset >> 11;
    blks = ((offset + length + 0x7FF) >> 11) - blk;
    ptr = MD_WORDRAM_VGM_PTR + (offset & 0x7FF);

    // store a copy in both banks
    scd_switch_to_bank(0);
    scd_read_sectors(MCD_WORDRAM_VGM_PTR, blk + foffset, blks, NULL);

    // copy data from WORD RAM bank 0 to bank 1
    for (offset = 0; offset < length; )
    {
        int chunk = VGM_LZSS_BUF_SIZE;
        if (offset + chunk > length)
            chunk = length - offset;

        scd_switch_to_bank(1);
        memcpy(vgm_lzss_buf, ptr + offset, chunk);
        scd_switch_to_bank(0);
        memcpy(ptr + offset, vgm_lzss_buf, chunk);

        offset += chunk;
    }

    return ptr;
}

void vgm_play_scd_samples(int offset, int length, int freq)
{
    void *ptr = (char *)MCD_WORDRAM + vgm_word_ram_offs + pcm_baseoffs + offset;

    scd_queue_setptr_buf(VGM_MCD_BUFFER_ID, ptr, length);
    scd_queue_play_src(VGM_MCD_SOURCE_ID, VGM_MCD_BUFFER_ID, freq, 128, 255, 0);
}

void vgm_play_dac_samples(int offset, int length, int freq)
{
    extern uint16_t dac_freq, dac_len, dac_center;
    extern void *dac_samples;

    if (dac_freq != freq)
    {
        int cycle;
        int ntsc = (*(volatile uint16_t *)0xC00004 & 1) == 0; // check VDP control port value

        if (ntsc)
            cycle = (((23011361 << 1)/freq + 1) >> 1) + 1; // for NTSC clock
        else
            cycle = (((22801467 << 1)/freq + 1) >> 1) + 1; // for PAL clock
        dac_center = cycle >> 1;
        dac_freq = freq;

        MARS_PWM_CTRL = 0x05; // left and right
        MARS_PWM_CYCLE = cycle;

        if (dac_len == 0)
        {
            MARS_PWM_MONO = dac_center;
            MARS_PWM_MONO = dac_center;
            MARS_PWM_MONO = dac_center;
        }
    }

    dac_samples = vgm_lzss.base + pcm_baseoffs + offset;
    dac_len = length;
}

void vgm_stop_dac_samples(void)
{
    extern uint16_t dac_len, dac_center;

    if (dac_len == 0)
        return;

    dac_len = 0;
    MARS_PWM_MONO = dac_center;
    MARS_PWM_MONO = dac_center;
    MARS_PWM_MONO = dac_center;
}

void vgm_stop_rf5c68_samples(int chan)
{
    if (!cd_ok)
        return;
    if (chan != 0 && chan != 1)
        return;

    scd_queue_stop_src(VGM_MCD_SOURCE_ID+chan);
}

static uint8_t vgm_midipan2lcf(uint8_t pan)
{
    if (pan == 0b00001111) {
        return 0; // full left
    } else if (pan == 255) {
        // no panning
        return pan;
    } else {
        return (pan & 0xf0) | ((256 - pan) >> 4);
    }
}

void vgm_play_rf5c68_samples(int chan, int offset, int loopstart, int incr, int volpan)
{
    //int freq = ((uint16_t)incr * 32604) >> 11;
    int freq = incr << 4; // good enough
    int length = loopstart - offset;
    void *ptr = (char *)MCD_WORDRAM + vgm_word_ram_offs + rf5c68_dataofs + offset;
    int vol = (volpan >> 16) & 0xff;
    int pan = vgm_midipan2lcf(volpan & 0xff);
    int autoloop = 0;

    if (!cd_ok)
        return;
    if (chan != 0 && chan != 1)
        return;

    if (loopstart == 0xffff)
    {
        // hacky hack
        scd_queue_update_src(VGM_MCD_SOURCE_ID+chan, freq, pan, vol, 255);
        return;
    }

    if (!freq)
        return;

    // find the actual length in loop offsets table
    if (length == 0)
    {
        int i;
        for (i = 0; i < rf5c68_num_loops; i++)
        {
            if (rf5c68_loops[i] > offset)
            {
                autoloop = 1;
                length = rf5c68_loops[i] - offset;
                break;
            }
        }
        if (i == rf5c68_num_loops)
            return;
    }

    scd_queue_setptr_buf(VGM_MCD_BUFFER_ID+chan, ptr, length);
    scd_queue_play_src(VGM_MCD_SOURCE_ID+chan, VGM_MCD_BUFFER_ID+chan, freq, pan, vol, autoloop);
}

void vgm_stop_samples(void)
{
    vgm_stop_rf5c68_samples(0);
    vgm_stop_rf5c68_samples(1);
    //scd_flush_cmd_queue();
    vgm_stop_dac_samples();
}
