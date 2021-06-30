
/* P_maputl.c */

#include "doomdef.h"
#include "p_local.h"

fixed_t P_AproxDistance(fixed_t dx, fixed_t dy) ATTR_DATA_CACHE_ALIGN;
int P_PointOnLineSide(fixed_t x, fixed_t y, line_t* line) ATTR_DATA_CACHE_ALIGN;
int P_PointOnDivlineSide(fixed_t x, fixed_t y, divline_t* line) ATTR_DATA_CACHE_ALIGN;
void P_MakeDivline(line_t* li, divline_t* dl) ATTR_DATA_CACHE_ALIGN;
void P_LineOpening(line_t* linedef) ATTR_DATA_CACHE_ALIGN;
fixed_t* P_LineBBox(line_t* ld) ATTR_DATA_CACHE_ALIGN;
void P_UnsetThingPosition(mobj_t* thing) ATTR_DATA_CACHE_ALIGN;
void P_SetThingPosition(mobj_t* thing) ATTR_DATA_CACHE_ALIGN;
boolean P_BlockLinesIterator(int x, int y, boolean(*func)(line_t*)) ATTR_DATA_CACHE_ALIGN;
boolean P_BlockThingsIterator(int x, int y, boolean(*func)(mobj_t*)) ATTR_DATA_CACHE_ALIGN;

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

	ldx = line->v2->x - line->v1->x;
	ldy = line->v2->y - line->v1->y;
	if (!ldx)
	{
		if (x <= line->v1->x)
			return ldy > 0;
		return ldy < 0;
	}
	if (!ldy)
	{
		if (y <= line->v1->y)
			return ldx < 0;
		return ldx > 0;
	}
	
	dx = (x - line->v1->x);
	dy = (y - line->v1->y);
	
	left = (ldy>>16) * (dx>>16);
	right = (dy>>16) * (ldx>>16);
	
	if (right < left)
		return 0;		/* front side */
	return 1;			/* back side */
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
	
	if (!line->dx)
	{
		if (x <= line->x)
			return line->dy > 0;
		return line->dy < 0;
	}
	if (!line->dy)
	{
		if (y <= line->y)
			return line->dx < 0;
		return line->dx > 0;
	}
	
	dx = (x - line->x);
	dy = (y - line->y);
	
/* try to quickly decide by looking at sign bits */
	if ( (line->dy ^ line->dx ^ dx ^ dy)&0x80000000 )
	{
		if ( (line->dy ^ dx) & 0x80000000 )
			return 1;	/* (left is negative) */
		return 0;
	}
	
	FixedMul2(left, line->dy>>8, dx>>8);
	FixedMul2(right, dy>>8, line->dx>>8);
	
	if (right < left)
		return 0;		/* front side */
	return 1;			/* back side */
}



/*
==============
=
= P_MakeDivline
=
==============
*/

void P_MakeDivline (line_t *li, divline_t *dl)
{
	dl->x = li->v1->x;
	dl->y = li->v1->y;
	dl->dx = li->v2->x - li->v1->x;
	dl->dy = li->v2->y - li->v1->y;
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

fixed_t opentop, openbottom, openrange;
fixed_t	lowfloor;

void P_LineOpening (line_t *linedef)
{
	sector_t	*front, *back;
	
	if (linedef->sidenum[1] == -1)
	{	/* single sided line */
		openrange = 0;
		return;
	}
	 
	front = LD_FRONTSECTOR(linedef);
	back = LD_BACKSECTOR(linedef);
	
	if (front->ceilingheight < back->ceilingheight)
		opentop = front->ceilingheight;
	else
		opentop = back->ceilingheight;
	if (front->floorheight > back->floorheight)
	{
		openbottom = front->floorheight;
		lowfloor = back->floorheight;
	}
	else
	{
		openbottom = back->floorheight;
		lowfloor = front->floorheight;
	}
	
	openrange = opentop - openbottom;
}

fixed_t* P_LineBBox(line_t* ld)
{
	static fixed_t bbox[4];
	vertex_t* v1 = ld->v1, * v2 = ld->v2;

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

	return bbox;
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
		if (thing->snext)
			thing->snext->sprev = thing->sprev;
		if (thing->sprev)
			thing->sprev->snext = thing->snext;
		else
			thing->subsector->sector->thinglist = thing->snext;
	}
	
	if ( ! (thing->flags & MF_NOBLOCKMAP) )
	{	/* inert things don't need to be in blockmap */
/* unlink from block map */
		if (thing->bnext)
			thing->bnext->bprev = thing->bprev;
		if (thing->bprev)
			thing->bprev->bnext = thing->bnext;
		else
		{
			blockx = (thing->x - bmaporgx)>>MAPBLOCKSHIFT;
			blocky = (thing->y - bmaporgy)>>MAPBLOCKSHIFT;
			blocklinks[blocky*bmapwidth+blockx] = thing->bnext;
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
	subsector_t		*ss;
	sector_t		*sec;
	int				blockx, blocky;
	mobj_t			**link;
	
/* */
/* link into subsector */
/* */
	ss = R_PointInSubsector (thing->x,thing->y);
	thing->subsector = ss;
	if ( ! (thing->flags & MF_NOSECTOR) )
	{	/* invisible things don't go into the sector links */
		sec = ss->sector;
	
		thing->sprev = NULL;
		thing->snext = sec->thinglist;
		if (sec->thinglist)
			sec->thinglist->sprev = thing;
		sec->thinglist = thing;
	}
	
/* */
/* link into blockmap */
/* */
	if ( ! (thing->flags & MF_NOBLOCKMAP) )
	{	/* inert things don't need to be in blockmap		 */
		blockx = (thing->x - bmaporgx)>>MAPBLOCKSHIFT;
		blocky = (thing->y - bmaporgy)>>MAPBLOCKSHIFT;
		if (blockx>=0 && blockx < bmapwidth && blocky>=0 && blocky <bmapheight)
		{
			link = &blocklinks[blocky*bmapwidth+blockx];
			thing->bprev = NULL;
			thing->bnext = *link;
			if (*link)
				(*link)->bprev = thing;
			*link = thing;
		}
		else
		{	/* thing is off the map */
			thing->bnext = thing->bprev = NULL;
		}
	}
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

boolean P_BlockLinesIterator (int x, int y, boolean(*func)(line_t*) )
{
	int			offset;
	short		*list;
	line_t		*ld;
	
	if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
		return true;
	offset = y*bmapwidth+x;
	
	offset = *(blockmap+offset);

	for ( list = blockmaplump+offset ; *list != -1 ; list++)
	{
		ld = &lines[*list];
		if (ld->validcount == validcount)
			continue;		/* line has already been checked */
		ld->validcount = validcount;
		
		if ( !func(ld) )
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

boolean P_BlockThingsIterator (int x, int y, boolean(*func)(mobj_t*) )
{
	mobj_t		*mobj;
	
	if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
		return true;

	for (mobj = blocklinks[y*bmapwidth+x] ; mobj ; mobj = mobj->bnext)
		if (!func( mobj ) )
			return false;	

	return true;
}

