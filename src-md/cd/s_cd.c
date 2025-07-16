#include <string.h>
#include <stdio.h>
#include "cdfh.h"
#include "s_buffers.h"

int S_CD_LoadBuffers(sfx_buffer_t *buf, int numsfx, const char *name, const int32_t *offsetlen)
{
    int i;
    int minofs, maxofs;
    int datalen, cddatalen;
    int block;
    uint8_t *data;
    int32_t offset;
    int64_t lo;

    lo = open_file(name);
    if (lo < 0)
        return lo;
    offset = lo & 0x7fffffff;

    minofs = 0x7fffffff;
    maxofs = 0;
    for (i = 0; i < numsfx; i++) {
        int startofs = offsetlen[i*2];
        int length = offsetlen[i*2+1];
        int endofs = startofs + length;

        if (startofs < minofs) {
            minofs = startofs;
        }
        if (endofs > maxofs) {
            maxofs = endofs;
        }
    }

    datalen = maxofs - minofs;
    if (datalen <= 0)
        return datalen;

    cddatalen = datalen + 0x7FF;
    cddatalen += (minofs & 0x7FF);

    // allocate enough memory to hold all sound effects + CD block padding
    data = S_Buf_Alloc(cddatalen + 7);
    if (!data)
        return datalen;

    data = (void *)(((uintptr_t)data + 7) & ~7); // align the address for DMA reads
    block = (minofs >> 11) + offset;
    read_sectors(data, block, cddatalen >> 11);

    data = data + (minofs & 0x7FF);
    for (i = 0; i < numsfx; i++) {
        int ofs = offsetlen[i*2+0] - minofs;
        int len = offsetlen[i*2+1];
        S_Buf_SetData(buf + i, data + ofs, len);
    }

    return datalen;
}
