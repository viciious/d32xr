/* P_mobj.c */

#include "doomdef.h"
#include "p_local.h"
#include "sounds.h"

void G_PlayerReborn (int player);

/*
===============
=
= P_AddMobjToList
=
===============
*/
static void P_AddMobjToList (mobj_t *mobj_, mobj_t *head_)
{
	degenmobj_t *mobj = (void*)mobj_, *head = (void *)head_;
	((degenmobj_t *)head->prev)->next = mobj;
	mobj->next = head;
	mobj->prev = head->prev;
	head->prev = mobj;
}

static void P_RemoveMobjFromCurrList (mobj_t *mobj_)
{
	degenmobj_t *mobj = (void*)mobj_;
	((degenmobj_t *)mobj->next)->prev = mobj->prev;
	((degenmobj_t *)mobj->prev)->next = mobj->next;
}

/*
===============
=
= P_RemoveMobj
=
===============
*/

void P_RemoveMobj (mobj_t *mobj)
{
/* unlink from sector and block lists */
	P_UnsetThingPosition (mobj);

	if (mobj->flags & MF_RINGMOBJ)
	{
		// unlink from mobj list
		P_RemoveMobjFromCurrList(mobj);
		P_AddMobjToList(mobj, (void*)&freeringmobjhead);
		return;

	}
	else if (mobj->flags & MF_STATIC)
	{
		/* unlink from mobj list */
		P_RemoveMobjFromCurrList(mobj);
		P_AddMobjToList(mobj, (void*)&freestaticmobjhead);
		return;
	}

	mobj->target = NULL;
	mobj->extradata = 0;
	mobj->latecall = (latecall_t)-1;	/* make sure it doesn't come back to life... */

/* unlink from mobj list */
	P_RemoveMobjFromCurrList(mobj);
/* link to limbo mobj list */
	P_AddMobjToList(mobj, (void*)&limbomobjhead);
}

void P_FreeMobj(mobj_t* mobj)
{
/* unlink from mobj list */
	P_RemoveMobjFromCurrList(mobj);
/* link to limbo mobj list */
	P_AddMobjToList(mobj, (void*)&freemobjhead);
}

boolean P_IsObjectOnGround(mobj_t *mobj)
{
	if (mobj->player)
	{
		const player_t *player = &players[mobj->player - 1];
		if (player->pflags & PF_VERTICALFLIP)
		{
			if (mobj->z + (mobj->theight << FRACBITS) >= mobj->ceilingz)
				return true;
		}
		else if (mobj->z <= mobj->floorz)
			return true;
		else
			return false;
	}

	return mobj->z <= mobj->floorz;
}

int8_t P_MobjFlip(mobj_t *mo)
{
	if (mo->player && players[mo->player-1].pflags & PF_VERTICALFLIP)
		return -1;

	return 1;
}

/*
================
=
= P_SetMobjState
=
= Returns true if the mobj is still present
================
*/

boolean P_SetMobjState (mobj_t *mobj, statenum_t state)
{
	uint16_t changes = 0xffff;

	do {
		const state_t* st;

		if (state == S_NULL)
		{
			mobj->state = S_NULL;
			P_RemoveMobj(mobj);
			return false;
		}

		st = &states[state];
		mobj->state = state;
		mobj->tics = st->tics;
		mobj->sprite = st->sprite;
		mobj->frame = st->frame;

		if (st->action)		/* call action functions when the state is set */
			st->action(mobj);

		if (!(mobj->flags & (MF_RINGMOBJ|MF_STATIC)))
			mobj->latecall = NULL;	/* make sure it doesn't come back to life... */

		state = st->nextstate;
	} while (!mobj->tics && --changes > 0);

	return true;
}

/* 
=================== 
= 
= P_ExplodeMissile  
=
=================== 
*/ 

void P_ExplodeMissile (mobj_t *mo)
{
	const mobjinfo_t* info = &mobjinfo[mo->type];

	mo->momx = mo->momy = mo->momz = 0;
	P_SetMobjState (mo, mobjinfo[mo->type].deathstate);
	mo->tics -= P_Random()&1;
	if (mo->tics < 1)
		mo->tics = 1;
	mo->flags &= ~MF_MISSILE;
	if (info->deathsound)
		S_StartSound (mo, info->deathsound);
}


/*
===============
=
= P_SpawnMobj
=
===============
*/

