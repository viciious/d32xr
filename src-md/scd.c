
#include <stdint.h>
#include <string.h>
#include <stdio.h>

extern uint32_t vblank_vector;
extern uint16_t gen_lvl2;

extern uint32_t Sub_Start;
extern uint32_t Sub_End;

extern void Kos_Decomp(uint8_t *src, uint8_t *dst);

extern void write_byte(unsigned int dst, unsigned char val);
extern void write_word(unsigned int dst, unsigned short val);
extern void write_long(unsigned int dst, unsigned int val);
extern unsigned char read_byte(unsigned int src);
extern unsigned short read_word(unsigned int src);
extern unsigned int read_long(unsigned int src);

void scd_delay(void) __attribute__((section(".data"), aligned(16)));
char wait_cmd_ack(void) __attribute__((section(".data"), aligned(16)));
void wait_do_cmd(char cmd) __attribute__((section(".data"), aligned(16)));

extern void scd_init_pcm(void);
extern void bump_fm(void);

void scd_delay(void)
{
    int cnt = 50;
    do {
        asm __volatile("nop");
    } while (--cnt);
}

char wait_cmd_ack(void)
{
    char ack = 0;

    do {
        scd_delay();
        bump_fm();
        ack = read_byte(0xA1200F); // wait for acknowledge byte in sub comm port
    } while (!ack);

    return ack;
}

void wait_do_cmd(char cmd)
{
    while (read_byte(0xA1200F)) {
        scd_delay(); // wait until Sub-CPU is ready to receive command
        bump_fm();
    }
    write_byte(0xA1200E, cmd); // set main comm port to command
}

int64_t scd_open_file(const char *name)
{
    int i;
    char *scdfn = (char *)0x600000; /* word ram on MD side (in 1M mode) */
    union {
        int32_t lo[2];
        int64_t value;
    } handle;

    for (i = 0; name[i]; i++)
        *scdfn++ = name[i];
    *scdfn = 0;

    write_long(0xA12010, 0x0C0000); /* word ram on CD side (in 1M mode) */
    wait_do_cmd('F');
    wait_cmd_ack();
    handle.lo[0] = read_long(0xA12020);
    handle.lo[1] = read_long(0xA12024);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result

    return handle.value;
}

int64_t scd_read_directory(char *buf)
{
    int i, numwords;
    char *scdWordRam = (char *)0x600000; /* word ram on MD side (in 1M mode) */
    union {
        int32_t ln[2]; /* length, num entries */
        int64_t value;
    } res;

    memcpy(scdWordRam, buf, strlen(buf)+1);

    write_long(0xA12010, 0x0C0000); /* word ram on CD side (in 1M mode) */
    wait_do_cmd('M'); // ReadDir command
    wait_cmd_ack();
    res.ln[0] = read_long(0xA12020);
    res.ln[1] = read_long(0xA12024);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result

    if (res.value < 0)
        return res.value;

    // use word copy so that zero-byte writes aren't ignored for the fb
#if 1
    numwords = (res.ln[0] + 1)/2;
    for (i = 0; i < numwords; i++) {
        ((volatile int16_t *)buf)[i] = ((volatile int16_t *)scdWordRam)[i];
    }
#else
    memcpy(buf, scdWordRam, res.ln[0]);
#endif

    return res.value;
}

