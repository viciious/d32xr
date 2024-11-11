#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

int rawu82spcm(int v)
{
    v -= 128;
    if (v > 126) {
        v = 126;
    }
    if (v > 0) {
        return v|128;
    }
    return -v;
}

int main(int argc, char **argv)
{
#if 0
    FILE *fin = fopen(argv[1], "rb");
    FILE *fout = fopen(argv[2], "wb");
    int i, len;
    uint8_t *data;
    uint8_t *outdata;

    fseek(fin, 0, SEEK_END);
    len = ftell(fin);
    data = malloc(len);
    outdata = malloc(len*2);
    fseek(fin, 0, SEEK_SET);
    fread(data, 1, len, fin);

    for (i = 0; i < len; i++) {
        outdata[i*2] = 0;
        outdata[i*2+1] = rawu82spcm(data[i]);
    }

    fwrite(outdata, 1, len*2, fout);

    free(data);
    free(outdata);

    fclose(fin);
    fclose(fout);
#else
    FILE *fin = fopen(argv[1], "rb");
    FILE *fout = fopen(argv[2], "wb");
    int i, len;
    uint8_t *data;
    uint8_t *outdata;
    int num_channels = 2;
    uint8_t sector[2048];

    fseek(fin, 0, SEEK_END);
    len = ftell(fin);
    data = malloc(len);
    outdata = malloc(len + 4096);
    fseek(fin, 0, SEEK_SET);
    fread(data, 1, len, fin);

    memset(sector, 0, sizeof(sector));
    memset(outdata, 0, len + 4096);

    fwrite(sector, 1, 2048, fout);

    if (num_channels == 1) {
        for (i = 0; i < len; i++) {
            outdata[i] = rawu82spcm(data[i]);
        }
    }
    else {
        int num_samples = len / 2;

        for (i = 0; i < len; i += 2) {
            int sectornum = i / 4096;
            int bytenum = (i % 4096) / 2;

            int leftoff = sectornum * 2 * 2048;
            int rightoff = leftoff + 2048;

            outdata[leftoff  + bytenum] = rawu82spcm(data[i]);
            outdata[rightoff + bytenum] = rawu82spcm(data[i+1]);
        }
    }

    if (num_channels == 1) {
        int num_sectors = (len + 2047) / 2048;
        for (i = 0; i < num_sectors; i++)
        {
            fwrite(&outdata[i*2048], 1, 2048, fout);
        }
    } else {
        int num_sectors = (len/2 + 2047) / 2048;
        num_sectors *= 2;
        for (i = 0; i < num_sectors; i++)
        {
            fwrite(&outdata[i*2048], 1, 2048, fout);
        }
    }
    fwrite(sector, 1, 2048, fout);

    free(data);
    free(outdata);

    fclose(fin);
    fclose(fout);
#endif
    return 0;
}