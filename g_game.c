/* G_game.c  */

#include "doomdef.h"
#include "p_local.h"
#include <string.h>

void G_PlayerReborn (int player);

void G_DoReborn (int playernum);

void G_DoLoadLevel (void);


gameaction_t    gameaction;
int				gamemaplump;
dmapinfo_t		gamemapinfo;
dgameinfo_t		gameinfo;

VINT 			*gamemapnumbers;
VINT 			*gamemaplumps;
VINT 			gamemapcount;

gametype_t		netgame;

boolean         playeringame[MAXPLAYERS];
player_t        players[MAXPLAYERS];
playerresp_t	playersresp[MAXPLAYERS];

int             consoleplayer = 0;          /* player taking events and displaying  */
int             gametic;
int             leveltime;
VINT            fadetime;
VINT           totalitems, totalsecret;    /* for intermission  */
uint16_t        emeralds;
uint16_t        token;
uint16_t        tokenbits;

boolean         demorecording;
boolean         demoplayback;

unsigned char *demo_p = 0;
unsigned char *demobuffer = 0;

/*
==============
=
= G_DoLoadLevel
=
==============
*/

extern int              skytexture;
extern texture_t		*skytexturep;
extern texture_t		*textures;

static int G_MapNumForLumpNum(int lump)
{
	int i;

	/* find the map by its lump */
	for (i = 0; i < gamemapcount; i++)
	{
		if (gamemaplumps[i] == lump)
		{
			return gamemapnumbers[i];
		}
	}
	return -1;
}

int G_LumpNumForMapNum(int map)
{
	int i;

 	/* find the map by its number */
	for (i = 0; i < gamemapcount; i++)
	{
		if (gamemapnumbers[i] > map)
			break;
		if (gamemapnumbers[i] == map)
			return gamemaplumps[i];
	}
	return -1;
}

static char* G_GetMapNameForLump(int lump)
{
	static char name[9];
	D_memcpy(name, W_GetNameForNum(lump), 8);
	name[8] = '0';
	return name;
}

void G_DoLoadLevel (void) 
{ 
	int             i; 
	int		skytexturel;
	int 		gamemap;
	int			music;

	totalitems = totalsecret = 0;
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		if (playeringame[i]/* && players[i].playerstate == PST_DEAD*/)
			players[i].playerstate = PST_REBORN;

		players[i].killcount = players[i].secretcount
			= players[i].itemcount = 0;
		players[i].starpostnum = 0;
		players[i].pflags = 0;
		players[i].health = 1;
		players[i].shield = 0;
		D_memset(players[i].powers, 0, sizeof(players[i].powers));
		players[i].whiteFlash = 0;
		players[i].lossCount = 0;
		players[i].stillTimer = 0;
		players[i].justSprung = 0;
		players[i].scoreAdd = 0;
		players[i].dashSpeed = 0;
		players[i].homingTimer = 0;
		players[i].xtralife = 0;
	}

	Z_CheckHeap (mainzone);
#ifndef MARS
	Z_CheckHeap (refzone);
#endif

        Z_FreeTags (mainzone);
	/*PrintHex (1,1,Z_FreeMemory (mainzone)); */
/*
 * get lump number for the next map
 */

 /* free DMAPINFO now */
	if (gamemapinfo.data)
		Z_Free(gamemapinfo.data);
	gamemapinfo.data = NULL;

	if (G_FindMapinfo(gamemaplump, &gamemapinfo, NULL) == 0) {
		int nextmap;
		const char *mapname;

		mapname = G_GetMapNameForLump(gamemaplump);
		gamemap = G_BuiltinMapNumForMapName(mapname);
		if (gamemap == 0)
			gamemap = gamemapinfo.mapNumber + 1;

		gamemapinfo.sky = NULL;
		gamemapinfo.mapNumber = gamemap;
		gamemapinfo.lumpNum = gamemaplump;

		/* decide which level to go to next */
		nextmap = gamemap + 1;

		if (nextmap)
			gamemapinfo.next = G_LumpNumForMapNum(nextmap);
		else
			gamemapinfo.next = 0;
	}

	/* DMAPINFO can override the map number */
	gamemap = gamemapinfo.mapNumber;

	/* update the start map */
	startmap = gamemap;

	/*  */
	/* set the sky map for the episode  */
	/*  */
	if (gamemapinfo.sky == NULL)
	{
		if (gamemap < 9)
			gamemapinfo.sky = "SKY1";
		else if (gamemap < 18)
			gamemapinfo.sky = "SKY2";
		else
			gamemapinfo.sky = "SKY3";
	}

	skytexturel = R_TextureNumForName(gamemapinfo.sky);
 	skytexturep = &textures[skytexturel];

	P_SetupLevel (gamemaplump);
	gameaction = ga_nothing; 

	music = gamemapinfo.musicLump;
	if (music <= 0)
		music = S_SongForMapnum(gamemap);

	if (gamemapinfo.mapNumber != 30)
	{
		if (netgame != gt_single && !splitscreen)
			S_StopSong();
		S_StartSong(music, 1, gamemap);
	}

	//Z_CheckHeap (mainzone);  		/* DEBUG */
} 

