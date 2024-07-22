/*
  CALICO

  Actor movement and clipping

  The MIT License (MIT)

  Copyright (c) 2015 James Haley, Olde Skuul, id Software and ZeniMax Media
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "doomdef.h"
#include "p_local.h"

static boolean PIT_CheckThing(mobj_t* thing, pmovework_t *w) ATTR_DATA_CACHE_ALIGN;
static boolean PIT_CheckLine(line_t* ld, pmovework_t *w) ATTR_DATA_CACHE_ALIGN;
static boolean PIT_CheckPosition(pmovework_t *w) ATTR_DATA_CACHE_ALIGN;

static boolean P_TryMove2(pmovework_t *tm, boolean checkposonly) ATTR_DATA_CACHE_ALIGN;

boolean P_CheckPosition (pmovework_t *tm, mobj_t *thing, fixed_t x, fixed_t y) ATTR_DATA_CACHE_ALIGN;
boolean P_TryMove (pmovework_t *tm, mobj_t *thing, fixed_t x, fixed_t y) ATTR_DATA_CACHE_ALIGN;

//
// Check a single mobj in one of the contacted blockmap cells.
//
boolean PIT_CheckThing(mobj_t *thing, pmovework_t *w)
{
   fixed_t blockdist;
   int     delta;
   mobj_t  *tmthing = w->tmthing;
   int     tmflags = tmthing->flags;

   if(!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE)))
      return true;

   blockdist = thing->radius + tmthing->radius;
   
   delta = thing->x - w->tmx;
   if(delta < 0)
      delta = -delta;
   if(delta >= blockdist)
      return true; // didn't hit it

   delta = thing->y - w->tmy;
   if(delta < 0)
      delta = -delta;
   if(delta >= blockdist)
      return true; // didn't hit it

   if(thing == tmthing)
      return true; // don't clip against self

   // check for skulls slamming into things
   if(tmthing->flags & MF_SKULLFLY)
   {
	  w->hitthing = thing;
      return false; // stop moving
   }

   // missiles can hit other things
   if(tmthing->flags & MF_MISSILE)
   {
      if(tmthing->z > thing->z + thing->height)
         return true; // went overhead
      if(tmthing->z + tmthing->height < thing->z)
         return true; // went underneath
      if(thing == tmthing->target) // don't hit originator
         return true;
      if((tmthing->target->type == thing->type) ||
	    (tmthing->target->type == MT_KNIGHT && thing->type == MT_BRUISER) ||
	    (tmthing->target->type == MT_BRUISER && thing->type == MT_KNIGHT)
      ) // don't hit same species as originator
      {
         if(thing->type != MT_PLAYER) // let players missile each other
            return false; // explode, but do no damage
      }
      if(!(thing->flags & MF_SHOOTABLE))
         return !(thing->flags & MF_SOLID); // didn't do any damage
	  w->hitthing = thing;
      return false; // don't traverse any more
   }

   // check for special pickup
   if((thing->flags & MF_SPECIAL) && (tmflags & MF_PICKUP))
   {
      P_TouchSpecialThing (thing,tmthing);
   }

   return !(thing->flags & MF_SOLID);
}

//
// Check a single linedef in a blockmap cell.
//
// Adjusts tmfloorz and tmceilingz as lines are contacted.
//
boolean PIT_CheckLine(line_t *ld, pmovework_t *w)
{
   fixed_t   opentop, openbottom, lowfloor;
   sector_t *front, *back;
   mobj_t   *tmthing = w->tmthing;

   if(!P_BoxCrossLine(ld, w->tmbbox))
	return true;

   // The moving thing's destination positoin will cross the given line.
   // If this should not be allowed, return false.
   if(ld->sidenum[1] == -1)
      return false; // one-sided line

   if(!(tmthing->flags & MF_MISSILE))
   {
      if(ld->flags & ML_BLOCKING)
         return false; // explicitly blocking everything
      if(!tmthing->player && (ld->flags & ML_BLOCKMONSTERS))
         return false; // block monsters only
   }

   front = LD_FRONTSECTOR(ld);
   back  = LD_BACKSECTOR(ld);

   if(front->ceilingheight < back->ceilingheight)
      opentop = front->ceilingheight;
   else
      opentop = back->ceilingheight;

   if(front->floorheight > back->floorheight)
   {
      openbottom = front->floorheight;
      lowfloor   = back->floorheight;
   }
   else
   {
      openbottom = back->floorheight;
      lowfloor   = front->floorheight;
   }

   // adjust floor/ceiling heights
   if(opentop < w->tmceilingz)
   {
      w->tmceilingz = opentop;
	  w->ceilingline = ld;
   }
   if(openbottom > w->tmfloorz)
      w->tmfloorz = openbottom;
   if(lowfloor < w->tmdropoffz)
      w->tmdropoffz = lowfloor;

   if (ld->special)
   {
       if (w->numspechit < MAXSPECIALCROSS)
        w->spechit[w->numspechit++] = ld;
   }
   return true;
}

//
// This is purely informative, nothing is modified (except things picked up)
//
boolean PIT_CheckPosition(pmovework_t *w)
{
   int xl, xh, yl, yh, bx, by;
   mobj_t *tmthing = w->tmthing;
   int tmflags = tmthing->flags;
   VINT *lvalidcount;
   subsector_t *newsubsec;

   newsubsec = R_PointInSubsector(w->tmx, w->tmy);
   w->newsubsec = newsubsec;

   w->tmbbox[BOXTOP   ] = w->tmy + tmthing->radius;
   w->tmbbox[BOXBOTTOM] = w->tmy - tmthing->radius;
   w->tmbbox[BOXRIGHT ] = w->tmx + tmthing->radius;
   w->tmbbox[BOXLEFT  ] = w->tmx - tmthing->radius;

   // the base floor/ceiling is from the subsector that contains the point.
   // Any contacted lines the step closer together will adjust them.
   w->tmfloorz   = w->tmdropoffz = newsubsec->sector->floorheight;
   w->tmceilingz = newsubsec->sector->ceilingheight;

   w->numspechit = 0;
   w->hitthing = NULL;
   w->ceilingline = NULL;

   if(tmflags & MF_NOCLIP) // thing has no clipping?
      return true;

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   *lvalidcount = *lvalidcount + 1;
   if (*lvalidcount == 0)
      *lvalidcount = 1;

   // Check things first, possibly picking things up.
   // The bounding box is extended by MAXRADIUS because mobj_ts are grouped
   // into mapblocks based on their origin point, and can overlap into adjacent
   // blocks by up to MAXRADIUS units.
   xl = w->tmbbox[BOXLEFT  ] - bmaporgx - MAXRADIUS;
   xh = w->tmbbox[BOXRIGHT ] - bmaporgx + MAXRADIUS;
   yl = w->tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS;
   yh = w->tmbbox[BOXTOP   ] - bmaporgy + MAXRADIUS;

	if(xl < 0)
		xl = 0;
	if(yl < 0)
		yl = 0;
	if(yh < 0)
		return true;
	if(xh < 0)
		return true;

   xl = (unsigned)xl >> MAPBLOCKSHIFT;
   xh = (unsigned)xh >> MAPBLOCKSHIFT;
   yl = (unsigned)yl >> MAPBLOCKSHIFT;
   yh = (unsigned)yh >> MAPBLOCKSHIFT;

   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;

   // check things
   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockThingsIterator(bx, by, (blockthingsiter_t)PIT_CheckThing, w))
            return false;
      }
   }

   // check lines
   xl = w->tmbbox[BOXLEFT  ] - bmaporgx;
   xh = w->tmbbox[BOXRIGHT ] - bmaporgx;
   yl = w->tmbbox[BOXBOTTOM] - bmaporgy;
   yh = w->tmbbox[BOXTOP   ] - bmaporgy;

   if(xl < 0)
      xl = 0;
   if(yl < 0)
      yl = 0;
	if(yh < 0)
		return true;
	if(xh < 0)
		return true;

   xl = (unsigned)xl >> MAPBLOCKSHIFT;
   xh = (unsigned)xh >> MAPBLOCKSHIFT;
   yl = (unsigned)yl >> MAPBLOCKSHIFT;
   yh = (unsigned)yh >> MAPBLOCKSHIFT;

   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockLinesIterator(bx, by, (blocklinesiter_t)PIT_CheckLine, w))
            return false;
      }
   }

   return true;
}

//
// Attempt to move to a new position, crossing special lines unless MF_TELEPORT
// is set.
//
static boolean P_TryMove2(pmovework_t *w, boolean checkposonly)
{
   boolean trymove2; // result from PIT_CheckPosition
   mobj_t *tmthing = w->tmthing;

   trymove2 = PIT_CheckPosition(w);
   w->floatok = false;

   if(checkposonly)
      return trymove2;

   if(!trymove2)
      return false;

   if(!(tmthing->flags & MF_NOCLIP))
   {
      if(w->tmceilingz - w->tmfloorz < tmthing->height)
         return false; // doesn't fit
      w->floatok = true;
      if(!(tmthing->flags & MF_TELEPORT) && w->tmceilingz - tmthing->z < tmthing->height)
         return false; // mobj must lower itself to fit
      if(!(tmthing->flags & MF_TELEPORT) && w->tmfloorz - tmthing->z > 24*FRACUNIT)
         return false; // too big a step up
      if(!(tmthing->flags & (MF_DROPOFF|MF_FLOAT)) && w->tmfloorz - w->tmdropoffz > 24*FRACUNIT)
         return false; // don't stand over a dropoff
   }

   // the move is ok, so link the thing into its new position.
   P_UnsetThingPosition(tmthing);
   tmthing->floorz   = w->tmfloorz;
   tmthing->ceilingz = w->tmceilingz;
   tmthing->x        = w->tmx;
   tmthing->y        = w->tmy;
   P_SetThingPosition2(tmthing, w->newsubsec);

   return true;
}

void P_MoveCrossSpecials(mobj_t *tmthing, int numspechit, line_t **spechit, fixed_t oldx, fixed_t oldy)
{
    int i;

    if ((tmthing->flags&(MF_TELEPORT|MF_NOCLIP)) )
      return;

    for (i = 0; i < numspechit; i++)
    {
      line_t *ld = spechit[i];
      int side, oldside;
      // see if the line was crossed
      side = P_PointOnLineSide (tmthing->x, tmthing->y, ld);
      oldside = P_PointOnLineSide (oldx, oldy, ld);
      if (side != oldside)
         P_CrossSpecialLine(ld, tmthing);
    }
}


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

boolean P_CheckPosition (pmovework_t *tm, mobj_t *thing, fixed_t x, fixed_t y)
{
   pmovework_t tm2;
   if (tm == NULL)
      tm = &tm2;
	tm->tmthing = thing;
	tm->tmx = x;
	tm->tmy = y;
	return P_TryMove2 (tm, true);
}

boolean P_TryMove (pmovework_t *tm, mobj_t *thing, fixed_t x, fixed_t y)
{
   pmovework_t tm2;
   if (tm == NULL)
      tm = &tm2;
	tm->tmthing = thing;
	tm->tmx = x;
	tm->tmy = y;
	return P_TryMove2 (tm, false);
}

// EOF
