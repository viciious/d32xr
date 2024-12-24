/*
  CALICO

  Actor movement and clipping

  The MIT License (MIT)

  Copyright (c) 2015 James Haley, Olde Skuul, id Software and ZeniMax Media
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "doomdef.h"
#include "p_local.h"

boolean PIT_CheckThing(mobj_t* thing, pmovework_t *mw) ATTR_DATA_CACHE_ALIGN;
static boolean PIT_CheckLine(line_t* ld, pmovework_t *mw) ATTR_DATA_CACHE_ALIGN;
static boolean PM_CrossCheck(line_t* ld, pmovework_t *mw) ATTR_DATA_CACHE_ALIGN;
static boolean PM_CheckPosition(pmovework_t *mw) ATTR_DATA_CACHE_ALIGN;
boolean P_TryMove2(ptrymove_t *tm, boolean checkposonly) ATTR_DATA_CACHE_ALIGN;

//
// Check a single mobj in one of the contacted blockmap cells.
//
boolean PIT_CheckThing(mobj_t *thing, pmovework_t *mw)
{
   fixed_t blockdist;
   int     delta;
   int     damage;
   boolean solid;
   mobj_t  *tmthing = mw->tmthing;
//   int     tmflags = mw->tmflags;
   const mobjinfo_t* thinfo = &mobjinfo[tmthing->type];

   if(!((thing->flags & (MF_SOLID|MF_SPECIAL)) || Mobj_HasFlags2(thing, MF2_SHOOTABLE)))
      return true;

   if (thing->type == MT_PLAYER && Mobj_HasFlags2(tmthing, MF2_SHOOTABLE))
      return true;

   blockdist = mobjinfo[thing->type].radius + thinfo->radius;
   
   if (thing->flags & MF_RINGMOBJ)
   {
      ringmobj_t *ring = (ringmobj_t*)thing;
      delta = (ring->x << FRACBITS) - mw->tmx;
      if(delta < 0)
         delta = -delta;
      if(delta >= blockdist)
         return true; // didn't hit it

      delta = (ring->y << FRACBITS) - mw->tmy;
   }
   else
   {
      delta = thing->x - mw->tmx;
      if(delta < 0)
         delta = -delta;
      if(delta >= blockdist)
         return true; // didn't hit it

      delta = thing->y - mw->tmy;
   }

   if(delta < 0)
      delta = -delta;
   if(delta >= blockdist)
      return true; // didn't hit it

   if(thing == tmthing)
      return true; // don't clip against self
      
   // Z-checking
   const fixed_t theight = Mobj_GetHeight(thing);
   const fixed_t tmheight = Mobj_GetHeight(tmthing);

   if (thing->flags & MF_RINGMOBJ)
   {
      ringmobj_t *ring = (ringmobj_t*)thing;
      if(tmthing->z > (ring->z << FRACBITS) + theight)
         return true; // went overhead
      if(tmthing->z + tmheight < (ring->z << FRACBITS))
         return true; // went underneath
   }
   else
   {
      if(tmthing->z > thing->z + theight)
         return true; // went overhead
      if(tmthing->z + tmheight < thing->z)
         return true; // went underneath
   }
         
   // missiles can hit other things
   if(Mobj_HasFlags2(tmthing, MF2_MISSILE))
   {
      if(tmthing->target->type == thing->type) // don't hit same species as originator
      {
         if(thing == tmthing->target) // don't hit originator
            return true;
         if(thing->type != MT_PLAYER) // let players missile each other
            return false; // explode, but do no damage
      }
      if(!Mobj_HasFlags2(thing, MF2_SHOOTABLE))
         return !(thing->flags & MF_SOLID); // didn't do any damage

      // damage/explode
		damage = ((P_Random()&7)+1)* thinfo->damage;
		P_DamageMobj (thing, tmthing, tmthing->target, damage);
      return false; // don't traverse any more
   }

   // check for special pickup
   if(tmthing->type == MT_PLAYER)
   {
      P_TouchSpecialThing(thing,tmthing);
   }
   if (thing->type == MT_PLAYER)
   {
      P_TouchSpecialThing(tmthing,thing);
   }

   solid = (thing->flags & MF_SOLID) != 0;

   return !solid;
}

//
// Check if the thing intersects a linedef
//
boolean PM_BoxCrossLine(line_t *ld, pmovework_t *mw)
{
   fixed_t x1, x2;
   fixed_t lx, ly;
   fixed_t ldx, ldy;
   fixed_t dx1, dy1, dx2, dy2;
   boolean side1, side2;
   fixed_t ldbbox[4];

   P_LineBBox(ld, ldbbox);

   if(mw->tmbbox[BOXRIGHT ] <= ldbbox[BOXLEFT  ] ||
      mw->tmbbox[BOXLEFT  ] >= ldbbox[BOXRIGHT ] ||
      mw->tmbbox[BOXTOP   ] <= ldbbox[BOXBOTTOM] ||
      mw->tmbbox[BOXBOTTOM] >= ldbbox[BOXTOP   ])
   {
      return false; // bounding boxes don't intersect
   }

   if(ld->flags & ML_ST_POSITIVE)
   {
      x1 = mw->tmbbox[BOXLEFT ];
      x2 = mw->tmbbox[BOXRIGHT];
   }
   else
   {
      x1 = mw->tmbbox[BOXRIGHT];
      x2 = mw->tmbbox[BOXLEFT ];
   }

   lx  = vertexes[ld->v1].x << FRACBITS;
   ly  = vertexes[ld->v1].y << FRACBITS;
   ldx = vertexes[ld->v2].x - vertexes[ld->v1].x;
   ldy = vertexes[ld->v2].y - vertexes[ld->v1].y;

   dx1 = (x1 - lx) >> FRACBITS;
   dy1 = (mw->tmbbox[BOXTOP] - ly) >> FRACBITS;
   dx2 = (x2 - lx) >> FRACBITS;
   dy2 = (mw->tmbbox[BOXBOTTOM] - ly) >> FRACBITS;

   side1 = (ldy * dx1 < dy1 * ldx);
   side2 = (ldy * dx2 < dy2 * ldx);

   return (side1 != side2);
}

//
// Adjusts tmfloorz and tmceilingz as lines are contacted.
//
static boolean PIT_CheckLine(line_t *ld, pmovework_t *mw)
{
   fixed_t   opentop, openbottom, lowfloor;
   sector_t *front, *back;
   mobj_t   *tmthing = mw->tmthing;

   // The moving thing's destination positoin will cross the given line.
   // If this should not be allowed, return false.
   if(ld->sidenum[1] == -1)
      return false; // one-sided line

   if(!(tmthing->flags2 & MF2_MISSILE))
   {
      if(ld->flags & ML_BLOCKING)
         return false; // explicitly blocking everything
      if(!tmthing->player && (ld->flags & ML_BLOCKMONSTERS))
         return false; // block monsters only
   }

   front = LD_FRONTSECTOR(ld);
   back  = LD_BACKSECTOR(ld);
   fixed_t frontFloor = FloorZAtPos(front, tmthing->z, tmthing->theight << FRACBITS);
   fixed_t frontCeiling = CeilingZAtPos(front, tmthing->z, tmthing->theight << FRACBITS);
   fixed_t backFloor = FloorZAtPos(back, tmthing->z, tmthing->theight << FRACBITS);
   fixed_t backCeiling = CeilingZAtPos(back, tmthing->z, tmthing->theight << FRACBITS);

   if(frontCeiling == frontFloor ||
      backCeiling == backFloor)
   {
      mw->blockline = ld;
      return false; // probably a closed door
   }

   if(frontCeiling < backCeiling)
      opentop = frontCeiling;
   else
      opentop = backCeiling;

   if(frontFloor > backFloor)
   {
      openbottom = frontFloor;
      lowfloor   = backFloor;
   }
   else
   {
      openbottom = backFloor;
      lowfloor   = frontFloor;
   }

   if (ld->flags & ML_MIDTEXTUREBLOCK)
   {
      const side_t *side = &sides[ld->sidenum[0]];
      const texture_t *tex = &textures[side->midtexture];
      const fixed_t texheight = tex->height << (FRACBITS+1);
      int16_t rowoffset = (side->textureoffset & 0xf000) | ((unsigned)side->rowoffset << 4);
      rowoffset >>= 4; // sign extend
      fixed_t textop, texbottom;

      if (ld->flags & ML_DONTPEGBOTTOM)
      {
         texbottom = openbottom + ((int)rowoffset << (FRACBITS));
         textop = texbottom + texheight;
      }
      else
      {
         textop = opentop + ((int)rowoffset << (FRACBITS));
         texbottom = textop - texheight;
      }

      const fixed_t texmid = texbottom + (textop - texbottom) / 2;

      const fixed_t thingBottomToMid = D_abs(mw->tmthing->z - texmid);
      const fixed_t thingTopToMid = D_abs(mw->tmthing->z + (mw->tmthing->theight << FRACBITS) - texmid);

      if (thingBottomToMid > thingTopToMid)
      {
         if (opentop > texbottom)
            opentop = texbottom;
      }
      else
      {
         if (openbottom < textop)
            openbottom = textop;
      }
   }

   // adjust floor/ceiling heights
   if(opentop < mw->tmceilingz)
      mw->tmceilingz = opentop;
   if(openbottom >mw->tmfloorz)
      mw->tmfloorz = openbottom;
   if(lowfloor < mw->tmdropoffz)
      mw->tmdropoffz = lowfloor;

   if (tmthing->player)
   {
      player_t *player = &players[tmthing->player-1];
      if (player->num_touching_sectors < MAX_TOUCHING_SECTORS)
         player->touching_sectorlist[player->num_touching_sectors++] = front - sectors;
      if (player->num_touching_sectors < MAX_TOUCHING_SECTORS)
         player->touching_sectorlist[player->num_touching_sectors++] = back - sectors;
   }
   return true;
}

//
// Check a single linedef in a blockmap cell.
//
static boolean PM_CrossCheck(line_t *ld, pmovework_t *mw)
{
   if(PM_BoxCrossLine(ld, mw))
   {
      if(!PIT_CheckLine(ld, mw))
         return false;
   }
   return true;
}

// This way, we check for collisions even when standing.
void P_PlayerCheckForStillPickups(mobj_t *mobj)
{
	int xl, xh, yl, yh, bx, by;
	pmovework_t mw;
	mw.newsubsec = &subsectors[mobj->isubsector];
	mw.tmfloorz = mw.tmdropoffz = mw.newsubsec->sector->floorheight;
	mw.tmceilingz = mw.newsubsec->sector->ceilingheight;
	mw.blockline = NULL;
	mw.tmflags = mobj->flags;
	mw.tmx = mobj->x;
	mw.tmy = mobj->y;
	mw.tmthing = mobj;

	mw.tmbbox[BOXTOP   ] = mw.tmy + mobjinfo[MT_PLAYER].radius;
	mw.tmbbox[BOXBOTTOM] = mw.tmy - mobjinfo[MT_PLAYER].radius;
	mw.tmbbox[BOXRIGHT ] = mw.tmx + mobjinfo[MT_PLAYER].radius;
	mw.tmbbox[BOXLEFT  ] = mw.tmx - mobjinfo[MT_PLAYER].radius;

	xl = mw.tmbbox[BOXLEFT  ] - bmaporgx - MAXRADIUS;
	xh = mw.tmbbox[BOXRIGHT ] - bmaporgx + MAXRADIUS;
	yl = mw.tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS;
	yh = mw.tmbbox[BOXTOP   ] - bmaporgy + MAXRADIUS;

	if(xl < 0)
		xl = 0;
	if(yl < 0)
		yl = 0;
	if(yh < 0)
		return;
	if(xh < 0)
		return;

   xl = (unsigned)xl >> MAPBLOCKSHIFT;
   xh = (unsigned)xh >> MAPBLOCKSHIFT;
   yl = (unsigned)yl >> MAPBLOCKSHIFT;
   yh = (unsigned)yh >> MAPBLOCKSHIFT;

   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;

   // check things
   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockThingsIterator(bx, by, (blockthingsiter_t)PIT_CheckThing, &mw))
            return;
      }
   }
}

//
// This is purely informative, nothing is modified (except things picked up)
//
static boolean PM_CheckPosition(pmovework_t *mw)
{
   int xl, xh, yl, yh, bx, by;
   mobj_t *tmthing = mw->tmthing;
   VINT *lvalidcount;

   mw->tmflags = tmthing->flags;

   mw->tmbbox[BOXTOP   ] = mw->tmy + mobjinfo[tmthing->type].radius;
   mw->tmbbox[BOXBOTTOM] = mw->tmy - mobjinfo[tmthing->type].radius;
   mw->tmbbox[BOXRIGHT ] = mw->tmx + mobjinfo[tmthing->type].radius;
   mw->tmbbox[BOXLEFT  ] = mw->tmx - mobjinfo[tmthing->type].radius;

   mw->newsubsec = R_PointInSubsector(mw->tmx, mw->tmy);

   // the base floor/ceiling is from the subsector that contains the point.
   // Any contacted lines the step closer together will adjust them.
   mw->tmfloorz   = mw->tmdropoffz = FloorZAtPos(mw->newsubsec->sector, mw->tmthing->z, mw->tmthing->theight << FRACBITS);
   mw->tmceilingz = CeilingZAtPos(mw->newsubsec->sector, mw->tmthing->z, mw->tmthing->theight << FRACBITS);

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   *lvalidcount = *lvalidcount + 1;
   if (*lvalidcount == 0)
      *lvalidcount = 1;

   mw->blockline = NULL;

   if(mw->tmflags & MF_NOCLIP) // thing has no clipping?
      return true;

   // Check things first, possibly picking things up.
   // The bounding box is extended by MAXRADIUS because mobj_ts are grouped
   // into mapblocks based on their origin point, and can overlap into adjacent
   // blocks by up to MAXRADIUS units.
   xl = mw->tmbbox[BOXLEFT  ] - bmaporgx - MAXRADIUS;
   xh = mw->tmbbox[BOXRIGHT ] - bmaporgx + MAXRADIUS;
   yl = mw->tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS;
   yh = mw->tmbbox[BOXTOP   ] - bmaporgy + MAXRADIUS;

	if(xl < 0)
		xl = 0;
	if(yl < 0)
		yl = 0;
	if(yh < 0)
		return true;
	if(xh < 0)
		return true;

   xl = (unsigned)xl >> MAPBLOCKSHIFT;
   xh = (unsigned)xh >> MAPBLOCKSHIFT;
   yl = (unsigned)yl >> MAPBLOCKSHIFT;
   yh = (unsigned)yh >> MAPBLOCKSHIFT;

   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;

   // check things
   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockThingsIterator(bx, by, (blockthingsiter_t)PIT_CheckThing, mw))
            return false;
      }
   }

   // check lines
   xl = mw->tmbbox[BOXLEFT  ] - bmaporgx;
   xh = mw->tmbbox[BOXRIGHT ] - bmaporgx;
   yl = mw->tmbbox[BOXBOTTOM] - bmaporgy;
   yh = mw->tmbbox[BOXTOP   ] - bmaporgy;

   if(xl < 0)
      xl = 0;
   if(yl < 0)
      yl = 0;
	if(yh < 0)
		return true;
	if(xh < 0)
		return true;

   xl = (unsigned)xl >> MAPBLOCKSHIFT;
   xh = (unsigned)xh >> MAPBLOCKSHIFT;
   yl = (unsigned)yl >> MAPBLOCKSHIFT;
   yh = (unsigned)yh >> MAPBLOCKSHIFT;

   if(xh >= bmapwidth)
      xh = bmapwidth - 1;
   if(yh >= bmapheight)
      yh = bmapheight - 1;

   for(bx = xl; bx <= xh; bx++)
   {
      for(by = yl; by <= yh; by++)
      {
         if(!P_BlockLinesIterator(bx, by, (blocklinesiter_t)PM_CrossCheck, mw))
            return false;
      }
   }

   return true;
}

//
// Attempt to move to a new position, crossing special lines unless MF_TELEPORT
// is set.
//
boolean P_TryMove2(ptrymove_t *tm, boolean checkposonly)
{
   pmovework_t mw;
   boolean trymove2; // result from P_TryMove2
   mobj_t *tmthing = tm->tmthing;

   mw.tmx = tm->tmx;
   mw.tmy = tm->tmy;
   mw.tmthing = tm->tmthing;

   trymove2 = PM_CheckPosition(&mw);

   tm->floatok = false;
   tm->blockline = mw.blockline;
   tm->tmfloorz = mw.tmfloorz;
   tm->tmceilingz = mw.tmceilingz;
   tm->tmdropoffz = mw.tmdropoffz;

   if(checkposonly)
      return trymove2;

   if(!trymove2)
      return false;

   if(!(tmthing->flags & MF_NOCLIP))
   {
      if(mw.tmceilingz - mw.tmfloorz < Mobj_GetHeight(tmthing))
         return false; // doesn't fit
      tm->floatok = true;
      if(mw.tmceilingz - tmthing->z < Mobj_GetHeight(tmthing))
         return false; // mobj must lower itself to fit
      if(mw.tmfloorz - tmthing->z > 24*FRACUNIT)
         return false; // too big a step up
      if (!((tmthing->flags2 & MF2_FLOAT) || tmthing->player) && mw.tmfloorz - mw.tmdropoffz > 24*FRACUNIT)
         return false; // don't stand over a dropoff
      if (tmthing->type == MT_SKIM && mw.newsubsec->sector->heightsec == -1)
         return false; // Skim can't go out of water
   }

   // the move is ok, so link the thing into its new position.
   P_UnsetThingPosition(tmthing);
   tmthing->floorz   = mw.tmfloorz;
   tmthing->ceilingz = mw.tmceilingz;
   tmthing->x        = mw.tmx;
   tmthing->y        = mw.tmy;
   P_SetThingPosition2(tmthing, mw.newsubsec);

   return true;
}

// EOF

