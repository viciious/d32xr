/* m_main.c -- main menu */

#include "doomdef.h"

#ifdef MDSKY
#include "marshw.h"
#endif

#include "v_font.h"
#include "r_local.h"

#define MOVEWAIT		(I_IsPAL() ? TICVBLS*5 : TICVBLS*6)
#define CURSORX		(96)
#define CURSORWIDTH	24
#define ITEMX		(CURSORX+CURSORWIDTH)
#define STARTY			48
#define ITEMSPACE	12
#define CURSORY(y)	(STARTY+ITEMSPACE*(y))
#define	NUMLCHARS 64	

typedef enum
{
	mi_newgame,
	mi_loadgame,
	mi_savegame,
	mi_joingame,
	mi_level,
	mi_gamemode,
	mi_savelist,
	mi_singleplayer,
	mi_splitscreen,
	mi_network,
	mi_help,
	NUMMAINITEMS
} menu_t;

typedef enum
{
	single,
	coop,
	dmatch,
	NUMMODES
} playmode_t;

typedef struct
{
	VINT	x;
	uint8_t	y;
	char	screen;
	const char 	*name;
} mainitem_t;

typedef struct
{
	VINT firstitem;
	VINT numitems;
} mainscreen_t;

typedef enum
{
	ms_none = -1,
	ms_main,
	ms_gametype,
	ms_new,
	ms_load,
	ms_save,
	ms_help,
	NUMMAINSCREENS
} screen_t;

static mainitem_t mainitem[NUMMAINITEMS];
static mainscreen_t mainscreen[NUMMAINSCREENS];

//static const char* playmodes[NUMMODES] = { "Single", "Coop", "Deathmatch" };
jagobj_t* m_doom;

static VINT m_skull1lump;
static VINT numslump;
#define NUMHANDFRAMES 5
#define NUMKFISTFRAMES 7
#define NUMSBLINKFRAMES 3
#define NUMTBLINKFRAMES 3
#define NUMKBLINKFRAMES 3
#define NUMTAILWAGFRAMES 6
jagobj_t *m_hand[NUMHANDFRAMES];
jagobj_t *m_kfist[NUMKFISTFRAMES];
jagobj_t *m_sblink[NUMSBLINKFRAMES];
jagobj_t *m_tblink[NUMTBLINKFRAMES];
jagobj_t *m_kblink[NUMKBLINKFRAMES];
jagobj_t *m_tailwag[NUMTAILWAGFRAMES];
static char fistCounter = 5;
static char sBlinkCounter = 110;
static char tBlinkCounter = 25;
static char kBlinkCounter = 76;

static VINT	cursorframe;
static VINT cursordelay;
static VINT	movecount;
static VINT	playermap = 1;

static VINT currentplaymode = gt_single;
static VINT currentgametype = mi_singleplayer;
static VINT	cursorpos;
static VINT screenpos;

static VINT saveslot;
static VINT savecount;
static VINT prevsaveslot;
static VINT saveslotmap;
static VINT saveslotskill;
static VINT saveslotmode;

static char displaymapname[32];
static VINT displaymapnum;

static boolean startup;
VINT	uchar;

