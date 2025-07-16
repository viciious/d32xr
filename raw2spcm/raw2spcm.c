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
    int freq = argc > 3 ? atoi(argv[3]) : 0;
    int numc = argc > 4 ? atoi(argv[4]) : 2;
    int incr = (freq * 2048 / 32604);
    int i, j, k, len, test = 1, outlen;
    int loop_markers = 0;
    int chunk_sectors = 1;
    uint8_t *data;
    uint8_t *outdata, *outend;
    int num_channels = numc ? numc : 2;
    uint8_t sector[2048];

    fseek(fin, 0, SEEK_END);

    len = ftell(fin);   
    data = malloc(len);

    outlen = (len + 2047)/2048; // number of sectors
    outlen = outlen*(2048+32); //  max 32 bytes of loop markers per sector
    outlen = (outlen + 2047)/2048; // number of sectors
    outlen += 2;
    outlen *= 2048;
    outdata = malloc(outlen);

    fseek(fin, 0, SEEK_SET);
    fread(data, 1, len, fin);

    memset(sector, 0, sizeof(sector));
    memset(outdata, 0, outlen);

    sector[0] = 'S';
    sector[1] = 'P';
    sector[2] = 'C';
    sector[3] = 'M';

    sector[4] = (freq >> 8) & 0xff;
    sector[5] = (freq >> 0) & 0xff;
    sector[6] = (incr >> 8) & 0xff;
    sector[7] = (incr >> 0) & 0xff;

    sector[8] = numc;
    
    fwrite(sector, 1, 2048, fout);

    memset(sector, 0, sizeof(sector));

    if (num_channels == 1) {
        uint8_t *left;
        int num_samples = len;

        left = outdata;

        i = 0;
        for (k = 0; k < len; ) {
            int chunk = chunk_sectors*2048 - loop_markers;
            if (k + chunk > num_samples)
                chunk = num_samples - k;

            for (j = 0; j < chunk; j++) {
                left[j] = rawu82spcm(data[k++]);
            }
            left += j;

            if ((chunk % 2048) + loop_markers > 0)
            {
                for (j = 0; j < chunk % 2048; j++)
                {
                    left[j] = rawu82spcm(data[k++]);
                }
                left += j;

                for (j = 0; j < loop_markers; j++) {
                    left[j] = 0xff;
                }
                left += j;

                left += 2048;
            }
        }

        outend = left;
    }
    else {
        uint8_t *left, *right;
        int num_samples = len / 2;

        left = outdata;
        right = outdata + 2048;

        i = 0;
        for (k = 0; k < len; ) {
            int c;
            int chunk = chunk_sectors*2048 - loop_markers;
            if (k/2 + chunk > num_samples)
                chunk = num_samples - k/2;

            int num_sectors = chunk / 2048;

            for (c = 0; c < num_sectors; c++)
            {
                for (j = 0; j < 2048; j++)
                {
                    left[j] = rawu82spcm(data[k++]);
                    right[j] = rawu82spcm(data[k++]);
                }
                left += j;
                right += j;

                left += 2048;
                right += 2048;
            }

            if ((chunk % 2048) + loop_markers > 0)
            {
                for (j = 0; j < chunk % 2048; j++)
                {
                    left[j] = rawu82spcm(data[k++]);
                    right[j] = rawu82spcm(data[k++]);
                }
                left += j;
                right += j;

                for (j = 0; j < loop_markers; j++) {
                    left[j] = 0xff;
                    right[j] = 0xff;
                }
                left += j;
                right += j;

                left += 2048;
                right += 2048;
            }
        }

        outend = right;
    }

    int num_sectors = (outend - outdata + 2047) / 2048;
    for (i = 0; i < num_sectors; i++)
    {
        fwrite(&outdata[i*2048], 1, 2048, fout);
    }

    fwrite(sector, 1, 2048, fout);

    free(data);
    free(outdata);

    fclose(fin);
    fclose(fout);
#endif
    return 0;
}