/* 
============================================================================== 
 
						PLAYER STRUCTURE FUNCTIONS 
 
also see P_SpawnPlayer in P_Mobj 
============================================================================== 
*/ 

//
//  Clip the console player mouse aiming to the current view,
//  also returns a signed char for the player ticcmd if needed.
//  Used whenever the player view pitch is changed manually
//
//added:22-02-98:
//changed:3-3-98: do a angle limitation now
short G_ClipAimingPitch (int* aiming)
{
    int limitangle = ANG45;

    if (*aiming > limitangle)
        *aiming = limitangle;
    else if (*aiming < -limitangle)
        *aiming = -limitangle;

    return (*aiming) >> 16;
}
 
/* 
==================== 
= 
= G_PlayerFinishLevel 
= 
= Can when a player completes a level 
==================== 
*/ 
 
void G_PlayerFinishLevel (int player) 
{ 
	player_t        *p; 

	p = &players[player]; 
	 
	D_memset (p->powers, 0, sizeof (p->powers)); 
	p->whiteFlash = 0; 

	if (netgame == gt_deathmatch)
		return;
	if (gameaction == ga_died)
		return;

	if (p->health <= 0)
		return;

	P_UpdateResp(p);
} 
 
/* 
==================== 
= 
= G_PlayerReborn 
= 
= Called after a player dies 
= almost everything is cleared and initialized 
==================== 
*/ 
void G_PlayerReborn (int player) 
{ 
	player_t        *p; 
	int             itemcount;
	int             secretcount;
	int             starpostnum;
	VINT            starpostx;
	VINT            starposty;
	VINT            starpostz;
	VINT            starpostangle;
	VINT            lives;
	int             score;

	p = &players[player]; 
	itemcount = p->itemcount;
	secretcount = p->secretcount;
	starpostnum = p->starpostnum;
	starpostx = p->starpostx;
	starposty = p->starposty;
	starpostz = p->starpostz;
	starpostangle = p->starpostangle;
	lives = p->lives;
	score = p->score;

	D_memset (p, 0, sizeof(*p)); 

	p->itemcount = itemcount;
	p->secretcount = secretcount;
	p->starpostnum = starpostnum;
	p->starpostx = starpostx;
	p->starposty = starposty;
	p->starpostz = starpostz;
	p->starpostangle = starpostangle;
	p->lives = lives;
	p->score = score;

	p->playerstate = PST_LIVE;

	P_RestoreResp(p);
}
 
 
/* 
==================== 
= 
= G_CheckSpot  
= 
= Returns false if the player cannot be respawned at the given mapthing_t spot  
= because something is occupying it 
==================== 
*/ 
 
void P_SpawnPlayer (mapthing_t *mthing); 
 
boolean G_CheckSpot (int playernum, mapthing_t *mthing) 
{ 
	fixed_t         x,y; 
	int             an;
	ptrymove_t		tm;

	x = mthing->x << FRACBITS; 
	y = mthing->y << FRACBITS; 
	 
	// killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
	// corpse to detect collisions with other players in DM starts
	players[playernum].mo->flags |= MF_SOLID;
	an = P_CheckPosition(&tm, players[playernum].mo, x, y);
	players[playernum].mo->flags &= ~MF_SOLID;
	if (!an ) 
		return false; 

	return true; 
} 
 
/* 
==================== 
= 
= G_DeathMatchSpawnPlayer 
= 
= Spawns a player at one of the random death match spots 
= called at level load and each death 
==================== 
*/ 
 
