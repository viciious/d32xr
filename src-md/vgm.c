#include <stdint.h>
#include <string.h>
#include "lzss.h"

#define VGM_WORDRAM_OFS     0x4000
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
extern void scd_read_block(void *ptr, int lba, int len);

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

void *vgm_cache_scd(const char *name, int offset, int length)
{
    int64_t lo;
    int blk, blks;
    int flength, foffset;

    lo = scd_open_file(name);
    flength = lo >> 32;
    if (flength < 0)
        return NULL;
    foffset = lo & 0xffffffff;

    blk = offset >> 11;
    blks = ((offset + length + 0x7FF) >> 11) - blk;

    // store a copy in both banks
    scd_read_block(MCD_WORDRAM_VGM_PTR, blk + foffset, blks);
    scd_read_block(MCD_WORDRAM_VGM_PTR, blk + foffset, blks);

    return MD_WORDRAM_VGM_PTR + (offset & 0x7FF);
}
