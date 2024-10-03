/* P_map.c */

#include "doomdef.h"
#include "p_local.h"

typedef struct 
{
   fixed_t x, y;
   int16_t dx, dy;
} i16divline_t;

typedef struct
{
	int			usebbox[4];
	divline_t	useline;

	line_t		*closeline;
	fixed_t		closedist;
} plineuse_t;

typedef struct
{
	mobj_t		*bombsource;
	mobj_t		*bombspot;
	int			bombdamage;
} pradiusattack_t;

static fixed_t P_InterceptVector(divline_t* v2, i16divline_t* v1) ATTR_DATA_CACHE_ALIGN;
boolean	PIT_UseLines(line_t* li, plineuse_t *lu) ATTR_DATA_CACHE_ALIGN;
void P_UseLines(player_t* player) __attribute__((noinline));

boolean PIT_RadiusAttack(mobj_t* thing, pradiusattack_t *ra) ATTR_DATA_CACHE_ALIGN;
void P_RadiusAttack(mobj_t* spot, mobj_t* source, int damage) ATTR_DATA_CACHE_ALIGN;
fixed_t P_AimLineAttack(lineattack_t *la, mobj_t* t1, angle_t angle, fixed_t distance) ATTR_DATA_CACHE_ALIGN;
void P_LineAttack(lineattack_t *la, mobj_t* t1, angle_t angle, fixed_t distance, fixed_t slope, int damage) ATTR_DATA_CACHE_ALIGN;
static void P_MakeI16Divline(line_t* li, i16divline_t* dl) ATTR_DATA_CACHE_ALIGN;

/*============================================================================= */

/* 
============================================================================== 

							USE LINES

============================================================================== 
*/ 

/*
===============
=
= P_InterceptVector
=
= Returns the fractional intercept point along the first divline
=
===============
*/

static fixed_t P_InterceptVector (divline_t *v2, i16divline_t *v1)
{
	fixed_t	frac, num, den;
	
	den = v1->dy * v2->dx - v1->dx * v2->dy;
   	if(den == 0)
    	return -1;
	num  = (v1->x - v2->x) * v1->dy + (v2->y - v1->y) * v1->dx;
	frac = IDiv(num, den);

	return frac;
}



/*
==============
=
= P_MakeI16Divline
=
==============
*/

static void P_MakeI16Divline (line_t *li, i16divline_t *dl)
{
	dl->x = vertexes[li->v1].x << FRACBITS;
	dl->y = vertexes[li->v1].y << FRACBITS;
	dl->dx = vertexes[li->v2].x - vertexes[li->v1].x;
	dl->dy = vertexes[li->v2].y - vertexes[li->v1].y;
}


/*
================
=
= PIT_UseLines
=
================
*/

