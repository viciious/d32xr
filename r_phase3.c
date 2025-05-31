/*
  CALICO

  Renderer phase 3 - Sprite prep
*/

#include "doomdef.h"
#include "p_local.h"

static void R_PrepMobj(mobj_t* thing) ATTR_DATA_CACHE_ALIGN;
static void R_PrepRing(ringmobj_t* thing, uint8_t scenery) ATTR_DATA_CACHE_ALIGN;
void R_SpritePrep(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

//
// Project vissprite for potentially visible actor
//
static void R_PrepMobj(mobj_t *thing)
{
   fixed_t tr_x, tr_y;
   fixed_t tx, tz, x1, x2;
   fixed_t xscale;
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   angle_t      ang;
   patch_t      *patch;
   vissprite_t  *vis;
   VINT         *sprlump;
   VINT          lump;
   int8_t        flip;
   const int8_t doubleWide = (thing->flags2 & MF2_NARROWGFX) ? 2 : 1; // Sprites have half the horizontal resolution (like scenery)

   // transform origin relative to viewpoint
   tr_x = thing->x - vd.viewx;
   tr_y = thing->y - vd.viewy;

   tz = FixedMul(tr_x, vd.viewcos) + FixedMul(tr_y, vd.viewsin);

   // thing is behind view plane?
   if(tz < MINZ || tz > 2048*FRACUNIT) // Cull draw distance
      return;

   tx = -(FixedMul(tr_y, vd.viewcos) - FixedMul(tr_x, vd.viewsin));

   // too far off the side?
   if(tx > (tz << 2) || tx < -(tz<<2))
      return;

   sprdef = &sprites[states[thing->state].sprite];

   const uint16_t frame = states[thing->state].frame;

   // check frame for validity
//   if ((frame & FF_FRAMEMASK) >= sprdef->numframes)
//       return;

   sprframe = &spriteframes[sprdef->firstframe + (frame & FF_FRAMEMASK)];
   sprlump = &spritelumps[sprframe->lump];

   lump = sprlump[0];
   if(!(lump & SL_SINGLESIDED))
   {
      // select proper rotation depending on player's view point
      ang  = R_PointToAngle2(vd.viewx, vd.viewy, thing->x, thing->y);
      lump = sprlump[(ang - thing->angle + (unsigned int)(ANG45 / 2)*9) >> 29];
   }
   // else // sprite has a single view for all rotations

   flip = (frame & FF_FLIPPED) ? -1 : 1;
   if (lump & SL_FLIPPED)
      flip = -1;

   lump &= SL_LUMPMASK;

   if (thing->flags2 & MF2_FORWARDOFFSET)
      tz -= 1024; // Make sure this sprite is drawn on top of sprites with the same distance to the camera

   patch = W_POINTLUMPNUM(lump);
   xscale = FixedDiv(PROJECTION, tz);

   // calculate edges of the shape
   const fixed_t offset = (BIGSHORT(patch->leftoffset)*doubleWide) << FRACBITS; // yeet
   if (flip < 0)
      tx -= ((BIGSHORT(patch->width)*doubleWide) << FRACBITS) - offset;
   else
      tx -= offset;

   x1 = (centerXFrac + FixedMul(tx, xscale)) >> FRACBITS;

   // off the right side?
   if (x1 > viewportWidth)
       return;

   tx += (fixed_t)((BIGSHORT(patch->width)*doubleWide) << FRACBITS);

   x2 = ((centerXFrac + FixedMul(tx, xscale)) >> FRACBITS) - 1;

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
   const sector_t *sec = SS_PSECTOR(thing->pisubsector);
   if (sec->pheightsec != (SPTR)0 && vd.viewsector->pheightsec != (SPTR)0)   // only clip things which are in special sectors
   {
      const fixed_t localgzt = thing->z + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);
      const fixed_t waterHeight = vd.viewwaterheight;
      const sector_t *heightsec = SPTR_TO_LPTR(sec->pheightsec);
      const fixed_t thingHeight = heightsec->ceilingheight;

      if ((vd.viewz < waterHeight) != (localgzt < thingHeight))
         return;
   }

   // get a new vissprite
   if(vd.vissprite_p >= vd.vissprites + MAXVISSPRITES)
      return; // too many visible sprites already, leave room for psprites

   vis = (vissprite_t *)vd.vissprite_p;
   vd.vissprite_p++;
   const fixed_t texmid = (thing->z - vd.viewz) + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);

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
   vis->pheightsec = sec->pheightsec;

   // Minimize branching for flip
   vis->xiscale = FixedDiv(FRACUNIT, xscale) * flip;

   if (thing->flags2 & MF2_NARROWGFX)
      vis->xiscale >>= 1;

   if(flip < 0)
      vis->startfrac = ((fixed_t)BIGSHORT(patch->width) << FRACBITS) - 1;
   else
      vis->startfrac = 0;

   if (vis->x1 > x1)
      vis->startfrac += vis->xiscale*(vis->x1 - x1);

   if (vd.fixedcolormap)
       vis->colormap = vd.fixedcolormap;
   else
   {
      if ((thing->type == MT_EGGMOBILE_MECH) && (thing->flags2 & MF2_FRET) && (gametic & 1))
         vis->colormap = BOSSFLASHCOLORMAP;
      else
      {
         if (frame & FF_FULLBRIGHT)
            vis->colormap = 255;
         else
            vis->colormap = sec->lightlevel;

         vis->colormap = HWLIGHT(vis->colormap);
      }
   }

   // Draw these in high detail
   if (thing->type <= MT_SIGN)
      vis->patchnum |= 32768;
   
   if (thing->player && players[thing->player-1].pflags & PF_VERTICALFLIP)
      vis->patchnum |= 16384;

