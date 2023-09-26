
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

int scd_open_file(const char *name)
{
    int i;
    int length;
    char *scdfn = (uint8_t *)0x600000; /* word ram on MD side (in 1M mode) */

    if (name[0] != '/')
    {
        scdfn[0] = '/';
        scdfn++;
    }
    for (i = 0; name[i]; i++)
        *scdfn++ = name[i];
    *scdfn = 0;

    write_long(0xA12010, 0x0C0000); /* word ram on CD side (in 1M mode) */
    wait_do_cmd('F');
    wait_cmd_ack();
    length = read_long(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return length;
}

int scd_read_file(void *ptr, int length)
{
    write_long(0xA12010, (uintptr_t)ptr);
    write_long(0xA12014, length);
    wait_do_cmd('H');
    wait_cmd_ack();
    length = read_long(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return length;
}
