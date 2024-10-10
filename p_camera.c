#include "p_camera.h"

camera_t camera;
//#define NOCLIPCAMERA

static boolean PIT_CameraCheckLine(line_t *ld, pmovework_t *mw)
{
   fixed_t   opentop, openbottom, lowfloor;
   sector_t *front, *back;

   // The moving thing's destination positoin will cross the given line.
   // If this should not be allowed, return false.
   if(ld->sidenum[1] == -1)
      return false; // one-sided line

   if(ld->flags & ML_BLOCKING)
      return false; // explicitly blocking everything

   front = LD_FRONTSECTOR(ld);
   back  = LD_BACKSECTOR(ld);

   if(front->ceilingheight == front->floorheight ||
      back->ceilingheight == back->floorheight)
   {
      mw->blockline = ld;
      return false; // probably a closed door
   }

   if(front->ceilingheight < back->ceilingheight)
      opentop = front->ceilingheight;
   else
      opentop = back->ceilingheight;

   if(front->floorheight > back->floorheight)
   {
      openbottom = front->floorheight;
      lowfloor   = back->floorheight;
   }
   else
   {
      openbottom = back->floorheight;
      lowfloor   = front->floorheight;
   }

   // adjust floor/ceiling heights
   if(opentop < mw->tmceilingz)
      mw->tmceilingz = opentop;
   if(openbottom >mw->tmfloorz)
      mw->tmfloorz = openbottom;
   if(lowfloor < mw->tmdropoffz)
      mw->tmdropoffz = lowfloor;

   return true;
}

static boolean PM_CameraCrossCheck(line_t *ld, pmovework_t *mw)
{
   if(PM_BoxCrossLine(ld, mw))
   {
      if(!PIT_CameraCheckLine(ld, mw))
         return false;
   }
   return true;
}

static boolean PM_CameraCheckPosition(pmovework_t *mw)
{
   int xl, xh, yl, yh, bx, by;
   mobj_t *tmthing = mw->tmthing;
   VINT *lvalidcount;

   mw->tmflags = tmthing->flags;

   mw->tmbbox[BOXTOP   ] = mw->tmy + tmthing->radius;
   mw->tmbbox[BOXBOTTOM] = mw->tmy - tmthing->radius;
   mw->tmbbox[BOXRIGHT ] = mw->tmx + tmthing->radius;
   mw->tmbbox[BOXLEFT  ] = mw->tmx - tmthing->radius;

   mw->newsubsec = R_PointInSubsector(mw->tmx, mw->tmy);

   // the base floor/ceiling is from the subsector that contains the point.
   // Any contacted lines the step closer together will adjust them.
   mw->tmfloorz   = mw->tmdropoffz = mw->newsubsec->sector->floorheight;
   mw->tmceilingz = mw->newsubsec->sector->ceilingheight;

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   *lvalidcount = *lvalidcount + 1;
   if (*lvalidcount == 0)
      *lvalidcount = 1;

   mw->blockline = NULL;
   mw->numspechit = 0;

   if(mw->tmflags & MF_NOCLIP) // thing has no clipping?
      return true;

   // check lines
   xl = mw->tmbbox[BOXLEFT  ] - bmaporgx;
   xh = mw->tmbbox[BOXRIGHT ] - bmaporgx;
   yl = mw->tmbbox[BOXBOTTOM] - bmaporgy;
   yh = mw->tmbbox[BOXTOP   ] - bmaporgy;

   if(xl < 0)
      xl = 0;
   if(yl < 0)
      yl = 0;
	if(yh < 0 || xh < 0)
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
         if(!P_BlockLinesIterator(bx, by, (blocklinesiter_t)PM_CameraCrossCheck, mw))
            return false;
      }
   }

   return true;
}

static boolean P_CameraTryMove2(ptrymove_t *tm, boolean checkposonly)
{
   pmovework_t mw;
   boolean trymove2; // result from P_CameraTryMove2
   mobj_t *tmthing = tm->tmthing;

   mw.tmx = tm->tmx;
   mw.tmy = tm->tmy;
   mw.tmthing = tm->tmthing;
   mw.spechit = &tm->spechit[0];

   trymove2 = PM_CameraCheckPosition(&mw);

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
      if(mw.tmceilingz - mw.tmfloorz < tmthing->height)
         return false; // doesn't fit
      tm->floatok = true;
      if(mw.tmceilingz - tmthing->z < tmthing->height)
         return false; // mobj must lower itself to fit
      if(mw.tmfloorz - tmthing->z > 24*FRACUNIT)
         return false; // too big a step up
      if(!(tmthing->flags & (MF_DROPOFF|MF_FLOAT)) && mw.tmfloorz - mw.tmdropoffz > 24*FRACUNIT)
         return false; // don't stand over a dropoff
   }

   // the move is ok, so link the thing into its new position.
   tmthing->floorz   = mw.tmfloorz;
   tmthing->ceilingz = mw.tmceilingz;
   tmthing->x = mw.tmx;
   tmthing->y = mw.tmy;

   return true;
}

