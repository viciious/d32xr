/*
  CALICO

  Renderer phase 3 - Sprite prep
*/

#include "doomdef.h"
#include "r_local.h"

static void R_PrepMobj(mobj_t* thing) ATTR_DATA_CACHE_ALIGN;
static void R_PrepPSprite(pspdef_t* psp) ATTR_DATA_CACHE_ALIGN;
void R_SpritePrep(void) ATTR_DATA_CACHE_ALIGN;

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
   angle_t      ang;
   unsigned int rot;
   boolean      flip;
   int          lump;
   patch_t      *patch;
   vissprite_t  *vis;

   // transform origin relative to viewpoint
   tr_x = thing->x - vd.viewx;
   tr_y = thing->y - vd.viewy;

   FixedMul2(gxt, tr_x, vd.viewcos);
   FixedMul2(gyt, tr_y, vd.viewsin);
   gyt = -gyt;
   tz  = gxt - gyt;

   // thing is behind view plane?
   if(tz < MINZ)
      return;

   FixedMul2(gxt, tr_x, vd.viewsin);
   gxt = -gxt;
   FixedMul2(gyt, tr_y, vd.viewcos);
   tx  = -(gyt + gxt);

   // too far off the side?
   if(tx > (tz << 2) || tx < -(tz<<2))
      return;

   // check sprite for validity
   if(/*thing->sprite < 0 || */thing->sprite >= NUMSPRITES)
      return;

   sprdef = &sprites[thing->sprite];

   // check frame for validity
   if ((thing->frame & FF_FRAMEMASK) >= sprdef->numframes)
       return;

   sprframe = &spriteframes[sprdef->firstframe + (thing->frame & FF_FRAMEMASK)];

   if(sprframe->lump[1] != -1)
   {
      // select proper rotation depending on player's view point
      ang  = R_PointToAngle2(vd.viewx, vd.viewy, thing->x, thing->y);
      rot  = (ang - thing->angle + (unsigned int)(ANG45 / 2)*9) >> 29;
      lump = sprframe->lump[rot];
   }
   else
   {
      // sprite has a single view for all rotations
      lump = sprframe->lump[0];
   }

   flip = false;
   if (lump < 0)
   {
      lump = -(lump + 1);
      flip = true;
   }

   patch = W_POINTLUMPNUM(lump);
   xscale = FixedDiv(PROJECTION, tz);
   gzt = thing->z - vd.viewz;

   // calculate edges of the shape
   tx -= ((fixed_t)BIGSHORT(patch->leftoffset)) << FRACBITS;
   FixedMul2(x1, tx, xscale);
   x1 = (centerXFrac + x1) / FRACUNIT;

   // off the right side?
   if (x1 > viewportWidth)
       return;

   tx += ((fixed_t)BIGSHORT(patch->width) << FRACBITS);
   FixedMul2(x2, tx, xscale);
   x2 = ((centerXFrac + x2) / FRACUNIT) - 1;

   // off the left side
   if (x2 < 0)
       return;

   // killough 4/9/98: clip things which are out of view due to height
   FixedMul2(tz, gzt, xscale);
   if (tz > centerYFrac)
       return;

   texmid = gzt + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);
   FixedMul2(tz, texmid, xscale);
   if (FixedMul(texmid, xscale) < viewportHeight - centerYFrac)
       return;

   // get a new vissprite
   if(vissprite_p >= vissprites + MAXVISSPRITES - 4)
      return; // too many visible sprites already, leave room for psprites

   vis = vissprite_p++;
   vis->patchnum = lump;
   vis->pixels   = R_CheckPixels(lump + 1);
   vis->x1       = x1 < 0 ? 0 : x1;
   vis->x2       = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   vis->gx       = thing->x;
   vis->gy       = thing->y;
   vis->xscale   = xscale;
   vis->xiscale  = FixedDiv(FRACUNIT, xscale);
   FixedMul2(vis->yscale, xscale, stretch);
   vis->yiscale  = FixedDiv(FRACUNIT, vis->yscale); // CALICO_FIXME: -1 in GAS... test w/o.
   vis->texturemid = texmid;
   vis->startfrac = 0;

   if(flip)
      vis->xiscale = -vis->xiscale;

   if (vis->xiscale < 0)
       vis->startfrac = ((fixed_t)BIGSHORT(patch->width) << FRACBITS) - 1;
   if (x1 < 0)
       vis->startfrac += vis->xiscale * -x1;

   if (vd.fixedcolormap)
   {
       vis->colormap = vd.fixedcolormap;
   }
   else
   {
       if (thing->frame & FF_FULLBRIGHT)
           vis->colormap = 255;
       else
           vis->colormap = thing->subsector->sector->lightlevel;
       vis->colormap = HWLIGHT(vis->colormap);
   }

   vis->drawcol = (thing->flags & MF_SHADOW) ? drawfuzzcol : drawcol;
}

