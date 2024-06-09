/*
  CALICO
  
  Line-of-sight checking

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
#include "mars.h"

typedef struct 
{
   int16_t x, y, dx, dy;
} i16divline_t;

typedef struct
{
    fixed_t sightzstart;           // eye z of looker
    fixed_t topslope, bottomslope; // slopes to top and bottom of target

    i16divline_t strace;  // from t1 to t2
    fixed_t t2x, t2y;
} sightWork_t;

static fixed_t PS_InterceptVector2(i16divline_t* v2, i16divline_t* v1) ATTR_DATA_CACHE_ALIGN;
static boolean PS_CrossSubsector(sightWork_t* sw, int num) ATTR_DATA_CACHE_ALIGN;
static boolean PS_CrossBSPNode(sightWork_t* sw, int bspnum) ATTR_DATA_CACHE_ALIGN;
static boolean PS_RejectCheckSight(mobj_t* t1, mobj_t* t2) ATTR_DATA_CACHE_ALIGN;
static boolean P_MobjCanSightCheck(mobj_t *mobj) ATTR_DATA_CACHE_ALIGN;
static mobj_t *P_GetSightMobj(mobj_t *pmobj, int c, int *pcnt) ATTR_DATA_CACHE_ALIGN;
static boolean PS_CheckSight2(mobj_t* t1, mobj_t* t2) ATTR_DATA_CACHE_ALIGN;
#ifdef MARS
void P_CheckSights2(int c) ATTR_DATA_CACHE_ALIGN;
#else
void P_CheckSights2(void) ATTR_DATA_CACHE_ALIGN;
#endif

// Returns side 0 (front), 1 (back)
#define PS_DivlineSide(xx,yy,n) (((n)->dy * ((xx) - (n)->x)) <= (((yy) - (n)->y) * (n)->dx))

//
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings and addlines traversers.
//
static fixed_t PS_InterceptVector2(i16divline_t *v2, i16divline_t *v1)
{
   fixed_t frac, num, den;
 #ifdef MARS
   fixed_t temp;

   den =  (int32_t)v1->dy * v2->dx;
   den -= (int32_t)v1->dx * v2->dy;
   if (den == 0)
     return 0;

   num =  (int32_t)(v1->x - v2->x) * v1->dy;
   num -= (int32_t)(v1->y - v2->y) * v1->dx;

   __asm volatile (
      "mov #-128, %1\n\t"
      "add %1, %1 /* %1 is now 0xFFFFFF00 */ \n\t"
      "mov.l %3, @(0,%1) /* set 32-bit divisor */ \n\t"
      "swap.w %2, %0\n\t"
      "exts.w %0, %0\n\t"
      "mov.l %0, @(16,%1)\n\t"
      "shll16 %2\n\t"
      "mov.l %2, @(20,%1) /* start divide */\n\t"
      "mov.l @(20,%1), %0 /* get 32-bit quotient */ \n\t"
      : "=&r" (frac), "=&r" (temp)
      : "r" (num), "r" (den)
   );
#else
   fixed_t temp;

   den = FixedMul(v1->dy<<8,v2->dx<<8);
   temp = FixedMul(v1->dx<<8, v2->dy<<8);

   den = den - temp;
   if(den == 0)
      return 0;

   temp = FixedMul((v2->y - v1->y)< 8, v1->dx<<8);
   num = FixedMul((v1->x - v2->x)<<8, v1->dy<<8);

   num  = num + temp;
   frac = FixedDiv(num, den);
#endif

   return frac;
}

/*
=================
=
= PS_CrossSubsector
=
= Returns true if strace crosses the given subsector successfuly
=================
*/

