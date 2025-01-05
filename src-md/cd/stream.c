/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2024 Victor Luchits

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hw_md.h"
#include "cdfh.h"
#include "pcm.h"
#include "s_main.h"

#define MCD_WORDRAM     (void*)((uintptr_t)0x0C0000) /* word RAM address on the SCD in 1M mode */

#define BLOCK_SIZE      2048
#define CHUNK_BLOCKS    56
#define CHUNK_SIZE      (CHUNK_BLOCKS*BLOCK_SIZE)
#define CHUNKS_INWRAM   4 // how many chunks fit into word ram
#define CHUNK_CACHE_BLOCKS CHUNK_BLOCKS*CHUNKS_INWRAM
#define CHUNK_CACHE_SIZE CHUNK_CACHE_BLOCKS*BLOCK_SIZE

#define SCD_COMMFL_0     *((volatile char *)0xffff800E)
#define SCD_COMMFL_1     *((volatile char *)0xffff800F)

#define SCD_COMMREG_RW0 *((volatile int  *)0xffff8020)

#define MAX_SECTOR_ATTEMPTS 0xFFFF

typedef struct {
    volatile int blk, last_blk;
    volatile intptr_t dest;
    volatile int attempts;
    volatile uint16_t cnt;
    volatile uint16_t len;
    volatile char r;
    volatile char is_reading;
} timercbarg_t;

extern void switch_banks(void);

static void stream_tcb(timercbarg_t *t)
{
    int addr_mask;
    int (*read)(void *ptr);

    if (!t->is_reading) {
        return;
    }
    if (!t->len) {
        return;
    }

    if (t->dest >= (intptr_t)MCD_WORDRAM && t->dest < (intptr_t)MCD_WORDRAM+0x20000) {
        addr_mask = 0x0001FFFF; /* use 0x3FFFF for 2M mode */
        read = dma_cd_sector_wram;
    } else {
        addr_mask = -1;
        read = dma_cd_sector_prg;
    }

    if ((t->r = read((void *)(t->dest & addr_mask))) == 0) {
        if (!t->attempts) {
            t->r = -1;
            t->len = 0;
            return;
        }
        t->attempts--;
        return;
    }

    t->dest += 2048;
    t->len--;
    t->cnt++;
    t->attempts = MAX_SECTOR_ATTEMPTS;
}

static int stream_bread(timercbarg_t *t, void *dest, int nblk)
{
    if (t->blk + nblk > t->last_blk + 1) {
        nblk = t->last_blk - t->blk + 1;
    }

    if (!nblk) {
        return 0;
    }

    t->r = 0;
    t->dest = (intptr_t)dest;
    t->cnt = 0;
    t->attempts = MAX_SECTOR_ATTEMPTS;
    t->is_reading = 1;
    t->len = nblk;
    begin_read_cd(t->blk, nblk);

    return nblk;
}

static int stream_twait(timercbarg_t *t)
{
    if (!t->is_reading) {
        return 0;
    }

    while (t->r >= 0 && t->len);

    t->is_reading = 0;

    stop_read_cd();

    t->blk += t->cnt;

    return 1;
}

void stream_cd(int blk, int length, uint8_t *buf)
{
    int read_ofs;
    int write_ofs = 0;
    int wram_ofs = (0x20000/BLOCK_SIZE-CHUNK_BLOCKS)*BLOCK_SIZE;
    void *wram = MCD_WORDRAM+wram_ofs;
    int cmds = 0;
    int last_blk = blk + (length>>11);
    timercbarg_t t;

    memset(&t, 0, sizeof(t));
    t.blk = blk;
    t.last_blk = last_blk;

    pcm_start_timer((void (*)(void *))stream_tcb, &t);

    stream_bread(&t, wram, CHUNK_BLOCKS);
    stream_twait(&t);

    switch_banks();

    // read the next chunk into the other bank
    stream_bread(&t, wram, CHUNK_BLOCKS);
    stream_twait(&t);

    // cache the next few chunks in PRG RAM
    stream_bread(&t, buf, CHUNK_BLOCKS*CHUNKS_INWRAM);
    stream_twait(&t);

    SCD_COMMREG_RW0 = wram_ofs; // tell the MD where the data will be going to in WORD RAM
    SCD_COMMFL_1 = 'D'; // sub comm port = DONE
    while (SCD_COMMFL_0); // wait for result acknowledged
    SCD_COMMFL_1 = 0;   // sub comm port = READY

    read_ofs = 0;
    write_ofs = 0;

    while (1)
    {
        char cmd = SCD_COMMFL_0;
        if (cmd == 0) {
            continue;
        }
        if (cmd == -1) {
            break;
        }

        // let the MD see the next chunk in WORD RAM
        switch_banks();

        SCD_COMMFL_1 = 'D'; // sub comm port = DONE
        while (SCD_COMMFL_0); // wait for result acknowledged
        SCD_COMMFL_1 = 0;   // sub comm port = READY

        // copy the next chunk to WORD RAM
        memcpy(wram, buf+read_ofs, CHUNK_SIZE);
        read_ofs += CHUNK_SIZE;
        if (read_ofs >= CHUNK_CACHE_SIZE)
            read_ofs = 0;

        if (cmds & 1)
        {
            int nblk;

            stream_twait(&t);

            // begin reading the next chunk
            nblk = stream_bread(&t, buf + write_ofs, CHUNK_CACHE_BLOCKS/2);
            if (nblk > 0) {
                write_ofs += CHUNK_CACHE_SIZE/2;
                if (write_ofs >= CHUNK_CACHE_SIZE)
                    write_ofs = 0;
            }
        }

        cmds++;
    }

    stream_twait(&t);

    pcm_stop_timer();
}
