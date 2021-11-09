/*
  CALICO

  Renderer phase 4 - Late prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

static byte* lastopening;

static void R_FinishWall(viswall_t* wc) ATTR_DATA_CACHE_ALIGN;
static void R_FinishSprite(vissprite_t* vis) ATTR_DATA_CACHE_ALIGN;
static void R_FinishPSprite(vissprite_t* vis) ATTR_DATA_CACHE_ALIGN;
boolean R_LatePrep(void) ATTR_DATA_CACHE_ALIGN;

//
// Check if texture is loaded; return if so, flag for cache if not
//
#ifdef MARS
#define R_CheckPixels(lumpnum) (void *)((intptr_t)(W_POINTLUMPNUM(lumpnum)))
#else
static boolean cacheneeded;

static void *R_CheckPixels(int lumpnum)
{
    void *lumpdata = lumpcache[lumpnum];
   
   if(lumpdata)
   {
      // touch this graphic resource with the current frame number so that it 
      // will not be immediately purged again during the same frame
      memblock_t *memblock = (memblock_t *)((byte *)lumpdata - sizeof(memblock_t));
      memblock->lockframe = framecount;
   }
   else
      cacheneeded = true; // phase 5 will need to be executed to cache graphics
   
   return lumpdata;
}
#endif

//
// Late prep for viswalls
//
static void R_FinishWall(viswall_t* wc)
{
    unsigned int fw_actionbits = wc->actionbits;
    int rw_x = wc->start;
    int rw_stopx = wc->stop + 1;
    int width = rw_stopx - rw_x + 1;
    texture_t* fw_texture;

    if (fw_actionbits & AC_BOTTOMSIL)
    {
        wc->bottomsil = (byte*)lastopening - rw_x;
        lastopening += width;
    }

    if (fw_actionbits & AC_TOPSIL)
    {
        wc->topsil = (byte*)lastopening - rw_x;
        lastopening += width;
    }

    // has top or middle texture?
    if (fw_actionbits & AC_TOPTEXTURE)
    {
        fw_texture = &textures[wc->t_texturenum];
#ifdef MARS
        if (fw_texture->data == NULL)
#endif
            fw_texture->data = R_CheckPixels(fw_texture->lumpnum);
        R_TestTexCacheCandidate(&r_texcache, fw_texture - textures);
    }

    // has bottom texture?
    if (fw_actionbits & AC_BOTTOMTEXTURE)
    {
        fw_texture = &textures[wc->b_texturenum];
#ifdef MARS
        if (fw_texture->data == NULL)
#endif
            fw_texture->data = R_CheckPixels(fw_texture->lumpnum);
        R_TestTexCacheCandidate(&r_texcache, fw_texture - textures);
    }

    int floorpicnum = wc->floorpicnum;
    int ceilingpicnum = wc->ceilingpicnum;

#ifdef MARS
    if (flatpixels[floorpicnum] == NULL)
#endif
        flatpixels[floorpicnum] = R_CheckPixels(firstflat + floorpicnum);

    // get floor texture
    R_TestTexCacheCandidate(&r_texcache, numtextures + floorpicnum);

    // is there sky at this wall?
    if (ceilingpicnum == -1)
    {
        // cache skytexture if needed
        skytexturep->data = R_CheckPixels(skytexturep->lumpnum);
    }
    else
    {
        // normal ceilingpic
#ifdef MARS
        if (flatpixels[ceilingpicnum] == NULL)
#endif
            flatpixels[ceilingpicnum] = R_CheckPixels(firstflat + ceilingpicnum);
        R_TestTexCacheCandidate(&r_texcache, numtextures + ceilingpicnum);
    }
}

//
// Late prep for vissprites
//
static void R_FinishSprite(vissprite_t *vis)
{
   int      lump;
   byte    *patch;
   fixed_t  tx, xscale;
   fixed_t  gzt;
   int      x1, x2;

   // get column headers
   lump  = (int)vis->patch;
   patch = wadfileptr + BIGLONG(lumpinfo[lump].filepos); // CALICO: requires endianness correction
   vis->patch = (patch_t *)patch;
  
   // column pixel data is in the next lump
   vis->pixels = R_CheckPixels(lump + 1);

   tx = vis->x1;
   xscale = vis->xscale;

   // calculate edges of the shape
   tx -= ((fixed_t)BIGSHORT(vis->patch->leftoffset)) << FRACBITS;
   FixedMul2(x1, tx, xscale);
   x1  = (centerXFrac + x1) / FRACUNIT;

   // off the right side?
   if(x1 > viewportWidth)
   {
      vis->patch = NULL;
      return;
   }

   tx += ((fixed_t)BIGSHORT(vis->patch->width) << FRACBITS);
   FixedMul2(x2, tx, xscale);
   x2  = ((centerXFrac + x2) / FRACUNIT) - 1;

   // off the left side
   if(x2 < 0)
   {
      vis->patch = NULL;
      return;
   }

   // store information in vissprite
   gzt = vis->gz + ((fixed_t)BIGSHORT(vis->patch->topoffset) << FRACBITS);
   vis->texturemid = gzt - vd.viewz;
   vis->x1 = x1 < 0 ? 0 : x1;
   vis->x2 = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   
   if(vis->xiscale < 0)
      vis->startfrac = ((fixed_t)BIGSHORT(vis->patch->width) << FRACBITS) - 1;
   else
      vis->startfrac = 0;

   if(x1 < 0)
      vis->startfrac += vis->xiscale * -x1;
}

//
// Late prep for player psprites
//
static void R_FinishPSprite(vissprite_t *vis)
{
   fixed_t  topoffset;
   int      x1, x2;
   int      lump;
   byte    *patch;

   // get column headers
   lump  = (int)vis->patch;
   patch = wadfileptr + BIGLONG(lumpinfo[lump].filepos); // CALICO: requires endianness correction throughout
   vis->patch = (patch_t *)patch;

   // column pixel data is in the next lump
   vis->pixels = R_CheckPixels(lump + 1);

   topoffset = (fixed_t)BIGSHORT(vis->patch->topoffset) << FRACBITS;
   vis->texturemid = BASEYCENTER*FRACUNIT - (vis->texturemid - topoffset);

   x1 = vis->x1 - BIGSHORT(vis->patch->leftoffset);

   // off the right side
   if(x1 > viewportWidth)
      return;

   x2 = x1 + BIGSHORT(vis->patch->width) - 1;

   // off the left side
   if(x2 < 0)
      return;

   // store information in vissprite
   vis->x1 = x1 < 0 ? 0 : x1;
   vis->x2 = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   vis->xscale = FRACUNIT;
   vis->yscale = FRACUNIT;
   vis->yiscale = FRACUNIT;
   vis->xiscale = FRACUNIT;
   vis->startfrac = 0;
   if (x1 < 0)
    vis->startfrac = FRACUNIT * -x1;
}

//
// Start late prep rendering stage
//
boolean R_LatePrep(void)
{
   viswall_t   *wall;
   vissprite_t *spr;
   
#ifndef MARS
   cacheneeded = false;
#endif

   lastopening = (byte *)openings;

   for (wall = viswalls; wall < lastwallcmd; wall++)
       R_FinishWall(wall);

   // finish actor sprites   
   for(spr = vissprites; spr < lastsprite_p; spr++)
      R_FinishSprite(spr);
   
   // finish player psprites
   for(; spr < vissprite_p; spr++)
      R_FinishPSprite(spr);
 
#ifdef MARS
   return true;
#else
   return cacheneeded;
#endif
}

// EOF

