/* m_main.c -- main menu */

#include "doomdef.h"

#define MOVEWAIT		TICVBLS*6
#define CURSORX		(80)
#define CURSORWIDTH	24
#define ITEMX		(CURSORX+CURSORWIDTH)
#define STARTY			48
#define ITEMSPACE	20
#define CURSORY(y)	(STARTY+ITEMSPACE*(y))
#define	NUMLCHARS 64	

typedef enum
{
	mi_newgame,
	mi_loadgame,
	mi_savegame,
	mi_level,
	mi_gamemode,
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
	VINT	screen;
	char 	name[16];
} mainitem_t;

typedef struct
{
	VINT firstitem;
	VINT numitems;
	char name[12];
} mainscreen_t;

typedef enum
{
	ms_none = -1,
	ms_main,
	ms_new,
	ms_load,
	ms_save,
	NUMMAINSCREENS
} screen_t;

static mainitem_t mainitem[NUMMAINITEMS];
static mainscreen_t mainscreen[NUMMAINSCREENS];

static const char* playmodes[NUMMODES] = { "Single", "Coop", "Deathmatch" };
jagobj_t* m_doom;

static VINT m_skull1lump, m_skull2lump;
static VINT m_skilllump;
static VINT numslump;

static VINT	cursorframe;
static VINT cursordelay;
static VINT	movecount;
static VINT	playermap = 1;

static VINT currentplaymode = gt_single;
static VINT	cursorpos;
static VINT screenpos;
static VINT playerskill;

static VINT saveslot;
static VINT savecount;
static VINT prevsaveslot;
static VINT saveslotmap;
static VINT saveslotskill;
static VINT saveslotmode;

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

	m_skull1lump = W_CheckNumForName("M_SKULL1");
	m_skull2lump = W_CheckNumForName("M_SKULL2");

	m_skilllump = W_CheckNumForName("SKILL0");

	numslump = W_CheckNumForName("NUM_0");

	uchar = W_CheckNumForName("CHAR_065");

	cursorframe = -1;
	cursorpos = 0;
	cursordelay = MOVEWAIT;

	/* HACK: dunno why but the pistol sound comes out muffled if played from M_Start */
	/* so defer the playback until M_Ticker is called */
	screenpos = startup ? ms_main : ms_none;
	playerskill = startskill;
	if (startup)
		playermap = 1;

	saveslot = 0;
	savecount = SaveCount();
	prevsaveslot = -1;

	D_memset(mainscreen, 0, sizeof(mainscreen));
	D_memset(mainitem, 0, sizeof(mainitem));

	mainscreen[ms_new].firstitem = mi_level;
	mainscreen[ms_new].numitems = mi_difficulty - mainscreen[ms_new].firstitem + 1;

	mainscreen[ms_load].firstitem = mi_savelist;
	mainscreen[ms_load].numitems = 1;

	mainscreen[ms_main].firstitem = mi_newgame;
	mainscreen[ms_main].numitems = 1;

	mainscreen[ms_save].firstitem = mi_savelist;
	mainscreen[ms_save].numitems = 1;

	D_memcpy(mainitem[mi_newgame].name, "New Game", 9);
	mainitem[mi_newgame].x = ITEMX;
	mainitem[mi_newgame].y = CURSORY(0);
	mainitem[mi_newgame].screen = ms_new;

	D_memcpy(mainitem[mi_loadgame].name, "Load Game", 10);
	mainitem[mi_loadgame].x = ITEMX;
	mainitem[mi_loadgame].y = CURSORY(1);
	mainitem[mi_loadgame].screen = ms_load;
	mainscreen[ms_main].numitems++;

	if (!startup && netgame != gt_deathmatch)
	{
		D_memcpy(mainitem[mi_savegame].name, "Save Game", 10);
		mainitem[mi_savegame].x = ITEMX;
		mainitem[mi_savegame].y = CURSORY(2);
		mainitem[mi_savegame].screen = ms_save;
		mainscreen[ms_main].numitems++;
	}

	D_memcpy(mainitem[mi_level].name, "Area", 5);
	mainitem[mi_level].x = ITEMX;
	mainitem[mi_level].y = CURSORY(0);
	mainitem[mi_level].screen = ms_none;

	D_memcpy(mainitem[mi_gamemode].name, "Game Mode", 10);
	mainitem[mi_gamemode].x = ITEMX;
	mainitem[mi_gamemode].y = CURSORY((mainscreen[ms_new].numitems - 2) * 2);
	mainitem[mi_gamemode].screen = ms_none;

	D_memcpy(mainitem[mi_difficulty].name, "Difficulty", 11);
	mainitem[mi_difficulty].x = ITEMX;
	mainitem[mi_difficulty].y = CURSORY((mainscreen[ms_new].numitems - 1)*2);
	mainitem[mi_difficulty].screen = ms_none;

	D_memcpy(mainitem[mi_savelist].name, "Checkpoints", 12);
	mainitem[mi_savelist].x = ITEMX;
	mainitem[mi_savelist].y = CURSORY(0);
	mainitem[mi_savelist].screen = ms_none;

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
	int		buttons, oldbuttons;
	mainscreen_t* menuscr;
	int		oldplayermap;
	boolean newcursor = false;
	int sound = sfx_None;

	if (cursorframe == -1)
	{
		cursorframe = 0;
		cursordelay = MOVEWAIT+MOVEWAIT/2;
	}

	if (screenpos == ms_none)
	{
		screenpos = ms_main;
		S_StartSound(NULL, sfx_pistol);
	}

	menuscr = &mainscreen[screenpos];

	if (!mapnumbers)
		return ga_startnew;

