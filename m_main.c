/* m_main.c -- main menu */

#include "doomdef.h"

#define MOVEWAIT		2
#define CURSORX		50
#define STARTY			40
#define CURSORY(y)	(STARTY*(y))
#define	NUMLCHARS 64	

typedef enum
{
	mi_newgame,
	mi_loadgame,
	mi_savegame,
#ifndef MARS
	mi_gamemode,
#endif
	mi_level,
	mi_difficulty,
	mi_savelist,
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
	VINT	y;
	char 	name[16];
	uint8_t screen;
} mainitem_t;

typedef struct
{
	VINT firstitem;
	VINT numitems;
	char name[12];
} mainscreen_t;

typedef enum
{
	ms_none,
	ms_main,
	ms_new,
	ms_load,
	ms_save,
	NUMMAINSCREENS
} screen_t;

static mainitem_t mainitem[NUMMAINITEMS];
static mainscreen_t mainscreen[NUMMAINSCREENS];

jagobj_t* m_doom;
#ifndef MARS
jagobj_t *m_single,*m_coop,*m_deathmatch,*m_gamemode;
jagobj_t* m_playmode[NUMMODES];
#endif

static VINT m_skull1lump, m_skull2lump;
static VINT m_skilllump;
static VINT numslump;

static VINT	cursorframe, cursorcount;
static VINT	movecount;
static VINT	playermap;

static playmode_t currentplaymode;
static menu_t	cursorpos;
static screen_t  screenpos;
static skill_t	playerskill;

static VINT saveslot;
static VINT savecount;
static VINT prevsaveslot;
static VINT saveslotmap;
static VINT saveslotskill;

static VINT *mapnumbers;
static VINT mapcount;

static boolean startup;

extern VINT	uchar;
extern void print(int x, int y, const char* string);

