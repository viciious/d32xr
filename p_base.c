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
#include "mars.h"

static boolean PB_CheckPosition(pcheckwork_t *w, subsector_t **pnewsubsec) ATTR_DATA_CACHE_ALIGN;
static boolean PB_TryMove(pcheckwork_t *w, mobj_t* mo, fixed_t tryx, fixed_t tryy) ATTR_DATA_CACHE_ALIGN;
static void P_FloatChange(mobj_t* mo) ATTR_DATA_CACHE_ALIGN;
void P_ZMovement(mobj_t* mo) ATTR_DATA_CACHE_ALIGN;
void P_MobjThinker(mobj_t* mobj) ATTR_DATA_CACHE_ALIGN;
void P_XYMovement(mobj_t* mo) ATTR_DATA_CACHE_ALIGN;

//
// Check an mobj's position for validity against lines and other mobjs
//
static boolean PB_CheckPosition(pcheckwork_t *w, subsector_t **pnewsubsec)
{
   int xl, xh, yl, yh, bx, by;
   VINT *lvalidcount;
   mobj_t *mo = w->tmthing;
   subsector_t *testsubsec;

   w->tmbbox[BOXTOP   ] = w->tmy + mo->radius;
   w->tmbbox[BOXBOTTOM] = w->tmy - mo->radius;
   w->tmbbox[BOXRIGHT ] = w->tmx + mo->radius;
   w->tmbbox[BOXLEFT  ] = w->tmx - mo->radius;

   // the base floor / ceiling is from the subsector that contains the point.
   // Any contacted lines the step closer together will adjust them.
   testsubsec = *pnewsubsec = R_PointInSubsector(w->tmx, w->tmy);
   w->tmfloorz   = w->tmdropoffz = testsubsec->sector->floorheight;
   w->tmceilingz = testsubsec->sector->ceilingheight;

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   *lvalidcount = *lvalidcount + 1;
   if (*lvalidcount == 0)
      *lvalidcount = 1;

   w->numspechit  = 0;
   w->ceilingline = NULL;
   w->hitthing    = NULL;

   // the bounding box is extended by MAXRADIUS because mobj_ts are grouped into
   // mapblocks based on their origin point, and can overlap into adjacent blocks
   // by up to MAXRADIUS units
   xl = w->tmbbox[BOXLEFT  ] - bmaporgx - MAXRADIUS;
   xh = w->tmbbox[BOXRIGHT ] - bmaporgx + MAXRADIUS;
   yl = w->tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS;
   yh = w->tmbbox[BOXTOP   ] - bmaporgy + MAXRADIUS;

   if(xl < 0)
      xl = 0;
   if(yl < 0)
      yl = 0;
   if(xh < 0)
      return true;
   if(yh < 0)
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
         if(!P_BlockThingsIterator(bx, by, (blockthingsiter_t)PIT_CheckThing, w))
            return false;
         if(!P_BlockLinesIterator(bx, by, (blocklinesiter_t)PIT_CheckLine, w))
            return false;
      }
   }

   return true;
}

// 
// Try to move to the new position, and relink the mobj to the new position if
// successful.
//
static boolean PB_TryMove(pcheckwork_t *w, mobj_t *mo, fixed_t tryx, fixed_t tryy)
{
   subsector_t *testsubsec;

   w->tmx = tryx;
   w->tmy = tryy;
   w->tmthing = mo; // store for PB_CheckThing

   if(!PB_CheckPosition(w, &testsubsec))
      return false; // solid wall or thing

   if(w->tmceilingz - w->tmfloorz < mo->height)
      return false; // doesn't fit
   if(w->tmceilingz - mo->z < mo->height)
      return false; // mobj must lower itself to fit
   if(w->tmfloorz - mo->z > 24*FRACUNIT)
      return false; // too big a step up
   if(!(mo->flags & (MF_DROPOFF|MF_FLOAT)) && w->tmfloorz - w->tmdropoffz > 24*FRACUNIT)
      return false; // don't stand over a dropoff

   // the move is ok, so link the thing into its new position
   P_UnsetThingPosition(mo);
   mo->floorz   = w->tmfloorz;
   mo->ceilingz = w->tmceilingz;
   mo->x        = tryx;
   mo->y        = tryy;
   P_SetThingPosition2(mo, testsubsec);

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
      pcheckwork_t w;

      xleft -= xuse;
      yleft -= yuse;

      if(!PB_TryMove(&w, mo, mo->x + xuse, mo->y + yuse))
      {
         // blocked move

         // flying skull?
         if(mo->flags & MF_SKULLFLY)
         {
            mo->extradata = (intptr_t)w.hitthing;
            mo->latecall = L_SkullBash;
            return;
         }

         // explode a missile?
         if(mo->flags & MF_MISSILE)
         {
            if(w.ceilingline && w.ceilingline->sidenum[1] != -1 && LD_BACKSECTOR(w.ceilingline)->ceilingpic == -1)
            {
               mo->latecall = P_RemoveMobj;
               return;
            }
            mo->extradata = (intptr_t)w.hitthing;
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
      fixed_t gravity = (GRAVITY * vblsinframe) / TICVBLS;
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
   if (!(mobj->flags & MF_STATIC))
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
   }

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
#ifdef MARS
        // clear cache for mobj flags following the sight check as 
        // the other CPU might have modified the MF_SEETARGET state
        if (mo->tics == 1)
            Mars_ClearCacheLine(&mo->flags);
#endif
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
        if (mo->flags & MF_STATIC)
            continue;
        if (mo->latecall)
        {
            mo->latecall(mo);
            mo->latecall = NULL;
        }
    }

    /* move entities, removed this frame, from limbo to free list */
    for (mo = limbomobjhead.next; mo != (void*)&limbomobjhead; mo = next)
    {
        next = mo->next;
        P_FreeMobj(mo);
    }
}
