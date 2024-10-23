/* P_enemy.c */

#include "doomdef.h"
#include "p_local.h"


/*
===============================================================================

							ENEMY THINKING

enemies are allways spawned with targetplayer = -1, threshold = 0
Most monsters are spawned unaware of all players, but some can be made preaware

===============================================================================
*/

boolean P_RailThinker(mobj_t *mobj) ATTR_DATA_CACHE_ALIGN;
boolean P_CheckMeleeRange (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
boolean P_CheckMissileRange (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
boolean P_Move (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
boolean P_TryWalk (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
void P_NewChaseDir (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
void A_Look (mobj_t *actor, int16_t var1, int16_t var2) ATTR_DATA_CACHE_ALIGN;
void A_Chase (mobj_t *actor, int16_t var1, int16_t var2) ATTR_DATA_CACHE_ALIGN;
void A_Boss1Laser(mobj_t *actor, int16_t var1, int16_t var2) ATTR_DATA_CACHE_ALIGN;

/*
================
=
= P_CheckMeleeRange
=
================
*/

boolean P_CheckMeleeRange (mobj_t *actor)
{
	mobj_t		*pl;
	fixed_t		dist;
	
	if (!Mobj_HasFlags2(actor, MF2_SEETARGET))
		return false;
							
	if (!actor->target)
		return false;
		
	pl = actor->target;
	dist = P_AproxDistance (pl->x-actor->x, pl->y-actor->y);
	if (dist >= MELEERANGE)
		return false;
	
	return true;		
}

/*
================
=
= P_CheckMissileRange
=
================
*/

boolean P_CheckMissileRange (mobj_t *actor)
{
	fixed_t		dist;
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];

	if (!Mobj_HasFlags2(actor, MF2_SEETARGET))
		return false;
	
	dist = P_AproxDistance ( actor->x-actor->target->x, actor->y-actor->target->y) - 64*FRACUNIT;
	if (!ainfo->meleestate)
		dist -= 128*FRACUNIT;		/* no melee attack, so fire more */

	dist >>= 16;

	if (dist > 200)
		dist = 200;

	if (P_Random () < dist)
		return false;
		
	return true;
}


/*
================
=
= P_Move
=
= Move in the current direction
= returns false if the move is blocked
================
*/

fixed_t	xspeed[8] = {FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000};
fixed_t yspeed[8] = {0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000};

boolean P_Move (mobj_t *actor)
{
	fixed_t	tryx, tryy;
	fixed_t oldx, oldy;
	boolean		good;
	ptrymove_t	tm;

	if (actor->movedir == DI_NODIR)
		return false;
		
	oldx = actor->x;
	oldy = actor->y;
	
	tryx = actor->x + mobjinfo[actor->type].speed*xspeed[actor->movedir];
	tryy = actor->y + mobjinfo[actor->type].speed*yspeed[actor->movedir];
	
	if (!P_TryMove (&tm, actor, tryx, tryy) )
	{	/* open any specials */
		if ((actor->flags2 & MF2_FLOAT) && (actor->flags2 & MF2_ENEMY) && tm.floatok)
		{	/* must adjust height */
			if (actor->z < tm.tmfloorz)
				actor->z += FLOATSPEED;
			else
				actor->z -= FLOATSPEED;
			actor->flags2 |= MF2_INFLOAT;
			return true;
		}

//		blkline = tm.blockline;
		good = false;

		return good;
	}
	else
	{
		P_MoveCrossSpecials(actor, tm.numspechit, tm.spechit, oldx, oldy);
		actor->flags2 &= ~MF2_INFLOAT;
	}

	if (! (actor->flags2 & MF2_FLOAT) )	
		actor->z = actor->floorz;
	return true; 
}


/*
==================================
=
= TryWalk
=
= Attempts to move actoron in its current (ob->moveangle) direction.
=
= If blocked by either a wall or an actor returns FALSE
= If move is either clear or blocked only by a door, returns TRUE and sets
= If a door is in the way, an OpenDoor call is made to start it opening.
=
==================================
*/

boolean P_TryWalk (mobj_t *actor)
{	
	if (!P_Move (actor))
		return false;

	actor->movecount = P_Random()&15;
	return true;
}



/*
================
=
= P_NewChaseDir
=
================
*/

dirtype_t opposite[] =
{DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST, DI_EAST, DI_NORTHEAST,
DI_NORTH, DI_NORTHWEST, DI_NODIR};

dirtype_t diags[] = {DI_NORTHWEST,DI_NORTHEAST,DI_SOUTHWEST,DI_SOUTHEAST};

void P_NewChaseDir (mobj_t *actor)
{
	fixed_t		deltax,deltay;
	dirtype_t	d[3];
	dirtype_t	tdir, olddir, turnaround;

	if (!actor->target)
		I_Error ("P_NewChaseDir: called with no target");

	deltax = actor->target->x - actor->x;
	deltay = actor->target->y - actor->y;

	if (P_AproxDistance(deltax, deltay) > 4096*FRACUNIT)
	{
//		actor->angle = R_PointToAngle2(actor->x, actor->y, actor->target->x, actor->target->y);
		actor->target = NULL;
		return;
	}

	olddir = actor->movedir;
	turnaround=opposite[olddir];
	
	if (deltax>10*FRACUNIT)
		d[1]= DI_EAST;
	else if (deltax<-10*FRACUNIT)
		d[1]= DI_WEST;
	else
		d[1]=DI_NODIR;
	if (deltay<-10*FRACUNIT)
		d[2]= DI_SOUTH;
	else if (deltay>10*FRACUNIT)
		d[2]= DI_NORTH;
	else
		d[2]=DI_NODIR;

/* try direct route */
	if (d[1] != DI_NODIR && d[2] != DI_NODIR)
	{
		actor->movedir = diags[((deltay<0)<<1)+(deltax>0)];
		if (actor->movedir != turnaround && P_TryWalk(actor))
			return;
	}

/* try other directions */
	if (P_Random() > 200 || D_abs(deltay)> D_abs(deltax))
	{
		tdir=d[1];
		d[1]=d[2];
		d[2]=tdir;
	}

	if (d[1]==turnaround)
		d[1]=DI_NODIR;
	if (d[2]==turnaround)
		d[2]=DI_NODIR;
	
	if (d[1]!=DI_NODIR)
	{
		actor->movedir = d[1];
		if (P_TryWalk(actor))
			return;     /*either moved forward or attacked*/
	}

	if (d[2]!=DI_NODIR)
	{
		actor->movedir =d[2];
		if (P_TryWalk(actor))
			return;
	}

/* there is no direct path to the player, so pick another direction */

	if (olddir!=DI_NODIR)
	{
		actor->movedir =olddir;
		if (P_TryWalk(actor))
			return;
	}

	if (P_Random()&1) 	/*randomly determine direction of search*/
	{
		for (tdir=DI_EAST ; tdir<=DI_SOUTHEAST ; tdir++)
		{
			if (tdir!=turnaround)
			{
				actor->movedir =tdir;
				if ( P_TryWalk(actor) )
					return;
			}
		}
	}
	else
	{
		for (tdir=DI_SOUTHEAST ; (int)tdir >= (int)DI_EAST;tdir--)
		{
			if (tdir!=turnaround)
			{
				actor->movedir =tdir;
				if ( P_TryWalk(actor) )
				return;
			}
		}
	}

	if (turnaround !=  DI_NODIR)
	{
		actor->movedir =turnaround;
		if ( P_TryWalk(actor) )
			return;
	}

	actor->movedir = DI_NODIR;		/* can't move */
}


/*
================
=
= P_LookForPlayers
=
= If allaround is false, only look 180 degrees in front
= returns true if a player is targeted
================
*/

boolean P_LookForPlayers (mobj_t *actor, boolean allaround, boolean nothreshold)
{
	int 		i, j;
	angle_t		an;
	fixed_t		dist;
	mobj_t		*mo;
	
	if (! (actor->flags2 & MF2_SEETARGET) )
	{	/* pick another player as target if possible */
newtarget:
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (actor->target == players[i].mo)
			{
				i++; // advance to the next player
				break;
			}
		}

		for (j = 0; j < MAXPLAYERS; j++)
		{
			int p = (i + j) % MAXPLAYERS;
			if (playeringame[p])
			{
				actor->target = players[p].mo;
				break;
			}
		}

		return false;
	}
		
	mo = actor->target;
	if (!mo || mo->health <= 0)
		goto newtarget;
		
	if (!allaround)
	{
		an = R_PointToAngle2 (actor->x, actor->y, 
		mo->x, mo->y) - actor->angle;
		if (an > ANG90 && an < ANG270)
		{
			dist = P_AproxDistance (mo->x - actor->x,
				mo->y - actor->y);
			/* if real close, react anyway */
			if (dist > MELEERANGE)
				return false;		/* behind back */
		}
	}
	
	if (!nothreshold)
		actor->threshold = 60;

	return true;
}


/*
===============================================================================

						ACTION ROUTINES

===============================================================================
*/

/*
==============
=
= A_Look
=
= Stay in state until a player is sighted
=
==============
*/

void A_Look (mobj_t *actor, int16_t var1, int16_t var2)
{
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];
	
/* if current target is visible, start attacking */

/* if the sector has a living soundtarget, make that the new target */
	actor->threshold = 0;		/* any shot will wake up */

	if (!P_LookForPlayers (actor, false, true) )
		return;
	
//seeyou:
/* go into chase state */
	if (ainfo->seesound)
	{
		int		sound;
		
		switch (ainfo->seesound)
		{
		default:
			sound = ainfo->seesound;
			break;
		}

		S_StartSound (actor, sound);
	}

	P_SetMobjState (actor, ainfo->seestate);
}

void A_SpawnState(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->state != mobjinfo[actor->type].spawnstate)
		P_SetMobjState(actor, mobjinfo[actor->type].spawnstate);
}

/*
==============
=
= A_Chase
=
= Actor has a melee attack, so it tries to close as fast as possible
=
==============
*/

void A_Chase (mobj_t *actor, int16_t var1, int16_t var2)
{
	int		delta;
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];
	
/* */
/* modify target threshold */
/* */
	if  (actor->threshold)
		actor->threshold--;
	
/* */
/* turn towards movement direction if not there yet */
/* */
	if (actor->movedir < 8)
	{
		actor->angle &= (7<<29);
		delta = actor->angle - (actor->movedir << 29);
		if (delta > 0)
			actor->angle -= ANG90/2;
		else if (delta < 0)
			actor->angle += ANG90/2;
	}

	if (!actor->target || !(actor->target->flags2&MF2_SHOOTABLE)
		|| (netgame && !actor->threshold && !(actor->flags2 & MF2_SEETARGET)))
	{	/* look for a new target */
		if (P_LookForPlayers(actor,true,false))
			return;		/* got a new target */
		P_SetMobjState (actor, ainfo->spawnstate);
		return;
	}

/* */
/* check for melee attack */
/*	 */
	if (ainfo->meleestate && P_CheckMeleeRange (actor))
	{
		if (ainfo->attacksound)
			S_StartSound (actor, ainfo->attacksound);
		P_SetMobjState (actor, ainfo->meleestate);
		return;
	}

/* */
/* check for missile attack */
/* */
	if ( !actor->movecount && ainfo->missilestate
	&& P_CheckMissileRange (actor))
	{
		P_SetMobjState (actor, ainfo->missilestate);
		return;
	}
	
/* */
/* chase towards player */
/* */
	if (--actor->movecount<0 || !P_Move (actor))
		P_NewChaseDir (actor);
}

