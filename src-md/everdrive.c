#include <stdint.h>

#define SRAM_MAXSLOTS	10
#define SRAM_SLOTSIZE	200
#define SRAM_MAGIC1		0xDE
#define SRAM_MAGIC2		0xAD

#define MED_ATTR_DATA	__attribute__((section(".data")))

#define MED_SET_RV_1()	__asm volatile("movew #0x2700,%%sr\n\t" \
										"moveb #1,0xA15107\n\t" \
										: : : "cc"); // disable interrupts and set RV=1

#define MED_SET_RV_0()	__asm volatile("moveb #0,0xA15107\n\t" \
										"movew #0x2000,%%sr\n\t" \
										: : : "cc"); // set RV=0 and re-enable interrupts

uint16_t InitEverDrive(void) MED_ATTR_DATA;

uint16_t InitEverDrive(void)
{
	int i;
	const int bs = 0x100;
	volatile uint8_t *b0, *b2;
	int res = 0;

	MED_SET_RV_1();

	// map bank 0 to page 2 - if this is a standard mapper instead of MED, the
	//  MSB will do nothing, and the LSB disables and write-protects the sram
	__asm volatile("move.w #0x8002, 0xA130F0" : : : "memory");

	// compare first N bytes of banks 0 and 2
	b0 = (volatile uint8_t*)0x000100;
	b2 = (volatile uint8_t*)0x100100;
	for (i = 0; i < bs; i++)
	{
		if (b0[i] != b2[i])
			break;
	}

	if (i == bs)
	{
		res = 1; // MED detected

		// now check if v1/v2a MED, or v2b/v3 MED

		// map bank 0 to page 28 to look for save ram
		__asm volatile("move.w #0x801C, 0xA130F0" : : : "memory");
		b0 = (volatile uint8_t*)((SRAM_MAXSLOTS-1)*SRAM_SLOTSIZE);
		if (b0[7] == SRAM_MAGIC1 && b0[10] == SRAM_MAGIC2)
			res |= 0x0100; // old MED

		// map bank 0 back to page 0
		__asm volatile("move.w #0x8000, 0xA130F0" : : : "memory");
	}

	MED_SET_RV_0();

	return res;
}