void G_DeathMatchSpawnPlayer (int playernum) 
{ 
	int             i,j; 
	int				selections;
	
	selections = deathmatch_p - deathmatchstarts;
	if (selections < 4)
		I_Error ("Only %i deathmatch spots, 4 required", selections);
		
	for (j=0 ; j<20 ; j++) 
	{ 
		i = P_Random()%selections;
		if (G_CheckSpot (playernum, &deathmatchstarts[i]) ) 
		{ 
			deathmatchstarts[i].type = playernum+1; 
			P_SpawnPlayer (&deathmatchstarts[i]); 
			return; 
		} 
	} 
 
/* no good spot, so the player will probably get stuck  */
	P_SpawnPlayer (&playerstarts[playernum]); 
} 
 
/* 
==================== 
= 
= G_DoReborn 
= 
==================== 
*/ 
 
void G_DoReborn (int playernum) 
{ 
	int 		i; 
	 
	if (!netgame)
	{
		gameaction = ga_died;			/* reload the level from scratch  */
		return;
	}

/*	 */
/* respawn this player while the other players keep going */
/* */
	players[playernum].mo->player = 0;   /* dissasociate the corpse  */
		
	/* spawn at random spot if in death match  */
	if (netgame == gt_deathmatch) 
	{ 
		G_DeathMatchSpawnPlayer (playernum); 
		return; 
	} 
		
	if (G_CheckSpot (playernum, &playerstarts[playernum]) ) 
	{ 
		P_SpawnPlayer (&playerstarts[playernum]); 
		return; 
	} 
	/* try to spawn at one of the other players spots  */
	for (i=0 ; i<MAXPLAYERS ; i++) 
		if (G_CheckSpot (playernum, &playerstarts[i]) ) 
		{ 
			playerstarts[i].type = playernum+1;		/* fake as other player  */
			P_SpawnPlayer (&playerstarts[i]); 
			playerstarts[i].type = i+1;             /* restore  */
			return; 
		} 
	/* he's going to be inside something.  Too bad.  */
	P_SpawnPlayer (&playerstarts[playernum]); 
} 
 
 
/* 
==================== 
= 
= G_ExitLevel 
= 
==================== 
*/ 
 
void G_ExitLevel (void) 
{
	gameaction = ga_completed;
} 
 
void G_SecretExitLevel (void) 
{ 
	gameaction = ga_secretexit;
} 
  
/*============================================================================  */

static void G_InitPlayerResp(void)
{
	int i;
	for (i = 0; i < MAXPLAYERS; i++)
		R_ResetResp(&players[i]);
}

void G_Init(void)
{
	int i;
	int mapcount;
	dmapinfo_t** maplist;
	VINT* tempmapnums;

	// copy mapnumbers to a temp buffer, then free, then allocate again
	// to avoid zone memory fragmentation
	gamemapcount = 0;
	gamemapnumbers = NULL;

	maplist = G_LoadMaplist(&mapcount);
	if (maplist)
	{
		tempmapnums = (VINT*)I_WorkBuffer();
		for (i = 0; i < mapcount; i++)
		{
			int mn = maplist[i]->mapNumber;
			if (mn == 0)
				mn = i + 1;
			tempmapnums[i*2] = mn;
			tempmapnums[i*2+1] = maplist[i]->lumpNum;
		}

		for (i = 0; i < mapcount; i++)
			Z_Free(maplist[i]);
		Z_Free(maplist);

	}
	else
	{
		char lumpname[9];

		mapcount = 0;
		tempmapnums = (VINT*)I_WorkBuffer();
		for (i = 1; i < 99; i++) {
			int l;
			D_snprintf(lumpname, sizeof(lumpname), "MAP%02d", i);
			l = W_CheckNumForName(lumpname);
			if (l < 0)
				break;
			tempmapnums[mapcount*2] = i;
			tempmapnums[mapcount*2+1] = l;
			mapcount++;
		}
	}

	gamemapcount = mapcount;
	if (mapcount > 0)
	{
		gamemapnumbers = Z_Malloc(sizeof(*gamemapnumbers) * mapcount, PU_STATIC);
		gamemaplumps = Z_Malloc(sizeof(*gamemaplumps) * mapcount, PU_STATIC);
		for (i = 0; i < mapcount; i++)
		{
			gamemapnumbers[i] = tempmapnums[i*2];
			gamemaplumps[i] = tempmapnums[i*2+1];
		}
	}

	G_InitPlayerResp();

	if (G_FindGameinfo(&gameinfo))
	{
		if (gameinfo.borderFlat <= 0)
			gameinfo.borderFlat = W_CheckNumForName("SRB2TILE");
		if (gameinfo.endFlat <= 0)
			gameinfo.endFlat = gameinfo.borderFlat;
		return;
	}

	gameinfo.borderFlat = W_CheckNumForName("SRB2TILE");
	gameinfo.titlePage = W_CheckNumForName("title");
	gameinfo.titleTime = 540;
	gameinfo.endFlat = gameinfo.borderFlat;
	gameinfo.endShowCast = 1;
}

