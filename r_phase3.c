/*
  CALICO

  Renderer phase 3 - Sprite prep
*/

#include "doomdef.h"
#include "p_local.h"

static void R_PrepMobj(mobj_t* thing) ATTR_DATA_CACHE_ALIGN;
static void R_PrepRing(ringmobj_t* thing) ATTR_DATA_CACHE_ALIGN;
static void R_PrepScenery(scenerymobj_t* thing) ATTR_DATA_CACHE_ALIGN;
void R_SpritePrep(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

//
// Project vissprite for potentially visible actor
//
static void R_PrepMobj(mobj_t *thing)
{
   fixed_t tr_x, tr_y;
   fixed_t gxt, gyt;
   fixed_t tx, tz, x1, x2;
   fixed_t xscale;
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   VINT         *sprlump;
   angle_t      ang;
   unsigned int rot;
   boolean      flip;
   int          lump;
   patch_t      *patch;
   vissprite_t  *vis;
   const boolean doubleWide = (thing->flags2 & MF2_NARROWGFX); // Sprites have half the horizontal resolution (like scenery)

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

   if (tz > 2048*FRACUNIT) // Cull draw distance
      return;

   gxt = FixedMul(tr_x, vd.viewsin);
   gxt = -gxt;
   gyt = FixedMul(tr_y, vd.viewcos);
   tx  = -(gyt + gxt);

   // too far off the side?
   if(tx > (tz << 2) || tx < -(tz<<2))
      return;

   sprdef = &sprites[states[thing->state].sprite];

   const uint16_t frame = states[thing->state].frame;

   // check frame for validity
   if ((frame & FF_FRAMEMASK) >= sprdef->numframes)
       return;

   sprframe = &spriteframes[sprdef->firstframe + (frame & FF_FRAMEMASK)];
   sprlump = &spritelumps[sprframe->lump];

   lump = sprlump[0];
   if(!(lump & SL_SINGLESIDED))
   {
      // select proper rotation depending on player's view point
      ang  = R_PointToAngle2(vd.viewx, vd.viewy, thing->x, thing->y);
      rot  = (ang - thing->angle + (unsigned int)(ANG45 / 2)*9) >> 29;
      lump = sprlump[rot];
   }
   // else // sprite has a single view for all rotations

   flip = (frame & FF_FLIPPED);
   if (lump & SL_FLIPPED)
      flip = true;

   lump &= SL_LUMPMASK;

   if (thing->flags2 & MF2_FORWARDOFFSET)
      tz -= 1024; // Make sure this sprite is drawn on top of sprites with the same distance to the camera

   patch = W_POINTLUMPNUM(lump);
   xscale = FixedDiv(PROJECTION, tz);

   // calculate edges of the shape
   if (doubleWide)
   {
      if (flip)
         tx -= ((fixed_t)BIGSHORT(patch->width*2)-(fixed_t)BIGSHORT(patch->leftoffset*2)) << FRACBITS;
      else
         tx -= ((fixed_t)BIGSHORT(patch->leftoffset*2)) << FRACBITS;
   }
   else
   {
      if (flip)
         tx -= ((fixed_t)BIGSHORT(patch->width)-(fixed_t)BIGSHORT(patch->leftoffset)) << FRACBITS;
      else
         tx -= ((fixed_t)BIGSHORT(patch->leftoffset)) << FRACBITS;
   }

   x1 = FixedMul(tx, xscale);
   x1 = (centerXFrac + x1) / FRACUNIT;

   // off the right side?
   if (x1 > viewportWidth)
       return;

   if (doubleWide)
      tx += ((fixed_t)BIGSHORT(patch->width*2) << FRACBITS);
   else
      tx += ((fixed_t)BIGSHORT(patch->width) << FRACBITS);

   x2 = FixedMul(tx, xscale);
   x2 = ((centerXFrac + x2) / FRACUNIT) - 1;

   // off the left side
   if (x2 < 0)
       return;

   // killough 4/9/98: clip things which are out of view due to height
//   tz = FixedMul(gzt, xscale);
//   if (tz > centerYFrac)
//       return;

//   tz = FixedMul(texmid, xscale);
//   if (tz < viewportHeight - centerYFrac)
//       return;

   // killough 3/27/98: exclude things totally separated
   // from the viewer, by either water or fake ceilings
   // killough 4/11/98: improve sprite clipping for underwater/fake ceilings
   const int heightsec = subsectors[thing->isubsector].sector->heightsec;
   const int phs = vd.viewsubsector->sector->heightsec;
   if (heightsec != -1 && phs != -1)   // only clip things which are in special sectors
   {
      const sector_t *heightsector = &sectors[heightsec];
      const fixed_t localgzt = thing->z + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);

      if (vd.viewz < sectors[phs].ceilingheight) // camera is currently underwater
      {
         if (thing->z >= heightsector->ceilingheight) // thing is above water
            return;
      }
      else // camera is out of water
      {
         if (localgzt < heightsector->ceilingheight) // thing is underwater
            return;
      }
   }
   else if (phs != -1)
   {
      if (vd.viewz < sectors[phs].ceilingheight) // camera is currently underwater
      {
         if (thing->z >= sectors[phs].ceilingheight) // thing is above water
            return;
      }
   }

   // get a new vissprite
   if(vd.vissprite_p >= vd.vissprites + MAXVISSPRITES)
      return; // too many visible sprites already, leave room for psprites

   const fixed_t texmid = (thing->z - vd.viewz) + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);

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
   vis->patchheight = BIGSHORT(patch->height);
   vis->texturemid = texmid;
   vis->startfrac = 0;
   vis->heightsec = heightsec;

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

   if (doubleWide)
      vis->xiscale >>= 1;

   if (vis->x1 > x1)
      vis->startfrac += vis->xiscale*(vis->x1 - x1);

   if (vd.fixedcolormap)
   {
       vis->colormap = vd.fixedcolormap;
   }
   else
   {
      if (thing->type == MT_EGGMOBILE && (thing->flags2 & MF2_FRET) && (gametic & 1))
      {
         vis->colormap = BOSSFLASHCOLORMAP;
      }
      else
      {
         if (frame & FF_FULLBRIGHT)
            vis->colormap = 255;
         else
            vis->colormap = subsectors[thing->isubsector].sector->lightlevel;
         vis->colormap = HWLIGHT(vis->colormap);
      }
   }

   // Draw these in high detail
   if (thing->type <= MT_SIGN)
      vis->patchnum |= 32768;

