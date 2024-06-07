#include <stdint.h>
#include <string.h>
#include "lzss.h"
#include "scd_pcm.h"

#define VGM_MCD_BUFFER_ID   127
#define VGM_MCD_SOURCE_ID   8

#define VGM_WORDRAM_OFS     0x3000
#define VGM_READAHEAD       0x200
#define VGM_LZSS_BUF_SIZE   0x8000
#define VGM_MAX_SIZE        0x18000

#define MD_WORDRAM          (void*)0x600000
#define MCD_WORDRAM         (void*)0x0C0000

#define MD_WORDRAM_VGM_PTR  (MD_WORDRAM+VGM_WORDRAM_OFS)
#define MCD_WORDRAM_VGM_PTR (MCD_WORDRAM+VGM_WORDRAM_OFS)

static lzss_state_t vgm_lzss = { 0 };
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

extern int64_t scd_open_file(const char *name);
extern void scd_read_sectors(void *ptr, int lba, int len, void (*wait)(void));
extern void scd_switch_to_bank(int bank);

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

    lzss_setup(&vgm_lzss, fm_ptr, vgm_lzss_buf, VGM_LZSS_BUF_SIZE);

    s = lzss_compressed_size(&vgm_lzss);
    pcm_baseoffs = s+1 < vgm_size ? s + 1 : 0;

    // pre-convert unsigned 8-bit PCM samples to signed PCM format for the SegaCD
    if ((fm_ptr >= MD_WORDRAM_VGM_PTR && fm_ptr < MD_WORDRAM+0x20000) && pcm_baseoffs) {
#if 0
        int i;
        volatile uint8_t *start = (uint8_t*)fm_ptr + pcm_baseoffs;
        volatile uint8_t *end = (uint8_t*)fm_ptr + vgm_size;

        for (i = 0; i < 2; i++)
        {
            volatile uint8_t *pcm = start;

            scd_switch_to_bank(i);

            while (pcm < end)
            {
                int s = (int)*pcm - 128;
                s *= 3; // amplify
                *pcm++ = s < 0 ? (s < -127 ? 127 : -s) : (s > 126 ? 126 : s|128);
            }
        }
#endif
        pcm_baseoffs += (fm_ptr - MD_WORDRAM_VGM_PTR);
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

void *vgm_cache_scd(const char *name, int offset, int length)
{
    int64_t lo;
    int blk, blks;
    int flength, foffset;

    lo = scd_open_file(name);
    if (lo < 0)
        return NULL;
    flength = lo >> 32;
    if (flength < 0)
        return NULL;
    foffset = lo & 0xffffffff;

    blk = offset >> 11;
    blks = ((offset + length + 0x7FF) >> 11) - blk;

    // store a copy in both banks
    scd_read_sectors(MCD_WORDRAM_VGM_PTR, blk + foffset, blks, NULL);
    scd_read_sectors(MCD_WORDRAM_VGM_PTR, blk + foffset, blks, NULL);

    return MD_WORDRAM_VGM_PTR + (offset & 0x7FF);
}

void vgm_play_scd_samples(int offset, int length, int freq)
{
    void *ptr = (char *)MCD_WORDRAM_VGM_PTR + pcm_baseoffs + offset;
    scd_queue_setptr_buf(VGM_MCD_BUFFER_ID, ptr, length);
    scd_queue_play_src(VGM_MCD_SOURCE_ID, VGM_MCD_BUFFER_ID, freq, 128, 255, 0);
}

void vgm_play_samples(int offset, int length, int freq)
{
    vgm_play_scd_samples(offset, length, freq);
}

void vgm_stop_samples(void)
{
    scd_queue_stop_src(VGM_MCD_SOURCE_ID);
}
