#include <stdint.h>

#define MED_ATTR_DATA		 __attribute__((section(".data")))

int InitEverDrive(void) MED_ATTR_DATA;

int InitEverDrive(void)
{
	int i;
	const int bs = 0x100;
	volatile uint8_t *b0, *b2;
	int res = 0;

	// map bank 0 to page 2
	__asm volatile("move.w #0x8002, 0xA130F0" : : : "memory");

	// compare first N bytes of banks 0 and 2
	b0 = (volatile uint8_t*)0x100;
	b2 = (volatile uint8_t*)0x100100;
	for (i = 0; i < bs; i++)
	{
		if (b0[i] != b2[i])
			break;
	}

	if (i == bs)
	{
		res = 1; // MED Pro detected
		res |= 0x0100; // Extended SSF detected
	}

	// map bank 0 back to page 0
	__asm volatile("move.w #0x8000, 0xA130F0" : : : "memory");
	return res;
}
