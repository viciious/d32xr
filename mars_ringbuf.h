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

#define MARS_RINGBUF_MAXLINES    32
#define MARS_RINGBUF_MAXWORDS    (MARS_RINGBUF_MAXLINES*8)

typedef struct
__attribute__((aligned(16)))
{
    unsigned short readpos;
    unsigned short writepos;
    unsigned short readcnt;
    unsigned short writecnt;
    short ringbuf[MARS_RINGBUF_MAXWORDS] __attribute__((aligned(16)));
} marsrb_t;

extern marsrb_t marsrb;

static inline void Mars_RB_ResetRead(marsrb_t* wb)
{
    MARS_SYS_COMM2 = 0;
    wb->readpos = wb->readcnt = 0;
}

static inline void Mars_RB_ResetWrite(marsrb_t* wb)
{
    MARS_SYS_COMM6 = 0;
    wb->writepos = wb->writecnt = 0;
}

static inline void Mars_RB_ResetAll(marsrb_t* wb)
{
    Mars_RB_ResetWrite(wb);
    Mars_RB_ResetRead(wb);
}

static inline void Mars_RB_FinishRead(marsrb_t* wb)
{
    MARS_SYS_COMM2 = (MARS_SYS_COMM2 + wb->readpos + 7) & ~7;
    wb->readpos = wb->readcnt = 0;
}

static inline void Mars_RB_FinishWrite(marsrb_t* wb)
{
    MARS_SYS_COMM6 = (MARS_SYS_COMM6 + wb->writepos + 7) & ~7;
    wb->writepos = wb->writecnt = 0;
}

static inline void Mars_RB_WaitWriter(marsrb_t* wb)
{
    while (MARS_SYS_COMM2 >= MARS_SYS_COMM6) {}
}

static inline void Mars_RB_WaitReader(marsrb_t* wb, unsigned short window)
{
    while (MARS_SYS_COMM6 > MARS_SYS_COMM2 + window) {}
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

static inline short* Mars_RB_GetReadBuf(marsrb_t* wb, unsigned short wcnt)
{
    unsigned short rp = MARS_SYS_COMM2 % MARS_RINGBUF_MAXWORDS;

    if (wb->readpos > 0) {
        if (wb->readpos + wcnt <= 8)
	    return wb->ringbuf + rp + wb->readpos;
	Mars_RB_FinishRead(wb);
        rp = MARS_SYS_COMM2 % MARS_RINGBUF_MAXWORDS;
    }

    unsigned short numlines = (wcnt + 7) / 8;
    short* buf;

    // advance position if there's no space near the end
    unsigned short rpn = (MARS_SYS_COMM2 + wcnt + 7) & ~7;
    unsigned short rpe = rpn % MARS_RINGBUF_MAXWORDS;
    if (rpe < rp)
    {
        MARS_SYS_COMM2 = rpn;
        rp = rpe;
    }

    buf = wb->ringbuf + rp;
    wb->readcnt = wcnt;

    Mars_RB_WaitWriter(wb);

    Mars_ClearCacheLines(buf, numlines);

    return buf;
}

static inline short* Mars_RB_GetWriteBuf(marsrb_t* wb, short wcnt)
{
    unsigned short wp = MARS_SYS_COMM6 % MARS_RINGBUF_MAXWORDS;

    if (wb->writepos > 0) {
        if (wb->writepos + wcnt <= 8)
	    return wb->ringbuf + wp + wb->writepos;
        Mars_RB_FinishWrite(wb);
        wp = MARS_SYS_COMM6 % MARS_RINGBUF_MAXWORDS;
    }

    short* buf;
    
    // advance position if there's no space near the end
    unsigned short wpn = (MARS_SYS_COMM6 + wcnt + 7) & ~7;
    unsigned short wpe = wpn % MARS_RINGBUF_MAXWORDS;
    if (wpe < wp)
    {
        MARS_SYS_COMM6 = wpn;
        wp = wpe;
    }

    buf = wb->ringbuf + wp;
    wb->writecnt = wcnt;

    Mars_RB_WaitReader(wb, MARS_RINGBUF_MAXWORDS / 2);

    return buf;
}

#endif

#endif // _MARS_RB_H

