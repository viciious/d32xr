/* f_main.c -- finale */

#include "r_local.h"
#include "v_font.h"

typedef struct
{
	const char *piclumpName;
	const char *title;
	const char *name;
	const char *didWhat;
	const char *links;
} creditcard_t; // tap, chip, or swipe?

creditcard_t creditCards[] = {
	{"C_STJR",   "SONIC ROBO BLAST",     "32X",              "STAFF", "" },
	{"C_WSQUID", "MUSIC",                "Wesquiid",         "Greenflower 2",                       "x.com/@wessquiid\nwessquiid.carrd.co" },
	{"C_JXCHIP", "MUSIC",                "JX Chip",          "Greenflower 1\nEgg Rock 1 & 2",       "youtube.com/@JXChip\nko-fi.com/jx85638" },
	{"C_JOYTAY", "MUSIC",                "John \"Joy\" Tay", "Deep Sea 1",                          "x.com/@johntayjinf\nyoutube.com\n/@johntayjinf" },
	{"C_CRYPTK", "MUSIC",                "Cryptik",          "Miscellaneous",                       "x.com/@LunarCryptik\nyoutube.com/c\n/LunarCryptik\npatreon.com\n/LunarCryptik" },
	{"C_SAXMAN", "PROGRAMMING",          "Saxman",           "MegaDrive & 32X\nAdditional tooling", "rumble.com/user\n/ymtx81z" },
	{"C_SSN",    "PROGRAMMING",          "SSNTails",         "Gameplay\nAdditional Art",            "x.com/@SSNTails\nyoutube.com\n/@ssntails" },
	{"C_VIC",    "SPECIAL THANKS",       "Viciious",         "Doom 32X:\nResurrection",             "github.com/viciious" },
	{"C_MITTEN", "SPECIAL THANKS",       "Mittens",          "Mapping support",                     "youtube.com\n/@Mittens0407\ntwitch.tv\n/mittens0407" },
	{"C_STJR",   "BASED ON THE WORK BY", "Sonic Team Jr.",   "www.srb2.org",                        "Shout-outs to:\nAlice Alacroix\nMotor Roach\nNev3r\nGuyWithThePie\nAnd so many more!" },
	{NULL,       "THANKS FOR PLAYING!", "", "", "" },
	{NULL, NULL, NULL, NULL, NULL },
};

#define CARDTIME (20*TICRATE)

static VINT cardPFP = 0;
static VINT cardTimer = 0;
static VINT curCard = 0;

static void F_DrawBackground(void)
{
	// Black background like Sonic 1 & 2
	DrawFillRect(0, 0, 320, 224, 31);
}

/*============================================================================ */

/*
=======================
=
= F_Start
=
=======================
*/

static boolean F_NextCard()
{
	curCard++;

	if (!creditCards[curCard].title)
	{
		// We are done
		F_Stop();
		return false;
	}

	cardTimer = CARDTIME;
	if (creditCards[curCard].piclumpName)
		cardPFP = W_GetNumForName(creditCards[curCard].piclumpName);
	else
		cardPFP = 0;

	return true;
}

void F_Start (void)
{
	S_StartSong(gameinfo.endMus, 1, cdtrack_end);

	// Set this to black, prep for fade-in.
	const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
	I_SetPalette(dc_playpals+5*768);

	R_InitColormap();

	curCard = -1;
	F_NextCard();
}

void F_Stop (void)
{
	const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
	I_SetPalette(dc_playpals);

	R_InitColormap();
	
	// Cleanup
	// TODO: Get the game to go back to title
}

/*
=======================
=
= F_Ticker
=
=======================
*/

int F_Ticker (void)
{
	cardTimer--;

	if (cardTimer < 0)
	{
		if (!F_NextCard())
			return 1;
	}

	const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));

	if (cardTimer >= CARDTIME - 16)
	{
		int palIndex = cardTimer - (CARDTIME - 16);
		palIndex /= 4;
		if (palIndex > 4)
			palIndex = 4;

		palIndex += 6;
		I_SetPalette(dc_playpals+palIndex*768);
	}
	else if (cardTimer < 20)
	{
		int palIndex = 10 - (cardTimer / 4);
		I_SetPalette(dc_playpals+palIndex*768);
	}
	else
		I_SetPalette(dc_playpals);

	return 0;
}

/*
=======================
=
= F_Drawer
=
=======================
*/

void F_Drawer (void)
{
	// HACK
	viewportWidth = 320;
	viewportHeight = I_FrameBufferHeight();
	viewportbuffer = (pixel_t*)I_FrameBuffer();

	F_DrawBackground();

	const creditcard_t *card = &creditCards[curCard];

	if (curCard == 0)
	{
		V_DrawStringCenter(&creditFont, 160, 112-8-24, card->title); // SONIC ROBO BLAST
		V_DrawStringCenter(&creditFont, 160, 112-8, card->name); // 32X
		V_DrawStringCenter(&creditFont, 160, 112-8+16+8, card->didWhat); // STAFF
	}
	else
	{
		V_DrawStringCenter(&creditFont, 160, cardPFP ? 32 : 112-8, card->title);

		if (cardPFP)
			DrawJagobjLump(cardPFP, 32, 64, NULL, NULL);

		if (card->name)
			V_DrawStringCenter(&menuFont, 80, 64+100, card->name);

		if (card->didWhat)
			V_DrawStringLeft(&menuFont, 160-16, 64, card->didWhat);

		if (card->links)
			V_DrawStringLeft(&menuFont, 160-16, 96, card->links);
	}
}

