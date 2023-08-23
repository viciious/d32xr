#ifndef _LZSS_H_

#include <stdint.h>

/*
 * The buffer cannot be smaller than 0x1000 since that is the look-back size
 * for lzss.
 */

typedef struct
{
    // current global state
    uint8_t eof;
    uint8_t idbyte;
    uint8_t getidbyte;

    // compressed block run state
    uint16_t run;
    uint16_t runlen;
    uint32_t runpos;

    // incremented on each byte write
    // AND'ed with LZSS_BUF_MASK
    uint32_t outpos;

    // incremented on each byte read
    uint8_t *input;
    // only set once during setup
    uint8_t *base;

    // the output ring buffer
    uint32_t buf_size;
    uint32_t buf_mask;
    uint8_t *buf;
} lzss_state_t;

void lzss_setup(lzss_state_t* lzss, uint8_t* base, uint8_t *buf, uint32_t buf_size);
int lzss_read(lzss_state_t* lzss, uint16_t chunk);
int lzss_read_all(lzss_state_t* lzss);
uint32_t lzss_decompressed_size(lzss_state_t* lzss);
uint32_t lzss_compressed_size(lzss_state_t* lzss);
void lzss_reset(lzss_state_t* lzss);

#endif // _LZSS_H_
