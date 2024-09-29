/* G_game.c  */
 
#include "doomdef.h" 
#include "p_local.h" 

void G_PlayerReborn (int player); 
 
void G_DoReborn (int playernum); 
 
void G_DoLoadLevel (void); 
 
 
gameaction_t    gameaction; 
skill_t         gameskill; 
char			*gamemaplump;
dmapinfo_t		gamemapinfo;
dgameinfo_t		gameinfo;

dmapinfo_t		**gamemaplist = NULL;
VINT 			gamemapcount = 0;

gametype_t		netgame;

boolean         playeringame[MAXPLAYERS];
player_t        players[MAXPLAYERS];
playerresp_t	playersresp[MAXPLAYERS];

int             consoleplayer = 0;          /* player taking events and displaying  */
int             gametic;
int             prevgametic;
int             totalkills, totalitems, totalsecret;    /* for intermission  */
 
boolean         demorecording; 
boolean         demoplayback; 

mobj_t*         bodyque[BODYQUESIZE];
int             bodyqueslot;

boolean			finale, secretexit;

/* 
============== 
= 
= G_DoLoadLevel 
= 
============== 
*/ 
  
dmapinfo_t *G_MapInfoForLumpName(const char *lumpName)
{
	int i;

	/* find the map by its lump */
	for (i = 0; i < gamemapcount; i++)
	{
		if (!D_strcasecmp(gamemaplist[i]->lumpName, lumpName))
		{
			return gamemaplist[i];
		}
	}
	return NULL;
}

char *G_LumpNameForMapNum(int map)
{
	int i;

 	/* find the map by its number */
	for (i = 0; i < gamemapcount; i++)
	{
		if (gamemaplist[i]->mapNumber > map)
			break;
		if (gamemaplist[i]->mapNumber == map)
			return gamemaplist[i]->lumpName;
	}
	return "";
}

char *G_MapNameForMapNum(int map)
{
	int i;

 	/* find the map by its number */
	for (i = 0; i < gamemapcount; i++)
	{
		if (gamemaplist[i]->mapNumber > map)
			break;
		if (gamemaplist[i]->mapNumber == map)
			return gamemaplist[i]->name;
	}
	return "";
}

