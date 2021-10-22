/*
  CALICO
  
  Core mobj thinking, movement, and clipping checks.
  Derived from Doom 64 EX, used under MIT by permission.

  The MIT License (MIT)

  Copyright (C) 2016 Samuel Villarreal, James Haley
  
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

static mobj_t      *checkthing, *hitthing;
static fixed_t      testx, testy;
static fixed_t      testfloorz, testceilingz, testdropoffz;
static subsector_t *testsubsec;
static line_t      *ceilingline;
static fixed_t      testbbox[4];
static int          testflags;

static boolean PB_CheckThing(mobj_t* thing) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PB_BoxCrossLine(line_t* ld) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PB_CheckLine(line_t* ld) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PB_CrossCheck(line_t* ld) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PB_CheckPosition(mobj_t* mo) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PB_TryMove(mobj_t* mo, fixed_t tryx, fixed_t tryy) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void P_FloatChange(mobj_t* mo) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void P_ZMovement(mobj_t* mo) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void P_MobjThinker(mobj_t* mobj) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void P_XYMovement(mobj_t* mo) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

//
// Check for collision against another mobj in one of the blockmap cells.
//
static boolean PB_CheckThing(mobj_t *thing)
{
   fixed_t  blockdist;
   int      delta;
   mobj_t  *mo;

   if(!(thing->flags & MF_SOLID))
      return true; // not blocking

   mo = checkthing;
   blockdist = thing->radius + mo->radius;

   delta = thing->x - testx;
   if(delta < 0)
      delta = -delta;
   if(delta >= blockdist)
      return true; // didn't hit it

   delta = thing->y - testy;
   if(delta < 0)
      delta = -delta;
   if(delta >= blockdist)
      return true; // didn't hit it

   if(thing == mo)
      return true; // don't clip against self

   // check for skulls slamming into things
   if(testflags & MF_SKULLFLY)
   {
      hitthing = thing;
      return false;
   }

   // missiles can hit other things
   if(testflags & MF_MISSILE)
   {
      if(mo->z > thing->z + thing->height)
         return true; // went over
      if(mo->z + mo->height < thing->z)
         return true; // went underneath
      if(mo->target->type == thing->type) // don't hit same species as originator
      {
         if(thing == mo->target)
            return true; // don't explode on shooter
         if(thing->type != MT_PLAYER) // let players missile other players
            return false; // explode but do no damage
      }

      if(!(thing->flags & MF_SHOOTABLE))
         return !(thing->flags & MF_SOLID); // didn't do any damage

      // damage/explode
      hitthing = thing;
      return false;
   }

   return !(thing->flags & MF_SOLID);
}

//
// Test for a bounding box collision with a linedef.
//
static boolean PB_BoxCrossLine(line_t *ld)
{
   fixed_t x1, x2;
   fixed_t lx, ly;
   fixed_t ldx, ldy;
   fixed_t dx1, dy1, dx2, dy2;
   boolean side1, side2;
   const fixed_t *ldbbox = P_LineBBox(ld);

   // entirely outside bounding box of line?
   if(testbbox[BOXRIGHT ] <= ldbbox[BOXLEFT  ] ||
      testbbox[BOXLEFT  ] >= ldbbox[BOXRIGHT ] ||
      testbbox[BOXTOP   ] <= ldbbox[BOXBOTTOM] ||
      testbbox[BOXBOTTOM] >= ldbbox[BOXTOP   ])
   {
      return false;
   }

   if(ld->slopetype == ST_POSITIVE)
   {
      x1 = testbbox[BOXLEFT ];
      x2 = testbbox[BOXRIGHT];
   }
   else
   {
      x1 = testbbox[BOXRIGHT];
      x2 = testbbox[BOXLEFT ];
   }

   lx  = ld->v1->x;
   ly  = ld->v1->y;
   ldx = (ld->v2->x - ld->v1->x) >> FRACBITS;
   ldy = (ld->v2->y - ld->v1->y) >> FRACBITS;

   dx1 = (x1 - lx) >> FRACBITS;
   dy1 = (testbbox[BOXTOP] - ly) >> FRACBITS;
   dx2 = (x2 - lx) >> FRACBITS;
   dy2 = (testbbox[BOXBOTTOM] - ly) >> FRACBITS;

   side1 = (ldy * dx1 < dy1 * ldx);
   side2 = (ldy * dx2 < dy2 * ldx);

   return (side1 != side2);
}

//
// Adjusts testfloorz and testceilingz as lines are contacted.
//
static boolean PB_CheckLine(line_t *ld)
{
   fixed_t   opentop, openbottom, lowfloor;
   sector_t *front, *back;

   // The moving thing's destination position will cross the given line.
   // if this should not be allowed, return false.
   if(ld->sidenum[1] == -1)
      return false; // one-sided line

   if(!(testflags & MF_MISSILE) && (ld->flags & (ML_BLOCKING|ML_BLOCKMONSTERS)))
      return false; // explicitly blocking

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
   if(opentop < testceilingz)
   {
      testceilingz = opentop;
      ceilingline  = ld;
   }
   if(openbottom > testfloorz)
      testfloorz = openbottom;
   if(lowfloor < testdropoffz)
      testdropoffz = lowfloor;

   return true;
}

//
// Check a thing against a linedef in one of the blockmap cells.
//
static boolean PB_CrossCheck(line_t *ld)
{
   if(PB_BoxCrossLine(ld))
   {
      if(!PB_CheckLine(ld))
         return false;
   }
   return true;
}

//
// Check an mobj's position for validity against lines and other mobjs
//
static boolean PB_CheckPosition(mobj_t *mo)
{
   int xl, xh, yl, yh, bx, by;

   testflags = mo->flags;

   testbbox[BOXTOP   ] = testy + mo->radius;
   testbbox[BOXBOTTOM] = testy - mo->radius;
   testbbox[BOXRIGHT ] = testx + mo->radius;
   testbbox[BOXLEFT  ] = testx - mo->radius;

   // the base floor / ceiling is from the subsector that contains the point.
   // Any contacted lines the step closer together will adjust them.
   testsubsec   = R_PointInSubsector(testx, testy);
   testfloorz   = testdropoffz = testsubsec->sector->floorheight;
   testceilingz = testsubsec->sector->ceilingheight;

   ++validcount;

   ceilingline = NULL;
   hitthing    = NULL;

   // the bounding box is extended by MAXRADIUS because mobj_ts are grouped into
   // mapblocks based on their origin point, and can overlap into adjacent blocks
   // by up to MAXRADIUS units
   xl = (testbbox[BOXLEFT  ] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
   xh = (testbbox[BOXRIGHT ] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
   yl = (testbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
   yh = (testbbox[BOXTOP   ] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

   if(xl < 0)
      xl = 0;
   if(yl < 0)
      yl = 0;
   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;

   checkthing = mo; // store for PB_CheckThing
   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockThingsIterator(bx, by, PB_CheckThing))
            return false;
         if(!P_BlockLinesIterator(bx, by, PB_CrossCheck))
            return false;
      }
   }

   return true;
}

// 
// Try to move to the new position, and relink the mobj to the new position if
// successful.
//
static boolean PB_TryMove(mobj_t *mo, fixed_t tryx, fixed_t tryy)
{
   testx = tryx;
   testy = tryy;

   if(!PB_CheckPosition(mo))
      return false; // solid wall or thing

   if(testceilingz - testfloorz < mo->height)
      return false; // doesn't fit
   if(testceilingz - mo->z < mo->height)
      return false; // mobj must lower itself to fit
   if(testfloorz - mo->z > 24*FRACUNIT)
      return false; // too big a step up
   if(!(testflags & (MF_DROPOFF|MF_FLOAT)) && testfloorz - testdropoffz > 24*FRACUNIT)
      return false; // don't stand over a dropoff

   // the move is ok, so link the thing into its new position
   P_UnsetThingPosition(mo);
   mo->floorz   = testfloorz;
   mo->ceilingz = testceilingz;
   mo->x        = tryx;
   mo->y        = tryy;
   P_SetThingPosition(mo);

   return true;
}

#define STOPSPEED 0x1000
#define FRICTION  0xd240

//
// Do horizontal movement.
//
void P_XYMovement(mobj_t *mo)
{
   fixed_t xleft, yleft, xuse, yuse;

   xleft = xuse = mo->momx & ~7;
   yleft = yuse = mo->momy & ~7;

   while(xuse > MAXMOVE || xuse < -MAXMOVE || yuse > MAXMOVE || yuse < -MAXMOVE)
   {
      xuse >>= 1;
      yuse >>= 1;
   }

   while(xleft || yleft)
   {
      xleft -= xuse;
      yleft -= yuse;

      if(!PB_TryMove(mo, mo->x + xuse, mo->y + yuse))
      {
         // blocked move

         // flying skull?
         if(mo->flags & MF_SKULLFLY)
         {
            mo->extradata = (intptr_t)hitthing;
            mo->latecall = L_SkullBash;
            return;
         }

         // explode a missile?
         if(mo->flags & MF_MISSILE)
         {
            if(ceilingline && ceilingline->sidenum[1] != -1 && LD_BACKSECTOR(ceilingline)->ceilingpic == -1)
            {
               mo->latecall = P_RemoveMobj;
               return;
            }
            mo->extradata = (intptr_t)hitthing;
            mo->latecall = L_MissileHit;
            return;
         }

         mo->momx = mo->momy = 0;
         return;
      }
   }

   // slow down

   if(mo->flags & (MF_MISSILE|MF_SKULLFLY))
      return; // no friction for missiles or flying skulls ever

   if(mo->z > mo->floorz)
      return; // no friction when airborne

   if(mo->flags & MF_CORPSE && mo->floorz != mo->subsector->sector->floorheight)
      return; // sliding corpse: don't stop halfway off a step

   if(mo->momx > -STOPSPEED && mo->momx < STOPSPEED &&
      mo->momy > -STOPSPEED && mo->momy < STOPSPEED)
   {
      mo->momx = 0;
      mo->momy = 0;
   }
   else
   {
      mo->momx = (mo->momx >> 8) * (FRICTION >> 8);
      mo->momy = (mo->momy >> 8) * (FRICTION >> 8);
   }
}

//
// Float a flying monster up or down.
//
static void P_FloatChange(mobj_t *mo)
{
   mobj_t *target;
   fixed_t dist, delta;

   target = mo->target;                              // get the target object
   delta  = (target->z + (mo->height >> 1)) - mo->z; // get the height difference
   
   dist   = P_AproxDistance(target->x - mo->x, target->y - mo->y);
   delta *= 3;

   if(delta < 0)
   {
      if(dist < -delta)
         mo->z -= FLOATSPEED; // adjust height downward
   }
   else if(dist < delta)
      mo->z += FLOATSPEED;    // adjust height upward
}

//
// Do movement on the z axis.
//
void P_ZMovement(mobj_t *mo)
{
   mo->z += mo->momz;

   if((mo->flags & MF_FLOAT) && mo->target)
   {
      // float toward target if too close
      P_FloatChange(mo);
   }

   // clip movement
   if(mo->z <= mo->floorz)
   {
      if (mo->flags & MF_SKULLFLY)
      {
          // the skull slammed into something
          mo->momz = -mo->momz;
      }

      if(mo->momz < 0)
         mo->momz = 0;
      mo->z = mo->floorz; // hit the floor
      if(mo->flags & MF_MISSILE)
      {
         mo->latecall = P_ExplodeMissile;
         return;
      }
   }
   else if(!(mo->flags & MF_NOGRAVITY))
   {
      // apply gravity
      fixed_t gravity = (GRAVITY * vblsinframe[0]) / TICVBLS;
      if(!mo->momz)
         mo->momz = -gravity;
      else
         mo->momz -= gravity/2;
   }

   if(mo->z + mo->height > mo->ceilingz)
   {
      if(mo->momz > 0)
         mo->momz = 0;
      mo->z = mo->ceilingz - mo->height; // hit the ceiling

      if (mo->flags & MF_SKULLFLY)
      {
          // the skull slammed into something
          mo->momz = -mo->momz;
      }

      if(mo->flags & MF_MISSILE)
         mo->latecall = P_ExplodeMissile;
   }
}

//
// Perform main thinking logic for a single mobj per tic.
//
void P_MobjThinker(mobj_t *mobj)
{
   // momentum movement
   if(mobj->momx || mobj->momy)
      P_XYMovement(mobj);

   // removed or has a special action to perform?
   if(mobj->latecall)
      return;

   if(mobj->z != mobj->floorz || mobj->momz)
      P_ZMovement(mobj);

   // removed or has a special action to perform?
   if(mobj->latecall)
      return;

   // cycle through states
   if (mobj->tics != -1)
   {
       mobj->tics--;

       // you can cycle through multiple states in a tic
       if (!mobj->tics)
           P_SetMobjState(mobj, states[mobj->state].nextstate);
   }
}

//
// Do the main thinking logic for mobjs during a gametic.
//
void P_RunMobjBase2(void)
{
    mobj_t* mo;
    mobj_t* next;

    for (mo = mobjhead.next; mo != (void*)&mobjhead; mo = next)
    {
        next = mo->next;	/* in case mo is removed this time */
        if (!mo->player)
            P_MobjThinker(mo);
    }
}

/*
===================
=
= P_RunMobjLate
=
= Run stuff that doesn't happen every tick
===================
*/

void P_RunMobjLate(void)
{
    mobj_t* mo;
    mobj_t* next;

    for (mo = mobjhead.next; mo != (void*)&mobjhead; mo = next)
    {
        next = mo->next;	/* in case mo is removed this time */
        if (mo->latecall)
            mo->latecall(mo);
        mo->latecall = NULL;
    }

    /* move entities, removed this frame, from limbo to free list */
    for (mo = limbomobjhead.next; mo != (void*)&limbomobjhead; mo = next)
    {
        next = mo->next;
        P_FreeMobj(mo);
    }
}