static boolean P_CameraTryMove (ptrymove_t *tm, mobj_t *thing, fixed_t x, fixed_t y)
{
	tm->tmthing = thing;
	tm->tmx = x;
	tm->tmy = y;
	return P_CameraTryMove2 (tm, false);
}

//
// Try to slide the player against walls by finding the closest move available.
//
static void P_CameraSlideMove(pslidemove_t *sm)
{
   int i;
   fixed_t dx, dy, rx, ry;
   fixed_t frac, slide;
   pslidework_t sw;
   mobj_t *slidething = sm->slidething;

   dx = slidething->momx;
   dy = slidething->momy;
   sw.slidex = slidething->x;
   sw.slidey = slidething->y;
   sw.slidething = slidething;
   sw.numspechit = 0;
   sw.spechit = &sm->spechit[0];

   // perform a maximum of three bumps
   for(i = 0; i < 3; i++)
   {
      frac = P_CompletableFrac(&sw, dx, dy);
      if(frac != FRACUNIT)
         frac -= 0x1000;
      if(frac < 0)
         frac = 0;

      rx = FixedMul(frac, dx);
      ry = FixedMul(frac, dy);

      sw.slidex += rx;
      sw.slidey += ry;

      // made it the entire way
      if(frac == FRACUNIT)
      {
         slidething->momx = dx;
         slidething->momy = dy;
//         SL_CheckSpecialLines(&sw); // Camera doesn't trip lines
         sm->slidex = sw.slidex;
         sm->slidey = sw.slidey;
         return;
      }

      // project the remaining move along the line that blocked movement
      dx -= rx;
      dy -= ry;
      dx = FixedMul(dx, sw.blocknvx);
      dy = FixedMul(dy, sw.blocknvy);
      slide = dx + dy;

      dx = FixedMul(slide, sw.blocknvx);
      dy = FixedMul(slide, sw.blocknvy);
   }

   // some hideous situation has happened that won't let the camera slide
   sm->slidex = slidething->x;
   sm->slidey = slidething->y;
   sm->slidething->momx = slidething->momy = 0;
}

static void P_ResetCamera(player_t *player, camera_t *thiscam)
{
	thiscam->x = player->mo->x;
	thiscam->y = player->mo->y;
	thiscam->z = player->mo->z + VIEWHEIGHT;
}

// Process the mobj-ish required functions of the camera
static void P_CameraThinker(player_t *player, camera_t *thiscam)
{
#ifdef NOCLIPCAMERA
   	thiscam->x += thiscam->momx;
   	thiscam->y += thiscam->momy;
   	thiscam->z += thiscam->momz;
   	return;
#else
	// P_CameraXYMovement()
	if (thiscam->momx || thiscam->momy)
   	{
		fixed_t momx, momy;
      	pslidemove_t sm;
      	ptrymove_t tm;

		momx = vblsinframe*(thiscam->momx>>2);
		momy = vblsinframe*(thiscam->momy>>2);

		mobj_t mo; // Our temporary dummy to pretend we are a mobj
		mo.flags = MF_SOLID|MF_FLOAT;
		mo.x = thiscam->x;
		mo.y = thiscam->y;
		mo.z = thiscam->z;
		mo.floorz = thiscam->floorz;
		mo.ceilingz = thiscam->ceilingz;
		mo.momx = thiscam->momx;
		mo.momy = thiscam->momy;
		mo.momz = thiscam->momz;
		mo.radius = CAM_RADIUS;
		mo.height = CAM_HEIGHT;
		sm.slidething = &mo;

		P_CameraSlideMove(&sm);

		if (sm.slidex == mo.x && sm.slidey == mo.y)
			goto camstairstep;

		if (P_CameraTryMove(&tm, &mo, sm.slidex, sm.slidey))
			goto camwrapup;

camstairstep:
		// something fucked up in slidemove, so stairstep
		if (P_CameraTryMove(&tm, &mo, mo.x, mo.y + momy))
		{
			mo.momx = 0;
			mo.momy = momy;
			goto camwrapup;
		}

		if (P_CameraTryMove(&tm, &mo, mo.x + momx, mo.y))
		{
			mo.momx = momx;
			mo.momy = 0;
			goto camwrapup;
		}

		mo.momx = mo.momy = 0;

camwrapup:
		thiscam->x = mo.x;
		thiscam->y = mo.y;
		thiscam->z = mo.z;
		thiscam->floorz = mo.floorz;
		thiscam->ceilingz = mo.ceilingz;
		thiscam->momx = mo.momx;
		thiscam->momy = mo.momy;
		thiscam->momz = mo.momz;
   }

	// P_CameraZMovement()
	if (thiscam->momz)
	{
   		thiscam->z += thiscam->momz;

		// Don't go below the floor
		if (thiscam->z <= thiscam->floorz)
		{
			thiscam->z = thiscam->floorz;
			if (thiscam->z > player->viewz + CAM_HEIGHT + (16 << FRACBITS))
			{
				// Camera got stuck above the player
				P_ResetCamera(player, thiscam);
			}
		}

		// Don't go above the ceiling
		if (thiscam->z + CAM_HEIGHT > thiscam->ceilingz)
		{
			if (thiscam->momz > 0)
				thiscam->momz = 0;

			thiscam->z = thiscam->ceilingz - CAM_HEIGHT;

			if (thiscam->z + CAM_HEIGHT < player->mo->z - player->mo->height)
			{
				// Camera got stuck below the player
				P_ResetCamera(player, thiscam);
			}
		}
	}

   	if (thiscam->ceilingz - thiscam->z < CAM_HEIGHT && thiscam->ceilingz >= thiscam->z)
	{
		thiscam->ceilingz = thiscam->z + CAM_HEIGHT;
		thiscam->floorz = thiscam->z;
	}
#endif
}

