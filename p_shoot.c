/*
  CALICO

  Tracer attack code
  
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

// A line will be shootdiv'd from the middle of shooter in the direction of
// attackangle until either a shootable mobj is within the visible
// aimtopslope / aimbottomslope range, or a solid wall blocks further
// tracing.  If no thing is targeted along the entire range, the first line
// that blocks the midpoint of the shootdiv will be hit.

// CALICO: removed type punning by bringing back intercept_t
typedef struct intercept_s
{
    union ptr_u
    {
        mobj_t* mo;
        line_t* line;
    } d;
    fixed_t frac;
    boolean isaline;
} intercept_t;

typedef struct
{
   mobj_t    *shooter;
   fixed_t   attackrange;
   fixed_t   aimmidslope;              // for detecting first wall hit
   fixed_t   aimbottomslope;
   fixed_t   aimtopslope;
   divline_t shootdiv;
   fixed_t   shootx2, shooty2;
   fixed_t   firstlinefrac;
   int       shootdivpositive;
   int16_t   ssx1, ssy1, ssx2, ssy2;
   intercept_t old_intercept;

   line_t  *shootline;
   mobj_t  *shootmobj;
   fixed_t  shootslope;             // between aimtop and aimbottom
   fixed_t  shootx, shooty, shootz; // location for puff/blood
} shootWork_t;

static fixed_t PA_SightCrossLine(shootWork_t *sw, mapvertex_t *v1, mapvertex_t *v2) ATTR_DATA_CACHE_ALIGN;
static boolean PA_ShootLine(shootWork_t *sw, line_t* li, fixed_t interceptfrac) ATTR_DATA_CACHE_ALIGN;
static boolean PA_ShootThing(shootWork_t *sw, mobj_t* th, fixed_t interceptfrac) ATTR_DATA_CACHE_ALIGN;
static boolean PA_DoIntercept(shootWork_t *sw, intercept_t* in) ATTR_DATA_CACHE_ALIGN;
static boolean PA_CrossSubsector(shootWork_t *sw, int bspnum) ATTR_DATA_CACHE_ALIGN;
static boolean PA_CrossBSPNode(shootWork_t *sw, int bspnum) ATTR_DATA_CACHE_ALIGN;
void P_Shoot2(lineattack_t *la) ATTR_DATA_CACHE_ALIGN;

//
// Returns side 0 (front or on), 1 (back)
//
//#define PA_NodeSide(xx,yy,n) (!((((xx) - (n)->x * FRACUNIT)>>FRACBITS) * ((n)->dy) > (((yy) - (n)->y * FRACUNIT)>>FRACBITS) * ((n)->dx)))
ATTR_DATA_CACHE_ALIGN
static inline int PA_NodeSide(int x, int y, node_t *node)
{
	int32_t	dx,dy;
	int32_t r1, r2;

	dx = x - ((fixed_t)node->x << FRACBITS);
#ifdef MARS
	dx = (unsigned)dx >> FRACBITS;
	__asm volatile(
		"muls.w %0,%1\n\t"
		: : "r"(node->dy), "r"(dx) : "macl", "mach");
#else
	dx >>= FRACBITS;
	r1 = node->dy * dx;
#endif

	dy = y - ((fixed_t)node->y << FRACBITS);
#ifdef MARS
	dy = (unsigned)dy >> FRACBITS;
	__asm volatile(
		"sts macl, %0\n\t"
		"muls.w %2,%3\n\t"
		"sts macl, %1\n\t"
		: "=&r"(r1), "=&r"(r2) : "r"(dy), "r"(node->dx) : "macl", "mach");
#else
	dy >>= FRACBITS;
	r2 = dy * node->dx;
#endif

    return (r1 <= r2);
}

//
// First checks the endpoints of the line to make sure that they cross the
// sight trace treated as an infinite line.
//
// If so, it calculates the fractional distance along the sight trace that
// the intersection occurs at.  If 0 < intercept < 1.0, the line will block
// the sight.
//
static fixed_t PA_SightCrossLine(shootWork_t *sw, mapvertex_t *v1, mapvertex_t *v2)
{
   fixed_t s1, s2;
   fixed_t p1x, p1y, p2x, p2y, p3x, p3y, p4x, p4y, dx, dy, ndx, ndy;

   // p1, p2 are endpoints
   p1x = v1->x;
   p1y = v1->y;
   p2x = v2->x;
   p2y = v2->y;

   // p3, p4 are sight endpoints
   p3x = sw->ssx1;
   p3y = sw->ssy1;
   p4x = sw->ssx2;
   p4y = sw->ssy2;

   dx  = p2x - p3x;
   dy  = p2y - p3y;
   ndx = p4x - p3x;
   ndy = p4y - p3y;

   s1 = (ndy * dx) < (dy * ndx);

   dx = p1x - p3x;
   dy = p1y - p3y;

   s2 = (ndy * dx) < (dy * ndx);

   if(s1 == s2)
      return -1; // line isn't crossed

   ndx = p1y - p2y; // vector normal to world line
   ndy = p2x - p1x;

   s1 = ndx * dx + ndy * dy; // distance projected onto normal

   dx = p4x - p1x;
   dy = p4y - p1y;

   s2 = ndx * dx + ndy * dy; // distance projected onto normal

   /* try to quickly decide by looking at sign bits */
   if ( (s1 ^ (s1 + s2))&0x80000000 )
      return -1;

   return FixedDiv(s1, (s1 + s2));
}

