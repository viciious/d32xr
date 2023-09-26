
#include <stdint.h>

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

int scd_open_file(char *name)
{
    int i;
    int length;
    char *scdfn = (char *)0x600000; /* word ram on MD side (in 1M mode) */

    for (i = 0; name[i]; i++)
        *scdfn++ = name[i];
    *scdfn = 0;

    write_long(0xA12010, 0x0C0000); /* word ram on CD side (in 1M mode) */
    wait_do_cmd('F');
    wait_cmd_ack();
    length = read_long(0xA12020);
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

    return length;
}

int scd_read_file(void *ptr, int length)
{
    write_long(0xA12010, (uintptr_t)0x0C0000); /* word ram on CD side (in 1M mode) */
    write_long(0xA12014, length);
    wait_do_cmd('H');
    wait_cmd_ack();
    length = read_long(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result

    if ((intptr_t)ptr < 0x600000 || (uintptr_t)ptr >= 0x640000)
    {
        int i;
        int words;
        short *wordRam = (short *)0x600000; /* word ram on MD side (in 1M mode) */

        // copy from wordRam to destination buffer
        words = length / 2;
        for (i = 0; i < words; i++) {
            ((short *)ptr)[i] = wordRam[i];
        }
        ((short *)ptr)[i] = 0;
        ((short *)ptr)[i+1] = 0; // NULL-termination of strings of even length

        if (length & 1) {
            ((char *)ptr)[length - 1] = ((char *)wordRam)[length - 1];
        }
    }

    return length;
}

int scd_seek_file(int offset, int whence)
{
    write_long(0xA12010, whence); /* word ram on CD side (in 1M mode) */
    write_long(0xA12014, offset);
    wait_do_cmd('J');
    wait_cmd_ack();
    offset = read_long(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return offset;
}
