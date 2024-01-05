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

static void P_FloatChange(mobj_t* mo) ATTR_DATA_CACHE_ALIGN;
void P_ZMovement(mobj_t* mo) ATTR_DATA_CACHE_ALIGN;
void P_MobjThinker(mobj_t* mobj) ATTR_DATA_CACHE_ALIGN;
void P_XYMovement(mobj_t* mo) ATTR_DATA_CACHE_ALIGN;

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
      pmovework_t w;

      xleft -= xuse;
      yleft -= yuse;

      if(!P_TryMove(&w, mo, mo->x + xuse, mo->y + yuse))
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
      if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
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

      if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
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
