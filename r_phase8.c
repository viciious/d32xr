/*
  CALICO

  Renderer phase 8 - Sprites
*/

#include "r_local.h"
#ifdef MARS
#include "mars.h"
#endif

static boolean R_SegBehindPoint(viswall_t *viswall, int dx, int dy) ATTR_DATA_CACHE_ALIGN;
void R_DrawFOFSegRange(viswall_t *seg, int x, int stopx) __attribute__((noinline));
void R_DrawMaskedSegRange(viswall_t *seg, int x, int stopx) ATTR_DATA_CACHE_ALIGN;
void R_DrawVisSprite(vissprite_t* vis, unsigned short* spropening, int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
void R_ClipVisSprite(vissprite_t *vis, unsigned short *spropening, int sprscreenhalf, int16_t *walls) ATTR_DATA_CACHE_ALIGN;
static void R_DrawSortedSprites(int* sortedsprites, int sprscreenhalf) ATTR_DATA_CACHE_ALIGN;
void R_Sprites(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

void R_DrawFOFSegRange(viswall_t *seg, int x, int stopx)
{
   uint8_t *patch;
   fixed_t  spryscale, scalefrac, fracstep;
   uint16_t *spropening, *maskedcol;
   texture_t  *texture;
   int widthmask;

   if (x > stopx)
      return;

   texture   = &textures[seg->fof_texturenum];

//   if (texture->lumpnum < firstsprite || texture->lumpnum >= firstsprite + numsprites)
//      return;

   if (x <= seg->start && seg->start <= stopx)
      seg->start = stopx + 1;
   if (stopx >= seg->stop && seg->stop <= x)
      seg->stop = x - 1;

   patch     = texture->data[0];

   spropening = seg->clipbounds;
   maskedcol  = seg->clipbounds + (seg->realstop - seg->realstart + 1);

   widthmask = texture->width - 1;
   fracstep  = seg->scalestep;
   scalefrac = seg->scalefrac + (x - seg->realstart) * fracstep;

   I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);

   VINT thickness = (sectors[seg->fofSector].ceilingheight- sectors[seg->fofSector].floorheight) >> (FRACBITS+1);

   for(; x <= stopx; x++)
   {
      int light          = maskedcol[x] & OPENMARK;
      int colnum         = maskedcol[x] & widthmask;

#ifdef WALLDRAW2X
      spryscale = scalefrac << 1;
#else
      spryscale = scalefrac;
#endif
      scalefrac += fracstep;  

      if (light == OPENMARK)
         continue;
//      maskedcol[x] = OPENMARK;

#ifdef MARS
        volatile int32_t t;
        __asm volatile (
           "mov #-128, r0\n\t"
           "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
           "mov #0, %0\n\t"
           "mov.l %0, @(16, r0) /* set high bits of the 64-bit dividend */ \n\t"
           "mov.l %1, @(0, r0) /* set 32-bit divisor */ \n\t"
           "mov #-1, %0\n\t"
           "mov.l %0, @(20, r0) /* set low  bits of the 64-bit dividend, start divide */\n\t"
           : "=&r" (t) : "r" (scalefrac) : "r0");
#else
      fixed_t scale = scalefrac;
#endif

      int topclip     = (spropening[x] >> 8);
      int bottomclip  = (spropening[x] & 0xff) - 1;
      fixed_t sprtop, iscale;

      sprtop = FixedMul(seg->fof_texturemid, spryscale);
      sprtop = centerYFrac - sprtop;

#ifdef MARS
#ifdef WALLDRAW2X
      __asm volatile (
         "mov #-128, r0\n\t"
         "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
         "mov.l @(20, r0), %0 /* get 32-bit quotient */ \n\t"
         "shar %0\n\t"
         : "=r" (iscale) : : "r0");
#else
      __asm volatile (
         "mov #-128, r0\n\t"
         "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
         "mov.l @(20, r0), %0 /* get 32-bit quotient */ \n\t"
         : "=r" (iscale) : : "r0");
#endif
#else
      iscale = 0xffffffffu / scalefrac;
#endif

      // column loop
      // a post record has four bytes: topdelta length pixelofs*2
      {
         int top    = sprtop;
         int bottom = thickness * spryscale + top;
         int dataofs = colnum * texture->height;
         int count;
         fixed_t frac;
         int temp = FRACUNIT-1;

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

         drawcol(x, top, bottom, light, frac, iscale, patch + dataofs, texture->height);
      }
   }
}

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
   int widthmask;

   if (x > stopx)
      return;

   texture   = &textures[seg->m_texturenum];
   if (texture->lumpnum < firstsprite || texture->lumpnum >= firstsprite + numsprites)
      return;

   if (x <= seg->start && seg->start <= stopx)
      seg->start = stopx + 1;
   if (stopx >= seg->stop && seg->stop <= x)
      seg->stop = x - 1;

   patch     = W_POINTLUMPNUM(texture->lumpnum);
   pixels    = /*W_POINTLUMPNUM(texture->lumpnum+1)*/texture->data[0];

   spropening = seg->clipbounds;
   maskedcol  = seg->clipbounds + (seg->realstop - seg->realstart + 1);

   widthmask = texture->width - 1;
   fracstep  = seg->scalestep;
   scalefrac = seg->scalefrac + (x - seg->realstart) * fracstep;

   I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);

   for(; x <= stopx; x++)
   {
      int light          = maskedcol[x] & OPENMARK;
      int colnum         = maskedcol[x] & widthmask;

#ifdef WALLDRAW2X
      spryscale = scalefrac << 1;
#else
      spryscale = scalefrac;
#endif
      scalefrac += fracstep;  

      if (light == OPENMARK)
         continue;
      maskedcol[x] = OPENMARK;

#ifdef MARS
        volatile int32_t t;
        __asm volatile (
           "mov #-128, r0\n\t"
           "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
           "mov #0, %0\n\t"
           "mov.l %0, @(16, r0) /* set high bits of the 64-bit dividend */ \n\t"
           "mov.l %1, @(0, r0) /* set 32-bit divisor */ \n\t"
           "mov #-1, %0\n\t"
           "mov.l %0, @(20, r0) /* set low  bits of the 64-bit dividend, start divide */\n\t"
           : "=&r" (t) : "r" (scalefrac) : "r0");
#else
      fixed_t scale = scalefrac;
#endif

      int topclip     = (spropening[x] >> 8);
      int bottomclip  = (spropening[x] & 0xff) - 1;
      byte *columnptr = ((byte *)patch + BIGSHORT(patch->columnofs[colnum]));
      fixed_t sprtop, iscale;

      sprtop = FixedMul(seg->m_texturemid, spryscale);
      sprtop = centerYFrac - sprtop;

#ifdef MARS
#ifdef WALLDRAW2X
      __asm volatile (
         "mov #-128, r0\n\t"
         "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
         "mov.l @(20, r0), %0 /* get 32-bit quotient */ \n\t"
         "shar %0\n\t"
         : "=r" (iscale) : : "r0");
#else
      __asm volatile (
         "mov #-128, r0\n\t"
         "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
         "mov.l @(20, r0), %0 /* get 32-bit quotient */ \n\t"
         : "=r" (iscale) : : "r0");
#endif
#else
      iscale = 0xffffffffu / scalefrac;
#endif

      // column loop
      // a post record has four bytes: topdelta length pixelofs*2
      for(; *columnptr != 0xff; columnptr += sizeof(column_t))
      {
         column_t *column = (column_t *)columnptr;
         int top    = column->topdelta * spryscale + sprtop;
         int bottom = column->length   * spryscale + top;
         byte *dataofsofs = columnptr + offsetof(column_t, dataofs);
         int dataofs = (dataofsofs[0] << 8) | dataofsofs[1];
         int count;
         fixed_t frac;
         int temp = FRACUNIT-1;

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

         drawcol(x, top, bottom, light, frac, iscale, pixels + BIGSHORT(dataofs), 128);
      }
   }
}

