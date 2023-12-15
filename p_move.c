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

static boolean PM_CheckThing(mobj_t *thing, pcheckwork_t *w) ATTR_DATA_CACHE_ALIGN;
static boolean PM_CheckPosition(pcheckwork_t *w, subsector_t **pnewsubsec) ATTR_DATA_CACHE_ALIGN;
boolean P_TryMove2(pcheckwork_t *tm, boolean checkposonly) ATTR_DATA_CACHE_ALIGN;

static boolean PM_CheckThing(mobj_t *thing, pcheckwork_t *w)
{
   mobj_t *tmthing = w->tmthing;
   int     damage;
   const mobjinfo_t* thinfo = &mobjinfo[tmthing->type];

   if (PIT_CheckThing(thing, w)) {
      return true;
   }

   if (!w->hitthing) {
      return false;
   }

   if(tmthing->flags & MF_SKULLFLY) {
      damage = ((P_Random()&7)+1)* thinfo->damage;
      P_DamageMobj (thing, tmthing, tmthing, damage);
      tmthing->flags &= ~MF_SKULLFLY;
      tmthing->momx = tmthing->momy = tmthing->momz = 0;
      P_SetMobjState (tmthing, thinfo->spawnstate);
      return false;
   }

   if(tmthing->flags & MF_MISSILE) {
      // damage/explode
      damage = ((P_Random()&7)+1)* thinfo->damage;
		P_DamageMobj (thing, tmthing, tmthing->target, damage);
      return false;
   }

   return false;
}

//
// This is purely informative, nothing is modified (except things picked up)
//
static boolean PM_CheckPosition(pcheckwork_t *w, subsector_t **pnewsubsec)
{
   int xl, xh, yl, yh, bx, by;
   mobj_t *tmthing = w->tmthing;
   int tmflags = tmthing->flags;
   VINT *lvalidcount;
   subsector_t *newsubsec;

   newsubsec = R_PointInSubsector(w->tmx, w->tmy);
   *pnewsubsec = newsubsec;

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
         if(!P_BlockThingsIterator(bx, by, (blockthingsiter_t)PM_CheckThing, w))
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
boolean P_TryMove2(pcheckwork_t *tm, boolean checkposonly)
{
   subsector_t *newsubsec;
   boolean trymove2; // result from P_TryMove2
   mobj_t *tmthing = tm->tmthing;

   trymove2 = PM_CheckPosition(tm, &newsubsec);
   tm->floatok = false;

   if(checkposonly)
      return trymove2;

   if(!trymove2)
      return false;

   if(!(tmthing->flags & MF_NOCLIP))
   {
      if(tm->tmceilingz - tm->tmfloorz < tmthing->height)
         return false; // doesn't fit
      tm->floatok = true;
      if(!(tmthing->flags & MF_TELEPORT) && tm->tmceilingz - tmthing->z < tmthing->height)
         return false; // mobj must lower itself to fit
      if(!(tmthing->flags & MF_TELEPORT) && tm->tmfloorz - tmthing->z > 24*FRACUNIT)
         return false; // too big a step up
      if(!(tmthing->flags & (MF_DROPOFF|MF_FLOAT)) && tm->tmfloorz - tm->tmdropoffz > 24*FRACUNIT)
         return false; // don't stand over a dropoff
   }

   // the move is ok, so link the thing into its new position.
   P_UnsetThingPosition(tmthing);
   tmthing->floorz   = tm->tmfloorz;
   tmthing->ceilingz = tm->tmceilingz;
   tmthing->x        = tm->tmx;
   tmthing->y        = tm->tmy;
   P_SetThingPosition2(tmthing, newsubsec);

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

