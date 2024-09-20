#include <stdint.h>
#include <string.h>
#include "lzss.h"

#define VGM_READAHEAD       0x200
#define VGM_LZSS_BUF_SIZE   0x8000

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
int vgm_read2(int cnt) __attribute__((section(".data"), aligned(16)));

int vgm_setup(void* fm_ptr)
{
    int s;

    if (cd_ok && vgm_size < 0x20000) {
        // precache the whole VGM file in word ram to reduce bus
        // contention when reading from ROM during the gameplay
        uint8_t *scdWordRam = (uint8_t *)0x600000;
        memcpy(scdWordRam, fm_ptr, vgm_size);
        fm_ptr = scdWordRam;
    }

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
