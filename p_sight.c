/*
  CALICO
  
  Line-of-sight checking

  The MIT License (MIT)

  Copyright (c) 2026 Victor Luchits
  
  Copyright (c) 2015 James Haley, Olde Skuul, id Software and ZeniMax Media

  Copyright (c) 2003 Christoph Oelckers, Randy Heit
  
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
    fixed_t sidek;        // -(strace.dy*strace.x - strace.dx*strace.y + 1)
    int16_t sidec[4];     // strace.dy, -strace.dx, twice: one pair per vertex
} sightWork_t;

static boolean PS_CrossLine(sightWork_t* sw, line_t *line, mapvertex_t *v1, mapvertex_t *v2, fixed_t iden) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
static boolean PS_CrossBlock(sightWork_t* sw, int bnum) ATTR_DATA_CACHE_ALIGN;
static boolean PS_CrossBlockmap(sightWork_t* sw) ATTR_DATA_CACHE_ALIGN;
static boolean PS_RejectCheckSight(mobj_t* t1, mobj_t* t2) ATTR_DATA_CACHE_ALIGN;
static boolean P_MobjCanSightCheck(mobj_t *mobj) ATTR_DATA_CACHE_ALIGN;
static mobj_t *P_GetSightMobj(int c) ATTR_DATA_CACHE_ALIGN;
static boolean PS_CheckSight2(mobj_t* t1, mobj_t* t2) ATTR_DATA_CACHE_ALIGN;
#ifdef MARS
static void P_CheckSights2(int c) ATTR_DATA_CACHE_ALIGN;
#else
static void P_CheckSights2(void) ATTR_DATA_CACHE_ALIGN;
#endif
boolean P_CheckSight(mobj_t *t1, mobj_t *t2) ATTR_DATA_CACHE_ALIGN;

// Dot product of (xx,yy) - (x,y) with the divline normal (-dy,dx),
// >= 0 on the back side
#define PS_DivlineDot(xx,yy,n) ((((yy) - (n)->y) * (n)->dx) - ((n)->dy * ((xx) - (n)->x)))

/*
=================
=
= PS_CrossLine
=
= The line's vertices are on different sides of strace, check whether
= strace crosses the line and if so, whether it blocks the view.
=
= Returns true if the view isn't blocked.
=================
*/

static boolean PS_CrossLine(sightWork_t *sw, line_t *line, mapvertex_t *v1, mapvertex_t *v2, fixed_t iden)
{
   sector_t     *front;
   sector_t     *back;
   fixed_t      opentop;
   fixed_t      openbottom;
   i16divline_t divl;
   fixed_t      slope;
   fixed_t      inum, d2;
   i16divline_t *strace = &sw->strace;
   fixed_t      sightzstart = sw->sightzstart;

   divl.x = v1->x;
   divl.y = v1->y;
   divl.dx = v2->x - v1->x;
   divl.dy = v2->y - v1->y;

   // dot products for the trace start and end, >= 0 on the back side;
   // d2 - inum = strace.dy*divl.dx - strace.dx*divl.dy, which is exactly
   // a2 - a1 from PS_CrossBlock, passed in as iden = a1 - a2
   inum = PS_DivlineDot(strace->x, strace->y, &divl);
   d2 = inum - iden;

   // line isn't crossed?
   if ((inum >= 0) == (d2 >= 0))
      return true;

   // stop because it is not two sided anyway
   if (line->sidenum[1] < 0)
      return false;

   front = LD_FRONTSECTOR(line);
   back = LD_SIDESECTOR(line, 1);

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

   // intercept fraction along strace is inum / iden

   if (front->floorheight != back->floorheight)
   {
      slope = P_SlopeDiv(openbottom - sightzstart, iden, inum);
      if (slope > sw->bottomslope)
         sw->bottomslope = slope;
   }

   if (front->ceilingheight != back->ceilingheight)
   {
      slope = P_SlopeDiv(opentop - sightzstart, iden, inum);
      if (slope < sw->topslope)
         sw->topslope = slope;
   }

   if (sw->topslope <= sw->bottomslope)
      return false; // stop

   return true;
}

