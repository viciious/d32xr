
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define BLOCK_SIZE 2048
#define CHUNK_SHIFT 3
#define CHUNK_BLOCKS (1<<CHUNK_SHIFT) 
#define CHUNK_SIZE (CHUNK_BLOCKS*BLOCK_SIZE)
#define MCD_DISC_BUFFER (void*)((uintptr_t)0x0C0000 + 0x20000 - CHUNK_SIZE)
#define MD_DISC_BUFFER (void*)((uintptr_t)0x600000 + 0x20000 - CHUNK_SIZE)

typedef struct CDFileHandle {
    int32_t  (*Seek)(struct CDFileHandle *handle, int32_t offset, int32_t whence);
    int32_t  (*Tell)(struct CDFileHandle *handle);
    int32_t  (*Read)(struct CDFileHandle *handle, void *ptr, int32_t size);
    uint8_t  (*Eof)(struct CDFileHandle *handle);
    int32_t  offset; // start block of file
    int32_t  length; // length of file
    int32_t  blk; // current block in buffer
    int32_t  pos; // current position in file
} CDFileHandle_t;

extern CDFileHandle_t *cd_handle_from_name(CDFileHandle_t *handle, const char *name);
extern CDFileHandle_t *cd_handle_from_offset(CDFileHandle_t *handle, int32_t offset, int32_t length);

extern void write_byte(unsigned int dst, unsigned char val);
extern void write_word(unsigned int dst, unsigned short val);
extern void write_long(unsigned int dst, unsigned int val);
extern unsigned char read_byte(unsigned int src);
extern unsigned short read_word(unsigned int src);
extern unsigned int read_long(unsigned int src);

static void scd_delay(void)
{
    int cnt = 5;
    do {
        asm __volatile("nop");
    } while (--cnt);
}

static char wait_cmd_ack(void)
{
    char ack = 0;

    do {
        scd_delay();
        ack = read_byte(0xA1200F); // wait for acknowledge byte in sub comm port
    } while (!ack);

    return ack;
}

static void wait_do_cmd(char cmd)
{
    while (read_byte(0xA1200F)) {
        scd_delay(); // wait until Sub-CPU is ready to receive command
    }
    write_byte(0xA1200E, cmd); // set main comm port to command
}

int64_t local_scd_open_file(char *name)
{
    int i;
    int length, offset;
    char *scdfn = (char *)0x600000; /* word ram on MD side (in 1M mode) */

    for (i = 0; name[i]; i++)
        *scdfn++ = name[i];
    *scdfn = 0;

    write_long(0xA12010, 0x0C0000); /* word ram on CD side (in 1M mode) */
    wait_do_cmd('F');
    wait_cmd_ack();
    length = read_long(0xA12020);
    offset = read_long(0xA12024);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result

    if (length < 0)
        return length;
    return ((int64_t)length << 32) | offset;
}

void local_scd_read_block(void *ptr, int lba, int len)
{
    int blk;
    write_long(0xA12010, (uintptr_t)ptr); /* end of 128K of word ram on CD side (in 1M mode) */
    write_long(0xA12014, lba);
    write_long(0xA12018, len);
    wait_do_cmd('H');
    wait_cmd_ack();
    blk = read_long(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return blk;
}

/// 

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
    int32_t wait = 0;
    int32_t pos, blk, len, read = 0;
    uint8_t *dst = ptr;

    if (!handle)
        return -1;

    while (size > 0 && !handle->Eof(handle))
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
            local_scd_read_block((void *)MCD_DISC_BUFFER, blk*CHUNK_BLOCKS + handle->offset, CHUNK_BLOCKS);
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

static int32_t cd_Tell(CDFileHandle_t *handle)
{
    return handle ? handle->pos : 0;
}

CDFileHandle_t *cd_handle_from_offset(CDFileHandle_t *handle, int32_t offset, int32_t length)
{
    if (handle)
    {
        handle->Eof  = &cd_Eof;
        handle->Read = &cd_Read;
        handle->Seek = &cd_Seek;
        handle->Tell = &cd_Tell;
        handle->offset = offset;
        handle->length = length;
        handle->blk = -1; // nothing read yet
        handle->pos = 0;
    }
    return handle;
}

///

CDFileHandle_t gfh;

int scd_open_file(char *name)
{
    int64_t lo;
    CDFileHandle_t *handle = &gfh;
    int length, offset;

    lo = local_scd_open_file(name);
    length = lo >> 32;
    if (length < 0)
        return -1;
    offset = lo & 0xffffffff;
    
    handle->Eof  = &cd_Eof;
    handle->Read = &cd_Read;
    handle->Seek = &cd_Seek;
    handle->Tell = &cd_Tell;
    handle->offset = offset;
    handle->length = length;
    handle->blk = -1; // nothing read yet
    handle->pos = 0;
    return length;
}

int scd_read_file(void *ptr, int length)
{
    return cd_Read(&gfh, ptr, length);
}

int scd_seek_file(int offset, int whence)
{
    return cd_Seek(&gfh, offset, whence);
}
