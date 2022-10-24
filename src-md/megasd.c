#include <stdint.h>

#define MEGASD_ATTR_DATA		 __attribute__((section(".data")))

#define MEGASD_COMM_OVERLAY		*(volatile uint16_t *)0x3F7FA
#define MEGASD_OVERLAY_MAGIC	0xCD54

#define MEGASD_CTRL_PORT		*(volatile uint16_t *)0x3F7FE
#define MEGASD_RSLT_PORT		*(volatile uint8_t *)0x3F7FC
#define MEGASD_DATA_AREA		(volatile uint8_t *)0x3F800

#define MEGASD_CMD_GET_SERIAL	0x1000
#define MEGASD_CMD_PLAYCD_ONCE	0x1100
#define MEGASD_CMD_PLAYCD_LOOP	0x1200
#define MEGASD_CMD_PAUSECD		0x1300
#define MEGASD_CMD_SETVOLUME	0x1500
#define MEGASD_CMD_GET_NUMTRKS	0x2700

#define MEGASD_MAX_CDTRACKS		0x63

#define MEGASD_32X_SET_RV_1()	__asm volatile("movew #0x2700,%%sr\n\t" \
												"moveb #1,0xA15107\n\t" \
												: : : "cc"); // disable interrupts and set RV=1

#define MEGASD_32X_SET_RV_0()	__asm volatile("moveb #0,0xA15107\n\t" \
												"movew #0x2000,%%sr\n\t" \
												: : : "cc"); // set RV=0 and re-enable interrupts

extern uint16_t megasd_num_cdtracks;

uint16_t InitMegaSD(void) MEGASD_ATTR_DATA;
void MegaSD_PlayCDTrack(uint16_t track, uint16_t loop) MEGASD_ATTR_DATA;
void MegaSD_PauseCD(void) MEGASD_ATTR_DATA;
void MegaSD_SetCDVolume(int volume) MEGASD_ATTR_DATA;
static uint16_t ProtectedInitMegaSD(void) MEGASD_ATTR_DATA;

static uint16_t ProtectedInitMegaSD(void)
{
	char* idstr;
	const int timeout = 50000;

#define MEGASD_WAIT_CMD_RSLT(t) do { \
		volatile int timeout_ = t; \
		while (MEGASD_CTRL_PORT != 0) { \
			if (timeout_-- == 0) break; \
		} \
	} while (0)

	/* Retrieve the MegaSD version and serial number. */
	MEGASD_CTRL_PORT = MEGASD_CMD_GET_SERIAL;
	MEGASD_WAIT_CMD_RSLT(timeout);

	idstr = (char*)MEGASD_DATA_AREA;
	if (idstr[0] == 'M' &&
		idstr[1] == 'E' &&
		idstr[2] == 'G' &&
		idstr[3] == 'A' &&
		idstr[4] == 'S' &&
		idstr[5] == 'D')
	{
		MEGASD_CTRL_PORT = MEGASD_CMD_GET_NUMTRKS;
		MEGASD_WAIT_CMD_RSLT(timeout);
		megasd_num_cdtracks = MEGASD_RSLT_PORT;
		return 0x0003; // MD+, CD OK
	}

#undef MEGASD_WAIT_CMD_RSLT

	return 0;
}

uint16_t InitMegaSD(void)
{
	uint16_t res;

	MEGASD_32X_SET_RV_1();

	MEGASD_COMM_OVERLAY = MEGASD_OVERLAY_MAGIC;
	res = ProtectedInitMegaSD();
	MEGASD_COMM_OVERLAY = 0;

	MEGASD_32X_SET_RV_0();

	return res;
}

void MegaSD_PlayCDTrack(uint16_t track, uint16_t loop)
{
	if (megasd_num_cdtracks == 0)
		return;
	//if (track > MEGASD_MAX_CDTRACKS)
	//	track = MEGASD_MAX_CDTRACKS;

	MEGASD_32X_SET_RV_1();

	MEGASD_COMM_OVERLAY = MEGASD_OVERLAY_MAGIC;
	MEGASD_CTRL_PORT = (loop ? MEGASD_CMD_PLAYCD_LOOP : MEGASD_CMD_PLAYCD_ONCE) | track;
	MEGASD_COMM_OVERLAY = 0;

	MEGASD_32X_SET_RV_0();
}

void MegaSD_PauseCD(void)
{
	if (megasd_num_cdtracks == 0)
		return;

	MEGASD_32X_SET_RV_1();

	MEGASD_COMM_OVERLAY = MEGASD_OVERLAY_MAGIC;
	MEGASD_CTRL_PORT = MEGASD_CMD_PAUSECD;
	MEGASD_COMM_OVERLAY = 0;

	MEGASD_32X_SET_RV_0();
}

void MegaSD_SetCDVolume(int volume)
{
	MEGASD_32X_SET_RV_1();

	MEGASD_COMM_OVERLAY = MEGASD_OVERLAY_MAGIC;
	MEGASD_CTRL_PORT = MEGASD_CMD_SETVOLUME|volume;
	MEGASD_COMM_OVERLAY = 0;

	MEGASD_32X_SET_RV_0();
}
