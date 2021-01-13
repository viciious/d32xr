/*
  CALICO

  Code for sliding the player against walls

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

// CALICO_TODO: these should be in a header
extern mobj_t *slidething;

fixed_t slidex, slidey;            // the final position
line_t  *specialline;              // line special to trigger, if any

static fixed_t slidedx, slidedy;   // current move for completable frac
static fixed_t blockfrac;          // the fraction of the move that gets completed
static fixed_t blocknvx, blocknvy; // the vector of the line that blocks move
static fixed_t endbox[4];          // final proposed position
static fixed_t nvx, nvy;           // normalized line vector

static vertex_t *p1, *p2; // p1, p2 are line endpoints
static fixed_t p3x, p3y, p4x, p4y; // p3, p4 are move endpoints

#define CLIPRADIUS 23

enum
{
   SIDE_BACK = -1,
   SIDE_ON,
   SIDE_FRONT
};

//
// Simple point-on-line-side check.
//
static int SL_PointOnSide(fixed_t x, fixed_t y)
{
   fixed_t dx, dy, dist;

   dx = x - p1->x;
   dy = y - p1->y;

   dist = FixedMul(dx, nvx) + FixedMul(dy, nvy);

   if(dist > FRACUNIT)
      return SIDE_FRONT;
   else if(dist < -FRACUNIT)
      return SIDE_BACK;
   else
      return SIDE_ON;
}

//
// Return fractional intercept along the slide line.
//
static fixed_t SL_CrossFrac(void)
{
   fixed_t dx, dy, dist1, dist2;

   // project move start and end points onto line normal
   dx    = p3x - p1->x;
   dy    = p3y - p1->y;
   dist1 = FixedMul(dx, nvx) + FixedMul(dy, nvy);

   dx    = p4x - p1->x;
   dy    = p4y - p1->y;
   dist2 = FixedMul(dx, nvx) + FixedMul(dy, nvy);

   if((dist1 < 0) == (dist2 < 0))
      return FRACUNIT; // doesn't cross the line
   else
      return FixedDiv(dist1, dist1 - dist2);
}

//
// Call with p1 and p2 set to the endpoints, and nvx, nvy set to a 
// normalized vector. Assumes the start point is definitely on the front side
// of the line. Returns the fraction of the current move that crosses the line
// segment.
//
static void SL_ClipToLine(void)
{
   fixed_t frac;
   int     side2, side3;

   // adjust start so it will be the first point contacted on the player circle
   // p3, p4 are move endpoints

   p3x = slidex - CLIPRADIUS * nvx;
   p3y = slidey - CLIPRADIUS * nvy;
   p4x = p3x + slidedx;
   p4y = p3y + slidedy;

   // if the adjusted point is on the other side of the line, the endpoint must
   // be checked.
   side2 = SL_PointOnSide(p3x, p3y);
   if(side2 == SIDE_BACK)
      return; // ClipToPoint and slide along normal to line

   side3 = SL_PointOnSide(p4x, p4y);
   if(side3 == SIDE_ON)
      return; // the move goes flush with the wall
   else if(side3 == SIDE_FRONT)
      return; // move doesn't cross line

   if(side2 == SIDE_ON)
   {
      frac = 0; // moves toward the line
      goto blockmove;
   }

   // the line endpoints must be on opposite sides of the move trace

   // find the fractional intercept
   frac = SL_CrossFrac();

   if(frac < blockfrac)
   {
blockmove:
      blockfrac =  frac;
      blocknvx  = -nvy;
      blocknvy  =  nvx;
   }
}

//
// Check a linedef during wall sliding motion.
//
static boolean SL_CheckLine(line_t *ld)
{
   fixed_t   opentop, openbottom;
   sector_t *front, *back;
   int       side1;
   vertex_t *vtmp;

   // check bounding box
   if(endbox[BOXRIGHT ] < ld->bbox[BOXLEFT  ] ||
      endbox[BOXLEFT  ] > ld->bbox[BOXRIGHT ] ||
      endbox[BOXTOP   ] < ld->bbox[BOXBOTTOM] ||
      endbox[BOXBOTTOM] > ld->bbox[BOXTOP   ])
   {
      return true;
   }

   // see if it can possibly block movement
   if(!ld->backsector || (ld->flags & ML_BLOCKING))
      goto findfrac;

   front = ld->frontsector;
   back  = ld->backsector;

   if(front->floorheight > back->floorheight)
      openbottom = front->floorheight;
   else
      openbottom = back->floorheight;

   if(openbottom - slidething->z > 24*FRACUNIT)
      goto findfrac; // too big a step up

   if(front->ceilingheight < back->ceilingheight)
      opentop = front->ceilingheight;
   else
      opentop = back->ceilingheight;

   if(opentop - openbottom >= 56*FRACUNIT)
      return true; // the line doesn't block movement

   // the line is definitely blocking movement at this point
findfrac:
   p1  = ld->v1;
   p2  = ld->v2;
   nvx = finesine[ld->fineangle];
   nvy = -finecosine[ld->fineangle];
   
   side1 = SL_PointOnSide(slidex, slidey);
   switch(side1)
   {
   case SIDE_ON:
      return true;
   case SIDE_BACK:
      if(!ld->backsector)
         return true; // don't clip to backs of one-sided lines
      // reverse coordinates and angle
      vtmp = p1;
      p1   = p2;
      p2   = vtmp;
      nvx  = -nvx;
      nvy  = -nvy;
      break;
   default:
      break;
   }

   SL_ClipToLine();
   return true;
}

//
// Returns the fraction of the move that is completable.
//
fixed_t P_CompletableFrac(fixed_t dx, fixed_t dy)
{
   int xl, xh, yl, yh, bx, by;

   blockfrac = FRACUNIT;
   slidedx = dx;
   slidedy = dy;

   endbox[BOXTOP   ] = slidey + CLIPRADIUS * FRACUNIT;
   endbox[BOXBOTTOM] = slidey - CLIPRADIUS * FRACUNIT;
   endbox[BOXRIGHT ] = slidex + CLIPRADIUS * FRACUNIT;
   endbox[BOXLEFT  ] = slidex - CLIPRADIUS * FRACUNIT;

   if(dx > 0)
      endbox[BOXRIGHT ] += dx;
   else
      endbox[BOXLEFT  ] += dx;

   if(dy > 0)
      endbox[BOXTOP   ] += dy;
   else
      endbox[BOXBOTTOM] += dy;

   ++validcount;

   // check lines
   xl = (endbox[BOXLEFT  ] - bmaporgx) >> MAPBLOCKSHIFT;
   xh = (endbox[BOXRIGHT ] - bmaporgx) >> MAPBLOCKSHIFT;
   yl = (endbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
   yh = (endbox[BOXTOP   ] - bmaporgy) >> MAPBLOCKSHIFT;

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
         P_BlockLinesIterator(bx, by, SL_CheckLine);
   }

   // examine results
   if(blockfrac < 0x1000)
   {
      blockfrac   = 0;
      specialline = NULL; // can't cross anything on a bad move
      return 0;           // solid wall
   }

   return blockfrac;
}

//
// Point on side check for special crosses.
//
static int SL_PointOnSide2(fixed_t x1, fixed_t y1, 
                           fixed_t x2, fixed_t y2, 
                           fixed_t x3, fixed_t y3)
{
   fixed_t nx, ny;
   fixed_t dist;

   x1 = (x1 - x2);
   y1 = (y1 - y2);

   nx = (y3 - y2);
   ny = (x2 - x3);

   dist = FixedMul(x1, nx) + FixedMul(y1, ny);

   return ((dist < 0) ? SIDE_BACK : SIDE_FRONT);
}

static void SL_CheckSpecialLines(void)
{
   fixed_t x1 = slidething->x;
   fixed_t y1 = slidething->y;
   fixed_t x2 = slidex;
   fixed_t y2 = slidey;

   fixed_t bx, by, xl, xh, yl, yh;
   fixed_t bxl, bxh, byl, byh;
   fixed_t x3, y3, x4, y4;
   int side1, side2;

   if(x1 < x2)
   {
      xl = x1;
      xh = x2;
   }
   else
   {
      xl = x2;
      xh = x1;
   }

   if(y1 < y2)
   {
      yl = y1;
      yh = y2;
   }
   else
   {
      yl = y2;
      yh = y1;
   }

   bxl = (xl - bmaporgx) >> MAPBLOCKSHIFT;
   bxh = (xh - bmaporgx) >> MAPBLOCKSHIFT;
   byl = (yl - bmaporgy) >> MAPBLOCKSHIFT;
   byh = (yh - bmaporgy) >> MAPBLOCKSHIFT;

   if(bxl < 0)
      bxl = 0;
   if(byl < 0)
      byl = 0;
   if(bxh >= bmapwidth)
      bxh = bmapwidth - 1;
   if(byh >= bmapheight)
      byh = bmapheight - 1;

   specialline = NULL;
   ++validcount;

   for(bx = bxl; bx <= bxh; bx++)
   {
      for(by = byl; by <= byh; by++)
      {
         short  *list;
         line_t *ld;
         int offset = by * bmapwidth + bx;
         offset = *(blockmap + offset);
         
         for(list = blockmaplump + offset; *list != -1; list++)
         {
            ld = &lines[*list];
            if(!ld->special)
               continue;
            if(ld->validcount == validcount)
               continue; // already checked
            
            ld->validcount = validcount;

            if(xh < ld->bbox[BOXLEFT  ] ||
               xl > ld->bbox[BOXRIGHT ] ||
               yh < ld->bbox[BOXBOTTOM] ||
               yl > ld->bbox[BOXTOP   ])
            {
               continue;
            }

            x3 = ld->v1->x;
            y3 = ld->v1->y;
            x4 = ld->v2->x;
            y4 = ld->v2->y;

            side1 = SL_PointOnSide2(x1, y1, x3, y3, x4, y4);
            side2 = SL_PointOnSide2(x2, y2, x3, y3, x4, y4);

            if(side1 == side2)
               continue; // move doesn't cross line

            side1 = SL_PointOnSide2(x3, y3, x1, y1, x2, y2);
            side2 = SL_PointOnSide2(x4, y4, x1, y1, x2, y2);

            if(side1 == side2)
               continue; // line doesn't cross move

            specialline = ld;
            return;
         }
      }
   }
}

//
// Try to slide the player against walls by finding the closest move available.
//
void P_SlideMove(void)
{
   fixed_t dx, dy, rx, ry;
   fixed_t frac, slide;
   int i;

   dx = slidething->momx;
   dy = slidething->momy;
   slidex = slidething->x;
   slidey = slidething->y;

   // perform a maximum of three bumps
   for(i = 0; i < 3; i++)
   {
      frac = P_CompletableFrac(dx, dy);
      if(frac != FRACUNIT)
         frac -= 0x1000;
      if(frac < 0)
         frac = 0;

      rx = FixedMul(frac, dx);
      ry = FixedMul(frac, dy);

      slidex += rx;
      slidey += ry;

      // made it the entire way
      if(frac == FRACUNIT)
      {
         slidething->momx = dx;
         slidething->momy = dy;
         SL_CheckSpecialLines();
         return;
      }

      // project the remaining move along the line that blocked movement
      dx -= rx;
      dy -= ry;
      slide = FixedMul(dx, blocknvx) + FixedMul(dy, blocknvy);

      dx = FixedMul(slide, blocknvx);
      dy = FixedMul(slide, blocknvy);
   }

   // some hideous situation has happened that won't let the player slide
   slidex = slidething->x;
   slidey = slidething->y;
   slidething->momx = slidething->momy = 0;
}

// EOF

