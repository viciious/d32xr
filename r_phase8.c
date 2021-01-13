/*
  CALICO

  Renderer phase 8 - Sprites
*/

#include "r_local.h"

static int spropening[SCREENWIDTH + 1];

static void R_DrawVisSprite(vissprite_t *vis)
{
   patch_t *patch;
   fixed_t  iscale, xfrac, spryscale, sprtop, fracstep;
   int light, x, stopx;

   patch     = vis->patch;
   iscale    = vis->yiscale;
   xfrac     = vis->startfrac;
   spryscale = vis->yscale;

   sprtop = CENTERYFRAC - FixedMul(vis->texturemid, spryscale);
   spryscale >>= 8;

   // blitter iinc
   light = -((255 - vis->colormap) << 14) & 0xffffff;

   stopx    = vis->x2 + 1;
   fracstep = vis->xiscale;
   
   for(x = vis->x1; x < stopx; x++, xfrac += fracstep)
   {
      column_t *column = (column_t *)((byte *)patch + BIGSHORT(patch->columnofs[xfrac>>FRACBITS]));
      int topclip      = spropening[x] >> 8;
      int bottomclip   = (spropening[x] & 0xff) - 1;

      // column loop
      // a post record has four bytes: topdelta length pixelofs*2
      for(; column->topdelta != 0xff; column++)
      {
         int top    = ((column->topdelta * spryscale) << 8) + sprtop;
         int bottom = ((column->length   * spryscale) << 8) + top;
         int count;
         fixed_t frac;

         top += (FRACUNIT - 1);
         top /= FRACUNIT;
         bottom -= 1;
         bottom /= FRACUNIT;

         // clip to bottom
         if(bottom > bottomclip)
            bottom = bottomclip;

         frac = 0;

         // clip to top
         if(topclip > top)
         {
            frac += (topclip - top) * iscale;
            top = topclip;
         }

         // calc count
         count = bottom - top + 1;
         if(!count)
            continue;

         // CALICO: invoke column drawer
         I_DrawColumn(x, top, bottom, light, frac, iscale, vis->pixels + BIGSHORT(column->dataofs), 128);
      }
   }
}

//
// Compare the vissprite to a viswall. Similar to R_PointOnSegSide, but less accurate.
//
static boolean R_SegBehindPoint(viswall_t *viswall, int dx, int dy)
{
   fixed_t x1, y1, sdx, sdy;

   x1  = viswall->seg->v1->x;
   y1  = viswall->seg->v1->y;
   sdx = viswall->seg->v2->x;
   sdy = viswall->seg->v2->y;

   sdx -= x1;
   sdy -= y1;
   dx  -= x1;
   dy  -= y1;

   sdx /= FRACUNIT;
   sdy /= FRACUNIT;
   dx  /= FRACUNIT;
   dy  /= FRACUNIT;

   dx  *= sdy;
   sdx *=  dy;

   return (sdx < dx);
}

//
// Clip a sprite to the openings created by walls
//
static void R_ClipVisSprite(vissprite_t *vis)
{
   int     x;          // r15
   int     x1;         // FP+5
   int     x2;         // r22
   fixed_t gz;         // FP+8
   int     gzt;        // FP+9
   int     scalefrac;  // FP+3
   int     r1;         // FP+7
   int     r2;         // r18
   int     silhouette; // FP+4
   byte   *topsil;     // FP+6
   byte   *bottomsil;  // r21
   int     opening;    // r16
   int     top;        // r19
   int     bottom;     // r20
   
   viswall_t *ds;      // r17

   x1  = vis->x1;
   x2  = vis->x2;
   gz  = (vis->gz  - viewz) / (1 << 10);
   gzt = (vis->gzt - viewz) / (1 << 10);
   
   scalefrac = vis->yscale;
   
   x = vis->x1;

   while(x <= x2)
   {
      spropening[x] = SCREENHEIGHT;
      ++x;
   }
   
   ds = lastwallcmd;
   do
   {
      --ds;

      if(ds->start > x2 || ds->stop < x1 ||                            // does not intersect
         (ds->scalefrac < scalefrac && ds->scale2 < scalefrac) ||      // is completely behind
         !(ds->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL))) // does not clip sprites
      {
         continue;
      }

      if(ds->scalefrac <= scalefrac || ds->scale2 <= scalefrac)
      {
         if(R_SegBehindPoint(ds, vis->gx, vis->gy))
            continue;
      }

      r1 = ds->start < x1 ? x1 : ds->start;
      r2 = ds->stop  > x2 ? x2 : ds->stop;

      silhouette = (ds->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL));

      if(silhouette == AC_SOLIDSIL)
      {
         x = r1;
         while(x <= r2)
         {
            spropening[x] = (SCREENHEIGHT << 8);
            ++x;
         }
         continue;
      }

      topsil    = ds->topsil;
      bottomsil = ds->bottomsil;

      if(silhouette == AC_BOTTOMSIL)
      {
         x = r1;
         while(x <= r2)
         {
            opening = spropening[x];
            if((opening & 0xff) == SCREENHEIGHT)
               spropening[x] = (opening & OPENMARK) + bottomsil[x];
            ++x;
         }
      }
      else if(silhouette == AC_TOPSIL)
      {
         x = r1;
         while(x <= r2)
         {
            opening = spropening[x];
            if(!(opening & OPENMARK))
               spropening[x] = (topsil[x] << 8) + (opening & 0xff);
            ++x;
         }
      }
      else if(silhouette == (AC_TOPSIL | AC_BOTTOMSIL))
      {
         x = r1;
         while(x <= r2)
         {
            top    = spropening[x];
            bottom = top & 0xff;
            top >>= 8;
            if(bottom == SCREENHEIGHT)
               bottom = bottomsil[x];
            if(top == 0)
               top = topsil[x];
            spropening[x] = (top << 8) + bottom;
            ++x;
         }
      }
   }
   while(ds != viswalls);
}

//
// Render all sprites
//
void R_Sprites(void)
{
   ptrdiff_t i = 0, count = lastsprite_p - vissprites;
   vissprite_t *best = NULL;

   // draw mobj sprites
   while(i < count)
   {
      fixed_t bestscale = D_MAXINT;
      vissprite_t *ds = vissprites;

      while(ds != lastsprite_p)
      {
         if(ds->xscale < bestscale)
         {
            bestscale = ds->xscale;
            best = ds;
         }
         ++ds;
      }

      if(best->patch != NULL)
      {
         R_ClipVisSprite(best);
         R_DrawVisSprite(best);
      }

      best->xscale = D_MAXINT;

      ++i;
   }

   // draw psprites
   while(lastsprite_p < vissprite_p)
   {
      ptrdiff_t stopx = lastsprite_p->x2 + 1;
      i = lastsprite_p->x1;
      
      // clear out the clipping array across the range of the psprite
      while(i < stopx)
      {
         spropening[i] = SCREENHEIGHT;
         ++i;
      }

      R_DrawVisSprite(lastsprite_p);
      
      ++lastsprite_p;
   }
}

// EOF

