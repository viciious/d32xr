/* f_main.c -- finale */

#include "doomdef.h"
#include "r_local.h"
#include "v_font.h"

static void F_DrawBackground(void);

typedef struct
{
	const char *piclumpName;
	const char *title;
	const char *didWhat;
	const char *name;
	const char *links;
} creditcard_t; // tap, chip, or swipe?

creditcard_t creditCards[] = {
	{"C_STJR", "BASED ON THE WORK BY", "Sonic Team Jr.", "www.srb2.org" },
	{"C_WSQUID", "MUSIC", "Wesquiid", "GFZ2", "x.com/@wessquiid\nwessquiid.carrd.co" },
	{"C_JXCHIP", "MUSIC", "JX Chip", "GFZ1\nERZ1\nERZ2", "youtube.com/@JXChip\nko-fi.com/jx85638" },
	{"C_JOYTAY", "MUSIC", "John \"Joy\" Tay", "Deep Sea 1", "x.com/@johntayjinf\nyoutube.com/@johntayjinf" },
	{"C_CRYPTK", "MUSIC", "Cryptik", "Miscellaneous", "x.com/@LunarCryptik\nyoutube.com/c/LunarCryptik\npatreon.com/LunarCryptik" },
	{"C_SAXMAN", "PROGRAMMING", "Saxman", "MegaDrive & 32X\nAdditional tooling", "rumble.com/user/ymtx81z" },
	{"C_SSN", "PROGRAMMING", "SSNTails", "Gameplay\nAdditional Art", "x.com/@SSNTails\nyoutube.com/@ssntails" },
	{"C_SPCTHX", "SPECIAL THANKS", "Viciious", "Doom 32X: Resurrection" },
	{"C_SPCTHX", "SPECIAL THANKS", "Muffin", "Mapping support" },
	{0, NULL, NULL, NULL, NULL },
};

#define CARDTIME (10*TICRATE)

static VINT cardPFP = 0;
static VINT cardTimer = 0;
static VINT curCard = 0;

static void F_DrawBackground(void)
{
	// Black background like Sonic 1 & 2
	DrawFillRect(0, 0, 320, 224, 255);
}

/*
==================
=
= BufferedDrawSprite
=
= Cache and draw a game sprite to the 8 bit buffered screen
==================
*/

void BufferedDrawSprite (int sprite, int frame, int rotation, int top, int left)
{
	spritedef_t	*sprdef;
	spriteframe_t	*sprframe;
	VINT 		*sprlump;
	patch_t		*patch;
	byte		*pixels, *src;
	int			x, sprleft, sprtop, spryscale;
	fixed_t 	spriscale;
	column_t	*column;
	int			lump;
	boolean		flip;
	int			texturecolumn;
	int			light = HWLIGHT(143);
	int 		height = I_FrameBufferHeight();

	if ((unsigned)sprite >= NUMSPRITES)
		I_Error ("BufferedDrawSprite: invalid sprite number %i "
		,sprite);
	sprdef = &sprites[sprite];
	if ( (frame&FF_FRAMEMASK) >= sprdef->numframes )
		I_Error ("BufferedDrawSprite: invalid sprite frame %i : %i "
		,sprite,frame);
	sprframe = &spriteframes[sprdef->firstframe + (frame & FF_FRAMEMASK)];
	sprlump = &spritelumps[sprframe->lump];

	if (sprlump[rotation] != -1)
		lump = sprlump[rotation];
	else
		lump = sprlump[0];

	flip = false;
	if (lump < 0)
	{
		lump = -(lump + 1);
		flip = true;
	}

	if (lump <= 0)
		return;

	patch = (patch_t *)W_POINTLUMPNUM(lump);
	pixels = R_CheckPixels(lump + 1);

	S_UpdateSounds ();
	 	
/* */
/* coordinates are in a 160*112 screen (doubled pixels) */
/* */
	sprtop = top;
	sprleft = left;
	spryscale = 2;
	spriscale = FRACUNIT/spryscale;

	sprtop -= patch->topoffset;
	sprleft -= patch->leftoffset;

/* */
/* draw it by hand */
/* */
	for (x=0 ; x<patch->width ; x++)
	{
		if (sprleft+x < 0)
			continue;
		if (sprleft+x >= 160)
			break;

		if (flip)
			texturecolumn = patch->width-1-x;
		else
			texturecolumn = x;
			
		column = (column_t *) ((byte *)patch +
		 BIGSHORT(patch->columnofs[texturecolumn]));

/* */
/* draw a masked column */
/* */
		for ( ; column->topdelta != 0xff ; column++) 
		{
			int top    = column->topdelta + sprtop;
			int bottom = top + column->length - 1;

			top *= spryscale;
			bottom *= spryscale;
			src = pixels + BIGSHORT(column->dataofs);

			if (top < 0) top = 0;
			if (bottom >= height) bottom = height - 1;
			if (top > bottom) continue;

//			fin->drcol(sprleft+x, top, bottom, light, 0, spriscale, src, 128);
		}
	}
}


/*============================================================================ */

/*
=======================
=
= F_Start
=
=======================
*/

static void F_NextCard()
{
	curCard++;

	if (!creditCards[curCard].title)
	{
		// We are done
		F_Stop();
		return;
	}

	cardTimer = CARDTIME;
	cardPFP = W_GetNumForName(creditCards[curCard].piclumpName);
}

void F_Start (void)
{
	S_StartSong(gameinfo.endMus, 1, cdtrack_end);

	// Set this to black, prep for fade-in.
	const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
	I_SetPalette(dc_playpals+5*768);

	R_InitColormap(true);

	curCard = -1;
	F_NextCard();
}

void F_Stop (void)
{
	int	i;

	R_InitColormap(lowResMode);
	
	// Cleanup
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
		F_NextCard();

	if (cardTimer > CARDTIME - 20)
	{
		int palIndex = cardTimer - (CARDTIME - 20);
		palIndex /= 4;
		if (palIndex > 5)
			palIndex = 5;

		const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
		I_SetPalette(dc_playpals+palIndex*768);
	}
	else if (cardTimer < 20)
	{
		int palIndex = (cardTimer / 4);

		const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
		I_SetPalette(dc_playpals+palIndex*768);
	}

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

	creditcard_t *card = &creditCards[curCard];
	DrawJagobjLump(cardPFP, 32, 96, NULL, NULL);

	V_DrawStringCenter(&menuFont, 64, 96+100, card->name);

	V_DrawStringCenter(&creditFont, 160, 64, card->title);
	V_DrawStringLeft(&titleFont, 160, 128, card->didWhat);
	V_DrawStringLeft(&menuFont, 160, 144, card->links);
}