/*
=================
=
= PS_CrossBlock
=
= Checks all lines in a blockmap cell against strace. Lines are processed
= as they are found rather than sorted by distance: the slope window only
= ever narrows (max of bottom slopes, min of top slopes), so the result
= doesn't depend on the order. Lines listed in several cells may be checked
= more than once, which is harmless for the same reason.
=
= A vertex lying exactly on strace (dot product 0) counts as being on the
= back side, so a line touching the trace with a vertex is checked as crossed
= when its other vertex is on the front side.
=
= Returns true if strace crosses the cell successfully.
=================
*/

static boolean PS_CrossBlock(sightWork_t *sw, int bnum)
{
   int16_t      *list;
   line_t       *line;
   mapvertex_t  *v1, *v2;
   fixed_t      a1, a2;
   int16_t      ld;
   const int16_t *sc = sw->sidec;
   const fixed_t k = sw->sidek;
   line_t       *llines = lines;
   mapvertex_t  *lvertexes = vertexes;

   list = blockmaplump + blockmaplump[4 + bnum];

   ld = *list;
   list++;

   while (1)
   {
      if (__builtin_expect(ld < 0, 0))
         break;

      line = &llines[ld];
      v1 = &lvertexes[line->v1];
      v2 = &lvertexes[line->v2];

      // side values of the line's vertices against strace:
      // a1, a2 = -PS_DivlineDot(v1/v2->x, v1/v2->y, strace) - 1, the sign bit
      // is set when the vertex is on the back side
      P_VertexSides(sc, k, &v1, &v2, &a1, &a2);

      // the next entry is loaded before the test, so it can go into the
      // branch delay slot
      ld = *list;
      list++;
#ifdef MARS
      __asm__ ("" : "+r" (list)); // keeps GCC from turning *list++ into list[-1]
#endif

      // line isn't crossed?
      if ((a1 ^ a2) >= 0)
         continue;

      if (!PS_CrossLine(sw, line, v1, v2, a1 - a2))
         return false;
   }

   // passed the cell ok
   return true;
}

/*
=================
=
= PS_CrossBlockmap
=
= Walks the blockmap cells along strace with an exact integer DDA: no
= divides, no rounding, so no cell is ever skipped. When the trace goes
= exactly through a cell corner, the two cells touching it are checked too.
=
= Returns true if strace crosses the blockmap successfully.
=================
*/