static boolean PS_CrossSubsector(sightWork_t *sw, int num)
{
   seg_t       *seg;
   line_t      *line;
   VINT         s1;
   VINT         s2;
   int          count;
   subsector_t *sub;
   sector_t    *front;
   sector_t    *back;
   fixed_t      opentop;
   fixed_t      openbottom;
   i16divline_t divl;
   mapvertex_t  *v1, *v2;
   fixed_t      frac;
   fixed_t      slope;
   i16divline_t *strace = &sw->strace;
   int16_t      t2x = sw->t2x, t2y = sw->t2y;
   fixed_t      sightzstart = sw->sightzstart;
   VINT         *lvalidcount, vc;
   VINT         side;

   sub = &subsectors[num];

   // check lines
   count = sub->numlines;
   seg   = &segs[sub->firstline];

	I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
	vc = *lvalidcount;
	++lvalidcount;

   for( ; count; seg++, count--)
   {
      line = &lines[seg->linedef];

      // allready checked other side?
      if(lvalidcount[seg->linedef] == vc)
         continue;
      lvalidcount[seg->linedef] = vc;

      v1 = &vertexes[line->v1];
      v2 = &vertexes[line->v2];

      s1 = PS_DivlineSide(v1->x, v1->y, strace);
      s2 = PS_DivlineSide(v2->x, v2->y, strace);

      // line isn't crossed?
      if (s1 == s2)
         continue;

      divl.x = v1->x;
      divl.y = v1->y;
      divl.dx = v2->x - v1->x;
      divl.dy = v2->y - v1->y;
      s1 = PS_DivlineSide (strace->x, strace->y, &divl);
      s2 = PS_DivlineSide (t2x, t2y, &divl);

      // line isn't crossed?
      if (s1 == s2)
         continue;

      // stop because it is not two sided anyway
      if(!(line->flags & ML_TWOSIDED))
         return false;

      // crosses a two sided line
      side = seg->sideoffset & 1;
      front = &sectors[sides[line->sidenum[side]].sector];
      back = &sectors[sides[line->sidenum[side^1]].sector];

      // no wall to block sight with?
      if(front->floorheight == back->floorheight && front->ceilingheight == back->ceilingheight)
         continue;

      // possible occluder
      // because of ceiling height differences
      if (front->ceilingheight < back->ceilingheight)
         opentop = front->ceilingheight;
      else
         opentop = back->ceilingheight;

      // because of ceiling height differences
      if (front->floorheight > back->floorheight)
         openbottom = front->floorheight;
      else
         openbottom = back->floorheight;

      // quick test for totally closed doors
      if(openbottom >= opentop)
         return false; // stop

      frac = PS_InterceptVector2(strace, &divl);

      if(front->floorheight != back->floorheight)
      {
         slope = FixedDiv(openbottom - sightzstart , frac);
         if(slope > sw->bottomslope)
             sw->bottomslope = slope;
      }

      if(front->ceilingheight != back->ceilingheight)
      {
         slope = FixedDiv (opentop - sightzstart , frac);
         if(slope < sw->topslope)
             sw->topslope = slope;
      }

      if(sw->topslope <= sw->bottomslope)
         return false;    // stop
   }

   // passed the subsector ok
   return true;
}

