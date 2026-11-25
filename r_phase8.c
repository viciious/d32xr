/*
  CALICO

  Renderer phase 8 - Sprites
*/

#include "r_local.h"
#ifdef MARS
#include "mars.h"
#endif
#include <stdlib.h>

// everything I_DrawSpriteCols and I_DrawMaskedCols need
// !!! must match sh2_sprite.s !!!
typedef struct drawsprargs_s
{
   fixed_t    xfrac;
   fixed_t    fracstep;
   fixed_t    spryscale;
   patch_t   *patch;
   inpixel_t *pixels;
   drawcol_t  dcol;
   int16_t    texturemid;
   int16_t    colormap;
   int16_t    widthmask;
   uint16_t  *maskedcol;
   fixed_t    scalefrac;
   fixed_t    scalestep;
} drawsprargs_t;

static boolean R_SegBehindPoint(viswall_t *viswall, int dx, int dy) ATTR_DATA_CACHE_ALIGN;
void R_DrawMaskedSegRange(viswall_t *seg, int x, int stopx) ATTR_DATA_CACHE_ALIGN;
void R_DrawVisSprite(vissprite_t* vis, unsigned short* spropening, int x, int stopx) ATTR_DATA_CACHE_ALIGN;
static void R_ClipVisSprite(vissprite_t *vis, unsigned short *spropening, int x1, int x2, viswall_t **walls) ATTR_DATA_CACHE_ALIGN;
static void R_DrawSortedSprites(int* sortedsprites, int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
static void R_DrawPSprites(int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
void R_Sprites(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

#if !defined(MARS) || defined(USE_C_DRAW)
void I_DrawSpriteColsC(drawsprargs_t *drawargs, int x, int stopx, unsigned short *spropening) ATTR_DATA_CACHE_ALIGN;
void I_DrawMaskedColsC(drawsprargs_t *drawargs, int x, int stopx, unsigned short *spropening) ATTR_DATA_CACHE_ALIGN;

//
// C version of I_DrawMaskedColsA in sh2_sprite.s: for each column x in
// [x, stopx) not yet drawn (maskedcol[x] != OPENMARK), mark it drawn, read
// the opening, walk the patch posts and call
// dcol(x, top, bottom, light, frac, iscale, src, 128); the scale changes
// per column
//
void I_DrawMaskedColsC(drawsprargs_t *drawargs, int x, int stopx, unsigned short *spropening)
{
   fixed_t       scalefrac  = drawargs->scalefrac;
   const fixed_t scalestep  = drawargs->scalestep;
   patch_t      *patch      = drawargs->patch;
   inpixel_t    *pixels     = drawargs->pixels;
   drawcol_t     dcol       = drawargs->dcol;
   const fixed_t texturemid = FROMHEIGHT16(drawargs->texturemid);
   const int     widthmask  = drawargs->widthmask;
   uint16_t     *maskedcol  = drawargs->maskedcol;

   do
   {
      const int masked       = maskedcol[x];
      const fixed_t spryscale = scalefrac;
      const unsigned open    = spropening[x];
      const int topclip      = open >> 8;
      const int bottomclip   = (open & 0xff) - 1;
      unsigned columnofs;
      const column_t *column;
      fixed_t sprtop, iscale, fracbase;
      int light;

      scalefrac += scalestep;

      if (masked == OPENMARK)
         continue;
      maskedcol[x] = OPENMARK;

      // column is fully covered: don't touch the patch data at all
      if (topclip > bottomclip)
         continue;

      light = masked & OPENMARK;

      // a post record has four bytes: topdelta length pixelofs*2
      // an inlined column is a single post: topdelta count pixels...
      columnofs = BIGSHORT(patch->columnofs[masked & widthmask]);
      column = (const column_t *)((byte *)patch + (columnofs & ~COLUMN_INLINE));
      if (column->topdelta == 0xff)
         continue;

      sprtop   = centerYFrac - FixedMul(texturemid, spryscale);
      iscale   = 0xffffffffu / spryscale;
      fracbase = FixedMul(-sprtop, iscale);

      do {
         int top    = column->topdelta * spryscale + sprtop;
         int bottom = ((column->length & 0x7f) + 1) * spryscale + top;
         inpixel_t *src;
         fixed_t frac;

         // an inlined column has its pixels right after the two byte
         // header; dataofs may sit at an odd address: two byte reads
         if ((columnofs & COLUMN_INLINE) != 0)
            src = (inpixel_t *)((byte *)column + 2);
         else
         {
            const byte *dataofs = (const byte *)column + offsetof(column_t, dataofs);
            src = pixels + ((dataofs[0] << 8) | dataofs[1]);
         }

         top = (top + FRACUNIT - 1) >> FRACBITS;
         bottom = (bottom - 1) >> FRACBITS;

         // posts go from top to bottom: once a post starts below the
         // opening, so do all the posts that follow it
         if (top > bottomclip)
            break;

         // clip to bottom
         if (bottom > bottomclip)
            bottom = bottomclip;

         // clip to top
         if (topclip > top)
            top = topclip;

         // nothing left of the post after clipping
         if (top <= bottom)
         {
            // The texture position is measured from the top of the column, not
            // from the post's first screen row, so that posts that touch are
            // drawn exactly like a single post
            // fracbase + top * iscale is FixedMul((top << FRACBITS) - sprtop, iscale)
            frac = fracbase + top * iscale - (column->topdelta << FRACBITS);
            if (frac < 0)
               frac = 0;

            dcol(x, top, bottom, light, frac, iscale, src, 128);
         }

         if (column->length < 0)
            break;
         column++;
      } while (1);
   } while (++x < stopx);
}

void I_DrawSpriteColsC(drawsprargs_t *drawargs, int x, int stopx, unsigned short *spropening)
{
   fixed_t       xfrac     = drawargs->xfrac;
   const fixed_t fracstep  = drawargs->fracstep;
   const fixed_t spryscale = drawargs->spryscale;
   patch_t      *patch     = drawargs->patch;
   inpixel_t    *pixels    = drawargs->pixels;
   drawcol_t     dcol      = drawargs->dcol;
   const fixed_t sprtop    = centerYFrac - FixedMul(FROMHEIGHT16(drawargs->texturemid), spryscale);
   const int     light     = drawargs->colormap;
   const fixed_t iscale    = FixedDiv(FRACUNIT, spryscale); // CALICO_FIXME: -1 in GAS... test w/o.
   const fixed_t fracbase  = FixedMul(-sprtop, iscale);
   unsigned short *opening = spropening + x;

   do
   {
      const unsigned open    = *opening++;
      const int topclip      = open >> 8;
      const int bottomclip   = (open & 0xff) - 1;
      const int colnum       = xfrac >> FRACBITS;
      unsigned columnofs;
      const column_t *column;

      xfrac += fracstep;

      // column is fully covered: don't touch the patch data at all
      if (topclip > bottomclip)
         continue;

      // a post record has four bytes: topdelta length pixelofs*2
      // an inlined column is a single post: topdelta count pixels...
      columnofs = BIGSHORT(patch->columnofs[colnum]);
      column = (const column_t *)((byte *)patch + (columnofs & ~COLUMN_INLINE));
      if (column->topdelta == 0xff)
         continue;

      do {
         int top    = column->topdelta * spryscale + sprtop;
         int bottom = ((column->length & 0x7f) + 1) * spryscale + top;
         inpixel_t *src;
         fixed_t frac;

         // an inlined column has its pixels right after the two byte
         // header; dataofs may sit at an odd address: two byte reads
         if ((columnofs & COLUMN_INLINE) != 0)
            src = (inpixel_t *)((byte *)column + 2);
         else
         {
            const byte *dataofs = (const byte *)column + offsetof(column_t, dataofs);
            src = pixels + ((dataofs[0] << 8) | dataofs[1]);
         }

         top = (top + FRACUNIT - 1) >> FRACBITS;
         bottom = (bottom - 1) >> FRACBITS;

         // posts go from top to bottom: once a post starts below the
         // opening, so do all the posts that follow it
         if (top > bottomclip)
            break;

         // clip to bottom
         if (bottom > bottomclip)
            bottom = bottomclip;

         // clip to top
         if (topclip > top)
            top = topclip;

         // nothing left of the post after clipping
         if (top <= bottom)
         {
            // The texture position is measured from the top of the column, not
            // from the post's first screen row, so that posts that touch are
            // drawn exactly like a single post
            // fracbase + top * iscale is FixedMul((top << FRACBITS) - sprtop, iscale)
            frac = fracbase + top * iscale - (column->topdelta << FRACBITS);
            if (frac < 0)
               frac = 0;

            // CALICO: invoke column drawer
            dcol(x, top, bottom, light, frac, iscale, src, 128);
         }

         if (column->length < 0)
            break;
         column++;
      } while (1);
   } while (++x < stopx);
}

#endif

void R_DrawMaskedSegRange(viswall_t *seg, int x, int stopx)
{
   drawsprargs_t drawargs;
   texture_t *texture;

   if (x > stopx)
      return;

   texture = &textures[seg->m_texturenum];
   if (texture->lumpnum < firstsprite || texture->lumpnum >= firstsprite + numsprites)
      return;

   drawargs.patch      = W_POINTLUMPNUM(texture->lumpnum);
   drawargs.pixels     = /*W_POINTLUMPNUM(texture->lumpnum+1)*/texture->data[0];
   drawargs.dcol       = drawcol;
   drawargs.texturemid = seg->m_texturemid;
   drawargs.widthmask  = texture->width - 1;
   drawargs.maskedcol  = seg->clipbounds + (seg->stop - seg->start + 1);
   drawargs.scalestep  = seg->scalestep;
   drawargs.scalefrac  = seg->scalefrac + (x - seg->start) * seg->scalestep;

   I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);

   I_DrawMaskedCols(&drawargs, x, stopx + 1, seg->clipbounds);
}

//
// Draw columns x..stopx (inclusive) of the sprite; the caller clips the
// range to its part of the screen
//
void R_DrawVisSprite(vissprite_t *vis, unsigned short *spropening, int x, int stopx)
{
   drawsprargs_t drawargs;

   if (x > stopx)
      return;

   drawargs.xfrac      = vis->startfrac + (x - vis->x1) * vis->xiscale;
   drawargs.fracstep   = vis->xiscale;

   drawargs.patch      = W_POINTLUMPNUM(vis->patchnum);
#ifdef MARS
   drawargs.pixels     = R_CheckSpritePixels(vis->patchnum);
#else
   drawargs.pixels     = vis->pixels;
#endif
   drawargs.spryscale  = vis->yscale;
   drawargs.dcol       = vis->colormap < 0 ? drawfuzzcol : drawcol;
   drawargs.texturemid = vis->texturemid;
   drawargs.colormap   = vis->colormap < 0 ? -vis->colormap : vis->colormap;

   I_SetThreadLocalVar(DOOMTLS_COLORMAP, vis->colormap2 ? dc_colormaps2 : dc_colormaps);

   I_DrawSpriteCols(&drawargs, x, stopx + 1, spropening);
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
// Clip columns x1..x2 (inclusive) of a sprite to the openings created by
// walls; the caller clips the range to its part of the screen
//
static void R_ClipVisSprite(vissprite_t *vis, unsigned short *spropening, int x1, int x2, viswall_t **walls)
{
   int     x;          // r15
   fixed_t scalefrac; // FP+3
   int     r1;         // FP+7
   int     r2;         // r18
   unsigned silhouette; // FP+4
   int16_t *sil;     // FP+6
   int16_t *opening;
   int top;        // r19
   int bottom;     // r20
   unsigned short openmark = OPENMARK;
   unsigned vph = (uint16_t)viewportHeight;
   viswall_t *ds;      // r17

   // keeps vph in one register (GCC otherwise keeps a sign-extended copy
   // for (int8_t)vph and spills it)
   __asm ("" : "+r" (vph));
   scalefrac = vis->yscale;  

   if (x1 > x2)
       return;

   {
      unsigned short *p = spropening + x2 + 1;
      x = x2 - x1 + 1;
      unsigned n = (x + 3) >> 2;
#ifdef MARS
      unsigned dispatch = ((4 - (x & 3)) & 3) * 2;
      __asm volatile (
         "braf %[dispatch]\n\t"
         "nop\n\t"
         "0:\n\t"
         "mov.w %[value],@-%[p]\n\t"
         "mov.w %[value],@-%[p]\n\t"
         "mov.w %[value],@-%[p]\n\t"
         "dt %[n]\n\t"
         "bf/s 0b\n\t"
         "mov.w %[value],@-%[p]\n\t"
         : [p] "+&r" (p), [n] "+&r" (n)
         : [value] "r" (vph), [dispatch] "r" (dispatch)
         : "t", "memory"
      );
#else
      switch (x & 3)
      {
         case 0: do { *--p = vph;
         case 3:      *--p = vph;
         case 2:      *--p = vph;
         case 1:      *--p = vph;
         } while (--n > 0);
      }
#endif
   }

#ifdef MARS
   __asm ("mov.l @%0+,%1" : "+r" (walls), "=r" (ds) : "m" (*walls));
#else
   ds = *walls++;
#endif
   do
   {
      if(ds->start > x2 || ds->stop < x1)                          // does not intersect
         goto nextwall;

      r1 = ds->start < x1 ? x1 : ds->start;
      r2 = ds->stop  > x2 ? x2 : ds->stop;

      if((ds->scalefrac < scalefrac && ds->scale2 < scalefrac) ||
         ((ds->scalefrac <= scalefrac || ds->scale2 <= scalefrac) && R_SegBehindPoint(ds, vis->gx, vis->gy))) {
         if (ds->actionbits & AC_MIDTEXTURE)
            R_DrawMaskedSegRange(ds, r1, r2);
         goto nextwall;
      }

      sil = (int16_t *)ds->clipbounds + r1;
      opening = (int16_t *)spropening + r1;
      x = r2 - r1 + 1;

      silhouette = (ds->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL));
      silhouette /= AC_TOPSIL;
      if(silhouette == 0)
         goto nextwall;

      if(silhouette < 3)
      {
         int8_t *popn = (int8_t *)opening;
         int8_t *psil = (int8_t *)sil;
         int mark = 0;

         if (silhouette == 2)
         {
            mark = (int8_t)vph;
            popn++, psil++;
         }

         unsigned n = (x + 3) >> 2;
         switch (x & 3)
         {
            case 0: do { if(*popn == mark) *popn = *psil; popn += 2, psil += 2;
            case 3:      if(*popn == mark) *popn = *psil; popn += 2, psil += 2;
            case 2:      if(*popn == mark) *popn = *psil; popn += 2, psil += 2;
            case 1:      if(*popn == mark) *popn = *psil; popn += 2, psil += 2;
            } while (--n > 0);
         }
      }
      else if (silhouette == 3)
      {
         int clip = 0;
         int ivph = vph, omark = openmark;
         do
         {
            clip = *sil++;
            top    = *opening;
            __asm volatile ("" : "+r" (clip), "+r" (opening), "+r" (sil));
            bottom = top & 0xff;
            top = top & omark;
            if(bottom == ivph)
               bottom = clip & 0xff;
            if(top == 0)
               top = clip & omark;
            *opening++ = top | bottom;
         } while (--x);
      }
      else
      {
         opening += x;
         unsigned n = (x + 3) >> 2;
#ifdef MARS
         unsigned dispatch = ((4 - (x & 3)) & 3) * 2;
         __asm volatile (
            "braf %[dispatch]\n\t"
            "nop\n\t"
            "0:\n\t"
            "mov.w %[value],@-%[p]\n\t"
            "mov.w %[value],@-%[p]\n\t"
            "mov.w %[value],@-%[p]\n\t"
            "dt %[n]\n\t"
            "bf/s 0b\n\t"
            "mov.w %[value],@-%[p]\n\t"
            : [p] "+&r" (opening), [n] "+&r" (n)
            : [value] "r" (openmark), [dispatch] "r" (dispatch)
            : "t", "memory"
         );
#else
         switch (x & 3)
         {
            case 0: do { *--opening = openmark;
            case 3:      *--opening = openmark;
            case 2:      *--opening = openmark;
            case 1:      *--opening = openmark;
            } while (--n > 0);
         }
#endif
      }
nextwall:
#ifdef MARS
      __asm ("mov.l @%0+,%1" : "+r" (walls), "=r" (ds) : "m" (*walls));
#else
      ds = *walls++;
#endif
   } while (ds != NULL);
}

static void R_DrawSortedSprites(int* sortedsprites, int sprscreenhalf)
{
   int i;
   int x1 = 0, x2 = viewportWidth - 1;
   uint16_t spropening[SCREENWIDTH];
   int count = sortedsprites[0];
   viswall_t *walls[MAXWALLCMDS+1], **pwalls;
   viswall_t *ds;

#ifdef MARS
   if (sprscreenhalf > 0)
      x2 = sprscreenhalf - 1;
   else if (sprscreenhalf < 0)
      x1 = -sprscreenhalf;
#endif 

   // compile the list of walls that clip sprites for this side part of the screen
   pwalls = walls;

   ds = vd->lastwallcmd;
   if (ds == vd->viswalls)
       return;
   do
   {
      --ds;

      if(ds->start > x2 || ds->stop < x1 ||                             // does not intersect
         !(ds->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL | AC_MIDTEXTURE)))  // does not clip sprites
         continue;

      *pwalls++ = ds;
   } while (ds != vd->viswalls);

   if (pwalls == walls)
      return;
   *pwalls = NULL;

   sortedsprites++;
   for (i = 0; i < count; i++)
   {
      vissprite_t* ds;
      int sx1, sx2;

      ds = (vissprite_t *)(vd->vissprites + (sortedsprites[i] & 0xff));

      // this CPU's part of the sprite
      sx1 = ds->x1 < x1 ? x1 : ds->x1;
      sx2 = ds->x2 > x2 ? x2 : ds->x2;

      R_ClipVisSprite(ds, spropening, sx1, sx2, walls);
      R_DrawVisSprite(ds, spropening, sx1, sx2);
   }

   // draw masked segments
   pwalls = walls;
#ifdef MARS
   __asm ("mov.l @%0+,%1" : "+r" (pwalls), "=r" (ds) : "m" (*pwalls));
#else
   ds = *pwalls++;
#endif
   do
   {
      int r1, r2;

      r1 = ds->start < x1 ? x1 : ds->start;
      r2 = ds->stop  > x2 ? x2 : ds->stop;

      if (ds->actionbits & AC_MIDTEXTURE)
         R_DrawMaskedSegRange(ds, r1, r2);

#ifdef MARS
      __asm ("mov.l @%0+,%1" : "+r" (pwalls), "=r" (ds) : "m" (*pwalls));
#else
      ds = *pwalls++;
#endif
   } while (ds != NULL);
}

static void R_DrawPSprites(int sprscreenhalf)
{
    unsigned i;
    unsigned short spropening[SCREENWIDTH];
    viswall_t *spr;
    unsigned vph = viewportHeight;
    int sx1 = 0, sx2 = viewportWidth - 1;   // this CPU's part of the screen

#ifdef MARS
    if (sprscreenhalf > 0)
        sx2 = sprscreenhalf - 1;
    else if (sprscreenhalf < 0)
        sx1 = -sprscreenhalf;
#endif

    // draw psprites
    for (spr = vd->vispsprites; spr < vd->vispsprite_p; spr++)
    {
        vissprite_t *vis = (vissprite_t *)spr;
        int x1 = vis->x1, x2 = vis->x2;

        // clear out the clipping array across the range of the psprite
        {
            unsigned short *p = spropening + x2 + 1;
            i = x2 - x1 + 1;
            do {
                *--p = vph;
            } while (--i);
        }

        R_DrawVisSprite(vis, spropening, x1 < sx1 ? sx1 : x1, x2 > sx2 ? sx2 : x2);
    }
}

#ifdef MARS

void Mars_Sec_R_DrawSprites()
{  
    Mars_ClearCacheLine(&vd->vissprites); // this will also take care of vispsprite_p, vissprite_p & sprscreenhalf

    // mobj sprites, sorted by either CPU
    R_DrawSortedSprites(vd->gsortedsprites, -vd->sprscreenhalf);

    R_DrawPSprites(-vd->sprscreenhalf);
}

#endif

//
// Render all sprites
//
void R_Sprites(void)
{
#ifdef MARS
   Mars_R_BeginDrawSprites();
#endif

#ifdef MARS
   Mars_ClearCacheLine(&vd->vissprites); // this will also take care of vispsprite_p, vissprite_p & sprscreenhalf
#endif

   R_DrawSortedSprites(vd->gsortedsprites, vd->sprscreenhalf);

   R_DrawPSprites(vd->sprscreenhalf);

#ifdef MARS
   Mars_R_EndDrawSprites();
#endif
}

// EOF
