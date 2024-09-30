
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define MD_WORDRAM          (void*)0x600000 /* word RAM address on the MD in 1M mode */
#define MCD_WORDRAM         (void*)((uintptr_t)0x0C0000) /* word RAM address on the SCD in 1M mode */

#define BLOCK_SIZE 2048
#define CHUNK_SHIFT 3
#define CHUNK_BLOCKS (1<<CHUNK_SHIFT) 
#define CHUNK_SIZE (CHUNK_BLOCKS*BLOCK_SIZE)

#define DISC_BUFFER_WORDRAM_OFS (0x20000 - CHUNK_SIZE)
#define MCD_DISC_BUFFER (void*)(MCD_WORDRAM + DISC_BUFFER_WORDRAM_OFS)
#define MD_DISC_BUFFER (void*)(MD_WORDRAM + DISC_BUFFER_WORDRAM_OFS)

extern int64_t scd_open_file(const char *name);
extern void scd_read_sectors(void *ptr, int lba, int len, void (*wait)(void));
extern void bump_fm(void);

typedef struct CDFileHandle {
    int32_t  offset; // start block of file
    int32_t  length; // length of file
    int32_t  blk; // current block in buffer
    int32_t  pos; // current position in file
} CDFileHandle_t;

CDFileHandle_t gfh;

static uint8_t cd_Eof(CDFileHandle_t *handle)
{
    if (!handle)
        return 1;

    if (handle->pos >= handle->length)
        return 1;

    return 0;
}

static int32_t cd_Read(CDFileHandle_t *handle, void *ptr, int32_t size)
{
    int32_t pos, blk, len, read = 0;
    uint8_t *dst = ptr;

    if (!handle)
        return -1;

    while (size > 0 && !cd_Eof(handle))
    {
        pos = handle->pos;
        len = CHUNK_SIZE - (pos & (CHUNK_SIZE-1));
        if (len > size)
            len = size;
        if (len > (handle->length - pos))
            len = (handle->length - pos);

        blk = pos / CHUNK_SIZE;
        if (blk != handle->blk)
        {
            // keep the FM music going by calling bump_fm
            scd_read_sectors((void *)MCD_DISC_BUFFER, blk*CHUNK_BLOCKS + handle->offset, CHUNK_BLOCKS, bump_fm);
            handle->blk = blk;
        }

        memcpy(dst, (char *)MD_DISC_BUFFER + (pos & (CHUNK_SIZE-1)), len);

        handle->pos += len;
        dst += len;
        read += len;
        size -= len;
    }

    return read;
}

static int32_t cd_Seek(CDFileHandle_t *handle, int32_t offset, int32_t whence)
{
    int32_t pos;

    if (!handle)
        return -1;

    pos = handle->pos;
    switch(whence)
    {
        case SEEK_CUR:
            pos += offset;
            break;
        case SEEK_SET:
            pos = offset;
            break;
        case SEEK_END:
            pos = handle->length - offset - 1;
            break;
    }
    if (pos < 0)
        handle->pos = 0;
    else if (pos > handle->length)
        handle->pos = handle->length;
    else
        handle->pos = pos;

    return handle->pos;
}

int64_t scd_open_gfile_by_name(char *name)
{
    int64_t lo;
    CDFileHandle_t *handle = &gfh;
    int length, offset;

    lo = scd_open_file(name);
    if (lo < 0)
        return lo;
    length = lo >> 32;
    offset = lo & 0x7fffffff;

    handle->offset = offset;
    handle->length = length;
    handle->blk = -1; // nothing read yet
    handle->pos = 0;
    return lo;
}

void scd_open_gfile_by_length_offset(int length, int offset)
{
    CDFileHandle_t *handle = &gfh;
    handle->offset = offset;
    handle->length = length;
    handle->blk = -1; // nothing read yet
    handle->pos = 0;
}

int scd_read_gfile(void *ptr, int length)
{
    return cd_Read(&gfh, ptr, length);
}

int scd_seek_gfile(int offset, int whence)
{
    return cd_Seek(&gfh, offset, whence);
}