//   vis->colormaps = dc_colormaps;
}

static void R_PrepRing(ringmobj_t *thing, uint8_t scenery)
{
   fixed_t tr_x, tr_y;
   fixed_t tx, tz, x1, x2;
   fixed_t xscale;
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   VINT         *sprlump;
   patch_t      *patch;
   vissprite_t  *vis;
   const scenerymobj_t *scenerymobj = NULL;
   VINT          lump;
   int8_t      flip;
   int8_t     doubleWide = scenery ? 2 : 1;
   int16_t x, y;

   if (scenery)
   {
      scenerymobj = (const scenerymobj_t*)thing;

      // transform origin relative to viewpoint
      x = scenerymobj->x;
      y = scenerymobj->y;
      tr_x = (x<<FRACBITS) - vd.viewx;
      tr_y = (y<<FRACBITS) - vd.viewy;
   }
   else
   {
      // transform origin relative to viewpoint
      x = thing->x;
      y = thing->y;
      tr_x = (thing->x<<FRACBITS) - vd.viewx;
      tr_y = (thing->y<<FRACBITS) - vd.viewy;
   }

   tz = FixedMul(tr_x, vd.viewcos) + FixedMul(tr_y, vd.viewsin);

   // thing is behind view plane?
   if(tz < MINZ || (tz > (2048-(512*scenery))*FRACUNIT)) // Cull draw distance
      return;

   tx = -(FixedMul(tr_y, vd.viewcos) - FixedMul(tr_x, vd.viewsin));

   // too far off the side?
   if(tx > (tz << 2) || tx < -(tz<<2))
      return;

   const state_t *state = &states[ringmobjstates[thing->type]];
   const VINT thingframe = state->frame;

   // We can assume a lot of things here!
   sprdef = &sprites[state->sprite];

   flip = (thingframe & FF_FLIPPED) ? -1 : 1;

   sprframe = &spriteframes[sprdef->firstframe + (thingframe & FF_FRAMEMASK)];
   sprlump = &spritelumps[sprframe->lump];

   // sprite has a single view for all rotations
   lump = sprlump[0];
   lump &= SL_LUMPMASK;

   patch = W_POINTLUMPNUM(lump);
   xscale = FixedDiv(PROJECTION, tz);

   // calculate edges of the shape
#ifdef NARROW_SCENERY
   if (flip < 0)
      tx -= ((fixed_t)(BIGSHORT(patch->width)*doubleWide)-(fixed_t)(BIGSHORT(patch->leftoffset)*doubleWide)) << FRACBITS;
   else
      tx -= ((fixed_t)BIGSHORT(patch->leftoffset)*doubleWide) << FRACBITS;
#else
   const fixed_t offset = BIGSHORT(patch->leftoffset) << (FRACBITS);
   if (flip < 0)
      tx -= ((BIGSHORT(patch->width) << (FRACBITS)) - offset);
   else
      tx -= offset;
#endif

   x1 = (centerXFrac + FixedMul(tx, xscale)) >> FRACBITS;

   // off the right side?
   if (x1 > viewportWidth)
       return;

#ifdef NARROW_SCENERY
   tx += ((fixed_t)((BIGSHORT(patch->width)*doubleWide))) << FRACBITS;
#else
   tx += ((fixed_t)BIGSHORT(patch->width) << FRACBITS);
#endif

   x2 = ((centerXFrac + FixedMul(tx, xscale)) >> FRACBITS) - 1;

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
   const sector_t *sec = SS_PSECTOR(thing->isubsector);
   const fixed_t thingz = scenery ? (thing->type < MT_STALAGMITE0 || thing->type > MT_STALAGMITE7 ? sec->floorheight : sec->ceilingheight - mobjinfo[thing->type].height) : thing->z << FRACBITS;
   if (sec->pheightsec != (SPTR)0 && vd.viewsector->pheightsec != (SPTR)0)   // only clip things which are in special sectors
   {
      const fixed_t localgzt = thingz + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);
      const fixed_t waterHeight = vd.viewwaterheight;
      const sector_t *heightsec = SPTR_TO_LPTR(sec->pheightsec);
      const fixed_t thingHeight = heightsec->ceilingheight;

      if ((vd.viewz < waterHeight) != (localgzt < thingHeight))
         return;
   }

   // get a new vissprite
   if(vd.vissprite_p >= vd.vissprites + MAXVISSPRITES)
      return; // too many visible sprites already, leave room for psprites
   vis = (vissprite_t *)vd.vissprite_p;
   vd.vissprite_p++;

   const fixed_t texmid = (thingz - vd.viewz) + ((fixed_t)BIGSHORT(patch->topoffset) << FRACBITS);

   vis->patchnum = lump;
