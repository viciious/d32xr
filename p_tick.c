#include "doomdef.h"
#include "p_local.h"

int	playertics, thinkertics, sighttics, basetics, latetics;
int	tictics;

boolean		gamepaused;
jagobj_t	*pausepic;
char		clearscreen = 0;

/*
===============================================================================

								THINKERS

All thinkers should be allocated by Z_Malloc so they can be operated on uniformly.  The actual
structures will vary in size, but the first element must be thinker_t.

Mobjs are similar to thinkers, but kept seperate for more optimal list
processing
===============================================================================
*/

thinker_t	thinkercap;	/* both the head and tail of the thinker list */
degenmobj_t		mobjhead;	/* head and tail of mobj list */
degenmobj_t		freemobjhead;	/* head and tail of free mobj list */
degenmobj_t		limbomobjhead;

//int			activethinkers;	/* debug count */
//int			activemobjs;	/* debug count */

/*
===============
=
= P_InitThinkers
=
===============
*/

void P_InitThinkers (void)
{
	thinkercap.prev = thinkercap.next  = &thinkercap;
	mobjhead.next = mobjhead.prev = (void *)&mobjhead;
	freemobjhead.next = freemobjhead.prev = (void *)&freemobjhead;
	limbomobjhead.next = limbomobjhead.prev = (void*)&limbomobjhead;
}


/*
===============
=
= P_AddThinker
=
= Adds a new thinker at the end of the list
=
===============
*/

void P_AddThinker (thinker_t *thinker)
{
	thinkercap.prev->next = thinker;
	thinker->next = &thinkercap;
	thinker->prev = thinkercap.prev;
	thinkercap.prev = thinker;
}


/*
===============
=
= P_RemoveThinker
=
= Deallocation is lazy -- it will not actually be freed until its
= thinking turn comes up
=
===============
*/

void P_RemoveThinker (thinker_t *thinker)
{
	thinker->function = (think_t)-1;
}

/*
===============
=
= P_RunThinkers
=
===============
*/

void P_RunThinkers (void)
{
	thinker_t	*currentthinker;
	
	//activethinkers = 0;
	
	currentthinker = thinkercap.next;
	while (currentthinker != &thinkercap)
	{
		if (currentthinker->function == (think_t)-1)
		{	/* time to remove it */
			currentthinker->next->prev = currentthinker->prev;
			currentthinker->prev->next = currentthinker->next;
			Z_Free (currentthinker);
		}
		else
		{
			if (currentthinker->function)
			{
				currentthinker->function (currentthinker);
			}
			//activethinkers++;
		}
		currentthinker = currentthinker->next;
	}
}


/*============================================================================= */

/*
===============
=
= P_CheckSights
=
= Check sights of all mobj thinkers that are going to change state this
= tic and have MF_COUNTKILL set
===============
*/

void P_CheckSights2 ();

void P_CheckSights (void)
{
#ifdef JAGUAR
	extern	int p_sight_start;
	DSPFunction (&p_sight_start);
#else
	P_CheckSights2 ();
#endif
}

/*============================================================================= */

/* 
=================== 
= 
= P_RunMobjBase  
=
= Run stuff that doesn't happen every tick
=================== 
*/ 

void	P_RunMobjBase (void)
{
#ifdef JAGUAR
	extern	int p_base_start;
	 
	DSPFunction (&p_base_start);
#else
	P_RunMobjBase2 ();
#endif
}

/*============================================================================= */

/*
==============
=
= P_CheckCheats
=
==============
*/
 
