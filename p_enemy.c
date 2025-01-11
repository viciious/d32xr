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

boolean P_CheckMeleeRange (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
boolean P_CheckMissileRange (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
boolean P_Move (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
boolean P_TryWalk (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
void P_NewChaseDir (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
boolean P_LookForPlayers (mobj_t *actor, boolean allaround) ATTR_DATA_CACHE_ALIGN;
void A_Look (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
void A_Chase (mobj_t *actor) ATTR_DATA_CACHE_ALIGN;
void A_SpawnFly(mobj_t *mo);
void P_Telefrag (mobj_t *thing, fixed_t x, fixed_t y);

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
	
	if (! (actor->flags&MF_SEETARGET) )
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

	if (! (actor->flags & MF_SEETARGET) )
		return false;

	if (actor->flags & MF_JUSTHIT)
	{	/* the target just hit the enemy, so fight back! */
		actor->flags &= ~MF_JUSTHIT;
		return true;
	}
	
	if (actor->reactiontime)
		return false;		/* don't attack yet */
		
	dist = P_AproxDistance ( actor->x-actor->target->x, actor->y-actor->target->y) - 64*FRACUNIT;
	if (!ainfo->meleestate)
		dist -= 128*FRACUNIT;		/* no melee attack, so fire more */

	dist >>= 16;

	if (actor->type == MT_SKULL
		|| actor->type == MT_CYBORG
		|| actor->type == MT_SPIDER)
		dist >>= 1;

	if (dist > 200)
		dist = 200;

	if (actor->type == MT_CYBORG && dist > 160)
		dist = 160;

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
	line_t		*blkline;
	pmovework_t tm;

	if (actor->movedir == DI_NODIR)
		return false;
	if (actor->movedir == DI_NODIRWAIT)
		return true;
		
	oldx = actor->x;
	oldy = actor->y;
	tryx = actor->x + actor->speed*xspeed[actor->movedir];
	tryy = actor->y + actor->speed*yspeed[actor->movedir];
	
	if (!P_TryMove (&tm, actor, tryx, tryy) )
	{
		if (actor->flags & MF_SKULLFLY && tm.hitthing)
		{
			actor->extradata = (intptr_t)tm.hitthing;
			L_SkullBash(actor);
			return false;
		}

		/* open any specials */
		if (actor->flags & MF_FLOAT && tm.floatok)
		{	/* must adjust height */
			if (actor->z < tm.tmfloorz)
				actor->z += FLOATSPEED;
			else
				actor->z -= FLOATSPEED;
			actor->flags |= MF_INFLOAT;
			return true;
		}

		good = false;
		if (tm.numspechit)
		{
			int i = tm.numspechit;
			do
			{
				actor->movedir = DI_NODIR;
				/* if the special isn't a door that can be opened, return false */
				blkline = tm.spechit[--i];
				if (P_UseSpecialLine(actor, blkline))
					good = true;
			} while (i);
		}

		return good;
	}
	else
	{
		P_MoveCrossSpecials(actor, tm.numspechit, tm.spechit, oldx, oldy);
		actor->flags &= ~MF_INFLOAT;
	}

	if (! (actor->flags & MF_FLOAT) )	
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

VINT opposite[] =
{DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST, DI_EAST, DI_NORTHEAST,
DI_NORTH, DI_NORTHWEST, DI_NODIR};

VINT diags[] = {DI_NORTHWEST,DI_NORTHEAST,DI_SOUTHWEST,DI_SOUTHEAST};

void P_NewChaseDir (mobj_t *actor)
{
	fixed_t		deltax,deltay;
	dirtype_t	d[3];
	dirtype_t	tdir, olddir, turnaround;

	if (!actor->target)
		I_Error ("P_NewChaseDir: called with no target");
		
	olddir = actor->movedir;
	if (olddir == DI_NODIRWAIT)
		olddir = DI_NODIR;
	turnaround=opposite[olddir];

	deltax = actor->target->x - actor->x;
	deltay = actor->target->y - actor->y;

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

	actor->movedir = DI_NODIRWAIT;		/* can't move for a period of time */
	actor->movecount = P_Random()&31;
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

boolean P_LookForPlayers (mobj_t *actor, boolean allaround)
{
	int 		i, j;
	angle_t		an;
	fixed_t		dist;
	mobj_t		*mo;
	
	if (! (actor->flags & MF_SEETARGET) )
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
		an = R_PointToAngle (actor->x, actor->y, 
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

void A_Look (mobj_t *actor)
{
	mobj_t		*targ;
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];
	
/* if current target is visible, start attacking */

/* if the sector has a living soundtarget, make that the new target */
	actor->threshold = 0;		/* any shot will wake up */
	targ = SPTR_TO_LPTR(actor->subsector->sector->soundtarget);
	if (targ && (targ->flags & MF_SHOOTABLE))
	{
		/* ambush guys will turn around on a shot */
		if ((actor->flags & (MF_AMBUSH|MF_SEETARGET)) == MF_AMBUSH)
		{
			if (actor->target != targ)
			{
				actor->target = targ;
				return;
			}
		}
		else
		{
			goto seeyou;
		}
	}

	if (!P_LookForPlayers (actor, false) )
		return;
	
seeyou:
/* go into chase state */
	if (ainfo->seesound)
	{
		int		sound;
		
		switch (ainfo->seesound)
		{
		case sfx_posit1:
		case sfx_posit2:
		case sfx_posit3:
#ifdef MARS
			if (S_sfx[sfx_posit3].lump > 0)
				sound = sfx_posit1+(P_Random()%3);
			else
#endif
				sound = sfx_posit1+(P_Random()&1);
			break;
		case sfx_bgsit1:
		case sfx_bgsit2:
			sound = sfx_bgsit1+(P_Random()&1);
			break;
		default:
			sound = ainfo->seesound;
			break;
		}

		if (actor->type == MT_SPIDER
			|| actor->type == MT_CYBORG)
		{
			// full volume
			S_StartSound (NULL, sound);
		}
		else
			S_StartSound (actor, sound);
	}

	P_SetMobjState (actor, ainfo->seestate);
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

void A_Chase (mobj_t *actor)
{
	int		delta;
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];
	
	if (actor->reactiontime)
		actor->reactiontime--;
				
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

	if (!actor->target || !(actor->target->flags&MF_SHOOTABLE)
		|| (netgame && !actor->threshold && !(actor->flags & MF_SEETARGET) 
			&& actor->target != SPTR_TO_LPTR(actor->subsector->sector->soundtarget)))
	{	/* look for a new target */
		if (P_LookForPlayers(actor,true))
			return;		/* got a new target */
		P_SetMobjState (actor, ainfo->spawnstate);
		return;
	}

/* */
/* don't attack twice in a row */
/* */
	if (actor->flags & MF_JUSTATTACKED)
	{
		actor->flags &= ~MF_JUSTATTACKED;
		P_NewChaseDir (actor);
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
	if ( (gameskill == sk_nightmare || !actor->movecount) && ainfo->missilestate
	&& P_CheckMissileRange (actor))
	{
		P_SetMobjState (actor, ainfo->missilestate);
		if (gameskill != sk_nightmare)
			actor->flags |= MF_JUSTATTACKED;
		return;
	}
	
/* */
/* chase towards player */
/* */
	if (--actor->movecount<0 || !P_Move (actor))
		P_NewChaseDir (actor);
		
/* */
/* make active sound */
/* */
	if (ainfo->activesound && P_Random () < 3)
		S_StartSound (actor, ainfo->activesound);
}

/*============================================================================= */

/*
==============
=
= A_FaceTarget
=
==============
*/

void A_FaceTarget (mobj_t *actor)
{	
	if (!actor->target)
		return;
	actor->flags &= ~MF_AMBUSH;
	
	actor->angle = R_PointToAngle (actor->x, actor->y
	, actor->target->x, actor->target->y);

	if (actor->target->flags & MF_SHADOW)
		actor->angle += (P_Random()-P_Random()) << 21;
}


/*
==============
=
= A_PosAttack
=
==============
*/

void A_PosAttack (mobj_t *actor)
{
	int		angle, damage;
	lineattack_t la;
	
	if (!actor->target)
		return;
		
	A_FaceTarget (actor);
	angle = actor->angle;

	S_StartSound (actor, sfx_pistol);
	angle += (P_Random()-P_Random())<<20;
	damage = ((P_Random()&7)+1)*3;
	P_LineAttack (&la, actor, angle, MISSILERANGE, D_MAXINT, damage);
}

void A_SPosAttack (mobj_t *actor)
{
	int		i;
	int		angle, bangle, damage;
	lineattack_t la;

	if (!actor->target)
		return;

	S_StartSound (actor, sfx_shotgn);
	A_FaceTarget (actor);
	bangle = actor->angle;

	for (i=0 ; i<3 ; i++)
	{
		angle = bangle + ((P_Random()-P_Random())<<20);
		damage = ((P_Random()&7)+1)*3;
		P_LineAttack (&la, actor, angle, MISSILERANGE, D_MAXINT, damage);
	}
}

void A_CPosAttack (mobj_t* actor)
{
	int		angle;
	int		bangle;
	int		damage;
	int		slope;
	lineattack_t	la;

	if (!actor->target)
		return;

	S_StartSound (actor, sfx_shotgn);
	A_FaceTarget (actor);
	bangle = actor->angle;
	slope = P_AimLineAttack (&la, actor, bangle, MISSILERANGE);

	angle = bangle + ((P_Random()-P_Random())<<20);
	damage = ((P_Random()%5)+1)*3;
	P_LineAttack (&la, actor, angle, MISSILERANGE, slope, damage);
}

void A_CPosRefire (mobj_t* actor)
{
	// keep firing unless target got out of sight
	A_FaceTarget (actor);
	if (P_Random () < 40)
		return;
	if (!actor->target || actor->target->health <= 0|| !(actor->flags&MF_SEETARGET) )
		P_SetMobjState (actor, mobjinfo[actor->type].seestate);
}

void A_SpidRefire (mobj_t *actor)
{	
	/* keep firing unless target got out of sight */
	A_FaceTarget (actor);
	if (P_Random () < 10)
		return;
	if (!actor->target || actor->target->health <= 0 || !(actor->flags&MF_SEETARGET) )
		P_SetMobjState (actor, mobjinfo[actor->type].seestate);
}

void A_BspiAttack (mobj_t *actor)
{
	if (!actor->target)
		return;
	A_FaceTarget (actor);
	// launch a missile
	P_SpawnMissile (actor, actor->target, MT_ARACHPLAZ);
}

/*
==============
=
= A_TroopAttack
=
==============
*/

void A_TroopAttack (mobj_t *actor)
{
	int		damage;
	
	if (!actor->target)
		return;
		
	A_FaceTarget (actor);
	if (P_CheckMeleeRange (actor))
	{
		S_StartSound (actor, sfx_claw);
		damage = ((P_Random()&7)+1)*3;
		P_DamageMobj (actor->target, actor, actor, damage);
		return;
	}
/* */
/* launch a missile */
/* */
	P_SpawnMissile (actor, actor->target, MT_TROOPSHOT);
}


void A_SargAttack (mobj_t *actor)
{
	int		damage;
	lineattack_t la;

	if (!actor->target)
		return;
		
	A_FaceTarget (actor);
	damage = ((P_Random()&7)+1)*4;
	P_LineAttack (&la, actor, actor->angle, MELEERANGE, 0, damage);
}

void A_HeadAttack (mobj_t *actor)
{
	int		damage;
	
	if (!actor->target)
		return;
		
	A_FaceTarget (actor);
	if (P_CheckMeleeRange (actor))
	{
		damage = ((P_Random()&7)+1)*8;
		P_DamageMobj (actor->target, actor, actor, damage);
		return;
	}
/* */
/* launch a missile */
/* */
	P_SpawnMissile (actor, actor->target, MT_HEADSHOT);
}

void A_CyberAttack (mobj_t *actor)
{	
	if (!actor->target)
		return;
		
	A_FaceTarget (actor);
	P_SpawnMissile (actor, actor->target, MT_ROCKET);
}

void A_BruisAttack (mobj_t *actor)
{
	int		damage;
	
	if (!actor->target)
		return;
		
	if (P_CheckMeleeRange (actor))
	{
		S_StartSound (actor, sfx_claw);
		damage = ((P_Random()&7)+1)*10;
		P_DamageMobj (actor->target, actor, actor, damage);
		return;
	}
/* */
/* launch a missile */
/* */
	P_SpawnMissile (actor, actor->target, MT_BRUISERSHOT);
}


//
// A_SkelMissile
//
void A_SkelMissile (mobj_t* actor)
{	
	mobj_t*	mo;

	if (!actor->target)
		return;

	A_FaceTarget (actor);
	actor->z += 16*FRACUNIT;	// so missile spawns higher
	mo = P_SpawnMissile (actor, actor->target, MT_TRACER);
	actor->z -= 16*FRACUNIT;	// back to normal

	mo->x += mo->momx;
	mo->y += mo->momy;
	mo->extradata = (uintptr_t)actor->target;
}

const int TRACEANGLE = 0xc000000;

void A_Tracer (mobj_t *actor)
{
	angle_t	exact;
	fixed_t	dist;
	fixed_t	slope;
	mobj_t*	dest;
	mobj_t*	th;
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];

	if (gametic & 3)
		return;

	// spawn a puff of smoke behind the rocket
	P_SpawnPuff (actor->x, actor->y, actor->z, 0);

	th = P_SpawnMobj (actor->x-actor->momx, actor->y-actor->momy, actor->z, MT_SMOKE);

	th->momz = FRACUNIT;
	th->tics -= P_Random()&3;
	if (th->tics < 1)
		th->tics = 1;

	// adjust direction
	dest = (void*)actor->extradata;

	if (!dest || dest->health <= 0)
		return;

	// change angle	
	exact = R_PointToAngle (actor->x, actor->y, dest->x, dest->y);

	if (exact != actor->angle)
	{
		if (exact - actor->angle > 0x80000000)
		{
			actor->angle -= TRACEANGLE;
			if (exact - actor->angle < 0x80000000)
				actor->angle = exact;
		}
		else
		{
			actor->angle += TRACEANGLE;
			if (exact - actor->angle > 0x80000000)
				actor->angle = exact;
		}
	}

	exact = actor->angle>>ANGLETOFINESHIFT;
	actor->momx = FixedMul (ainfo->speed, finecosine(exact));
	actor->momy = FixedMul (ainfo->speed, finesine(exact));

	// change slope
	dist = P_AproxDistance (dest->x - actor->x, dest->y - actor->y);

	dist = dist / ainfo->speed;

	if (dist < 1)
		dist = 1;
	slope = (dest->z+40*FRACUNIT - actor->z) / dist;

	if (slope < actor->momz)
		actor->momz -= FRACUNIT/8;
	else
		actor->momz += FRACUNIT/8;
	}


void A_SkelWhoosh (mobj_t *actor)
{
	if (!actor->target)
		return;
	A_FaceTarget (actor);
	S_StartSound (actor,sfx_skeswg);
}

void A_SkelFist (mobj_t *actor)
{
	int		damage;

	if (!actor->target)
		return;

	A_FaceTarget (actor);

	if (P_CheckMeleeRange (actor))
	{
		damage = ((P_Random()%10)+1)*6;
		S_StartSound (actor, sfx_skepch);
		P_DamageMobj (actor->target, actor, actor, damage);
	}
}

//
// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it. 
//
#define	FATSPREAD	(ANG90/8)

void A_FatRaise (mobj_t *actor)
{
    A_FaceTarget (actor);
    S_StartSound (actor, sfx_manatk);
}

void A_FatAttack1 (mobj_t* actor)
{
    mobj_t*	mo;
    int		an;
	const mobjinfo_t* moinfo;

    A_FaceTarget (actor);
    // Change direction  to ...
    actor->angle += FATSPREAD;
    P_SpawnMissile (actor, actor->target, MT_FATSHOT);

    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle += FATSPREAD;
    an = mo->angle >> ANGLETOFINESHIFT;
	moinfo = &mobjinfo[mo->type];
    mo->momx = FixedMul (moinfo->speed, finecosine(an));
    mo->momy = FixedMul (moinfo->speed, finesine(an));
}

void A_FatAttack2 (mobj_t* actor)
{
    mobj_t*	mo;
    int		an;
	const mobjinfo_t* moinfo;

    A_FaceTarget (actor);
    // Now here choose opposite deviation.
    actor->angle -= FATSPREAD;
    P_SpawnMissile (actor, actor->target, MT_FATSHOT);

    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle -= FATSPREAD*2;
    an = mo->angle >> ANGLETOFINESHIFT;
	moinfo = &mobjinfo[mo->type];
    mo->momx = FixedMul (moinfo->speed, finecosine(an));
    mo->momy = FixedMul (moinfo->speed, finesine(an));
}

void A_FatAttack3 (mobj_t*	actor)
{
    mobj_t*	mo;
    int		an;
	const mobjinfo_t* moinfo;

    A_FaceTarget (actor);
    
    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle -= FATSPREAD/2;
    an = mo->angle >> ANGLETOFINESHIFT;
	moinfo = &mobjinfo[mo->type];
    mo->momx = FixedMul (moinfo->speed, finecosine(an));
    mo->momy = FixedMul (moinfo->speed, finesine(an));

    mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
    mo->angle += FATSPREAD/2;
    an = mo->angle >> ANGLETOFINESHIFT;
	moinfo = &mobjinfo[mo->type];
    mo->momx = FixedMul (moinfo->speed, finecosine(an));
    mo->momy = FixedMul (moinfo->speed, finesine(an));
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

#define	SKULLSPEED		(40*FRACUNIT)

void A_SkullAttack (mobj_t *actor)
{
	mobj_t			*dest;
	angle_t			an;
	int				dist;
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];

	if (!actor->target)
		return;
		
	dest = actor->target;	
	actor->flags |= MF_SKULLFLY;

	S_StartSound (actor, ainfo->attacksound);
	A_FaceTarget (actor);
	an = actor->angle >> ANGLETOFINESHIFT;
	actor->momx = FixedMul (SKULLSPEED, finecosine(an));
	actor->momy = FixedMul (SKULLSPEED, finesine(an));
	dist = P_AproxDistance (dest->x - actor->x, dest->y - actor->y);
	dist = dist / SKULLSPEED;
	if (dist < 1)
		dist = 1;
	actor->momz = (dest->z+(dest->height>>1) - actor->z) / dist;
}

void A_Scream (mobj_t *actor)
{
	int		sound;
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];

	switch (ainfo->deathsound)
	{
	case 0:
		return;
		
	case sfx_podth1:
	case sfx_podth2:
	case sfx_podth3:
#ifdef MARS
		if (S_sfx[sfx_podth3].lump > 0)
			sound = sfx_podth1 + (P_Random()%3);
		else
#endif
			sound = sfx_podth1 + (P_Random()&1);
		break;
		
	case sfx_bgdth1:
	case sfx_bgdth2:
		sound = sfx_bgdth1 + (P_Random ()&1);
		break;
	
	default:
		sound = ainfo->deathsound;
		break;
	}

	if (actor->type == MT_SPIDER
		|| actor->type == MT_CYBORG)
	{
		// full volume
		S_StartSound (NULL, sound);
	}
	else
		S_StartSound (actor, sound);
}

void A_XScream (mobj_t *actor)
{
	S_StartSound (actor, sfx_slop);	
}

void A_Pain (mobj_t *actor)
{
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];
	if (ainfo->painsound)
		S_StartSound (actor, ainfo->painsound);	
}

void A_Fall (mobj_t *actor)
{
/* actor is on ground, it can be walked over */
	actor->flags &= ~MF_SOLID;
}


/*
================
=
= A_Explode
=
================
*/

void A_Explode (mobj_t *thingy)
{
	P_RadiusAttack ( thingy, thingy->target, 128 );
}


/*
================
=
= A_BossDeath
=
= Possibly trigger special effects
================
*/

void A_BossDeath (mobj_t *mo)
{
	int 		i;
	mobj_t		*mo2;
	line_t		junk;
	int			mapspecials = gamemapinfo->specials;
		
	if (mo->type == MT_BRUISER && !(mapspecials & MI_BARON_SPECIAL))
		return;			/* bruisers apear on other levels */
	if (mo->type == MT_CYBORG && !(mapspecials & MI_CYBER_SPECIAL))
		return;
	if (mo->type == MT_SPIDER && !(mapspecials & MI_SPIDER_SPECIAL))
		return;
	if (mo->type == MT_FATSO && !(mapspecials & MI_FATSO_SPECIAL))
		return;
	if (mo->type == MT_BABY && !(mapspecials & MI_BABY_SPECIAL))
		return;

    // make sure there is a player alive for victory
    for (i=0 ; i<MAXPLAYERS ; i++)
		if (playeringame[i] && players[i].health > 0)
			break;
    if (i == MAXPLAYERS)
		return;

/* */
/* scan the remaining thinkers to see if all bosses are dead */
/*	 */

/* FIXME */
	for (mo2=mobjhead.next ; mo2 != (void *)&mobjhead ; mo2=mo2->next)
	{
		if (mo2 != mo && mo2->type == mo->type && mo2->health > 0)
			return;		/* other boss not dead */
	}
	
/* */
/* victory! */
/* */
	switch (mo->type) {
		case MT_FATSO:
		case MT_BRUISER:
			EV_DoFloorTag (&junk, lowerFloorToLowest, 666);
			break;
		case MT_BABY:
			EV_DoFloorTag (&junk, raiseToTexture, 667);
			break;
		case MT_CYBORG:
			if (mapspecials & MI_CYBER_SPECIAL2)
			{
				EV_DoDoorTag (&junk, blazeOpen, 666);
			}
			else
			{
				G_ExitLevel();
			}
			break;
		case MT_SPIDER:
			if (mapspecials & MI_SPIDER_SPECIAL2)
			{
				EV_DoFloorTag (&junk, lowerFloorToLowest, 666);
			}
			else
			{
				G_ExitLevel();
			}
			return;
	}
}


void A_Hoof (mobj_t *mo)
{
	S_StartSound (mo, sfx_hoof);
	A_Chase (mo);
}

void A_Metal (mobj_t *mo)
{
	S_StartSound (mo, sfx_metal);
	A_Chase (mo);
}

void A_BabyMetal (mobj_t* mo)
{
	S_StartSound (mo, sfx_bspwlk);
	A_Chase (mo);
}

#define MAXBRAINTARGETS 16
SPTR braintargets[MAXBRAINTARGETS];
VINT numbraintargets;
VINT braintargeton = 0;

void A_BrainAwake(mobj_t *mo)
{
	mobj_t *m;

	// find all the target spots
	numbraintargets = 0;
	braintargeton = 0;

	for (m=mobjhead.next ; m != (void *)&mobjhead ; m=m->next)
	{
		if (m->type == MT_BOSSTARGET)
		{
			if (numbraintargets == MAXBRAINTARGETS)
				break;
			braintargets[numbraintargets] = LPTR_TO_SPTR(m);
			numbraintargets++;
		}
	}

	//S_StartSound(NULL, sfx_bossit);
}

void A_BrainPain (mobj_t*	mo)
{
    S_StartSound (NULL,sfx_bospn);
}

void A_BrainScream (mobj_t*	mo)
{
	int		x;
	int		y;
	int		z;
	mobj_t*	th;

	for (x=mo->x - 196*FRACUNIT ; x< mo->x + 320*FRACUNIT ; x+= FRACUNIT*8)
	{
		y = mo->y - 320*FRACUNIT;
		z = 128 + P_Random()*2*FRACUNIT;
		th = P_SpawnMobj (x,y,z, MT_ROCKET);
		th->momz = P_Random()*512;

		P_SetMobjState (th, S_BRAINEXPLODE1);

		th->tics -= P_Random()&3;
		if (th->tics < 1)
			th->tics = 1;
	}
}

void A_BrainExplode (mobj_t* mo)
{
	int		x;
	int		y;
	int		z;
	mobj_t*	th;

	x = mo->x + (P_Random()-P_Random()) * 2048;
	y = mo->y;
	z = 128 + P_Random()*2*FRACUNIT;
	th = P_SpawnMobj (x,y,z, MT_ROCKET);
	th->momz = P_Random()*512;

	P_SetMobjState (th, S_BRAINEXPLODE1);

	th->tics -= P_Random()&3;
	if (th->tics < 1)
		th->tics = 1;

	S_StartSound (NULL,sfx_bosdth);
}

void A_BrainDie (mobj_t *mo)
{
	G_ExitLevel ();
}

void A_BrainSpit(mobj_t *mo)
{
	mobj_t *targ;
	mobj_t *newmobj;
	int tics, vanillatics;
	int alivecnt;
	mobj_t *corpse;
	static VINT easy = 0;

	easy ^= 1;
	if (gameskill <= sk_easy && (!easy))
		return;

	alivecnt = 0;
	corpse = NULL;

	{
		mobj_t *m;
		for (m=mobjhead.next ; m != (void *)&mobjhead ; m=m->next)
		{
			if (m->flags & MF_COUNTCUBE)
			{
				if (m->health <= 0)
					corpse = m;
				else
					alivecnt++;
			}
		}
	}

	// stop spawning if there's too many monsters alive
	if (alivecnt >= numbraintargets*2)
		return;

	// shoot a cube at current target
	targ = SPTR_TO_LPTR(braintargets[braintargeton]);
	if (numbraintargets == 0)
		I_Error("A_BrainSpit: numbraintargets == 0");
	braintargeton = (braintargeton + 1) % numbraintargets;

	// remove the oldest monster corpse
	if (corpse)
		P_RemoveMobj(corpse);

	// spawn brain missile
	newmobj = P_SpawnMissile(mo, targ, MT_SPAWNSHOT);
	if (!(newmobj->flags & MF_MISSILE))
		return;

	vanillatics = states[S_SPAWN1].tics + states[S_SPAWN2].tics;
	tics = states[S_SPAWN1].tics > states[S_SPAWN2].tics ? states[S_SPAWN1].tics : states[S_SPAWN2].tics;

	newmobj->reactiontime = ((targ->y - mo->y) / newmobj->momy) * tics / vanillatics;
	newmobj->target = targ;

	S_StartSound(NULL, sfx_bospit);
}

// travelling cube sound
void A_SpawnSound(mobj_t *mo)
{
	S_StartSound(mo, sfx_boscub);
	A_SpawnFly(mo);
}

void A_SpawnFly(mobj_t *mo)
{
	mobj_t *newmobj;
	mobj_t *fog;
	mobj_t *targ;
	int r;
	mobjtype_t type;

	if (--mo->reactiontime)
		return; // still flying

	targ = mo->target;

	// First spawn teleport fog.
	fog = P_SpawnMobj(targ->x, targ->y, targ->z, MT_TFOG);
	S_StartSound(fog, sfx_telept);

	// Randomly select monster to spawn.
	r = P_Random();

	// Probability distribution (kind of :),
	// decreasing likelihood.
	if (r < 50)
		type = MT_TROOP;
	else if (r < 90)
		type = MT_SERGEANT;
	else if (r < 120)
		type = MT_SHADOWS;
	else if (r < 130)
		type = MT_HEAD;
	else if (r < 172)
		type = MT_UNDEAD;
	else if (r < 192)
		type = MT_BABY;
	else if (r < 222)
		type = MT_FATSO;
	else if (r < 246)
		type = MT_KNIGHT;
	else
		type = MT_BRUISER;

	newmobj = P_SpawnMobj(targ->x, targ->y, targ->z, type);
	if (P_LookForPlayers(newmobj, true))
	{
		const mobjinfo_t* ainfo = &mobjinfo[type];
		P_SetMobjState(newmobj, ainfo->seestate);
	}

	// telefrag anything in this spot
	newmobj->flags |= MF_COUNTCUBE;
	newmobj->flags = (newmobj->flags & ~MF_SHOOTABLE) | MF_TELEPORT;
	P_Telefrag (newmobj, targ->x, targ->y);
	newmobj->flags = (newmobj->flags & ~MF_TELEPORT) | MF_SHOOTABLE;

	// remove self (i.e., cube).
	P_RemoveMobj(mo);
}

//
// A_KeenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void A_KeenDie (mobj_t* mo)
{
    mobj_t*	mo2;
    line_t	junk;

    A_Fall (mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
	for (mo2=mobjhead.next ; mo2 != (void *)&mobjhead ; mo2=mo2->next)
	{
		if (mo2 != mo && mo2->type == mo->type && mo2->health > 0)
			return;		/* other Keen not dead */
	}

    EV_DoDoorTag(&junk, open, 666);
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
	
	mo->flags &= ~MF_SKULLFLY;
	mo->momx = mo->momy = mo->momz = 0;
	P_SetMobjState (mo, moinfo->spawnstate);
}

/*  */