//
// Handle shooting a line.
//
static boolean PA_ShootLine(shootWork_t *sw, line_t *li, fixed_t interceptfrac)
{
   fixed_t   slope;
   fixed_t   dist;
   sector_t *front, *back;
   fixed_t   opentop, openbottom;

   if(li->sidenum[1] == -1)
   {
      if(!sw->shootline)
      {
         sw->shootline = li;
         sw->firstlinefrac = interceptfrac;
      }
      sw->old_intercept.frac = 0; // don't shoot anything past this
      return false;
   }

   // crosses a two-sided line
   front = LD_FRONTSECTOR(li);
   back  = LD_BACKSECTOR(li);

   if(front->ceilingheight < back->ceilingheight)
      opentop = front->ceilingheight;
   else
      opentop = back->ceilingheight;

   if(front->floorheight > back->floorheight)
      openbottom = front->floorheight;
   else
      openbottom = back->floorheight;

   dist = FixedMul(sw->attackrange, interceptfrac);

   if(front->floorheight != back->floorheight)
   {
      slope = FixedDiv(openbottom - sw->shootz, dist);
      if(slope >= sw->aimmidslope && !sw->shootline)
      {
         sw->shootline = li;
         sw->firstlinefrac = interceptfrac;
      }
      if(slope > sw->aimbottomslope)
         sw->aimbottomslope = slope;
   }

   if(front->ceilingheight != back->ceilingheight)
   {
      slope = FixedDiv(opentop - sw->shootz, dist);
      if(slope <= sw->aimmidslope && !sw->shootline)
      {
         sw->shootline = li;
         sw->firstlinefrac = interceptfrac;
      }
      if(slope < sw->aimtopslope)
         sw->aimtopslope = slope;
   }

   if(sw->aimtopslope <= sw->aimbottomslope)
      return false;

   return true;
}

//
// Handle shooting a thing.
//
static boolean PA_ShootThing(shootWork_t *sw, mobj_t *th, fixed_t interceptfrac)
{
   fixed_t frac, dist, tmp;
   fixed_t thingaimtopslope, thingaimbottomslope;

   if(th == sw->shooter)
      return true; // can't shoot self

   if(!(th->flags & MF_SHOOTABLE))
      return true; // corpse or something

   // check angles to see if the thing can be aimed at
   dist = FixedMul(sw->attackrange, interceptfrac);
   
   thingaimtopslope = FixedDiv(th->z + th->height - sw->shootz, dist);
   if(thingaimtopslope < sw->aimbottomslope)
      return true; // shot over the thing

   thingaimbottomslope = FixedDiv(th->z - sw->shootz, dist);
   if(thingaimbottomslope > sw->aimtopslope)
      return true; // shot under the thing

   // this thing can be hit!
   if(thingaimtopslope > sw->aimtopslope)
      thingaimtopslope = sw->aimtopslope;
   if(thingaimbottomslope < sw->aimbottomslope)
      thingaimbottomslope = sw->aimbottomslope;

   // shoot midway in the visible part of the thing
   sw->shootslope = (thingaimtopslope + thingaimbottomslope) / 2;
   sw->shootmobj  = th;

   // position a bit closer
   frac   = interceptfrac - FixedDiv(10*FRACUNIT, sw->attackrange);
   sw->shootx = FixedMul(sw->shootdiv.dx, frac);
   sw->shooty = FixedMul(sw->shootdiv.dy, frac);
   tmp = FixedMul(frac, sw->attackrange);
   tmp = FixedMul(sw->shootslope, tmp);

   sw->shootx = sw->shootdiv.x + sw->shootx;
   sw->shooty = sw->shootdiv.y + sw->shooty;
   sw->shootz = sw->shootz + tmp;

   return false; // don't go any further
}