//   vis->colormaps = dc_colormaps;
}

static void R_PrepRing(ringmobj_t *thing)
{
   fixed_t tr_x, tr_y;
   fixed_t gxt, gyt;
   fixed_t tx, tz, x1, x2;
   fixed_t xscale;
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   VINT         *sprlump;
   boolean      flip;
   int          lump;
   patch_t      *patch;
   vissprite_t  *vis;

   // transform origin relative to viewpoint
   tr_x = (thing->x<<FRACBITS) - vd.viewx;
   tr_y = (thing->y<<FRACBITS) - vd.viewy;

   gxt = FixedMul(tr_x, vd.viewcos);
   gyt = FixedMul(tr_y, vd.viewsin);
   gyt = -gyt;
   tz  = gxt - gyt;

   // thing is behind view plane?
   if(tz < MINZ)
      return;

   if (tz > 2048*FRACUNIT) // Cull draw distance
      return;

   gxt = FixedMul(tr_x, vd.viewsin);
   gxt = -gxt;
   gyt = FixedMul(tr_y, vd.viewcos);
   tx  = -(gyt + gxt);

   // too far off the side?
   if(tx > (tz << 2) || tx < -(tz<<2))
      return;

   const state_t *state = &states[ringmobjstates[thing->type]];
   const VINT thingframe = state->frame;

   // We can assume a lot of things here!
   sprdef = &sprites[state->sprite];

   flip = thingframe & FF_FLIPPED;

   sprframe = &spriteframes[sprdef->firstframe + (thingframe & FF_FRAMEMASK)];
   sprlump = &spritelumps[sprframe->lump];

   // sprite has a single view for all rotations
   lump = sprlump[0];
   lump &= SL_LUMPMASK;

   patch = W_POINTLUMPNUM(lump);
   xscale = FixedDiv(PROJECTION, tz);

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
//   tz = FixedMul(gzt, xscale);
//   if (tz > centerYFrac)
//       return;

//   tz = FixedMul(texmid, xscale);
//   if (tz < viewportHeight - centerYFrac)
//       return;

   // killough 3/27/98: exclude things totally separated
   // from the viewer, by either water or fake ceilings
   // killough 4/11/98: improve sprite clipping for underwater/fake ceilings
   const int heightsec = subsectors[thing->isubsector].sector->heightsec;
   const int phs = vd.viewsubsector->sector->heightsec;
   if (heightsec != -1 && phs != -1)   // only clip things which are in special sectors
   {
      const sector_t *heightsector = &sectors[heightsec];
      const fixed_t localgzt = (thing->z<<FRACBITS) + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);

      if (vd.viewz < sectors[phs].ceilingheight) // camera is currently underwater
      {
         if ((thing->z<<FRACBITS) >= heightsector->ceilingheight) // thing is above water
            return;
      }
      else // camera is out of water
      {
         if (localgzt < heightsector->ceilingheight) // thing is underwater
            return;
      }
   }
   else if (phs != -1)
   {
      if (vd.viewz < sectors[phs].ceilingheight) // camera is currently underwater
      {
         if ((thing->z<<FRACBITS) >= sectors[phs].ceilingheight) // thing is above water
            return;
      }
   }

   // get a new vissprite
   if(vd.vissprite_p >= vd.vissprites + MAXVISSPRITES)
      return; // too many visible sprites already, leave room for psprites

   const fixed_t texmid = ((thing->z << FRACBITS) - vd.viewz) + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);

   vis = (vissprite_t *)vd.vissprite_p;
   vd.vissprite_p++;

   vis->patchnum = lump;
