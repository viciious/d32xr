#include <stdint.h>
#include <string.h>
#include "lzss.h"

#define VGM_WORDRAM_OFS     0x3000
#define VGM_READAHEAD       0x200
#define VGM_LZSS_BUF_SIZE   0x8000
#define VGM_MAX_SIZE        0x18000

#define VGM_USE_PWM_FOR_DAC

#define MD_WORDRAM          (void*)0x600000
#define MCD_WORDRAM         (void*)0x0C0000

#define MD_WORDRAM_VGM_PTR  (MD_WORDRAM+VGM_WORDRAM_OFS)
#define MCD_WORDRAM_VGM_PTR (MCD_WORDRAM+VGM_WORDRAM_OFS)

#define MARS_PWM_CTRL       (*(volatile unsigned short *)0xA15130)
#define MARS_PWM_CYCLE      (*(volatile unsigned short *)0xA15132)
#define MARS_PWM_MONO       (*(volatile unsigned short *)0xA15138)

static lzss_state_t vgm_lzss = { 0 };
extern void *fm_ptr;
extern void *vgm_ptr;
extern int pcm_baseoffs;
extern int vgm_size;
extern uint16_t cd_ok;

__attribute__((aligned(4))) uint8_t vgm_lzss_buf[VGM_LZSS_BUF_SIZE];

void lzss_setup(lzss_state_t* lzss, uint8_t* base, uint8_t *buf, uint32_t buf_size) __attribute__((section(".data"), aligned(16)));
int lzss_read(lzss_state_t* lzss, uint16_t chunk) __attribute__((section(".data"), aligned(16)));
void lzss_reset(lzss_state_t* lzss) __attribute__((section(".data"), aligned(16)));

int vgm_setup(void* lump_ptr) __attribute__((section(".data"), aligned(16)));
int vgm_read(void) __attribute__((section(".data"), aligned(16)));
int vgm_read2(int cnt) __attribute__((section(".data"), aligned(16)));
void vgm_play_dac_samples(int offset, int length, int freq) __attribute__((section(".data"), aligned(16)));
void vgm_stop_samples(void) __attribute__((section(".data"), aligned(16)));

int vgm_setup(void* lump_ptr)
{
    int s;

    fm_ptr = lump_ptr - 0x80000;    // Use the 0x880000-0x8FFFFF region.

    /*if (cd_ok && vgm_size < 0x20000) {
        // precache the whole VGM file in word ram to reduce bus
        // contention when reading from ROM during the gameplay
        uint8_t *scdWordRam = (uint8_t *)0x600000;
        memcpy(scdWordRam, fm_ptr, vgm_size);
        fm_ptr = scdWordRam;
    }*/

    lzss_setup(&vgm_lzss, fm_ptr, vgm_lzss_buf, VGM_LZSS_BUF_SIZE);

    s = lzss_compressed_size(&vgm_lzss);
    pcm_baseoffs = s+1 < vgm_size ? s + 1 : 0;

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

int vgm_read2(int cnt)
{
    return lzss_read(&vgm_lzss, cnt);
}

int vgm_fixup(int cnt, int cnt2)
{
    vgm_lzss.outpos = VGM_READAHEAD - ((cnt + cnt2) & (VGM_READAHEAD-1));
    lzss_read(&vgm_lzss, cnt);
    return vgm_lzss.outpos;
}

void vgm_play_dac_samples(int offset, int length, int freq)
{
#ifdef DAC_STREAMING_ENABLED
#ifdef VGM_USE_PWM_FOR_DAC
    extern uint16_t dac_freq, dac_len, dac_center;
    extern void *dac_samples;

    /*if (!cd_ok)
        return;*/

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

    //dac_samples = (char *)MD_WORDRAM_VGM_PTR + pcm_baseoffs + offset;
    //dac_samples = ((char *)fm_ptr) + pcm_baseoffs + offset;
    dac_samples = vgm_lzss.base + pcm_baseoffs + offset;
    dac_len = length;

    /*if (vgm_lzss.base == 0) {
        dac_freq = vgm_lzss.base;
        while (dac_len == 0 && dac_samples == 0);
    }*/
#else
    if (cd_ok)
        vgm_play_scd_samples(offset, length, freq);
#endif
#endif
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

void vgm_stop_samples(void)
{
    //vgm_stop_rf5c68_samples(0);
    //vgm_stop_rf5c68_samples(1);
    //scd_flush_cmd_queue();
    vgm_stop_dac_samples();
}
