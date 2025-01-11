
/* P_maputl.c */

#include "doomdef.h"
#include "p_local.h"

fixed_t P_AproxDistance(fixed_t dx, fixed_t dy) ATTR_DATA_CACHE_ALIGN;
int P_PointOnLineSide(fixed_t x, fixed_t y, line_t* line) ATTR_DATA_CACHE_ALIGN;
boolean P_BoxCrossLine(line_t *ld, fixed_t testbbox[4]) ATTR_DATA_CACHE_ALIGN;
int P_PointOnDivlineSide(fixed_t x, fixed_t y, divline_t* line) ATTR_DATA_CACHE_ALIGN;
fixed_t P_LineOpening(line_t* linedef) ATTR_DATA_CACHE_ALIGN;
void P_LineBBox(line_t* ld, fixed_t* bbox) ATTR_DATA_CACHE_ALIGN;
void P_UnsetThingPosition(mobj_t* thing) ATTR_DATA_CACHE_ALIGN;
void P_SetThingPosition(mobj_t* thing) ATTR_DATA_CACHE_ALIGN;
void P_SetThingPosition2(mobj_t* thing, subsector_t *ss) ATTR_DATA_CACHE_ALIGN;
boolean P_BlockLinesIterator(int x, int y, boolean(*func)(line_t*, void*), void *userp) ATTR_DATA_CACHE_ALIGN;
boolean P_BlockThingsIterator(int x, int y, boolean(*func)(mobj_t*, void*), void *userp) ATTR_DATA_CACHE_ALIGN;

/*
===================
=
= P_AproxDistance
=
= Gives an estimation of distance (not exact)
=
===================
*/

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy)
{
	dx = D_abs(dx);
	dy = D_abs(dy);
	if (dx < dy)
		return dx+dy-(dx>>1);
	return dx+dy-(dy>>1);
}


/*
==================
=
= P_PointOnLineSide
=
= Returns 0 or 1
==================
*/

int P_PointOnLineSide (fixed_t x, fixed_t y, line_t *line)
{
	fixed_t	dx,dy;
	fixed_t	left, right;
	fixed_t ldx,ldy;

	ldx = vertexes[line->v2].x - vertexes[line->v1].x;
	ldy = vertexes[line->v2].y - vertexes[line->v1].y;

	dx = x - (vertexes[line->v1].x << FRACBITS);
	dy = y - (vertexes[line->v1].y << FRACBITS);

#ifdef MARS
	dx = (unsigned)dx >> FRACBITS;
	__asm volatile(
		"muls.w %0,%1\n\t"
		: : "r"(ldy), "r"(dx) : "macl", "mach");
#else
	left = (ldy) * (dx>>16);
#endif

#ifdef MARS
	dy = (unsigned)dy >> FRACBITS;
	__asm volatile(
		"sts macl, %0\n\t"
		"muls.w %2,%3\n\t"
		"sts macl, %1\n\t"
		: "=&r"(left), "=&r"(right) : "r"(dy), "r"(ldx) : "macl", "mach");
#else
	right = (dy>>16) * (ldx);
#endif

	if (right < left)
		return 0;		/* front side */
	return 1;			/* back side */
}

//
// Check if the thing intersects a linedef
//
boolean P_BoxCrossLine(line_t *ld, fixed_t testbbox[4])
{
   fixed_t x1, x2;
   fixed_t lx, ly;
   fixed_t ldx, ldy;
   fixed_t dx1, dy1, dx2, dy2;
   boolean side1, side2;
   fixed_t ldbbox[4];

   P_LineBBox(ld, ldbbox);

   // entirely outside bounding box of line?
   if(testbbox[BOXRIGHT ] <= ldbbox[BOXLEFT  ] ||
      testbbox[BOXLEFT  ] >= ldbbox[BOXRIGHT ] ||
      testbbox[BOXTOP   ] <= ldbbox[BOXBOTTOM] ||
      testbbox[BOXBOTTOM] >= ldbbox[BOXTOP   ])
   {
      return false;
   }

   lx  = vertexes[ld->v1].x << FRACBITS;
   ly  = vertexes[ld->v1].y << FRACBITS;
   ldx = vertexes[ld->v2].x - vertexes[ld->v1].x;
   ldy = vertexes[ld->v2].y - vertexes[ld->v1].y;

   if (ldx && ldy && ( (ldx ^ ldy) & 0x80000000 ) == 0) /* positive */
   {
      x1 = testbbox[BOXLEFT ];
      x2 = testbbox[BOXRIGHT];
   }
   else
   {
      x1 = testbbox[BOXRIGHT];
      x2 = testbbox[BOXLEFT ];
   }

   dx1 = (x1 - lx) >> FRACBITS;
   dy1 = (testbbox[BOXTOP] - ly) >> FRACBITS;
   dx2 = (x2 - lx) >> FRACBITS;
   dy2 = (testbbox[BOXBOTTOM] - ly) >> FRACBITS;

   side1 = (ldy * dx1 < dy1 * ldx);
   side2 = (ldy * dx2 < dy2 * ldx);

   return (side1 != side2);
}