//
// Project player weapon sprite
//
static void R_PrepPSprite(pspdef_t *psp)
{
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   int          lump;
   patch_t      *patch;
   vissprite_t  *vis;
   fixed_t      center, xscale;
   fixed_t      topoffset, texmid;
   fixed_t      tx, x1, x2;
   const state_t* state;

   state = &states[psp->state];
   sprdef = &sprites[state->sprite];
   sprframe = &spriteframes[sprdef->firstframe + (state->frame & FF_FRAMEMASK)];
   lump     = sprframe->lump[0];
   patch    = W_POINTLUMPNUM(lump);

   xscale = weaponXScale;
   center = centerXFrac - 80 * weaponXScale;
   center /= (lowResMode ? 1 : 2);

   tx = psp->sx + center;
   topoffset = (((fixed_t)BIGSHORT(patch->topoffset) - weaponYpos) << FRACBITS) - psp->sy;

   texmid = BASEYCENTER * FRACUNIT + topoffset;

   // calculate edges of the shape
   tx = tx - (((fixed_t)BIGSHORT(patch->leftoffset)) << FRACBITS);
   FixedMul2(x1, tx, xscale);

   tx = ((fixed_t)BIGSHORT(patch->width) << FRACBITS);
   FixedMul2(x2, tx, xscale);
   x2 += x1;

   x1 /= FRACUNIT;
   x2 /= FRACUNIT;
   x2 -= 1;

   // off the right side?
   if (x1 > viewportWidth)
       return;

   // off the left side
   if (x2 < 0)
       return;

   // store information in vissprite
   if(vissprite_p == vissprites + MAXVISSPRITES)
      return; // out of vissprites

   vis = vissprite_p++;
   vis->patchnum = lump;
   vis->pixels = R_CheckPixels(lump + 1);
   vis->x1 = x1 < 0 ? 0 : x1;
   vis->x2 = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   vis->xscale = xscale;
   vis->yscale = FRACUNIT;
   vis->yiscale = FRACUNIT;
   vis->xiscale = FixedDiv(FRACUNIT, xscale);
   vis->texturemid = texmid;
   vis->startfrac = 0;
   if (x1 < 0)
       vis->startfrac = vis->xiscale * -x1;

   if (vd.fixedcolormap)
   {
       vis->colormap = vd.fixedcolormap;
   }
   else
   {
       if (state->frame & FF_FULLBRIGHT)
           vis->colormap = 255;
       else
           vis->colormap = vd.lightlevel;
       vis->colormap = HWLIGHT(vis->colormap);
   }
   vis->drawcol = drawcol;
}

//
// Process actors in all visible subsectors
//
void R_SpritePrep(void)
{
   sector_t **pse = vissectors;
   pspdef_t     *psp;
   int i;

   while(pse < lastvissector)
   {
      sector_t    *se = *pse;
      mobj_t *thing = se->thinglist;

      while(thing) // walk sector thing list
      {
        R_PrepMobj(thing);
        thing = thing->snext;
      }
      ++pse;
   }

   // remember end of actor vissprites
   lastsprite_p = vissprite_p;

   // draw player weapon sprites
   for(i = 0, psp = vd.viewplayer->psprites; i < NUMPSPRITES; i++, psp++)
   {
      if(psp->state)
         R_PrepPSprite(psp);
   }
}

// EOF