/* 
==================== 
= 
= G_InitNew 
= 
==================== 
*/ 
 
extern mobj_t emptymobj;
 
void G_InitNew (int map, gametype_t gametype, boolean splitscr)
{ 
	int             i; 

//D_printf ("G_InitNew\n");

	M_ClearRandom (); 

	if (gamemapinfo.data)
		Z_Free(gamemapinfo.data);
	D_memset(&gamemapinfo, 0, sizeof(gamemapinfo));

	/* these may be reset by I_NetSetup */
	gamemaplump = G_LumpNumForMapNum(map);

	if (gamemaplump < 0)
		I_Error("Lump MAP%02d not found!", map);

 	netgame = gametype;
	splitscreen = splitscr;
#ifndef MARS
	I_DrawSbar ();			/* draw frag boxes if multiplayer */
#endif

/* force players to be initialized upon first level load          */
	for (i=0 ; i<MAXPLAYERS ; i++) 
	{
		players[i].lives = 3;
		players[i].starpostnum = 0;
		players[i].playerstate = PST_REBORN;
	}

	for (i=0 ; i<MAXPLAYERS ; i++)
		players[i].mo = &emptymobj;	/* for net consistency checks */

	G_InitPlayerResp();

	playeringame[0] = true;	
	if (netgame != gt_single)
		playeringame[1] = true;	
	else
		playeringame[1] = false;	

	demorecording = false;
	demoplayback = false;

	gamepaused = false;
	gametic = 0;
	emeralds = 0;
	token = 0;
	tokenbits = 0;
} 

void G_LoadGame(int saveslot)
{
	playerresp_t backup[MAXPLAYERS];

	ReadGame(saveslot);

	D_memcpy(backup, playersresp, sizeof(playersresp));

	G_InitNew(startmap, starttype, startsplitscreen);

	D_memcpy(playersresp, backup, sizeof(playersresp));
}

/*============================================================================  */
 
static int 		nextmapl = -1;
static int      returnspecstagemapl = -1;

/*
=================
=
= G_RunGame
=
= The game should allready have been initialized or laoded
=================
*/
void G_RunGame (void)
{
	int		i;

	while (1)
	{
		boolean		finale;
#ifdef JAGUAR
		int			nextmap;
#endif

		if (nextmapl == -1)
			nextmapl = G_LumpNumForMapNum(1);

		/* run a level until death or completion */
		MiniLoop(P_Start, P_Stop, P_Ticker, P_Drawer, P_Update);

		if (gameaction == ga_startnew)
		{
startnew:
			if (starttype != gt_single && !startsplitscreen)
				I_NetSetup();
			else
				I_NetStop();
			if (startsave != -1)
				G_LoadGame(startsave);
			else
				G_InitNew(startmap, starttype, startsplitscreen);
			continue;
		}

		/* take away cards and stuff */

		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i])
				G_PlayerFinishLevel(i);

		if (gameaction == ga_died)
			continue;			/* died, so restart the level */

		if (gameaction == ga_warped)
		{
			if (starttype != netgame || startmap != gamemapinfo.mapNumber)
			{
				gameaction = ga_startnew;
				goto startnew;
			}
			continue;			/* skip intermission */
		}

		if (token && emeralds < 127) // Got a token, and missing at least one emerald
		{
			if (gamemapinfo.mapNumber < SSTAGE_START || gamemapinfo.mapNumber > SSTAGE_END)
				returnspecstagemapl = gamemapinfo.next; // Save the 'next' regular stage to go to

			token--;

			for (i = 0; i < 7; i++)
			{
				if (!(emeralds & (1<<i)))
				{
					nextmapl = G_LumpNumForMapNum(SSTAGE_START + i); // Going to the special stage
					break;
				}
			}
		}
		else if (gamemapinfo.mapNumber < SSTAGE_START || gamemapinfo.mapNumber > SSTAGE_END)
			nextmapl = gamemapinfo.next;
		else
			nextmapl = returnspecstagemapl;

		finale = nextmapl == 0;

		if (gameaction == ga_backtotitle)
		{
			finale = false;
			nextmapl = 0;
			break;
		}