#ifndef MARS
   vis->pixels   = R_CheckPixels(lump + 1);
#endif
   vis->x1       = x1 < 0 ? 0 : x1;
   vis->x2       = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   vis->gx       = thing->x;
   vis->gy       = thing->y;
   vis->xscale   = xscale;
   vis->yscale   = FixedMul(xscale, stretch);
   vis->patchheight = BIGSHORT(patch->height);
   vis->texturemid = texmid;
   vis->startfrac = 0;
   vis->heightsec = heightsec;

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

   if (vd.fixedcolormap)
       vis->colormap = vd.fixedcolormap;
   else
      vis->colormap = HWLIGHT(subsectors[thing->isubsector].sector->lightlevel);
 
//   vis->colormaps = dc_colormaps;
}

static void R_PrepScenery(scenerymobj_t *thing)
{
   fixed_t tr_x, tr_y;
   fixed_t gxt, gyt;
   fixed_t tx, tz, x1, x2;
   fixed_t xscale;
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   VINT         *sprlump;
   int          lump;
   patch_t      *patch;
   vissprite_t  *vis;

   // transform origin relative to viewpoint
   tr_x = (thing->x << FRACBITS) - vd.viewx;
   tr_y = (thing->y << FRACBITS) - vd.viewy;

   gxt = FixedMul(tr_x, vd.viewcos);
   gyt = FixedMul(tr_y, vd.viewsin);
   gyt = -gyt;
   tz  = gxt - gyt;

   // thing is behind view plane?
   if(tz < MINZ)
      return;

   if (tz > 1536*FRACUNIT) // Cull draw distance
      return;

   gxt = FixedMul(tr_x, vd.viewsin);
   gxt = -gxt;
   gyt = FixedMul(tr_y, vd.viewcos);
   tx  = -(gyt + gxt);

   // too far off the side?
   if(tx > (tz << 2) || tx < -(tz<<2))
      return;

   const state_t *state = &states[ringmobjstates[thing->type]];
   const VINT thingframe = state->frame;

   // We can assume a lot of things here!
   sprdef = &sprites[state->sprite];

   const boolean flip = thingframe & FF_FLIPPED;

   sprframe = &spriteframes[sprdef->firstframe + (thingframe & FF_FRAMEMASK)];
   sprlump = &spritelumps[sprframe->lump];

   // sprite has a single view for all rotations
   lump = sprlump[0];
   lump &= SL_LUMPMASK;

   patch = W_POINTLUMPNUM(lump);
   xscale = FixedDiv(PROJECTION, tz);

   // calculate edges of the shape
#ifdef NARROW_SCENERY
   if (flip)
      tx -= ((fixed_t)BIGSHORT(patch->width*2)-(fixed_t)BIGSHORT(patch->leftoffset*2)) << FRACBITS;
   else
      tx -= ((fixed_t)BIGSHORT(patch->leftoffset*2)) << FRACBITS;
#else
  if (flip)
      tx -= ((fixed_t)BIGSHORT(patch->width)-(fixed_t)BIGSHORT(patch->leftoffset)) << FRACBITS;
   else
      tx -= ((fixed_t)BIGSHORT(patch->leftoffset)) << FRACBITS;
#endif

   x1 = FixedMul(tx, xscale);
   x1 = (centerXFrac + x1) / FRACUNIT;

   // off the right side?
   if (x1 > viewportWidth)
       return;

#ifdef NARROW_SCENERY
   tx += ((fixed_t)BIGSHORT(patch->width*2) << FRACBITS);
#else
   tx += ((fixed_t)BIGSHORT(patch->width) << FRACBITS);
#endif
   x2 = FixedMul(tx, xscale);
   x2 = ((centerXFrac + x2) / FRACUNIT) - 1;

   // off the left side
   if (x2 < 0)
       return;

   const fixed_t thingz = subsectors[thing->isubsector].sector->floorheight;

   // killough 4/9/98: clip things which are out of view due to height
//   tz = FixedMul(gzt, xscale);
//   if (tz > centerYFrac)
//       return;

//   tz = FixedMul(texmid, xscale);
//   if (tz < viewportHeight - centerYFrac)
//       return;

   // killough 3/27/98: exclude things totally separated
   // from the viewer, by either water or fake ceilings
   // killough 4/11/98: improve sprite clipping for underwater/fake ceilings
   const int heightsec = subsectors[thing->isubsector].sector->heightsec;
   const int phs = vd.viewsubsector->sector->heightsec;
   if (heightsec != -1 && phs != -1)   // only clip things which are in special sectors
   {
      const sector_t *heightsector = &sectors[heightsec];
      const fixed_t localgzt = thingz + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);

      if (vd.viewz < sectors[phs].ceilingheight) // camera is currently underwater
      {
         if (thingz >= heightsector->ceilingheight) // thing is above water
            return;
      }
      else // camera is out of water
      {
         if (localgzt < heightsector->ceilingheight) // thing is underwater
            return;
      }
   }
   else if (phs != -1)
   {
      if (vd.viewz < sectors[phs].ceilingheight) // camera is currently underwater
      {
         if (thingz >= sectors[phs].ceilingheight) // thing is above water
            return;
      }
   }

   // get a new vissprite
   if(vd.vissprite_p >= vd.vissprites + MAXVISSPRITES)
      return; // too many visible sprites already, leave room for psprites

   const fixed_t texmid = (thingz - vd.viewz) + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);

   vis = (vissprite_t *)vd.vissprite_p;
   vd.vissprite_p++;

   vis->patchnum = lump;
