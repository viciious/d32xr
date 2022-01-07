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
   fixed_t gxt, gyt;
   fixed_t tx, tz;
   fixed_t xscale;
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   angle_t      ang;
   unsigned int rot;
   boolean      flip;
   int          lump;
   vissprite_t *vis;

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


   // get a new vissprite
   if(vissprite_p == vissprites + MAXVISSPRITES)
      return; // too many visible sprites already
   vis = vissprite_p++;

   vis->patch    = (void *)lump;
   vis->x1       = tx;
   vis->gx       = thing->x;
   vis->gy       = thing->y;
   vis->gz       = thing->z;
   vis->xscale   = xscale = FixedDiv(PROJECTION, tz);
   FixedMul2(vis->yscale, xscale, stretch);
   vis->yiscale  = FixedDiv(FRACUNIT, vis->yscale); // CALICO_FIXME: -1 in GAS... test w/o.

   if(flip)
      vis->xiscale = -FixedDiv(FRACUNIT, xscale);
   else
      vis->xiscale = FixedDiv(FRACUNIT, xscale);

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

   vis->drawcol = (thing->flags & MF_SHADOW) ? drawfuzzycol : drawcol;
}

//
// Project player weapon sprite
//
static void R_PrepPSprite(pspdef_t *psp)
{
   spritedef_t   *sprdef;
   spriteframe_t *sprframe;
   int            lump;
   vissprite_t   *vis;
   fixed_t        center, xscale;
   const state_t* state = &states[psp->state];

   sprdef = &sprites[state->sprite];
   sprframe = &spriteframes[sprdef->firstframe + (state->frame & FF_FRAMEMASK)];
   lump     = sprframe->lump[0];

   if(vissprite_p == vissprites + MAXVISSPRITES)
      return; // out of vissprites
   vis = vissprite_p++;

   vis->patch = (void *)lump;
   vis->texturemid = psp->sy;

   center = centerX;
   if (!lowResMode)
       center /= 2;
   center -= 80;
   center *= FRACUNIT;

   xscale = weaponXScale;

   vis->x1 = psp->sx + center;
   vis->xscale = xscale;
   vis->yscale = FRACUNIT;
   vis->yiscale = FRACUNIT;
   vis->xiscale = FixedDiv(FRACUNIT, xscale);
   vis->texturemid = (vis->texturemid / FRACUNIT + weaponYpos) * FRACUNIT;

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
   vis->drawcol = I_DrawColumnLow;
}

//
// Process actors in all visible subsectors
//
void R_SpritePrep(void)
{
   subsector_t **ssp = vissubsectors;
   pspdef_t     *psp;
   int i;

   while(ssp < lastvissubsector)
   {
      subsector_t *ss = *ssp;
      sector_t    *se = ss->sector;

      if(se->validcount != validcount) // not already processed?
      {
         mobj_t *thing = se->thinglist;
         se->validcount = validcount;  // mark it as processed

         while(thing) // walk sector thing list
         {
            R_PrepMobj(thing);
            thing = thing->snext;
         }
      }
      ++ssp;
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

