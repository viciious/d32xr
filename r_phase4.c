/*
  CALICO

  Renderer phase 4 - Late prep
*/

#include "doomdef.h"
#include "r_local.h"

static boolean cacheneeded;
static fixed_t hyp;
angle_t normalangle;

//
// Check if texture is loaded; return if so, flag for cache if not
//
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
// Get distance to point in 3D projection
//
static fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
   int angle;
   fixed_t dx, dy, temp;
   
   dx = D_abs(x - viewx);
   dy = D_abs(y - viewy);
   
   if(dy > dx)
   {
      temp = dx;
      dx = dy;
      dy = temp;
   }
   
   angle = (tantoangle[FixedDiv(dy, dx)>>DBITS] + ANG90) >> ANGLETOFINESHIFT;
   
   // use as cosine
   return FixedDiv(dx, finesine[angle]);
}

//
// Convert angle and distance within view frustum to texture scale factor.
//
static fixed_t R_ScaleFromGlobalAngle(fixed_t rw_distance, angle_t visangle)
{
   angle_t anglea, angleb;
   fixed_t num, den;
   int     sinea, sineb;

   visangle += ANG90;
   
   anglea = visangle - viewangle;
   sinea  = finesine[anglea >> ANGLETOFINESHIFT];
   angleb = visangle - normalangle;
   sineb  = finesine[angleb >> ANGLETOFINESHIFT];
   
   num = sineb * 22 * 8;
   den = FixedMul(rw_distance, sinea);

   return FixedDiv(num, den);
}

//
// Setup texture calculations for lines with upper and lower textures
//
static void R_SetupCalc(viswall_t *wc)
{
   fixed_t sineval, rw_offset;
   angle_t offsetangle;

   offsetangle = normalangle - wc->angle1;

   if(offsetangle > ANG180)
      offsetangle = 0 - offsetangle;

   if(offsetangle > ANG90)
      offsetangle = ANG90;

   sineval = finesine[offsetangle >> ANGLETOFINESHIFT];
   rw_offset = FixedMul(hyp, sineval);

   if(normalangle - wc->angle1 < ANG180)
      rw_offset = -rw_offset;

   wc->offset += rw_offset;
   wc->centerangle = ANG90 + viewangle - normalangle;
}