void scd_read_sectors(void *ptr, int lba, int len, void (*wait)(void))
{
    char ack = 0;
    if (!wait)
        wait = scd_delay;

    write_long(0xA12010, (uintptr_t)ptr);
    write_long(0xA12014, lba);
    write_long(0xA12018, len);
    wait_do_cmd('H');
    do {
        // do some useful work while waiting
        wait();
        ack = read_byte(0xA1200F); // wait for acknowledge byte in sub comm port
    } while (!ack);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_switch_to_bank(int bank)
{
    write_byte(0xA12010, bank);
    wait_do_cmd('J');
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_play_spcm_track(const char *name, int repeat)
{
    char *scdWordRam = (char *)0x600000; /* word ram on MD side (in 1M mode) */

    memcpy(scdWordRam, name, strlen(name)+1);
    write_long(0xA12010, 0x0C0000); /* word ram on CD side (in 1M mode) */
    write_long(0xA12014, repeat);
    wait_do_cmd('Q'); // PlaySPCMTrack command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_stop_spcm_track(const char *name)
{
    wait_do_cmd('R'); // StopSPCMTrack command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_resume_spcm_track(void)
{
    wait_do_cmd('X'); // ResumeSPCMTrack command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

uint16_t InitCD(void)
{
    char *bios;

    /*
     * Check for CD BIOS
     * When a cart is inserted in the MD, the CD hardware is mapped to
     * 0x400000 instead of 0x000000. So the BIOS ROM is at 0x400000, the
     * Program RAM bank is at 0x420000, and the Word RAM is at 0x600000.
     */
    bios = (char *)0x415800;
    if (memcmp(bios + 0x6D, "SEGA", 4))
    {
        bios = (char *)0x416000;
        if (memcmp(bios + 0x6D, "SEGA", 4))
        {
            // check for WonderMega/X'Eye
            if (memcmp(bios + 0x6D, "WONDER", 6))
            {
                bios = (char *)0x41AD00; // might also be 0x40D500
                // check for LaserActive
                if (memcmp(bios + 0x6D, "SEGA", 4))
                    return 0; // no CD
            }
        }
    }

    /*
     * Reset the Gate Array - this specific sequence of writes is recognized by
     * the gate array as a reset sequence, clearing the entire internal state -
     * this is needed for the LaserActive
     */
    write_word(0xA12002, 0xFF00);
    write_byte(0xA12001, 0x03);
    write_byte(0xA12001, 0x02);
    write_byte(0xA12001, 0x00);

    /*
     * Reset the Sub-CPU, request the bus
     */
    write_byte(0xA12001, 0x02);
    while (!(read_byte(0xA12001) & 2)) write_byte(0xA12001, 0x02); // wait on bus acknowledge

    /*
     * Decompress Sub-CPU BIOS to Program RAM at 0x00000
     */
    write_word(0xA12002, 0x0002); // no write-protection, bank 0, 2M mode, Word RAM assigned to Sub-CPU
    memset((char *)0x420000, 0, 0x20000); // clear program ram first bank - needed for the LaserActive
    Kos_Decomp((uint8_t *)bios, (uint8_t *)0x420000);

    /*
     * Copy Sub-CPU program to Program RAM at 0x06000
     */
    memcpy((char *)0x426000, (char *)&Sub_Start, (int)&Sub_End - (int)&Sub_Start);

    write_byte(0xA1200E, 0x00); // clear main comm port
    write_byte(0xA12002, 0x2A); // write-protect up to 0x05400
    write_byte(0xA12001, 0x01); // clear bus request, deassert reset - allow CD Sub-CPU to run
    while (!(read_byte(0xA12001) & 1)) write_byte(0xA12001, 0x01); // wait on Sub-CPU running

    /*
     * Set the vertical blank handler to generate Sub-CPU level 2 ints.
     * The Sub-CPU BIOS needs these in order to run.
     */
    gen_lvl2 = 1; // generate Level 2 IRQ to Sub-CPU

    /*
     * Wait for Sub-CPU program to set sub comm port indicating it is running -
     * note that unless there's something wrong with the hardware, a timeout isn't
     * needed... just loop until the Sub-CPU program responds, but 2000000 is about
     * ten times what the LaserActive needs, and the LA is the slowest unit to
     * initialize
     */
    while (read_byte(0xA1200F) != 'I')
    {
        static int timeout = 0;
        timeout++;
        if (timeout > 2000000)
        {
            gen_lvl2 = 0;
            return 0; // no CD
        }
    }

    /*
     * Wait for Sub-CPU to indicate it is ready to receive commands
     */
    while (read_byte(0xA1200F) != 0x00) ;

    scd_init_pcm();

    return 0x1; // CD ready to go!
}
