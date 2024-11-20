#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "scd_pcm.h"

typedef struct
{
    uint32_t cmd;
    uint16_t arg[6];
} scd_cmd_t;

#define MAX_SCD_CMDS    16

extern void write_byte(unsigned int dst, unsigned char val);
extern void write_word(unsigned int dst, unsigned short val);
extern void write_long(unsigned int dst, unsigned int val);
extern unsigned char read_byte(unsigned int src);
extern unsigned short read_word(unsigned int src);
extern unsigned int read_long(unsigned int src);

static scd_cmd_t scd_cmds[MAX_SCD_CMDS];
static int16_t num_scd_cmds;

static void scd_delay(void) SCD_CODE_ATTR;
static char wait_cmd_ack(void) SCD_CODE_ATTR;
static void wait_do_cmd(char cmd) SCD_CODE_ATTR;

static int mystrlen(const char* string)
{
	int rc = 0;

	while (*(string++))
		rc++;

	return rc;
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

void scd_init_pcm(void)
{
    /*
    * Initialize the PCM driver
    */
    wait_do_cmd('I');
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

/* data must be pointing to a memory area in SCD WORD RAM (0x0Cxxxx in 1M mode)*/
void scd_setptr_buf(uint16_t buf_id, const uint8_t *data, uint32_t data_len)
{
    write_word(0xA12010, buf_id); /* buf_id */
    write_word(0xA12012, 1); /* set pointer mode */
    write_long(0xA12014, (intptr_t)data); /* word ram on CD side */
    write_long(0xA12018, data_len); /* sample length */
    wait_do_cmd('B'); // SfxCopyBuffer command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_upload_buf(uint16_t buf_id, const uint8_t *data, uint32_t data_len)
{
    uint8_t *scdWordRam = (uint8_t *)0x600000;

    memcpy(scdWordRam, data, data_len);

    write_word(0xA12010, buf_id); /* buf_id */
    write_word(0xA12012, 0); /* copy data mode */
    write_long(0xA12014, 0x0C0000); /* word ram on CD side (in 1M mode) */
    write_long(0xA12018, data_len); /* sample length */
    wait_do_cmd('B'); // SfxCopyBuffer command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_upload_buf_fileofs(uint16_t buf_id, int numsfx, const uint8_t *data)
{
    int filelen;
    char *scdWordRam = (char *)0x600000;

    // copy filename
    filelen = mystrlen((char *)data);
    memcpy(scdWordRam, data, filelen+1);

    // copy offsets and lengths
    scdWordRam = (void *)(((uintptr_t)scdWordRam + filelen + 1 + 3) & ~3);
    data = (void *)(((uintptr_t)data + filelen + 1 + 3) & ~3);
    memcpy(scdWordRam, data, numsfx*2*sizeof(int32_t));

    write_word(0xA12010, buf_id); /* buf_id */
    write_word(0xA12012, numsfx); /* num samples */
    write_long(0xA12014, 0x0C0000); /* word ram on CD side (in 1M mode) */
    wait_do_cmd('K'); // SfxCopyBuffer command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

uint8_t scd_play_src(uint8_t src_id, uint16_t buf_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop)
{
    write_long(0xA12010, ((unsigned)src_id<<16)|buf_id); /* src|buf_id */
    write_long(0xA12014, ((unsigned)freq<<16)|pan); /* freq|pan */
    write_long(0xA12018, ((unsigned)vol<<16)|autoloop); /* vol|autoloop */
    wait_do_cmd('A'); // SfxPlaySource command
    wait_cmd_ack();
    src_id = read_byte(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return src_id;
}

uint8_t scd_punpause_src(uint8_t src_id, uint8_t paused)
{
    write_long(0xA12010, ((unsigned)src_id<<16)|paused); /* src|paused */
    wait_do_cmd('N'); // SfxPUnPSource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return src_id;
}

void scd_update_src(uint8_t src_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop)
{
    write_long(0xA12010, ((unsigned)src_id<<16)); /* src|0 */
    write_long(0xA12014, ((unsigned)freq<<16)|pan); /* freq|pan */
    write_long(0xA12018, ((unsigned)vol<<16)|autoloop); /* vol|autoloop */
    wait_do_cmd('U'); // SfxUpdateSource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

uint16_t scd_getpos_for_src(uint8_t src_id)
{
    uint16_t pos;
    write_long(0xA12010, src_id<<16);
    wait_do_cmd('G'); // SfxGetSourcePosition command
    wait_cmd_ack();
    pos = read_word(0xA12020);
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
    return pos;
}

void scd_stop_src(uint8_t src_id)
{
    write_long(0xA12010, ((unsigned)src_id<<16)); /* src|0 */
    wait_do_cmd('O'); // SfxStopSource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_rewind_src(uint8_t src_id)
{
    write_long(0xA12010, ((unsigned)src_id<<16)); /* src|0 */
    wait_do_cmd('W'); // SfxRewindSource command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

void scd_clear_pcm(void)
{
    wait_do_cmd('L'); // SfxClear command
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result
}

int scd_get_playback_status(void)
{
    return read_byte(0xA1202F);
}

uint8_t scd_queue_play_src(uint8_t src_id, uint16_t buf_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop)
{
    scd_cmd_t *cmd = scd_cmds + num_scd_cmds;
    if (num_scd_cmds >= MAX_SCD_CMDS)
        return 0;
    cmd->cmd = 'A';
    cmd->arg[0] = src_id;
    cmd->arg[1] = buf_id;
    cmd->arg[2] = freq;
    cmd->arg[3] = pan;
    cmd->arg[4] = vol;
    cmd->arg[5] = autoloop;
    num_scd_cmds++;
    return 0;
}

void scd_queue_update_src(uint8_t src_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop)
{
    scd_cmd_t *cmd = scd_cmds + num_scd_cmds;
    if (num_scd_cmds >= MAX_SCD_CMDS)
        return;
    cmd->cmd = 'U';
    cmd->arg[0] = src_id;
    cmd->arg[1] = freq;
    cmd->arg[2] = pan;
    cmd->arg[3] = vol;
    cmd->arg[4] = autoloop;
    num_scd_cmds++;
}

void scd_queue_setptr_buf(uint16_t buf_id, const uint8_t *data, int len)
{
    scd_cmd_t *cmd = scd_cmds + num_scd_cmds;
    intptr_t offset = (intptr_t)data;
    if (num_scd_cmds >= MAX_SCD_CMDS)
        return;
    cmd->cmd = 'B';
    cmd->arg[0] = buf_id;
    cmd->arg[1] = offset>>16;
    cmd->arg[2] = offset&0xffff;
    cmd->arg[3] = len>>16;
    cmd->arg[4] = len&0xffff;
    num_scd_cmds++;
}

void scd_queue_stop_src(uint8_t src_id)
{
    scd_cmd_t *cmd = scd_cmds + num_scd_cmds;
    if (num_scd_cmds >= MAX_SCD_CMDS)
        return;
    cmd->cmd = 'S';
    cmd->arg[0] = src_id;
    num_scd_cmds++;
}

void scd_queue_clear_pcm(void)
{
    scd_cmd_t *cmd = scd_cmds + num_scd_cmds;
    if (num_scd_cmds >= MAX_SCD_CMDS)
        return;
    cmd->cmd = 'L';
    num_scd_cmds++;
}

int scd_flush_cmd_queue(void)
{
    int i;
    scd_cmd_t *cmd;

    if (!num_scd_cmds) {
        return 0;
    }

    write_byte(0xA12010, 1);
    wait_do_cmd('E'); // suspend the mixer/decoder
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result

    for (i = 0, cmd = scd_cmds; i < num_scd_cmds; i++, cmd++) {
        switch (cmd->cmd) {
            case 'A':
                scd_play_src(cmd->arg[0], cmd->arg[1], cmd->arg[2], cmd->arg[3], cmd->arg[4], cmd->arg[5]);
                break;
            case 'U':
                scd_update_src(cmd->arg[0], cmd->arg[1], cmd->arg[2], cmd->arg[3], cmd->arg[4]);
                break;
            case 'S':
                scd_stop_src(cmd->arg[0]);
                break;
            case 'L':
                scd_clear_pcm();
                break;
            case 'B':
                scd_setptr_buf(cmd->arg[0], (void*)((cmd->arg[1]<<16)|cmd->arg[2]), (cmd->arg[3]<<16)|cmd->arg[4]);
                break;
            default:
                break;
        }
    }

    write_byte(0xA12010, 0);
    wait_do_cmd('E'); // unsuspend
    wait_cmd_ack();
    write_byte(0xA1200E, 0x00); // acknowledge receipt of command result

    num_scd_cmds = 0;
    return i;
}

static void scd_delay(void)
{
    int cnt = 50;
    do {
        asm __volatile("nop");
    } while (--cnt);
}
