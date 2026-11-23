/*
  CALICO

  Renderer phase 3 - Sprite prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

static atomic_flag spr_lock = ATOMIC_FLAG_INIT;

static void R_PrepMobj(mobj_t* thing) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
static void R_PrepPSprite(pspdef_t* psp) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
void R_SpritePrep(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
void R_PSpritePrep(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

#define R_LockSpr() do { while (atomic_flag_test_and_set(&spr_lock)); } while(0)
#define R_UnlockSpr() do { atomic_flag_clear(&spr_lock); } while(0)

//
// Project vissprite for potentially visible actor
//
static void R_PrepMobj(mobj_t *thing)
{
   fixed_t tr_x, tr_y;
   fixed_t gxt, gyt, gzt;
   fixed_t tx, tz, x1, x2, cx1, cx2;
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
   fixed_t      topoffset, leftoffset;

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
   lump = sprframe->lump;
   if (lump == -1)
      return;

   if(!(lump & SL_SINGLESIDED))
   {
      sprlump = &spritelumps[sprframe->lump];
      lump = sprlump[0];
   }

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

#ifdef MARS
   SH2_DIVU_DVSR = tz;        // set 32-bit divisor
   SH2_DIVU_DVDNTH = centerXFrac>>FRACBITS; // high long of 64-bit dividend
   SH2_DIVU_DVDNTL = 0;       // low long, starts the divide
#endif
   patch = W_POINTLUMPNUM(lump);
   gzt = thing->z - vd->viewz;

   leftoffset = BIGSHORT(patch->leftoffset);
   topoffset = BIGSHORT(patch->topoffset);

   if (thing->flags & MF_ATTACHED)
   {
      leftoffset += thing->radius;
      topoffset += thing->height;
   }

   // calculate edges of the shape
   if (flip)
      tx -= ((fixed_t)BIGSHORT(patch->width)-leftoffset) << FRACBITS;
   else
      tx -= leftoffset << FRACBITS;

#ifdef MARS
   xscale = SH2_DIVU_DVDNTL; // get 32-bit quotient
#else
   xscale = FixedDiv(PROJECTION, tz);
#endif
   x1 = centerXFrac + FixedMul(tx, xscale);
   x2 = x1 + BIGSHORT(patch->width) * xscale;

   // off the right side?
   x1 = (x1 >> FRACBITS);
   if (x1 > viewportWidth)
       return;

   x2 = (x2 >> FRACBITS) - 1;
   // off the left side
   if (x2 < 0)
       return;

   // killough 4/9/98: clip things which are out of view due to height
   tz = FixedMul(gzt, xscale);
   if (tz > centerYFrac)
       return;

   texmid = gzt + ((fixed_t)topoffset << FRACBITS);
   tz = FixedMul(texmid, xscale);
   if (tz < viewportHeight - centerYFrac)
       return;

   cx1 = x1 < 0 ? 0 : x1;
   cx2 = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   if (cx1 > cx2)
      return;

   R_LockSpr();
   Mars_ClearCacheLine(&vd->vissprite_p);
   vis = (vissprite_t *)vd->vissprite_p;

   // get a new vissprite
   if(vd->vissprite_p >= vd->vissprites + MAXVISSPRITES - NUMPSPRITES) {
      R_UnlockSpr();
      return; // too many visible sprites already, leave room for psprites
   }
   vd->vissprite_p++;
   R_UnlockSpr();

#ifdef MARS
   SH2_DIVU_DVSR = xscale; // set 32-bit divisor
   SH2_DIVU_DVDNTH = 0;    // high long of 64-bit dividend
   SH2_DIVU_DVDNTL = -1;   // low long, starts the divide
#endif
   vis->patchnum = lump;
#ifndef MARS
   vis->pixels   = R_CheckPixels(lump + 1);
#endif
   vis->x1       = cx1;
   vis->x2       = cx2;
   vis->gx       = thing->x >> FRACBITS;
   vis->gy       = thing->y >> FRACBITS;
   vis->startfrac = 0;
   vis->texturemid = HEIGHT16(texmid);
   vis->yscale   = FixedMul(xscale, stretch);
   vis->xscale   = xscale;
#ifdef MARS
   vis->xiscale  = SH2_DIVU_DVDNTL; // get 32-bit quotient
#else
   vis->xiscale  = FixedDiv(FRACUNIT, xscale);
#endif

   if (thing->flags & MF_ATTACHED)
   {
      vis->xscale +=1;
   }

   if(flip)
   {
      vis->xiscale = -vis->xiscale;
      vis->startfrac = ((fixed_t)BIGSHORT(patch->width) << FRACBITS) - 1;
   }

   if (x1 < 0)
       vis->startfrac += vis->xiscale * -x1;

   if (thing->flags & MF_SHADOW)
      vis->colormap = -vd->fuzzcolormap;
   else if (vd->fixedcolormap)
       vis->colormap = vd->fixedcolormap;
   else
   {
      if (fullbright)
           vis->colormap = 255;
       else
           vis->colormap = SSEC_SECTOR(thing->subsector)->lightlevel;
       vis->colormap = HWLIGHT(vis->colormap);
   }

   if (thing->flags & MF_COLORMAP2)
      vis->colormap2 = 1;
   else
      vis->colormap2 = 0;
}

//
// Project player weapon sprite
//
static void R_PrepPSprite(pspdef_t *psp)
{
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   int          lump, framenum;
   patch_t      *patch;
   vissprite_t  *vis;
   fixed_t      center, xscale;
   fixed_t      topoffset, texmid;
   fixed_t      tx, x1, x2, cx1, cx2;
   fixed_t      leftoffset;
   const state_t* state;
   boolean      flip = handedness != 0;

   state = &states[psp->state];
   sprdef = &sprites[state->sprite];
   framenum = sprdef->firstframe + (state->frame & FF_FRAMEMASK);
   if (framenum < 0 || framenum >= numspriteframes)
      return;

   sprframe = &spriteframes[framenum];
   if (sprframe->lump == -1)
      return;
   lump     = sprframe->lump & SL_LUMPMASK;
   patch    = W_POINTLUMPNUM(lump);

   if (lump < firstsprite || lump >= firstsprite + numsprites)
      return;

   xscale = weaponXScale;
   center = centerXFrac - (lowres ? 320 - viewportWidth : 160) * weaponXScale;

   tx = psp->sx + center;
   topoffset = (((fixed_t)BIGSHORT(patch->topoffset) - weaponYpos) << FRACBITS) - psp->sy;

   texmid = BASEYCENTER * FRACUNIT + topoffset;
   leftoffset = BIGSHORT(patch->leftoffset);
   if (flip)
      tx += viewportWidth << FRACBITS;

   // calculate edges of the shape
   if (flip)
      tx -= ((fixed_t)BIGSHORT(patch->width)-leftoffset) << FRACBITS;
   else
      tx -= (leftoffset << FRACBITS);

   x1 = FixedMul(tx, xscale);
   x2 = x1 + BIGSHORT(patch->width) * xscale;

   x1 = (x1 >> FRACBITS);
   x2 = (x2 >> FRACBITS) - 1;

   // off the right side?
   if (x1 > viewportWidth)
       return;

   // off the left side
   if (x2 < 0)
       return;

   cx1 = x1 < 0 ? 0 : x1;
   cx2 = x2 >= viewportWidth ? viewportWidth - 1 : x2;
   if (cx1 > cx2)
      return;

   // store information in vissprite
   if(vd->vissprite_p >= vd->vissprites + MAXVISSPRITES)
      return; // out of vissprites

   vis = (vissprite_t *)vd->vissprite_p;
   vd->vissprite_p++;

#ifdef MARS
   SH2_DIVU_DVSR = xscale; // set 32-bit divisor
   SH2_DIVU_DVDNTH = 0;    // high long of 64-bit dividend
   SH2_DIVU_DVDNTL = -1;   // low long, starts the divide
#endif
   vis->patchnum = lump;
#ifndef MARS
   vis->pixels = R_CheckPixels(lump + 1);
#endif
   vis->x1 = cx1;
   vis->x2 = cx2;
   vis->yscale = FRACUNIT;
   vis->texturemid = HEIGHT16(texmid);
   vis->startfrac = 0;
   vis->xscale = xscale;
   if (vd->shadow)
      vis->colormap = -vd->fuzzcolormap;
   else if (vd->fixedcolormap)
       vis->colormap = vd->fixedcolormap;
   else
   {
       if (state->frame & FF_FULLBRIGHT)
           vis->colormap = 255;
       else
           vis->colormap = vd->lightlevel;
       vis->colormap = HWLIGHT(vis->colormap);
   }
   vis->colormap2 = 0;
#ifdef MARS
   vis->xiscale  = SH2_DIVU_DVDNTL; // get 32-bit quotient
#else
   vis->xiscale  = FixedDiv(FRACUNIT, xscale);
#endif

   if (flip)
   {
      vis->xiscale = -vis->xiscale;
      vis->startfrac = ((fixed_t)BIGSHORT(patch->width) << FRACBITS) - 1;
   }

   if (x1 < 0)
       vis->startfrac += vis->xiscale * -x1;
}

//
// Process actors in all visible subsectors
//
void R_SpritePrep(void)
{
   volatile sector_t * volatile *pse;

   while(1)
   {
      volatile mobj_t *thing;

      R_LockSpr();
#ifdef MARS      
      Mars_ClearCacheLine(&vd->pvissector); // this also takes care of psecthing because of the alignment
#endif

      pse = vd->pvissector;
      if (vd->psecthing == NULL) {
         pse = ++vd->pvissector;
         if (pse >= vd->lastvissector) {
            R_UnlockSpr();
            break;
         }
         vd->psecthing = SPTR_TO_LPTR((*pse)->thinglist);
      }

      R_UnlockSpr();

      while(1) // walk sector thing list
      {
        volatile mobj_t *next;

        R_LockSpr();

#ifdef MARS
        Mars_ClearCacheLine(&vd->pvissector); // this also takes care of psecthing because of the alignment
#endif
        if (vd->psecthing == NULL) {
         R_UnlockSpr();
         break;
        }

        thing = vd->psecthing;
        next = SPTR_TO_LPTR(thing->snext);
        vd->psecthing = next;

        R_UnlockSpr();

        R_PrepMobj((mobj_t *)thing);
      }
   }
}

#ifdef MARS

void Mars_Sec_R_SpritePrep(void)
{
   R_SpritePrep();
}

#endif

//
// Process actors in all visible subsectors
//
void R_PSpritePrep(void)
{
   pspdef_t     *psp;

#ifdef MARS
   Mars_R_SecWait();
   Mars_ClearCacheLine(&vd->vissprites); // this will also take care of lastsprite_p, vissprite_p & gsortedsprites
#endif

   // remember end of actor vissprites
   vd->lastsprite_p = vd->vissprite_p;

   // draw player weapon sprites
   for(psp = vd->psprites; psp < vd->lastpsprite; psp++)
   {
      if(psp->state)
         R_PrepPSprite(psp);
   }
}

// EOF

