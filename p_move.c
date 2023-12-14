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

// CALICO_TODO: these ought to be in headers.
typedef struct
{
   // input
   mobj_t  *tmthing;
   fixed_t  tmx, tmy;

   fixed_t  tmfloorz;   // current floor z for P_TryMove2
   fixed_t  tmceilingz; // current ceiling z for P_TryMove2
   line_t  *blockline;  // possibly a special to activate

   fixed_t tmbbox[4];
   int     tmflags;
   fixed_t tmdropoffz; // lowest point contacted

	int    	numspechit;
 	line_t	**spechit;

   subsector_t *newsubsec; // destination subsector
} pmovework_t;

boolean PIT_CheckThing(mobj_t* thing, pmovework_t *mw) ATTR_DATA_CACHE_ALIGN;
static boolean PIT_CheckLine(line_t* ld, pmovework_t *mw) ATTR_DATA_CACHE_ALIGN;
static boolean PM_CrossCheck(line_t* ld, pmovework_t *mw) ATTR_DATA_CACHE_ALIGN;
static boolean PM_CheckPosition(pmovework_t *mw) ATTR_DATA_CACHE_ALIGN;
boolean P_TryMove2(ptrymove_t *tm, boolean checkposonly) ATTR_DATA_CACHE_ALIGN;

//
// Check a single mobj in one of the contacted blockmap cells.
//
boolean PIT_CheckThing(mobj_t *thing, pmovework_t *mw)
{
   fixed_t blockdist;
   int     delta;
   int     damage;
   boolean solid;
   mobj_t  *tmthing = mw->tmthing;
   int     tmflags = mw->tmflags;
   const mobjinfo_t* thinfo = &mobjinfo[tmthing->type];

   if(!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE)))
      return true;

   blockdist = thing->radius + tmthing->radius;
   
   delta = thing->x - mw->tmx;
   if(delta < 0)
      delta = -delta;
   if(delta >= blockdist)
      return true; // didn't hit it

   delta = thing->y - mw->tmy;
   if(delta < 0)
      delta = -delta;
   if(delta >= blockdist)
      return true; // didn't hit it

   if(thing == tmthing)
      return true; // don't clip against self

   // check for skulls slamming into things
   if(tmthing->flags & MF_SKULLFLY)
   {
		damage = ((P_Random()&7)+1)* thinfo->damage;
		P_DamageMobj (thing, tmthing, tmthing, damage);
		tmthing->flags &= ~MF_SKULLFLY;
		tmthing->momx = tmthing->momy = tmthing->momz = 0;
		P_SetMobjState (tmthing, thinfo->spawnstate);
      return false; // stop moving
   }

   // missiles can hit other things
   if(tmthing->flags & MF_MISSILE)
   {
      if(tmthing->z > thing->z + thing->height)
         return true; // went overhead
      if(tmthing->z + tmthing->height < thing->z)
         return true; // went underneath
      if((tmthing->target->type == thing->type) ||
	    (tmthing->target->type == MT_KNIGHT && thing->type == MT_BRUISER) ||
	    (tmthing->target->type == MT_BRUISER && thing->type == MT_KNIGHT)
      ) // don't hit same species as originator
      {
         if(thing == tmthing->target) // don't hit originator
            return true;
         if(thing->type != MT_PLAYER) // let players missile each other
            return false; // explode, but do no damage
      }
      if(!(thing->flags & MF_SHOOTABLE))
         return !(thing->flags & MF_SOLID); // didn't do any damage

      // damage/explode
		damage = ((P_Random()&7)+1)* thinfo->damage;
		P_DamageMobj (thing, tmthing, tmthing->target, damage);
      return false; // don't traverse any more
   }

   solid = (thing->flags & MF_SOLID) != 0;

   // check for special pickup
   if((thing->flags & MF_SPECIAL) && (tmflags & MF_PICKUP))
   {
      P_TouchSpecialThing (thing,tmthing);
   }

   return !solid;
}

//
// Adjusts tmfloorz and tmceilingz as lines are contacted.
//
static boolean PIT_CheckLine(line_t *ld, pmovework_t *mw)
{
   fixed_t   opentop, openbottom, lowfloor;
   sector_t *front, *back;
   mobj_t   *tmthing = mw->tmthing;

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

   if(front->ceilingheight == front->floorheight ||
      back->ceilingheight == back->floorheight)
   {
      mw->blockline = ld;
      return false; // probably a closed door
   }

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
   if(opentop < mw->tmceilingz)
      mw->tmceilingz = opentop;
   if(openbottom >mw->tmfloorz)
      mw->tmfloorz = openbottom;
   if(lowfloor < mw->tmdropoffz)
      mw->tmdropoffz = lowfloor;

   if (ld->special)
   {
       if (mw->numspechit < MAXSPECIALCROSS)
        mw->spechit[mw->numspechit++] = ld;
   }
   return true;
}

