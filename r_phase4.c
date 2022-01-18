/*
  CALICO

  Renderer phase 4 - Late prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

static void R_FinishSprite(vissprite_t* vis) ATTR_DATA_CACHE_ALIGN;
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
   int      lump = (int)vis->patch;
 
   vis->patch = (patch_t *)W_POINTLUMPNUM(lump);
  
   // column pixel data is in the next lump
   vis->pixels = R_CheckPixels(lump + 1);
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

   // finish sprites
   for(spr = vissprites; spr < vissprite_p; spr++)
      R_FinishSprite(spr);
 
#ifdef MARS
   return true;
#else
   return cacheneeded;
#endif
}

// EOF