static boolean PS_CrossBlockmap(sightWork_t *sw)
{
   i16divline_t *strace = &sw->strace;
   fixed_t tx1, ty1, tx2, ty2;
   unsigned x1, y1, x2, y2;
   int nx, ny, adx, ady, err, bnum, bstepx, bstepy;

   tx1 = (fixed_t)strace->x << FRACBITS;
   ty1 = (fixed_t)strace->y << FRACBITS;
   tx2 = sw->t2x << FRACBITS;
   ty2 = sw->t2y << FRACBITS;

   // an end outside of the map: nothing to see. With both ends inside,
   // every cell the trace walks through is inside as well.
   if (tx1 < bmaporgx || ty1 < bmaporgy || tx2 < bmaporgx || ty2 < bmaporgy)
      return false;

   x1 = tx1 - bmaporgx;
   y1 = ty1 - bmaporgy;
   x2 = tx2 - bmaporgx;
   y2 = ty2 - bmaporgy;

   if ((int)(x1 >> MAPBLOCKSHIFT) >= bmapwidth
      || (int)(x2 >> MAPBLOCKSHIFT) >= bmapwidth
      || (int)(y1 >> MAPBLOCKSHIFT) >= bmapheight
      || (int)(y2 >> MAPBLOCKSHIFT) >= bmapheight)
      return false;

   nx = (x2 >> MAPBLOCKSHIFT) - (x1 >> MAPBLOCKSHIFT);
   ny = (y2 >> MAPBLOCKSHIFT) - (y1 >> MAPBLOCKSHIFT);
   bnum = (y1 >> MAPBLOCKSHIFT) * bmapwidth + (x1 >> MAPBLOCKSHIFT);

   // positions within the cells, in map units: the trace ends and the
   // blockmap origin are whole units, so nothing is lost
   x1 = (x1 & MAPBMASK) >> FRACBITS;
   y1 = (y1 & MAPBMASK) >> FRACBITS;

   // distances to the first vertical and horizontal cell boundaries ahead
   adx = strace->dx;
   if (adx >= 0)
   {
      bstepx = 1;
      x1 = MAPBLOCKUNITS - x1;
   }
   else
   {
      bstepx = -1;
      adx = -adx;
      nx = -nx;
   }

   ady = strace->dy;
   if (ady >= 0)
   {
      bstepy = bmapwidth;
      y1 = MAPBLOCKUNITS - y1;
   }
   else
   {
      bstepy = -bmapwidth;
      ady = -ady;
      ny = -ny;
   }

   // the boundaries are reached at x1/adx and y1/ady of the trace, err is
   // that difference scaled by adx*ady: < 0 means the vertical one comes first
   err = (int)x1 * ady - (int)y1 * adx;

   // from here on err moves by whole cells
   adx *= MAPBLOCKUNITS;
   ady *= MAPBLOCKUNITS;

   while (1)
   {
      if (!PS_CrossBlock(sw, bnum))
         return false;

      if (!(nx | ny))
         return true;

      if (ny == 0 || (nx != 0 && err < 0))
      {
         bnum += bstepx;
         err += ady;
         nx--;
      }
      else if (nx == 0 || err > 0)
      {
         bnum += bstepy;
         err -= adx;
         ny--;
      }
      else
      {
         // exactly through a corner, also check the two cells touching it
         if (!PS_CrossBlock(sw, bnum + bstepx))
            return false;
         if (!PS_CrossBlock(sw, bnum + bstepy))
            return false;
         bnum += bstepx + bstepy;
         err += ady - adx;
         nx--;
         ny--;
      }
   }
}

//
// Returns false if a straight line between t1 and t2 is obstructed
//
static boolean PS_RejectCheckSight(mobj_t *t1, mobj_t *t2)
{
   int s1, s2;
   unsigned pnum, bitnum;
   int bytenum;

   if (t1->subsector == t2->subsector)
   {
      // we don't care abgout compatibility with original demos
      return true;
   }

   // First check for trivial rejection
   s1 = t1->subsector->sector;
   s2 = t2->subsector->sector;

   if (s1 > s2)
   {
       int t = s2;
       s2 = s1;
       s1 = t;
   }
   pnum = s1*numsectors + s2;
   pnum -= (unsigned)(s1 * (s1 + 1)) >> 1; // s1 >= 0: plain shift, no signed-divide fixup
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

   // look from eyes of t1 to any part of t2
   sw.sightzstart = t1->z + (t1->height*FRACUNIT) - ((t1->height*FRACUNIT) >> 2);
   sw.topslope    = (t2->z + (t2->height*FRACUNIT)) - sw.sightzstart;
   sw.bottomslope = (t2->z) - sw.sightzstart;

   // make sure it never lies exactly on a vertex coordinate
   sw.strace.x = ((t1->x & ~0x1ffff) | 0x10000) >> FRACBITS;
   sw.strace.y = ((t1->y & ~0x1ffff) | 0x10000) >> FRACBITS;
   sw.t2x = ((t2->x & ~0x1ffff) | 0x10000) >> FRACBITS;
   sw.t2y = ((t2->y & ~0x1ffff) | 0x10000) >> FRACBITS;
   sw.strace.dx = sw.t2x - sw.strace.x;
   sw.strace.dy = sw.t2y - sw.strace.y;
   sw.sidek = -((fixed_t)sw.strace.dy * sw.strace.x - (fixed_t)sw.strace.dx * sw.strace.y + 1);
   sw.sidec[0] = sw.sidec[2] = sw.strace.dy;
   sw.sidec[1] = sw.sidec[3] = -sw.strace.dx;

   return PS_CrossBlockmap(&sw);
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

   return true;
}

#ifdef MARS

