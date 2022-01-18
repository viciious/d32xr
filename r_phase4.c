/*
  CALICO

  Renderer phase 4 - Late prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

static void R_FinishSprite(vissprite_t* vis) ATTR_DATA_CACHE_ALIGN;
static void R_FinishPSprite(vissprite_t* vis) ATTR_DATA_CACHE_ALIGN;
boolean R_LatePrep(void) ATTR_DATA_CACHE_ALIGN;

//
// Check if texture is loaded; return if so, flag for cache if not
//
#ifndef MARS
static void R_FinishWall(viswall_t* wc) ATTR_DATA_CACHE_ALIGN;

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

//
// Late prep for viswalls
//
static void R_FinishWall(viswall_t* wc)
{
    unsigned int fw_actionbits = wc->actionbits;
    texture_t* fw_texture;

    // has top or middle texture?
    if (fw_actionbits & AC_TOPTEXTURE)
    {
        fw_texture = &textures[wc->t_texturenum];
        if (fw_texture->data == NULL)
            fw_texture->data = R_CheckPixels(fw_texture->lumpnum);
    }

    // has bottom texture?
    if (fw_actionbits & AC_BOTTOMTEXTURE)
    {
        fw_texture = &textures[wc->b_texturenum];
        if (fw_texture->data == NULL)
            fw_texture->data = R_CheckPixels(fw_texture->lumpnum);
    }

    int floorpicnum = wc->floorpicnum;
    int ceilingpicnum = wc->ceilingpicnum;

    if (flatpixels[floorpicnum] == NULL)
        flatpixels[floorpicnum] = R_CheckPixels(firstflat + floorpicnum);

    // is there sky at this wall?
    if (ceilingpicnum == -1)
    {
        // cache skytexture if needed
        skytexturep->data = R_CheckPixels(skytexturep->lumpnum);
    }
    else
    {
        // normal ceilingpic
        if (flatpixels[ceilingpicnum] == NULL)
            flatpixels[ceilingpicnum] = R_CheckPixels(firstflat + ceilingpicnum);
    }
}
#endif

//
// Late prep for vissprites
//
static void R_FinishSprite(vissprite_t *vis)
{
   int      lump;
   byte    *patch;
   fixed_t  tx, xscale;
   fixed_t  gzt, texmid, tz;
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

   // killough 4/9/98: clip things which are out of view due to height
   gzt = vis->gz - vd.viewz;
   FixedMul2(tz, gzt, xscale);
   if (tz > centerYFrac)
   {
       vis->patch = NULL;
       return;
   }

   texmid = gzt + ((fixed_t)BIGSHORT(vis->patch->topoffset) << FRACBITS);
   FixedMul2(tz, texmid, xscale);
   if (FixedMul(texmid, xscale) < viewportHeight - centerYFrac)
   {
       vis->patch = NULL;
       return;
   }

   vis->texturemid = texmid;
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
   fixed_t  tx;
   fixed_t  xscale;
   fixed_t  x1, x2;
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

   xscale = vis->xscale;

   // calculate edges of the shape
   tx = -((fixed_t)BIGSHORT(vis->patch->leftoffset)) << FRACBITS;
   x1 = tx;

   tx = ((fixed_t)BIGSHORT(vis->patch->width) << FRACBITS);
   FixedMul2(x2, tx, xscale);
   x1 += (tx - x2) / 2;

   x1 += vis->x1;
   x2 += x1;

   x1 /= FRACUNIT;
   x2 /= FRACUNIT;
   x2 -= 1;

   // off the right side?
   if (x1 > viewportWidth)
   {
       vis->patch = NULL;
       return;
   }

   // off the left side
   if (x2 < 0)
   {
       vis->patch = NULL;
       return;
   }

   // store information in vissprite
   vis->x1 = x1 < 0 ? 0 : x1;
   vis->x2 = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   vis->startfrac = 0;
   if (x1 < 0)
    vis->startfrac = vis->xiscale * -x1;
}

//
// Start late prep rendering stage
//
boolean R_LatePrep(void)
{
   vissprite_t *spr;
   
#ifndef MARS
   cacheneeded = false;
#endif

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