void R_DrawVisSprite(vissprite_t *vis, unsigned short *spropening, int sprscreenhalf)
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
   const boolean verticalFlip = vis->patchnum & 16384;
   const boolean highres = vis->patchnum & 32768;

   patch     = W_POINTLUMPNUM(vis->patchnum & 0x1FFF);
#ifdef MARS
   pixels    = W_POINTLUMPNUM((vis->patchnum & 0x1FFF)+1);
#else
   pixels    = vis->pixels;
#endif
   iscale    = FixedDiv(FRACUNIT, vis->yscale); // CALICO_FIXME: -1 in GAS... test w/o.
   xfrac     = vis->startfrac;
   spryscale = vis->yscale;

#ifdef HIGH_DETAIL_SPRITES
   if (lowResMode && highres) {
      if (verticalFlip)
         dcol = drawcolflipped;
      else
         dcol      = drawspritecol;
   }
   else {
      dcol      = drawcol;
   }
#else
   dcol      = drawcol;
#endif

   sprtop = FixedMul(vis->texturemid, spryscale);
   sprtop = centerYFrac - sprtop;

   // blitter iinc
   light    = vis->colormap;
   x        = vis->x1;
   stopx    = vis->x2 + 1;
   fracstep = vis->xiscale;

   // NOTE: Probably not needed?