/*============================================================================= */

void A_BuzzFly(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->reactiontime)
		actor->reactiontime--;

	// modify target threshold
	if (actor->threshold)
		actor->threshold--;

	if (!actor->target || actor->target->health <= 0 || (netgame && !actor->threshold && !(actor->flags2 & MF2_SEETARGET)))
	{
		actor->momx = actor->momy = actor->momz = 0;
		P_SetMobjState(actor, mobjinfo[actor->type].spawnstate); // Go back to looking around
		return;
	}

	// turn towards movement direction if not there yet
	actor->angle = R_PointToAngle2(actor->x, actor->y, actor->target->x, actor->target->y);

	// chase towards player
	{
		int dist, realspeed;
		const fixed_t mf = 5*(FRACUNIT/4);

		realspeed = mobjinfo[actor->type].speed;

		dist = P_AproxDistance(P_AproxDistance(actor->target->x - actor->x,
			actor->target->y - actor->y), actor->target->z - actor->z);

		if (dist < 1)
			dist = 1;

		actor->momx = FixedMul(FixedDiv(actor->target->x - actor->x, dist), realspeed);
		actor->momy = FixedMul(FixedDiv(actor->target->y - actor->y, dist), realspeed);
		actor->momz = FixedMul(FixedDiv(actor->target->z - actor->z, dist), realspeed);

		fixed_t watertop = GetWatertopMo(actor);
		if (actor->z + actor->momz < watertop)
		{
			actor->z = watertop;
			actor->momz = 0;
		}
	}
}

