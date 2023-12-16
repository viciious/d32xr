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

#include <limits.h>
#include "doomdef.h"
#include "p_local.h"
#include "mars.h"

static int P_DivlineSide(fixed_t x, fixed_t y, divline_t *node) ATTR_DATA_CACHE_ALIGN;
static fixed_t P_InterceptVector2(divline_t *v2, divline_t *v1) ATTR_DATA_CACHE_ALIGN;
static boolean PS_RejectCheckSight(mobj_t *t1, mobj_t *t2) ATTR_DATA_CACHE_ALIGN;
static boolean P_MobjCanSightCheck(mobj_t *mobj) ATTR_DATA_CACHE_ALIGN;
static mobj_t *P_GetSightMobj(mobj_t *pmobj, int c, int *pcnt) ATTR_DATA_CACHE_ALIGN;
static boolean PS_CheckSight2(mobj_t *t1, mobj_t *t2) ATTR_DATA_CACHE_ALIGN;
#ifdef MARS
void P_CheckSights2(int c) ATTR_DATA_CACHE_ALIGN;
#else
void P_CheckSights2(void) ATTR_DATA_CACHE_ALIGN;
#endif

//
// Returns side 0 (front), 1 (back), or 2 (on).
//
static int P_DivlineSide(fixed_t x, fixed_t y, divline_t *node)
{
   fixed_t dx;
   fixed_t dy;
   fixed_t left;
   fixed_t right;

   dx = x - node->x;
   dy = y - node->y;

   left = (node->dy >> FRACBITS) * (dx >> FRACBITS);
   right = (dy >> FRACBITS) * (node->dx >> FRACBITS);

   return (left <= right) + (left == right);
}

//
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings and addlines traversers.
//
static fixed_t P_InterceptVector2(divline_t *v2, divline_t *v1)
{
   fixed_t frac;
#ifdef MARS
#if 1
   union
   {
      int64_t i64;
      uint32_t i32[2];
   } den, num;

   den.i64 = (int64_t)v1->dy * v2->dx;
   den.i64 -= (int64_t)v1->dx * v2->dy;
   if (den.i32[0] == 0)
      return 0;

   num.i64 = (int64_t)(v1->x - v2->x) * v1->dy;
   num.i64 -= (int64_t)(v1->y - v2->y) * v1->dx;
   num.i64 >>= 16;

   do
   {
      __asm volatile(
          "mov #-128, r0\n\t"
          "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
          "mov.l %3, @(0,r0) /* set 32-bit divisor */ \n\t"
          "mov.l %1, @(16,r0)\n\t"
          "mov.l %2, @(20,r0) /* start divide */\n\t"
          "mov.l @(20,r0), %0 /* get 32-bit quotient */ \n\t"
          : "=r"(frac)
          : "r"(num.i32[0]), "r"(num.i32[1]), "r"(den.i32[0])
          : "r0");
   } while (0);
#else
   fixed_t num;
   fixed_t den, temp;

   temp = FixedMul(v1->dy, FRACUNIT / 256);
   den = FixedMul(temp, v2->dx);

   temp = FixedMul(v1->dx, FRACUNIT / 256);
   temp = FixedMul(temp, v2->dy);

   den = den - temp;
   if (den == 0)
      return 0;

   temp = FixedMul((v2->y - v1->y), FRACUNIT / 256);
   temp = FixedMul(temp, v1->dx);

   num = FixedMul((v1->x - v2->x), FRACUNIT / 256);
   num = FixedMul(num, v1->dy);

   num = num + temp;
   frac = FixedDiv(num, den);
#endif
#else
   fixed_t num;
   fixed_t den, temp;

   den = FixedMul(v1->dy >> 8, v2->dx);
   temp = FixedMul(v1->dx >> 8, v2->dy);

   den = den - temp;
   if (den == 0)
      return 0;

   temp = FixedMul((v2->y - v1->y) >> 8, v1->dx);
   num = FixedMul((v1->x - v2->x) >> 8, v1->dy);

   num = num + temp;
   frac = FixedDiv(num, den);
#endif

   return frac;
}

boolean PTR_SightTraverse(sightWork_t *sw, intercept_t * in)
{
   fixed_t opentop;
   fixed_t openbottom;
   fixed_t slope;
   sector_t *front;
   sector_t *back;
   fixed_t sightzstart = sw->sightzstart;
   line_t *line = in->d.line;

   if (line->sidenum[1] < 0)
      return false; // stop

	front = LD_FRONTSECTOR(line);
	back = LD_BACKSECTOR(line);

   // no wall to block sight with?
   if (front->floorheight == back->floorheight && front->ceilingheight == back->ceilingheight)
      return true;

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
   if (openbottom >= opentop)
      return false; // stop

   if (front->floorheight != back->floorheight)
   {
      slope = FixedDiv(openbottom - sightzstart, in->frac);
      if (slope > sw->bottomslope)
         sw->bottomslope = slope;
   }

   if (front->ceilingheight != back->ceilingheight)
   {
      slope = FixedDiv(opentop - sightzstart, in->frac);
      if (slope < sw->topslope)
         sw->topslope = slope;
   }

   if (sw->topslope <= sw->bottomslope)
      return false; // stop

   return true;                // keep going
}

