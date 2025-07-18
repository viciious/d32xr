/*
  CALICO

  Renderer phase 3 - Sprite prep
*/

#include "doomdef.h"
#include "r_local.h"

static void R_PrepMobj(mobj_t* thing) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
static void R_PrepPSprite(pspdef_t* psp) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
void R_SpritePrep(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

//
// Project vissprite for potentially visible actor
//
static void R_PrepMobj(mobj_t *thing)
{
   fixed_t tr_x, tr_y;
   fixed_t gxt, gyt, gzt;
   fixed_t tx, tz, x1, x2;
   fixed_t xscale;
   fixed_t texmid;
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   VINT         *sprlump;
   int          sprite;
   angle_t      ang;
   unsigned int rot;
   boolean      flip;
   int          lump, framenum;
   patch_t      *patch;
   vissprite_t  *vis;
   const state_t* state;
   boolean      fullbright;

   // transform origin relative to viewpoint
   tr_x = thing->x - vd->viewx;
   tr_y = thing->y - vd->viewy;

   gxt = FixedMul(tr_x, vd->viewcos);
   gyt = FixedMul(tr_y, vd->viewsin);
   gyt = -gyt;
   tz  = gxt - gyt;

   // thing is behind view plane?
   if(tz < MINZ)
      return;

   gxt = FixedMul(tr_x, vd->viewsin);
   gxt = -gxt;
   gyt = FixedMul(tr_y, vd->viewcos);
   tx  = -(gyt + gxt);

   // too far off the side?
   if(tx > (tz << 2) || tx < -(tz<<2))
      return;

   // check sprite for validity
   state  = &states[thing->state];
   sprite = state->sprite;
   if(/*sprite < 0 || */sprite >= NUMSPRITES)
      return;

   sprdef = &sprites[sprite];
   fullbright = (state->frame & FF_FULLBRIGHT) != 0;
   framenum = (state->frame & FF_FRAMEMASK);

   // check frame for validity
   if (framenum >= sprdef->numframes)
       return;

   framenum = sprdef->firstframe + framenum;
   if (framenum < 0 || framenum >= numspriteframes)
      return;

   sprframe = &spriteframes[framenum];
   sprlump = &spritelumps[sprframe->lump];

   lump = sprlump[0];
   if(!(lump & SL_SINGLESIDED) && !(thing->flags & MF_STATIC))
   {
      // select proper rotation depending on player's view point
      ang  = R_PointToAngle(vd->viewx, vd->viewy, thing->x, thing->y);
      rot  = (ang - thing->angle + (unsigned int)(ANG45 / 2)*9) >> 29;
      lump = sprlump[rot];
   }
   else
   {
      // sprite has a single view for all rotations
   }

   flip = false;
   if (lump & SL_FLIPPED)
      flip = true;

   lump &= SL_LUMPMASK;
   if (lump < firstsprite || lump >= firstsprite + numsprites)
      return;

   patch = W_POINTLUMPNUM(lump);
   xscale = FixedDiv(PROJECTION, tz);
   gzt = thing->z - vd->viewz;

   // calculate edges of the shape
   if (flip)
      tx -= ((fixed_t)BIGSHORT(patch->width)-(fixed_t)BIGSHORT(patch->leftoffset)) << FRACBITS;
   else
      tx -= ((fixed_t)BIGSHORT(patch->leftoffset)) << FRACBITS;

   x1 = FixedMul(tx, xscale);
   x1 = (centerXFrac + x1) >> FRACBITS;

   // off the right side?
   if (x1 > viewportWidth)
       return;

   tx += ((fixed_t)BIGSHORT(patch->width) << FRACBITS);
   x2 = FixedMul(tx, xscale);
   x2 = ((centerXFrac + x2) >> FRACBITS) - 1;

   // off the left side
   if (x2 < 0)
       return;

   // killough 4/9/98: clip things which are out of view due to height
   tz = FixedMul(gzt, xscale);
   if (tz > centerYFrac)
       return;

   texmid = gzt + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);
   tz = FixedMul(texmid, xscale);
   if (tz < viewportHeight - centerYFrac)
       return;

   // get a new vissprite
   if(vd->vissprite_p >= vd->vissprites + MAXVISSPRITES - NUMPSPRITES)
      return; // too many visible sprites already, leave room for psprites

   vis = (vissprite_t *)vd->vissprite_p;
   vd->vissprite_p++;

   vis->patchnum = lump;
#ifndef MARS
   vis->pixels   = R_CheckPixels(lump + 1);
#endif
   vis->x1       = x1 < 0 ? 0 : x1;
   vis->x2       = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   vis->gx       = thing->x >> FRACBITS;
   vis->gy       = thing->y >> FRACBITS;
   vis->xscale   = xscale;
   vis->yscale   = FixedMul(xscale, stretch);
   vis->texturemid = texmid;

   if(flip)
   {
      vis->xiscale = -FixedDiv(FRACUNIT, xscale);
      vis->startfrac = ((fixed_t)BIGSHORT(patch->width) << FRACBITS) - 1;
   }
   else
   {
      vis->xiscale = FixedDiv(FRACUNIT, xscale);
      vis->startfrac = 0;
   }

   if (vis->xiscale < 0)
       vis->startfrac = ((fixed_t)BIGSHORT(patch->width) << FRACBITS) - 1;
   if (x1 < 0)
       vis->startfrac += vis->xiscale * -x1;

   if (thing->flags & MF_SHADOW)
   {
      vis->colormap = -vd->fuzzcolormap;
   }
   else if (vd->fixedcolormap)
   {
       vis->colormap = vd->fixedcolormap;
   }
   else
   {
      if (fullbright)
           vis->colormap = 255;
       else
           vis->colormap = SSEC_SECTOR(thing->subsector)->lightlevel;
       vis->colormap = HWLIGHT(vis->colormap);
   }

   if (thing->flags & MF_KNIGHT_CMAP)
      vis->colormaps = dc_colormaps2;
   else
      vis->colormaps = dc_colormaps;
}