#ifdef JAGUAR
		if (finale)
			nextmap = gamemapinfo.mapnumber; /* don't add secret level to eeprom */
		else
			nextmap = G_LumpNumForMapNum(nextmapl);

		if (nextmap > maxlevel)
		{	/* allow higher menu selection now */
			void WriteEEProm(void);
			maxlevel = nextmap;
			WriteEEProm();
		}
#endif

#ifdef MARS
		if (netgame == gt_deathmatch)
		{
			if (finale)
			{
				/* go back to start map */
				finale = 0;
				nextmapl = G_LumpNumForMapNum(1);
			}
		}
		else
		{
			if (!finale)
			{
				/* quick save */
				int nextmap = G_MapNumForLumpNum(nextmapl);
				QuickSave(nextmap);
			}
		}
#endif

	/* run a stats intermission */
	if (gameaction == ga_specialstageexit)
		MiniLoop (IN_Start, IN_Stop, IN_Ticker, IN_Drawer, UpdateBuffer);
	
	/* run the finale if needed */
		if (finale)
		{
#ifdef MARS
			MiniLoop(F_Start, F_Stop, F_Ticker, F_Drawer, I_Update);
#else
			MiniLoop(F_Start, F_Stop, F_Ticker, F_Drawer, UpdateBuffer);
#endif
			I_NetStop();
			break;
		}

		gamemaplump = nextmapl;
	}
}


int G_PlayInputDemoPtr (unsigned char *demo)
{
	int		exit;
	int		map;

	demobuffer = demo;

	map = demo[7];
	
	demo_p = demo + 8;
	
	G_InitNew (map, gt_single, false);
	demoplayback = true;
	exit = MiniLoop (P_Start, P_Stop, P_Ticker, P_Drawer, P_Update);
	demoplayback = false;
	
	return exit;
}

#ifdef PLAY_POS_DEMO
int G_PlayPositionDemoPtr (unsigned char *demo)
{
	int		exit;
	int		map;

	demobuffer = demo;

	map = demo[9];

	demo_p = demo + 0xA;

	G_InitNew (map, gt_single, false);
	demoplayback = true;
	exit = MiniLoop (P_Start, P_Stop, P_Ticker, P_Drawer, P_Update);
	demoplayback = false;

	return exit;
}
#endif

/*
=================
=
= G_RecordDemo
=
=================
*/

#ifdef REC_INPUT_DEMO
void G_RecordInputDemo (void)
{
	demo_p = demobuffer = Z_Malloc (0x5000, PU_STATIC);
	
	((long *)demo_p)[0] = 0; // startskill
	((long *)demo_p)[1] = startmap;

	demo_p += 8;
	
	G_InitNew (startmap, gt_single, false);
	demorecording = true;
	MiniLoop (P_Start, P_Stop, P_Ticker, P_Drawer, P_Update);
	demorecording = false;

#ifdef MARS
	I_Error("%d %p", demo_p - demobuffer, demobuffer);
#endif

	D_printf ("w %x,%x",demobuffer,demo_p);
	
	while (1)
	{
		G_PlayInputDemoPtr (demobuffer);
	D_printf ("w %x,%x",demobuffer,demo_p);
	}
}
#endif

#ifdef REC_POS_DEMO
void G_RecordPositionDemo (void)
{
	demo_p = demobuffer = Z_Malloc (0x5000, PU_STATIC);
	
	*demo_p++ = 'P';
	*demo_p++ = 'D';
	*demo_p++ = 'M';
	*demo_p++ = 'O';
	*demo_p++ = 0x00;		// short length
	*demo_p++ = 0x0A;		// ...
	*demo_p++ = 0xFF;		// short frames
	*demo_p++ = 0xFF;		// ...
	*demo_p++ = 0;			// char unused
	*demo_p++ = startmap;	// char map
	
	G_InitNew (startmap, gt_single, false);
	demorecording = true;
	MiniLoop (P_Start, P_Stop, P_Ticker, P_Drawer, P_Update);
	demorecording = false;

#ifdef MARS
	I_Error("%d %p", demo_p - demobuffer, demobuffer);
#endif

	D_printf ("w %x,%x",demobuffer,demo_p);
	
	while (1)
	{
		G_PlayPositionDemoPtr (demobuffer);
	D_printf ("w %x,%x",demobuffer,demo_p);
	}
	
}
#endif
