/*
  CALICO

  Renderer phase 8 - Sprites
*/

#include "r_local.h"
#ifdef MARS
#include "mars.h"
#endif
#include <stdlib.h>

static VINT sprscreenhalf;
static int fuzzpos[2];

static boolean R_SegBehindPoint(viswall_t *viswall, int dx, int dy) ATTR_DATA_CACHE_ALIGN;
void R_DrawVisSprite(vissprite_t* vis, unsigned short* spropening, int *fuzzpos, int screenhalf) ATTR_DATA_CACHE_ALIGN;
void R_ClipVisSprite(vissprite_t *vis, unsigned short *spropening, int screenhalf) ATTR_DATA_CACHE_ALIGN;
static void R_DrawSpritesLoop(const int cpu, int* sortedsprites, int count) ATTR_DATA_CACHE_ALIGN;
static void R_DrawPSprites(const int cpu) ATTR_DATA_CACHE_ALIGN;
void R_Sprites(void) ATTR_DATA_CACHE_ALIGN;

void R_DrawVisSprite(vissprite_t *vis, unsigned short *spropening, int *fuzzpos, int screenhalf)
{
   patch_t *patch;
   fixed_t  iscale, xfrac, spryscale, sprtop, fracstep;
   int light, x, stopx;
   drawcol_t drawcol;

   patch     = vis->patch;
   iscale    = vis->yiscale;
   xfrac     = vis->startfrac;
   spryscale = vis->yscale;
   drawcol   = vis->drawcol;

   FixedMul2(sprtop, vis->texturemid, spryscale);
   sprtop = centerYFrac - sprtop;
   spryscale = (unsigned)spryscale >> 8;

   // blitter iinc
   light    = vis->colormap;
   x        = vis->x1;
   stopx    = vis->x2 + 1;
   fracstep = vis->xiscale;

#ifdef MARS
   if (screenhalf)
   {
       if (screenhalf == 1)
       {
           if (stopx > sprscreenhalf)
               stopx = sprscreenhalf;
       }
       else
       {
           if (x < sprscreenhalf)
           {
               xfrac += (sprscreenhalf - x) * fracstep;
               x = sprscreenhalf;
           }
       }
   }
#endif

   for(; x < stopx; x++, xfrac += fracstep)
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
         if(count <= 0)
            continue;

         // CALICO: invoke column drawer
         drawcol(x, top, bottom, light, frac, iscale, vis->pixels + BIGSHORT(column->dataofs), 128, fuzzpos);
      }
   }
}

//
// Compare the vissprite to a viswall. Similar to R_PointOnSegSide, but less accurate.
//
static boolean R_SegBehindPoint(viswall_t *viswall, int dx, int dy)
{
   fixed_t x1, y1, sdx, sdy;
   vertex_t *v1 = &vertexes[viswall->seg->v1], *v2 = &vertexes[viswall->seg->v2];

   x1  = v1->x;
   y1  = v1->y;
   sdx = v2->x;
   sdy = v2->y;

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
void R_ClipVisSprite(vissprite_t *vis, unsigned short *spropening, int screenhalf)
{
   int     x;          // r15
   int     x1;         // FP+5
   int     x2;         // r22
   unsigned scalefrac; // FP+3
   int     r1;         // FP+7
   int     r2;         // r18
   int     silhouette; // FP+4
   byte   *topsil;     // FP+6
   byte   *bottomsil;  // r21
   int     opening;    // r16
   unsigned short top;        // r19
   unsigned short bottom;     // r20
   viswall_t *ds;      // r17

   x1  = vis->x1;
   x2  = vis->x2;
   scalefrac = vis->yscale;  

#ifdef MARS
   if (screenhalf)
   {
       if (screenhalf == 1)
       {
           if (x2 >= sprscreenhalf)
               x2 = sprscreenhalf - 1;
       }
       else
       {
           if (x1 < sprscreenhalf)
               x1 = sprscreenhalf;
       }
   }

   if (x1 > x2)
       return;
#endif

   for(x = x1; x <= x2; x++)
      spropening[x] = viewportHeight;
   
   ds = lastwallcmd;
   if (ds == viswalls)
       return;

   do
   {
      --ds;

      silhouette = (ds->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL));

      if(ds->start > x2 || ds->stop < x1 ||                            // does not intersect
         (ds->scalefrac < scalefrac && ds->scale2 < scalefrac) ||      // is completely behind
         !silhouette)                                                  // does not clip sprites
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

      if(silhouette == AC_SOLIDSIL)
      {
         for (x = r1;  x <= r2; x++)
            spropening[x] = OPENMARK;
         continue;
      }

      topsil    = ds->topsil;
      bottomsil = ds->bottomsil;

      if(silhouette == AC_BOTTOMSIL)
      {
         for(x = r1; x <= r2; x++)
         {
            opening = spropening[x];
            if((opening & 0xff) == viewportHeight)
               spropening[x] = (opening & OPENMARK) | bottomsil[x];
         }
      }
      else if(silhouette == AC_TOPSIL)
      {
         for(x = r1; x <= r2; x++)
         {
            opening = spropening[x];
            if(!(opening & OPENMARK))
               spropening[x] = (topsil[x] << 8) | (opening & 0xff);
         }
      }
      else if(silhouette == (AC_TOPSIL | AC_BOTTOMSIL))
      {
         for(x = r1; x <= r2; x++)
         {
            top    = spropening[x];
            bottom = top & 0xff;
            top >>= 8;
            if(bottom == viewportHeight)
               bottom = bottomsil[x];
            if(top == 0)
               top = topsil[x];
            spropening[x] = (top << 8) | bottom;
         }
      }
   }
   while(ds != viswalls);
}