void M_Start2 (boolean startup_)
{
	int i;

/* cache all needed graphics	 */
	m_skull1lump = W_CheckNumForName("M_CURSOR");

	startup = startup_;
	m_doom = NULL;
	if (startup)
	{
		i = W_CheckNumForName("M_TITLE");
		m_doom = i != -1 ? W_CacheLumpNum(i, PU_STATIC) : NULL;

		for (i = 0; i < NUMHANDFRAMES; i++)
		{
			char entry[9];
			D_snprintf(entry, 8, "M_HAND%d", i + 1);
			m_hand[i] = W_CacheLumpName(entry, PU_STATIC);
		}

		for (int i = 0; i < NUMKFISTFRAMES; i++)
		{
			char entry[9];
			D_snprintf(entry, sizeof(entry), "KFIST%d", i + 1);
			m_kfist[i] = W_CacheLumpName(entry, PU_STATIC);
		}

		for (int i = 0; i < NUMSBLINKFRAMES; i++)
		{
			char entry[9];
			D_snprintf(entry, sizeof(entry), "SBLINK%d", i + 1);
			m_sblink[i] = W_CacheLumpName(entry, PU_STATIC);
		}

		for (int i = 0; i < NUMKBLINKFRAMES-1; i++)
		{
			char entry[9];
			D_snprintf(entry, sizeof(entry), "KBLINK%d", i + 1);
			m_kblink[i] = W_CacheLumpName(entry, PU_STATIC);
		}
		m_kblink[2] = W_CacheLumpName("KBLINK1", PU_STATIC);

		for (int i = 0; i < NUMTBLINKFRAMES-1; i++)
		{
			char entry[9];
			D_snprintf(entry, sizeof(entry), "TBLINK%d", i + 1);
			m_tblink[i] = W_CacheLumpName(entry, PU_STATIC);
		}
		m_tblink[2] = W_CacheLumpName("TBLINK1", PU_STATIC);

		for (int i = 0; i < NUMTAILWAGFRAMES; i++)
		{
			char entry[9];
			D_snprintf(entry, sizeof(entry), "TAILWAG%d", i + 1);
			m_tailwag[i] = W_CacheLumpName(entry, PU_STATIC);
		}
	}

	numslump = W_CheckNumForName("STTNUM0");

	uchar = W_CheckNumForName("STCFN065");

	cursorframe = -1;
	cursorpos = 0;
	cursordelay = MOVEWAIT;

	/* HACK: dunno why but the pistol sound comes out muffled if played from M_Start */
	/* so defer the playback until M_Ticker is called */
	screenpos = startup ? ms_main : ms_none;
	if (startup)
		playermap = 1;

	saveslot = 0;
	savecount = SaveCount();
	prevsaveslot = -1;

	displaymapnum = -1;

	D_memset(mainscreen, 0, sizeof(mainscreen));
	D_memset(mainitem, 0, sizeof(mainitem));

	mainscreen[ms_new].firstitem = mi_level;
	mainscreen[ms_new].numitems = mi_level - mainscreen[ms_new].firstitem + 1;

	mainscreen[ms_load].firstitem = mi_savelist;
	mainscreen[ms_load].numitems = 1;

	mainscreen[ms_main].firstitem = mi_newgame;
	mainscreen[ms_main].numitems = 1;

	mainscreen[ms_save].firstitem = mi_savelist;
	mainscreen[ms_save].numitems = 1;

	mainscreen[ms_gametype].firstitem = mi_singleplayer;
	mainscreen[ms_gametype].numitems = 1;

	mainscreen[ms_help].firstitem = mi_help;
	mainscreen[ms_help].numitems = 1;

	mainitem[mi_newgame].name = "START GAME";
	mainitem[mi_newgame].x = ITEMX;
	mainitem[mi_newgame].y = CURSORY(0);
	mainitem[mi_newgame].screen = ms_gametype;

	mainitem[mi_loadgame].name = "ABOUT";
	mainitem[mi_loadgame].x = ITEMX;
	mainitem[mi_loadgame].y = CURSORY(1);
	mainitem[mi_loadgame].screen = ms_help;
	mainscreen[ms_main].numitems++;

	mainitem[mi_level].name = "Select Act";
	mainitem[mi_level].x = ITEMX;
	mainitem[mi_level].y = CURSORY(0);
	mainitem[mi_level].screen = ms_none;

	mainitem[mi_gamemode].name = "Game Mode";
	mainitem[mi_gamemode].x = ITEMX;
	mainitem[mi_gamemode].y = CURSORY((mainscreen[ms_new].numitems - 2) * 2);
	mainitem[mi_gamemode].screen = ms_none;

	mainitem[mi_savelist].name = "Checkpoints";
	mainitem[mi_savelist].x = ITEMX;
	mainitem[mi_savelist].y = CURSORY(0);
	mainitem[mi_savelist].screen = ms_none;

	mainitem[mi_singleplayer].name = "SINGLE PLAYER";
	mainitem[mi_singleplayer].x = ITEMX;
	mainitem[mi_singleplayer].y = CURSORY(0);
	mainitem[mi_singleplayer].screen = ms_new;

	mainitem[mi_splitscreen].name = "Split-Screen";
	mainitem[mi_splitscreen].x = ITEMX;
	mainitem[mi_splitscreen].y = CURSORY(1);
	mainitem[mi_splitscreen].screen = ms_new;

	mainitem[mi_network].name = "Multiplayer";
	mainitem[mi_network].x = ITEMX;
	mainitem[mi_network].y = CURSORY(2);
	mainitem[mi_network].screen = ms_new;

#ifndef MARS
	DoubleBufferSetup();
#endif
}