#ifndef MARS
   vis->pixels   = R_CheckPixels(lump + 1);
#endif
   vis->x1       = x1 < 0 ? 0 : x1;
   vis->x2       = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   vis->gx       = x;
   vis->gy       = y;
   vis->xscale   = xscale;
   vis->yscale   = FixedMul(xscale, stretch);
   vis->patchheight = BIGSHORT(patch->height);
   vis->texturemid = texmid;
   vis->startfrac = 0;
   vis->pheightsec = sec->pheightsec;

   vis->xiscale = FixedDiv(FRACUNIT, xscale) * flip;
#ifdef NARROW_SCENERY
   if (scenery)
      vis->xiscale >>= 1;
#endif

   if(flip < 0)
      vis->startfrac = ((fixed_t)BIGSHORT(patch->width) << FRACBITS) - 1;
   else
      vis->startfrac = 0;

   if (vis->x1 > x1)
      vis->startfrac += vis->xiscale*(vis->x1 - x1);

   if (vd.fixedcolormap)
       vis->colormap = vd.fixedcolormap;
   else
       vis->colormap = HWLIGHT(sec->lightlevel);
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
      mobj_t *thing = SPTR_TO_LPTR(se->thinglist);

      while(thing) // walk sector thing list
      {
         if (thing->flags & MF_RINGMOBJ)
            R_PrepRing((ringmobj_t*)thing, !!(thing->flags & MF_NOBLOCKMAP)); // Devolve the MF_BLOCKMAP flag into a 1 or 0
         else if (!(thing->flags2 & MF2_DONTDRAW))
            R_PrepMobj(thing);

         thing = SPTR_TO_LPTR(thing->snext);
      }
      ++pse;
   }

   // remember end of actor vissprites
   vd.lastsprite_p = vd.vissprite_p;
}

// EOF