void A_JetJawRoam(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->reactiontime)
	{
		actor->reactiontime--;
		P_InstaThrust(actor, actor->angle, mobjinfo[actor->type].speed*FRACUNIT/4);
	}
	else
	{
		actor->reactiontime = mobjinfo[actor->type].reactiontime;
		actor->angle += ANG180;
	}

	if (P_LookForPlayers (actor, false, false))
		P_SetMobjState(actor, mobjinfo[actor->type].seestate);
}

void A_JetJawChomp(mobj_t *actor, int16_t var1, int16_t var2)
{
	const mobjinfo_t *ainfo = &mobjinfo[actor->type];
	int delta;

	if (!(leveltime & 7))
		S_StartSound(actor, ainfo->attacksound);

/* */
/* turn towards movement direction if not there yet */
/* */
	if (actor->movedir < 8)
	{
		actor->angle &= (7<<29);
		delta = actor->angle - (actor->movedir << 29);
		if (delta > 0)
			actor->angle -= ANG90/2;
		else if (delta < 0)
			actor->angle += ANG90/2;
	}

	if (!actor->target || !(actor->target->flags2&MF2_SHOOTABLE)
		|| !actor->target->health || !(actor->flags2 & MF2_SEETARGET))
	{
		P_SetMobjState(actor, ainfo->spawnstate);
		return;
	}
	
/* */
/* chase towards player */
/* */
	if (--actor->movecount<0 || !P_Move(actor))
		P_NewChaseDir(actor);

	fixed_t watertop = GetWatertopMo(actor);
	if (actor->z > watertop - (actor->theight << FRACBITS))
		actor->z = watertop - (actor->theight << FRACBITS);
}

/*
==============
=
= A_FaceTarget
=
==============
*/

void A_FaceTarget (mobj_t *actor, int16_t var1, int16_t var2)
{	
	if (!actor->target)
		return;
	
	actor->angle = R_PointToAngle2 (actor->x, actor->y
	, actor->target->x, actor->target->y);
}

/*
==================
=
= SkullAttack
=
= Fly at the player like a missile
=
==================
*/

#define	SKULLSPEED		(24*FRACUNIT)
void P_Shoot2 (lineattack_t *la);

void A_SkullAttack (mobj_t *actor, int16_t var1, int16_t var2)
{
	mobj_t			*dest;
	angle_t			an;
	int				dist;
	const mobjinfo_t* mobjInfo = &mobjinfo[actor->type];

	if (!actor->target)
		return;
		
	dest = actor->target;	
	actor->flags2 |= MF2_SKULLFLY;

	S_StartSound (actor, mobjInfo->activesound);
	A_FaceTarget (actor, 0, 0);

	dist = P_AproxDistance (dest->x - actor->x, dest->y - actor->y);

	{
		static int k = 0;
		int i, j;
		angle_t testang = 0;
		lineattack_t la;
		la.shooter = actor;
		la.attackrange = dist;
		la.aimtopslope = 100*FRACUNIT/160;
		la.aimbottomslope = 100*FRACUNIT/160;

		if (P_Random() & 1) // Imaginary 50% chance
		{
			i = 9;
			j = 27;
		}
		else
		{
			i = 27;
			j = 9;
		}

#define dostuff(q) \
			testang = actor->angle + ((i+(q))*(ANG90/9));\
			la.attackangle = testang;\
			P_Shoot2(&la);\
			if (P_AproxDistance(la.shootx - actor->x, la.shooty - actor->y) > dist + 2*mobjInfo->radius)\
				break;

		if (P_Random() & 1) // imaginary 50% chance
		{
			for (k = 0; k < 9; k++)
			{
				dostuff(i+k)
				dostuff(i-k)
				dostuff(j+k)
				dostuff(j-k)
			}
		}
		else
		{
			for (int k = 0; k < 9; k++)
			{
				dostuff(i-k)
				dostuff(i+k)
				dostuff(j-k)
				dostuff(j+k)
			}
		}
		actor->angle = testang;

#undef dostuff
	}

	an = actor->angle >> ANGLETOFINESHIFT;
	actor->momx = FixedMul (SKULLSPEED, finecosine(an));
	actor->momy = FixedMul (SKULLSPEED, finesine(an));
	dist = dist / SKULLSPEED;
	if (dist < 1)
		dist = 1;
	actor->momz = (dest->z+(dest->theight<<(FRACBITS-1)) - actor->z) / dist;

	actor->momz = 0; // Horizontal only
}