//
// Check a single linedef in a blockmap cell.
//
static boolean PM_CrossCheck(line_t *ld, pmovework_t *mw)
{
   if(P_BoxCrossLine(ld, mw->tmbbox))
   {
      if(!PIT_CheckLine(ld, mw))
         return false;
   }
   return true;
}

//
// This is purely informative, nothing is modified (except things picked up)
//
static boolean PM_CheckPosition(pmovework_t *mw)
{
   int xl, xh, yl, yh, bx, by;
   mobj_t *tmthing = mw->tmthing;
   VINT *lvalidcount;

   mw->tmflags = tmthing->flags;

   mw->tmbbox[BOXTOP   ] = mw->tmy + tmthing->radius;
   mw->tmbbox[BOXBOTTOM] = mw->tmy - tmthing->radius;
   mw->tmbbox[BOXRIGHT ] = mw->tmx + tmthing->radius;
   mw->tmbbox[BOXLEFT  ] = mw->tmx - tmthing->radius;

   mw->newsubsec = R_PointInSubsector(mw->tmx, mw->tmy);

   // the base floor/ceiling is from the subsector that contains the point.
   // Any contacted lines the step closer together will adjust them.
   mw->tmfloorz   = mw->tmdropoffz = mw->newsubsec->sector->floorheight;
   mw->tmceilingz = mw->newsubsec->sector->ceilingheight;

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   *lvalidcount = *lvalidcount + 1;
   if (*lvalidcount == 0)
      *lvalidcount = 1;

   mw->blockline = NULL;
   mw->numspechit = 0;

   if(mw->tmflags & MF_NOCLIP) // thing has no clipping?
      return true;

   // Check things first, possibly picking things up.
   // The bounding box is extended by MAXRADIUS because mobj_ts are grouped
   // into mapblocks based on their origin point, and can overlap into adjacent
   // blocks by up to MAXRADIUS units.
   xl = mw->tmbbox[BOXLEFT  ] - bmaporgx - MAXRADIUS;
   xh = mw->tmbbox[BOXRIGHT ] - bmaporgx + MAXRADIUS;
   yl = mw->tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS;
   yh = mw->tmbbox[BOXTOP   ] - bmaporgy + MAXRADIUS;

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
         if(!P_BlockThingsIterator(bx, by, (blockthingsiter_t)PIT_CheckThing, mw))
            return false;
      }
   }

   // check lines
   xl = mw->tmbbox[BOXLEFT  ] - bmaporgx;
   xh = mw->tmbbox[BOXRIGHT ] - bmaporgx;
   yl = mw->tmbbox[BOXBOTTOM] - bmaporgy;
   yh = mw->tmbbox[BOXTOP   ] - bmaporgy;

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
         if(!P_BlockLinesIterator(bx, by, (blocklinesiter_t)PM_CrossCheck, mw))
            return false;
      }
   }

   return true;
}

//
// Attempt to move to a new position, crossing special lines unless MF_TELEPORT
// is set.
//
boolean P_TryMove2(ptrymove_t *tm, boolean checkposonly)
{
   pmovework_t mw;
   boolean trymove2; // result from P_TryMove2
   mobj_t *tmthing = tm->tmthing;

   mw.tmx = tm->tmx;
   mw.tmy = tm->tmy;
   mw.tmthing = tm->tmthing;
   mw.spechit = &tm->spechit[0];

   trymove2 = PM_CheckPosition(&mw);

   tm->floatok = false;
   tm->blockline = mw.blockline;
   tm->tmfloorz = mw.tmfloorz;
   tm->tmceilingz = mw.tmceilingz;
   tm->tmdropoffz = mw.tmdropoffz;
   tm->numspechit = mw.numspechit;

   if(checkposonly)
      return trymove2;

   if(!trymove2)
      return false;

   if(!(tmthing->flags & MF_NOCLIP))
   {
      if(mw.tmceilingz - mw.tmfloorz < tmthing->height)
         return false; // doesn't fit
      tm->floatok = true;
      if(!(tmthing->flags & MF_TELEPORT) && mw.tmceilingz - tmthing->z < tmthing->height)
         return false; // mobj must lower itself to fit
      if(!(tmthing->flags & MF_TELEPORT) && mw.tmfloorz - tmthing->z > 24*FRACUNIT)
         return false; // too big a step up
      if(!(tmthing->flags & (MF_DROPOFF|MF_FLOAT)) && mw.tmfloorz - mw.tmdropoffz > 24*FRACUNIT)
         return false; // don't stand over a dropoff
   }

   // the move is ok, so link the thing into its new position.
   P_UnsetThingPosition(tmthing);
   tmthing->floorz   = mw.tmfloorz;
   tmthing->ceilingz = mw.tmceilingz;
   tmthing->x        = mw.tmx;
   tmthing->y        = mw.tmy;
   P_SetThingPosition2(tmthing, mw.newsubsec);

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

// EOF