boolean	PIT_UseLines (line_t *li, plineuse_t *lu)
{
	i16divline_t	dl;
	fixed_t		frac;
	fixed_t 	libbox[4];

	P_LineBBox(li, libbox);

/* */
/* check bounding box first */
/* */
	if (lu->usebbox[BOXRIGHT] <= libbox[BOXLEFT]
	||	lu->usebbox[BOXLEFT] >= libbox[BOXRIGHT]
	||	lu->usebbox[BOXTOP] <= libbox[BOXBOTTOM]
	||	lu->usebbox[BOXBOTTOM] >= libbox[BOXTOP] )
		return true;

/* */
/* find distance along usetrace */
/* */
	P_MakeI16Divline (li, &dl);
	frac = P_InterceptVector (&lu->useline, &dl);
	if (frac < 0)
		return true;		/* behind source */
	if (frac > lu->closedist)
		return true;		/* too far away */

/* */
/* the line is actually hit, find the distance  */
/* */
	if (!li->special)
	{
		fixed_t openrange = P_LineOpening (li);
		if (openrange > 0)
			return true;	/* keep going */
	}
	
	lu->closeline = li;
	lu->closedist = frac;
	
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
	plineuse_t	lu;
	VINT 		*lvalidcount;
	
	angle = player->mo->angle >> ANGLETOFINESHIFT;
	x1 = player->mo->x;
	y1 = player->mo->y;
	x2 = x1 + (USERANGE>>FRACBITS)*finecosine(angle);
	y2 = y1 + (USERANGE>>FRACBITS)*finesine(angle);
	
	lu.useline.x = x1;
	lu.useline.y = y1;
	lu.useline.dx = x2-x1;
	lu.useline.dy = y2-y1;
	
	if (lu.useline.dx > 0)
	{
		lu.usebbox[BOXRIGHT] = x2;
		lu.usebbox[BOXLEFT] = x1;
	}
	else
	{
		lu.usebbox[BOXRIGHT] = x1;
		lu.usebbox[BOXLEFT] = x2;
	}
	
	if (lu.useline.dy > 0)
	{
		lu.usebbox[BOXTOP] = y2;
		lu.usebbox[BOXBOTTOM] = y1;
	}
	else
	{
		lu.usebbox[BOXTOP] = y1;
		lu.usebbox[BOXBOTTOM] = y2;
	}
	
	lu.useline.dx >>= 16;
	lu.useline.dy >>= 16;

	yh = lu.usebbox[BOXTOP] - bmaporgy;
	yl = lu.usebbox[BOXBOTTOM] - bmaporgy;
	xh = lu.usebbox[BOXRIGHT] - bmaporgx;
	xl = lu.usebbox[BOXLEFT] - bmaporgx;

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
	
	lu.closeline = NULL;
	lu.closedist = FRACUNIT;

	I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
	*lvalidcount = *lvalidcount + 1;
	if (*lvalidcount == 0)
		*lvalidcount = 1;

	for (y=yl ; y<=yh ; y++)
		for (x=xl ; x<=xh ; x++)
			P_BlockLinesIterator (x, y, (blocklinesiter_t)PIT_UseLines, &lu );
			
/* */
/* check closest line */
/* */
	if (!lu.closeline)
		return;
		
	if (!lu.closeline->special)
		S_StartSound (player->mo, sfx_noway);
	else
		P_UseSpecialLine (player->mo, lu.closeline);
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
	fixed_t		dx, dy, dist;
	
	if (!(thing->flags & MF_SHOOTABLE) )
		return true;

	// Boss spider and cyborg
	// take no damage from concussion.
	if (thing->type == MT_CYBORG
		|| thing->type == MT_SPIDER)
		return true;	

	dx = D_abs(thing->x - ra->bombspot->x);
	dy = D_abs(thing->y - ra->bombspot->y);
	dist = dx>dy ? dx : dy;
	dist = (dist - thing->radius) >> FRACBITS;
	if (dist < 0)
		dist = 0;
	if (dist >= ra->bombdamage)
		return true;		/* out of range */
/* FIXME?	if ( P_CheckSight (thing, bombspot) )	// must be in direct path */
		P_DamageMobj (thing, ra->bombspot, ra->bombsource, ra->bombdamage - dist);
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
	pradiusattack_t ra;
	
	dist = (damage+MAXRADIUS)<<FRACBITS;
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

	ra.bombspot = spot;
	ra.bombsource = source;
	ra.bombdamage = damage;
	
	for (y=yl ; y<=yh ; y++)
		for (x=xl ; x<=xh ; x++)
			P_BlockThingsIterator (x, y, (blockthingsiter_t)PIT_RadiusAttack, &ra );
}


/*============================================================================ */

void P_Shoot2 (lineattack_t *la);

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

fixed_t P_AimLineAttack (lineattack_t *la, mobj_t *t1, angle_t angle, fixed_t distance)
{
	mobj_t  *linetarget;

	la->shooter = t1;
	la->attackrange = distance;
	la->attackangle = angle;
	la->aimtopslope = 100*FRACUNIT/160;	/* can't shoot outside view angles */
	la->aimbottomslope = -100*FRACUNIT/160;

	P_Shoot2 (la);

	linetarget = la->shootmobj;
	if (linetarget)
		return la->shootslope;
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

void P_LineAttack (lineattack_t *la, mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope, int damage)
{
	mobj_t  *linetarget;
	line_t	*shootline2;
	int		shootx2, shooty2, shootz2;
	
	la->shooter = t1;
	la->attackrange = distance;
	la->attackangle = angle;
	
	if (slope == D_MAXINT)
	{
		la->aimtopslope = 100*FRACUNIT/160;	/* can't shoot outside view angles */
		la->aimbottomslope = -100*FRACUNIT/160;
	}
	else
	{
		la->aimtopslope = slope+1;
		la->aimbottomslope = slope-1;
	}

	P_Shoot2 (la);

	linetarget = la->shootmobj;
	shootline2 = la->shootline;
	shootx2 = la->shootx;
	shooty2 = la->shooty;
	shootz2 = la->shootz;

/* */
/* shoot thing */
/* */
	if (linetarget)
	{		
		if (linetarget->flags & MF_NOBLOOD)
			P_SpawnPuff (shootx2,shooty2,shootz2, distance);
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
				if (backsector->ceilingheight < shootz2)
					return;		/* it's a sky hack wall */
		}
				
		P_SpawnPuff (shootx2,shooty2,shootz2,distance);
	}
	
	
}
 