void A_Pain (mobj_t *actor, int16_t var1, int16_t var2)
{
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];
	if (ainfo->painsound)
		S_StartSound (actor, ainfo->painsound);

	actor->flags2 &= ~MF2_FIRING;
}

void A_Fall (mobj_t *actor, int16_t var1, int16_t var2)
{
/* actor is on ground, it can be walked over */
	actor->flags &= ~MF_SOLID;

	// Also want to set these
	P_UnsetThingPosition(actor);
	actor->flags |= MF_NOBLOCKMAP|MF_NOCLIP|MF_NOGRAVITY;
	P_SetThingPosition(actor);

	if (var1)
		actor->extradata = var1;
}


/*
================
=
= A_Explode
=
================
*/

void A_Explode (mobj_t *thingy, int16_t var1, int16_t var2)
{
	P_RadiusAttack ( thingy, thingy->target, 128 );
}

//
// A_BossScream
//
// Spawns explosions and plays appropriate sounds around the defeated boss.
//
// var1: If nonzero, will spawn S_FRET at this height
// var2 = Object to spawn, if not specified, uses MT_SONIC3KBOSSEXPLODE
//
void A_BossScream(mobj_t *actor, int16_t var1, int16_t var2)
{
	const mobjtype_t explodetype = var2 > 0 ? var2 : MT_SONIC3KBOSSEXPLODE;

	const angle_t fa = ((leveltime*(ANG45/9))>>ANGLETOFINESHIFT) & FINEMASK;

	const fixed_t x = actor->x + FixedMul(finecosine(fa),mobjinfo[actor->type].radius);
	const fixed_t y = actor->y + FixedMul(finesine(fa),mobjinfo[actor->type].radius);
	const fixed_t z = actor->z + (P_Random()<<(FRACBITS-2)) - 8*FRACUNIT;

	mobj_t *mo = P_SpawnMobj(x, y, z, explodetype);

	if (mobjinfo[actor->type].deathsound)
		S_StartSound(mo, mobjinfo[actor->type].deathsound);

	if (var1 > 0)
	{
		mo = P_SpawnMobj(actor->x, actor->y, actor->z + (var1 << FRACBITS), MT_GHOST);
		mo->reactiontime = 2;
		P_SetMobjState(mo, S_FRET);
	}
}

/*
================
=
= A_BossDeath
=
= Possibly trigger special effects
================
*/

void P_DoBossVictory(mobj_t *mo)
{
	// Trigger the egg capsule (if it exists)
	if (gamemapinfo.afterBossMusic)
		S_StartSong(gamemapinfo.afterBossMusic, 1, gamemapinfo.mapNumber);

	VINT outerNum = P_FindSectorWithTag(254, -1);
	VINT innerNum = P_FindSectorWithTag(255, -1);

	if (outerNum == -1 || innerNum == -1)
		return;

	sector_t *outer = &sectors[outerNum];
	sector_t *inner = &sectors[innerNum];

	inner->floorheight += 16*FRACUNIT; // OK to just insta-set this
	inner->floorpic = R_FlatNumForName("YELFLR");
	outer->floorpic = R_FlatNumForName("TRAPFLR");
	outer->heightsec = -1;
	inner->heightsec = -1;

	// Move the outer
	floormove_t *floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC);
	P_AddThinker (&floor->thinker);
	outer->specialdata = floor;
	floor->thinker.function = T_MoveFloor;
	floor->type = eggCapsuleOuter;
	floor->crush = false;
	floor->direction = 1;
	floor->sector = outer;
	floor->speed = 2*FRACUNIT;
	floor->floordestheight = 
		(outer->floorheight>>FRACBITS) + 128;

	// Move the inner
	floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC);
	P_AddThinker (&floor->thinker);
	inner->specialdata = floor;
	floor->thinker.function = T_MoveFloor;
	floor->type = eggCapsuleInner;
	floor->crush = false;
	floor->direction = 1;
	floor->sector = inner;
	floor->speed = 2*FRACUNIT;
	floor->floordestheight = 
		(inner->floorheight>>FRACBITS) + 128;
}

//
// A_BossJetFume
//
// Description: Spawns jet fumes/other attachment miscellany for the boss. To only be used when he is spawned.
//
// var1:
//		0 - Triple jet fume pattern
// var2 = unused
//
void A_BossJetFume(mobj_t *actor, int16_t var1, int16_t var2)
{
	mobj_t *filler;

	if (var1 == 0) // Boss1 jet fumes
	{
		fixed_t jetx = actor->x;
		fixed_t jety = actor->y;
		fixed_t jetz = actor->z;

		// One
		P_ThrustValues(actor->angle, -64*FRACUNIT, &jetx, &jety);
		jetz += 38*FRACUNIT;

		filler = P_SpawnMobj(jetx, jety, jetz, MT_JETFUME1);
		filler->target = actor;
		filler->movecount = 1; // This tells it which one it is

		// Two
		jetz = actor->z + 12*FRACUNIT;
		jetx = actor->x;
		jety = actor->y;
		P_ThrustValues(actor->angle-ANG90, 24*FRACUNIT, &jetx, &jety);
		filler = P_SpawnMobj(jetx, jety, jetz, MT_JETFUME1);
		filler->target = actor;
		filler->movecount = 2; // This tells it which one it is

		// Three
		jetx = actor->x;
		jety = actor->y;
		P_ThrustValues(actor->angle+ANG90, 24*FRACUNIT, &jetx, &jety);
		filler = P_SpawnMobj(jetx, jety, jetz, MT_JETFUME1);
		filler->target = actor;
		filler->movecount = 3; // This tells it which one it is
	}

	actor->flags2 |= MF2_SPAWNEDJETS;
}

