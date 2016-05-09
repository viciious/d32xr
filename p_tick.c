#include "doomdef.h"
#include "p_local.h"

int	playertics, thinkertics, sighttics, basetics, latetics;
int	tictics;

boolean		gamepaused;
jagobj_t	*pausepic;

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
mobj_t		mobjhead;	/* head and tail of mobj list */
int			activethinkers;	/* debug count */
int			activemobjs;	/* debug count */

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
	mobjhead.next = mobjhead.prev = &mobjhead;
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
	
	activethinkers = 0;
	
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
			activethinkers++;
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
=================== 
= 
= P_RunMobjLate  
=
= Run stuff that doesn't happen every tick
=================== 
*/ 

void	P_RunMobjLate (void)
{
	mobj_t	*mo;
	mobj_t	*next;
	
	for (mo=mobjhead.next ; mo != &mobjhead ; mo=next)
	{
		next = mo->next;	/* in case mo is removed this time */
		if (mo->latecall)
		{
			mo->latecall(mo);
		}
	}
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

	if ( (oldbuttons&JP_PAUSE) || !(buttons & JP_PAUSE ) )
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
		gamemap = warpmap;
		gameaction = ga_warped;
	}
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

int P_Ticker (void)
{
	int		start;
	int		ticstart;
	player_t	*pl;
	
	ticstart = samplecount;
	
	while (!I_RefreshLatched () )
	;		/* wait for refresh to latch all needed data before */
			/* running the next tick */

#ifdef JAGAUR
	while (DSPRead (&dspfinished) != 0xdef6)
	;		/* wait for sound mixing to complete */
#endif

	gameaction = ga_nothing;
	
	gametic++; 		 
 
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


	if (gamepaused)
		return 0;

/* */
/* run player actions */
/* */
	start = samplecount;
	for (playernum=0,pl=players ; playernum<MAXPLAYERS ; playernum++,pl++)
		if (playeringame[playernum])
		{
			if (pl->playerstate == PST_REBORN) 
				G_DoReborn (playernum); 
			AM_Control (pl);
			P_PlayerThink (pl);
		}
	playertics = samplecount - start;
	
	
	start = samplecount;
	P_RunThinkers ();
	thinkertics = samplecount - start;
		
	start = samplecount;
	P_CheckSights ();	
	sighttics = samplecount - start;

	start = samplecount;
	P_RunMobjBase ();
	basetics = samplecount - start;

	start = samplecount;
	P_RunMobjLate ();
	latetics = samplecount - start;

	P_UpdateSpecials ();

	P_RespawnSpecials ();
	
	ST_Ticker ();			/* update status bar */
		
	tictics = samplecount - ticstart;
	
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
#ifdef JAGUAR
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
	
#endif
}

/* 
============= 
= 
= DrawPlaque 
= 
============= 
*/ 
 
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
	static boolean	refreshdrawn;
#ifndef MARS
	if (players[consoleplayer].automapflags & AF_OPTIONSACTIVE)
	{
		O_Drawer ();
		refreshdrawn = false;
	}
	else if (gamepaused && refreshdrawn)
		DrawPlaque (pausepic);
	else if (players[consoleplayer].automapflags & AF_ACTIVE)
	{
		ST_Drawer ();
		AM_Drawer ();
		I_Update ();
		refreshdrawn = true;
	}
	else
#endif
	{
#ifdef JAGUAR
		ST_Drawer ();
#endif
		R_RenderPlayerView (); 
		refreshdrawn = true;
	/* assume part of the refresh is now running parallel with main code */
	}
} 
 
 
extern	 int		ticremainder[2];

void P_Start (void)
{
#ifndef MARS
	AM_Start ();
	S_RestartSounds ();
#endif
	players[0].automapflags = 0;
	players[1].automapflags = 0;
	ticremainder[0] = ticremainder[1] = 0;
	M_ClearRandom ();
}

void P_Stop (void)
{
	Z_FreeTags (mainzone);
}

