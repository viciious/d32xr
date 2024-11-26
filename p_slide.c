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
static int SL_PointOnSide(pslidework_t *sw, fixed_t x, fixed_t y)
{
fixed_t dx, dy, dist;

   dx = x - sw->p1.x;
   dy = y - sw->p1.y;

   dx = FixedMul(dx, sw->nvx);
   dy = FixedMul(dy, sw->nvy);
   dist = dx + dy;

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
static fixed_t SL_CrossFrac(pslidework_t *sw)
{
   fixed_t dx, dy, dist1, dist2;

   // project move start and end points onto line normal
   dx = sw->p3x - sw->p1.x;
   dy = sw->p3y - sw->p1.y;
   dx = FixedMul(dx, sw->nvx);
   dy = FixedMul(dy, sw->nvy);
   dist1 = dx + dy;

   dx = sw->p4x - sw->p1.x;
   dy = sw->p4y - sw->p1.y;
   dx = FixedMul(dx, sw->nvx);
   dy = FixedMul(dy, sw->nvy);
   dist2 = dx + dy;

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
static void SL_ClipToLine(pslidework_t *sw)
{
   fixed_t frac;
   int     side2, side3;

   // adjust start so it will be the first point contacted on the player circle
   // p3, p4 are move endpoints

   sw->p3x = sw->slidex - CLIPRADIUS * sw->nvx;
   sw->p3y = sw->slidey - CLIPRADIUS * sw->nvy;
   sw->p4x = sw->p3x + sw->slidedx;
   sw->p4y = sw->p3y + sw->slidedy;

   // if the adjusted point is on the other side of the line, the endpoint must
   // be checked.
   side2 = SL_PointOnSide(sw, sw->p3x, sw->p3y);
   if(side2 == SIDE_BACK)
      return; // ClipToPoint and slide along normal to line

   side3 = SL_PointOnSide(sw, sw->p4x, sw->p4y);
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
   frac = SL_CrossFrac(sw);

   if(frac < sw->blockfrac)
   {
blockmove:
      sw->blockfrac =  frac;
      sw->blocknvx  = -sw->nvy;
      sw->blocknvy  =  sw->nvx;
   }
}

void GetSectorAABB(sector_t *sector, fixed_t bbox[4]);
void P_SpawnBustables(sector_t *sec, mobj_t *playermo)
{
   fixed_t blockbox[4];
   GetSectorAABB(sec, blockbox);
   const fixed_t spawnInterval = 64<<FRACBITS;

   for (fixed_t z = sec->ceilingheight - spawnInterval/2; z >= sec->floorheight + spawnInterval/2; z -= spawnInterval/2)
   {
      for (fixed_t x = blockbox[BOXLEFT]; x <= blockbox[BOXRIGHT]; x += spawnInterval)
      {
         for (fixed_t y = blockbox[BOXBOTTOM]; y <= blockbox[BOXTOP]; y += spawnInterval)
         {
            if (R_PointInSubsector(x, y)->sector == sec)
               P_SpawnMobj(x, y, z, MT_GFZDEBRIS);               
         }
      }
   }
}

//
// Check a linedef during wall sliding motion.
//
static boolean SL_CheckLine(line_t *ld, pslidework_t *sw)
{
   fixed_t   opentop, openbottom;
   sector_t *front, *back;
   int       side1, dx, dy;
   angle_t fineangle;
   vertex_t vtmp;
   fixed_t  ldbbox[4];

   P_LineBBox(ld, ldbbox);

   // check bounding box
   if(sw->endbox[BOXRIGHT ] < ldbbox[BOXLEFT  ] ||
      sw->endbox[BOXLEFT  ] > ldbbox[BOXRIGHT ] ||
      sw->endbox[BOXTOP   ] < ldbbox[BOXBOTTOM] ||
      sw->endbox[BOXBOTTOM] > ldbbox[BOXTOP   ])
   {
      return true;
   }

   // see if it can possibly block movement
   if(ld->sidenum[1] == -1 || (ld->flags & ML_BLOCKING))
      goto findfrac;

   front = LD_FRONTSECTOR(ld);
   back  = LD_BACKSECTOR(ld);

   if (ld->special == 254 && ld->tag > 0 && sw->slidething->player
      && (players[sw->slidething->player-1].pflags & PF_SPINNING)) // Bustable block
   {
      back->floorheight = P_FindNextLowestFloor(back, back->floorheight);
      ld->special = 0;
      ld->tag = 0;
      S_StartSound(sw->slidething, sfx_s3k_59);
      P_SpawnBustables(back, sw->slidething);
   }

   if(front->floorheight > back->floorheight)
      openbottom = front->floorheight;
   else
      openbottom = back->floorheight;

   if(openbottom - sw->slidething->z > 24*FRACUNIT)
      goto findfrac; // too big a step up

   if(front->ceilingheight < back->ceilingheight)
      opentop = front->ceilingheight;
   else
      opentop = back->ceilingheight;

   if(opentop - openbottom >= sw->slidething->theight << FRACBITS)
      return true; // the line doesn't block movement

   // the line is definitely blocking movement at this point
findfrac:
   sw->p1.x = vertexes[ld->v1].x << FRACBITS;
   sw->p1.y = vertexes[ld->v1].y << FRACBITS;
   sw->p2.x = vertexes[ld->v2].x << FRACBITS;
   sw->p2.y = vertexes[ld->v2].y << FRACBITS;

   dx = sw->p2.x - sw->p1.x;
   dy = sw->p2.y - sw->p1.y;
   fineangle = ( dy == 0 ) ? (( dx < 0 ) ? ANG180 : 0 ) :
               ( dx == 0 ) ? (( dy < 0 ) ? ANG270 : ANG90 ) :
               R_PointToAngle2(0, 0, dx, dy);
   fineangle >>= ANGLETOFINESHIFT;

   sw->nvx = finesine(fineangle);
   sw->nvy = -finecosine(fineangle);
   
   side1 = SL_PointOnSide(sw, sw->slidex, sw->slidey);
   switch(side1)
   {
   case SIDE_ON:
      return true;
   case SIDE_BACK:
      if(ld->sidenum[1] == -1)
         return true; // don't clip to backs of one-sided lines
      // reverse coordinates and angle
      vtmp.x = sw->p1.x;
      vtmp.y = sw->p1.y;
      sw->p1.x = sw->p2.x;
      sw->p1.y = sw->p2.y;
      sw->p2.x = vtmp.x;
      sw->p2.y = vtmp.y;
      sw->nvx  = -sw->nvx;
      sw->nvy  = -sw->nvy;
      break;
   default:
      break;
   }

   SL_ClipToLine(sw);
   return true;
}

//
// Returns the fraction of the move that is completable.
//
fixed_t P_CompletableFrac(pslidework_t *sw, fixed_t dx, fixed_t dy)
{
   int xl, xh, yl, yh, bx, by;
   VINT *lvalidcount;

   sw->blockfrac = FRACUNIT;
   sw->slidedx = dx;
   sw->slidedy = dy;

   sw->endbox[BOXTOP   ] = sw->slidey + CLIPRADIUS * FRACUNIT;
   sw->endbox[BOXBOTTOM] = sw->slidey - CLIPRADIUS * FRACUNIT;
   sw->endbox[BOXRIGHT ] = sw->slidex + CLIPRADIUS * FRACUNIT;
   sw->endbox[BOXLEFT  ] = sw->slidex - CLIPRADIUS * FRACUNIT;

   if(dx > 0)
      sw->endbox[BOXRIGHT ] += dx;
   else
      sw->endbox[BOXLEFT  ] += dx;

   if(dy > 0)
      sw->endbox[BOXTOP   ] += dy;
   else
      sw->endbox[BOXBOTTOM] += dy;

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   *lvalidcount = *lvalidcount + 1;
   if (*lvalidcount == 0)
      *lvalidcount = 1;

   // check lines
   xl = sw->endbox[BOXLEFT  ] - bmaporgx;
   xh = sw->endbox[BOXRIGHT ] - bmaporgx;
   yl = sw->endbox[BOXBOTTOM] - bmaporgy;
   yh = sw->endbox[BOXTOP   ] - bmaporgy;

   if(xl < 0)
      xl = 0;
   if(yl < 0)
      yl = 0;
   if(yh < 0)
      return FRACUNIT;
   if(xh < 0)
      return FRACUNIT;

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
         P_BlockLinesIterator(bx, by, (blocklinesiter_t)SL_CheckLine, sw);
   }

   // examine results
   if(sw->blockfrac < 0x1000)
   {
      sw->blockfrac   = 0;
      return 0;           // solid wall
   }

   return sw->blockfrac;
}

//
// Try to slide the player against walls by finding the closest move available.
//
void P_SlideMove(pslidemove_t *sm)
{
   int i;
   fixed_t dx, dy, rx, ry;
   fixed_t frac, slide;
   pslidework_t sw;
   mobj_t *slidething = sm->slidething;

   dx = slidething->momx;
   dy = slidething->momy;
   sw.slidex = slidething->x;
   sw.slidey = slidething->y;
   sw.slidething = slidething;

   // perform a maximum of three bumps
   for(i = 0; i < 3; i++)
   {
      frac = P_CompletableFrac(&sw, dx, dy);
      if(frac != FRACUNIT)
         frac -= 0x1000;
      if(frac < 0)
         frac = 0;

      rx = FixedMul(frac, dx);
      ry = FixedMul(frac, dy);

      sw.slidex += rx;
      sw.slidey += ry;

      // made it the entire way
      if(frac == FRACUNIT)
      {
         slidething->momx = dx;
         slidething->momy = dy;
         sm->slidex = sw.slidex;
         sm->slidey = sw.slidey;
         return;
      }

      // project the remaining move along the line that blocked movement
      dx -= rx;
      dy -= ry;
      dx = FixedMul(dx, sw.blocknvx);
      dy = FixedMul(dy, sw.blocknvy);
      slide = dx + dy;

      dx = FixedMul(slide, sw.blocknvx);
      dy = FixedMul(slide, sw.blocknvy);
   }

   // some hideous situation has happened that won't let the player slide
   sm->slidex = slidething->x;
   sm->slidey = slidething->y;
   sm->slidething->momx = slidething->momy = 0;
}

// EOF