static void P_SpawnBoss1Junk(mobj_t *mo)
{
	mobj_t *mo2 = P_SpawnMobj(
		mo->x + P_ReturnThrustX(mo->angle - ANG90, 32<<FRACBITS),
		mo->y + P_ReturnThrustY(mo->angle - ANG90, 32<<FRACBITS),
		mo->z + (32<<FRACBITS), MT_BOSSJUNK);

	mo2->angle = mo->angle;
	P_InstaThrust(mo2, mo2->angle - ANG90, 4*FRACUNIT);
	P_SetObjectMomZ(mo2, 4*FRACUNIT, false);
	P_SetMobjState(mo2, S_BOSSEGLZ1);

	mo2 = P_SpawnMobj(
		mo->x + P_ReturnThrustX(mo->angle + ANG90, 32<<FRACBITS),
		mo->y + P_ReturnThrustY(mo->angle + ANG90, 32<<FRACBITS),
		mo->z + (32<<FRACBITS), MT_BOSSJUNK);

	mo2->angle = mo->angle;
	P_InstaThrust(mo2, mo2->angle + ANG90, 4*FRACUNIT);
	P_SetObjectMomZ(mo2, 4*FRACUNIT, false);
	P_SetMobjState(mo2, S_BOSSEGLZ2);
}

angle_t InvAngle(angle_t a)
{
	return (0xffffffff-a) + 1;
}

static void P_DoBossDefaultDeath(mobj_t *mo)
{
	// Stop exploding and prepare to run.
	P_SetMobjState(mo, mobjinfo[mo->type].xdeathstate);
	mo->target = P_FindFirstMobjOfType(MT_BOSSFLYPOINT);

	P_UnsetThingPosition(mo);
	mo->flags |= MF_NOBLOCKMAP|MF_NOGRAVITY|MF_NOCLIP;
	mo->flags2 |= MF2_FLOAT;
	P_SetThingPosition(mo);

	mo->movedir = 0;
	mo->extradata = 2*TICRATE;
	mo->flags2 |= MF2_BOSSFLEE;
	mo->momz = P_MobjFlip(mo)*(1 << (FRACBITS-1));
}

void A_BossDeath (mobj_t *mo, int16_t var1, int16_t var2)
{
	int 		i;

    // make sure there is a player alive for victory
    for (i=0 ; i<MAXPLAYERS ; i++)
		if (playeringame[i] && players[i].health > 0)
			break;
    if (i == MAXPLAYERS)
		return;

	P_DoBossVictory(mo);

	switch (mo->type)
	{
		case MT_EGGMOBILE:
			P_SpawnBoss1Junk(mo);
			break;
		default:
			break;
	}

	P_DoBossDefaultDeath(mo);
}

void A_FishJump(mobj_t *mo, int16_t var1, int16_t var2)
{
	fixed_t watertop = mo->floorz;

	if (subsectors[mo->isubsector].sector->heightsec != -1)
		watertop = GetWatertopMo(mo) - (64<<FRACBITS);

	if ((mo->z <= mo->floorz) || (mo->z <= watertop))
	{
		fixed_t jumpval;

		if (mo->angle)
			jumpval = (mo->angle / ANGLE_1)<<(FRACBITS-2);
		else
			jumpval = 16 << FRACBITS;

		jumpval = FixedMul(jumpval, FixedDiv(30 << FRACBITS, 35 << FRACBITS));

		mo->momz = jumpval;
		P_SetMobjState(mo, mobjinfo[mo->type].seestate);
	}

	if (mo->momz < 0
		&& (mo->state < mobjinfo[mo->type].meleestate || mo->state > mobjinfo[mo->type].xdeathstate))
		P_SetMobjState(mo, mobjinfo[mo->type].meleestate);
}

void A_MonitorPop(mobj_t *actor, int16_t var1, int16_t var2)
{
	mobjtype_t iconItem = 0;
	mobj_t *newmobj;

	// Spawn the "pop" explosion.
	if (mobjinfo[actor->type].deathsound)
		S_StartSound(actor, mobjinfo[actor->type].deathsound);
	P_SpawnMobj(actor->x, actor->y, actor->z + (actor->theight << FRACBITS)/4, MT_EXPLODE);

	// We're dead now. De-solidify.
	actor->health = 0;
	P_UnsetThingPosition(actor);
	actor->flags &= ~MF_SOLID;
	actor->flags2 &= ~MF2_SHOOTABLE;
	actor->flags |= MF_NOCLIP;
	P_SetThingPosition(actor);

	iconItem = mobjinfo[actor->type].damage;

	if (iconItem == 0)
	{
//		CONS_Printf("A_MonitorPop(): 'damage' field missing powerup item definition.\n");
		return;
	}

	newmobj = P_SpawnMobj(actor->x, actor->y, actor->z + 13*FRACUNIT, iconItem);
	newmobj->target = players[0].mo; // TODO: Not multiplayer compatible, but don't care right now
	//actor->target; // transfer the target
}

// Having one function for all box awarding cuts down ROM size
void A_AwardBox(mobj_t *actor, int16_t var1, int16_t var2)
{
	player_t *player;
	mobj_t *orb;

	if (!actor->target || !actor->target->player)
	{
//		CONS_Printf("A_AwardBox(): Monitor has no target.\n");
		return;
	}

	player = &players[actor->target->player - 1];

	switch(actor->type)
	{
		case MT_RING_ICON:
			P_GivePlayerRings(player, mobjinfo[actor->type].reactiontime);
			break;
		case MT_ELEMENTAL_ICON:
			if (player->powers[pw_underwater] <= 12*TICRATE + 1)
				P_RestoreMusic(player);
		case MT_ARMAGEDDON_ICON:
		case MT_ATTRACT_ICON:
		case MT_FORCE_ICON:
		case MT_WHIRLWIND_ICON:
			orb = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, mobjinfo[actor->type].painchance);
			orb->target = player->mo;
			player->shield = mobjinfo[orb->type].painchance;
			break;
		case MT_1UP_ICON:
			player->lives++;
			S_StopSong();
			S_StartSong(gameinfo.xtlifeMus, 0, cdtrack_xtlife);
			player->powers[pw_extralife] = EXTRALIFETICS + 1;
			break;
		case MT_INVULN_ICON:
			player->powers[pw_invulnerability] = INVULNTICS + 1;
			S_StopSong();
			S_StartSong(gameinfo.invincMus, 0, cdtrack_invincibility);
			break;
		case MT_SNEAKERS_ICON:
			player->powers[pw_sneakers] = SNEAKERTICS + 1;
			S_StopSong();
			S_StartSong(gameinfo.sneakerMus, 0, cdtrack_sneakers);
			break;
		default:
			// Dunno what kind of monitor this is, but we fail gracefully.
			break;
	}

	if (mobjinfo[actor->type].seesound)
		S_StartSound(player->mo, mobjinfo[actor->type].seesound);
}

