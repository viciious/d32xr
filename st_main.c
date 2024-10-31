/* st_main.c -- status bar */

#include "doomdef.h"
#include "st_main.h"
#include "v_font.h"
#include "st_inter.h"
#include <stdio.h>
#include "r_local.h"
#include "mars.h"

stbar_t	*stbar;
int stbar_tics;

static short stbarframe;
static short   stbar_y;

static short score, time, rings;
static short snums; // Numbers
static short timecolon; // : for TIME
static short face, livex;
static short go_game, go_over;

static short ltzz_blue_lump, chev_blue_lump, lt_blue_lump;
static short ltzz_red_lump, chev_red_lump, lt_red_lump;

#ifndef MARS
byte		*sbartop;
#endif

#ifndef MARS
//#define ST_EraseBlock EraseBlock
#endif

//static void ST_EraseBlock(int x, int y, int width, int height);

/*
====================
=
= ST_Init
=
= Locate and load all needed graphics
====================
*/

void ST_Init (void)
{
	V_FontInit();

	stbar = Z_Malloc(sizeof(*stbar)*MAXPLAYERS, PU_STATIC);

	score = W_CheckNumForName("STTSCORE");
	time = W_CheckNumForName("STTTIME");
	rings = W_CheckNumForName("STTRINGS");
	snums = W_CheckNumForName("STTNUM0");
	timecolon = W_CheckNumForName("STTCOLON");
	face = W_CheckNumForName("STFACE");
	livex = W_CheckNumForName("STLIVEX");
	go_game = W_CheckNumForName("SLIDGAME");
	go_over = W_CheckNumForName("SLIDOVER");

	ltzz_blue_lump = W_CheckNumForName("LTZZTEXT");
	chev_blue_lump = W_CheckNumForName("CHEVBLUE");
	lt_blue_lump = W_CheckNumForName("LTACTBLU");
	ltzz_red_lump = W_CheckNumForName("LTZZWARN");
	chev_red_lump = W_CheckNumForName("CHEVRED");
	lt_red_lump	= W_CheckNumForName("LTACTRED");
}

void ST_ForceDraw(void)
{
	stbarframe = 0;
	ST_Ticker();
}

/*================================================== */
/* */
/*  Init this stuff EVERY LEVEL */
/* */
/*================================================== */
void ST_InitEveryLevel(void)
{
	int		i, p;
	int		numplayers;

	stbarframe = 0;

	numplayers = 0;
	for (i = 0; i < MAXPLAYERS; i++)
		numplayers += playeringame[i] ? 1 : 0;

	D_memset(stbar, 0, sizeof(stbar[0])*MAXPLAYERS);

	for (p = 0; p < numplayers; p++)
	{
		stbar_t* sb = &stbar[p];
		sb->score = 0;
	}
	stbar_tics = 0;
}

/*
====================
=
= ST_Ticker
=
====================
*/

static void ST_Ticker_(stbar_t* sb)
{
	player_t* p;
	int			pnum = sb - &stbar[0];

#ifdef MARS
	// double-buffered renderer on MARS
	if ((stbarframe & 1) == 1)
		return;
#endif

	p = &players[pnum];

	sb->rings = p->health - 1;
	sb->score = p->score;
	sb->lives = p->lives;
	sb->exiting = p->exiting;
	sb->deadTimer = p->deadTimer;

	if (sb->intermission)
	{
		Y_Ticker();
		return;
	}

	if (sb->exiting >= 4*TICRATE && !sb->intermission)
	{
		Y_StartIntermission(p);
		sb->intermission = true;
	}
}

void ST_Ticker(void)
{
	int	p = splitscreen ? 0 : consoleplayer;
	int e = splitscreen ? MAXPLAYERS : consoleplayer + 1;

	while (p < e)
	{
		if (playeringame[p])
			ST_Ticker_(&stbar[p]);
		p++;
	}

	stbarframe++;
	stbar_tics++;
}

