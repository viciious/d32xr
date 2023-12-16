//** Copyright(C) 1993-1996 Id Software, Inc.
//** Copyright(C) 1993-1996 Raven Software
//** Copyright(C) 2006 Randi Heit

//**************************************************************************
//**
//** p_sight2.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: p_sight.c,v $
//** $Revision: 1.1 $
//** $Date: 95/05/11 00:22:50 $
//** $Author: bgokey $
//**
//**************************************************************************

#include <limits.h>
#include "doomdef.h"
#include "p_local.h"

/*
====================
=
= P_SightTraverseIntercepts
=
= Returns true if the traverser function returns true for all lines
====================
*/
boolean P_SightTraverseIntercepts(sightWork_t *sw)
{
   int count;
   fixed_t dist;
   intercept_t *scan, *in;
   intercept_t *intercepts, *intercept_p;

   count = sw->numintercepts;
   intercepts = sw->intercepts;
   intercept_p = intercepts + count;

   //
   // go through in order
   //
   in = intercepts; // shut up compiler warning
   if (!count)
      return true;

   do
   {
      dist = INT32_MAX;
      for (scan = intercepts; scan < intercept_p; scan++)
         if (scan->frac < dist)
         {
            dist = scan->frac;
            in = scan;
         }

      if (!PTR_SightTraverse(sw, in))
         return false; // don't bother going farther
      in->frac = INT32_MAX;
   } while (--count);

   return true; // everything was traversed
}

boolean P_SightPathTraverse(sightWork_t *sw)
{
   fixed_t xt1, yt1, xt2, yt2;
   fixed_t xstep, ystep;
   fixed_t partialx, partialy;
   fixed_t xintercept, yintercept;
   int mapx, mapy, mapxstep, mapystep;
   divline_t *strace = &sw->strace;
   int count;
   fixed_t x1 = sw->strace.x, y1 = sw->strace.y;
   fixed_t x2 = sw->t2x, y2 = sw->t2y;

   sw->numintercepts = 0;

   if (((x1 - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0)
      x1 += FRACUNIT; // don't side exactly on a line
   if (((y1 - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0)
      y1 += FRACUNIT; // don't side exactly on a line
   strace->x = x1;
   strace->y = y1;
   strace->dx = x2 - x1;
   strace->dy = y2 - y1;

   x1 -= bmaporgx;
   y1 -= bmaporgy;

   x2 -= bmaporgx;
   y2 -= bmaporgy;

   if (x1 < 0 || y1 < 0 || x2 < 0 || y2 < 0)
      return false;

   xt1 = (unsigned)x1 >> MAPBLOCKSHIFT;
   yt1 = (unsigned)y1 >> MAPBLOCKSHIFT;

   xt2 = (unsigned)x2 >> MAPBLOCKSHIFT;
   yt2 = (unsigned)y2 >> MAPBLOCKSHIFT;

   // points should never be out of bounds, but check once instead of
   // each block
   if (xt1 >= bmapwidth || yt1 >= bmapheight || xt2 >= bmapwidth || yt2 >= bmapheight)
      return false;

   if (xt2 > xt1)
   {
      mapxstep = 1;
      partialx = FRACUNIT - (((unsigned)x1 >> MAPBTOFRAC) & (FRACUNIT - 1));
      ystep = FixedDiv(y2 - y1, D_abs(x2 - x1));
   }
   else if (xt2 < xt1)
   {
      mapxstep = -1;
      partialx = ((unsigned)x1 >> MAPBTOFRAC) & (FRACUNIT - 1);
      ystep = FixedDiv(y2 - y1, D_abs(x2 - x1));
   }
   else
   {
      mapxstep = 0;
      partialx = FRACUNIT;
      ystep = 256 * FRACUNIT;
   }
   yintercept = ((unsigned)y1 >> MAPBTOFRAC) + FixedMul(partialx, ystep);

   if (yt2 > yt1)
   {
      mapystep = 1;
      partialy = FRACUNIT - (((unsigned)y1 >> MAPBTOFRAC) & (FRACUNIT - 1));
      xstep = FixedDiv(x2 - x1, D_abs(y2 - y1));
   }
   else if (yt2 < yt1)
   {
      mapystep = -1;
      partialy = ((unsigned)y1 >> MAPBTOFRAC) & (FRACUNIT - 1);
      xstep = FixedDiv(x2 - x1, D_abs(y2 - y1));
   }
   else
   {
      mapystep = 0;
      partialy = FRACUNIT;
      xstep = 256 * FRACUNIT;
   }
   xintercept = ((unsigned)x1 >> MAPBTOFRAC) + FixedMul(partialy, xstep);

   // [RH] Fix for traces that pass only through blockmap corners. In that case,
   // xintercept and yintercept can both be set ahead of mapx and mapy, so the
   // for loop would never advance anywhere.

   if (D_abs(xstep) == FRACUNIT && D_abs(ystep) == FRACUNIT)
   {
      if (ystep < 0)
         partialx = FRACUNIT - partialx;
      if (xstep < 0)
         partialy = FRACUNIT - partialy;
      if (partialx == partialy)
      {
         xintercept = xt1 << FRACBITS;
         yintercept = yt1 << FRACBITS;
      }
   }

   //
   // step through map blocks
   // Count is present to prevent a round off error from skipping the break
   mapx = xt1;
   mapy = yt1;

   for (count = 0; count < 64; count++)
   {
      if (sw->numintercepts == MAXINTERCEPTS)
         break;

      if (!P_SightBlockLinesIterator(sw, mapx, mapy))
         return false; // early out

      if (mapxstep == 0 || mapystep == 0)
         break;

      switch ((((yintercept >> FRACBITS) == mapy) << 1) | ((xintercept >> FRACBITS) == mapx))
      {
      case 0: // neither xintercept nor yintercept match!
         // Continuing won't make things any better, so we might as well stop right here
         goto traverse;

      case 1: // xintercept matches
         xintercept += xstep;
         mapy += mapystep;
         if (mapy == yt2)
            mapystep = 0;
         break;

      case 2: // yintercept matches
         yintercept += ystep;
         mapx += mapxstep;
         if (mapx == xt2)
            mapxstep = 0;
         break;

      case 3: // xintercept and yintercept both match
         // The trace is exiting a block through its corner. Not only does the block
         // being entered need to be checked (which will happen when this loop
         // continues), but the other two blocks adjacent to the corner also need to
         // be checked.
         if (!P_SightBlockLinesIterator(sw, mapx + mapxstep, mapy) ||
             !P_SightBlockLinesIterator(sw, mapx, mapy + mapystep))
            return false;

         xintercept += xstep;
         yintercept += ystep;
         mapx += mapxstep;
         mapy += mapystep;
         if (mapx == xt2)
            mapxstep = 0;
         if (mapy == yt2)
            mapystep = 0;
         break;
      }
   }

   //
   // couldn't early out, so go through the sorted list
   //
traverse:
   return P_SightTraverseIntercepts(sw);
}
