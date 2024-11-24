/* m_main.c -- main menu */

#include "doomdef.h"

#define MOVEWAIT		(I_IsPAL() ? TICVBLS*5 : TICVBLS*6)
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
	mi_joingame,
	mi_level,
	mi_gamemode,
	mi_difficulty,
	mi_savelist,
	mi_singleplayer,
	mi_splitscreen,
	mi_network,
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
	ms_gametype,
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
static VINT currentgametype = mi_singleplayer;
static VINT	cursorpos;
static VINT screenpos;
static VINT playerskill;

static VINT saveslot;
static VINT savecount;
static VINT prevsaveslot;
static VINT saveslotmap;
static VINT saveslotskill;
static VINT saveslotmode;
static char savewadname[32];
static char savemapname[32];
static char savewadmismatch;

static boolean startup;

extern VINT	uchar;
extern void print(int x, int y, const char* string);

void M_Start2 (boolean startup_)
{
	int i;

/* cache all needed graphics	 */
	startup = startup_;
	if (startup)
	{
		W_LoadPWAD(PWAD_CD);

		i = W_CheckNumForName("M_DOOM");
		m_doom = i != -1 ? W_CacheLumpNum(i, PU_STATIC) : NULL;

		W_LoadPWAD(PWAD_NONE);
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
	savewadmismatch = 0;

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

	mainscreen[ms_gametype].firstitem = mi_singleplayer;
	mainscreen[ms_gametype].numitems = 3;

	D_memcpy(mainitem[mi_newgame].name, "New Game", 9);
	mainitem[mi_newgame].x = ITEMX;
	mainitem[mi_newgame].y = CURSORY(0);
	mainitem[mi_newgame].screen = ms_gametype;

	D_memcpy(mainitem[mi_loadgame].name, "Load Game", 10);
	mainitem[mi_loadgame].x = ITEMX;
	mainitem[mi_loadgame].y = CURSORY(1);
	mainitem[mi_loadgame].screen = ms_load;
	mainscreen[ms_main].numitems++;

	D_memcpy(mainitem[mi_savegame].name, "Save Game", 10);
	mainitem[mi_savegame].x = ITEMX;
	mainitem[mi_savegame].y = CURSORY(2);
	mainitem[mi_savegame].screen = ms_save;
	mainscreen[ms_main].numitems++;

	D_memcpy(mainitem[mi_joingame].name, "Join Game", 10);
	mainitem[mi_joingame].x = ITEMX;
	mainitem[mi_joingame].y = CURSORY(3);
	mainitem[mi_joingame].screen = ms_none;
	mainscreen[ms_main].numitems++;

	D_memcpy(mainitem[mi_level].name, "Level", 6);
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

	D_memcpy(mainitem[mi_singleplayer].name, "Single Player", 14);
	mainitem[mi_singleplayer].x = ITEMX;
	mainitem[mi_singleplayer].y = CURSORY(0);
	mainitem[mi_singleplayer].screen = ms_new;

	D_memcpy(mainitem[mi_splitscreen].name, "Split-Screen", 13);
	mainitem[mi_splitscreen].x = ITEMX;
	mainitem[mi_splitscreen].y = CURSORY(1);
	mainitem[mi_splitscreen].screen = ms_new;

	D_memcpy(mainitem[mi_network].name, "Multiplayer", 12);
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

static const char* M_MapName(VINT mapnum)
{
	return G_MapNameForMapNum(mapnum);
}

static void M_UpdateSaveInfo(void)
{
	if (prevsaveslot != saveslot) {
		prevsaveslot = saveslot;
		saveslotmap = -1;
		saveslotskill = -1;
		saveslotmode = gt_single;
		GetSaveInfo(saveslot, &saveslotmap, &saveslotskill, &saveslotmode, savewadname, savemapname);
		savewadmismatch = D_strcasecmp(savewadname, cd_pwad_name);
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

/* animate skull */
	if (gametic != prevgametic && (gametic&3) == 0)
	{
		cursorframe ^= 1;
	}

	M_UpdateSaveInfo();

	buttons = ticrealbuttons & MENU_BTNMASK;
	oldbuttons = oldticrealbuttons & MENU_BTNMASK;

	if ((buttons & (BT_A | BT_LMBTN)) && !(oldbuttons & (BT_A | BT_LMBTN)))
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
			S_StartSound(NULL, sfx_pistol);
			return ga_nothing;
		}
	}

	if ((buttons & (BT_B | BT_RMBTN)) && !(oldbuttons & (BT_B | BT_RMBTN)))
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
			consoleplayer = 0;
			startsave = -1;
			startmap = gamemaplist[playermap - 1]->mapNumber; /*set map number */
			startskill = playerskill;	/* set skill level */
			starttype = currentplaymode;	/* set play type */
			startsplitscreen = currentgametype == mi_splitscreen;
			if ((ticrealbuttons & (BT_Y|BT_MODE)) == (BT_Y|BT_MODE))
			{
				// hold Y and MODE to begin recording a demo
				demorecording = true;
			}
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
			if (savecount > 0 && !savewadmismatch)
			{
				startsplitscreen = saveslotmode != gt_single;
				startsave = saveslot;
				return ga_startnew;
			}
		}

		if (screenpos == ms_save)
		{
			S_StartSound(NULL, sfx_pistol);
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
					}
					if (buttons & BT_LEFT)
					{
						if(--playermap == 0)
							playermap = gamemapcount;
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
= M_Drawer
=
=================
*/

void M_Drawer (void)
{
	int i;
	int mapnumber = gamemaplist[playermap - 1]->mapNumber;
	int	leveltens = mapnumber / 10, levelones = mapnumber % 10;
	int scrpos = screenpos == ms_none ? ms_main : screenpos;
	mainscreen_t* menuscr = &mainscreen[scrpos];
	mainitem_t* items = &mainitem[menuscr->firstitem];
	int y, y_offset = 0;

/* Draw main menu */
	if (m_doom && (scrpos == ms_main || scrpos == ms_gametype))
	{
		DrawJagobj(m_doom, 100, 4);
		y_offset = m_doom->height + 4 - STARTY;
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
		const char *tmp;
		int tmplen;

		/* draw game mode information */
		item = &mainitem[mi_gamemode];
		y = y_offset + item->y;
		print(item->x + 10, y + ITEMSPACE + 2, playmodes[currentplaymode]);

		/* draw start level information */
		item = &mainitem[mi_level];
		y = y_offset + item->y;

		tmp = M_MapName(mapnumber);
		tmplen = mystrlen(tmp);

#ifndef MARS
		EraseBlock(80, m_doom_height + CURSORY(NUMMAINITEMS - 2) + ITEMSPACE + 2, 320, nums[0]->height);
#endif
		if (leveltens)
		{
			DrawJagobjLump(numslump + leveltens, item->x + 76, y, NULL, NULL);
			DrawJagobjLump(numslump + levelones, item->x + 90, y, NULL, NULL);
		}
		else
			DrawJagobjLump(numslump + levelones, item->x + 76, y, NULL, NULL);

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

		M_UpdateSaveInfo();

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
				char *mapname;
				int mapnamelen;

				mapname = savemapname;
				mapnamelen = mystrlen(mapname);

				leveltens = saveslotmap / 10, levelones = saveslotmap % 10;

				print(item->x + 10, y + ITEMSPACE*2 + 2, "Level");

				if (leveltens)
				{
					DrawJagobjLump(numslump + leveltens, item->x + 86, y + ITEMSPACE*2 + 3, NULL, NULL);
					DrawJagobjLump(numslump + levelones, item->x + 100, y + ITEMSPACE*2 + 3, NULL, NULL);
				}
				else
					DrawJagobjLump(numslump + levelones, item->x + 86, y + ITEMSPACE*2 + 3, NULL, NULL);

				print((320 - (mapnamelen * 14)) >> 1, y + ITEMSPACE*3 + 3, mapname);

				if (scrpos == ms_load && savewadmismatch)
				{
					print(item->x + 10, y + ITEMSPACE*4 + 2, "WAD Mismatch");
					print(item->x + 10, y + ITEMSPACE*5 + 2, savewadname);
					return;
				}
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
}
