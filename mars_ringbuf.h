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

#include "mars.h"

#ifdef MARS

#define MARS_RINGBUF_MAXLINES    64
#define MARS_RINGBUF_MAXWORDS    (MARS_RINGBUF_MAXLINES*8)

typedef struct
__attribute__((aligned(16)))
{
//    unsigned short readpos; // MARS_SYS_COMM2
//    unsigned short writepos; // MARS_SYS_COMM6
    short ringbuf[MARS_RINGBUF_MAXWORDS] __attribute__((aligned(16)));
} marsrb_t;

extern marsrb_t marsrb;

static inline void Mars_RB_Reset(marsrb_t* wb)
{
    MARS_SYS_COMM2 = 0;
    MARS_SYS_COMM6 = 0;
}

static inline void Mars_RB_WaitWriter(marsrb_t* wb)
{
    while(MARS_SYS_COMM2 >= MARS_SYS_COMM6) {}
}

static inline void Mars_RB_WaitReader(marsrb_t* wb, short window)
{
    while (MARS_SYS_COMM6 > MARS_SYS_COMM2 + window) {}
}

static inline void Mars_RB_AdvanceWriter(marsrb_t* wb, short wcnt)
{
    MARS_SYS_COMM6 = (MARS_SYS_COMM6 + wcnt + 7) & ~7;
}

static inline void Mars_RB_AdvanceReader(marsrb_t* wb, short wcnt)
{
    MARS_SYS_COMM2 = (MARS_SYS_COMM2 + wcnt + 7) & ~7;
}

static inline short* Mars_RB_GetReadBuf(marsrb_t* wb, short wcnt)
{
    short numlines = (wcnt + 7) / 8;
    short* buf = wb->ringbuf + MARS_SYS_COMM2 % MARS_RINGBUF_MAXWORDS;

    Mars_RB_WaitWriter(wb);

    Mars_ClearCacheLines(buf, numlines);

    return buf;
}

static inline short* Mars_RB_GetWriteBuf(marsrb_t* wb, short wcnt)
{
    short* buf = wb->ringbuf + MARS_SYS_COMM6 % MARS_RINGBUF_MAXWORDS;

    Mars_RB_WaitReader(wb, MARS_RINGBUF_MAXWORDS / 2);

    return buf;
}

#endif

#endif // _MARS_RB_H