/* animate skull */
	if (gametic != prevgametic && (gametic&3) == 0)
	{
		cursorframe ^= 1;
	}

	if (prevsaveslot != saveslot) {
		prevsaveslot = saveslot;
		saveslotmap = -1;
		saveslotskill = -1;
		saveslotmode = gt_single;
		GetSaveInfo(saveslot, &saveslotmap, &saveslotskill, &saveslotmode);
	}

	buttons = ticrealbuttons;
	oldbuttons = oldticrealbuttons;

	if ((buttons & (BT_A | BT_LMBTN)) && !(oldbuttons & (BT_A | BT_LMBTN)))
	{
		int itemno = menuscr->firstitem + cursorpos;
		if (mainitem[itemno].screen != ms_none)
		{
			movecount = 0;
			cursorpos = 0;
			screenpos = mainitem[itemno].screen;
			clearscreen = 2;
			saveslot = screenpos == ms_save;
			S_StartSound(NULL, sfx_pistol);
			return ga_nothing;
		}
	}

	if ((buttons & (BT_B | BT_RMBTN)) && !(oldbuttons & (BT_B | BT_RMBTN)))
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
			S_StartSound(NULL, sfx_swtchn);
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
	if ((buttons & (BT_A | BT_LMBTN | BT_START)) && !(oldbuttons & (BT_A | BT_LMBTN | BT_START)))
	{
		if (screenpos == ms_new)
		{
			startsave = -1;
			startmap = mapnumbers[playermap - 1]; /*set map number */
			startskill = playerskill;	/* set skill level */
			starttype = currentplaymode;	/* set play type */
			//splitscreen = (starttype != gt_single);
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

	if (buttons == 0)
	{
		cursordelay = 0;
		return ga_nothing;
	}

	cursordelay -= vblsinframe[0];
	if (cursordelay > 0)
		return ga_nothing;

	cursordelay = MOVEWAIT;

/* check for movement */
	oldplayermap = playermap;
	if (! (buttons & (BT_UP|BT_DOWN|BT_LEFT|BT_RIGHT) ) )
		movecount = 0;		/* move immediately on next press */
	else
	{
		int itemno = menuscr->firstitem + cursorpos;

		movecount = 0;		/* repeat move */

		if (++movecount == 1)
		{
			int oldplayerskill = playerskill;
			int oldsaveslot = saveslot;
			int oldcursorpos = cursorpos;
			int oldplayermode = currentplaymode;

			if (buttons & BT_DOWN)
			{
				if (++cursorpos == menuscr->numitems)
					cursorpos = 0;
			}
		
			if (buttons & BT_UP)
			{
				if (--cursorpos == -1)
					cursorpos = menuscr->numitems-1;
			}

			switch (itemno)
			{
				case mi_gamemode:
					if (buttons & BT_RIGHT)
					{
						if (++currentplaymode == NUMMODES)
							currentplaymode--;
					}
					if (buttons & BT_LEFT)
					{
						if (--currentplaymode == -1)
							currentplaymode++;
					}
					break;
				case mi_level:
					if (buttons & BT_RIGHT)
					{			
						if (++playermap == mapcount + 1)
							playermap = 1;
					}
					if (buttons & BT_LEFT)
					{
						if(--playermap == 0)
							playermap = mapcount;
					}
					break;
				case mi_difficulty:
					if (buttons & BT_RIGHT)
					{
						if (++playerskill > sk_nightmare)
							playerskill--;
					}
					if (buttons & BT_LEFT)
					{
						if (--playerskill == -1)
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

			newcursor = cursorpos != oldcursorpos;

			if (newcursor)
				sound = sfx_pistol;
			else if (oldplayerskill != playerskill ||
				oldsaveslot != saveslot ||
				oldplayermap != playermap ||
				oldplayermode != currentplaymode)
				sound = sfx_stnmov;
		}
	}

	if (sound != sfx_None)
		S_StartSound(NULL, sound);

	if (newcursor || sound != sfx_None)
	{
		/* long menu item names can spill onto the screen border */
		clearscreen = 2;
		oldplayermap = playermap;
	}

	return ga_nothing;
}


/*
=================
=
= M_MapName
=
=================
*/

static char* M_MapName(VINT mapnum, char *mapname, size_t mapnamesize)
{
	dmapinfo_t mi;
	char buf[512];

	G_FindMapinfo(G_LumpNumForMapNum(mapnum), &mi, buf);
	D_snprintf(mapname, mapnamesize, "%s", mi.name);
	mapname[mapnamesize - 1] = '\0';

	return mapname;
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
	int scrpos = screenpos == ms_none ? ms_main : screenpos;
	mainscreen_t* menuscr = &mainscreen[scrpos];
	mainitem_t* items = &mainitem[menuscr->firstitem];
	int y, y_offset = 0;

/* Draw main menu */
	if (m_doom && scrpos == ms_main)
	{
		DrawJagobj(m_doom, 100, 4);
		y_offset = m_doom->height + 4 - STARTY;
	}

	if (screenpos == ms_new)
	{
		if (currentplaymode == (int)gt_single)
			D_memcpy(mainitem[mi_gamemode].name, "Game Mode", 10);
		else
			D_memcpy(mainitem[mi_gamemode].name, "Split Mode", 11);
	}

/* erase old skulls */
#ifndef MARS
	EraseBlock (CURSORX, m_doom_height,m_skull1->width, CURSORY(menuscr->numitems)- CURSORY(0));
#endif

/* draw new skull */
	if (cursorframe)
		DrawJagobjLump(m_skull2lump, CURSORX, y_offset+items[cursorpos].y - 2, NULL, NULL);
	else
		DrawJagobjLump(m_skull1lump, CURSORX, y_offset+items[cursorpos].y - 2, NULL, NULL);

/* draw menu items */
	for (i = 0; i < menuscr->numitems; i++)
	{
		int y = y_offset + items[i].y;
		print(items[i].x, y, items[i].name);
	}

	if (scrpos == ms_new)
	{
		mainitem_t* item;
		char tmp[64];
		int tmplen;

		/* draw game mode information */
		item = &mainitem[mi_gamemode];
		y = y_offset + item->y;
		print(item->x + 10, y + ITEMSPACE + 2, playmodes[currentplaymode]);

		/* draw start level information */
		item = &mainitem[mi_level];
		y = y_offset + item->y;

		M_MapName(mapnumber, tmp, sizeof(tmp));
		tmplen = mystrlen(tmp);

#ifndef MARS
		EraseBlock(80, m_doom_height + CURSORY(NUMMAINITEMS - 2) + ITEMSPACE + 2, 320, nums[0]->height);
#endif
		if (leveltens)
		{
			DrawJagobjLump(numslump + leveltens,
				item->x + 70, y + 2, NULL, NULL);
			DrawJagobjLump(numslump + levelones, item->x + 84, y + 2, NULL, NULL);
		}
		else
			DrawJagobjLump(numslump + levelones, item->x + 70, y + 2, NULL, NULL);

		print((320 - (tmplen * 14)) >> 1, y + ITEMSPACE + 2, tmp);

		item = &mainitem[mi_difficulty];
		y = y_offset + item->y;

#ifndef MARS
		EraseBlock(82, m_doom_height + CURSORY(NUMMAINITEMS - 1) + ITEMSPACE + 2, 320 - 72, m_skill[playerskill]->height + 10);
#endif
		/* draw difficulty information */
		DrawJagobjLump(m_skilllump + playerskill, item->x + 10, y + ITEMSPACE + 2, NULL, NULL);
	}
	else if (scrpos == ms_load || scrpos == ms_save)
	{
		mainitem_t* item;
		
		item = &mainitem[mi_savelist];
		y = y_offset + item->y;

		if (savecount > 0)
		{
			if (saveslot == 0)
				print(item->x + 10, y + 20 + 2, "Last autosave.");
			else
			{
				print(item->x + 10, y + 20, "Slot ");
				DrawJagobjLump(numslump + saveslot % 10, item->x + 70, y + ITEMSPACE + 1, NULL, NULL);
			}

			if (saveslotmap != -1)
			{
				char mapname[64];
				int mapnamelen;

				M_MapName(saveslotmap, mapname, sizeof(mapname));
				mapnamelen = mystrlen(mapname);

				leveltens = saveslotmap / 10, levelones = saveslotmap % 10;

				print(item->x + 10, y + 40 + 2, "Area");

				if (leveltens)
				{
					DrawJagobjLump(numslump + leveltens,
						item->x + 80, y + 40 + 3, NULL, NULL);
					DrawJagobjLump(numslump + levelones, item->x + 94, y + ITEMSPACE*2 + 3, NULL, NULL);
				}
				else
					DrawJagobjLump(numslump + levelones, item->x + 80, y + ITEMSPACE*2 + 3, NULL, NULL);

				print((320 - (mapnamelen * 14)) >> 1, y + ITEMSPACE*3 + 3, mapname);
			}
			else
			{
				print(item->x + 10, y + ITEMSPACE*2 + 2, "Empty");
			}
			if (saveslotskill != -1)
			{
				/* draw difficulty information */
				print(item->x + 10, y + ITEMSPACE*4 + 2, playmodes[saveslotmode]);
				DrawJagobjLump(m_skilllump + saveslotskill, item->x + 10, y + ITEMSPACE*5 + 2, NULL, NULL);
			}
		}
		else
		{
			print(CURSORX, y + ITEMSPACE+10 + 2, "Reach your first");
			print(CURSORX, y + ITEMSPACE*2+10 + 2, "checkpoint after");
			print(CURSORX, y + ITEMSPACE*3+10 + 2, "the first area.");
		}
	}

#ifndef MARS
	UpdateBuffer();
#endif
}