void A_FlickyCheck(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->target && actor->z <= actor->floorz)
	{
		P_SetMobjState(actor, mobjinfo[actor->type].seestate);
		actor->reactiontime = P_Random();
		if (actor->reactiontime < 90)
			actor->reactiontime = 90;
	}
}

void A_FlickyFly(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->z <= actor->floorz)
	{
		A_FaceTarget(actor, 0, 0);
		actor->momz = mobjinfo[actor->type].mass << FRACBITS;

		if (mobjinfo[actor->type].painchance != 0)
		{
			P_InstaThrust(actor, actor->angle, mobjinfo[actor->type].speed);
			P_SetMobjState(actor, mobjinfo[actor->type].meleestate);
		}
	}
	
	if (mobjinfo[actor->type].painchance == 0)
		P_InstaThrust(actor, actor->angle, mobjinfo[actor->type].speed);

	actor->reactiontime--;
	if (actor->reactiontime == 0)
		P_RemoveMobj(actor);
}

void A_BubbleRise(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->type == MT_EXTRALARGEBUBBLE)
		actor->momz = FixedDiv(6*FRACUNIT, 5*FRACUNIT);
	else
		actor->momz += FRACUNIT / 32;

	if (P_Random() < 32)
		P_InstaThrust(actor, P_Random() & 1 ? actor->angle + ANG90 : actor->angle,
			(P_Random() & 1) ? FRACUNIT / 2 : -FRACUNIT/2);
	else if (P_Random() < 32)
		P_InstaThrust(actor, P_Random() & 1 ? actor->angle - ANG90 : actor->angle - ANG180,
			(P_Random() & 1) ? FRACUNIT/2 : -FRACUNIT/2);

	if (subsectors[actor->isubsector].sector->heightsec == -1
		|| actor->z + (actor->theight << (FRACBITS-1)) > GetWatertopMo(actor))
		P_RemoveMobj(actor);
}

// Boss 1 Stuff
void A_FocusTarget(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (actor->target)
	{
		fixed_t speed = mobjinfo[actor->type].speed;
		angle_t hangle = actor->angle;
		angle_t vangle = ANG90;

		actor->momx -= actor->momx>>4, actor->momy -= actor->momy>>4, actor->momz -= actor->momz>>4;
		actor->momz += FixedMul(finecosine(vangle>>ANGLETOFINESHIFT), speed);
		actor->momx += FixedMul(finesine(vangle>>ANGLETOFINESHIFT), FixedMul(finecosine(hangle>>ANGLETOFINESHIFT), speed));
		actor->momy += FixedMul(finesine(vangle>>ANGLETOFINESHIFT), FixedMul(finesine(hangle>>ANGLETOFINESHIFT), speed));
	}
}

// mobj->target getting set to NULL is our hint that the mobj was removed.
boolean P_RailThinker(mobj_t *mobj)
{
	fixed_t x, y, z;
	x = mobj->x, y = mobj->y, z = mobj->z;

	if (mobj->momx || mobj->momy)
	{
		P_XYMovement(mobj);
		if (mobj->target == NULL)
			return true; // was removed
	}

	if (mobj->momz)
	{
		P_ZMovement(mobj);
		if (mobj->target == NULL)
			return true; // was removed
	}

	return mobj->target == NULL || (x == mobj->x && y == mobj->y && z == mobj->z);
}

void A_Boss1Chase(mobj_t *actor, int16_t var1, int16_t var2)
{
	int delta;

	if (leveltime < 5)
		actor->movecount = 2*TICRATE;

	if (actor->reactiontime)
		actor->reactiontime--;

	if (actor->z < actor->floorz+33*FRACUNIT)
		actor->z = actor->floorz+33*FRACUNIT;

	// turn towards movement direction if not there yet
	if (actor->movedir < NUMDIRS)
	{
		actor->angle &= (7<<29);
		delta = actor->angle - (actor->movedir << 29);

		if (delta > 0)
			actor->angle -= ANG45;
		else if (delta < 0)
			actor->angle += ANG45;
	}

	// do not attack twice in a row
	if (actor->flags2 & MF2_JUSTATTACKED)
	{
		actor->flags2 &= ~MF2_JUSTATTACKED;
		P_NewChaseDir(actor);
		return;
	}

	if (actor->movecount)
		goto nomissile;

	if (actor->target && P_AproxDistance(actor->x - actor->target->x, actor->y - actor->target->y) > 768*FRACUNIT)
		goto nomissile;

	if (actor->reactiontime <= 0)
	{
		if (actor->health > mobjinfo[actor->type].damage)
		{
			if (P_Random() & 1)
				P_SetMobjState(actor, mobjinfo[actor->type].missilestate);
			else
				P_SetMobjState(actor, mobjinfo[actor->type].meleestate);
		}
		else
			P_SetMobjState(actor, mobjinfo[actor->type].mass);

		actor->flags2 |= MF2_JUSTATTACKED;
		actor->reactiontime = 2*TICRATE;
		return;
	}

	// ?
nomissile:
	// possibly choose another target
	if ((splitscreen || netgame) && P_Random() < 2)
	{
		if (P_LookForPlayers(actor, true, true))
			return; // got a new target
	}

	// chase towards player
	if (--actor->movecount < 0 || !P_Move(actor))
		P_NewChaseDir(actor);
}

