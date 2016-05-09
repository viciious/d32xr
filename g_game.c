/* G_game.c  */
 
#include "doomdef.h" 
#include "p_local.h" 
 
void G_PlayerReborn (int player); 
 
void G_DoReborn (int playernum); 
 
void G_DoLoadLevel (void); 
 
 
gameaction_t    gameaction; 
skill_t         gameskill; 
int             gamemap; 
int				nextmap;				/* the map to go to after the stats */

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
extern texture_t		textures[];

void G_DoLoadLevel (void) 
{ 
	int             i; 
	 
	for (i=0 ; i<MAXPLAYERS ; i++) 
	{ 
		if (playeringame[i] && players[i].playerstate == PST_DEAD) 
			players[i].playerstate = PST_REBORN; 
		players[i].frags = 0;
	} 

/*  */
/* set the sky map for the episode  */
/*  */
	if (gamemap < 9) 
		skytexture = R_TextureNumForName ("SKY1"); 
	else if (gamemap < 18)
		skytexture = R_TextureNumForName ("SKY2"); 
	else
		skytexture = R_TextureNumForName ("SKY3"); 

 	skytexturep = &textures[skytexture];
		 
	P_SetupLevel (gamemap, gameskill);   
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
	mo = P_SpawnMobj (x+20*finecosine[an], y+20*finesine[an], ss->sector->floorheight 
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

/* these may be reset by I_NetSetup */
	gamemap = map; 
	gameskill = skill; 
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

	if ( skill == sk_nightmare )
	{ 
		states[S_SARG_ATK1].tics = 2;
		states[S_SARG_ATK2].tics = 2;
		states[S_SARG_ATK3].tics = 2;
		mobjinfo[MT_SERGEANT].speed = 15; 
		mobjinfo[MT_SHADOWS].speed = 15; 
		
		mobjinfo[MT_BRUISERSHOT].speed = 40*FRACUNIT; 
		mobjinfo[MT_HEADSHOT].speed = 40*FRACUNIT; 
		mobjinfo[MT_TROOPSHOT].speed = 40*FRACUNIT; 
	} 
	else 
	{ 
		states[S_SARG_ATK1].tics = 4;
		states[S_SARG_ATK2].tics = 4;
		states[S_SARG_ATK3].tics = 4;
		mobjinfo[MT_SERGEANT].speed = 10; 
		mobjinfo[MT_SHADOWS].speed = 10; 
		
		mobjinfo[MT_BRUISERSHOT].speed = 30*FRACUNIT; 
		mobjinfo[MT_HEADSHOT].speed = 20*FRACUNIT; 
		mobjinfo[MT_TROOPSHOT].speed = 20*FRACUNIT; 
	}   
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
					
	/* decide which level to go to next */
#ifdef MARS
		if (gameaction == ga_secretexit)
			nextmap = 24;
		else
		{
			switch (gamemap)
			{
			case 15: nextmap = 23; break;
			case 24: nextmap = 4; break;
			default: nextmap = gamemap+1; break;
			}
		}
#else
		if (gameaction == ga_secretexit)
		{
			 nextmap = 24;
		}
		else
		{
			switch (gamemap)
			{
			case 24: nextmap = 4; break;
			case 23: nextmap = 23; break;	/* don't add secret level to eeprom */
			default: nextmap = gamemap+1; break;
			}
#ifdef JAGUAR
			if (nextmap > maxlevel)
			{	/* allow higher menu selection now */
				void WriteEEProm (void);
				maxlevel = nextmap;
				WriteEEProm ();
			}
#endif

		}
#endif

	/* run a stats intermission */
		MiniLoop (IN_Start, IN_Stop, IN_Ticker, IN_Drawer);
	
	/* run the finale if needed */
		if (gamemap == 23)
			MiniLoop (F_Start, F_Stop, F_Ticker, F_Drawer);
			
		gamemap = nextmap;
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

