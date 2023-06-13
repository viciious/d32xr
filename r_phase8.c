/*
  CALICO

  Renderer phase 8 - Sprites
*/

#include "r_local.h"
#ifdef MARS
#include "mars.h"
#endif
#include <stdlib.h>

static int fuzzpos[2];

static boolean R_SegBehindPoint(viswall_t *viswall, int dx, int dy) ATTR_DATA_CACHE_ALIGN;
void R_DrawVisSprite(vissprite_t* vis, unsigned short* spropening, int *fuzzpos, int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
void R_ClipVisSprite(vissprite_t *vis, unsigned short *spropening, int sprscreenhalf, int16_t *walls) ATTR_DATA_CACHE_ALIGN;
static void R_DrawSortedSprites(int *fuzzpos, int* sortedsprites, int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
static void R_DrawPSprites(int *fuzzpos, int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
void R_Sprites(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

void R_DrawVisSprite(vissprite_t *vis, unsigned short *spropening, int *fuzzpos, int sprscreenhalf)
{
   patch_t *patch;
   fixed_t  iscale, xfrac, spryscale, sprtop, fracstep;
   int light, x, stopx;
   drawcol_t dcol;
#ifdef MARS
	inpixel_t 	*pixels;
#else
	pixel_t		*pixels;		/* data patch header references */
#endif

   patch     = W_POINTLUMPNUM(vis->patchnum);
#ifdef MARS
   pixels    = W_POINTLUMPNUM(vis->patchnum+1);
#else
   pixels    = vis->pixels;
#endif
   iscale    = FixedDiv(FRACUNIT, vis->yscale); // CALICO_FIXME: -1 in GAS... test w/o.
   xfrac     = vis->startfrac;
   spryscale = vis->yscale;
   dcol      = vis->colormap < 0 ? drawfuzzcol : drawcol;

   sprtop = FixedMul(vis->texturemid, spryscale);
   sprtop = centerYFrac - sprtop;
   spryscale = (unsigned)spryscale >> 8;

   // blitter iinc
   light    = vis->colormap < 0 ? -vis->colormap : vis->colormap;
   x        = vis->x1;
   stopx    = vis->x2 + 1;
   fracstep = vis->xiscale;

#ifdef MARS
   if (sprscreenhalf > 0)
   {
      if (stopx > sprscreenhalf)
         stopx = sprscreenhalf;
   }
   else if (sprscreenhalf < 0)
   {
      sprscreenhalf =- sprscreenhalf;
      if (x < sprscreenhalf)
      {
         xfrac += (sprscreenhalf - x) * fracstep;
         x = sprscreenhalf;
      }
   }
#endif

   for(; x < stopx; x++, xfrac += fracstep)
   {
      column_t *column = (column_t *)((byte *)patch + BIGSHORT(patch->columnofs[xfrac>>FRACBITS]));
      int topclip      = (spropening[x] >> 8) - 1;
      int bottomclip   = (spropening[x] & 0xff) - 1 - 1;

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
         dcol(x, top, bottom, light, frac, iscale, pixels + BIGSHORT(column->dataofs), 128, fuzzpos);
      }
   }
}

//
// Compare the vissprite to a viswall. Similar to R_PointOnSegSide, but less accurate.
//
static boolean R_SegBehindPoint(viswall_t *viswall, int dx, int dy)
{
   fixed_t x1, y1, sdx, sdy;
   mapvertex_t *v1 = &viswall->v1, *v2 = &viswall->v2;

   x1  = v1->x;
   y1  = v1->y;
   sdx = v2->x;
   sdy = v2->y;

   sdx -= x1;
   sdy -= y1;

   dx  -= x1;
   dy  -= y1;

   dx  *= sdy;
   sdx *=  dy;

   return (sdx < dx);
}

//
// Clip a sprite to the openings created by walls
//
void R_ClipVisSprite(vissprite_t *vis, unsigned short *spropening, int sprscreenhalf, int16_t *walls)
{
   int     x;          // r15
   int     x1;         // FP+5
   int     x2;         // r22
   unsigned scalefrac; // FP+3
   int     r1;         // FP+7
   int     r2;         // r18
   unsigned silhouette; // FP+4
   byte   *topsil;     // FP+6
   byte   *bottomsil;  // r21
   unsigned opening;    // r16
   unsigned short top;        // r19
   unsigned short bottom;     // r20
   unsigned short openmark;
   viswall_t *ds;      // r17
   unsigned short vhplus1 = viewportHeight + 1;

   x1  = vis->x1;
   x2  = vis->x2;
   scalefrac = vis->yscale;  

#ifdef MARS
   if (sprscreenhalf > 0)
   {
      if (x2 >= sprscreenhalf)
         x2 = sprscreenhalf - 1;
   }
   else if (sprscreenhalf < 0)
   {
      sprscreenhalf = -sprscreenhalf;
      if (x1 < sprscreenhalf)
         x1 = sprscreenhalf;
   }

   if (x1 > x2)
       return;
#endif

   for(x = x1; x <= x2; x++)
      spropening[x] = vhplus1 | (1<<8);
   
   do
   {
      int width;

      ds = viswalls + *walls++;

      silhouette = (ds->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL));
      
      if(ds->start > x2 || ds->stop < x1 ||                         // does not intersect
         (ds->scalefrac < scalefrac && ds->scale2 < scalefrac))     // is completely behind
         continue;

      if(ds->scalefrac <= scalefrac || ds->scale2 <= scalefrac)
      {
         if(R_SegBehindPoint(ds, vis->gx, vis->gy))
            continue;
      }

      r1 = ds->start < x1 ? x1 : ds->start;
      r2 = ds->stop  > x2 ? x2 : ds->stop;
      width = ds->stop - ds->start + 1;

      silhouette /= AC_TOPSIL;
      if(silhouette == 4)
      {
#ifdef MARS
         // force GCC into keeping constants in registers as it 
         // is stupid enough to reload them on each loop iteration
         __asm volatile("mov %1,%0\n\t" : "=&r" (openmark) : "r"(OPENMARK));
#endif
         for (x = r1;  x <= r2; x++)
            spropening[x] = openmark;
         continue;
      }

      topsil = ds->sil;
      bottomsil = ds->sil + (silhouette & 1 ? width : 0);

      if(silhouette == 1)
      {
         for(x = r1; x <= r2; x++)
         {
            opening = spropening[x];
            if((opening>>8) == 1)
               spropening[x] = (topsil[x] << 8) | (opening & 0xff);
         }
      }
      else if(silhouette == 2)
      {
         for(x = r1; x <= r2; x++)
         {
            opening = spropening[x];
            if((opening & 0xff) == vhplus1)
               spropening[x] = (((volatile uint16_t)opening >> 8) << 8) | bottomsil[x];
         }
      }
      else
      {
         for(x = r1; x <= r2; x++)
         {
            top    = spropening[x];
            bottom = top & 0xff;
            top >>= 8;
            if(bottom == vhplus1)
               bottom = bottomsil[x];
            if(top == 1)
               top = topsil[x];
            spropening[x] = (top << 8) | bottom;
         }
      }
   } while (*walls != -1);
}

static void R_DrawSortedSprites(int *fuzzpos, int* sortedsprites, int sprscreenhalf)
{
   int i;
   int x1, x2;
   uint16_t spropening[SCREENWIDTH];
   int count = sortedsprites[0];
   int16_t walls[MAXWALLCMDS+1], *pwalls;
   viswall_t *ds;

#ifdef MARS
   if (sprscreenhalf > 0)
   {
      x1 = 0;
      x2 = sprscreenhalf - 1;
   } else
#else
   sprscreenhalf = 0;
#endif 
   {
      x1 = -sprscreenhalf;
      x2 = viewportWidth - 1;
   }

   // compile the list of walls that clip sprites for this side part of the screen
   pwalls = walls;

   ds = lastwallcmd;
   if (ds == viswalls)
       return;
   do
   {
      --ds;

      if(ds->start > x2 || ds->stop < x1 ||                             // does not intersect
         !(ds->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL)))  // does not clip sprites
         continue;

      *pwalls++ = ds - viswalls;
   } while (ds != viswalls);

   if (pwalls == walls)
      return;
   *pwalls = -1;

   sortedsprites++;
   for (i = 0; i < count; i++)
   {
      vissprite_t* ds;

      ds = (vissprite_t *)(vissprites + (sortedsprites[i] & 0x7f));

      R_ClipVisSprite(ds, spropening, sprscreenhalf, walls);
      R_DrawVisSprite(ds, spropening, fuzzpos, sprscreenhalf);
   }
}

static void R_DrawPSprites(int *fuzzpos, int sprscreenhalf)
{
    unsigned i;
    unsigned short spropening[SCREENWIDTH];
    viswall_t *spr;
    unsigned vhplus1 = viewportHeight + 1;

    // draw psprites
    for (spr = lastsprite_p; spr < vissprite_p; spr++)
    {
        vissprite_t *vis = (vissprite_t *)spr;
        unsigned stopx = vis->x2 + 1;
        i = vis->x1;

        if (vis->patchnum < 0)
            continue;

        // clear out the clipping array across the range of the psprite
        while (i < stopx)
        {
            spropening[i] = vhplus1 | (1<<8);
            ++i;
        }

        R_DrawVisSprite(vis, spropening, fuzzpos, sprscreenhalf);
    }
}

#ifdef MARS
void Mars_Sec_R_DrawSprites(int sprscreenhalf, int *sortedsprites)
{
    Mars_ClearCacheLine(&vissprites);
    Mars_ClearCacheLine(&lastsprite_p);
    Mars_ClearCacheLine(&vissprite_p);

    // mobj sprites
    //Mars_ClearCacheLines(sortedsprites, ((lastsprite_p - vissprites + 1) * sizeof(*sortedsprites) + 31) / 16);

    R_DrawSortedSprites(&fuzzpos[1], sortedsprites, -sprscreenhalf);

    R_DrawPSprites(&fuzzpos[1], -sprscreenhalf);
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
   viswall_t *spr;
   int sortedsprites[1+MAXVISSPRITES];
   int *gsortedsprites = sortedsprites;
   viswall_t *wc;
   vertex_t *verts;

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
       vissprite_t* ds = (vissprite_t *)(vissprites + i);
       if (ds->patchnum < 0)
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
       sortedsprites[1+sortedcount++] = (xscale << 7) + i;
   }

   // add the gun midpoint
   for (spr = lastsprite_p; spr < vissprite_p; spr++) {
        vissprite_t *pspr = (vissprite_t *)spr;
        unsigned xscale;
        unsigned pixcount = pspr->x2 + 1 - pspr->x1;

        xscale = pspr->xscale;
        if (pspr->patchnum < 0 || pspr->x2 < pspr->x1)
            continue;

        midcount += xscale;
        half += (pspr->x1 + (pixcount >> 1)) * xscale;
   }

   // average the mid point
   if (midcount > 0)
   {
      half /= midcount;
      if (!half || half > viewportWidth)
         half = viewportWidth / 2;
   }

   // draw mobj sprites
   sortedsprites[0] = sortedcount;
   D_isort(sortedsprites+1, sortedcount);

#ifdef MARS
   // bank switching
   verts = W_GetLumpData(gamemaplump+ML_VERTEXES);
#else
   verts = vertexes;
#endif

   for (wc = viswalls; wc < lastwallcmd; wc++)
   {
      if (wc->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL))
      {
         volatile int v1 = wc->seg->v1, v2 = wc->seg->v2;
         wc->v1.x = verts[v1].x>>16, wc->v1.y = verts[v1].y>>16;
         wc->v2.x = verts[v2].x>>16, wc->v2.y = verts[v2].y>>16;
      }
   }

#ifdef MARS
   // re-use the openings array in VRAM
	gsortedsprites = (int*)(((intptr_t)segclip + 3) & ~3);
   for (i = 0; i < sortedcount+1; i++)
      gsortedsprites[i] = sortedsprites[i];
#endif

#ifdef MARS
   Mars_R_BeginDrawSprites(half, gsortedsprites);

	Mars_ClearCacheLines(openings, ((lastopening - openings) * sizeof(*openings) + 31) / 16);
#endif

   R_DrawSortedSprites(&fuzzpos[0], sortedsprites, half);

   R_DrawPSprites(0, half);

#ifdef MARS
   Mars_R_EndDrawSprites();
#endif
}

// EOF

