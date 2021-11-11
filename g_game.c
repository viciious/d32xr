/* G_game.c  */
 
#include "doomdef.h" 
#include "p_local.h" 

void G_PlayerReborn (int player); 
 
void G_DoReborn (int playernum); 
 
void G_DoLoadLevel (void); 
 
 
gameaction_t    gameaction; 
skill_t         gameskill; 
int				gamemaplump;
dmapinfo_t		gamemapinfo;
dgameinfo_t		gameinfo;

gametype_t		netgame;

boolean         playeringame[MAXPLAYERS];
player_t        players[MAXPLAYERS];
playerresp_t	playersresp[MAXPLAYERS];

int             consoleplayer;          /* player taking events and displaying  */
int             displayplayer;          /* view being displayed  */
int             gametic;
int             prevgametic;
int             totalkills, totalitems, totalsecret;    /* for intermission  */
 
boolean         demorecording; 
boolean         demoplayback; 
   
 
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

void G_DoLoadLevel (void) 
{ 
	int             i; 
	int		skytexturel;
	int 		gamemap;
	int			music;

	for (i=0 ; i<MAXPLAYERS ; i++) 
	{ 
		if (playeringame[i] && players[i].playerstate == PST_DEAD) 
			players[i].playerstate = PST_REBORN; 
		players[i].frags = 0;
	} 

	totalkills = totalitems = totalsecret = 0;
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		players[i].killcount = players[i].secretcount
			= players[i].itemcount = 0;
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
		gamemap = G_MapNumForMapName(mapname);
		if (gamemap == 0)
			gamemap = gamemapinfo.mapNumber + 1;

		gamemapinfo.sky = NULL;
		gamemapinfo.mapNumber = gamemap;
		gamemapinfo.lumpNum = gamemaplump;
		gamemapinfo.baronSpecial = (gamemap == 8);
		gamemapinfo.secretNext = G_LumpNumForMapNum(24);

		/* decide which level to go to next */
#ifdef MARS
		switch (gamemap)
		{
			case 15: nextmap = 23; break;
			case 23: nextmap = 0; break;
			case 24: nextmap = 4; break;
			default: nextmap = gamemap + 1; break;
		}
#else
		switch (gamemap)
		{
			case 23: nextmap = 0; break;
			case 24: nextmap = 4; break;
			default: nextmap = mapnumber + 1; break;
		}
#endif
		if (nextmap)
			gamemapinfo.next = G_LumpNumForMapNum(nextmap);
		else
			gamemapinfo.next = 0;
	}

	/* DMAPINFO can override the map number */
	gamemap = gamemapinfo.mapNumber;

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

	P_SetupLevel (gamemaplump, gameskill);   
	displayplayer = consoleplayer;		/* view the guy you are playing     */
	gameaction = ga_nothing; 

	music = S_SongForLump(gamemapinfo.musicLump);
	if (!music)
		music = S_SongForMapnum(gamemapinfo.mapNumber);
	S_StartSong(music, 1, gamemap);

	Z_CheckHeap (mainzone);  		/* DEBUG */
} 
 
 
 
/* 
============================================================================== 
 
						PLAYER STRUCTURE FUNCTIONS 
 
also see P_SpawnPlayer in P_Mobj 
============================================================================== 
*/ 
 
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
	D_memset (p->cards, 0, sizeof (p->cards)); 
	p->mo->flags &= ~MF_SHADOW;             /* cancel invisibility  */
	p->extralight = 0;                      /* cancel gun flashes  */
	p->damagecount = 0;                     /* no palette changes  */
	p->bonuscount = 0; 

	if (netgame == gt_deathmatch)
		return;
	if (gameaction == ga_died)
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
	int             frags; 
	
	p = &players[player]; 
	frags = p->frags;
	D_memset (p, 0, sizeof(*p)); 
	p->frags = frags;
	p->usedown = p->attackdown = true;		/* don't do anything immediately */
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
	int                     an;

	x = mthing->x << FRACBITS; 
	y = mthing->y << FRACBITS; 
	 
	// killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
	// corpse to detect collisions with other players in DM starts
	players[playernum].mo->flags |= MF_SOLID;
	an = P_CheckPosition(players[playernum].mo, x, y);
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

	D_memset(playersresp, 0, sizeof(playersresp));
	for (i = 0; i < MAXPLAYERS; i++)
	{
		int j;
		playerresp_t* resp;

		resp = &playersresp[i];
		resp->weapon = wp_pistol;
		resp->health = MAXHEALTH;
		resp->ammo[am_clip] = 50;
		resp->weaponowned[wp_fist] = true;
		resp->weaponowned[wp_pistol] = true;
		for (j = 0; j < NUMAMMO; j++)
			resp->maxammo[j] = maxammo[j];
	}
}

void G_Init(void)
{
	G_InitPlayerResp();

	if (G_FindGameinfo(&gameinfo))
		return;

	gameinfo.borderFlat = W_CheckNumForName("ROCKS");
	gameinfo.titlePage = W_CheckNumForName("title");
	gameinfo.titleTime = 540;
}

