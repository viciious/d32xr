/*
 * SEGA CD Mode 1 Support
 * by Chilly Willy
 */

#include <stdint.h>
#include <string.h>
#include "scd_pcm.h"

extern uint16_t cd_ok;
extern uint16_t megasd_ok, megasd_num_cdtracks;
extern uint16_t everdrive_ok;

extern uint16_t InitMegaSD(void);
extern uint16_t InitEverDrive(void);

extern void do_main(void);
extern uint16_t InitCD(void);

int mystrlen(const char* string)
{
	int rc = 0;
	while (*(string++))
		rc++;
	return rc;
}

int main(void)
{
    cd_ok = InitCD();
    megasd_ok = InitMegaSD();
    everdrive_ok = 0;

    /* only attempt a MEDPro detection if a MegaSD is not present or is fake */
    if (megasd_ok == 0 || megasd_ok & 0x4) {
        everdrive_ok = InitEverDrive();
    }

    if (megasd_num_cdtracks == 1) {
        /* workaround a MD+ quirk where it exposes a single-track MD+ OST, */
        /* which only occurs if the letter 'C' is present in the ROM header */
        megasd_num_cdtracks = 0;
    }

    /*
     * Main loop in ram - you need to have it in ram to avoid bus contention
     * for the rom with the SH2s.
     */
    do_main(); // never returns

    return 0;
}