//
// Late prep for viswalls
//
static void R_FinishWallPrep(viswall_t *wc)
{
   unsigned int fw_actionbits = wc->actionbits;
   texture_t   *fw_texture;
   angle_t      distangle, offsetangle;
   seg_t       *seg = wc->seg;
   fixed_t      sineval, rw_distance;
   fixed_t      scalefrac, scale2;
   
   // has top or middle texture?
   if(fw_actionbits & AC_TOPTEXTURE)
   {
      fw_texture = wc->t_texture;
      fw_texture->data = R_CheckPixels(fw_texture->lumpnum);
   }
   
   // has bottom texture?
   if(fw_actionbits & AC_BOTTOMTEXTURE)
   {
      fw_texture = wc->b_texture;
      fw_texture->data = R_CheckPixels(fw_texture->lumpnum);
   }
   
   // get floor texture
   wc->floorpic = R_CheckPixels(firstflat + wc->floorpicnum); // CALICO: use floorpicnum field here
   
   // is there sky at this wall?
   if(wc->ceilingpicnum == -1) // CALICO: likewise for ceilingpicnum
   {
      // cache skytexture if needed
      skytexturep->data = R_CheckPixels(skytexturep->lumpnum);
   }
   else
   {
      // normal ceilingpic
      wc->ceilingpic = R_CheckPixels(firstflat + wc->ceilingpicnum);
   }
   
   // this is essentially R_StoreWallRange
   // calculate rw_distance for scale calculation
   normalangle = seg->angle + ANG90;
   offsetangle = normalangle - wc->angle1;

   if((int)offsetangle < 0)
      offsetangle = 0 - offsetangle;
   
   if(offsetangle > ANG90)
      offsetangle = ANG90;
   
   distangle = ANG90 - offsetangle;
   hyp = R_PointToDist(seg->v1->x, seg->v1->y);
   sineval = finesine[distangle >> ANGLETOFINESHIFT];
   wc->distance = rw_distance = FixedMul(hyp, sineval);
   
   scalefrac = scale2 = wc->scalefrac =
      R_ScaleFromGlobalAngle(rw_distance, viewangle + xtoviewangle[wc->start]);

   if(wc->stop > wc->start)
   {
      scale2 = R_ScaleFromGlobalAngle(rw_distance, viewangle + xtoviewangle[wc->stop]);
      wc->scalestep = (int)(scale2 - scalefrac) / (int)(wc->stop - wc->start);
   }

   wc->scale2 = scale2;

   // does line have top or bottom textures?
   if(wc->actionbits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE))
   {
      wc->actionbits |= AC_CALCTEXTURE; // set to calculate texture info
      R_SetupCalc(wc);                  // do calc setup
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
   int      x1, x2;

   // get column headers
   lump  = vis->patchnum;                                // CALICO: use patchnum to avoid type punning
   patch = wadfileptr + BIGLONG(lumpinfo[lump].filepos); // CALICO: requires endianness correction
   vis->patch = (patch_t *)patch;
  
   // column pixel data is in the next lump
   vis->pixels = R_CheckPixels(lump + 1);

   tx = vis->x1;
   xscale = vis->xscale;

   // calculate edges of the shape
   tx -= ((fixed_t)BIGSHORT(vis->patch->leftoffset)) << FRACBITS;
   x1  = (CENTERXFRAC + FixedMul(tx, xscale)) / FRACUNIT;

   // off the right side?
   if(x1 > SCREENWIDTH)
   {
      vis->patch = NULL;
      return;
   }

   tx += ((fixed_t)BIGSHORT(vis->patch->width) << FRACBITS);
   x2  = ((CENTERXFRAC + FixedMul(tx, xscale)) / FRACUNIT) - 1;

   // off the left side
   if(x2 < 0)
   {
      vis->patch = NULL;
      return;
   }

   // store information in vissprite
   vis->gzt = vis->gz + ((fixed_t)BIGSHORT(vis->patch->topoffset) << FRACBITS);
   vis->texturemid = vis->gzt - viewz;
   vis->x1 = x1 < 0 ? 0 : x1;
   vis->x2 = x2 >= SCREENWIDTH ? SCREENWIDTH - 1 : x2;
   
   if(vis->xiscale < 0)
      vis->startfrac = ((fixed_t)BIGSHORT(vis->patch->width) << FRACBITS) - 1;
   else
      vis->startfrac = 0;

   if(x1 < 0)
      vis->startfrac += vis->xiscale * (vis->x1 - x1);
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
   lump  = vis->patchnum;                                // CALICO: use patchnum to avoid type punning
   patch = wadfileptr + BIGLONG(lumpinfo[lump].filepos); // CALICO: requires endianness correction throughout
   vis->patch = (patch_t *)patch;

   // column pixel data is in the next lump
   vis->pixels = R_CheckPixels(lump + 1);

   topoffset = (fixed_t)BIGSHORT(vis->patch->topoffset) << FRACBITS;
   vis->texturemid = 100*FRACUNIT - (vis->texturemid - topoffset);

   x1 = vis->x1 - BIGSHORT(vis->patch->leftoffset);

   // off the right side
   if(x1 > 160)
      return;

   x2 = (x1 + BIGSHORT(vis->patch->width)) - 1;

   // off the left side
   if(x2 < 0)
      return;

   // store information in vissprite
   vis->x1 = x1 < 0 ? 0 : x1;
   vis->x2 = x2 >= 160 ? 160 - 1 : x2;
   vis->xscale = FRACUNIT;
   vis->yscale = FRACUNIT;
   vis->yiscale = FRACUNIT;
   vis->xiscale = FRACUNIT;
   vis->startfrac = 0;
}

//
// Start late prep rendering stage
//
boolean R_LatePrep(void)
{
   viswall_t   *wall;
   vissprite_t *spr;
   
   cacheneeded = false;   
   
   // finish viswalls
   for(wall = viswalls; wall < lastwallcmd; wall++)
      R_FinishWallPrep(wall);

   // finish actor sprites   
   for(spr = vissprites; spr < lastsprite_p; spr++)
      R_FinishSprite(spr);
   
   // finish player psprites
   for(; spr < vissprite_p; spr++)
      R_FinishPSprite(spr);
   
   return cacheneeded;
}

// EOF