//   I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);

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

#ifdef HIGH_DETAIL_SPRITES
   if (lowResMode && highres) {
      x <<= 1;
      stopx = (stopx << 1) - 1;
      fracstep >>= 1;
   }
#endif

   if (verticalFlip)
   {
      // TODO: Properly clip sprite
      for(; x < stopx; x++, xfrac += fracstep)
      {
         byte *columnptr  = ((byte *)patch + BIGSHORT(patch->columnofs[xfrac>>FRACBITS]));

   #ifdef HIGH_DETAIL_SPRITES
         int topclip;
         int bottomclip;

         if (lowResMode && (vis->patchnum & 32768)) {
            topclip      = (spropening[x>>1] >> 8);
            bottomclip   = (spropening[x>>1] & 0xff) - 1;
         }
         else {
            topclip      = (spropening[x] >> 8);
            bottomclip   = (spropening[x] & 0xff) - 1;
         }
   #else
         int topclip      = (spropening[x] >> 8);
         int bottomclip   = (spropening[x] & 0xff) - 1;
   #endif

         // column loop
         // a post record has four bytes: topdelta length pixelofs*2
         for(; *columnptr != 0xff; columnptr += sizeof(column_t))
         {
            column_t *column = (column_t *)columnptr;
            int top    = (vis->patchheight - column->topdelta) * spryscale + sprtop;
            int bottom = column->length   * spryscale + top;
            byte *dataofsofs = columnptr + offsetof(column_t, dataofs);
            int dataofs = (dataofsofs[0] << 8) | dataofsofs[1];
            int count;
            fixed_t frac;

            top += (FRACUNIT - 1);
            top >>= FRACBITS;
            bottom -= 1;
            bottom >>= FRACBITS;

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
            dcol(x, top, bottom, light, frac, iscale, pixels + BIGSHORT(dataofs), 128);
         }
      }
   }
   else
   {
      for(; x < stopx; x++, xfrac += fracstep)
      {
         byte *columnptr  = ((byte *)patch + BIGSHORT(patch->columnofs[xfrac>>FRACBITS]));

   #ifdef HIGH_DETAIL_SPRITES
         int topclip;
         int bottomclip;

         if (lowResMode && (vis->patchnum & 32768)) {
            topclip      = (spropening[x>>1] >> 8);
            bottomclip   = (spropening[x>>1] & 0xff) - 1;
         }
         else {
            topclip      = (spropening[x] >> 8);
            bottomclip   = (spropening[x] & 0xff) - 1;
         }
   #else
         int topclip      = (spropening[x] >> 8);
         int bottomclip   = (spropening[x] & 0xff) - 1;
   #endif

         // column loop
         // a post record has four bytes: topdelta length pixelofs*2
         for(; *columnptr != 0xff; columnptr += sizeof(column_t))
         {
            column_t *column = (column_t *)columnptr;
            int top    = column->topdelta * spryscale + sprtop;
            int bottom = column->length   * spryscale + top;
            byte *dataofsofs = columnptr + offsetof(column_t, dataofs);
            int dataofs = (dataofsofs[0] << 8) | dataofsofs[1];
            int count;
            fixed_t frac;
            int temp = FRACUNIT-1;

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
            dcol(x, top, bottom, light, frac, iscale, pixels + BIGSHORT(dataofs), 128);
         }
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
   fixed_t scalefrac; // FP+3
   int     r1;         // FP+7
   int     r2;         // r18
   unsigned silhouette; // FP+4
   uint16_t *sil;     // FP+6
   uint16_t *opening;
   int top;        // r19
   int bottom;     // r20
   unsigned short openmark = OPENMARK;
   viswall_t *ds;      // r17

   x1  = vis->x1;
   x2  = vis->x2;
   scalefrac = vis->yscale;  

#ifdef MARS
   if (sprscreenhalf > 0)
   {
      if (x1 < 0)
         x1 = 0;
      if (x2 >= sprscreenhalf)
         x2 = sprscreenhalf - 1;
   }
   else if (sprscreenhalf < 0)
   {
      if (x1 < -sprscreenhalf)
         x1 = -sprscreenhalf;
      if (x2 >= viewportWidth)
         x2 = viewportWidth-1;
   }

   if (x1 > x2)
       return;
#endif

   for(x = x1; x <= x2; x++)
      spropening[x] = viewportHeight;

   do
   {
      ds = vd.viswalls + *walls++;
      if(ds->start > x2 || ds->stop < x1)                          // does not intersect
         continue;

      r1 = ds->start < x1 ? x1 : ds->start;
      r2 = ds->stop  > x2 ? x2 : ds->stop;
      if (r1 > r2)
         continue;

      if((ds->scalefrac < scalefrac && ds->scale2 < scalefrac) ||
         ((ds->scalefrac <= scalefrac || ds->scale2 <= scalefrac) && R_SegBehindPoint(ds, vis->gx, vis->gy))) {

         if (ds->actionbits & AC_FOFSIDE)
            R_DrawFOFSegRange(ds, r1, r2);

         if (ds->actionbits & AC_MIDTEXTURE)
            R_DrawMaskedSegRange(ds, r1, r2);

         continue;
      }

      sil = ds->clipbounds + r1;
      opening = spropening + r1;
      x = r2 - r1 + 1;

      silhouette = (ds->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL));
      silhouette /= AC_TOPSIL;
      if(silhouette == 0)
         continue;

      if(silhouette == 1)
      {
         int8_t *popn = (int8_t *)opening;
         int8_t *psil = (int8_t *)sil;
         do
         {
            if(*popn == 0)
               *popn = *psil;
            popn += 2, psil += 2;
         } while (--x);
      }
      else if(silhouette == 2)
      {
         int8_t *popn = (int8_t *)opening;
         int8_t *psil = (int8_t *)sil;
         int vph = (int8_t)viewportHeight;
         popn++, psil++;
         do
         {
            if(*popn == vph)
               *popn = *psil;
            popn += 2, psil += 2;
         } while (--x);
      }
      else if (silhouette == 3)
      {
         do
         {
            uint16_t clip = *sil;
            top    = *opening;
            bottom = top & 0xff;
            top = top & openmark;
            if(bottom == viewportHeight)
               bottom = clip & 0xff;
            if(top == 0)
               top = clip & openmark;
            *opening = top | bottom;
            opening++, sil++;
         } while (--x);
      }
      else
      {
         opening += x;
         do {
            --opening;
            *opening = openmark;
         } while (--x);
      }
   } while (*walls != -1);

   // killough 3/27/98:
  // Clip the sprite against deep water and/or fake ceilings.
  // killough 4/9/98: optimize by adding mh
  // killough 4/11/98: improve sprite clipping for underwater/fake ceilings

  if (vis->heightsec >= 0)  // only things in specially marked sectors
    {
      fixed_t h,mh;
      int phs = vd.viewsector->heightsec;
      // Recalculate these lost values from phase3
      const fixed_t gzt = vis->texturemid + vd.viewz;
      const fixed_t gz = gzt - (vis->patchheight << FRACBITS);

      if ((mh = sectors[vis->heightsec].ceilingheight) > gz &&
          (h = centerYFrac - FixedMul(mh-=vd.viewz, vis->yscale)) >= 0 &&
          (h >>= FRACBITS) < viewportHeight)
      {
        if (mh <= 0 || (phs >= 0 && vd.viewz > vd.viewwaterheight))
          {                          // clip bottom
            for (x=vis->x1 ; x<=vis->x2 ; x++)
            {
               const int bottomclip   = (spropening[x] & 0xff) - 1;
               if (bottomclip == viewportHeight || h < bottomclip)
                  spropening[x] = (spropening[x] & 0xff00) + h;
            }
          }
        else                        // clip top
          for (x=vis->x1 ; x<=vis->x2 ; x++)
          {
            const int topclip      = (spropening[x] >> 8);
            if (topclip == 0 || h > topclip)
               spropening[x] = (spropening[x] & 0x00ff) + (h << 8);
          }
      }
    }
  // killough 3/27/98: end special clipping for deep water / fake ceilings
}

