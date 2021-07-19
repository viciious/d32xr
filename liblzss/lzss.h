#ifndef _LZSS_H_

#include <stdint.h>

/*
 * The buffer cannot be smaller than 0x1000 since that is the look-back size
 * for lzss.
 */
#ifndef LZSS_BUF_SIZE
#define LZSS_BUF_SIZE   0x1000
#endif
#define LZSS_BUF_MASK   (LZSS_BUF_SIZE-1)

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
    __attribute__((aligned(4))) uint8_t buf[LZSS_BUF_SIZE];
} lzss_state_t;

void lzss_setup(lzss_state_t* lzss, uint8_t* base);
int lzss_read(lzss_state_t* lzss, uint16_t chunk);

#endif // _LZSS_H_