void M_Start(void)
{
	M_Start2(true);
}

void M_Stop (void)
{	
#ifndef MARS
	int i;
#endif

/* they stay cached by status bar */

/* free all loaded graphics */

	if (m_doom != NULL)
	{
		Z_Free (m_doom);
		m_doom = NULL;
	}

	if (startup)
	{
		int i;
		for (i = 0; i < NUMHANDFRAMES; i++)
		{
			if (m_hand[i])
				Z_Free(m_hand[i]);
			m_hand[i] = NULL;
		}

		for (int i = 0; i < NUMKFISTFRAMES; i++)
		{
			if (m_kfist[i])
				Z_Free(m_kfist[i]);
			m_kfist[i] = NULL;
		}

		for (int i = 0; i < NUMSBLINKFRAMES; i++)
		{
			if (m_sblink[i])
				Z_Free(m_sblink[i]);
			m_sblink[i] = NULL;
		}

		for (int i = 0; i < NUMKBLINKFRAMES; i++)
		{
			if (m_kblink[i])
				Z_Free(m_kblink[i]);
			m_kblink[i] = NULL;
		}

		for (int i = 0; i < NUMTBLINKFRAMES; i++)
		{
			if (m_tblink[i])
				Z_Free(m_tblink[i]);
			m_tblink[i] = NULL;
		}

		for (int i = 0; i < NUMTAILWAGFRAMES; i++)
		{
			if (m_tailwag[i])
				Z_Free(m_tailwag[i]);
			m_tailwag[i] = NULL;
		}
	}

#ifndef MARS
	WriteEEProm ();
#endif
}

/*
=================
=
= M_MapName
=
=================
*/

static char* M_MapName(VINT mapnum)
{
	dmapinfo_t mi;
	char buf[512];

	if (displaymapnum == mapnum)
		return displaymapname;

	G_FindMapinfo(G_LumpNumForMapNum(mapnum), &mi, buf);
	D_snprintf(displaymapname, sizeof(displaymapname), "%s", mi.name);
	displaymapname[sizeof(displaymapname) - 1] = '\0';

	displaymapnum = mapnum;
	return displaymapname;
}

static void M_UpdateSaveInfo(void)
{
	if (prevsaveslot != saveslot) {
		prevsaveslot = saveslot;
		saveslotmap = -1;
		saveslotskill = -1;
		saveslotmode = gt_single;
		GetSaveInfo(saveslot, &saveslotmap, &saveslotskill, &saveslotmode);
	}
}

/*
=================
=
= M_Ticker
=
=================
*/