static void R_DrawSortedSprites(int* sortedsprites, int sprscreenhalf)
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

   ds = vd.lastwallcmd;
   if (ds == vd.viswalls)
       return;
   do
   {
      --ds;

      if(ds->start > x2 || ds->stop < x1 ||                             // does not intersect
         !(ds->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL | AC_MIDTEXTURE | AC_FOFSIDE)))  // does not clip sprites
         continue;

      *pwalls++ = ds - vd.viswalls;
   } while (ds != vd.viswalls);

   if (pwalls == walls)
      return;
   *pwalls = -1;

   sortedsprites++;
   for (i = 0; i < count; i++)
   {
      vissprite_t* ds;

      ds = (vissprite_t *)(vd.vissprites + (sortedsprites[i] & 0x7f));

#ifdef HIGH_DETAIL_SPRITES
      if (ds->patchnum & 32878)
      {
         if (lowResMode && sprscreenhalf > 0) {
            sprscreenhalf += 1;

            R_ClipVisSprite(ds, spropening, sprscreenhalf, walls);
            R_DrawVisSprite(ds, spropening, sprscreenhalf);

            sprscreenhalf -= 1;
         }
         else
         {
            R_ClipVisSprite(ds, spropening, sprscreenhalf, walls);
            R_DrawVisSprite(ds, spropening, sprscreenhalf);
         }
      }
      else
#endif
      {
         R_ClipVisSprite(ds, spropening, sprscreenhalf, walls);
         R_DrawVisSprite(ds, spropening, sprscreenhalf);
      }
   }

   // draw masked segments
   pwalls = walls;
   do
   {
      int r1, r2;

      ds = vd.viswalls + *pwalls++;
      r1 = ds->start < x1 ? x1 : ds->start;
      r2 = ds->stop  > x2 ? x2 : ds->stop;

      if (ds->actionbits & AC_FOFSIDE)
         R_DrawFOFSegRange(ds, r1, r2);

      if (ds->actionbits & AC_MIDTEXTURE)
         R_DrawMaskedSegRange(ds, r1, r2);

   } while (*pwalls != -1);
}