static void R_DrawSpritesLoop(const int cpu, int* sortedsprites, int count)
{
    int i;
    unsigned short spropening[SCREENWIDTH];

    for (i = 0; i < count; i++)
    {
        vissprite_t* ds;

        ds = vissprites + (sortedsprites[i] & 0x7f);

        R_ClipVisSprite(ds, spropening, cpu+1);

        R_DrawVisSprite(ds, spropening, &fuzzpos[cpu], cpu+1);
    }
}

static void R_DrawPSprites(const int cpu)
{
    unsigned i;
    unsigned short spropening[SCREENWIDTH];
    vissprite_t *vis = lastsprite_p;

    // draw psprites
    while (vis < vissprite_p)
    {
        unsigned  stopx = vis->x2 + 1;
        i = vis->x1;

        if (vis->patch == NULL)
            continue;

        // clear out the clipping array across the range of the psprite
        while (i < stopx)
        {
            spropening[i] = viewportHeight;
            ++i;
        }

        R_DrawVisSprite(vis, spropening, &fuzzpos[cpu], cpu+1);

        ++vis;
    }
}

#ifdef MARS
void Mars_Sec_R_DrawSprites(int* sortedsprites, int count)
{
    Mars_ClearCacheLines((intptr_t)sortedsprites & ~15, (count * sizeof(*sortedsprites) + 15) / 16);

    Mars_ClearCacheLines((intptr_t)&vissprites & ~15, 1);
    Mars_ClearCacheLines((intptr_t)&vissprite_p & ~15, 1);
    Mars_ClearCacheLines((intptr_t)vissprites & ~15, ((vissprite_p - vissprites) * sizeof(vissprite_t) + 15) / 16);

    Mars_ClearCacheLines((intptr_t)&sprscreenhalf & ~15, 1);

    R_DrawSpritesLoop(1, sortedsprites, count);
}

void Mars_Sec_R_DrawPSprites(void)
{
    Mars_ClearCacheLines((intptr_t)&sprscreenhalf & ~15, 1);
    R_DrawPSprites(1);
}

#endif

//
// Render all sprites
//
void R_Sprites(void)
{
   int i = 0, count;
   int half, sortedcount;
   unsigned midcount;
   int sortedsprites[MAXVISSPRITES];

   sortedcount = 0;
   count = lastsprite_p - vissprites;
   if (count > MAXVISSPRITES)
       count = MAXVISSPRITES;

   // sort mobj sprites by distance (back to front)
   // find approximate average middle point for all
   // sprites - this will be used to split the draw 
   // load between the two CPUs on the 32X
   half = 0;
   midcount = 0;
   for (i = 0; i < count; i++)
   {
       vissprite_t* ds = vissprites + i;
       if (ds->patch == NULL)
           continue;
       if (ds->x1 > ds->x2)
           continue;

       // average mid point
       unsigned xscale = ds->xscale;
       unsigned pixcount = ds->x2 + 1 - ds->x1;
       if (pixcount > 10) // FIXME: an arbitrary number
       {
           midcount += xscale;
           half += (ds->x1 + (pixcount >> 1)) * xscale;
       }

       // composite sort key: distance + id
       sortedsprites[sortedcount++] = (xscale << 7) + i;
   }

   D_isort(sortedsprites, sortedcount);

   // draw mobj sprites

#ifdef MARS
   // average the mid point
   if (midcount > 0)
       half /= midcount;
   if (!half || half > viewportWidth)
       half = viewportWidth / 2;
   sprscreenhalf = half;

   Mars_R_BeginDrawSprites(sortedsprites, sortedcount);

   Mars_ClearCacheLines((intptr_t)&lastopening & ~15, 1);
   Mars_ClearCacheLines((intptr_t)openings & ~15, ((lastopening - openings) * sizeof(*openings) + 15) / 16);
   R_DrawSpritesLoop(0, sortedsprites, sortedcount);

   Mars_R_EndDrawSprites();

   half = viewportWidth / 2;
   if (!lowResMode)
       half /= 2;
   sprscreenhalf = half;

   Mars_R_BeginDrawPSprites();

   R_DrawPSprites(0);

   Mars_R_EndDrawPSprites();
#else
   R_DrawSpritesLoop(0, sortedsprites, sortedcount);

   R_DrawPSprites(0);
#endif
}

// EOF

