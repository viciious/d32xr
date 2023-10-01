
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define BLOCK_SIZE 2048
#define CHUNK_SIZE 4*BLOCK_SIZE
#define MCD_DISC_BUFFER (void*)((uintptr_t)0x0C0000 + 0x20000 - CHUNK_SIZE)
#define MD_DISC_BUFFER (void*)((uintptr_t)0x600000 + 0x20000 - CHUNK_SIZE)

typedef struct CDFileHandle {
    int32_t  (*Seek)(struct CDFileHandle *handle, int32_t offset, int32_t whence);
    int32_t  (*Tell)(struct CDFileHandle *handle);
    int32_t  (*Read)(struct CDFileHandle *handle, void *ptr, int32_t size);
    uint8_t  (*Eof)(struct CDFileHandle *handle);
    int32_t  offset; // start block of file
    int32_t  length; // length of file
    int32_t  sblk, eblk; // current block in buffer
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

#if 0
    {
        volatile char *temp = (volatile char *)0x600000; /* word ram on MD side (in 1M mode) */
        for (i = 0; temp[i]; i++) {}
        int words = (i + 2) / 2;
        for (i = 0; i < words; i++) {
            *((volatile short *)name+i) = 0;
        }
        for (i = 0; temp[i]; i++) {
            ((volatile char *)name)[i] = temp[i];
        }
    }
#endif

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
        len = CHUNK_SIZE - (pos & (BLOCK_SIZE-1));
        if (len > size)
            len = size;
        if (len > (handle->length - pos))
            len = (handle->length - pos);

        blk = (pos >> 11) + handle->offset;
        if (blk >= handle->eblk || blk < handle->sblk)
        {
            int blks = CHUNK_SIZE>>11;
            local_scd_read_block((void *)MCD_DISC_BUFFER, blk, blks);
            handle->sblk = blk;
            handle->eblk = blk + blks;
        }

        memcpy(dst, (char *)MD_DISC_BUFFER + (pos & (BLOCK_SIZE-1)), len);

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
        handle->eblk = -1; // nothing read yet
        handle->sblk = 0x0fffffff;
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
    handle->eblk = -1; // nothing read yet
    handle->sblk = 0x0fffffff;
    handle->pos = 0;
    return length;
}

int scd_read_file(void *ptr, int length)
{
#if 0
    int r;
    uint8_t *dst = ptr;

    r = 0;
    while (r < length)
    {
        int l;

        l = length - r;
        if (l > CHUNK_SIZE)
            l = CHUNK_SIZE;

        write_long(0xA12010, (uintptr_t)0x0C0000 + 0x20000 - CHUNK_SIZE); /* end of 128K of word ram on CD side (in 1M mode) */
        write_long(0xA12014, l);
        wait_do_cmd('H');
        wait_cmd_ack();
        l = read_long(0xA12020);
        write_byte(0xA1200E, 0x00); // acknowledge receipt of command result

        if (l == 0)
            break;

        if ((intptr_t)dst < 0x600000 || (uintptr_t)dst >= 0x640000)
        {
            short *wordRam = (void *)((uintptr_t)0x600000 + 0x20000 - CHUNK_SIZE); /* end of 128K of word ram on MD side (in 1M mode) */
#if 1
            int i, words;
            
            // copy from wordRam to destination buffer
            words = l / 2;
            for (i = 0; i < words; i++) {
                ((short *)dst)[i] = wordRam[i];
            }
            ((short *)dst)[i] = 0;

            if (l & 1) {
                dst[l - 1] = ((char *)wordRam)[l - 1];
            }
#else
            memmove(dst, wordRam, l);
#endif
        }

        r += l;
        dst += l;
        if (l < CHUNK_SIZE)
            break;
    }
#else
    int r;

    r = cd_Read(&gfh, ptr, length);
    if (r < 0)
        return r;
#endif

    return r;
}

int scd_seek_file(int offset, int whence)
{
#if 0 
    write_long(0xA12010, whence); /* word ram on CD side (in 1M mode) */
    write_long(0xA12014, offset);
    wait_do_cmd('J');
    wait_cmd_ack();
    offset = read_long(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return offset;
#else
    return cd_Seek(&gfh, offset, whence);
#endif
}