//
// Project player weapon sprite
//
static void R_PrepPSprite(pspdef_t *psp)
{
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   VINT         *sprlump;
   int          lump, framenum;
   patch_t      *patch;
   vissprite_t  *vis;
   fixed_t      center, xscale;
   fixed_t      topoffset, texmid;
   fixed_t      tx, x1, x2;
   const state_t* state;

   state = &states[psp->state];
   sprdef = &sprites[state->sprite];
   framenum = sprdef->firstframe + (state->frame & FF_FRAMEMASK);
   if (framenum < 0 || framenum >= numspriteframes)
      return;

   sprframe = &spriteframes[framenum];
   if (sprframe->lump < 0)
      return;
   sprlump  = &spritelumps[sprframe->lump];
   lump     = sprlump[0] & SL_LUMPMASK;
   patch    = W_POINTLUMPNUM(lump);

   if (lump < firstsprite || lump >= firstsprite + numsprites)
      return;

   xscale = weaponXScale;
   center = centerXFrac - (lowres ? 320 - viewportWidth : 160) * weaponXScale;

   tx = psp->sx + center;
   topoffset = (((fixed_t)BIGSHORT(patch->topoffset) - weaponYpos) << FRACBITS) - psp->sy;

   texmid = BASEYCENTER * FRACUNIT + topoffset;

   // calculate edges of the shape
   tx = tx - (((fixed_t)BIGSHORT(patch->leftoffset)) << FRACBITS);
   x1 = FixedMul(tx, xscale);

   tx = ((fixed_t)BIGSHORT(patch->width) << FRACBITS);
   x2 = FixedMul(tx, xscale);
   x2 += x1;

   x1 >>= FRACBITS;
   x2 >>= FRACBITS;
   x2 -= 1;

   // off the right side?
   if (x1 > viewportWidth)
       return;

   // off the left side
   if (x2 < 0)
       return;

   // store information in vissprite
   if(vd->vissprite_p == vd->vissprites + MAXVISSPRITES)
      return; // out of vissprites

   vis = (vissprite_t *)vd->vissprite_p;
   vd->vissprite_p++;

   vis->patchnum = lump;
#ifndef MARS
   vis->pixels = R_CheckPixels(lump + 1);
#endif
   vis->x1 = x1 < 0 ? 0 : x1;
   vis->x2 = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   vis->xscale = xscale;
   vis->yscale = FRACUNIT;
   vis->xiscale = FixedDiv(FRACUNIT, xscale);
   vis->texturemid = texmid;
   vis->startfrac = 0;
   if (x1 < 0)
       vis->startfrac = vis->xiscale * -x1;

   if (vd->shadow)
   {
      vis->colormap = -vd->fuzzcolormap;
   }
   else if (vd->fixedcolormap)
   {
       vis->colormap = vd->fixedcolormap;
   }
   else
   {
       if (state->frame & FF_FULLBRIGHT)
           vis->colormap = 255;
       else
           vis->colormap = vd->lightlevel;
       vis->colormap = HWLIGHT(vis->colormap);
   }
   vis->colormaps = dc_colormaps;
}

//
// Process actors in all visible subsectors
//
void R_SpritePrep(void)
{
   sector_t **pse = vd->vissectors;
   pspdef_t     *psp;
   int i;

   while(pse < vd->lastvissector)
   {
      sector_t    *se = *pse;
      mobj_t *thing = SPTR_TO_LPTR(se->thinglist);

      while(thing) // walk sector thing list
      {
        R_PrepMobj(thing);
        thing = SPTR_TO_LPTR(thing->snext);
      }
      ++pse;
   }

   // remember end of actor vissprites
   vd->lastsprite_p = vd->vissprite_p;

   // draw player weapon sprites
   for(i = 0, psp = vd->psprites; i < NUMPSPRITES; i++, psp++)
   {
      if(psp->state)
         R_PrepPSprite(psp);
   }
}

// EOF