/*
==================
=
= P_PointOnDivlineSide
=
= Returns 0 or 1
==================
*/

int P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t *line)
{
	fixed_t	dx,dy;
	fixed_t	left, right;
	
	dx = (x - line->x);
	dy = (y - line->y);
	
#ifndef MARS
/* try to quickly decide by looking at sign bits */
	if ( (line->dy ^ line->dx ^ dx ^ dy)&0x80000000 )
	{
		if ( (line->dy ^ dx) & 0x80000000 )
			return 1;	/* (left is negative) */
		return 0;
	}
#endif

	left = FixedMul(line->dy>>8, dx>>8);
	right = FixedMul(dy>>8, line->dx>>8);
	
	if (right < left)
		return 0;		/* front side */
	return 1;			/* back side */
}

/*
==================
=
= P_LineOpening
=
= Sets opentop and openbottom to the window through a two sided line
= OPTIMIZE: keep this precalculated
==================
*/

fixed_t P_LineOpening (line_t *linedef)
{
	sector_t	*front, *back;
	fixed_t opentop, openbottom;
	
	if (linedef->sidenum[1] == -1)
	{	/* single sided line */
		return 0;
	}
	 
	front = LD_FRONTSECTOR(linedef);
	back = LD_BACKSECTOR(linedef);
	
	if (front->ceilingheight < back->ceilingheight)
		opentop = front->ceilingheight;
	else
		opentop = back->ceilingheight;
	if (front->floorheight > back->floorheight)
		openbottom = front->floorheight;
	else
		openbottom = back->floorheight;
	
	return opentop - openbottom;
}

void P_LineBBox(line_t* ld, fixed_t *bbox)
{
	mapvertex_t* v1 = &vertexes[ld->v1], * v2 = &vertexes[ld->v2];

	if (v1->x < v2->x)
	{
		bbox[BOXLEFT] = v1->x;
		bbox[BOXRIGHT] = v2->x;
	}
	else
	{
		bbox[BOXLEFT] = v2->x;
		bbox[BOXRIGHT] = v1->x;
	}
	if (v1->y < v2->y)
	{
		bbox[BOXBOTTOM] = v1->y;
		bbox[BOXTOP] = v2->y;
	}
	else
	{
		bbox[BOXBOTTOM] = v2->y;
		bbox[BOXTOP] = v1->y;
	}

	bbox[BOXTOP] <<= FRACBITS;
	bbox[BOXBOTTOM] <<= FRACBITS;
	bbox[BOXLEFT] <<= FRACBITS;
	bbox[BOXRIGHT] <<= FRACBITS;
}

/*
===============================================================================

						THING POSITION SETTING

===============================================================================
*/

/*
===================
=
= P_UnsetThingPosition 
=
= Unlinks a thing from block map and sectors
=
===================
*/

void P_UnsetThingPosition (mobj_t *thing)
{
	int				blockx, blocky;

	if ( ! (thing->flags & MF_NOSECTOR) )
	{	/* inert things don't need to be in blockmap */
/* unlink from subsector */
		mobj_t *snext = SPTR_TO_LPTR(thing->snext);
		mobj_t *sprev = SPTR_TO_LPTR(thing->sprev);

		if (snext)
			snext->sprev = thing->sprev;
		if (sprev)
			sprev->snext = thing->snext;
		else
			thing->subsector->sector->thinglist = thing->snext;
	}
	
	if ( ! (thing->flags & MF_NOBLOCKMAP) )
	{	/* inert things don't need to be in blockmap */
/* unlink from block map */
		mobj_t *bnext = SPTR_TO_LPTR(thing->bnext);
		mobj_t *bprev = SPTR_TO_LPTR(thing->bprev);

		if (bnext)
			bnext->bprev = thing->bprev;
		if (bprev)
			bprev->bnext = thing->bnext;
		else
		{
			blockx = thing->x - bmaporgx;
			blocky = thing->y - bmaporgy;
			if (blockx >= 0 && blocky >= 0)
			{
				blockx = (unsigned)blockx >> MAPBLOCKSHIFT;
				blocky = (unsigned)blocky >> MAPBLOCKSHIFT;
				if (blockx < bmapwidth && blocky <bmapheight)
					blocklinks[blocky*bmapwidth+blockx] = thing->bnext;
			}
		}
	}
}