void P_CheckCheats (void)
{
#ifdef JAGUAR
	int		buttons, oldbuttons;
	int 	warpmap;
	int		i;
	player_t	*p;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		if (!playeringame[i])
			continue;
		buttons = ticbuttons[i];
		oldbuttons = oldticbuttons[i];
	
		if ( (buttons & BT_PAUSE) && !(oldbuttons&BT_PAUSE) )
			gamepaused ^= 1;
	}

	if (netgame)
		return;

	buttons = ticbuttons[0];
	oldbuttons = oldticbuttons[0];

	if ( (oldbuttons&BT_PAUSE) || !(buttons & BT_PAUSE ) )
		return;

	if (buttons&JP_NUM)
	{	/* free stuff */
		p=&players[0];
		for (i=0 ; i<NUMCARDS ; i++)
			p->cards[i] = true;			
		p->armorpoints = 200;
		p->armortype = 2;
		for (i=0;i<NUMWEAPONS;i++) p->weaponowned[i] = true;
		for (i=0;i<NUMAMMO;i++) p->ammo[i] = p->maxammo[i] = 500;
	}

	if (buttons&JP_STAR)
	{	/* godmode */
		players[0].cheats ^= CF_GODMODE;
	}
	warpmap = 0;
	if (buttons&JP_1) warpmap = 1;
	if (buttons&JP_2) warpmap = 2;
	if (buttons&JP_3) warpmap = 3;
	if (buttons&JP_4) warpmap = 4;
	if (buttons&JP_5) warpmap = 5;
	if (buttons&JP_6) warpmap = 6;
	if (buttons&JP_7) warpmap = 7;
	if (buttons&JP_8) warpmap = 8;
	if (buttons&JP_9) warpmap = 9;
	if (buttons&JP_A) warpmap += 10;
	else if (buttons&JP_B) warpmap += 20;
	
	if (warpmap>0 && warpmap < 27)
	{
		gamemaplump = G_LumpNumForMapNum(warpmap);
		gameaction = ga_warped;
	}
#elif defined(MARS)
	int i;
	int buttons;
	int oldbuttons;
	const int stuff_combo = BT_A | BT_B | BT_C | BT_UP;
	const int godmode_combo = BT_X | BT_Y | BT_Z | BT_UP;
	player_t* p;

	if (netgame)
		return;
	if (!gamepaused)
		return;

	buttons = ticrealbuttons;
	oldbuttons = oldticrealbuttons;

	if ((buttons & stuff_combo) == stuff_combo 
		&& (oldbuttons & stuff_combo) != stuff_combo)
	{	/* free stuff */
		p = &players[0];
		for (i = 0; i < NUMCARDS; i++)
			p->cards[i] = true;
		p->armorpoints = 200;
		p->armortype = 2;
		for (i = 0; i < NUMWEAPONS; i++) p->weaponowned[i] = true;
		for (i = 0; i < NUMAMMO; i++) p->ammo[i] = p->maxammo[i] = 500;
	}

	if ((buttons & godmode_combo) == godmode_combo 
		&& (oldbuttons & godmode_combo) != godmode_combo)
	{	/* godmode */
		players[0].cheats ^= CF_GODMODE;
	}
#endif
}
  

int playernum;

void G_DoReborn (int playernum); 

/*
=================
=
= P_Ticker
=
=================
*/

int		ticphase;

#ifdef MARS
#define frtc I_GetFRTCounter()
#else
#define frtc samplecount
#endif

int P_Ticker (void)
{
	int		start;
	int		ticstart;
	player_t	*pl;
	
	if (demoplayback)
	{
		if (M_Ticker())
			return ga_exitdemo;
	}


	while (!I_RefreshLatched () )
	;		/* wait for refresh to latch all needed data before */
			/* running the next tick */

#ifdef JAGAUR
	while (DSPRead (&dspfinished) != 0xdef6)
	;		/* wait for sound mixing to complete */
#endif

	gameaction = ga_nothing;

/* */
/* check for pause and cheats */
/* */
	P_CheckCheats ();
	
/* */
/* do option screen processing */
/* */

	for (playernum=0,pl=players ; playernum<MAXPLAYERS ; playernum++,pl++)
		if (playeringame[playernum])
			O_Control (pl);

	/* */
	/* run player actions */
	/* */
	if (gamepaused)
		return 0;

	start = frtc;
	for (playernum = 0, pl = players; playernum < MAXPLAYERS; playernum++, pl++)
		if (playeringame[playernum])
		{
			if (pl->playerstate == PST_REBORN)
				G_DoReborn(playernum);
			AM_Control(pl);
			P_PlayerThink(pl);
		}
	playertics = frtc - start;

#ifdef THINKERS_30HZ
	start = frtc;
	P_RunThinkers();
	thinkertics = frtc - start;
#endif

	if (gametic != prevgametic)
	{
		ticstart = frtc;

#ifndef THINKERS_30HZ
		start = frtc;
		P_RunThinkers();
		thinkertics = frtc - start;
#endif

		start = frtc;
		P_CheckSights();
		sighttics = frtc - start;

		start = frtc;
		P_RunMobjBase();
		basetics = frtc - start;

		start = frtc;
		P_RunMobjLate();
		latetics = frtc - start;

		P_UpdateSpecials();

		P_RespawnSpecials();

		ST_Ticker();			/* update status bar */

		tictics = frtc - ticstart;
	}

	return gameaction;		/* may have been set to ga_died, ga_completed, */
							/* or ga_secretexit */
}