/*
====================
=
= ST_DrawTitleCard
=
====================
*/

static void ST_DrawTitleCard()
{
	short ltzz_lump;
	short chev_lump;
	short lt_lump;

	if (gamemapinfo.act == 3) {
		ltzz_lump = ltzz_red_lump;
		chev_lump = chev_red_lump;
		lt_lump = lt_red_lump;
	}
	else {
		ltzz_lump = ltzz_blue_lump;
		chev_lump = chev_blue_lump;
		lt_lump = lt_blue_lump;
	}

	if (gametic < 30)
		DrawFillRect(0, 22, 320, 180, COLOR_BLACK);

	if (gametic < 16) {
		// Title card moving into the frame.

		DrawScrollingBanner(ltzz_lump, (gametic-16) << 4, gametic << 1);

		DrawScrollingChevrons(chev_lump, 16 + ((gametic-16) << 4), -gametic << 1);

		if (gamemapinfo.act >= 1 && gamemapinfo.act <= 3) {
			DrawJagobjLump(lt_lump, 160+70-24 + ((16 - gametic) << 5), 100 - ((16 - gametic) << 5), NULL, NULL);
			V_DrawValueLeft(&titleNumberFont, 160+70 + ((16 - gametic) << 5), 124-4, gamemapinfo.act);
		}
		V_DrawStringRight(&titleFont, 160+68 - ((16 - gametic) << 5), 100, gamemapinfo.name);
		V_DrawStringLeft(&titleFont, 160 + ((16 - gametic) << 5), 124, "Zone");
	}
	else if (gametic < 80) {
		// Title card at rest in the frame.

		DrawScrollingBanner(ltzz_lump, 0, gametic << 1);

		DrawScrollingChevrons(chev_lump, 16, -gametic << 1);

		if (gamemapinfo.act >= 1 && gamemapinfo.act <= 3) {
			DrawJagobjLump(lt_lump, 160+68-24, 100, NULL, NULL);

			V_DrawValueLeft(&titleNumberFont, 160+68, 124-4, gamemapinfo.act);
		}
		V_DrawStringRight(&titleFont, 160+68, 100, gamemapinfo.name);
		V_DrawStringLeft(&titleFont, 160, 124, "Zone");
	}
	else {
		// Title card moving out of the frame.
		DrawScrollingBanner(ltzz_lump, (80-gametic) << 4, gametic << 1);

		DrawScrollingChevrons(chev_lump, 16 + ((80-gametic) << 4), -gametic << 1);

		if (gamemapinfo.act >= 1 && gamemapinfo.act <= 3) {
			jagobj_t *lt_obj = (jagobj_t*)W_POINTLUMPNUM(lt_lump);
			VINT lt_y = 100 + ((gametic - 80) << 5);
			VINT lt_height = lt_y + lt_obj->height > (180+22)
					? lt_obj->height - ((lt_y + lt_obj->height) - (180+22))
					: lt_obj->height;
			DrawJagobj2(lt_obj, 160+68-24 - ((gametic - 80) << 5), lt_y, 0, 0,
					lt_obj->width, lt_height, I_OverwriteBuffer());
			
			V_DrawValueLeft(&titleNumberFont, 160+68 - ((gametic - 80) << 5), 124-4, gamemapinfo.act);
		}
		V_DrawStringRight(&titleFont, 160+68 + ((gametic - 80) << 5), 100, gamemapinfo.name);
		V_DrawStringLeft(&titleFont, 160 - ((gametic - 80) << 5), 124, "Zone");
	}
}

/*
====================
=
= ST_Drawer
=
====================
*/

