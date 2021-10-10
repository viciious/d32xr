/*
  CALICO

  Renderer phase 8 - Sprites
*/

#include "r_local.h"
#ifdef MARS
#include "mars.h"
#endif
#include <stdlib.h>

static int sortedcount;
static int *sortedsprites;
static int fuzzpos[2];

static boolean R_SegBehindPoint(viswall_t *viswall, int dx, int dy) ATTR_DATA_CACHE_ALIGN;
void R_DrawVisSprite(vissprite_t* vis, unsigned short* spropening, int *fuzzpos) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void R_ClipVisSprite(vissprite_t *vis, unsigned short *spropening) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;;
static void R_DrawSpritesStride(const int start, int* fuzzpos) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void R_Sprites(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

void R_DrawVisSprite(vissprite_t *vis, unsigned short *spropening, int *fuzzpos)
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
   light = HWLIGHT(vis->colormap);
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
void R_ClipVisSprite(vissprite_t *vis, unsigned short *spropening)
{
   int     x;          // r15
   int     x1;         // FP+5
   int     x2;         // r22
   int     scalefrac;  // FP+3
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

   for(x = vis->x1; x <= x2; x++)
      spropening[x] = viewportHeight;
   
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

static void R_DrawSpritesStride(const int start, int *fuzzpos)
{
    int i;
    unsigned short spropening[SCREENWIDTH];
#ifdef MARS
    const int stride = 2;
#else
    const int stride = 1;
    start = 0;
#endif

    for (i = start; i < sortedcount; i += stride)
    {
        vissprite_t* ds, *ds2;
        boolean overlap = false;

        ds = vissprites + (sortedsprites[i] & 0x7f);
#ifdef MARS
        ds2 = i+1 < sortedcount ? vissprites + (sortedsprites[i+1] & 0x7f) : NULL;
#endif

        R_ClipVisSprite(ds, spropening);

#ifdef MARS
        Mars_R_WaitNextSprite(i);
        overlap = (ds2 != NULL) && !(ds2->x1 > ds->x2 || ds2->x2 < ds->x1);
        if (!overlap)
            Mars_R_AdvanceNextSprite();
#endif

        R_DrawVisSprite(ds, spropening, fuzzpos);

#ifdef MARS
        if (overlap)
            Mars_R_AdvanceNextSprite();
#endif
    }
}

#ifdef MARS
void Mars_Sec_R_DrawSprites(void)
{
    Mars_ClearCacheLines((intptr_t)&sortedcount & ~15, 1);
    Mars_ClearCacheLines((intptr_t)&sortedsprites & ~15, 1);
    Mars_ClearCacheLines((intptr_t)sortedsprites & ~15, (sortedcount * sizeof(*sortedsprites) + 15) / 16);

    Mars_ClearCacheLines((intptr_t)&vissprites & ~15, 1);
    Mars_ClearCacheLines((intptr_t)&vissprite_p & ~15, 1);
    Mars_ClearCacheLines((intptr_t)vissprites & ~15, ((vissprite_p - vissprites) * sizeof(vissprite_t) + 15) / 16);

    Mars_ClearCacheLines((intptr_t)&fuzzpos[1] & ~15, 1);

    R_DrawSpritesStride(1, &fuzzpos[1]);
}
#endif

static void R_DrawPSprites(void)
{
    int i;
    unsigned short spropening[SCREENWIDTH];

    // draw psprites
    while (lastsprite_p < vissprite_p)
    {
        ptrdiff_t stopx = lastsprite_p->x2 + 1;
        i = lastsprite_p->x1;

        // clear out the clipping array across the range of the psprite
        while (i < stopx)
        {
            spropening[i] = viewportHeight;
            ++i;
        }

        R_DrawVisSprite(lastsprite_p, spropening, &fuzzpos[0]);

        ++lastsprite_p;
    }
}

//
// Render all sprites
//
void R_Sprites(void)
{
   int i = 0, count;
#ifdef MARS
   __attribute__((aligned(16)))
#endif
   int sortarr[MAXVISSPRITES * 2];

   sortedcount = 0;
   sortedsprites = sortarr;

   count = lastsprite_p - vissprites;

   // draw mobj sprites
   for (i = 0; i < count; i++)
   {
       vissprite_t* ds = vissprites + i;
       if (ds->patch == NULL)
           continue;
       sortedsprites[sortedcount] = ((unsigned)ds->xscale << 7) + i;
       sortedcount++;
   }

   D_isort(sortedsprites, sortedcount);

#ifdef MARS
   Mars_R_ResetNextSprite();

   Mars_R_BeginDrawSprites();

   R_DrawSpritesStride(0, &fuzzpos[0]);

   Mars_R_EndDrawSprites();
#else
   R_DrawSpritesStride(0, &fuzzpos[0]);
#endif

   R_DrawPSprites();
}

// EOF