/* 
============= 
= 
= DrawPlaque 
= 
============= 
*/ 
 
void DrawPlaque (jagobj_t *pl)
{
#ifndef MARS
	int			x,y,w;
	short		*sdest;
	byte		*bdest, *source;
	extern		int isrvmode;
	
	while ( !I_RefreshCompleted () )
	;

	bufferpage = (byte *)screens[!workpage];
	source = pl->data;
	
	if (isrvmode == 0xc1 + (3<<9) )
	{	/* 320 mode, stretch pixels */
		bdest = (byte *)bufferpage + 80*320 + (160 - pl->width);
		w = pl->width;
		for (y=0 ; y<pl->height ; y++)
		{
			for (x=0 ; x<w ; x++)
			{
				bdest[x*2] = bdest[x*2+1] = *source++;
			}
			bdest += 320;
		}
	}
	else
	{	/* 160 mode, draw directly */
		sdest = (short *)bufferpage + 80*160 + (80 - pl->width/2);
		w = pl->width;
		for (y=0 ; y<pl->height ; y++)
		{
			for (x=0 ; x<w ; x++)
			{
				sdest[x] = palette8[*source++];
			}
			sdest += 160;
		}
	}
#else
	clearscreen = 2;
#endif
}

/* 
============= 
= 
= DrawPlaque 
= 
============= 
*/ 
#ifndef MARS
void DrawSinglePlaque (jagobj_t *pl)
{
	int			x,y,w;
	byte		*bdest, *source;
	
	while ( !I_RefreshCompleted () )
	;

	bufferpage = (byte *)screens[!workpage];
	source = pl->data;
	
	bdest = (byte *)bufferpage + 80*320 + (160 - pl->width/2);
	w = pl->width;
	for (y=0 ; y<pl->height ; y++)
	{
		for (x=0 ; x<w ; x++)
			bdest[x] = *source++;
		bdest += 320;
	}
}
#endif


/* 
============= 
= 
= P_Drawer 
= 
= draw current display 
============= 
*/ 
 
 
void P_Drawer (void) 
{ 	
	boolean automapactive = (players[consoleplayer].automapflags & AF_ACTIVE) != 0;
	boolean optionsactive = (players[consoleplayer].automapflags & AF_OPTIONSACTIVE) != 0;

#ifndef MARS
	if (optionsactive)
	{
		O_Drawer ();
	}
	else if (gamepaused && refreshdrawn)
		DrawPlaque (pausepic);
	else if (automapactive)
	{
		ST_Drawer ();
		AM_Drawer ();
		I_Update ();
		clearscreen = 0;
	}
	else
#else
	if (automapactive)
	{
		R_RenderPlayerView();
		ST_Drawer();
		AM_Drawer();
		if (optionsactive)
			O_Drawer();
		I_Update();
		clearscreen = 2;
	}
	else
#endif
	{
#ifdef MARS
		static boolean o_wasactive = false;

		if (!optionsactive && o_wasactive)
			clearscreen = 2;

		if (clearscreen > 0) {
			I_ResetLineTable();
		}

		if (clearscreen > 0) {
			if (viewportWidth == 160)
				I_ClearFrameBuffer();
			else
				DrawTiledBackground();

			I_DrawSbar();

			if (clearscreen == 2 || optionsactive)
			{
				ST_ForceDraw();
			}
		}

		if (clearscreen > 0) {
			clearscreen--;
		}

		R_RenderPlayerView();

		ST_Drawer();

		if (demoplayback)
			M_Drawer();

		if (optionsactive)
		{
			O_Drawer();
		}

		o_wasactive = optionsactive;

		I_Update ();
#else
#ifdef JAGUAR
		ST_Drawer ();
#endif
		R_RenderPlayerView (); 
		clearscreen = 0;
#endif
	/* assume part of the refresh is now running parallel with main code */
	}
} 
 
 
extern	 int		ticremainder[2];

void P_Start (void)
{
	AM_Start ();
#ifndef MARS
	S_RestartSounds ();
#endif
	players[0].automapflags = 0;
	players[1].automapflags = 0;
	ticremainder[0] = ticremainder[1] = 0;
	M_ClearRandom ();

	clearscreen = 2;
}

void P_Stop (void)
{
	Z_FreeTags (mainzone);
}

