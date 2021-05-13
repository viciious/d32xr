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
extern mobj_t  *tmthing;
extern fixed_t  tmx, tmy;
extern boolean  checkposonly;

boolean  trymove2;   // result from P_TryMove2
boolean  floatok;    // if true, move would be ok if within tmfloorz - tmceilingz
fixed_t  tmfloorz;   // current floor z for P_TryMove2
fixed_t  tmceilingz; // current ceiling z for P_TryMove2
mobj_t  *movething;  // skull/missile target, or special
line_t  *blockline;  // possibly a special to activate

static fixed_t oldx, oldy;
static fixed_t tmbbox[4];
static int     tmflags;
static fixed_t tmdropoffz; // lowest point contacted

static subsector_t *newsubsec; // destination subsector

boolean PIT_CheckThing(mobj_t* thing) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PM_BoxCrossLine(line_t* ld) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PIT_CheckLine(line_t* ld) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PM_CrossCheck(line_t* ld) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void PM_CheckPosition(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void P_TryMove2(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

//
// Check a single mobj in one of the contacted blockmap cells.
//
boolean PIT_CheckThing(mobj_t *thing)
{
   fixed_t blockdist;
   int     delta;

   if(!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE)))
      return true;

   blockdist = thing->radius + tmthing->radius;
   
   delta = thing->x - tmx;
   if(delta < 0)
      delta = -delta;
   if(delta >= blockdist)
      return true; // didn't hit it

   delta = thing->y - tmy;
   if(delta < 0)
      delta = -delta;
   if(delta >= blockdist)
      return true; // didn't hit it

   if(thing == tmthing)
      return true; // don't clip against self

   // check for skulls slamming into things
   if(tmthing->flags & MF_SKULLFLY)
   {
      movething = thing;
      return false; // stop moving
   }

   // missiles can hit other things
   if(tmthing->flags & MF_MISSILE)
   {
      if(tmthing->z > thing->z + thing->height)
         return true; // went overhead
      if(tmthing->z + tmthing->height < thing->z)
         return true; // went underneath
      if(tmthing->target->type == thing->type) // don't hit same species as originator
      {
         if(thing == tmthing->target) // don't hit originator
            return true;
         if(thing->type != MT_PLAYER) // let players missile each other
            return false; // explode, but do no damage
      }
      if(!(thing->flags & MF_SHOOTABLE))
         return !(thing->flags & MF_SOLID); // didn't do any damage

      // damage/explode
      movething = thing;
      return false; // don't traverse any more
   }

   // check for special pickup
   if((thing->flags & MF_SPECIAL) && (tmflags & MF_PICKUP))
   {
      movething = thing;
      return true;
   }

   return !(thing->flags & MF_SOLID);
}

//
// Check if the thing intersects a linedef
//
static boolean PM_BoxCrossLine(line_t *ld)
{
   fixed_t x1, x2, y1, y2;
   fixed_t lx, ly, ldx, ldy;
   fixed_t dx1, dx2, dy1, dy2;
   boolean side1, side2;
   fixed_t *ldbbox = P_LineBBox(ld);

   if(tmbbox[BOXRIGHT ] <= ldbbox[BOXLEFT  ] ||
      tmbbox[BOXLEFT  ] >= ldbbox[BOXRIGHT ] ||
      tmbbox[BOXTOP   ] <= ldbbox[BOXBOTTOM] ||
      tmbbox[BOXBOTTOM] >= ldbbox[BOXTOP   ])
   {
      return false; // bounding boxes don't intersect
   }

   y1 = tmbbox[BOXTOP   ];
   y2 = tmbbox[BOXBOTTOM];

   if(ld->slopetype == ST_POSITIVE)
   {
      x1 = tmbbox[BOXLEFT ];
      x2 = tmbbox[BOXRIGHT];
   }
   else
   {
      x1 = tmbbox[BOXRIGHT];
      x2 = tmbbox[BOXLEFT ];
   }

   lx  = ld->v1->x;
   ly  = ld->v1->y;
   ldx = (ld->v2->x - lx) >> FRACBITS;
   ldy = (ld->v2->y - ly) >> FRACBITS;

   dx1 = (x1 - lx) >> FRACBITS;
   dy1 = (y1 - ly) >> FRACBITS;
   dx2 = (x2 - lx) >> FRACBITS;
   dy2 = (y2 - ly) >> FRACBITS;

   side1 = (ldy * dx1 < dy1 * ldx);
   side2 = (ldy * dx2 < dy2 * ldx);

   return (side1 != side2);
}