mobj_t *P_SpawnMobj (fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
	mobj_t		*mobj;
	const state_t		*st;
	const mobjinfo_t *info = &mobjinfo[type];

/* try to reuse a previous mobj first */
	if (info->flags & MF_RINGMOBJ)
	{
		if (info->flags & MF_NOBLOCKMAP)
		{
			scenerymobj_t *scenerymobj = &scenerymobjlist[numscenerymobjs];
			scenerymobj->type = type;
			scenerymobj->x = x >> FRACBITS;
			scenerymobj->y = y >> FRACBITS;
			scenerymobj->flags = info->flags;

			/* set subsector and/or block links */
			P_SetThingPosition2 ((mobj_t*)scenerymobj, R_PointInSubsector(x, y));

			const fixed_t floorz = scenerymobj->subsector->sector->floorheight;
			const fixed_t ceilingz = scenerymobj->subsector->sector->ceilingheight;
			if (z == ONFLOORZ)
				scenerymobj->z = floorz >> FRACBITS;
			else if (z == ONCEILINGZ)
				scenerymobj->z = (ceilingz - info->height) >> FRACBITS;
			else 
				scenerymobj->z = z >> FRACBITS;

			numscenerymobjs++;

			return (mobj_t*)scenerymobj;
		}

		if (freeringmobjhead.next != (void*)&freeringmobjhead)
		{
			mobj = freeringmobjhead.next;
			P_RemoveMobjFromCurrList(mobj);
		}
		else
			mobj = Z_Malloc(ring_mobj_size, PU_LEVEL);
		
		D_memset(mobj, 0, ring_mobj_size);

		mobj->type = type;
		mobj->x = x;
		mobj->y = y;
		mobj->flags = info->flags;

		/* set subsector and/or block links */
		P_SetThingPosition2 (mobj, R_PointInSubsector(x, y));
		
		const fixed_t floorz = mobj->subsector->sector->floorheight;
		const fixed_t ceilingz = mobj->subsector->sector->ceilingheight;
		if (z == ONFLOORZ)
			mobj->z = floorz;
		else if (z == ONCEILINGZ)
			mobj->z = ceilingz - info->height;
		else 
			mobj->z = z;
			
		P_AddMobjToList(mobj, (void *)&mobjhead);

		return mobj;
	}
	else if (info->flags & MF_STATIC)
	{
		if (freestaticmobjhead.next != (void *)&freestaticmobjhead)
		{
			mobj = freestaticmobjhead.next;
			P_RemoveMobjFromCurrList(mobj);
		}
		else
		{
			mobj = Z_Malloc (static_mobj_size, PU_LEVEL);
		}
		D_memset (mobj, 0, static_mobj_size);
	}
	else
	{
		if (freemobjhead.next != (void *)&freemobjhead)
		{
			mobj = freemobjhead.next;
			P_RemoveMobjFromCurrList(mobj);
		}
		else
		{
			mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL);
		}
		D_memset (mobj, 0, sizeof (*mobj));
	}

	mobj->type = type;
	mobj->x = x;
	mobj->y = y;
	mobj->theight = info->height >> FRACBITS;
	mobj->flags = info->flags;
	mobj->health = info->spawnhealth;

/* do not set the state with P_SetMobjState, because action routines can't */
/* be called yet */
	st = &states[info->spawnstate];

	mobj->state = info->spawnstate;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;

/* set subsector and/or block links */
	P_SetThingPosition (mobj);
	
	mobj->floorz = mobj->subsector->sector->floorheight;
	mobj->ceilingz = mobj->subsector->sector->ceilingheight;
	if (z == ONFLOORZ)
		mobj->z = mobj->floorz;
	else if (z == ONCEILINGZ)
		mobj->z = mobj->ceilingz - info->height;
	else 
		mobj->z = z;
	
/* */
/* link into the mobj list */
/* */
	if (mobj->flags & MF_RINGMOBJ)
		P_AddMobjToList(mobj, (void *)&ringmobjhead);
	else
		P_AddMobjToList(mobj, (void *)&mobjhead);

	return mobj;
}