static void ST_Drawer_ (stbar_t* sb)
{
	if (gametic < 96) {
		ST_DrawTitleCard();
	}
	else
	{
		const int delaytime = gamemapinfo.act == 3 ? 2*TICRATE : 3*TICRATE;
		int worldTime = leveltime - delaytime + TICRATE - sb->exiting - sb->deadTimer;
		if (worldTime < 0)
			worldTime = 0;
		DrawJagobjLump(score, 16, 10+22, NULL, NULL);
		V_DrawValuePaddedRight(&hudNumberFont, 16 + 120, 10+22, sb->score, 0);

		const int minutes = worldTime/(60*TICRATE);
		const int seconds = (worldTime/(TICRATE))%60;
		DrawJagobjLump(time, 16, 26+22, NULL, NULL);
		V_DrawValuePaddedRight(&hudNumberFont, 72, 26+22, minutes, 0);
		DrawJagobjLump(timecolon, 72, 26+22, NULL, NULL);
		V_DrawValuePaddedRight(&hudNumberFont, 72+8+16, 26+22, seconds, 2);

		if (sb->rings <= 0 && (gametic / 4 & 1))
			DrawJagobjLumpWithColormap(rings, 16, 42+22, NULL, NULL, YELLOWTEXTCOLORMAP);
		else
			DrawJagobjLump(rings, 16, 42+22, NULL, NULL);
		V_DrawValuePaddedRight(&hudNumberFont, 96, 42+22, sb->rings, 0);

		DrawJagobjLump(face, 16, 176, NULL, NULL);
		V_DrawStringLeftWithColormap(&menuFont, 16 + 20, 176, "SONIC", YELLOWTEXTCOLORMAP);
		DrawJagobjLump(livex, 16 + 22, 176 + 10, NULL, NULL);
		V_DrawValuePaddedRight(&menuFont, 16 + 58, 176+8, sb->lives, 0);
	}

	if (sb->lives == 0 && sb->deadTimer > 3*TICRATE)
	{
		int gameStartX = -85;
		int overStartX = 320;
		int gameX = 160 - 85 - 8;
		int overX = 160 + 8;
		int timelength = 3*TICRATE + TICRATE/2;
		int timesize = timelength - 3*TICRATE;

		if (sb->deadTimer < timelength)
		{
			int timepassed = timesize - (timelength - sb->deadTimer);
			gameX = gameStartX + (((gameX - gameStartX)/timesize) * timepassed);
			overX = overStartX + (((overX - overStartX)/timesize) * timepassed);
		}

		DrawJagobjLump(go_game, gameX, 102, NULL, NULL);
		DrawJagobjLump(go_over, overX, 102, NULL, NULL);
	}

	if (sb->intermission)
		Y_IntermissionDrawer();
}

void CONS_Printf(char *msg, ...) 
{
	if (stbar)
	{
		va_list ap;

		va_start(ap, msg);
		D_vsnprintf(stbar->msg, sizeof(stbar->msg), msg, ap);
		va_end(ap);

		stbar->msgTics = 4 * TICRATE;
	}
} 

void ST_Drawer(void)
{
	int	p = splitscreen ? 0 : consoleplayer;
	int e = splitscreen ? MAXPLAYERS : consoleplayer + 1;
	int y[MAXPLAYERS];

	if (debugmode == DEBUGMODE_NODRAW)
		return;

	if (demoplayback)
		return;

	y[consoleplayer] = I_FrameBufferHeight();
	y[consoleplayer^1] = 0;

	while (p < e)
	{
#ifdef MARS
		stbar_y = y[p];
#else
		stbar_y = 0;
		bufferpage = sbartop;		/* draw into status bar overlay */
#endif

		if (playeringame[p])
			ST_Drawer_(&stbar[p]);
		p++;
	}

	if (stbar->msgTics)
	{
		V_DrawStringLeft(&menuFont, 0, 24, stbar->msg);
		stbar->msgTics--;
	}
}

#ifdef MARS
/*
void ST_EraseBlock(int x, int y, int width, int height)
{
	if (debugmode == DEBUGMODE_NODRAW)
		return;

	if (x & 1)
		x -= 1;
	if (width & 1)
		width -= 1;

	DrawJagobj2(sbar, x, stbar_y + y, x, y, width, height, I_FrameBuffer());
}*/

#endif
