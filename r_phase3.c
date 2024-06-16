/*
  CALICO

  Renderer phase 3 - Sprite prep
*/

#include "doomdef.h"
#include "r_local.h"

static void R_PrepMobj(mobj_t* thing) ATTR_DATA_CACHE_ALIGN;
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
   angle_t      ang;
   unsigned int rot;
   boolean      flip;
   int          lump;
   patch_t      *patch;
   vissprite_t  *vis;

   // transform origin relative to viewpoint
   tr_x = thing->x - vd.viewx;
   tr_y = thing->y - vd.viewy;

   gxt = FixedMul(tr_x, vd.viewcos);
   gyt = FixedMul(tr_y, vd.viewsin);
   gyt = -gyt;
   tz  = gxt - gyt;

   // thing is behind view plane?
   if(tz < MINZ)
      return;

   gxt = FixedMul(tr_x, vd.viewsin);
   gxt = -gxt;
   gyt = FixedMul(tr_y, vd.viewcos);
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
   sprlump = &spritelumps[sprframe->lump];

   if(sprlump[1] != -1)
   {
      // select proper rotation depending on player's view point
      ang  = R_PointToAngle2(vd.viewx, vd.viewy, thing->x, thing->y);
      rot  = (ang - thing->angle + (unsigned int)(ANG45 / 2)*9) >> 29;
      lump = sprlump[rot];
   }
   else
   {
      // sprite has a single view for all rotations
      lump = sprlump[0];
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
   if (flip)
      tx -= ((fixed_t)BIGSHORT(patch->width)-(fixed_t)BIGSHORT(patch->leftoffset)) << FRACBITS;
   else
      tx -= ((fixed_t)BIGSHORT(patch->leftoffset)) << FRACBITS;

   x1 = FixedMul(tx, xscale);
   x1 = (centerXFrac + x1) / FRACUNIT;

   // off the right side?
   if (x1 > viewportWidth)
       return;

   tx += ((fixed_t)BIGSHORT(patch->width) << FRACBITS);
   x2 = FixedMul(tx, xscale);
   x2 = ((centerXFrac + x2) / FRACUNIT) - 1;

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
   if(vd.vissprite_p >= vd.vissprites + MAXVISSPRITES)
      return; // too many visible sprites already, leave room for psprites

   vis = (vissprite_t *)vd.vissprite_p;
   vd.vissprite_p++;

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
   vis->startfrac = 0;

   if(flip)
   {
      vis->xiscale = -FixedDiv(FRACUNIT, xscale);
      vis->startfrac = ((fixed_t)BIGSHORT(patch->width) << FRACBITS) - 1;

      // Adjust offset
      
   }
   else
   {
      vis->startfrac = 0;
      vis->xiscale = FixedDiv(FRACUNIT, xscale);
   }

   if (vis->x1 > x1)
      vis->startfrac += vis->xiscale*(vis->x1 - x1);

   if (thing->flags & MF_SHADOW)
   {
      vis->colormap = -vd.fuzzcolormap;
   }
   else if (vd.fixedcolormap)
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

   vis->colormaps = dc_colormaps;
}

//
// Process actors in all visible subsectors
//
void R_SpritePrep(void)
{
   sector_t **pse = vd.vissectors;

   while(pse < vd.lastvissector)
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
   vd.lastsprite_p = vd.vissprite_p;
}

// EOF