void A_Boss1Laser(mobj_t *actor, int16_t var1, int16_t var2)
{
	fixed_t x, y, z, floorz, speed;
	int upperend = (var2>>8);
	var2 &= 0xff;
	int i;
	angle_t angle;
	mobj_t *point;
	int dur;

	if (!actor->target)
		return;

	if (states[actor->state].tics > 1)
		dur = actor->tics;
	else
	{
		if ((upperend & 1) && ((int32_t)actor->extradata > 1))
			actor->extradata = ((int32_t)actor->extradata) - 1;

		dur = (int32_t)actor->extradata;
	}

	switch (var2)
	{
		case 0:
			x = actor->x + P_ReturnThrustX(actor->angle+ANG90, 44*FRACUNIT);
			y = actor->y + P_ReturnThrustY(actor->angle+ANG90, 44*FRACUNIT);
			z = actor->z + 56*FRACUNIT;
			break;
		case 1:
			x = actor->x + P_ReturnThrustX(actor->angle-ANG90, 44*FRACUNIT);
			y = actor->y + P_ReturnThrustY(actor->angle-ANG90, 44*FRACUNIT);
			z = actor->z + 56*FRACUNIT;
			break;
		case 2:
			A_Boss1Laser(actor, var1, 3); // middle laser
			A_Boss1Laser(actor, var1, 0); // left laser
			A_Boss1Laser(actor, var1, 1); // right laser
			return;
			break;
		case 3:
			x = actor->x + P_ReturnThrustX(actor->angle, 42*FRACUNIT);
			y = actor->y + P_ReturnThrustY(actor->angle, 42*FRACUNIT);
			z = actor->z + (actor->theight << (FRACBITS-1));
			break;
		default:
			x = actor->x;
			y = actor->y;
			z = actor->z + (actor->theight << (FRACBITS-1));
			break;
	}

	if (!(actor->flags2 & MF2_FIRING) && dur > 1)
	{
		actor->angle = R_PointToAngle2(x, y, actor->target->x, actor->target->y);
		if (mobjinfo[var1].seesound)
			S_StartSound(actor, mobjinfo[var1].seesound);

		point = P_SpawnMobj(x + P_ReturnThrustX(actor->angle, mobjinfo[actor->type].radius), y + P_ReturnThrustY(actor->angle, mobjinfo[actor->type].radius), actor->z - (actor->theight << (FRACBITS-1)) / 2, MT_EGGMOBILE_TARGET);
		point->angle = actor->angle;
		point->reactiontime = dur+1;
		point->target = actor->target;
		actor->target = point;
	}

	angle = R_PointToAngle2(z + (mobjinfo[var1].height>>1), 0, actor->target->z, P_AproxDistance(x - actor->target->x, y - actor->target->y));

	point = P_SpawnMobj(x, y, z, var1);

	const int iterations = 24;
	point->target = actor;
	point->angle = actor->angle;
	speed = mobjinfo[point->type].radius + (mobjinfo[point->type].radius / 2);
	point->momz = FixedMul(finecosine(angle>>ANGLETOFINESHIFT), speed);
	point->momx = FixedMul(finesine(angle>>ANGLETOFINESHIFT), FixedMul(finecosine(point->angle>>ANGLETOFINESHIFT), speed));
	point->momy = FixedMul(finesine(angle>>ANGLETOFINESHIFT), FixedMul(finesine(point->angle>>ANGLETOFINESHIFT), speed));

	const mobjinfo_t *pointInfo = &mobjinfo[point->type];
	for (i = 0; i < iterations; i++)
	{
		mobj_t *mo = P_SpawnMobj(point->x, point->y, point->z, point->type);
		mo->target = actor;

		mo->angle = point->angle;
//		P_UnsetThingPosition(mo);
//		mo->flags = MF_NOCLIP|MF_NOGRAVITY;
//		P_SetThingPosition(mo);

		if ((dur & 1) && pointInfo->missilestate)
			P_SetMobjState(mo, pointInfo->missilestate);

		x = point->x, y = point->y, z = point->z;
		if (P_RailThinker(point))
			break;
	}

	x += point->momx;
	y += point->momy;
	floorz = R_PointInSubsector(x, y)->sector->floorheight;
	if (z - floorz < (mobjinfo[MT_EGGMOBILE_FIRE].height>>1) && (dur & 1))
	{
		point = P_SpawnMobj(x, y, floorz, MT_EGGMOBILE_FIRE);

		point->angle = actor->angle;
		point->target = actor;

		const sector_t *pointSector = subsectors[point->isubsector].sector;

		if (pointSector->heightsec != -1 && point->z <= GetWatertopSec(pointSector))
		{
//			for (i = 0; i < 2; i++)
			{
				mobj_t *steam = P_SpawnMobj(x, y, GetWatertopSec(pointSector) - mobjinfo[MT_DUST].height/2, MT_DUST);
				if (mobjinfo[point->type].painsound)
					S_StartSound(steam, mobjinfo[point->type].painsound);
			}
		}
		else
		{
//			fixed_t distx = P_ReturnThrustX(point, point->angle, mobjinfo[point->type].radius);
//			fixed_t disty = P_ReturnThrustY(point, point->angle, mobjinfo[point->type].radius);
//			if (P_TryMove(point, point->x + distx, point->y + disty, false) // prevents the sprite from clipping into the wall or dangling off ledges
//				&& P_TryMove(point, point->x - 2*distx, point->y - 2*disty, false)
//				&& P_TryMove(point, point->x + distx, point->y + disty, false))
			{
				if (mobjinfo[point->type].seesound)
					S_StartSound(point, mobjinfo[point->type].seesound);
			}
//			else
//				P_RemoveMobj(point);
		}
	}

	if (dur > 1)
		actor->flags2 |= MF2_FIRING;
	else
		actor->flags2 &= ~MF2_FIRING;
}

