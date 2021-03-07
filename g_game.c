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

gametype_t		netgame;

boolean         playeringame[MAXPLAYERS]; 
player_t        players[MAXPLAYERS]; 
 
int             consoleplayer;          /* player taking events and displaying  */
int             displayplayer;          /* view being displayed  */
int             gametic; 
int             totalkills, totalitems, totalsecret;    /* for intermission  */
 
char            demoname[32]; 
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

	if (G_FindMapinfo(gamemaplump, &gamemapinfo) == 0) {
		int nextmap;
		const char *mapname;

		mapname = G_GetMapNameForLump(gamemaplump);
		gamemap = G_MapNumForMapName(mapname);
		if (gamemap == 0)
			gamemap = gamemapinfo.mapnumber + 1;

		gamemapinfo.sky = NULL;
		gamemapinfo.mapnumber = gamemap;
		gamemapinfo.lumpnum = gamemaplump;
		gamemapinfo.baronspecial = (gamemap == 8);
		gamemapinfo.secretnext = G_LumpNumForMapNum(24);

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
	gamemap = gamemapinfo.mapnumber;

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

    /* S_StartSong(1, 0); */  /* Added CEF */

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
	p->fixedcolormap = 0;                   /* cancel ir gogles  */
	p->damagecount = 0;                     /* no palette changes  */
	p->bonuscount = 0; 
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
	int                     i; 
	int             frags; 
	 
	
	p = &players[player]; 
	frags = p->frags;
	D_memset (p, 0, sizeof(*p)); 
	p->frags = frags;	
	p->usedown = p->attackdown = true;		/* don't do anything immediately */
	p->playerstate = PST_LIVE;       
	p->health = MAXHEALTH; 
	p->readyweapon = p->pendingweapon = wp_pistol; 
	p->weaponowned[wp_fist] = true; 
	p->weaponowned[wp_pistol] = true; 
	p->ammo[am_clip] = 50; 
	 
	for (i=0 ; i<NUMAMMO ; i++) 
		p->maxammo[i] = maxammo[i]; 
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
	subsector_t *ss; 
	int                     an; 
	mobj_t		*mo;
	
	x = mthing->x << FRACBITS; 
	y = mthing->y << FRACBITS; 
	 
	if (!P_CheckPosition (players[playernum].mo, x, y) ) 
		return false; 
 
	ss = R_PointInSubsector (x,y); 
	an = ( ANG45 * (mthing->angle/45) ) >> ANGLETOFINESHIFT; 
 
/* spawn a teleport fog  */
	mo = P_SpawnMobj (x+20*finecosine(an), y+20*finesine(an), ss->sector->floorheight 
	, MT_TFOG); 
	S_StartSound (mo, sfx_telept);
	
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
	players[playernum].mo->player = NULL;   /* dissasociate the corpse  */
		
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

D_printf ("G_InitNew\n");

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
		int				mapcount;

		/* find the map by its number */
		maplist = G_LoadMaplist(&mapcount);
		for (i = 0; i < mapcount; i++)
		{
			if (maplist[i]->mapnumber > map)
				break;
			if (maplist[i]->mapnumber == map)
			{
				gamemaplump = maplist[i]->lumpnum;
				break;
			}
		}

		for (i = 0; i < mapcount; i++)
			Z_Free(maplist[i]);
		Z_Free(maplist);
	}

	if (gamemaplump < 0)
		I_Error("Lump MAP%02d not found!", map);

 	netgame = gametype;
	I_DrawSbar ();			/* draw frag boxes if multiplayer */

/* force players to be initialized upon first level load          */
	for (i=0 ; i<MAXPLAYERS ; i++) 
		players[i].playerstate = PST_REBORN; 

	players[0].mo = players[1].mo = &emptymobj;	/* for net consistancy checks */
 	
	playeringame[0] = true;	
	if (netgame != gt_single)
		playeringame[1] = true;	
	else
		playeringame[1] = false;	

	demorecording = false;
	demoplayback = false;
	

	gametic = 0; 
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
		G_DoLoadLevel ();   
	
	/* run a level until death or completion */
		MiniLoop (P_Start, P_Stop, P_Ticker, P_Drawer);
	
	/* take away cards and stuff */
			
		for (i=0 ; i<MAXPLAYERS ; i++) 
			if (playeringame[i]) 
				G_PlayerFinishLevel (i);	 

		if (gameaction == ga_died)
			continue;			/* died, so restart the level */
	
		if (gameaction == ga_warped)
			continue;			/* skip intermission */
		
		if (gameaction == ga_secretexit && gamemapinfo.secretnext)
			nextmapl = gamemapinfo.secretnext;
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
			void WriteEEProm (void);
			maxlevel = nextmap;
			WriteEEProm ();
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
	
	D_printf ("w %x,%x",demobuffer,demo_p);
	
	while (1)
	{
		G_PlayDemoPtr (demobuffer);
	D_printf ("w %x,%x",demobuffer,demo_p);
	}
	
}

