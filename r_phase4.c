/*
  CALICO

  Renderer phase 4 - Late prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

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

static VINT CalcFlatSize(int lumplength)
{
    switch(lumplength)
    {
        case 256:
            return 16;
        case 1024:
            return 32;
        default:
            return 64;
    }
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

    uint8_t floorpicnum = wc->floorpicnum;
    uint8_t ceilingpicnum = wc->ceilingpicnum;

    if (flatpixels[floorpicnum] == NULL)
    {
        flatpixels[floorpicnum].data = R_CheckPixels(firstflat + floorpicnum);
        flatpixels[floorpicnum].size = CalcFlatSize(W_LumpLength(firstflat + floorpicnum));
    }

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
        {
            flatpixels[ceilingpicnum].data = R_CheckPixels(firstflat + ceilingpicnum);
            flatpixels[ceilingpicnum].size = CalcFlatSize(W_LumpLength(firstflat + ceilingpicnum));
        }
    }
}
#endif

//
// Start late prep rendering stage
//
boolean R_LatePrep(void)
{
#ifdef MARS
    return true;
#else
   return cacheneeded;
#endif
}

// EOF

