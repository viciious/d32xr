#include <string.h>
#include "s_buffers.h"
#include "s_channels.h"
#include "pcm.h"
#include "adpcm.h"

#define S_LE_SHORT(chunk) (((chunk)[1]<<8)|(((chunk)[0]) << 0))
#define S_LE_LONG(chunk)  (((chunk)[3]<<24)|(((chunk)[2]) << 16)|((chunk)[1]<<8)|(((chunk)[0]) << 0))

#define S_WAV_FORMAT_PCM         0x1
#define S_WAV_FORMAT_IMA_ADPCM   0x11
#define S_WAV_FORMAT_CREATIVE_LABS_ADPCM   0x0200
#define S_WAV_FORMAT_EXTENSIBLE  0xfffe

static uint8_t *s_mem_start, *s_mem_end;
static uint8_t *s_mem_rover;

sfx_buffer_t s_buffers [ S_MAX_BUFFERS ];

void S_InitBuffers(uint8_t *start_addr, uint32_t size)
{
    int i;

    s_mem_start = start_addr;
    s_mem_end = s_mem_start + size;
    s_mem_rover = s_mem_start;

    for (i = 0; i < S_MAX_BUFFERS; i++) {
        sfx_buffer_t *buf = s_buffers + i;
        buf->data = NULL;
        buf->freq = 0;
        buf->num_channels = 0;
        buf->buf = 0;
        buf->size = 0;
        buf->format = S_FORMAT_NONE;
    }
}

void S_ClearBuffersMem(void)
{
    s_mem_rover = s_mem_start;
}

int S_Buf_ParseWaveFile(sfx_buffer_t *buf, uint8_t *data, uint32_t len)
{
    char riff;
    int length;

    if (len < 4) {
        return 0;
    }

    riff = data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F';
    if (!riff) {
        return 0;
    }

    // find the format chunk
    uint8_t *chunk = data + 12;
    uint8_t *end = data + 0x40 - 4;
    int format = 0;

    // set default block size
    buf->adpcm_block_size = 256;

    while (chunk < end) {
        // a long value in little endian format
        length = S_LE_LONG(&chunk[4]);

        if (chunk[0] == 'd' && chunk[1] == 'a' && chunk[2] == 't' && chunk[3] == 'a')
            break;

        if (chunk[0] == 'f' && chunk[1] == 'm' && chunk[2] == 't' && chunk[3] == ' ') // 'fmt '
        {
            int channels = S_LE_SHORT(&chunk[10]);
            int sample_rate = S_LE_LONG(&chunk[12]);
            int block_align = S_LE_SHORT(&chunk[20]);

            format = S_LE_SHORT(&chunk[8]);
            if (format == S_WAV_FORMAT_EXTENSIBLE && length == 40) {
                format = S_LE_LONG(&chunk[32]); // sub-format
            }

            buf->freq = sample_rate;
            buf->adpcm_block_size = block_align;
            buf->num_channels = channels;
        }

        chunk += 8 + length;
    }

    if (chunk >= end)
        return -1;

    switch (format) {
        case S_WAV_FORMAT_PCM:
            buf->format = S_FORMAT_RAW_U8;
            break;
        case S_WAV_FORMAT_IMA_ADPCM:
            buf->adpcm_codec = ADPCM_CODEC_IMA;
            buf->format = S_FORMAT_WAV_ADPCM;
            break;
        case S_WAV_FORMAT_CREATIVE_LABS_ADPCM:
            buf->adpcm_codec = ADPCM_CODEC_SB4;
            buf->format = S_FORMAT_WAV_ADPCM;
            break;
        default:
            return -1;
    }

    buf->data = &chunk[8];
    buf->data_len = length;
    return 1;
}

uint8_t *S_Buf_Alloc(uint32_t data_len)
{
    void *ptr;

    if (s_mem_rover + data_len > s_mem_end) {
        return NULL;
    }

    ptr = s_mem_rover;
    s_mem_rover += data_len;
    return ptr;
}

void S_Buf_SetData(sfx_buffer_t *buf, uint8_t *data, uint32_t data_len)
{
    int wav;

    buf->freq = 0;
    buf->num_channels = 0;
    if (!data) {
error:        
        buf->data = NULL;
        buf->data_len = 0;
        return;
    }

    wav = S_Buf_ParseWaveFile(buf, data, data_len);
    if (wav < 0) { // a WAV, but borked
        goto error;
    }

    if (wav == 0) {
        buf->data = data;
        buf->data_len = data_len;
        buf->format = S_FORMAT_RAW_U8;
        buf->num_channels = 1;
    }
}

void S_Buf_CopyData(sfx_buffer_t *buf, const uint8_t *data, uint32_t data_len)
{
    if (buf->data && buf->size >= data_len) {
        // in-place update
        memcpy(buf->data, data, data_len);
        S_Buf_SetData(buf, buf->data, data_len);
        return;
    }

    if (s_mem_rover + data_len > s_mem_end) {
        return;
    }

    memcpy(s_mem_rover, data, data_len);
    buf->size = data_len;
    S_Buf_SetData(buf, s_mem_rover, data_len);
    s_mem_rover += data_len;
}