int M_Ticker (void)
{
	int		buttons, oldbuttons;
	mainscreen_t* menuscr;
	boolean newcursor = false;
	int sound = sfx_None;

	if (cursorframe == -1)
	{
		cursorframe = 0;
		cursordelay = MOVEWAIT;
	}

	if (screenpos == ms_none)
	{
		screenpos = ms_main;
		S_StartSound(NULL, sfx_None);
	}

	menuscr = &mainscreen[screenpos];

	if (!gamemapnumbers)
		return ga_startnew;

/* animate skull */
	if (gametic & 1)
	{
		cursorframe++;
	}

	M_UpdateSaveInfo();

	buttons = ticrealbuttons & MENU_BTNMASK;
	oldbuttons = oldticrealbuttons & MENU_BTNMASK;

	if ((gamemapinfo.mapNumber == 30 && (buttons & (BT_B | BT_LMBTN | BT_START)) && !(oldbuttons & (BT_B | BT_LMBTN | BT_START)))
		|| (gamemapinfo.mapNumber != 30 && (buttons & (BT_B | BT_LMBTN)) && !(oldbuttons & (BT_B | BT_LMBTN))))
	{
		int itemno = menuscr->firstitem + cursorpos;

		int nextscreen = mainitem[itemno].screen;
		if (nextscreen == ms_save && (startup || netgame == gt_deathmatch))
			nextscreen = ms_none;

		if (nextscreen != ms_none)
		{
			movecount = 0;
			cursorpos = 0;
			switch (itemno) {
				case mi_splitscreen:
				case mi_network:
					if (currentplaymode == single)
						currentplaymode++;
					currentgametype = itemno;
					break;
				case mi_singleplayer:
				default:
					currentplaymode = single;
					currentgametype = itemno;
					break;
			}
			screenpos = nextscreen;
			clearscreen = 2;
			prevsaveslot = -1;
			saveslot = screenpos == ms_save;
			S_StartSound(NULL, sfx_None);
			return ga_nothing;
		}
	}

	if ((buttons & (BT_A | BT_C | BT_RMBTN)) && !(oldbuttons & (BT_A | BT_C | BT_RMBTN)))
	{
		if (screenpos != ms_main)
		{
			int i, j;
			int curscreenpos = screenpos;

			cursorpos = 0;
			screenpos = ms_main;

			for (i =0; i < NUMMAINITEMS; i++)
			{
				if (mainitem[i].screen == curscreenpos) {
					for (j = 0; j < NUMMAINSCREENS; j++) {
						if (i >= mainscreen[j].firstitem && i < mainscreen[j].firstitem+mainscreen[j].numitems) {
							screenpos = j;
							cursorpos = i - mainscreen[j].firstitem;
							break;
						}
					}
					break;				
				}
			}

			movecount = 0;
			clearscreen = 2;
			S_StartSound(NULL, sfx_None);
			return 0;
		}
		else
		{
			if (!startup)
			{
				return ga_completed;
			}
		}
	}

	/* exit menu if button press */
	if ((buttons & (BT_B | BT_LMBTN | BT_START)) && !(oldbuttons & (BT_B | BT_LMBTN | BT_START)))
	{
		if (screenpos == ms_new)
		{
			consoleplayer = 0;
			startsave = -1;
			startmap = gamemapnumbers[playermap - 1]; /*set map number */
			starttype = currentplaymode;	/* set play type */
			startsplitscreen = currentgametype == mi_splitscreen;
			return ga_startnew;		/* done with menu */
		}

		if (screenpos == ms_main)
		{
			int itemno = menuscr->firstitem + cursorpos;
			if (itemno == mi_joingame)
			{
				consoleplayer = 1;
				I_NetSetup();
				if (starttype != gt_single)
					return ga_startnew;
				clearscreen = 2;
				return ga_nothing;
			}
		}

		if (screenpos == ms_load)
		{
			if (savecount > 0)
			{
				startsplitscreen = saveslotmode != gt_single;
				startsave = saveslot;
				return ga_startnew;
			}
		}

		if (screenpos == ms_save)
		{
			S_StartSound(NULL, sfx_None);
			SaveGame(saveslot);
			prevsaveslot = -1;
			return ga_died;
		}
	}

	if (buttons == 0)
	{
		cursordelay = 0;
		return ga_nothing;
	}

	cursordelay -= vblsinframe;
	if (cursordelay > 0)
		return ga_nothing;

	cursordelay = MOVEWAIT;

/* check for movement */
	if (! (buttons & (BT_UP|BT_DOWN|BT_LEFT|BT_RIGHT) ) )
		movecount = 0;		/* move immediately on next press */
	else
	{
		int itemno = menuscr->firstitem + cursorpos;

		movecount = 0;		/* repeat move */

		if (++movecount == 1)
		{
			int oldcursorpos = cursorpos;

			if (buttons & BT_DOWN)
			{
				//S_StartSound(NULL, sfx_s3k_5b);
				if (++cursorpos == menuscr->numitems)
					cursorpos = 0;
			}
		
			if (buttons & BT_UP)
			{
				//S_StartSound(NULL, sfx_s3k_5b);
				if (--cursorpos == -1)
					cursorpos = menuscr->numitems-1;
			}

			switch (itemno)
			{
				case mi_gamemode:
					if (currentgametype == mi_singleplayer)
					{
						currentplaymode = single;
					}
					else
					{
						if (buttons & BT_RIGHT)
						{
							if (++currentplaymode == NUMMODES)
								currentplaymode--;
						}
						if (buttons & BT_LEFT)
						{
							if (--currentplaymode <= single)
								currentplaymode++;
						}
					}
					break;
				case mi_level:
					if (buttons & BT_RIGHT)
					{			
						if (++playermap == gamemapcount + 1)
							playermap = 1;

#ifdef SHOW_DISCLAIMER
						while (gamemapnumbers[playermap-1] == 30 || (gamemapnumbers[playermap-1] >= SSTAGE_START && gamemapnumbers[playermap-1] <= SSTAGE_END))
						{
							if (++playermap == gamemapcount + 1)
								playermap = 1;
						}
#endif
					}
					if (buttons & BT_LEFT)
					{
						if(--playermap == 0)
							playermap = gamemapcount;
#ifdef SHOW_DISCLAIMER
						while (gamemapnumbers[playermap-1] == 30 || (gamemapnumbers[playermap-1] >= SSTAGE_START && gamemapnumbers[playermap-1] <= SSTAGE_END))
						{
							if(--playermap == 0)
								playermap = gamemapcount;
						}
#endif
					}
					break;
				case mi_savelist:
					if (buttons & BT_RIGHT)
					{
						saveslot++;
						if (saveslot >= savecount + (screenpos == ms_save) || saveslot >= MaxSaveCount())
							saveslot--;
					}
					if (buttons & BT_LEFT)
					{
						saveslot--;
						if (saveslot == -1 + (screenpos == ms_save))
							saveslot++;
					}
					break;
				default:
					break;
			}

			newcursor = cursorpos != oldcursorpos;

			if (newcursor)
				sound = sfx_None;
		}
	}

	if (sound != sfx_None)
		S_StartSound(NULL, sound);

	if (newcursor || sound != sfx_None)
	{
		/* long menu item names can spill onto the screen border */
		clearscreen = 2;
	}

	return ga_nothing;
}

