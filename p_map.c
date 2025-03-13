/* P_map.c */

#include "doomdef.h"
#include "p_local.h"

typedef struct
{
	mobj_t		*bombsource;
	mobj_t		*bombspot;
	int			bombdamage;
	fixed_t     dist;
} pradiusattack_t;

boolean PIT_RadiusAttack(mobj_t* thing, pradiusattack_t *ra);
void P_RadiusAttack(mobj_t* spot, mobj_t* source, int damage);

/*============================================================================= */



/*============================================================================= */

/*
===================
=
= P_TryMove
=
in:
tmthing		a mobj_t (can be valid or invalid)
tmx,tmy		a position to be checked (doesn't need relate to the mobj_t->x,y)

out:

newsubsec
floatok			if true, move would be ok if within tmfloorz - tmceilingz
floorz
ceilingz
tmdropoffz		the lowest point contacted (monsters won't move to a dropoff)

movething

==================
*/

boolean P_TryMove2 (ptrymove_t *tm, boolean checkposonly);

boolean P_CheckPosition (ptrymove_t *tm, mobj_t *thing, fixed_t x, fixed_t y)
{
	tm->tmthing = thing;
	tm->tmx = x;
	tm->tmy = y;
	return P_TryMove2 (tm, true);
}


boolean P_TryMove (ptrymove_t *tm, mobj_t *thing, fixed_t x, fixed_t y)
{
	tm->tmthing = thing;
	tm->tmx = x;
	tm->tmy = y;
	return P_TryMove2 (tm, false);
}

/* 
============================================================================== 

							RADIUS ATTACK

============================================================================== 
*/ 

/*
=================
=
= PIT_RadiusAttack
=
= Source is the creature that casued the explosion at spot
=================
*/

boolean PIT_RadiusAttack (mobj_t *thing, pradiusattack_t *ra)
{
	fixed_t		dx, dy, dz, dist;

	if (thing->flags & MF_RINGMOBJ)
		return true;
	
	if (!Mobj_HasFlags2(thing, MF2_SHOOTABLE))
		return true;

	dx = D_abs(thing->x - ra->bombspot->x);
	dy = D_abs(thing->y - ra->bombspot->y);
	dz = D_abs(thing->z - ra->bombspot->z);
	dist = dx>dy ? dx : dy;
	dist = (dist - mobjinfo[thing->type].radius);
	if (dist < 0)
		dist = 0;
	if (dist >= ra->dist || dz >= ra->dist)
		return true;		/* out of range */
/* FIXME?	if ( P_CheckSight (thing, bombspot) )	// must be in direct path */
		P_DamageMobj (thing, ra->bombspot, ra->bombsource, (ra->bombdamage - dist) >> FRACBITS);
	return true;
}


/*
=================
=
= P_RadiusAttack
=
= Source is the creature that casued the explosion at spot
=================
*/

void P_RadiusAttack (mobj_t *spot, mobj_t *source, int damage)
{
	int			x,y, xl, xh, yl, yh;
	pradiusattack_t ra;
	
	ra.dist = (damage+MAXRADIUS)<<FRACBITS;
	yh = spot->y + ra.dist - bmaporgy;
	yl = spot->y - ra.dist - bmaporgy;
	xh = spot->x + ra.dist - bmaporgx;
	xl = spot->x - ra.dist - bmaporgx;

	if(xl < 0)
		xl = 0;
	if(yl < 0)
		yl = 0;
	if(yh < 0)
		return;
	if(xh < 0)
		return;

    xl = (unsigned)xl >> MAPBLOCKSHIFT;
    xh = (unsigned)xh >> MAPBLOCKSHIFT;
    yl = (unsigned)yl >> MAPBLOCKSHIFT;
    yh = (unsigned)yh >> MAPBLOCKSHIFT;

   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;

	ra.bombspot = spot;
	ra.bombsource = source;
	ra.bombdamage = damage;
	
	for (y=yl ; y<=yh ; y++)
		for (x=xl ; x<=xh ; x++)
			P_BlockThingsIterator (x, y, (blockthingsiter_t)PIT_RadiusAttack, &ra );
}

#define RING_DIST (512*FRACUNIT)

boolean PIT_RingMagnet(mobj_t *thing, mobj_t *spot)
{
	if (!(thing->type == MT_RING || thing->type == MT_FLINGRING))
		return true;

	ringmobj_t *ring = (ringmobj_t*)thing;

	const fixed_t dist = P_AproxDistance(P_AproxDistance((ring->x << FRACBITS) - spot->x, (ring->y << FRACBITS) - spot->y), (ring->z << FRACBITS) - spot->z);

	if (dist > RING_DIST)
		return true;

	// Replace object with an attraction ring
	mobj_t *attractring = P_SpawnMobj(ring->x << FRACBITS, ring->y << FRACBITS, ring->z << FRACBITS, MT_ATTRACTRING);
	attractring->target = spot;
	P_RemoveMobj(thing);
	return true;
}

void P_RingMagnet(mobj_t *spot)
{
	const fixed_t		dist = RING_DIST;
	int			x,y, xl, xh, yl, yh;
	
	yh = spot->y + dist - bmaporgy;
	yl = spot->y - dist - bmaporgy;
	xh = spot->x + dist - bmaporgx;
	xl = spot->x - dist - bmaporgx;

	if(xl < 0)
		xl = 0;
	if(yl < 0)
		yl = 0;
	if(yh < 0)
		return;
	if(xh < 0)
		return;

    xl = (unsigned)xl >> MAPBLOCKSHIFT;
    xh = (unsigned)xh >> MAPBLOCKSHIFT;
    yl = (unsigned)yl >> MAPBLOCKSHIFT;
    yh = (unsigned)yh >> MAPBLOCKSHIFT;

   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;
	
	for (y=yl ; y<=yh ; y++)
		for (x=xl ; x<=xh ; x++)
			P_BlockThingsIterator(x, y, (blockthingsiter_t)PIT_RingMagnet, spot);
}


/*============================================================================ */

#ifdef JAGUAR

int	dspterminate;
int	dsppc;


int	goodreads;
int badreads;
int	doublebadreads;

int	val1, val2;

int DSPRead (void volatile *adr)
{
reread:
	val1 = *(int volatile *)adr;
	val2 = *(int volatile *)adr;
	if (val1 != val2)
		goto reread;
	return val1;
}

int DSPFunction (void *start)
{
	int samp;

	while (DSPRead (&dspfinished) != 0xdef6)
	;
	
	samp = samplecount;

	dspfinished = 0x1234;
	
	dspcodestart = (int)start;
	do
	{
		dspterminate = DSPRead(&dspfinished);
	} while (dspterminate != 0xdef6);

	return samplecount-samp;
}

#elif !defined(MARS)
int DSPRead (void volatile *adr)
{
	return *(int *)adr;
}
#endif