//
// Process an intercept
//
#define COPY_INTERCEPT(dst,src) do { (dst)->d.line = (src)->d.line, (dst)->frac = (src)->frac, (dst)->isaline = (src)->isaline; } while(0)
static boolean PA_DoIntercept(shootWork_t *sw, intercept_t *in)
{
   intercept_t temp;

   if(sw->old_intercept.frac < in->frac)
   {
      COPY_INTERCEPT(&temp, &sw->old_intercept);
      COPY_INTERCEPT(&sw->old_intercept, in);
      COPY_INTERCEPT(in, &temp);
   }

   if(in->frac == 0 || in->frac >= FRACUNIT)
      return true;

   if(in->isaline)
      return PA_ShootLine(sw, in->d.line, in->frac);
   return PA_ShootThing(sw, in->d.mo, in->frac);
}

//
// Returns true if strace crosses the given subsector successfuly
//
static boolean PA_CrossSubsector(shootWork_t *sw, int bspnum)
{
   seg_t   *seg;
   line_t  *line;
   int      count;
   fixed_t  frac;
   mobj_t  *thing;
   subsector_t *sub = &subsectors[bspnum];
   intercept_t  in;
   mapvertex_t tv1, tv2;
   VINT     *lvalidcount, vc;

   // check things
   for(thing = SPTR_TO_LPTR(sub->sector->thinglist); thing; thing = SPTR_TO_LPTR(thing->snext))
   {
      if(thing->subsector != sub)
         continue;
      if(!(thing->flags & MF_SHOOTABLE))
         continue; // corpse or something

      // check a corner to corner cross-section for hit
      if(sw->shootdivpositive)
      {
         tv1.x = (thing->x - thing->radius) >> FRACBITS;
         tv1.y = (thing->y + thing->radius) >> FRACBITS;
         tv2.x = (thing->x + thing->radius) >> FRACBITS;
         tv2.y = (thing->y - thing->radius) >> FRACBITS;
      }
      else
      {
         tv1.x = (thing->x - thing->radius) >> FRACBITS;
         tv1.y = (thing->y - thing->radius) >> FRACBITS;
         tv2.x = (thing->x + thing->radius) >> FRACBITS;
         tv2.y = (thing->y + thing->radius) >> FRACBITS;
      }

      frac = PA_SightCrossLine(sw, &tv1, &tv2);
      if(frac < 0 || frac > FRACUNIT)
         continue;

      in.d.mo    = thing;
      in.isaline = false;
      in.frac    = frac;

      if(!PA_DoIntercept(sw, &in))
         return false;
   }

   // check lines
   count = sub->numlines;
   seg   = &segs[sub->firstline];

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   vc = *lvalidcount + 1;
   if (vc == 0)
      vc = 1;

   *lvalidcount = vc;
   ++lvalidcount;

   for(; count; seg++, count--)
   {
      int ld = SEG_UNPACK_LINEDEF(seg);
      line = &lines[ld];

      if(lvalidcount[ld] == vc)
         continue; // already checked other side
      lvalidcount[ld] = vc;

      frac = PA_SightCrossLine(sw, &vertexes[line->v1], &vertexes[line->v2]);
      if(frac < 0 || frac > FRACUNIT)
         continue;

      in.d.line  = line;
      in.isaline = true;
      in.frac    = frac;

      if(!PA_DoIntercept(sw, &in))
         return false;
   }

   return true; // passed the subsector ok
}