boolean P_SightBlockLinesIterator(sightWork_t *sw, int x, int y)
{
   int offset;
   short *list;
   line_t *line;
   int s1, s2;
   divline_t divl;
   vertex_t v1, v2;
   VINT *lvalidcount, vc;
   divline_t *strace = &sw->strace;
   fixed_t t2x = sw->t2x, t2y = sw->t2y;

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   vc = *lvalidcount;
   ++lvalidcount;

   offset = y * bmapwidth + x;

   offset = *(blockmaplump + 4 + offset);

   for (list = blockmaplump + offset; *list != -1; list++)
   {
      line = &lines[*list];
      if (lvalidcount[*list] == vc)
         continue;
      lvalidcount[*list] = vc;

      v1.x = vertexes[line->v1].x << FRACBITS;
      v1.y = vertexes[line->v1].y << FRACBITS;
      v2.x = vertexes[line->v2].x << FRACBITS;
      v2.y = vertexes[line->v2].y << FRACBITS;

      s1 = P_DivlineSide(v1.x, v1.y, strace);
      s2 = P_DivlineSide(v2.x, v2.y, strace);

      // line isn't crossed?
      if (s1 == s2)
         continue;

      divl.x = v1.x;
      divl.y = v1.y;
      divl.dx = v2.x - v1.x;
      divl.dy = v2.y - v1.y;
      s1 = P_DivlineSide(strace->x, strace->y, &divl);
      s2 = P_DivlineSide(t2x, t2y, &divl);

      // line isn't crossed?
      if (s1 == s2)
         continue;

      // try to early out the check
      if (!(line->flags & ML_TWOSIDED))
         return false;

      // store the line for later intersection testing
      if (sw->numintercepts < MAXINTERCEPTS)
      {
         sw->intercepts[sw->numintercepts].d.line = line;
         sw->intercepts[sw->numintercepts].frac = P_InterceptVector2(strace, &divl);
         sw->numintercepts++;
      }
   }

   return true; // everything was checked
}

//
// Returns false if a straight line between t1 and t2 is obstructed
//
static boolean PS_RejectCheckSight(mobj_t *t1, mobj_t *t2)
{
   unsigned s1, s2;
   unsigned pnum, bytenum, bitnum;

   // First check for trivial rejection
   s1 = ((uintptr_t)t1->subsector->sector - (uintptr_t)sectors);
   s2 = ((uintptr_t)t2->subsector->sector - (uintptr_t)sectors);
   pnum = (s1 * numsectors + s2) / sizeof(sector_t);
   bytenum = pnum >> 3;

   bitnum = 1;
   switch (pnum & 7)
   {
   case 7:
      do
      {
         bitnum <<= 1;
      case 6:
         bitnum <<= 1;
      case 5:
         bitnum <<= 1;
      case 4:
         bitnum <<= 1;
      case 3:
         bitnum <<= 1;
      case 2:
         bitnum <<= 1;
      case 1:
         bitnum <<= 1;
      case 0:
         break;
      } while (0);
   }

   if (rejectmatrix[bytenum] & bitnum)
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
   sw.topslope = (t2->z + t2->height) - sw.sightzstart;
   sw.bottomslope = (t2->z) - sw.sightzstart;

   // make sure it never lies exactly on a vertex coordinate
   sw.strace.x = (t1->x & ~0x1ffff) | 0x10000;
   sw.strace.y = (t1->y & ~0x1ffff) | 0x10000;
   sw.t2x = (t2->x & ~0x1ffff) | 0x10000;
   sw.t2y = (t2->y & ~0x1ffff) | 0x10000;
   sw.strace.dx = sw.t2x - sw.strace.x;
   sw.strace.dy = sw.t2y - sw.strace.y;

   return P_SightPathTraverse(&sw);
}

static boolean P_MobjCanSightCheck(mobj_t *mobj)
{
   // must be killable
   if (!(mobj->flags & MF_COUNTKILL))
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
   do
   {
      __asm volatile(
          "tas.b %1\n\t"
          "movt %0\n\t"
          : "=&r"(res)
          : "m"(ps_lock));
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

   for (next = MARS_SYS_COMM6;; next++)
   {
      if (c == 1)
      {
         for (; cnt < next; cnt++)
         {
            if (mobj == (void *)&mobjhead)
               goto done;
            Mars_ClearCacheLine(&mobj->next);
            mobj = mobj->next;
         }
         Mars_ClearCacheLines(mobj, (sizeof(mobj_t) + 31) / 16);
      }
      else
      {
         for (; cnt < next; cnt++)
         {
            if (mobj == (void *)&mobjhead)
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
   for (; mobj != (void *)&mobjhead; mobj = mobj->next)
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

   for (mobj = mobjhead.next;; mobj = P_NextSightMobj(mobj))
   {
      if ((mobj = P_GetSightMobj(mobj, c, &cnt)) == (void *)&mobjhead)
         return;

#ifdef MARS
      if (c == 1 && ctrgt != mobj->target)
      {
         Mars_ClearCacheLines(mobj->target, (sizeof(mobj_t) + 31) / 16);
         ctrgt = mobj->target;
      }
#endif

      if (PS_CheckSight2(mobj, mobj->target))
         mobj->flags |= MF_SEETARGET;
   }
}

// EOF
