/* m_main.c -- main menu */

#include "doomdef.h"
	
#define MOVEWAIT		3
#define CURSORX		50
#define STARTY			40
#define CURSORY(y)	(STARTY*(y))
#define	NUMLCHARS 64	

typedef enum
{
	gamemode,
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

jagobj_t	*m_doom,*m_skull1,*m_skull2,*m_gamemode,*m_level,
			*m_difficulty,*m_single,*m_coop,*m_deathmatch;
			
jagobj_t *nums[10];
jagobj_t	*m_skill[5];
jagobj_t *m_playmode[NUMMODES];

int		cursorframe, cursorcount;
int		movecount;
int 		playermap;

playmode_t currentplaymode;
menu_t	cursorpos;
skill_t	playerskill;

void M_Start (void)
{	
	int i,l;
	
/* cache all needed graphics	 */
	m_doom = W_CacheLumpName ("M_DOOM",PU_STATIC);
	m_skull1 = W_CacheLumpName ("M_SKULL1",PU_STATIC);
	m_skull2 = W_CacheLumpName ("M_SKULL2",PU_STATIC);
	m_gamemode = W_CacheLumpName ("M_GAMMOD",PU_STATIC);
	m_level = W_CacheLumpName ("M_LEVEL",PU_STATIC);
	m_difficulty =W_CacheLumpName ("M_DIFF",PU_STATIC);

	l = W_GetNumForName ("M_SINGLE");
	for (i = 0; i < NUMMODES; i++)
		m_playmode[i] = W_CacheLumpNum (l+i, PU_STATIC);

	l = W_GetNumForName ("SKILL0");
	for (i = 0; i < 5; i++)
		m_skill[i] = W_CacheLumpNum (l+i, PU_STATIC);

	l = W_GetNumForName ("NUM_0");
	for (i = 0; i < 10; i++)
		nums[i] = W_CacheLumpNum (l+i, PU_STATIC);

	cursorcount = 0;
	cursorframe = 0;
	cursorpos = 0;
	playerskill = startskill;
	playermap = startmap;
	
	DoubleBufferSetup ();
}

void M_Stop (void)
{	
#if 0
/* they stay cached by status bar */

	int i;

/* free all loaded graphics */

	Z_Free (m_doom);
	Z_Free (m_skull1);
	Z_Free (m_skull2);
	Z_Free (m_gamemode);
	Z_Free (m_level);
	Z_Free (m_difficulty);

	for (i = 0; i < NUMMODES; i++)
		Z_Free(m_playmode[i]);

	for (i = 0; i < 5; i++)
		Z_Free(m_skill[i]);

	for (i = 0; i < 10; i++)
		Z_Free(nums[i]);
#endif

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
	
	buttons = ticbuttons[consoleplayer];
	
/* exit menu if button press */
	if ( ticon > 10 &&	(buttons & (JP_A|JP_B|JP_C))   )
	{
		startmap =	playermap; /*set map number */
		startskill = playerskill;	/* set skill level */
		starttype = currentplaymode;	/* set play type */
		return 1;		/* done with menu */
	}
/* animate skull */
	if (++cursorcount == 4)
	{
		cursorframe ^= 1;
		cursorcount = 0;
	}
			

/* check for movement */
	if (! (buttons & (JP_UP|JP_DOWN|JP_LEFT|JP_RIGHT) ) )
		movecount = 0;		/* move immediately on next press */
	else
	{
		if (cursorpos ==level && movecount == 3)
			movecount = 0;		/* fast level select */
		if (movecount == 6)
			movecount = 0;		/* slower everything else */
		if (++movecount == 1)
		{
			if (buttons & JP_DOWN)
			{
				cursorpos++;
				if (cursorpos == NUMMENUITEMS)
					cursorpos = 0;
			}
		
			if (buttons & JP_UP)
			{
				cursorpos--;
				if (cursorpos == -1)
					cursorpos = NUMMENUITEMS-1;
			}

			switch (cursorpos)
			{
				case	gamemode:
					if (buttons & JP_RIGHT)
					{
						currentplaymode++;
						if (currentplaymode == NUMMODES)
							currentplaymode--;
					}
					if (buttons & JP_LEFT)
					{
						currentplaymode--;
						if (currentplaymode == -1)
							currentplaymode++;
				}
					break;
				case	level:
					if (buttons & JP_RIGHT)
					{			
						playermap++;
						if (playermap == maxlevel+1)
							playermap--;
					}	
					if (buttons & JP_LEFT)
					{
						playermap--;
						if(playermap == 0)
							playermap++;
					}
					break;
				case	difficulty:
					if (buttons & JP_RIGHT)
					{
						playerskill++;
						if (playerskill > sk_nightmare)
							playerskill--;
					}
					if (buttons & JP_LEFT)
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
	int	leveltens, levelones;		

/* Draw main menu */
	DrawJagobj (m_doom, 100, 2);
	
/* erase old skulls */
	EraseBlock (CURSORX, 0,m_skull1->width,240);

/* draw new skull */
	if (cursorframe)
		DrawJagobj (m_skull2, CURSORX, CURSORY(cursorpos)+m_doom->height);
	else
		DrawJagobj (m_skull1, CURSORX, CURSORY(cursorpos)+m_doom->height);

/* draw menu items */

/* draw game mode information */
	DrawJagobj(m_gamemode, 74, m_doom->height+2); 
	EraseBlock(90,m_doom->height+22,320-90,240-m_doom->height+22);
	DrawJagobj(m_playmode[currentplaymode],90,m_doom->height+22);

/* draw start level information */
	DrawJagobj(m_level, 74 ,CURSORY(1)+m_doom->height+2); 
	leveltens = playermap / 10;
	levelones = playermap % 10;
	EraseBlock(90,m_doom->height+61,320-90,200-m_doom->height+62);
	if (leveltens)	
	{
		DrawJagobj(nums[leveltens],
		90,m_doom->height+62);
		DrawJagobj(nums[levelones],104,m_doom->height+62);
	}
	else
		DrawJagobj(nums[levelones],90,m_doom->height+62);

/* draw difficulty information */
	DrawJagobj(m_difficulty, CURSORX+24, CURSORY(2)+m_doom->height+2); 
	EraseBlock(92,m_doom->height+102,320-92,240-m_doom->height+102);
	DrawJagobj(m_skill[playerskill],92,m_doom->height+102);

	UpdateBuffer ();
}