/* 
==================== 
= 
= G_InitNew 
= 
==================== 
*/ 
 
extern mobj_t emptymobj;
 
void G_InitNew (skill_t skill, int map, gametype_t gametype) 
{ 
	int             i; 

//D_printf ("G_InitNew\n");

	M_ClearRandom (); 

	if (gamemapinfo.data)
		Z_Free(gamemapinfo.data);
	D_memset(&gamemapinfo, 0, sizeof(gamemapinfo));

	/* these may be reset by I_NetSetup */
	gamemaplump = G_LumpNumForMapNum(map);
	gameskill = skill;
	if (gamemaplump < 0)
	{
		dmapinfo_t** maplist;
		VINT		mapcount;

		/* find the map by its number */
		maplist = G_LoadMaplist(&mapcount);
		for (i = 0; i < mapcount; i++)
		{
			if (maplist[i]->mapNumber > map)
				break;
			if (maplist[i]->mapNumber == map)
			{
				gamemaplump = maplist[i]->lumpNum;
				break;
			}
		}

		if (maplist)
		{
			for (i = 0; i < mapcount; i++)
				Z_Free(maplist[i]);
			Z_Free(maplist);
		}
	}

	if (gamemaplump < 0)
		I_Error("Lump MAP%02d not found!", map);

 	netgame = gametype;
	I_DrawSbar ();			/* draw frag boxes if multiplayer */

/* force players to be initialized upon first level load          */
	for (i=0 ; i<MAXPLAYERS ; i++) 
		players[i].playerstate = PST_REBORN; 

	players[0].mo = players[1].mo = &emptymobj;	/* for net consistancy checks */

	G_InitPlayerResp();

	playeringame[0] = true;	
	if (netgame != gt_single)
		playeringame[1] = true;	
	else
		playeringame[1] = false;	

	demorecording = false;
	demoplayback = false;

	gametic = 0; 
} 

void G_LoadGame(int saveslot)
{
	playerresp_t backup[MAXPLAYERS];

	ReadGame(saveslot);

	D_memcpy(backup, playersresp, sizeof(playersresp));

	G_InitNew(startskill, startmap, starttype);

	D_memcpy(playersresp, backup, sizeof(playersresp));
}

/*============================================================================  */
 
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
		int 		nextmapl;
		boolean		finale;
#ifdef JAGUAR
		int			nextmap;
#endif

		/* load a level */
		G_DoLoadLevel();

		/* run a level until death or completion */
		MiniLoop(P_Start, P_Stop, P_Ticker, P_Drawer);

		if (gameaction == ga_startnew)
		{
			if (startsave != -1)
				G_LoadGame(startsave);
			else
				G_InitNew(startskill, startmap, starttype);
			continue;
		}

		/* take away cards and stuff */

		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i])
				G_PlayerFinishLevel(i);

		if (gameaction == ga_died)
			continue;			/* died, so restart the level */

		if (gameaction == ga_warped)
			continue;			/* skip intermission */

		if (gameaction == ga_secretexit && gamemapinfo.secretNext)
			nextmapl = gamemapinfo.secretNext;
		else
			nextmapl = gamemapinfo.next;
		finale = nextmapl == 0;

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
		if (!finale)
		{
			/* quick save */
			int nextmap = G_MapNumForLumpNum(nextmapl);
			QuickSave(nextmap);
		}
#endif

	/* run a stats intermission */
		MiniLoop (IN_Start, IN_Stop, IN_Ticker, IN_Drawer);
	
	/* run the finale if needed */
		if (finale)
			MiniLoop (F_Start, F_Stop, F_Ticker, F_Drawer);

		gamemaplump = nextmapl;
	}
}


int G_PlayDemoPtr (int *demo)
{
	int		exit;
	int		skill, map;

	demobuffer = demo;
	
	skill = *demo++;
	map = *demo++;

	demo_p = demo;
	
	G_InitNew (skill, map, gt_single);
	G_DoLoadLevel ();   
	demoplayback = true;
	exit = MiniLoop (P_Start, P_Stop, P_Ticker, P_Drawer);
	demoplayback = false;
	
	return exit;
}

/*
=================
=
= G_RecordDemo
=
=================
*/

void G_RecordDemo (void)
{
	demo_p = demobuffer = Z_Malloc (0x8000, PU_STATIC, NULL);
	
	*demo_p++ = startskill;
	*demo_p++ = startmap;
	
	G_InitNew (startskill, startmap, gt_single);
	G_DoLoadLevel ();  
	demorecording = true; 
	MiniLoop (P_Start, P_Stop, P_Ticker, P_Drawer);
	demorecording = false;

#ifdef MARS
	I_Error("%d %p", demo_p - demobuffer, demobuffer);
#endif

	D_printf ("w %x,%x",demobuffer,demo_p);
	
	while (1)
	{
		G_PlayDemoPtr (demobuffer);
	D_printf ("w %x,%x",demobuffer,demo_p);
	}
	
}