//
// Walk the BSP tree to follow the trace.
//
static boolean PA_CrossBSPNode(shootWork_t *sw, int bspnum)
{
   node_t *bsp;
   int side, side2;

check:
   if(bspnum & NF_SUBSECTOR)
   {
      // CALICO: bspnum == -1 case not originally handled here
      return PA_CrossSubsector(sw, bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
   }

   bsp = &nodes[bspnum];

   // decide which side the start point is on
   side = PA_NodeSide(sw->shootdiv.x, sw->shootdiv.y, bsp);
   side2 = PA_NodeSide(sw->shootx2, sw->shooty2, bsp);

   // cross the starting side
   if(!PA_CrossBSPNode(sw, bsp->children[side]))
      return false;

   // the partition plane is crossed here
   if(side == side2)
      return true; // the line doesn't touch the other side
   
   // cross the ending side
   bspnum = bsp->children[side ^ 1];
   goto check;
}

//
// Main function to trace a line attack.
//
void P_Shoot2(lineattack_t *la)
{
   mobj_t  *t1;
   angle_t  angle;
   fixed_t  tmp;
   shootWork_t sw;

   t1        = la->shooter;
   sw.shooter = la->shooter;
   sw.shootline = NULL;
   sw.shootmobj = NULL;
   angle     = la->attackangle >> ANGLETOFINESHIFT;

   sw.attackrange = la->attackrange;
   sw.shootdiv.x  = t1->x;
   sw.shootdiv.y  = t1->y;
   sw.shootx2     = t1->x + (la->attackrange >> FRACBITS) * finecosine(angle);
   sw.shooty2     = t1->y + (la->attackrange >> FRACBITS) * finesine(angle);
   sw.shootdiv.dx = sw.shootx2 - sw.shootdiv.x;
   sw.shootdiv.dy = sw.shooty2 - sw.shootdiv.y;
   sw.shootz      = t1->z + (t1->height >> 1) + 8*FRACUNIT;

   sw.shootdivpositive = (sw.shootdiv.dx ^ sw.shootdiv.dy) > 0;

   sw.ssx1 = sw.shootdiv.x >> FRACBITS;
   sw.ssy1 = sw.shootdiv.y >> FRACBITS;
   sw.ssx2 = sw.shootx2    >> FRACBITS;
   sw.ssy2 = sw.shooty2    >> FRACBITS;

   sw.aimtopslope    = la->aimtopslope;
   sw.aimbottomslope = la->aimbottomslope;
   sw.aimmidslope = (la->aimtopslope + la->aimbottomslope) / 2;

   // cross everything
   sw.old_intercept.d.line  = NULL;
   sw.old_intercept.frac    = 0;
   sw.old_intercept.isaline = false;

   PA_CrossBSPNode(&sw, numnodes - 1);

   // check the last intercept if needed
   if(!sw.shootmobj)
   {
      intercept_t in;
      in.d.mo    = NULL;
      in.isaline = false;
      in.frac    = FRACUNIT;
      PA_DoIntercept(&sw, &in);
   }

   la->shootline = sw.shootline;
   la->shootmobj = sw.shootmobj;
   la->shootslope = sw.shootslope;
   la->shootx = sw.shootx;
   la->shooty = sw.shooty;
   la->shootz = sw.shootz;

   // post-process
   if(la->shootmobj)
      return;

   if(!la->shootline)
      return;

   // calculate the intercept point for the first line hit

   // position a bit closer
   sw.firstlinefrac -= FixedDiv(4*FRACUNIT, la->attackrange);
   la->shootx = FixedMul(sw.shootdiv.dx, sw.firstlinefrac);
   la->shooty = FixedMul(sw.shootdiv.dy, sw.firstlinefrac);
   tmp = FixedMul(sw.firstlinefrac, la->attackrange);
   tmp = FixedMul(tmp, sw.aimmidslope);

   la->shootx  = sw.shootdiv.x + la->shootx;
   la->shooty  = sw.shootdiv.y + la->shooty;
   la->shootz += tmp;
}

// EOF