void O_DrawHelp (VINT yPos);
/*
=================
=
= M_Drawer
=
=================
*/
void M_Drawer (void)
{
	int i;
	int mapnumber = gamemapnumbers[playermap - 1];
	int	leveltens = mapnumber / 10, levelones = mapnumber % 10;
	int scrpos = screenpos == ms_none ? ms_main : screenpos;
	mainscreen_t* menuscr = &mainscreen[scrpos];
	mainitem_t* items = &mainitem[menuscr->firstitem];
	int y, y_offset = 0;

	if (titlescreen && gamemapinfo.mapNumber == TITLE_MAP_NUMBER) {
		// Fill the area above the viewport with the sky color.
		DrawFillRect(0, 0, 320, 44, gamemapinfo.skyTopColor);
	}

/* Draw main menu */
	if (m_doom && (scrpos == ms_main || scrpos == ms_gametype))
	{
		VINT logoPos = 160 - (m_doom->width / 2);
		DrawJagobj(m_doom, logoPos, 16);
		y_offset = m_doom->height + 16 - STARTY;

		DrawJagobj(m_hand[cursorframe % NUMHANDFRAMES], 160 + 3, 16 + 32);

		DrawJagobj(m_tailwag[cursorframe % NUMTAILWAGFRAMES], logoPos + 5, 16 + 2);

		if (gametic & 1)
		{
			fistCounter--;

			if (fistCounter <= -NUMKFISTFRAMES)
				fistCounter = 15 + (M_Random() & 7);
		}

		if (fistCounter < 0)
			DrawJagobj(m_kfist[D_abs(fistCounter)], logoPos + 188, 16 + 43);
		else
			DrawJagobj(m_kfist[0], logoPos + 188, 16 + 43);

		sBlinkCounter--;
		if (sBlinkCounter <= -NUMSBLINKFRAMES)
			sBlinkCounter = M_Random() & 127;
		tBlinkCounter--;
		if (tBlinkCounter <= -NUMTBLINKFRAMES)
			tBlinkCounter = M_Random() & 127;
		kBlinkCounter--;
		if (kBlinkCounter <= -NUMKBLINKFRAMES)
			kBlinkCounter = M_Random() & 127;

		if (sBlinkCounter < 0)
			DrawJagobj(m_sblink[D_abs(sBlinkCounter)], logoPos + 93, 16 + 27);

		if (tBlinkCounter < 0)
			DrawJagobj(m_tblink[D_abs(tBlinkCounter)], logoPos + 54, 16 + 40);

		if (kBlinkCounter < 0)
			DrawJagobj(m_kblink[D_abs(kBlinkCounter)], logoPos + 158, 16 + 37);
	}

/* erase old skulls */
#ifndef MARS
	EraseBlock (CURSORX, m_doom_height,m_skull1->width, CURSORY(menuscr->numitems)- CURSORY(0));
#endif

	if (scrpos == ms_help)
	{
		O_DrawHelp(80);
		return;
	}

/* draw menu items */
	int selectedPos = 0;
	for (i = 0; i < menuscr->numitems; i++)
	{
		int y = y_offset + items[i].y;

		if (scrpos == ms_main)
		{
			if (i == cursorpos)
				selectedPos = V_DrawStringCenterWithColormap(&menuFont, 160, y, items[i].name, YELLOWTEXTCOLORMAP);
			else
				V_DrawStringCenter(&menuFont, 160, y, items[i].name);
		}
		else
		{
			if (i == cursorpos)
				selectedPos = V_DrawStringLeftWithColormap(&menuFont, items[i].x, y, items[i].name, YELLOWTEXTCOLORMAP);
			else
				V_DrawStringLeft(&menuFont, items[i].x, y, items[i].name);
		}
	}

	/* draw new skull */
	DrawJagobjLump(m_skull1lump, scrpos == ms_main ? 160 - (selectedPos - 160) - 20 : CURSORX, y_offset+items[cursorpos].y, NULL, NULL);

	if (scrpos == ms_new)
	{
		char mapNum[8];
		mainitem_t* item;
		char *tmp;
		int tmplen;

		/* draw game mode information */
		item = &mainitem[mi_gamemode];
		y = y_offset + item->y;
//		V_DrawStringLeft(&menuFont, item->x + 10, y + ITEMSPACE + 2, playmodes[currentplaymode]);

		/* draw start level information */
		item = &mainitem[mi_level];
		y = y_offset + item->y;

		tmp = M_MapName(mapnumber);
		tmplen = mystrlen(tmp);

#ifndef MARS
		EraseBlock(80, m_doom_height + CURSORY(NUMMAINITEMS - 2) + ITEMSPACE + 2, 320, nums[0]->height);
#endif
		D_snprintf(mapNum, sizeof(mapNum), "%d", mapnumber);

		V_DrawStringLeft(&titleNumberFont, item->x + 96, y + 2, mapNum);

		V_DrawStringLeft(&menuFont, (320 - (tmplen * 14)) >> 1, y + ITEMSPACE + 2, tmp);

		O_DrawHelp(120);
	}
	else if (scrpos == ms_load || scrpos == ms_save)
	{
		mainitem_t* item;
		
		item = &mainitem[mi_savelist];
		y = y_offset + item->y;

		M_UpdateSaveInfo();

		if (savecount > 0)
		{
			if (saveslot == 0)
				V_DrawStringLeft(&menuFont, item->x + 10, y + 20 + 2, "Last autosave.");
			else
			{
				V_DrawStringLeft(&menuFont, item->x + 10, y + 20, "Slot ");
				DrawJagobjLump(numslump + saveslot % 10, item->x + 70, y + ITEMSPACE + 1, NULL, NULL);
			}

			if (saveslotmap != -1)
			{
				char *mapname;
				int mapnamelen;

				mapname = M_MapName(saveslotmap);
				mapnamelen = mystrlen(mapname);

				leveltens = saveslotmap / 10, levelones = saveslotmap % 10;

				V_DrawStringLeft(&menuFont, item->x + 10, y + 40 + 2, "Area");

				if (leveltens)
				{
					DrawJagobjLump(numslump + leveltens,
						item->x + 80, y + 40 + 3, NULL, NULL);
					DrawJagobjLump(numslump + levelones, item->x + 94, y + ITEMSPACE*2 + 3, NULL, NULL);
				}
				else
					DrawJagobjLump(numslump + levelones, item->x + 80, y + ITEMSPACE*2 + 3, NULL, NULL);

				V_DrawStringLeft(&menuFont, (320 - (mapnamelen * 14)) >> 1, y + ITEMSPACE*3 + 3, mapname);
			}
			else
			{
				V_DrawStringLeft(&menuFont, item->x + 10, y + ITEMSPACE*2 + 2, "Empty");
			}
		}
		else
		{
			V_DrawStringLeft(&menuFont, CURSORX, y + ITEMSPACE+10 + 2, "Reach your first");
			V_DrawStringLeft(&menuFont, CURSORX, y + ITEMSPACE*2+10 + 2, "checkpoint after");
			V_DrawStringLeft(&menuFont, CURSORX, y + ITEMSPACE*3+10 + 2, "the first area.");
		}
	}
}
