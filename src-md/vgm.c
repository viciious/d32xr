#include <stdint.h>
#include <string.h>
#include "lzss.h"

#define VGM_READAHEAD       0x200
#define VGM_LZSS_BUF_SIZE   0x8000

static lzss_state_t vgm_lzss = { 0 };
extern void* fm_ptr, * vgm_ptr;
extern int pcm_baseoffs;
extern int vgm_size;

__attribute__((aligned(4))) uint8_t vgm_lzss_buf[VGM_LZSS_BUF_SIZE];

void lzss_setup(lzss_state_t* lzss, uint8_t* base, uint8_t *buf, uint32_t buf_size) __attribute__((section(".data"), aligned(16)));
int lzss_read(lzss_state_t* lzss, uint16_t chunk) __attribute__((section(".data"), aligned(16)));

int vgm_setup(void) __attribute__((section(".data"), aligned(16)));
int vgm_read(void) __attribute__((section(".data"), aligned(16)));

int vgm_setup(void)
{
    int s;

    lzss_setup(&vgm_lzss, fm_ptr, vgm_lzss_buf, VGM_LZSS_BUF_SIZE);

    s = lzss_compressed_size(&vgm_lzss);
    pcm_baseoffs = s+1 < vgm_size ? s + 1 : 0;

    vgm_ptr = vgm_lzss_buf;

    return vgm_read();
}

int vgm_read(void)
{
    return lzss_read(&vgm_lzss, VGM_READAHEAD);
}
