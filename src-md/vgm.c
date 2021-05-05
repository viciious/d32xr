#include <stdint.h>
#include <string.h>
#include "lzss.h"

#define VGM_READAHEAD 0x100

static lzss_state_t vgm_lzss = { 0 };
extern void* fm_ptr, * pcm_ptr, * vgm_ptr;

int vgm_setup(void) __attribute__((section(".data"), aligned(16)));
int vgm_read(void) __attribute__((section(".data"), aligned(16)));

int vgm_setup(void)
{
    lzss_setup(&vgm_lzss, fm_ptr);

    vgm_ptr = vgm_lzss.buf;
    pcm_ptr = NULL;

    return vgm_read();
}

int vgm_read(void)
{
    return lzss_read(&vgm_lzss, VGM_READAHEAD);
}