/*
===================
=
= P_SetThingPosition2
=
= Links a thing into both a block and a subsector based on it's x y
=
===================
*/
void P_SetThingPosition2 (mobj_t *thing, subsector_t *ss)
{
	sector_t		*sec;
	int				blockx, blocky;
	SPTR 			*link;

/* */
/* link into subsector */
/* */
	thing->subsector = ss;
	if ( ! (thing->flags & MF_NOSECTOR) )
	{	/* invisible things don't go into the sector links */
		sec = ss->sector;
	
		thing->sprev = (SPTR)0;
		thing->snext = sec->thinglist;
		if (sec->thinglist)
			((mobj_t *)SPTR_TO_LPTR(sec->thinglist))->sprev = LPTR_TO_SPTR(thing);
		sec->thinglist = LPTR_TO_SPTR(thing);
	}
	
/* */
/* link into blockmap */
/* */
	if ( ! (thing->flags & MF_NOBLOCKMAP) )
	{	/* inert things don't need to be in blockmap		 */
		blockx = thing->x - bmaporgx;
		blocky = thing->y - bmaporgy;
		if (blockx>=0 && blocky>=0)
		{
			blockx = (unsigned)blockx >> MAPBLOCKSHIFT;
			blocky = (unsigned)blocky >> MAPBLOCKSHIFT;
			if (blockx < bmapwidth && blocky <bmapheight)
			{
				link = &blocklinks[blocky*bmapwidth+blockx];
				thing->bprev = (SPTR)0;
				thing->bnext = *link;
				if (*link)
					((mobj_t *)(SPTR_TO_LPTR(*link)))->bprev = LPTR_TO_SPTR(thing);
				*link = LPTR_TO_SPTR(thing);
			}
			else
			{	/* thing is off the map */
				thing->bnext = thing->bprev = (SPTR)0;
			}
		}
		else
		{	/* thing is off the map */
			thing->bnext = thing->bprev = (SPTR)0;
		}
	}
}

/*
===================
=
= P_SetThingPosition
=
= Links a thing into both a block and a subsector based on it's x y
= Sets thing->subsector properly
=
===================
*/

void P_SetThingPosition (mobj_t *thing)
{
	P_SetThingPosition2(thing, R_PointInSubsector (thing->x,thing->y));
}



/*
===============================================================================

						BLOCK MAP ITERATORS

For each line/thing in the given mapblock, call the passed function.
If the function returns false, exit with false without checking anything else.

===============================================================================
*/

/*
==================
=
= P_BlockLinesIterator
=
= The validcount flags are used to avoid checking lines
= that are marked in multiple mapblocks, so increment validcount before
= the first call to P_BlockLinesIterator, then make one or more calls to it
===================
*/

boolean P_BlockLinesIterator (int x, int y, blocklinesiter_t func, void *userp )
{
	int			offset;
	short		*list;
	line_t		*ld;
	VINT 		*lvalidcount, vc;

	//if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
	//	return true;
	offset = y*bmapwidth+x;
	
	offset = *(blockmaplump+4+offset);

	I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
	vc = *lvalidcount;
	++lvalidcount;

	for ( list = blockmaplump+offset ; *list != -1 ; list++)
	{
		int l = *list;
		ld = &lines[*list];

		if (lvalidcount[l] == vc)
			continue;		/* line has already been checked */
		lvalidcount[l] = vc;
		
		if ( !func(ld, userp) )
			return false;
	}
	
	return true;		/* everything was checked */
}


/*
==================
=
= P_BlockThingsIterator
=
==================
*/

boolean P_BlockThingsIterator (int x, int y, blockthingsiter_t func, void *userp )
{
	mobj_t		*mobj;
	
	//if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
	//	return true;

	for (mobj = SPTR_TO_LPTR(blocklinks[y*bmapwidth+x]) ; mobj ; mobj = SPTR_TO_LPTR(mobj->bnext))
		if (!func( mobj, userp ) )
			return false;	

	return true;
}

void P_SectorOrg(mobj_t* sec_, fixed_t *org)
{
	sector_t *sec = (void *)sec_;
	org[0] = sec->soundorg[0] << FRACBITS;
	org[1] = sec->soundorg[1] << FRACBITS;
}

int P_GetLineTag (line_t *line)
{
	VINT j;
	VINT rowsize = (unsigned)numlinetags / LINETAGS_HASH_SIZE;
	VINT ld = line - lines;
	VINT h = (unsigned)ld % LINETAGS_HASH_SIZE;
	VINT s = h * rowsize;

	for (j = 0; j < numlinetags; j++)
	{
		int16_t *l;
		VINT e;

		e = s + j;
		if (e >= numlinetags)
			e -= numlinetags;

		l = &linetags[e * 2];
		if (l[0] == ld) {
			return l[1];
		}
	}

	return 0;
}