void A_PrepareRepeat(mobj_t *actor, int16_t var1, int16_t var2)
{
	actor->extradata = var1;
}

void A_Repeat(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (var1 && (!actor->extradata || (int32_t)actor->extradata > var1))
		actor->extradata = var1;

	actor->extradata = (int)actor->extradata - 1;
	if (actor->extradata > 0)
		P_SetMobjState(actor, var2);
}

//
// var1 = height
// var2 = unused
//
void A_ChangeHeight(mobj_t *actor, int16_t var1, int16_t var2)
{
	P_UnsetThingPosition(actor);
	actor->theight = var1;
	P_SetThingPosition(actor);
}

void A_UnidusBall(mobj_t *actor)
{
	actor->angle += ANG180 / 16;

	if (actor->movecount)
	{
		if (P_AproxDistance(actor->momx, actor->momy) < (mobjinfo[actor->type].damage << (FRACBITS-1)))
			P_ExplodeMissile(actor);
		return;
	}

	if (!actor->target || !actor->target->health)
	{
		P_RemoveMobj(actor);
		return;
	}

	P_UnsetThingPosition(actor);
	{
		const angle_t angle = actor->movedir + ANGLE_1 * (mobjinfo[actor->type].speed*(leveltime%360));
		const uint16_t fa = angle>>ANGLETOFINESHIFT;

		actor->x = actor->target->x + FixedMul(finecosine(fa),actor->threshold);
		actor->y = actor->target->y + FixedMul(  finesine(fa),actor->threshold);
		actor->z = actor->target->z + (actor->target->theight << (FRACBITS-1)) - (actor->theight << (FRACBITS-1));
	}
	P_SetThingPosition(actor);
}

// Function: A_BubbleSpawn
//
// Description: Spawns a randomly sized bubble from the object's location. Only works underwater.
//
void A_BubbleSpawn(mobj_t *actor, int16_t var1, int16_t var2)
{
	if (subsectors[actor->isubsector].sector->heightsec != -1
		&& actor->z < GetWatertopMo(actor) - 32*FRACUNIT)
	{
		int i;
		uint8_t prandom;
		actor->flags2 &= ~MF2_DONTDRAW;

		// Quick! Look through players!
		// Don't spawn bubbles unless a player is relatively close by (var1).
		for (i = 0; i < MAXPLAYERS; ++i)
			if (playeringame[i] && players[i].mo
			 && P_AproxDistance(actor->x - players[i].mo->x, actor->y - players[i].mo->y) < (1024<<FRACBITS))
				break; // Stop looking.
		if (i == MAXPLAYERS)
			return; // don't make bubble!

		prandom = P_Random();

		if (leveltime % (3*TICRATE) < 8)
			P_SpawnMobj(actor->x, actor->y, actor->z + (actor->theight << (FRACBITS-1)), MT_EXTRALARGEBUBBLE);
		else if (prandom > 128)
			P_SpawnMobj(actor->x, actor->y, actor->z + (actor->theight << (FRACBITS-1)), MT_SMALLBUBBLE);
		else if (prandom < 128 && prandom > 96)
			P_SpawnMobj(actor->x, actor->y, actor->z + (actor->theight << (FRACBITS-1)), MT_MEDIUMBUBBLE);
	}
	else
	{
		actor->flags2 |= MF2_DONTDRAW;
		return;
	}
}

// Function: A_SignSpin
//
// Description: Spawns MT_SPARK around the signpost.
//
void A_SignSpin(mobj_t *actor, int16_t var1, int16_t var2)
{
	fixed_t radius = mobjinfo[actor->type].radius;

	actor->angle += ANG45 / 9;
	for (int i = -1; i < 2; i += 2)
	{
		P_SpawnMobj(
			actor->x + P_ReturnThrustX(actor->angle, i * radius),
			actor->y + P_ReturnThrustY(actor->angle, i * radius),
			actor->z + (actor->theight << FRACBITS),
			mobjinfo[actor->type].painchance);
	}
}

/*============================================================================= */

/* a move in p_base.c crossed a special line */
void L_CrossSpecial (mobj_t *mo)
{
	line_t	*line;
	
	line = (line_t *)(mo->extradata & ~1);
	
	P_CrossSpecialLine (line, mo);
}

/* a move in p_base.c caused a missile to hit another thing or wall */
void L_MissileHit (mobj_t *mo)
{
	int	damage;
	mobj_t	*missilething;
	const mobjinfo_t* moinfo = &mobjinfo[mo->type];

	missilething = (mobj_t *)mo->extradata;
	if (missilething)
	{
		mo->extradata = 0;
		damage = ((P_Random()&7)+1)* moinfo->damage;
		P_DamageMobj (missilething, mo, mo->target, damage);
	}
	P_ExplodeMissile (mo);
}

/* a move in p_base.c caused a flying skull to hit another thing or a wall */
void L_SkullBash (mobj_t *mo)
{
	int	damage;
	mobj_t	*skullthing;
	const mobjinfo_t* moinfo = &mobjinfo[mo->type];

	skullthing = (mobj_t *)mo->extradata;
	if (skullthing)
	{
		mo->extradata = 0;
		damage = ((P_Random()&7)+1)* moinfo->damage;
		P_DamageMobj (skullthing, mo, mo, damage);
	}
	
//	mo->flags &= ~MF_SKULLFLY;
	mo->momx = mo->momy = mo->momz = 0;
	P_SetMobjState (mo, moinfo->spawnstate);
}

/*  */