static atomic_flag ps_lock = ATOMIC_FLAG_INIT;
static volatile mobj_t * volatile next_sight;

#define P_LockSight() do { while (atomic_flag_test_and_set(&ps_lock)) { SH2_Delay64(); } } while (0)
#define P_UnlockSight() do { atomic_flag_clear(&ps_lock); } while (0)

static mobj_t *P_GetSightMobj(int c)
{
   volatile mobj_t *mobj;

   P_LockSight();

   Mars_ClearCacheLine(&next_sight);
   mobj = (mobj_t *)next_sight;
   if (mobj == NULL)
      goto done;

   while (1)
   {
      mobj = SPTR_TO_NNLPTR(mobj->next);
      if (mobj == (void*)&mobjhead)
      {
         mobj = NULL;
         break;
      }

      if (P_MobjCanSightCheck((mobj_t *)mobj))
         break;
   }

done:
   next_sight = mobj;
   P_UnlockSight();

   return (mobj_t *)mobj;
}

#define P_NextSightMobj(mobj) (mobj)

#else

// the last mobj handed out, or NULL once the list is exhausted
static mobj_t *next_sight;

static mobj_t *P_GetSightMobj(int c)
{
   mobj_t *mobj = next_sight;

   if (mobj == NULL)
      return NULL;

   while (1)
   {
      mobj = SPTR_TO_NNLPTR(mobj->next);
      if (mobj == (void*)&mobjhead)
      {
         mobj = NULL;
         break;
      }

      if (P_MobjCanSightCheck(mobj))
         break;
   }

   next_sight = mobj;
   return mobj;
}

#define P_NextSightMobj(mobj) (mobj)->next

#endif

//
// Optimal mobj sight checking that checks sights in the main tick loop rather
// than from multiple mobj action routines.
//
#ifdef MARS
static void P_CheckSights2(int c)
#else
static void P_CheckSights2(void)
#endif
{
   mobj_t *mobj;
#ifndef MARS
   int c = 0;
#endif

   while (1)
   {
      if ((mobj = P_GetSightMobj(c)) == NULL)
         return;

      if (!PS_RejectCheckSight(mobj, mobj->target))
         continue;

      if (PS_CheckSight2(mobj, mobj->target))
         mobj->flags |= MF_SEETARGET;
   }
}

/*============================================================================= */

/*
===============
=
= P_CheckSights
=
= Check sights of all mobj thinkers that are going to change state this
= tic and have MF_COUNTKILL set
===============
*/

#ifdef MARS
void Mars_Sec_P_CheckSights(void)
{
    // bank-switch to the page with map data
   I_SetBankPage(segspage);

   if (demorecording || demoplayback) {
      return;
   }
   if (netgame && !splitscreen) {
      return;
   }

   Mars_ClearCache();

   P_CheckSights2(1);
}
#endif

void P_CheckSights (void)
{
#ifdef MARS
	mobj_t *mo;
#endif

#ifdef JAGUAR
	extern	int p_sight_start;
	DSPFunction (&p_sight_start);
#elif defined(MARS)
   next_sight = (void*)&mobjhead;

	Mars_P_BeginCheckSights();

	P_CheckSights2(0);

	Mars_P_EndCheckSights();

	// clear cache for mobj flags right after the sight check as the
	// other CPU might have modified the MF_SEETARGET state; this must
	// happen before anything does a read-modify-write of mobj->flags
	// (MF_JUSTHIT, P_KillMobj, etc) from a stale cache line
	for (mo = SPTR_TO_NNLPTR(mobjhead.next); mo != (void*)&mobjhead; mo = SPTR_TO_NNLPTR(mo->next))
	{
		if (mo->tics == 1)
			Mars_ClearCacheLine(&mo->flags);
	}
#else
   next_sight = (void*)&mobjhead;

	P_CheckSights2();
#endif
}

boolean P_CheckSight(mobj_t *t1, mobj_t *t2)
{
   if (!PS_RejectCheckSight(t1, t2))
      return false;
   return PS_CheckSight2(t1, t2);      
}

// EOF