#ifdef MARS
void Mars_Sec_R_DrawSprites(int sprscreenhalf)
{  
    Mars_ClearCacheLine(&vd.vissprites);
    Mars_ClearCacheLine(&vd.lastsprite_p);
    Mars_ClearCacheLine(&vd.vissprite_p);
    Mars_ClearCacheLine(&vd.gsortedsprites);

    // mobj sprites
    //Mars_ClearCacheLines(vd.gsortedsprites, ((lastsprite_p - vissprites + 1) * sizeof(*vd.gsortedsprites) + 31) / 16);

    R_DrawSortedSprites(vd.gsortedsprites, -sprscreenhalf);
}

#endif

//
// Render all sprites
//
void R_Sprites(void)
{
   int i = 0, count, sortedcount;
   unsigned half;
   unsigned midcount;
   int *sortedsprites = (void *)vd.vissectors;
   viswall_t *wc;
   mapvertex_t *verts;

   sortedcount = 0;
   count = vd.lastsprite_p - vd.vissprites;
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
       vissprite_t* ds = (vissprite_t *)(vd.vissprites + i);
       if ((ds->patchnum & 0x3FFF) < 0)
           continue;
       if (ds->x1 > ds->x2)
           continue;

       // average mid point
       unsigned xscale = ds->xscale;
       unsigned pixcount = ds->x2 + 1 - ds->x1;

       midcount += pixcount;
       half += (ds->x1 + (pixcount >> 1)) * pixcount;

       // composite sort key: distance + id
       sortedsprites[1+sortedcount++] = (xscale << 7) + i;
   }

   // add masked segs
   for (wc = vd.viswalls; wc < vd.lastwallcmd; wc++)
   {
      unsigned pixcount = wc->stop - wc->start + 1;
      if (wc->start > wc->stop)
         continue;
      if (!((wc->actionbits & AC_MIDTEXTURE) || (wc->actionbits & AC_FOFSIDE)))
         continue;
      midcount += pixcount;
      half += (wc->start + (pixcount >> 1)) * pixcount;
   }

   // average the mid point
   if (midcount > 0)
   {
      half /= midcount;
      if (!half || half > (unsigned)viewportWidth)
         half = viewportWidth / 2;
   }

   // draw mobj sprites
   sortedsprites[0] = sortedcount;
   D_isort(sortedsprites+1, sortedcount);

#ifdef MARS
   // bank switching
   verts = /*W_POINTLUMPNUM(gamemaplump+ML_VERTEXES)*/vertexes;
#else
   verts = vertexes;
#endif

   for (wc = vd.viswalls; wc < vd.lastwallcmd; wc++)
   {
      if (wc->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL | AC_MIDTEXTURE | AC_FOFSIDE))
      {
         volatile int v1 = wc->seg->v1, v2 = wc->seg->v2;
         wc->v1.x = verts[v1].x, wc->v1.y = verts[v1].y;
         wc->v2.x = verts[v2].x, wc->v2.y = verts[v2].y;
      }
   }

#ifdef MARS
   Mars_R_SecWait();
   for (i = 0; i < sortedcount+1; i++)
      vd.gsortedsprites[i] = sortedsprites[i];
#endif

#ifdef MARS
   Mars_R_BeginDrawSprites(half);
#endif

   R_DrawSortedSprites(sortedsprites, half);

#ifdef MARS
   Mars_R_EndDrawSprites();
#endif
}

// EOF

