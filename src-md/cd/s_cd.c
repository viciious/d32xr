#include <string.h>
#include <stdio.h>
#include "cdfh.h"
#include "s_buffers.h"

int S_CD_LoadBuffers(sfx_buffer_t *buf, int numsfx, const char *name, const int32_t *offsetlen)
{
    int i;
    int minofs, maxofs;
    int datalen;
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

    // allocate enough memory to hold all sound effects + CD block padding
    data = S_Buf_Alloc(datalen + 0x800);
    if (!data)
        return datalen;

    block = (minofs >> 11) + offset;
    read_sectors(data, block, (datalen + 0x800 + 0x7FF) >> 11);

    data = data + (minofs & 0x7FF);
    for (i = 0; i < numsfx; i++) {
        int ofs = offsetlen[i*2+0] - minofs;
        int len = offsetlen[i*2+1];
        S_Buf_SetData(buf + i, data + ofs, len);
    }

    return datalen;
}
