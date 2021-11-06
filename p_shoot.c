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

// CALICO_FIXME: should be in a header:
extern mobj_t  *shooter;
extern angle_t  attackangle;
extern fixed_t  attackrange;
extern fixed_t  aimtopslope;
extern fixed_t  aimbottomslope;

line_t  *shootline;
mobj_t  *shootmobj;
fixed_t  shootslope;             // between aimtop and aimbottom
fixed_t  shootx, shooty, shootz; // location for puff/blood

// A line will be shootdiv'd from the middle of shooter in the direction of
// attackangle until either a shootable mobj is within the visible
// aimtopslope / aimbottomslope range, or a solid wall blocks further
// tracing.  If no thing is targeted along the entire range, the first line
// that blocks the midpoint of the shootdiv will be hit.

static fixed_t   aimmidslope;              // for detecting first wall hit
static divline_t shootdiv;
static fixed_t   shootx2, shooty2;
static fixed_t   firstlinefrac;
static int       shootdivpositive;
static int       ssx1, ssy1, ssx2, ssy2;

static line_t thingline;
static vertex_t tv1, tv2;

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

static intercept_t old_intercept;

static fixed_t PA_SightCrossLine(line_t* line) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PA_ShootLine(line_t* li, fixed_t interceptfrac) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PA_ShootThing(mobj_t* th, fixed_t interceptfrac) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PA_DoIntercept(intercept_t* in) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PA_CrossSubsector(int bspnum) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE ATTR_OPTIMIZE_SIZE;
static int PA_DivlineSide(fixed_t x, fixed_t y, divline_t* line) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static boolean PA_CrossBSPNode(int bspnum) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void P_Shoot2(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

//
// First checks the endpoints of the line to make sure that they cross the
// sight trace treated as an infinite line.
//
// If so, it calculates the fractional distance along the sight trace that
// the intersection occurs at.  If 0 < intercept < 1.0, the line will block
// the sight.
//
static fixed_t PA_SightCrossLine(line_t *line)
{
   fixed_t s1, s2;
   fixed_t p1x, p1y, p2x, p2y, p3x, p3y, p4x, p4y, dx, dy, ndx, ndy;

   // p1, p2 are endpoints
   p1x = line->v1->x >> FRACBITS;
   p1y = line->v1->y >> FRACBITS;
   p2x = line->v2->x >> FRACBITS;
   p2y = line->v2->y >> FRACBITS;

   // p3, p4 are sight endpoints
   p3x = ssx1;
   p3y = ssy1;
   p4x = ssx2;
   p4y = ssy2;

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

   return FixedDiv(s1, (s1 + s2));
}

//
// Handle shooting a line.
//
static boolean PA_ShootLine(line_t *li, fixed_t interceptfrac)
{
   fixed_t   slope;
   fixed_t   dist;
   sector_t *front, *back;
   fixed_t   opentop, openbottom;

   if(!(li->flags & ML_TWOSIDED))
   {
      if(!shootline)
      {
         shootline = li;
         firstlinefrac = interceptfrac;
      }
      old_intercept.frac = 0; // don't shoot anything past this
      return false;
   }

   // crosses a two-sided line
   front = &sectors[LD_FRONTSECTORNUM(li)];
   back  = &sectors[LD_BACKSECTORNUM(li)];

   if(front->ceilingheight < back->ceilingheight)
      opentop = front->ceilingheight;
   else
      opentop = back->ceilingheight;

   if(front->floorheight > back->floorheight)
      openbottom = front->floorheight;
   else
      openbottom = back->floorheight;

   FixedMul2(dist, attackrange, interceptfrac);

   if(front->floorheight != back->floorheight)
   {
      slope = FixedDiv(openbottom - shootz, dist);
      if(slope >= aimmidslope && !shootline)
      {
         shootline = li;
         firstlinefrac = interceptfrac;
      }
      if(slope > aimbottomslope)
         aimbottomslope = slope;
   }

   if(front->ceilingheight != back->ceilingheight)
   {
      slope = FixedDiv(opentop - shootz, dist);
      if(slope <= aimmidslope && !shootline)
      {
         shootline = li;
         firstlinefrac = interceptfrac;
      }
      if(slope < aimtopslope)
         aimtopslope = slope;
   }

   if(aimtopslope <= aimbottomslope)
      return false;

   return true;
}

//
// Handle shooting a thing.
//
static boolean PA_ShootThing(mobj_t *th, fixed_t interceptfrac)
{
   fixed_t frac, dist, tmp;
   fixed_t thingaimtopslope, thingaimbottomslope;

   if(th == shooter)
      return true; // can't shoot self

   if(!(th->flags & MF_SHOOTABLE))
      return true; // corpse or something

   // check angles to see if the thing can be aimed at
   FixedMul2(dist, attackrange, interceptfrac);
   
   thingaimtopslope = FixedDiv(th->z + th->height - shootz, dist);
   if(thingaimtopslope < aimbottomslope)
      return true; // shot over the thing

   thingaimbottomslope = FixedDiv(th->z - shootz, dist);
   if(thingaimbottomslope > aimtopslope)
      return true; // shot under the thing

   // this thing can be hit!
   if(thingaimtopslope > aimtopslope)
      thingaimtopslope = aimtopslope;
   if(thingaimbottomslope < aimbottomslope)
      thingaimbottomslope = aimbottomslope;

   // shoot midway in the visible part of the thing
   shootslope = (thingaimtopslope + thingaimbottomslope) / 2;
   shootmobj  = th;

   // position a bit closer
   frac   = interceptfrac - FixedDiv(10*FRACUNIT, attackrange);
   FixedMul2(shootx, shootdiv.dx, frac);
   FixedMul2(shooty, shootdiv.dy, frac);
   FixedMul2(tmp, frac, attackrange);
   FixedMul2(tmp, shootslope, tmp);

   shootx = shootdiv.x + shootx;
   shooty = shootdiv.y + shooty;
   shootz = shootz + tmp;

   return false; // don't go any further
}

//
// Process an intercept
//
#define COPY_INTERCEPT(dst,src) do { (dst)->d.line = (src)->d.line, (dst)->frac = (src)->frac, (dst)->isaline = (src)->isaline; } while(0)
static boolean PA_DoIntercept(intercept_t *in)
{
   intercept_t temp;

   if(old_intercept.frac < in->frac)
   {
      COPY_INTERCEPT(&temp, &old_intercept);
      COPY_INTERCEPT(&old_intercept, in);
      COPY_INTERCEPT(in, &temp);
   }

   if(in->frac == 0 || in->frac >= FRACUNIT)
      return true;

   if(in->isaline)
      return PA_ShootLine(in->d.line, in->frac);
   else
      return PA_ShootThing(in->d.mo, in->frac);
}

//
// Returns true if strace crosses the given subsector successfuly
//
static boolean PA_CrossSubsector(int bspnum)
{
   seg_t   *seg;
   line_t  *line;
   int      count;
   fixed_t  frac;
   mobj_t  *thing;

   subsector_t *sub = &subsectors[bspnum];
   intercept_t  in;

   // CALICO: removed type punning
   thingline.v1 = &tv1;
   thingline.v2 = &tv2;

   // check things
   for(thing = sub->sector->thinglist; thing; thing = thing->snext)
   {
      if(thing->subsector != sub)
         continue;

      // check a corner to corner cross-section for hit
      if(shootdivpositive)
      {
         thingline.v1->x = thing->x - thing->radius;
         thingline.v1->y = thing->y + thing->radius;
         thingline.v2->x = thing->x + thing->radius;
         thingline.v2->y = thing->y - thing->radius;
      }
      else
      {
         thingline.v1->x = thing->x - thing->radius;
         thingline.v1->y = thing->y - thing->radius;
         thingline.v2->x = thing->x + thing->radius;
         thingline.v2->y = thing->y + thing->radius;
      }

      frac = PA_SightCrossLine(&thingline);

      if(frac < 0 || frac > FRACUNIT)
         continue;

      in.d.mo    = thing;
      in.isaline = false;
      in.frac    = frac;

      if(!PA_DoIntercept(&in))
         return false;
   }

   // check lines
   count = sub->numlines;
   seg   = &segs[sub->firstline];

   for(; count; seg++, count--)
   {
      line = &lines[seg->linedef];

      if(line->validcount == validcount)
         continue; // already checked other side
      line->validcount = validcount;

      frac = PA_SightCrossLine(line);

      if(frac < 0 || frac > FRACUNIT)
         continue;

      in.d.line  = line;
      in.isaline = true;
      in.frac    = frac;

      if(!PA_DoIntercept(&in))
         return false;
   }

   return true; // passed the subsector ok
}

/*
=====================
=
= PA_DivlineSide
=
=====================
*/
static int PA_DivlineSide(fixed_t x, fixed_t y, divline_t *line)
{
	fixed_t dx, dy;

	x = (x - line->x) >> FRACBITS;
	y = (y - line->y) >> FRACBITS;

	dx = x * (line->dy >> FRACBITS);
	dy = y * (line->dx >> FRACBITS);

	return (dy < dx) ^ 1;
}

//
// Walk the BSP tree to follow the trace.
//
static boolean PA_CrossBSPNode(int bspnum)
{
   node_t *bsp;
   int side;
   divline_t div;

   if(bspnum & NF_SUBSECTOR)
   {
      if(bspnum == -1) // CALICO: case not originally handled here
         return PA_CrossSubsector(0);
      else
         return PA_CrossSubsector(bspnum & ~NF_SUBSECTOR);
   }

   bsp = &nodes[bspnum];

   // decide which side the start point is on
   div.x  = bsp->x;  // CALICO: no type punning here
   div.y  = bsp->y;
   div.dx = bsp->dx;
   div.dy = bsp->dy;
   side = PA_DivlineSide(shootdiv.x, shootdiv.y, &div);

   // cross the starting side
   if(!PA_CrossBSPNode(bsp->children[side]))
      return false;

   // the partition plane is crossed here
   if(side == PA_DivlineSide(shootx2, shooty2, &div))
      return true; // the line doesn't touch the other side
   
   // cross the ending side
   return PA_CrossBSPNode(bsp->children[side^1]);
}

//
// Main function to trace a line attack.
//
void P_Shoot2(void)
{
   mobj_t  *t1;
   angle_t  angle;
   fixed_t  tmp;

   t1        = shooter;
   shootline = NULL;
   shootmobj = NULL;
   angle     = attackangle >> ANGLETOFINESHIFT;

   shootdiv.x  = t1->x;
   shootdiv.y  = t1->y;
   shootx2     = t1->x + (attackrange >> FRACBITS) * finecosine(angle);
   shooty2     = t1->y + (attackrange >> FRACBITS) * finesine(angle);
   shootdiv.dx = shootx2 - shootdiv.x;
   shootdiv.dy = shooty2 - shootdiv.y;
   shootz      = t1->z + (t1->height >> 1) + 8*FRACUNIT;

   shootdivpositive = (shootdiv.dx ^ shootdiv.dy) > 0;

   ssx1 = shootdiv.x >> FRACBITS;
   ssy1 = shootdiv.y >> FRACBITS;
   ssx2 = shootx2    >> FRACBITS;
   ssy2 = shooty2    >> FRACBITS;

   aimmidslope = (aimtopslope + aimbottomslope) / 2;

   // cross everything
   old_intercept.d.line  = NULL;
   old_intercept.frac    = 0;
   old_intercept.isaline = false;

   PA_CrossBSPNode(numnodes - 1);

   // check the last intercept if needed
   if(!shootmobj)
   {
      intercept_t in;
      in.d.mo    = NULL;
      in.isaline = false;
      in.frac    = FRACUNIT;
      PA_DoIntercept(&in);
   }

   // post-process
   if(shootmobj)
      return;

   if(!shootline)
      return;

   // calculate the intercept point for the first line hit

   // position a bit closer
   firstlinefrac -= FixedDiv(4*FRACUNIT, attackrange);
   FixedMul2(shootx, shootdiv.dx, firstlinefrac);
   FixedMul2(shooty, shootdiv.dy, firstlinefrac);
   FixedMul2(tmp, firstlinefrac, attackrange);
   FixedMul2(tmp, aimmidslope, tmp);

   shootx  = shootdiv.x + shootx;
   shooty  = shootdiv.y + shooty;
   shootz += tmp;
}

// EOF

