/* P_map.c */

#include "doomdef.h"
#include "p_local.h"



fixed_t P_InterceptVector(divline_t* v2, divline_t* v1) ATTR_DATA_CACHE_ALIGN;
boolean	PIT_UseLines(line_t* li) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void P_UseLines(player_t* player) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
boolean PIT_RadiusAttack(mobj_t* thing) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void P_RadiusAttack(mobj_t* spot, mobj_t* source, int damage) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
fixed_t P_AimLineAttack(mobj_t* t1, angle_t angle, fixed_t distance) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void P_LineAttack(mobj_t* t1, angle_t angle, fixed_t distance, fixed_t slope, int damage) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

/*============================================================================= */

/*================== */
/* */
/* in */
/* */
/*================== */

mobj_t		*tmthing;
fixed_t		tmx, tmy;
boolean		checkposonly;


/*================== */
/* */
/* out */
/* */
/*================== */
extern	boolean		trymove2;

extern	boolean		floatok;				/* if true, move would be ok if */
											/* within tmfloorz - tmceilingz */
											
extern	fixed_t		tmfloorz, tmceilingz, tmdropoffz;

extern	mobj_t	*movething;

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

void P_TryMove2 (void);

int checkpostics;

boolean P_CheckPosition (mobj_t *thing, fixed_t x, fixed_t y)
{
	tmthing = thing;
	tmx = x;
	tmy = y;

	checkposonly = true;	
	
#ifdef JAGUAR
{
	extern	p_move_start;
	checkpostics += DSPFunction (&p_move_start);
}
#else
	P_TryMove2 ();
#endif

	return trymove2;
}


boolean P_TryMove (mobj_t *thing, fixed_t x, fixed_t y)
{
	int		damage;
	mobj_t	*latchedmovething;
	
	tmthing = thing;
	tmx = x;
	tmy = y;
	
#ifdef JAGUAR
{
	extern	int p_move_start;
	checkpostics += DSPFunction (&p_move_start);
}
#else
	P_TryMove2 ();
#endif


/* */
/* pick up the specials */
/* */
	latchedmovething =  (mobj_t *)DSPRead (&movething);
	
	if (latchedmovething)
	{
		const mobjinfo_t* thinfo = &mobjinfo[thing->type];

		if (thing->flags & MF_MISSILE)
		{	/* missile bash into a monster */
			damage = ((P_Random()&7)+1)* thinfo->damage;
			P_DamageMobj (latchedmovething, thing, thing->target, damage);
		}
		else if (thing->flags & MF_SKULLFLY)
		{	/* skull bash into a monster */
			damage = ((P_Random()&7)+1)* thinfo->damage;
			P_DamageMobj (latchedmovething, thing, thing, damage);
			thing->flags &= ~MF_SKULLFLY;
			thing->momx = thing->momy = thing->momz = 0;
			P_SetMobjState (thing, thinfo->spawnstate);
		}
		else	/* pick up  */
			P_TouchSpecialThing (latchedmovething, thing);
	}
		
	return trymove2;
}


/* 
============================================================================== 

							USE LINES

============================================================================== 
*/ 
 
int			usebbox[4];
divline_t	useline;

line_t		*closeline;
fixed_t		closedist;

/*
===============
=
= P_InterceptVector
=
= Returns the fractional intercept point along the first divline
=
===============
*/

fixed_t P_InterceptVector (divline_t *v2, divline_t *v1)
{
	fixed_t	frac, num, den;
	
	den = (v1->dy>>16)*(v2->dx>>16) - (v1->dx>>16)*(v2->dy>>16);
	if (den == 0)
		return -1;
	num = ((v1->x-v2->x)>>16) *(v1->dy>>16) + 
		((v2->y-v1->y)>>16) * (v1->dx>>16);
	frac = (num<<16) / den;

	return frac;
}


/*
================
=
= PIT_UseLines
=
================
*/