//
// Returns true if strace crosses the given node successfuly
//
static boolean PS_CrossBSPNode(sightWork_t* sw, int bspnum)
{
   node_t *bsp;
   int side, side2;
   i16divline_t *strace = &sw->strace;

#ifdef MARS
   while ((int16_t)bspnum >= 0)
#else
   while (!(bspnum & NF_SUBSECTOR))
#endif
   {
      bsp = &nodes[bspnum];

      // decide which side the start point is on
      side = PS_DivlineSide(strace->x, strace->y, (i16divline_t *)bsp);
      side2 = PS_DivlineSide(sw->t2x, sw->t2y, (i16divline_t *)bsp);

      // the partition plane is crossed here
      if (side == side2)
         bspnum = bsp->children[side]; // the line doesn't touch the other side
      else if (!PS_CrossBSPNode(sw, bsp->children[side]))
         return false; // cross the starting side
      else
         bspnum = bsp->children[side ^ 1]; // cross the ending side
   }

   return PS_CrossSubsector(sw, bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}

//
// Returns false if a straight line between t1 and t2 is obstructed
//
static boolean PS_RejectCheckSight(mobj_t *t1, mobj_t *t2)
{
   unsigned s1, s2;
   unsigned pnum, bitnum;
   int bytenum;

   // First check for trivial rejection
   s1 = ((uintptr_t)t1->subsector->sector - (uintptr_t)sectors);
   s2 = ((uintptr_t)t2->subsector->sector - (uintptr_t)sectors);
   pnum = (s1*numsectors + s2) / sizeof(sector_t);
   bytenum = pnum >> 3;

#ifdef MARS
   bitnum = pnum & 7;
   __asm volatile(
      "add #-7,%0\n\t"
      "neg %0,%0\n\t"
      "add %0,%0\n\t"
      "braf %0\n\t"
      "mov #1,%0\n\t"
      "shll %0\n\t"
      "shll %0\n\t"
      "shll %0\n\t"
      "shll %0\n\t"
      "shll %0\n\t"
      "shll %0\n\t"
      "shll %0\n\t"
      : "+r"(bitnum)
   );
#else
   bitnum = 1;
   switch (pnum & 7)
   {
   case 7: do { bitnum <<= 1;
   case 6:      bitnum <<= 1;
   case 5:      bitnum <<= 1;
   case 4:      bitnum <<= 1;
   case 3:      bitnum <<= 1;
   case 2:      bitnum <<= 1;
   case 1:      bitnum <<= 1;
   case 0:      break;
   } while (0);
   }
#endif

   if((int8_t)rejectmatrix[bytenum] & bitnum) 
   {
      return false; // can't possibly be connected
   }

   return true;
}

//
// Returns true if a straight line between t1 and t2 is unobstructed
//
static boolean PS_CheckSight2(mobj_t *t1, mobj_t *t2)
{
   sightWork_t sw;
   VINT *lvalidcount;

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   *lvalidcount = *lvalidcount + 1;
   if (*lvalidcount == 0)
      *lvalidcount = 1;

   // look from eyes of t1 to any part of t2
   sw.sightzstart = t1->z + t1->height - (t1->height >> 2);
   sw.topslope    = (t2->z + t2->height) - sw.sightzstart;
   sw.bottomslope = (t2->z) - sw.sightzstart;

   // make sure it never lies exactly on a vertex coordinate
   sw.strace.x = ((t1->x & ~0x1ffff) | 0x10000) >> FRACBITS;
   sw.strace.y = ((t1->y & ~0x1ffff) | 0x10000) >> FRACBITS;
   sw.t2x = ((t2->x & ~0x1ffff) | 0x10000) >> FRACBITS;
   sw.t2y = ((t2->y & ~0x1ffff) | 0x10000) >> FRACBITS;
   sw.strace.dx = sw.t2x - sw.strace.x;
   sw.strace.dy = sw.t2y - sw.strace.y;

   return PS_CrossBSPNode(&sw, numnodes-1);
}

static boolean P_MobjCanSightCheck(mobj_t *mobj)
{
   // must be killable or be a monster spawner
   if (!(mobj->flags & MF_COUNTKILL) && (mobj->type != MT_BOSSSPIT))
      return false;

   // must be about to change states
   if (mobj->tics != 1)
      return false;

   mobj->flags &= ~MF_SEETARGET;

   // must have a target
   if (!mobj->target)
      return false;

   if (!PS_RejectCheckSight(mobj, mobj->target))
      return false;

   return true;
}

#ifdef MARS
static char ps_lock = 0;

static void P_LockSight(void)
{
    int res;
    do {
        __asm volatile (\
        "tas.b %1\n\t" \
            "movt %0\n\t" \
            : "=&r" (res) \
            : "m" (ps_lock) \
            );
    } while (res == 0);
}

static void P_UnlockSight(void)
{
   ps_lock = 0;
}

static mobj_t *P_GetSightMobj(mobj_t *mobj, int c, int *pcnt)
{
   int next;
   int cnt = *pcnt;

   P_LockSight();

   for (next = MARS_SYS_COMM6; ; next++)
   {
      if (c == 1)
      {
         for (; cnt < next; cnt++)
         {
            if (mobj == (void*)&mobjhead)
               goto done;
            Mars_ClearCacheLine(&mobj->next);
            mobj = mobj->next;
         }
         Mars_ClearCacheLines(mobj, (sizeof(mobj_t)+31)/16);
      }
      else
      {
         for (; cnt < next; cnt++)
         {
            if (mobj == (void*)&mobjhead)
               goto done;
            mobj = mobj->next;
         }
      }

      if (P_MobjCanSightCheck(mobj))
        break;
   }

done:
   MARS_SYS_COMM6 = cnt + 1;

   P_UnlockSight();

   *pcnt = cnt;
   return mobj;
}

#define P_NextSightMobj(mobj) (mobj)

#else

static mobj_t *P_GetSightMobj(mobj_t *mobj, int c, int *pcnt)
{
   for ( ; mobj != (void*)&mobjhead; mobj = mobj->next)
   {
      if (P_MobjCanSightCheck(mobj))
         break;
   }
   return mobj;
}

#define P_NextSightMobj(mobj) (mobj)->next

#endif

//
// Optimal mobj sight checking that checks sights in the main tick loop rather
// than from multiple mobj action routines.
//
#ifdef MARS
void P_CheckSights2(int c)
#else
void P_CheckSights2(void)
#endif
{
    mobj_t *mobj;
    int cnt = 0;
#ifndef MARS
    int c = 0;
#else
    mobj_t *ctrgt = NULL;
    Mars_ClearCacheLines(&mobjhead.next, 1);
#endif

    for (mobj = mobjhead.next; ; mobj = P_NextSightMobj(mobj))
    {
        if ((mobj = P_GetSightMobj(mobj, c, &cnt)) == (void*)&mobjhead)
            return;

#ifdef MARS
        if (c == 1 && ctrgt != mobj->target)
        {
           Mars_ClearCacheLines(mobj->target, (sizeof(mobj_t)+31)/16);
           ctrgt = mobj->target;
        }
#endif

        if (PS_CheckSight2(mobj, mobj->target))
           mobj->flags |= MF_SEETARGET;
    }
}

// EOF

