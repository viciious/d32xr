/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2021 Victor Luchits

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

#ifndef _MARS_RB_H
#define _MARS_RB_H

#ifdef MARS

#include "mars.h"

#define MARS_RINGBUF_MAXLINES    16
#define MARS_RINGBUF_MAXWORDS    (MARS_RINGBUF_MAXLINES*8)

#define MARS_UNCACHED_RROVER    *(volatile unsigned *)(((intptr_t)&wb->readrover) | 0x20000000)
#define MARS_UNCACHED_WROVER    *(volatile unsigned *)(((intptr_t)&wb->writerover) | 0x20000000)

typedef struct
__attribute__((aligned(16)))
{
    unsigned writerover __attribute__((aligned(16)));
    unsigned readrover __attribute__((aligned(16)));

    unsigned readpos;
    unsigned writepos;
    unsigned readcnt;
    unsigned writecnt;
    short ringbuf[MARS_RINGBUF_MAXWORDS] __attribute__((aligned(16)));
} marsrb_t;

static inline void Mars_RB_ResetRead(marsrb_t* wb)
{
    MARS_UNCACHED_RROVER = 0;
    wb->readpos = wb->readcnt = 0;
}

static inline void Mars_RB_ResetWrite(marsrb_t* wb)
{
    MARS_UNCACHED_WROVER = 0;
    wb->writepos = wb->writecnt = 0;
}

static inline void Mars_RB_ResetAll(marsrb_t* wb)
{
    Mars_RB_ResetWrite(wb);
    Mars_RB_ResetRead(wb);
}

static inline int Mars_RB_Len(marsrb_t* wb)
{
    int len = (int)MARS_UNCACHED_WROVER - (int)MARS_UNCACHED_RROVER;
    if (len <= 0) return 0;
    return len;
}

static inline void Mars_RB_FinishRead(marsrb_t* wb)
{
    MARS_UNCACHED_RROVER = (MARS_UNCACHED_RROVER + wb->readpos + 7) & ~7;
    wb->readpos = wb->readcnt = 0;
}

static inline void Mars_RB_FinishWrite(marsrb_t* wb)
{
    MARS_UNCACHED_WROVER = (MARS_UNCACHED_WROVER + wb->writepos + 7) & ~7;
    wb->writepos = wb->writecnt = 0;
}

static inline void Mars_RB_WaitWriter(marsrb_t* wb, unsigned want)
{
    while (Mars_RB_Len(wb) < want) {}
}

static inline void Mars_RB_WaitReader(marsrb_t* wb, unsigned window)
{
    while (Mars_RB_Len(wb) > window) {}
}

static inline void Mars_RB_CommitWrite(marsrb_t* wb)
{
    wb->writepos += wb->writecnt;
    if (wb->writepos >= 8)
        Mars_RB_FinishWrite(wb);
}

static inline void Mars_RB_CommitRead(marsrb_t* wb)
{
    wb->readpos += wb->readcnt;
    if (wb->readpos >= 8)
        Mars_RB_FinishRead(wb);
}

static inline short* Mars_RB_GetReadBuf(marsrb_t* wb, unsigned wcnt)
{
    unsigned rrover = MARS_UNCACHED_RROVER;
    unsigned rp = rrover % MARS_RINGBUF_MAXWORDS;

    if (wcnt > MARS_RINGBUF_MAXWORDS)
        return NULL;

    if (wb->readpos > 0) {
        if (wb->readpos + wcnt <= 8) {
            wb->readcnt = wcnt;
            return wb->ringbuf + rp + wb->readpos;
        }
        Mars_RB_FinishRead(wb);
        rp = rrover % MARS_RINGBUF_MAXWORDS;
    }

    short* buf;

    // advance position if there's no space near the end
    unsigned rpn = (rrover + wcnt + 7) & ~7;
    unsigned rpe = rpn % MARS_RINGBUF_MAXWORDS;
    if (rpe < rp)
    {
        MARS_UNCACHED_RROVER = rpn;
        rp = rpe;
    }

    buf = wb->ringbuf + rp;
    wb->readcnt = wcnt;

    Mars_RB_WaitWriter(wb, wcnt);

    return buf;
}

static inline short* Mars_RB_GetWriteBuf(marsrb_t* wb, unsigned wcnt, boolean wait)
{
    unsigned wrover = MARS_UNCACHED_WROVER;
    unsigned wp = wrover % MARS_RINGBUF_MAXWORDS;
    unsigned window = wcnt;

    if (wcnt > MARS_RINGBUF_MAXWORDS)
        return NULL;

    if (window < MARS_RINGBUF_MAXWORDS / 2)
        window = MARS_RINGBUF_MAXWORDS / 2;
    else if (window > MARS_RINGBUF_MAXWORDS)
        window = MARS_RINGBUF_MAXWORDS;
    window = MARS_RINGBUF_MAXWORDS - window;

    if (!wait && Mars_RB_Len(wb) > window)
        return NULL;

    if (wb->writepos > 0) {
        if (wb->writepos + wcnt <= 8) {
            wb->writecnt = wcnt;
            return wb->ringbuf + wp + wb->writepos;
        }
        Mars_RB_FinishWrite(wb);
        wp = wrover % MARS_RINGBUF_MAXWORDS;
    }

    short* buf;

    // advance position if there's no space near the end
    unsigned wpn = (wrover + wcnt + 7) & ~7;
    unsigned wpe = wpn % MARS_RINGBUF_MAXWORDS;
    if (wpe < wp)
    {
        MARS_UNCACHED_WROVER = wpn;
        wp = wpe;
    }

    buf = wb->ringbuf + wp;
    wb->writecnt = wcnt;

    Mars_RB_WaitReader(wb, window);

    // return pointer in cache-through area
    return (short*)((intptr_t)buf | 0x20000000);
}

#endif

#endif // _MARS_RB_H