void G_DoLoadLevel (void) 
{ 
	int             i; 
	int 		gamemap;
	int			music;
	int 		cdtrack;
	dmapinfo_t  *mi;

	S_Clear();

	for (i=0 ; i<MAXPLAYERS ; i++) 
	{ 
		if (playeringame[i]/* && players[i].playerstate == PST_DEAD*/)
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
	mi = G_MapInfoForLumpName(gamemaplump);
	if (!mi)
		I_Error("No mapinfo for lump %s", gamemaplump);

	D_memcpy(&gamemapinfo, mi, sizeof(dmapinfo_t));

	/* update the start map */
	gamemap = gamemapinfo.mapNumber;
	startmap = gamemap;

	P_SetupLevel (gamemaplump, gameskill, gamemapinfo.skyTexture);
	gameaction = ga_nothing; 

	music = gamemapinfo.songNum;
	if (music <= mus_none)
		music = S_SongForMapnum(gamemap);

	cdtrack = gamemapinfo.cdaNum ? gamemapinfo.cdaNum : gameinfo.cdTrackOffset + music;

	if (netgame != gt_single && !splitscreen)
		S_StopSong();
	S_StartSong(music, 1, cdtrack);

	//Z_CheckHeap (mainzone);  		/* DEBUG */
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
	int             frags; 
	int             killcount;
	int             itemcount;
	int             secretcount;

	p = &players[player]; 
	frags = p->frags;
	killcount = p->killcount;
	itemcount = p->itemcount;
	secretcount = p->secretcount;
	D_memset (p, 0, sizeof(*p)); 
	p->frags = frags;
	p->killcount = killcount;
	p->itemcount = itemcount;
	p->secretcount = secretcount;
	p->usedown = p->attackdown = true;		/* don't do anything immediately */
	p->playerstate = PST_LIVE;

	if (gamemapinfo.specials & MI_PISTOL_START)
		P_ResetResp(p);
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
	pmovework_t	tm;

	x = mthing->x << FRACBITS; 
	y = mthing->y << FRACBITS; 
	 
	// killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
	// corpse to detect collisions with other players in DM starts
	players[playernum].mo->flags |= MF_SOLID;
	an = P_CheckPosition(&tm, players[playernum].mo, x, y);
	players[playernum].mo->flags &= ~MF_SOLID;
	if (!an ) 
		return false; 

	// flush an old corpse if needed
	if (bodyqueslot >= BODYQUESIZE)
		P_RemoveMobj(bodyque[bodyqueslot%BODYQUESIZE]);
	bodyque[bodyqueslot%BODYQUESIZE] = players[playernum].mo;
	bodyqueslot++;

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
		P_ResetResp(&players[i]);
}

void G_Init(void)
{
	int i;
	int mapcount = 0;
	dmapinfo_t** maplist;

	// copy mapnumbers to a temp buffer, then free, then allocate againccccbbgbdcfhk
	// to avoid zone memory fragmentation
	W_LoadPWAD(PWAD_CD);

	maplist = G_LoadMaplist(&mapcount, &gameinfo);
	if (mapcount > 99)
		mapcount = 99;
	if (maplist)
	{
		gamemaplist = maplist;
		gamemapcount = mapcount;
	}
	else
	{
		char lumpname[9];

		mapcount = 0;
		for (i = 1; i < 99; i++) {
			D_snprintf(lumpname, sizeof(lumpname), "MAP%02d", i);
			if (W_CheckNumForName(lumpname) < 0)
				break;
			mapcount++;
		}

		maplist = Z_Malloc(sizeof(*maplist) * (mapcount + 1), PU_STATIC);
		for (i = 0; i < mapcount; i++) {
			int len;

			D_snprintf(lumpname, sizeof(lumpname), "MAP%02d", i+1);
			len = mystrlen(lumpname);

			maplist[i] = Z_Malloc(sizeof(dmapinfo_t) + len + 1, PU_STATIC);
			maplist[i]->mapNumber = i + 1;
			maplist[i]->lumpName = (char*)maplist[i] + sizeof(dmapinfo_t);
			maplist[i]->name = maplist[i]->lumpName;
			maplist[i]->specials |= (maplist[i]->mapNumber == 8 ? MI_BARON_SPECIAL : 0);
			D_memcpy(maplist[i]->lumpName, lumpname, len + 1);
		}

		for (i = 0; i < mapcount; i++) {
			int nextmap = 0;
			int gamemap = maplist[i]->mapNumber;
			const char *sky;

			switch (gamemap)
			{
				case 15: nextmap = 23; break;
				case 23: nextmap = 0; break;
				case 24: nextmap = 4; break;
				default: nextmap = gamemap + 1; break;
			}

			if (gamemap < 9)
				sky = "SKY1";
			else if (gamemap < 18)
				sky = "SKY2";
			else
				sky = "SKY3";
			maplist[i]->skyTexture = W_CheckNumForName(sky);

			if (nextmap)
				maplist[i]->next = maplist[nextmap-1]->name;
			if (mapcount >= 24)
				maplist[i]->secretNext = maplist[23]->name;
		}

		gamemaplist = maplist;
		gamemapcount = mapcount;
	}

	W_LoadPWAD(PWAD_NONE);

	G_InitPlayerResp();

	// make sure SPCM dir is up to date
	for (i = 0; i < MAX_SPCM_PACKS; i++)
	{
		if (!spcmDir[0] || !gameinfo.spcmDirList[i][0]) {
			i = MAX_SPCM_PACKS;
			break;
		}
		if (!D_strcasecmp(gameinfo.spcmDirList[i], spcmDir)) {
			break;
		}
	}

	// if not found, default to the first dir
	if (i == MAX_SPCM_PACKS) {
		S_SetSPCMDir(gameinfo.spcmDirList[0]);

		// revert to FM if SPCM isn't present at all
		if (*gameinfo.spcmDirList[0] == '\0' && musictype == mustype_spcm) {
			S_SetMusicType(mustype_fm);
		}
	}

	if (!*gameinfo.borderFlat)
		gameinfo.borderFlat = "ROCKS";
	if (!*gameinfo.endFlat)
		gameinfo.endFlat = gameinfo.borderFlat;

	gameinfo.borderFlatNum = W_CheckNumForName(gameinfo.borderFlat);

	if (gameinfo.data)
		return;

	gameinfo.titlePage = "title";
	gameinfo.titleTime = 540;
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
 
void G_InitNew (skill_t skill, int map, gametype_t gametype, boolean splitscr)
{ 
	int             i; 

//D_printf ("G_InitNew\n");

	M_ClearRandom (); 

	D_memset(&gamemapinfo, 0, sizeof(gamemapinfo));

	/* these may be reset by I_NetSetup */
	gamemaplump = G_LumpNameForMapNum(map);
	gameskill = skill;

	if (gamemaplump == NULL || *gamemaplump == '\0')
		I_Error("Map %d not found!", map);

 	netgame = gametype;
	splitscreen = splitscr;
#ifndef MARS
	I_DrawSbar ();			/* draw frag boxes if multiplayer */
#endif

/* force players to be initialized upon first level load          */
	for (i=0 ; i<MAXPLAYERS ; i++) 
		players[i].playerstate = PST_REBORN;

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
} 

void G_LoadGame(int saveslot)
{
	playerresp_t backup[MAXPLAYERS];

	ReadGame(saveslot);

	D_memcpy(backup, playersresp, sizeof(playersresp));

	G_InitNew(startskill, startmap, starttype, startsplitscreen);

	D_memcpy(playersresp, backup, sizeof(playersresp));
}

/*============================================================================  */
 
/*
=================
=
= G_RunGame
=
= The game should allready have been initialized or loaded
=================
*/
void G_RunGame (void)
{
	int		i;

	while (1)
	{
		char		*nextmapl;
#ifdef JAGUAR
		int			nextmap;
#endif
		boolean 	finale_ = false;

		/* run a level until death or completion */
		MiniLoop(P_Start, P_Stop, P_Ticker, P_Drawer, P_Update);

		if (gameaction == ga_quit)
		{
			I_NetStop();
			return;
		}

		if (gameaction == ga_startnew)
		{
startnew:
			if (starttype != gt_single && !startsplitscreen)
			{
				if (consoleplayer == 0)
					I_NetSetup();
			}
			else
				I_NetStop();
			if (startsave != -1)
				G_LoadGame(startsave);
			else if (demorecording)
				G_RecordDemo();	// set startmap and startskill
			else
				G_InitNew(startskill, startmap, starttype, startsplitscreen);
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
			if (starttype != netgame || startskill != gameskill || startmap != gamemapinfo.mapNumber)
			{
				gameaction = ga_startnew;
				goto startnew;
			}
			continue;			/* skip intermission */
		}

		secretexit = gameaction == ga_secretexit && gamemapinfo.secretNext;
		if (secretexit)
			nextmapl = gamemapinfo.secretNext;
		else
			nextmapl = gamemapinfo.next;
		finale_ = nextmapl == NULL || *nextmapl == '\0';

#ifdef JAGUAR
		if (finale_)
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
			if (finale_)
			{
				/* go back to start map */
				finale_ = 0;
				nextmapl = G_LumpNameForMapNum(1);
			}
		}
		else
		{
			if (!finale_)
			{
				/* quick save */
				dmapinfo_t *mi = G_MapInfoForLumpName(nextmapl);
				if (mi)
					QuickSave(mi->mapNumber, mi->name);
			}
		}
#endif

	/* run a stats intermission */
		MiniLoop (IN_Start, IN_Stop, IN_Ticker, IN_Drawer, I_Update);

	/* run a text screen */
		finale = false;
		if (netgame != gt_deathmatch)
			if ((secretexit && *gamemapinfo.secretInterText) || *gamemapinfo.interText)
			{
#ifdef MARS
				MiniLoop(F_Start, F_Stop, F_Ticker, F_Drawer, I_Update);
#else
				MiniLoop(F_Start, F_Stop, F_Ticker, F_Drawer, UpdateBuffer);
#endif
			}

	/* run the finale if needed */
		finale = finale_;
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
		else 

		gamemaplump = nextmapl;
	}
}


int G_PlayDemoPtr (unsigned *demo)
{
	int		exit;
	int		skill, map;

	demobuffer = demo;
	
	skill = *demo++;
	map = *demo++;

	demo_p = demo;
	
	G_InitNew (skill, map, gt_single, false);
	demoplayback = true;
	exit = MiniLoop (P_Start, P_Stop, P_Ticker, P_Drawer, P_Update);
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
#ifdef MARS_USE_SRAM_DEMO
	demo_p = demobuffer = (void *)MARS_SRAM_DEMO_OFS;

	I_WriteU32SRAM((intptr_t)demo_p, startskill);
	demo_p++;

	I_WriteU32SRAM((intptr_t)demo_p, startmap);
	demo_p++;
#else
	demo_p = demobuffer = Z_Malloc (0x8000, PU_STATIC);

	*demo_p++ = startskill;
	*demo_p++ = startmap;
#endif

	G_InitNew (startskill, startmap, gt_single, false);
	demorecording = true; 
	MiniLoop (P_Start, P_Stop, P_Ticker, P_Drawer, P_Update);
	demorecording = false;

#ifdef MARS
	I_Error("%d %p", (intptr_t)demo_p - (intptr_t)demobuffer, demobuffer);
#endif

	D_printf ("w %x,%x",demobuffer,demo_p);
	
	while (1)
	{
		G_PlayDemoPtr (demobuffer);
	D_printf ("w %x,%x",demobuffer,demo_p);
	}
	
}

void G_DeInit (void)
{
	if (gamemaplist)
	{
		int i;
		for (i = 0; gamemaplist[i]; i++)
			Z_Free(gamemaplist[i]);
		Z_Free(gamemaplist);
		gamemaplist = NULL;
	}

	if (gameinfo.data)
	{
		Z_Free(gameinfo.data);
	}
	D_memset(&gameinfo, 0, sizeof(gameinfo));

	gamemapcount = 0;
}