/*
================
=
= P_PreSpawnMobjs
=
================
*/
void P_PreSpawnMobjs(int count, int staticcount, int ringcount, int scenerycount)
{
	if (scenerycount > 0)
	{
		scenerymobjlist = Z_Malloc(sizeof(scenerymobj_t)*scenerycount, PU_LEVEL);
		numscenerymobjs = 0;
	}

	if (staticcount > 0)
	{
		uint8_t *mobj = Z_Malloc (static_mobj_size*staticcount, PU_LEVEL);
		for (; staticcount > 0; staticcount--) {
			P_AddMobjToList((void *)mobj, (void *)&freestaticmobjhead);
			mobj += static_mobj_size;
		}
	}

	if (count > 0)
	{
		mobj_t *mobj = Z_Malloc (sizeof(*mobj)*count, PU_LEVEL);
		for (; count > 0; count--) {
			P_AddMobjToList(mobj, (void *)&freemobjhead);
			mobj++;
		}
	}

	if (ringcount > 0)
	{
		uint8_t *mobj = Z_Malloc (ring_mobj_size*ringcount, PU_LEVEL);
		for (; ringcount > 0; ringcount--) {
			P_AddMobjToList((void*)mobj, (void*)&freeringmobjhead);
			mobj += ring_mobj_size;
		}
	}
}

/*============================================================================= */


/*
============
=
= P_SpawnPlayer
=
= Called when a player is spawned on the level 
= Most of the player structure stays unchanged between levels
============
*/

void P_SpawnPlayer (mapthing_t *mthing)
{
	player_t	*p;
	fixed_t		x,y,z;
	mobj_t		*mobj;

	if (!playeringame[mthing->type-1])
		return;						/* not playing */
		
	p = &players[mthing->type-1];

	if (p->playerstate == PST_REBORN)
		G_PlayerReborn (mthing->type-1);

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;
#if 0
if (mthing->type==1)
{
x = 0xffb00000;
y = 0xff500000;
}
#endif
	z = ONFLOORZ;
	mobj = P_SpawnMobj (x,y,z, MT_PLAYER);
	
	mobj->angle = ANG45 * (mthing->angle/45);

	mobj->player = p - players + 1;
	mobj->health = p->health;
	p->mo = mobj;
	p->playerstate = PST_LIVE;	
	p->message = NULL;
	p->damagecount = 0;
	p->bonuscount = 0;
	p->extralight = 0;
	p->viewheight = VIEWHEIGHT;
	
	if (!netgame)
		return;
}


/*
=================
=
= P_MapThingSpawnsMobj
=
= Returns false for things that don't spawn a mobj
==================
*/
int P_MapThingSpawnsMobj (mapthing_t* mthing)
{
	int			i;

/* count deathmatch start positions */
	if (mthing->type == 11)
		return 0;

/* check for players specially */
#if 0
	if (mthing->type > 4)
		return false;	/*DEBUG */
#endif

	if (mthing->type <= 4)
		return 0;

/* check for apropriate skill level */
	if ((netgame != gt_deathmatch) && (mthing->options & 16))
		return 0;

/* find which type to spawn */
	{
		for (i = 0; i < NUMMOBJTYPES; i++)
		{
			if (mthing->type == mobjinfo[i].doomednum)
			{
				if (mobjinfo[i].flags & MF_RINGMOBJ)
				{
					if (mobjinfo[i].flags & MF_NOBLOCKMAP)
						return 4;

					return 3;
				}
				if (mobjinfo[i].flags & MF_STATIC)
					return 2;
				return 1;
			}
		}
	}

	return 0;
}

/*
=================
=
= P_SpawnMapThing
=
= The fields of the mapthing should already be in host byte order
==================
*/

void P_SpawnMapThing (mapthing_t *mthing, int thingid)
{
	int			i;
	mobj_t		*mobj;
	fixed_t		x,y,z;
		
/* count deathmatch start positions */
	if (mthing->type == 11)
	{
		if (deathmatch_p < deathmatchstarts + MAXDMSTARTS)
			D_memcpy (deathmatch_p, mthing, sizeof(*mthing));
		deathmatch_p++;
		return;
	}
	
/* check for players specially */

#if 0
if (mthing->type > 4)
return;	/*DEBUG */
#endif

	if (mthing->type <= 4)
	{
		/* save spots for respawning in network games */
		if (mthing->type <= MAXPLAYERS)
		{
			D_memcpy (&playerstarts[mthing->type-1], mthing, sizeof(*mthing));
			if (netgame != gt_deathmatch)
				P_SpawnPlayer (mthing);
		}
		return;
	}

	if (!P_MapThingSpawnsMobj(mthing))
		return;

/* find which type to spawn */
	for (i=0 ; i< NUMMOBJTYPES ; i++)
		if (mthing->type == mobjinfo[i].doomednum)
			break;
	
	if (i==NUMMOBJTYPES)
		I_Error ("P_SpawnMapThing: Unknown type %i at (%i, %i)",mthing->type
		, mthing->x, mthing->y);

	if (mobjinfo[i].flags & MF_RINGMOBJ)
	{
		ringmobjstates[i] = mobjinfo[i].spawnstate;
		ringmobjtics[i] = states[mobjinfo[i].spawnstate].tics;
	}
	
/* spawn it */

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;
	if (mobjinfo[i].flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else
		z = ONFLOORZ;
	mobj = P_SpawnMobj (x,y,z, i);
	if (mobj->type == MT_RING)
	{
		totalitems++;
		return;
	}
	if ((mobj->flags & MF_RINGMOBJ) && (mobj->flags & MF_NOBLOCKMAP))
		return;

	if (mobj->tics > 0)
		mobj->tics = 1 + (P_Random () % mobj->tics);
		
	mobj->angle = ANG45 * (mthing->angle/45);
	if (mobj->flags & MF_STATIC)
		return;

	if (mthing->options & MTF_AMBUSH)
		mobj->flags |= MF_AMBUSH;
}


/*
===============================================================================

						GAME SPAWN FUNCTIONS

===============================================================================
*/

/*
================
=
= P_SpawnPuff
=
================
*/

void P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z, fixed_t attackrange)
{
	// TODO: Umm.. no.
}


