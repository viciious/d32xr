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

typedef struct {
    volatile int readpos;
    volatile int writepos;
    volatile int maxreadpos;
    volatile char ropen, wopen;
    volatile char lock;
    char nolock;
    uint16_t size;
    uint8_t *data;
} marsrbuf_t __attribute((aligned(16)));

void *ringbuf_walloc(marsrbuf_t *buf, int size);
void ringbuf_wcommit(marsrbuf_t *buf, int size);
void *ringbuf_ralloc(marsrbuf_t *buf, int size);
void ringbuf_rcommit(marsrbuf_t *buf, int size);
void ringbuf_wait(marsrbuf_t *buf);
void ringbuf_init(marsrbuf_t *buf, void *data, int size, int lock);
int ringbuf_size(const marsrbuf_t *buf);
int ringbuf_nfree(marsrbuf_t *buf);