boolean	PIT_UseLines (line_t *li)
{
	divline_t	dl;
	fixed_t		frac;
	fixed_t 	*libbox = P_LineBBox(li);

/* */
/* check bounding box first */
/* */
	if (usebbox[BOXRIGHT] <= libbox[BOXLEFT]
	||	usebbox[BOXLEFT] >= libbox[BOXRIGHT]
	||	usebbox[BOXTOP] <= libbox[BOXBOTTOM]
	||	usebbox[BOXBOTTOM] >= libbox[BOXTOP] )
		return true;

/* */
/* find distance along usetrace */
/* */
	P_MakeDivline (li, &dl);
	frac = P_InterceptVector (&useline, &dl);
	if (frac < 0)
		return true;		/* behind source */
	if (frac > closedist)
		return true;		/* too far away */

/* */
/* the line is actually hit, find the distance  */
/* */
	if (!li->special)
	{
		P_LineOpening (li);
		if (openrange > 0)
			return true;	/* keep going */
	}
	
	closeline = li;
	closedist = frac;
	
	return true;			/* can't use for than one special line in a row */
}


/* 
================
= 
= P_UseLines
=
= Looks for special lines in front of the player to activate 
================  
*/ 
 
void P_UseLines (player_t *player) 
{
	int			angle;
	fixed_t		x1, y1, x2, y2;
	int			x,y, xl, xh, yl, yh;
	
	angle = player->mo->angle >> ANGLETOFINESHIFT;
	x1 = player->mo->x;
	y1 = player->mo->y;
	x2 = x1 + (USERANGE>>FRACBITS)*finecosine(angle);
	y2 = y1 + (USERANGE>>FRACBITS)*finesine(angle);
	
	useline.x = x1;
	useline.y = y1;
	useline.dx = x2-x1;
	useline.dy = y2-y1;
	
	if (useline.dx > 0)
	{
		usebbox[BOXRIGHT] = x2;
		usebbox[BOXLEFT] = x1;
	}
	else
	{
		usebbox[BOXRIGHT] = x1;
		usebbox[BOXLEFT] = x2;
	}
	
	if (useline.dy > 0)
	{
		usebbox[BOXTOP] = y2;
		usebbox[BOXBOTTOM] = y1;
	}
	else
	{
		usebbox[BOXTOP] = y1;
		usebbox[BOXBOTTOM] = y2;
	}
	
	yh = (usebbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;
	yl = (usebbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
	xh = (usebbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
	xl = (usebbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
	
	closeline = NULL;
	closedist = FRACUNIT;
	validcount++;
	
	for (y=yl ; y<=yh ; y++)
		for (x=xl ; x<=xh ; x++)
			P_BlockLinesIterator (x, y, PIT_UseLines );
			
/* */
/* check closest line */
/* */
	if (!closeline)
		return;
		
	if (!closeline->special)
		S_StartSound (player->mo, sfx_noway);
	else
		P_UseSpecialLine (player->mo, closeline);
}



/* 
============================================================================== 

							RADIUS ATTACK

============================================================================== 
*/ 
 
mobj_t		*bombsource;
mobj_t		*bombspot;
int			bombdamage;

/*
=================
=
= PIT_RadiusAttack
=
= Source is the creature that casued the explosion at spot
=================
*/

boolean PIT_RadiusAttack (mobj_t *thing)
{
	fixed_t		dx, dy, dist;
	
	if (!(thing->flags & MF_SHOOTABLE) )
		return true;
		
	dx = D_abs(thing->x - bombspot->x);
	dy = D_abs(thing->y - bombspot->y);
	dist = dx>dy ? dx : dy;
	dist = (dist - thing->radius) >> FRACBITS;
	if (dist < 0)
		dist = 0;
	if (dist >= bombdamage)
		return true;		/* out of range */
/* FIXME?	if ( P_CheckSight (thing, bombspot) )	// must be in direct path */
		P_DamageMobj (thing, bombspot, bombsource, bombdamage - dist);
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
	fixed_t		dist;
	
	dist = (damage+MAXRADIUS)<<FRACBITS;
	yh = (spot->y + dist - bmaporgy)>>MAPBLOCKSHIFT;
	yl = (spot->y - dist - bmaporgy)>>MAPBLOCKSHIFT;
	xh = (spot->x + dist - bmaporgx)>>MAPBLOCKSHIFT;
	xl = (spot->x - dist - bmaporgx)>>MAPBLOCKSHIFT;
	bombspot = spot;
	bombsource = source;
	bombdamage = damage;
	
	for (y=yl ; y<=yh ; y++)
		for (x=xl ; x<=xh ; x++)
			P_BlockThingsIterator (x, y, PIT_RadiusAttack );
}


/*============================================================================ */

int			sightcounts[2];

/*=================== */
/* */
/* IN */
/* */
/* A line will be traced from the middle of shooter in the direction of */
/* attackangle until either a shootable mobj is within the visible */
/* aimtopslope / aimbottomslope range, or a solid wall blocks further */
/* tracing.  If no thing is targeted along the entire range, the first line */
/* that blocks the midpoint of the trace will be hit. */
/*=================== */

mobj_t		*shooter;
angle_t		attackangle;
fixed_t		attackrange;
fixed_t		aimtopslope;
fixed_t		aimbottomslope;

/*=================== */
/* */
/* OUT */
/* */
/*=================== */

extern	line_t		*shootline;
extern	mobj_t		*shootmobj;
extern	fixed_t		shootslope;					/* between aimtop and aimbottom */
extern	fixed_t		shootx, shooty, shootz;		/* location for puff/blood */

void P_Shoot2 (void);

mobj_t	*linetarget;			/* shootmobj latched in main memory */

int		shoottics;


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

/*
=================
=
= P_AimLineAttack
=
=================
*/

fixed_t P_AimLineAttack (mobj_t *t1, angle_t angle, fixed_t distance)
{
	shooter = t1;
	attackrange = distance;
	attackangle = angle;
	aimtopslope = 100*FRACUNIT/160;	/* can't shoot outside view angles */
	aimbottomslope = -100*FRACUNIT/160;

	validcount++;
		
#ifdef JAGUAR
{
	extern	int p_shoot_start;
	shoottics += DSPFunction (&p_shoot_start);
}
#else
	P_Shoot2 ();
#endif
	linetarget = (mobj_t *)DSPRead (&shootmobj);
		
	if (shootmobj)
		return DSPRead(&shootslope);
	return 0;
}
 

/*
=================
=
= P_LineAttack
=
= If slope == MAXINT, use screen bounds for attacking
=
=================
*/

void P_LineAttack (mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope, int damage)
{
	line_t	*shootline2;
	int		shootx2, shooty2, shootz2;
	
	shooter = t1;
	attackrange = distance;
	attackangle = angle;
	
	if (slope == D_MAXINT)
	{
		aimtopslope = 100*FRACUNIT/160;	/* can't shoot outside view angles */
		aimbottomslope = -100*FRACUNIT/160;
	}
	else
	{
		aimtopslope = slope+1;
		aimbottomslope = slope-1;
	}
		
	validcount++;

#ifdef JAGUAR
{
	extern	int p_shoot_start;
	shoottics += DSPFunction (&p_shoot_start);
}
#else
	P_Shoot2 ();
#endif
	linetarget = (mobj_t *)DSPRead (&shootmobj);
	shootline2 = (line_t *)DSPRead (&shootline);
	shootx2 = DSPRead (&shootx);
	shooty2 = DSPRead (&shooty);
	shootz2 = DSPRead (&shootz);
/* */
/* shoot thing */
/* */
	if (linetarget)
	{		
		if (linetarget->flags & MF_NOBLOOD)
			P_SpawnPuff (shootx2,shooty2,shootz2);
		else
			P_SpawnBlood (shootx2,shooty2,shootz2, damage);
	
		P_DamageMobj (linetarget, t1, t1, damage);
		return;
	}
	
/* */
/* shoot wall */
/* */
	if (shootline2)
	{
		sector_t *frontsector, *backsector;

		if (shootline2->special)
			P_ShootSpecialLine (t1, shootline2);

		frontsector = LD_FRONTSECTOR(shootline2);
		backsector = LD_BACKSECTOR(shootline2);
		if (frontsector->ceilingpic == -1)
		{
			if (shootz2 > frontsector->ceilingheight)
				return;		/* don't shoot the sky! */
			if	(backsector 
			&& backsector->ceilingpic == -1)
				return;		/* it's a sky hack wall */
		}
				
		P_SpawnPuff (shootx2,shooty2,shootz2);
	}
	
	
}
 


