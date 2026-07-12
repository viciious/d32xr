
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
#include <string.h>
#include <stdio.h>

#define MD_WORDRAM          (void*)0x600000 /* word RAM address on the MD in 1M mode */
#define MCD_WORDRAM         (void*)((uintptr_t)0x0C0000) /* word RAM address on the SCD in 1M mode */

extern void chk_hotplug(void);
extern void snd_ctrl(void);
extern int dma_to_32x(void *, const void *, size_t, int, int);

extern int scd_stream_cd(int lba, int length);
extern void scd_stream_cmd(char cmd);

void scd_play_roq(volatile short *commreg, int gfh_offset, int gfh_length)
{
    int wram_rem, wram_ofs;
    int header_len = 0;
    int data_size = 0;
    char send_header = 1;
    int block_ofs, block_size;
    int file_offset = 0;
    int error = 0, retry;

    // start streaming CD
    block_ofs = scd_stream_cd(gfh_offset, gfh_length);
    block_size = 0x20000-block_ofs;

    wram_rem = block_size;
    wram_ofs = block_ofs;

    while (*commreg)
    {
        uint8_t header[8];
        int pad = 0, commval;
        int chunk_id, chunk_size;

        commval = *commreg;
        if (!(commval & 1))
        {
            chk_hotplug();

            snd_ctrl();

            // no pending data requests
            continue;
        }

        if (send_header)
        {
            // header
            retry = 0;
            do {
                retry = dma_to_32x(0, (uint8_t *)MD_WORDRAM+wram_ofs, 8, 0, retry != 0);
            } while (retry < 0);

            wram_rem -= 8;
            wram_ofs += 8;
            data_size = 8;
            send_header = 0;
            file_offset = 8;
            header_len = 0;
            goto done;
        }

        while (!send_header && file_offset < gfh_length)
        {
            int i;
            uint8_t *buf, *buf_end;

            buf = (uint8_t *)MD_WORDRAM + wram_ofs;

            for (i = 0; i < wram_rem; i++)
            {
                if (header_len >= 8)
                    break;
                header[header_len++] = buf[i];
            }

            buf += i;
            wram_rem -= i;
            wram_ofs += i;
            file_offset += i;

            if (!wram_rem)
            {
                scd_stream_cmd(1); // request the next block
                wram_rem = block_size;
                wram_ofs = block_ofs;
                continue;
            }

            chunk_id = header[0] | (header[1] << 8);
            chunk_size = header[2] | (header[3] << 8) | (header[4] << 16) | (header[5] << 24);
            if (header[1] != 0x10 || chunk_size > 0xffff)
            {
                error = header[1] != 0x10 ? 1 : 2;
                data_size = 0;
                file_offset = gfh_length;
                break;
            }

            buf = (uint8_t *)MD_WORDRAM + wram_ofs;

            if (chunk_size > wram_rem)
            {
                memcpy(buf-8, header, 8);
                if ((intptr_t)buf & 1)
                {
                    buf--;
                    pad = 1;
                }

                retry = 0;
                do {
                    retry = dma_to_32x((void *)((chunk_size << 2) | (pad << 1)), buf-8, wram_rem+8+pad, chunk_id, retry != 0);
                } while (retry < 0);

                data_size += 8;
                data_size += wram_rem;
                file_offset += wram_rem;
                chunk_size -= wram_rem;

                scd_stream_cmd(1); // request the next block

                gfh_offset += block_size/2048;

                wram_rem = block_size;
                wram_ofs = block_ofs;
                buf = (uint8_t *)MD_WORDRAM + wram_ofs;
                buf_end = buf + chunk_size;
            }
            else
            {
                buf_end = buf + chunk_size;
                buf -= 8;
                memcpy(buf, header, 8);
                if ((intptr_t)buf & 1)
                {
                    buf--;
                    pad = 1;
                }
            }

            file_offset += chunk_size;
            wram_ofs += chunk_size;
            wram_rem -= chunk_size;
            data_size += buf_end - buf;

            retry = 0;
            do {
                retry = dma_to_32x((void *)((chunk_size << 2) | (pad << 1)), buf, (buf_end - buf + 1) & ~1, chunk_id, retry != 0);
            } while (retry < 0);
            header_len = 0;
            break;
        }

done:
        if (pad)
            commval |= 2;
        if (file_offset >= gfh_length)
            commval |= 4;
        if (data_size == 0)
            commval |= 8;
        if (error == 1)
            commval |= 16;
        else if (error == 2)
            commval |= 32;
        commval &= ~1;
        *commreg = commval;
        data_size = 0;
    }

    scd_stream_cmd(-1); // EOS
}
