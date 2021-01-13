/*
  CALICO

  Renderer phase 3 - Sprite prep
*/

#include "doomdef.h"
#include "r_local.h"

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
   tr_x = thing->x - viewx;
   tr_y = thing->y - viewy;

   gxt =  FixedMul(tr_x, viewcos);
   gyt = -FixedMul(tr_y, viewsin);
   tz  = gxt - gyt;

   // thing is behind view plane?
   if(tz < MINZ)
      return;

   gxt = -FixedMul(tr_x, viewsin);
   gyt =  FixedMul(tr_y, viewcos);
   tx  = -(gyt + gxt);

   // too far off the side?
   if(tx > (tz << 2) || tx < -(tz<<2))
      return;

   // check sprite for validity
   if(thing->sprite < 0 || thing->sprite >= NUMSPRITES)
      return;

   sprdef = &sprites[thing->sprite];

   // check frame for validity
   if((thing->frame & FF_FRAMEMASK) >= sprdef->numframes)
      return;

   sprframe = &sprdef->spriteframes[thing->frame & FF_FRAMEMASK];

   if(sprframe->rotate)
   {
      // select proper rotation depending on player's view point
      ang  = R_PointToAngle2(viewx, viewy, thing->x, thing->y);
      rot  = (ang - thing->angle + (unsigned int)(ANG45 / 2)*9) >> 29;
      lump = sprframe->lump[rot];
      flip = (boolean)(sprframe->flip[rot]);
   }
   else
   {
      // sprite has a single view for all rotations
      lump = sprframe->lump[0];
      flip = (boolean)(sprframe->flip[0]);
   }

   // get a new vissprite
   if(vissprite_p == vissprites + MAXVISSPRITES)
      return; // too many visible sprites already
   vis = vissprite_p++;

   vis->patchnum = lump; // CALICO: store to patchnum, not patch (number vs pointer)
   vis->x1       = tx;
   vis->gx       = thing->x;
   vis->gy       = thing->y;
   vis->gz       = thing->z;
   vis->xscale   = xscale = FixedDiv(PROJECTION, tz);
   vis->yscale   = FixedMul(xscale, STRETCH);
   vis->yiscale  = FixedDiv(FRACUNIT, vis->yscale); // CALICO_FIXME: -1 in GAS... test w/o.

   if(flip)
      vis->xiscale = -FixedDiv(FRACUNIT, xscale);
   else
      vis->xiscale = FixedDiv(FRACUNIT, xscale);

   if(thing->frame & FF_FULLBRIGHT)
      vis->colormap = 255;
   else
      vis->colormap = thing->subsector->sector->lightlevel;
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

   sprdef   = &sprites[psp->state->sprite];
   sprframe = &sprdef->spriteframes[psp->state->frame & FF_FRAMEMASK];
   lump     = sprframe->lump[0];

   if(vissprite_p == vissprites + MAXVISSPRITES)
      return; // out of vissprites
   vis = vissprite_p++;

   vis->patchnum = lump; // CALICO: use patchnum here, not patch pointer
   vis->x1 = psp->sx / FRACUNIT;
   vis->texturemid = psp->sy;

   if(psp->state->frame & FF_FULLBRIGHT)
      vis->colormap = 255;
   else
      vis->colormap = viewplayer->mo->subsector->sector->lightlevel;
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
   for(i = 0, psp = viewplayer->psprites; i < NUMPSPRITES; i++, psp++)
   {
      if(psp->state)
         R_PrepPSprite(psp);
   }
}

// EOF

