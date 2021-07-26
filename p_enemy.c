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

boolean P_CheckMeleeRange (mobj_t *actor) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
boolean P_CheckMissileRange (mobj_t *actor) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
boolean P_Move (mobj_t *actor) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
boolean P_TryWalk (mobj_t *actor) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void P_NewChaseDir (mobj_t *actor) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
boolean P_LookForPlayers (mobj_t *actor, boolean allaround) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void A_Look (mobj_t *actor) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void A_Chase (mobj_t *actor) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

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

	if (actor->type == MT_SKULL)
		dist >>= 1;

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

extern	line_t *blockline;

boolean P_Move (mobj_t *actor)
{
	fixed_t	tryx, tryy;
	boolean		good;
	line_t		*blkline;

	if (actor->movedir == DI_NODIR)
		return false;
		
	tryx = actor->x + actor->speed*xspeed[actor->movedir];
	tryy = actor->y + actor->speed*yspeed[actor->movedir];
	
	if (!P_TryMove (actor, tryx, tryy) )
	{	/* open any specials */
		if (actor->flags & MF_FLOAT && floatok)
		{	/* must adjust height */
			if (actor->z < tmfloorz)
				actor->z += FLOATSPEED;
			else
				actor->z -= FLOATSPEED;
			actor->flags |= MF_INFLOAT;
			return true;
		}
		
		blkline = (line_t *)DSPRead (&blockline);
		if (!blkline || !blkline->special)
			return false;
			
		actor->movedir = DI_NODIR;
		good = false;
			/* if the special isn't a door that can be opened, return false */
		if (P_UseSpecialLine (actor, blkline))
			good = true;
		return good;
	}
	else
		actor->flags &= ~MF_INFLOAT;
		
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
		
	olddir = actor->movedir;
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

boolean P_LookForPlayers (mobj_t *actor, boolean allaround)
{
	angle_t		an;
	fixed_t		dist;
	mobj_t		*mo;
	
	if (! (actor->flags & MF_SEETARGET) )
	{	/* pick another player as target if possible */
newtarget:
		if (playeringame[1] && actor->target == players[0].mo)
			actor->target = players[1].mo;
		else
			actor->target = players[0].mo;
		return false;
	}
		
	mo = actor->target;
	if (!mo || mo->health <= 0)
		goto newtarget;
		
	if (actor->subsector->sector->soundtarget == actor->target)
		allaround = true;		/* ambush guys will turn around on a shot */
		
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
	/* mobj_t		*targ;
        */
	const mobjinfo_t* ainfo = &mobjinfo[actor->type];
	
/* if current target is visible, start attacking */

	if (!P_LookForPlayers (actor, false) )
		return;

#if 0
/* if the sector has a living soundtarget, make that the new target */
	actor->threshold = 0;		/* any shot will wake up */
	targ = actor->subsector->sector->soundtarget;
	if (targ && (targ->flags & MF_SHOOTABLE) )
		actor->target = targ;
	return;
#endif
		
/* go into chase state */
	if (ainfo->seesound)
	{
		int		sound;
		
		switch (ainfo->seesound)
		{
		case sfx_posit1:
		case sfx_posit2:
		case sfx_posit3:
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

	if (!actor->target || !(actor->target->flags&MF_SHOOTABLE))
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
	
	actor->angle = R_PointToAngle2 (actor->x, actor->y
	, actor->target->x, actor->target->y);
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
	
	if (!actor->target)
		return;
		
	A_FaceTarget (actor);
	angle = actor->angle;

	S_StartSound (actor, sfx_pistol);
	angle += (P_Random()-P_Random())<<20;
	damage = ((P_Random()&7)+1)*3;
	P_LineAttack (actor, angle, MISSILERANGE, D_MAXINT, damage);
}

void A_SPosAttack (mobj_t *actor)
{
	int		i;
	int		angle, bangle, damage;
	
	if (!actor->target)
		return;

	S_StartSound (actor, sfx_shotgn);
	A_FaceTarget (actor);
	bangle = actor->angle;

	for (i=0 ; i<3 ; i++)
	{
		angle = bangle + ((P_Random()-P_Random())<<20);
		damage = ((P_Random()&7)+1)*3;
		P_LineAttack (actor, angle, MISSILERANGE, D_MAXINT, damage);
	}
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

	if (!actor->target)
		return;
		
	A_FaceTarget (actor);
	damage = ((P_Random()&7)+1)*4;
	P_LineAttack (actor, actor->angle, MELEERANGE, 0, damage);
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
		damage = ((P_Random()&7)+1)*11;
		P_DamageMobj (actor->target, actor, actor, damage);
		return;
	}
/* */
/* launch a missile */
/* */
	P_SpawnMissile (actor, actor->target, MT_BRUISERSHOT);
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
		sound = sfx_podth1 + (P_Random ()&1);
		break;
		
	case sfx_bgdth1:
	case sfx_bgdth2:
		sound = sfx_bgdth1 + (P_Random ()&1);
		break;
	
	default:
		sound = ainfo->deathsound;
		break;
	}
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
	mobj_t		*mo2;
	line_t		junk;
		
	if (!gamemapinfo.baronSpecial)
		return;			/* bruisers apear on other levels */
		
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
	junk.tag = 666;
	EV_DoFloor (&junk, lowerFloorToLowest);
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