void M_Start2 (boolean startup_)
{
	int i;
#ifndef MARS
	int j;
#endif
	int y_offset;
	dmapinfo_t** maplist;
	VINT* tempmapnums;

	// copy mapnumbers to a temp buffer, then free, then allocate again
	// to avoid zone memory fragmentation
	maplist = G_LoadMaplist(&mapcount);
	if (maplist)
	{
		tempmapnums = (VINT*)I_WorkBuffer();
		for (i = 0; i < mapcount; i++)
			tempmapnums[i] = maplist[i]->mapNumber;

		for (i = 0; i < mapcount; i++)
			Z_Free(maplist[i]);
		Z_Free(maplist);

		mapnumbers = Z_Malloc(sizeof(*mapnumbers) * mapcount, PU_STATIC, 0);
		for (i = 0; i < mapcount; i++)
			mapnumbers[i] = tempmapnums[i];
	}
	else
	{
		int i, mapcount;
		char lumpname[9];

		mapcount = 0;
		tempmapnums = (VINT*)I_WorkBuffer();
		for (i = 1; i < 25; i++) {
			D_snprintf(lumpname, sizeof(lumpname), "MAP%02d", i);
			if (W_CheckNumForName(lumpname) < 0)
				continue;
			tempmapnums[mapcount++] = i;
		}

		if (mapcount > 0)
		{
			mapnumbers = Z_Malloc(sizeof(*mapnumbers) * mapcount, PU_STATIC, 0);
			for (i = 0; i < mapcount; i++)
				mapnumbers[i] = tempmapnums[i];
		}
	}

/* cache all needed graphics	 */
	startup = startup_;
	if (startup)
	{
		i = W_CheckNumForName("M_DOOM");
		m_doom = i != -1 ? W_CacheLumpNum(i, PU_STATIC) : NULL;
	}
	else
	{
		m_doom = NULL;
	}

	if (m_doom != NULL)
		y_offset = m_doom->height;
	else
		y_offset = 36;

	m_skull1lump = W_CheckNumForName("M_SKULL1");
	m_skull2lump = W_CheckNumForName("M_SKULL2");

#ifndef MARS
	m_gamemode = W_CacheLumpName ("M_GAMMOD",PU_STATIC);
#endif

#ifndef MARS
	l = W_GetNumForName ("M_SINGLE");
	for (i = 0; i < NUMMODES; i++)
		m_playmode[i] = W_CacheLumpNum (l+i, PU_STATIC);
#endif

	m_skilllump = W_CheckNumForName("SKILL0");

	numslump = W_CheckNumForName("NUM_0");

	uchar = W_CheckNumForName("CHAR_065");

	cursorcount = 0;
	cursorframe = 0;
	cursorpos = 0;
	screenpos = ms_main;
	playerskill = startskill;
	playermap = 1;

	saveslot = 0;
	savecount = SaveCount();
	prevsaveslot = -1;

	D_memset(mainscreen, 0, sizeof(mainscreen));
	D_memset(mainitem, 0, sizeof(mainitem));

#ifndef MARS
	mainscreen[ms_new].firstitem = mi_gamemode;
#else
	mainscreen[ms_new].firstitem = mi_level;
#endif
	mainscreen[ms_new].numitems = mi_difficulty - mainscreen[ms_new].firstitem + 1;

	mainscreen[ms_load].firstitem = mi_savelist;
	mainscreen[ms_load].numitems = 1;

	mainscreen[ms_main].firstitem = mi_newgame;
	mainscreen[ms_main].numitems = mi_savegame - mi_newgame + 1;
	if (startup)
	{
		// hide the 'save game' menu option
		mainscreen[ms_main].numitems--;
	}

	mainscreen[ms_save].firstitem = mi_savelist;
	mainscreen[ms_save].numitems = 1;

	D_strncpy(mainitem[mi_newgame].name, "New Game", 8);
	mainitem[mi_newgame].x = CURSORX + 24;
	mainitem[mi_newgame].y = y_offset+CURSORY(0);
	mainitem[mi_newgame].screen = ms_new;

	D_strncpy(mainitem[mi_loadgame].name, "Load Game", 9);
	mainitem[mi_loadgame].x = CURSORX + 24;
	mainitem[mi_loadgame].y = y_offset + CURSORY(1);
	mainitem[mi_loadgame].screen = ms_load;

	if (!startup)
	{
		D_strncpy(mainitem[mi_savegame].name, "Save Game", 9);
		mainitem[mi_savegame].x = CURSORX + 24;
		mainitem[mi_savegame].y = y_offset + CURSORY(2);
		mainitem[mi_savegame].screen = ms_save;
	}

#ifndef MARS
	D_strncpy(mainitem[mi_gamemode].name, "Mode", 4);
	mainitem[mi_gamemode].x = CURSORX + 24;
	mainitem[mi_gamemode].y = y_offset + CURSORY(0);
#endif

	D_strncpy(mainitem[mi_level].name, "Area", 4);
	mainitem[mi_level].x = CURSORX + 24;
	mainitem[mi_level].y = y_offset + CURSORY(mainscreen[ms_new].numitems - 2);

	D_strncpy(mainitem[mi_difficulty].name, "Difficulty", 10);
	mainitem[mi_difficulty].x = CURSORX + 24;
	mainitem[mi_difficulty].y = y_offset + CURSORY(mainscreen[ms_new].numitems - 1);

	D_strncpy(mainitem[mi_savelist].name, "Checkpoints", 11);
	mainitem[mi_savelist].x = CURSORX + 24;
	mainitem[mi_savelist].y = y_offset + CURSORY(0);

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
		Z_Free (m_doom);
#ifndef MARS
	Z_Free (m_gamemode);
#endif

#ifndef MARS
	for (i = 0; i < NUMMODES; i++)
		Z_Free(m_playmode[i]);
#endif
	
	if (mapnumbers != NULL)
		Z_Free(mapnumbers);

#ifndef MARS
	WriteEEProm ();
#endif
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
	int		buttons;
	char	newframe = 0;
	mainscreen_t* menuscr = &mainscreen[screenpos];

	if (!mapnumbers)
		return ga_startnew;

	buttons = ticrealbuttons;

/* animate skull */
	if (++cursorcount == ticrate)
	{
		cursorframe ^= 1;
		cursorcount = 0;
		newframe = 1;
	}

	if (prevsaveslot != saveslot) {
		prevsaveslot = saveslot;
		saveslotmap = -1;
		saveslotskill = -1;
		GetSaveInfo(saveslot, &saveslotmap, &saveslotskill);
	}

	if (!newframe)
		return ga_nothing;
	newframe = 0;

	/* exit menu if button press */
	if (ticon > 10 && (buttons & (BT_A | BT_LMBTN | BT_START)))
	{
		if (screenpos == ms_new)
		{
			startsave = -1;
			startmap = mapnumbers[playermap - 1]; /*set map number */
			startskill = playerskill;	/* set skill level */
			starttype = currentplaymode;	/* set play type */
			return ga_startnew;		/* done with menu */
		}

		if (screenpos == ms_load)
		{
			if (savecount > 0)
			{
				startsave = saveslot;
				return ga_startnew;
			}
		}

		if (screenpos == ms_save)
		{
			SaveGame(saveslot);
			return ga_died;
		}
	}

	if (buttons & (BT_A | BT_LMBTN))
	{
		int itemno = menuscr->firstitem + cursorpos;
		if (mainitem[itemno].screen != ms_none)
		{
			movecount = 0;
			cursorpos = 0;
			screenpos = mainitem[itemno].screen;
			clearscreen = 2;
			saveslot = screenpos == ms_save;
			return ga_nothing;
		}
	}

	if (buttons & (BT_C | BT_RMBTN))
	{
		if (screenpos != ms_main)
		{
			int i;
			mainscreen_t* mainscr = &mainscreen[ms_main];

			cursorpos = 0;
			for (i = mainscr->firstitem; i < mainscr->firstitem + mainscr->numitems; i++)
			{
				if (mainitem[i].screen == screenpos) {
					cursorpos = i - mainscr->firstitem;
					break;
				}
			}
			movecount = 0;
			screenpos = ms_main;
			clearscreen = 2;
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

/* check for movement */
	if (! (buttons & (BT_UP|BT_DOWN|BT_LEFT|BT_RIGHT) ) )
		movecount = 0;		/* move immediately on next press */
	else
	{
		int itemno = menuscr->firstitem + cursorpos;

		if (movecount == MOVEWAIT)
			movecount = 0;		/* slower everything else */

		if (++movecount == 1)
		{
			if (buttons & BT_DOWN)
			{
				cursorpos++;
				if (cursorpos == menuscr->numitems)
					cursorpos = 0;
			}
		
			if (buttons & BT_UP)
			{
				cursorpos--;
				if (cursorpos == -1)
					cursorpos = menuscr->numitems-1;
			}

			switch (itemno)
			{
#ifndef MARS
				case mi_gamemode:
					if (buttons & BT_RIGHT)
					{
						currentplaymode++;
						if (currentplaymode == NUMMODES)
							currentplaymode--;
					}
					if (buttons & BT_LEFT)
					{
						currentplaymode--;
						if (currentplaymode == -1)
							currentplaymode++;
				}
					break;
#endif
				case mi_level:
					if (buttons & BT_RIGHT)
					{			
						playermap++;
						if (mapcount == playermap || mapnumbers[playermap-1] == maxlevel+1)
							playermap--;
					}	
					if (buttons & BT_LEFT)
					{
						playermap--;
						if(playermap == 0)
							playermap++;
					}
					break;
				case mi_difficulty:
					if (buttons & BT_RIGHT)
					{
						playerskill++;
						if (playerskill > sk_nightmare)
							playerskill--;
					}
					if (buttons & BT_LEFT)
					{
						playerskill--;
						if (playerskill == -1)
							playerskill++;
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
		}
	}

	return ga_nothing;
}


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
	int mapnumber = mapnumbers[playermap - 1];
	int	leveltens = mapnumber / 10, levelones = mapnumber % 10;
	mainscreen_t* menuscr = &mainscreen[screenpos];
	mainitem_t* items = &mainitem[menuscr->firstitem];

/* Draw main menu */
	if (m_doom)
	{
		DrawJagobj(m_doom, 100, 2);
	}

/* erase old skulls */
#ifndef MARS
	EraseBlock (CURSORX, m_doom_height,m_skull1->width, CURSORY(menuscr->numitems)- CURSORY(0));
#endif

/* draw new skull */
	if (cursorframe)
		DrawJagobjLump(m_skull2lump, CURSORX, items[cursorpos].y - 2, NULL, NULL);
	else
		DrawJagobjLump(m_skull1lump, CURSORX, items[cursorpos].y - 2, NULL, NULL);

/* draw menu items */
	for (i = 0; i < menuscr->numitems; i++)
	{
		int y = items[i].y;
		print(items[i].x, y, items[i].name);
	}

	/* draw start level information */
	if (screenpos == ms_new)
	{
		mainitem_t* item;

		item = &mainitem[mi_level];
#ifndef MARS
		/* draw game mode information */
		DrawJagobj(m_gamemode, 64, m_doom_height + 2);
		EraseBlock(80, m_doom_height + 22, 320 - 80, 200 - m_doom_height + 22);
		DrawJagobj(m_playmode[currentplaymode], 80, m_doom_height + 22);
#endif

#ifndef MARS
		EraseBlock(80, m_doom_height + CURSORY(NUMMAINITEMS - 2) + 20 + 2, 320, nums[0]->height);
#endif
		if (leveltens)
		{
			DrawJagobjLump(numslump + leveltens,
				item->x + 10, item->y + 20 + 2, NULL, NULL);
			DrawJagobjLump(numslump + levelones, item->x + 24, item->y + 20 + 2, NULL, NULL);
		}
		else
			DrawJagobjLump(numslump + levelones, item->x + 10, item->y + 20 + 2, NULL, NULL);

		item = &mainitem[mi_difficulty];
#ifndef MARS
		EraseBlock(82, m_doom_height + CURSORY(NUMMAINITEMS - 1) + 20 + 2, 320 - 72, m_skill[playerskill]->height + 10);
#endif
		/* draw difficulty information */
		DrawJagobjLump(m_skilllump + playerskill, item->x + 10, item->y + 20 + 2, NULL, NULL);
	}
	else if (screenpos == ms_load || screenpos == ms_save)
	{
		mainitem_t* item;
		
		item = &mainitem[mi_savelist];

		if (savecount > 0)
		{
			if (saveslot == 0)
				print(item->x + 10, item->y + 20 + 2, "Last autosave.");
			else
			{
				print(item->x + 10, item->y + 20, "Slot ");
				DrawJagobjLump(numslump + saveslot % 10, item->x + 70, item->y + 20 + 1, NULL, NULL);
			}


			if (saveslotmap != -1)
			{
				leveltens = saveslotmap / 10, levelones = saveslotmap % 10;

				print(item->x + 10, item->y + 40 + 2, "Area");
				if (leveltens)
				{
					DrawJagobjLump(numslump + leveltens,
						item->x + 80, item->y + 40 + 3, NULL, NULL);
					DrawJagobjLump(numslump + levelones, item->x + 94, item->y + 40 + 3, NULL, NULL);
				}
				else
					DrawJagobjLump(numslump + levelones, item->x + 80, item->y + 40 + 3, NULL, NULL);
			}
			else
			{
				print(item->x + 10, item->y + 40 + 2, "Empty");
			}
			if (saveslotskill != -1)
			{
				/* draw difficulty information */
				print(item->x + 10, item->y + 60 + 2, "Difficulty");
				DrawJagobjLump(m_skilllump + saveslotskill, item->x + 10, item->y + 80 + 2, NULL, NULL);
			}
		}
		else
		{
			print(CURSORX, item->y + 30 + 2, "Reach your first");
			print(CURSORX, item->y + 50 + 2, "checkpoint after");
			print(CURSORX, item->y + 70 + 2, "the first area.");
		}
	}

#ifndef MARS
	UpdateBuffer();
#endif
}