void P_MoveChaseCamera(player_t *player, camera_t *thiscam)
{
	angle_t angle = 0;
	angle_t focusangle = 0;
	fixed_t x, y, z, viewpointx, viewpointy, camspeed, camdist, camheight, pviewheight;
	fixed_t dist;
	mobj_t *mo;
	subsector_t *newsubsec;

	mo = player->mo;
//	const fixed_t radius = CAM_RADIUS;
	const fixed_t height = CAM_HEIGHT;

	angle = focusangle = mo->angle;

	P_CameraThinker(player, thiscam);

	camspeed = FRACUNIT / 4;
	camdist = CAM_DIST;
	camheight = 20 << FRACBITS;

	if (P_AproxDistance(thiscam->x - mo->x, thiscam->y - mo->y) > camdist * 2)
	{
		// Camera is stuck, and the player has gone over twice as far away from it, so let's reset
		P_ResetCamera(player, thiscam);
	}

	dist = camdist;

	// If dead, camera is twice as close
	if (player->health <= 0)
		dist >>= 1;

	// Destination XY
	x = mo->x - FixedMul(finecosine((angle>>ANGLETOFINESHIFT) & FINEMASK), dist);
	y = mo->y - FixedMul(finesine((angle>>ANGLETOFINESHIFT) & FINEMASK), dist);

	pviewheight = VIEWHEIGHT;

	z = mo->z + pviewheight + camheight;

	// Look at halfway between the camera and player. Is the ceiling lower? Then the camera should try to move down to fit under it
	newsubsec = R_PointInSubsector(((mo->x>>FRACBITS) + (thiscam->x>>FRACBITS))<<(FRACBITS-1), ((mo->y>>FRACBITS) + (thiscam->y>>FRACBITS))<<(FRACBITS-1));

	{
		// camera fit?
		if (newsubsec->sector->ceilingheight != newsubsec->sector->floorheight // Don't try to fit in sectors with equal floor and ceiling heights
			&& newsubsec->sector->ceilingheight - height < z)
			z = newsubsec->sector->ceilingheight - height-(11<<FRACBITS);
	}

	if (thiscam->z < thiscam->floorz)
		thiscam->z = thiscam->floorz;

	// The camera actually focuses 64 units ahead of where the player is.
	// This is more aesthetically pleasing.
	dist = 64 << FRACBITS;
	viewpointx = mo->x + FixedMul(finecosine((angle>>ANGLETOFINESHIFT) & FINEMASK), dist);
	viewpointy = mo->y + FixedMul(finesine((angle>>ANGLETOFINESHIFT) & FINEMASK), dist);

	thiscam->angle = R_PointToAngle2(thiscam->x, thiscam->y, viewpointx, viewpointy);

	// Set the mom vector, cut by the camera speed, as it tries to move to the destination position
	thiscam->momx = FixedMul(x - thiscam->x, camspeed);
	thiscam->momy = FixedMul(y - thiscam->y, camspeed);
	thiscam->momz = FixedMul(z - thiscam->z, camspeed);
}