//
// Adjusts tmfloorz and tmceilingz as lines are contacted.
//
static boolean PIT_CheckLine(line_t *ld)
{
   fixed_t   opentop, openbottom, lowfloor;
   sector_t *front, *back;

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
      blockline = ld;
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
   if(opentop < tmceilingz)
      tmceilingz = opentop;
   if(openbottom > tmfloorz)
      tmfloorz = openbottom;
   if(lowfloor < tmdropoffz)
      tmdropoffz = lowfloor;

   return true;
}

//
// Check a single linedef in a blockmap cell.
//
static boolean PM_CrossCheck(line_t *ld)
{
   if(PM_BoxCrossLine(ld))
   {
      if(!PIT_CheckLine(ld))
         return false;
   }
   return true;
}

//
// This is purely informative, nothing is modified (except things picked up)
//
static void PM_CheckPosition(void)
{
   int xl, xh, yl, yh, bx, by;

   tmflags = tmthing->flags;

   tmbbox[BOXTOP   ] = tmy + tmthing->radius;
   tmbbox[BOXBOTTOM] = tmy - tmthing->radius;
   tmbbox[BOXRIGHT ] = tmx + tmthing->radius;
   tmbbox[BOXLEFT  ] = tmx - tmthing->radius;

   newsubsec = R_PointInSubsector(tmx, tmy);

   // the base floor/ceiling is from the subsector that contains the point.
   // Any contacted lines the step closer together will adjust them.
   tmfloorz   = tmdropoffz = newsubsec->sector->floorheight;
   tmceilingz = newsubsec->sector->ceilingheight;

   ++validcount;

   movething = NULL;
   blockline = NULL;

   if(tmflags & MF_NOCLIP) // thing has no clipping?
   {
      trymove2 = true;
      return;
   }

   // Check things first, possibly picking things up.
   // The bounding box is extended by MAXRADIUS because mobj_ts are grouped
   // into mapblocks based on their origin point, and can overlap into adjacent
   // blocks by up to MAXRADIUS units.
   xl = (tmbbox[BOXLEFT  ] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   xh = (tmbbox[BOXRIGHT ] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   yh = (tmbbox[BOXTOP   ] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   if(xl < 0)
      xl = 0;
   if(yl < 0)
      yl = 0;
   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;

   // check things
   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockThingsIterator(bx, by, PIT_CheckThing))
         {
            trymove2 = false;
            return;
         }
      }
   }

   // check lines
   xl = (tmbbox[BOXLEFT  ] - bmaporgx) >> MAPBLOCKSHIFT;
   xh = (tmbbox[BOXRIGHT ] - bmaporgx) >> MAPBLOCKSHIFT;
   yl = (tmbbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
   yh = (tmbbox[BOXTOP   ] - bmaporgy) >> MAPBLOCKSHIFT;

   if(xl < 0)
      xl = 0;
   if(yl < 0)
      yl = 0;
   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockLinesIterator(bx, by, PM_CrossCheck))
         {
            trymove2 = false;
            return;
         }
      }
   }

   trymove2 = true;
}

//
// Attempt to move to a new position, crossing special lines unless MF_TELEPORT
// is set.
//
void P_TryMove2(void)
{
   trymove2 = false;
   floatok  = false;

   oldx = tmthing->x;
   oldy = tmthing->y;

   PM_CheckPosition();

   if(checkposonly)
   {
      checkposonly = false;
      return;
   }

   if(!trymove2)
      return;

   if(!(tmthing->flags & MF_NOCLIP))
   {
      trymove2 = false;

      if(tmceilingz - tmfloorz < tmthing->height)
         return; // doesn't fit
      floatok = true;
      if(!(tmthing->flags & MF_TELEPORT) && tmceilingz - tmthing->z < tmthing->height)
         return; // mobj must lower itself to fit
      if(!(tmthing->flags & MF_TELEPORT) && tmfloorz - tmthing->z > 24*FRACUNIT)
         return; // too big a step up
      if(!(tmthing->flags & (MF_DROPOFF|MF_FLOAT)) && tmfloorz - tmdropoffz > 24*FRACUNIT)
         return; // don't stand over a dropoff
   }

   // the move is ok, so link the thing into its new position.
   P_UnsetThingPosition(tmthing);
   tmthing->floorz   = tmfloorz;
   tmthing->ceilingz = tmceilingz;
   tmthing->x        = tmx;
   tmthing->y        = tmy;
   P_SetThingPosition(tmthing);

   trymove2 = true;
}

// EOF

