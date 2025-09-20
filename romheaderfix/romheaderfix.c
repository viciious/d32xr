#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv)
{
    bool show_usage = false;
    bool insert_date = true;
    char line[49];
    const char *mapper = argv[1];
    const char *title = argv[2];

    printf(
        "========================================\n"
        "Genesis/32X ROM Header Fixer-Upper 1.00\n"
        "Programmed by Saxman\n"
        "========================================\n"
    );

    if (argc < 4) {
        show_usage = true;
    }

    if (show_usage) {
        printf(
            "\n"
            "Usage: romhdrfx mapper title file...\n"
        );
        return -1;
    }

    FILE *rom = fopen(argv[argc-1], "r+b");

    if (rom == NULL) {
        printf("\nERROR: ROM file not found.\n");
        return -1;
    }

    // Get the ROM file size
    fseek(rom, 0, SEEK_END);
    unsigned int size = ftell(rom);
    rewind(rom);

    printf("ROM size boundary: 0x%06X\n", size-1);

    // Calculate the checksum
    unsigned short *words = (short *)malloc(size);
    fread(words, 2, (size>>1), rom);

    unsigned short checksum = 0;
    for (int i=(0x200>>1); i < (size>>1); i++)
    {
        checksum += ((words[i] << 8) | (words[i] >> 8));
    }

    free(words);

    printf("Calculated checksum: 0x%04X\n", checksum);

    if (insert_date) {
        // Get the current date
        time_t s;
        struct tm *date;

        time(&s);
        date = localtime(&s);

        char date_string[12];

        sprintf(date_string,
            "%04d%02d%02d-%02d", 
            date->tm_year + 1900, 
            date->tm_mon + 1, 
            date->tm_mday, /* Per spec, don't add 1 to the value! */
            date->tm_hour);

        printf("Date string: %s\n", date_string);

        // Update the timestamp (serial) in the ROM header
        fseek(rom, 0x183, SEEK_SET);
        fwrite(date_string, 1, 11, rom);
    }

    // Update checksum in the ROM header
    checksum = (checksum >> 8) | (checksum << 8);
    fseek(rom, 0x18E, SEEK_SET);
    fwrite(&checksum, 2, 1, rom);

    // Update ROM size in the ROM header
    size--;
    size = (size >> 24) | ((size >> 8) & 0xFF00) | ((size & 0xFF00) << 8) | (size << 24);
    fseek(rom, 0x1A4, SEEK_SET);
    fwrite(&size, 1, 4, rom);

    snprintf(line, 33, "%-32s", mapper);

    fseek(rom, 0x100, SEEK_SET);
    fprintf(rom, "%s", line);

    snprintf(line, 49, "%-32s", title);

    fseek(rom, 0x120, SEEK_SET);
    fprintf(rom, "%s", line);

    fseek(rom, 0x150, SEEK_SET);
    fprintf(rom, "%s", line);

    fclose(rom);

    printf("\nThe ROM file successfully been updated.\n");

    return 0;
}
