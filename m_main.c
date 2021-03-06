/* m_main.c -- main menu */

#include "doomdef.h"

#define MOVEWAIT		2
#define CURSORX		40
#define STARTY			40
#define CURSORY(y)	(STARTY*(y))
#define	NUMLCHARS 64	

typedef enum
{
#ifndef MARS
	gamemode,
#endif
	level,
	difficulty,
	NUMMENUITEMS
} menu_t;

typedef enum
{
	single,
	coop,
	dmatch,
	NUMMODES
} playmode_t;

jagobj_t* m_doom;
#ifndef MARS
jagobj_t *m_single,*m_coop,*m_deathmatch,*m_gamemode;
jagobj_t* m_playmode[NUMMODES];
#endif

short m_skull1lump, m_skull2lump;
short m_skilllump;
short numslump;

int		cursorframe, cursorcount;
int		movecount;
int 		playermap;

playmode_t currentplaymode;
menu_t	cursorpos;
skill_t	playerskill;

static int *mapnumbers;
static int mapcount;

extern void print(int x, int y, const char* string);

void M_Start (void)
{	
	int i;
#ifndef MARS
	int j;
#endif
	dmapinfo_t** maplist;
	int* tempmapnums;

	// copy mapnumbers to a temp buffer, then free, then allocate again
	// to avoid zone memory fragmentation
	maplist = G_LoadMaplist(&mapcount);
	tempmapnums = (int *)I_WorkBuffer();
	for (i = 0; i < mapcount; i++)
		tempmapnums[i] = maplist[i]->mapnumber;

	for (i = 0; i < mapcount; i++)
		Z_Free(maplist[i]);
	Z_Free(maplist);

	mapnumbers = Z_Malloc(sizeof(*mapnumbers) * mapcount, PU_STATIC, 0);
	for (i = 0; i < mapcount; i++)
		mapnumbers[i] = tempmapnums[i];

/* cache all needed graphics	 */
	i = W_CheckNumForName("M_DOOM");
	m_doom = i != -1 ? W_CacheLumpNum(i,PU_STATIC) : NULL;
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

	cursorcount = 0;
	cursorframe = 0;
	cursorpos = 0;
	playerskill = startskill;
	playermap = startmap;

	DoubleBufferSetup();
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

	Z_Free(mapnumbers);

	WriteEEProm ();
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

	buttons = ticrealbuttons;
	
	if (!m_doom)
		return 1;

	/* exit menu if button press */
	if ( ticon > 10 &&	(buttons & (BT_A|BT_LMBTN|BT_START))   )
	{
		startmap = mapnumbers[playermap - 1]; /*set map number */
		startskill = playerskill;	/* set skill level */
		starttype = currentplaymode;	/* set play type */
		return 1;		/* done with menu */
	}

/* animate skull */
	if (++cursorcount == ticrate)
	{
		cursorframe ^= 1;
		cursorcount = 0;
		newframe = 1;
	}

	if (!newframe)
		return 0;
	newframe = 0;

/* check for movement */
	if (! (buttons & (BT_UP|BT_DOWN|BT_LEFT|BT_RIGHT) ) )
		movecount = 0;		/* move immediately on next press */
	else
	{
		if (cursorpos == level)
		{
			if (movecount == 1)
				movecount = 0;		/* faster level selection */
		}
		else
		{
			if (movecount == MOVEWAIT)
				movecount = 0;		/* slower everything else */
		}

		if (++movecount == 1)
		{
			if (buttons & BT_DOWN)
			{
				cursorpos++;
				if (cursorpos == NUMMENUITEMS)
					cursorpos = 0;
			}
		
			if (buttons & BT_UP)
			{
				cursorpos--;
				if (cursorpos == -1)
					cursorpos = NUMMENUITEMS-1;
			}

			switch (cursorpos)
			{
#ifndef MARS
				case	gamemode:
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
				case	level:
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
				case	difficulty:
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
				default:
					break;
			}
		}
	}


	return 0;
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
	int mapnumber = mapnumbers[playermap - 1];
	int	leveltens = mapnumber / 10, levelones = mapnumber % 10;

/* Draw main menu */
	if (!m_doom)
		return;
	int m_doom_height = m_doom->height;

	DrawJagobj(m_doom, 100, 2);
	
/* erase old skulls */
#ifndef MARS
	EraseBlock (CURSORX, m_doom_height,m_skull1->width, CURSORY(NUMMENUITEMS)- CURSORY(0));
#endif

/* draw new skull */
	if (cursorframe)
		DrawJagobjLump(m_skull2lump, CURSORX, CURSORY(cursorpos)+m_doom_height, NULL, NULL);
	else
		DrawJagobjLump(m_skull1lump, CURSORX, CURSORY(cursorpos)+m_doom_height, NULL, NULL);

/* draw menu items */

#ifndef MARS
/* draw game mode information */
	DrawJagobj(m_gamemode, 64, m_doom_height+2); 
	EraseBlock(80,m_doom_height+22,320- 80,200-m_doom_height+22);
	DrawJagobj(m_playmode[currentplaymode], 80,m_doom_height+22);
#endif

/* draw start level information */
	print(64, CURSORY(NUMMENUITEMS - 2) + m_doom_height + 2, "Area");
#ifndef MARS
	EraseBlock(80, m_doom_height + CURSORY(NUMMENUITEMS - 2) + 20 + 2, 320, nums[0]->height);
#endif

	if (leveltens)	
	{
		DrawJagobjLump(numslump+leveltens,
			80,m_doom_height+CURSORY(NUMMENUITEMS - 2)+20+2, NULL, NULL);
		DrawJagobjLump(numslump+levelones,94,m_doom_height+ CURSORY(NUMMENUITEMS - 2) + 20 + 2, NULL, NULL);
	}
	else
		DrawJagobjLump(numslump+levelones, 80,m_doom_height+ CURSORY(NUMMENUITEMS - 2) + 20 + 2, NULL, NULL);

/* draw difficulty information */
	print(CURSORX + 24, CURSORY(NUMMENUITEMS - 1) + m_doom_height + 2, "Difficulty");
#ifndef MARS
	EraseBlock(82, m_doom_height + CURSORY(NUMMENUITEMS - 1) + 20 + 2, 320 - 72, m_skill[playerskill]->height+10);
#endif
	DrawJagobjLump(m_skilllump + playerskill, 82,m_doom_height+ CURSORY(NUMMENUITEMS - 1) + 20 + 2, NULL, NULL);

#ifndef MARS
	UpdateBuffer ();
#endif
}
