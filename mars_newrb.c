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

#include <memory.h>
#include "mars_newrb.h"
#include "marshw.h"

#define ringbuf_delay() do { __asm volatile \
        ( \
            "mov #2, r0\n\t" \
            "shll8 r0\n\t" \
            "1:\n\t" \
            "nop\n\t" \
            "dt r0\n\t" \
            "bf 1b\n\t" \
            : : : "r0" \
        ); \
    } while(0)
        
static void ringbuf_lock(marsrbuf_t *buf) 
{
    int res;

    if (buf->nolock) {
        return;
    }

    do {
        __asm volatile (\
            "tas.b @%1\n\t" \
            "movt %0\n\t" \
            : "=&r" (res) \
            : "r" (&buf->lock) \
            );
    } while (res == 0);
}

static void ringbuf_unlock(marsrbuf_t *buf)
{
    if (buf->nolock) {
        return;
    }
    buf->lock = 0;
}

static void ringbuf_fixup(marsrbuf_t *buf)
{
    while (buf->readpos >= buf->size && buf->writepos >= buf->size)
    {
        buf->readpos -= buf->size;
        buf->writepos -= buf->size;
    }
}

void *ringbuf_walloc(marsrbuf_t *buf, int size)
{
    int w, r;
    int rem;
    void *data;

    if (size < 0 || size > buf->size)
        return NULL;

    ringbuf_lock(buf);

    Mars_ClearCacheLines(buf, 2);

    ringbuf_fixup(buf);

    if (buf->writepos == buf->readpos && !buf->ropen && !buf->wopen)
    {
        w = r = 0;
        buf->readpos = buf->writepos = buf->maxreadpos = 0;
    }
    else
    {
        for (w = buf->writepos; w > buf->size; w -= buf->size);
        for (r = buf->readpos ; r > buf->size; r -= buf->size);
    }

    if (buf->writepos >= buf->size)
    {
        rem = r - w;
    }
    else
    {
        rem = buf->size - buf->writepos;
        if (rem < size) {
            buf->maxreadpos = w;
            buf->writepos = buf->size;
            w = 0;
            rem = r;
        }
    }

    if (rem < size) {
        ringbuf_unlock(buf);
        if (!buf->nolock)
            ringbuf_delay();
        return NULL;
    }

    data = buf->data + w;

    buf->wopen = 1;

    ringbuf_unlock(buf);

    return data;
}

void ringbuf_wcommit(marsrbuf_t *buf, int size)
{
    if (size < 0 || size > buf->size)
        return;

    ringbuf_lock(buf);

    Mars_ClearCacheLines(buf, 2);

    if (size > buf->size)
        size = buf->size;

    buf->writepos += size;

    ringbuf_fixup(buf);

    buf->wopen = 0;

    ringbuf_unlock(buf);
}

void *ringbuf_ralloc(marsrbuf_t *buf, int size)
{
    int rem;
    void *data;

    if (size < 0 || size > buf->size)
        return NULL;

    ringbuf_lock(buf);

    Mars_ClearCacheLines(buf, 2);

    ringbuf_fixup(buf);

    if (buf->maxreadpos)
    {
        rem = buf->maxreadpos - buf->readpos;
    }
    else
    {
        rem = buf->writepos - buf->readpos;
    }

    if (rem < size) {
        ringbuf_unlock(buf);
        if (!buf->nolock)
            ringbuf_delay();
        return NULL;
    }

    buf->ropen = 1;
    data = buf->data + buf->readpos;

    ringbuf_unlock(buf);

    return data;
}

void ringbuf_rcommit(marsrbuf_t *buf, int size)
{
    if (size < 0 || size > buf->size)
        return;

    ringbuf_lock(buf);

    Mars_ClearCacheLines(buf, 2);

    buf->readpos += size;
    if (buf->maxreadpos && buf->readpos >= buf->maxreadpos) {
        buf->readpos = buf->size;
        buf->maxreadpos = 0;
    }

    buf->ropen = 0;

    ringbuf_unlock(buf);
}

void ringbuf_wait(marsrbuf_t *buf)
{
    while (1) {
        ringbuf_lock(buf);

        Mars_ClearCacheLines(buf, 2);

        if (buf->readpos == buf->writepos)
            break;

        ringbuf_unlock(buf);
    }

    ringbuf_unlock(buf);
}

void ringbuf_init(marsrbuf_t *buf, void *data, int size, int lock)
{
    memset(buf, 0, sizeof(*buf));
    buf->data = data;
    buf->size = size;
    buf->nolock = lock ? 0 : 1;
}

int ringbuf_size(const marsrbuf_t *buf)
{
    return buf->size;
}

int ringbuf_nfree(marsrbuf_t *buf)
{
    int w, r;
    int rem;

    ringbuf_lock(buf);

    Mars_ClearCacheLines(buf, 2);

    ringbuf_fixup(buf);

    if (buf->writepos == buf->readpos && !buf->ropen && !buf->wopen)
    {
        buf->readpos = buf->writepos = buf->maxreadpos = 0;
        rem = buf->size;
    }
    else
    {
        for (w = buf->writepos; w > buf->size; w -= buf->size);
        for (r = buf->readpos ; r > buf->size; r -= buf->size);

        if (buf->writepos >= buf->size)
            rem = r - w;
        else
            rem = buf->size - buf->writepos;
    }

    ringbuf_unlock(buf);

    return rem;
}
