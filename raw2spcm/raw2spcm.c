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
    uint8_t sector[2048];

    fseek(fin, 0, SEEK_END);
    len = ftell(fin);
    data = malloc(len);
    outdata = malloc(len);
    fseek(fin, 0, SEEK_SET);
    fread(data, 1, len, fin);

    memset(sector, 0, sizeof(sector));
    fwrite(sector, 1, 2048, fout);

    for (i = 0; i < len; i++) {
        outdata[i] = rawu82spcm(data[i]);
    }

    fwrite(outdata, 1, len, fout);

    if (len % 2048 != 0)
        fwrite(sector, 1, 2048 - (len % 2048), fout);
    fwrite(sector, 1, 2048, fout);

    free(data);
    free(outdata);

    fclose(fin);
    fclose(fout);
#endif
    return 0;
}