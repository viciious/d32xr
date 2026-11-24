/*
  CALICO

  Renderer phase 8 - Sprites
*/

#include "r_local.h"
#ifdef MARS
#include "mars.h"
#endif
#include <stdlib.h>

static boolean R_SegBehindPoint(viswall_t *viswall, int dx, int dy) ATTR_DATA_CACHE_ALIGN;
void R_DrawMaskedSegRange(viswall_t *seg, int x, int stopx) ATTR_DATA_CACHE_ALIGN;
void R_DrawVisSprite(vissprite_t* vis, unsigned short* spropening, int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
void R_ClipVisSprite(vissprite_t *vis, unsigned short *spropening, int sprscreenhalf, viswall_t **walls) ATTR_DATA_CACHE_ALIGN;
static void R_DrawSortedSprites(int* sortedsprites, int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
static void R_DrawPSprites(int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
void R_Sprites(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

void R_DrawMaskedSegRange(viswall_t *seg, int x, int stopx)
{
   patch_t *patch;
   fixed_t  spryscale, scalefrac, fracstep;
   uint16_t *spropening, *maskedcol;
#ifdef MARS
	inpixel_t 	*pixels;
#else
	pixel_t		*pixels;		/* data patch header references */
#endif
   texture_t  *texture;
   fixed_t     texturemid;
   int widthmask;

   if (x > stopx)
      return;

   texture   = &textures[seg->m_texturenum];
   patch     = W_POINTLUMPNUM(texture->lumpnum);
   pixels    = /*W_POINTLUMPNUM(texture->lumpnum+1)*/texture->data[0];

   if (texture->lumpnum < firstsprite || texture->lumpnum >= firstsprite + numsprites)
      return;

   spropening = seg->clipbounds;
   maskedcol  = seg->clipbounds + (seg->stop - seg->start + 1);

   widthmask = texture->width - 1;
   fracstep  = seg->scalestep;
   scalefrac = seg->scalefrac + (x - seg->start) * fracstep;
   texturemid= FROMHEIGHT16(seg->m_texturemid);

   I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);

   do {
      int light          = maskedcol[x];
      int colnum         = maskedcol[x] & widthmask;
      fixed_t scale      = scalefrac;
#ifdef MARS
      int32_t t, divunit;
#endif

      spryscale = scalefrac;
      scalefrac += fracstep;  

      if (light == OPENMARK)
         continue;

      light &= OPENMARK;
      maskedcol[x] = OPENMARK;

      int topclip     = (spropening[x] >> 8);
      int bottomclip  = (spropening[x] & 0xff) - 1;
      if (topclip > bottomclip)
         continue; // column is fully covered

#ifdef MARS
      __asm volatile (
         "mov #-128, %1\n\t"
         "add %1, %1 /* %1 is now 0xFFFFFF00 */ \n\t"
         "mov.l %2, @(0, %1) /* set 32-bit divisor */ \n\t"
         "mov #0, %0\n\t"
         "mov.l %0, @(16, %1) /* set high bits of the 64-bit dividend */ \n\t"
         "mov #-1, %0\n\t"
         "mov.l %0, @(20, %1) /* set low  bits of the 64-bit dividend, start divide */\n\t"
         : "=&r" (t), "=&r"(divunit) : "r" (scale));
#endif

      byte *src;
      column_t *column;
      unsigned columnofs = BIGSHORT(patch->columnofs[colnum]);
      byte *columnptr = ((byte *)patch + (columnofs & ~COLUMN_INLINE));
      fixed_t sprtop, iscale, fracbase;

      if (*(int8_t *)columnptr == -1)
         continue;

      sprtop = FixedMul(texturemid, spryscale);
      sprtop = centerYFrac - sprtop;

#ifdef MARS
      __asm volatile (
         "mov #-128, %0\n\t"
         "add %0, %0 /* %0 is now 0xFFFFFF00 */ \n\t"
         "mov.l @(20, %0), %0 /* get 32-bit quotient */ \n\t"
         : "=&r" (iscale) : "r"(sprtop), "r"(topclip), "r"(bottomclip));
#else
      iscale = 0xffffffffu / scale;
#endif

      fracbase = FixedMul(-sprtop, iscale);

      src = columnptr + 2;
      column = (column_t *)columnptr;
      if ((columnofs & COLUMN_INLINE) != 0)
         columnptr = NULL;

      // column loop
      // a post record has up to four bytes: topdelta length pixelofs*2
      // an inlined column is a single post: topdelta count pixels...
      // (bit 7 of 'count' is set for an inline post, like a last post)
      do {
         int top    = column->topdelta * spryscale + sprtop;
         int bottom = ((column->length & 0x7f) + 1) * spryscale + top;
         fixed_t frac;
         int temp = FRACUNIT-1;

         if (columnptr != NULL)
         {
            byte *dataofsofs = columnptr + offsetof(column_t, dataofs);
            int dataofs = BIGSHORT((dataofsofs[0] << 8) | dataofsofs[1]);
            src = pixels + dataofs;
            columnptr += sizeof(column_t);
         }

#ifdef MARS
         __asm volatile (
            "mov #-1, %0\n\t"
            "extu.w %0, %0\n\t"
            : "=&r" (temp));
#endif
         top += temp;
         top >>= FRACBITS;
         bottom -= 1;
         bottom >>= FRACBITS;

         // posts go from top to bottom: once a post starts below the
         // opening, so do all the posts that follow it
         if (top > bottomclip)
            break;

         // clip to bottom
         if(bottom > bottomclip)
            bottom = bottomclip;

         // clip to top
         if(topclip > top)
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

            drawcol(x, top, bottom, light, frac, iscale, src, 128);
         }

         if (column->length < 0)
            break;
         column = (column_t *)columnptr;
      } while (1);
   } while (++x <= stopx);
}

void R_DrawVisSprite(vissprite_t *vis, unsigned short *spropening, int sprscreenhalf)
{
   patch_t *patch;
   fixed_t  iscale, xfrac, spryscale, sprtop, fracstep, fracbase;
   int light, x, stopx;
   drawcol_t dcol;
#ifdef MARS
	inpixel_t 	*pixels;
#else
	pixel_t		*pixels;		/* data patch header references */
#endif

   patch     = W_POINTLUMPNUM(vis->patchnum);
#ifdef MARS
   pixels    = R_CheckSpritePixels(vis->patchnum);
   __asm volatile("" : "+r"(pixels)); /* materialize: avoid base+offset re-add per post */
#else
   pixels    = vis->pixels;
#endif
   iscale    = FixedDiv(FRACUNIT, vis->yscale); // CALICO_FIXME: -1 in GAS... test w/o.
   xfrac     = vis->startfrac;
   spryscale = vis->yscale;
   dcol      = vis->colormap < 0 ? drawfuzzcol : drawcol;

   sprtop = FixedMul(FROMHEIGHT16(vis->texturemid), spryscale);
   sprtop = centerYFrac - sprtop;
   fracbase = FixedMul(-sprtop, iscale);

   // blitter iinc
   light    = vis->colormap < 0 ? -vis->colormap : vis->colormap;
   x        = vis->x1;
   stopx    = vis->x2 + 1;
   fracstep = vis->xiscale;

   I_SetThreadLocalVar(DOOMTLS_COLORMAP, vis->colormap2 ? dc_colormaps2 : dc_colormaps);

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

   if (x >= stopx)
      return;

   do
   {
      byte *src;
      column_t *column;
      int colnum       = xfrac>>FRACBITS;
      int topclip      = (spropening[x] >> 8);
      int bottomclip   = (spropening[x] & 0xff) - 1;

      xfrac += fracstep;

      // column is fully covered: don't touch the patch data at all
      if (topclip > bottomclip)
         continue;

      unsigned columnofs = BIGSHORT(patch->columnofs[colnum]);
      byte *columnptr  = ((byte *)patch + (columnofs & ~COLUMN_INLINE));

      // column loop
      // a post record has up to four bytes: topdelta length pixelofs*2
      // an inlined column is a single post: topdelta count pixels...
      if (*(int8_t *)columnptr == -1)
         continue;

      column = (column_t *)columnptr;

      do {
         int top    = column->topdelta * spryscale + sprtop;
         int bottom = ((column->length & 0x7f) + 1) * spryscale + top;
         fixed_t frac;
         int temp = FRACUNIT-1;

         // an inlined column is a single post with its pixels right after
         // the two byte header; src is not needed after the call
         if ((columnofs & COLUMN_INLINE) != 0)
            src = (byte *)column + 2;
         else
         {
#ifdef MARS
            // may sit at an odd address: two byte reads; asm keeps GCC from
            // juggling r0 through a stack slot
            int dataofs;
            __asm ("mov.b @(2,%1),r0\n\t"
                   "extu.b r0,%0\n\t"
                   "mov.b @(3,%1),r0\n\t"
                   "shll8 %0\n\t"
                   "extu.b r0,r0\n\t"
                   "or r0,%0"
                   : "=&r" (dataofs) : "r" (column), "m" (*column) : "r0");
#else
            byte *dataofsofs = (byte *)column + offsetof(column_t, dataofs);
            int dataofs = BIGSHORT((dataofsofs[0] << 8) | dataofsofs[1]);
#endif
            src = pixels + dataofs;
         }

#ifdef MARS
         __asm volatile (
            "mov #-1, %0\n\t"
            "extu.w %0, %0\n\t"
            : "=&r" (temp));
#endif
         top += temp;
         top >>= FRACBITS;
         bottom -= 1;
         bottom >>= FRACBITS;

         // posts go from top to bottom: once a post starts below the
         // opening, so do all the posts that follow it
         if (top > bottomclip)
            break;

         // clip to bottom
         if(bottom > bottomclip)
            bottom = bottomclip;

         // clip to top
         if(topclip > top)
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
   } while(++x < stopx);
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

// GCC turns a plain *walls++ into add/mov/add #-64/mov.l @(60,rn)
static inline viswall_t *R_NextWall(viswall_t ***walls)
{
#ifdef MARS
   viswall_t *ds;
   __asm ("mov.l @%0+,%1" : "+r" (*walls), "=r" (ds) : "m" (**walls));
   return ds;
#else
   return *(*walls)++;
#endif
}

//
// Clip a sprite to the openings created by walls
//
void R_ClipVisSprite(vissprite_t *vis, unsigned short *spropening, int sprscreenhalf, viswall_t **walls)
{
   int     x;          // r15
   int     x1;         // FP+5
   int     x2;         // r22
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
      if (x1 < -sprscreenhalf)
         x1 = -sprscreenhalf;
   }
#endif

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

   ds = R_NextWall(&walls);
   do
   {
      if(ds->start > x2 || ds->stop < x1)                          // does not intersect
         continue;

      r1 = ds->start < x1 ? x1 : ds->start;
      r2 = ds->stop  > x2 ? x2 : ds->stop;

      if((ds->scalefrac < scalefrac && ds->scale2 < scalefrac) ||
         ((ds->scalefrac <= scalefrac || ds->scale2 <= scalefrac) && R_SegBehindPoint(ds, vis->gx, vis->gy))) {
         if (ds->actionbits & AC_MIDTEXTURE)
            R_DrawMaskedSegRange(ds, r1, r2);
         continue;
      }

      sil = (int16_t *)ds->clipbounds + r1;
      opening = (int16_t *)spropening + r1;
      x = r2 - r1 + 1;

      silhouette = (ds->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL));
      silhouette /= AC_TOPSIL;
      if(silhouette == 0)
         continue;

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
   } while ((ds = R_NextWall(&walls)) != NULL);
}

static void R_DrawSortedSprites(int* sortedsprites, int sprscreenhalf)
{
   int i;
   int x1, x2;
   uint16_t spropening[SCREENWIDTH];
   int count = sortedsprites[0];
   viswall_t *walls[MAXWALLCMDS+1], **pwalls;
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

      ds = (vissprite_t *)(vd->vissprites + (sortedsprites[i] & 0xff));

      R_ClipVisSprite(ds, spropening, sprscreenhalf, walls);
      R_DrawVisSprite(ds, spropening, sprscreenhalf);
   }

   // draw masked segments
   pwalls = walls;
   do
   {
      int r1, r2;

      ds = *pwalls++;
      r1 = ds->start < x1 ? x1 : ds->start;
      r2 = ds->stop  > x2 ? x2 : ds->stop;

      if (ds->actionbits & AC_MIDTEXTURE)
         R_DrawMaskedSegRange(ds, r1, r2);
   } while (*pwalls != NULL);
}

static void R_DrawPSprites(int sprscreenhalf)
{
    unsigned i;
    unsigned short spropening[SCREENWIDTH];
    viswall_t *spr;
    unsigned vph = viewportHeight;

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

        R_DrawVisSprite(vis, spropening, sprscreenhalf);
    }
}

#ifdef MARS
void Mars_Sec_R_DrawSprites(int sprscreenhalf)
{  
    Mars_ClearCacheLine(&vd->vissprites); // this will also take care of vispsprite_p, vissprite_p & sprscreenhalf

    // mobj sprites, sorted by either CPU
    R_DrawSortedSprites(vd->gsortedsprites, -sprscreenhalf);

    R_DrawPSprites(-sprscreenhalf);
}

#endif

//
// Render all sprites
//
void R_Sprites(void)
{
   int half;

#ifdef MARS
   // the secondary CPU may still be sorting the sprites in R_SpritePrep
   Mars_R_SecWait();
   Mars_ClearCacheLine(&vd->vissprites); // this will also take care of vispsprite_p, vissprite_p & sprscreenhalf
#endif

   half = vd->sprscreenhalf;

#ifdef MARS
   Mars_R_BeginDrawSprites(half);
#endif

   R_DrawSortedSprites(vd->gsortedsprites, half);

   R_DrawPSprites(half);

#ifdef MARS
   Mars_R_EndDrawSprites();
#endif
}

// EOF