/*
================
=
= P_SpawnBlood
=
================
*/

void P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage)
{
	// TODO: Umm.. no.
}

/*
================
=
= P_CheckMissileSpawn
=
= Moves the missile forward a bit and possibly explodes it right there
=
================
*/

void P_CheckMissileSpawn (mobj_t *th)
{
	ptrymove_t tm;

	th->x += (th->momx>>1);
	th->y += (th->momy>>1);	/* move a little forward so an angle can */
							/* be computed if it immediately explodes */
	th->z += (th->momz>>1);
	if (!P_TryMove (&tm, th, th->x, th->y))
		P_ExplodeMissile (th);
}

/*
================
=
= P_SpawnMissile
=
================
*/

void P_SpawnMissile (mobj_t *source, mobj_t *dest, mobjtype_t type)
{
	mobj_t		*th;
	angle_t		an;
	int			dist;
	int			speed;
	const mobjinfo_t* thinfo = &mobjinfo[type];

	th = P_SpawnMobj (source->x,source->y, source->z + 4*8*FRACUNIT, type);
	if (thinfo->seesound)
		S_StartSound (source, thinfo->seesound);
	th->target = source;		/* where it came from */
	an = R_PointToAngle2 (source->x, source->y, dest->x, dest->y);	
	th->angle = an;
	an >>= ANGLETOFINESHIFT;
	speed = mobjinfo[th->type].speed >> 16;
	th->momx = speed * finecosine(an);
	th->momy = speed * finesine(an);
	
	dist = P_AproxDistance (dest->x - source->x, dest->y - source->y);
	dist = dist / mobjinfo[th->type].speed;
	if (dist < 1)
		dist = 1;
	th->momz = (dest->z - source->z) / dist;
	P_CheckMissileSpawn (th);
}


/*
================
=
= P_SpawnPlayerMissile
=
= Tries to aim at a nearby monster
================
*/

void P_SpawnPlayerMissile (mobj_t *source, mobjtype_t type)
{
	mobj_t			*th;
	mobj_t 			*linetarget;
	angle_t			an;
	fixed_t			x,y,z, slope;
	int				speed;
	lineattack_t	la;
	const mobjinfo_t* thinfo = &mobjinfo[type];

/* */
/* see which target is to be aimed at */
/* */
	an = source->angle;
	slope = P_AimLineAttack (&la, source, an, 16*64*FRACUNIT);
	linetarget = la.shootmobj;
	if (!linetarget)
	{
		an += 1<<26;
		slope = P_AimLineAttack (&la, source, an, 16*64*FRACUNIT);
		linetarget = la.shootmobj;
		if (!linetarget)
		{
			an -= 2<<26;
			slope = P_AimLineAttack (&la, source, an, 16*64*FRACUNIT);
			linetarget = la.shootmobj;
		}
		if (!linetarget)
		{
			an = source->angle;
			slope = 0;
		}
	}
	
	x = source->x;
	y = source->y;
	z = source->z + 4*8*FRACUNIT;
	
	th = P_SpawnMobj (x,y,z, type);
	if (thinfo->seesound)
		S_StartSound (source, thinfo->seesound);
	th->target = source;
	th->angle = an;
	
	speed = mobjinfo[th->type].speed >> 16;
	
	th->momx = speed * finecosine(an>>ANGLETOFINESHIFT);
	th->momy = speed * finesine(an>>ANGLETOFINESHIFT);
	th->momz = speed * slope;

	P_CheckMissileSpawn (th);
}