#ifndef MARS
   vis->pixels   = R_CheckPixels(lump + 1);
#endif
   vis->x1       = x1 < 0 ? 0 : x1;
   vis->x2       = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   vis->gx       = thing->x;
   vis->gy       = thing->y;
   vis->xscale   = xscale;
   vis->yscale   = FixedMul(xscale, stretch);
   vis->patchheight = BIGSHORT(patch->height);
   vis->texturemid = texmid;
   vis->startfrac = 0;
   vis->heightsec = heightsec;

   if(flip)
   {
      vis->xiscale = -FixedDiv(FRACUNIT, xscale);
      vis->startfrac = ((fixed_t)BIGSHORT(patch->width) << FRACBITS) - 1;
   }
   else
   {
      vis->startfrac = 0;
      vis->xiscale = FixedDiv(FRACUNIT, xscale);
   }

#ifdef NARROW_SCENERY
   vis->xiscale >>= 1;
#endif

   if (vis->x1 > x1)
      vis->startfrac += vis->xiscale*(vis->x1 - x1);

   if (vd.fixedcolormap)
       vis->colormap = vd.fixedcolormap;
   else
      vis->colormap = HWLIGHT(subsectors[thing->isubsector].sector->lightlevel);
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
         if (thing->flags & MF_RINGMOBJ)
         {
            if (thing->flags & MF_NOBLOCKMAP)
               R_PrepScenery((scenerymobj_t*)thing);
            else
               R_PrepRing((ringmobj_t*)thing);
         }
         else if (!(thing->flags2 & MF2_DONTDRAW))
            R_PrepMobj(thing);

         thing = thing->snext;
      }
      ++pse;
   }

   // remember end of actor vissprites
   vd.lastsprite_p = vd.vissprite_p;
}

// EOF

