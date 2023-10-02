#include <string.h>
#include <stdio.h>
#include "cdfh.h"
#include "s_buffers.h"

void S_CD_LoadBufferData(sfx_buffer_t *buf, const char *name, int32_t offset, int32_t len)
{
    int flen;
    static char fname[256] = { 0 };
    static CDFileHandle_t fh;

    flen = mystrlen(name);
    if (!memcmp(fname, name, flen+1)) {
        // same file
    } else {
        if (!cd_handle_from_name(&fh, name))
        {
            fname[0] = 0;
            return;
        }
        memcpy(fname, name, flen+1);
    }

    if (!S_Buf_AllocData(buf, len))
        return;

    fh.Seek(&fh, offset, SEEK_SET);

    fh.Read(&fh, buf->buf, len);

    S_Buf_SetData(buf, buf->buf, len);
}
