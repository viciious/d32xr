#include "p_camera.h"

camera_t camera;
//#define NOCLIPCAMERA
#define CAM_PHYS_HEIGHT (12 << FRACBITS)

static boolean PIT_CameraCheckLine(line_t *ld, pmovework_t *mw)
{
   fixed_t   opentop, openbottom, lowfloor;
   sector_t *front, *back;

   // The moving thing's destination positoin will cross the given line.
   // If this should not be allowed, return false.
   if(ld->sidenum[1] == -1)
      return false; // one-sided line

   if(ldflags[ld-lines] & ML_BLOCKING)
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

   const fixed_t tradius = mobjinfo[tmthing->type].radius;
   mw->tmbbox[BOXTOP   ] = mw->tmy + tradius;
   mw->tmbbox[BOXBOTTOM] = mw->tmy - tradius;
   mw->tmbbox[BOXRIGHT ] = mw->tmx + tradius;
   mw->tmbbox[BOXLEFT  ] = mw->tmx - tradius;

   mw->newsubsec = I_TO_SS(R_PointInSubsector2(mw->tmx, mw->tmy));
   mw->newsec = I_TO_SEC(mw->newsubsec->isector);

   // the base floor/ceiling is from the subsector that contains the point.
   // Any contacted lines the step closer together will adjust them.
   mw->tmfloorz   = mw->tmdropoffz = mw->newsec->floorheight;
   mw->tmceilingz = mw->newsec->ceilingheight;

   I_GetThreadLocalVar(DOOMTLS_VALIDCOUNT, lvalidcount);
   *lvalidcount = *lvalidcount + 1;
   if (*lvalidcount == 0)
      *lvalidcount = 1;

   mw->blockline = NULL;

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
      if(mw.tmceilingz - mw.tmfloorz < (tmthing->theight << FRACBITS))
         return false; // doesn't fit
      tm->floatok = true;
      if((fixed_t)tm->tmthing->angle <= CAM_DIST && mw.tmceilingz - tmthing->z < (tmthing->theight << FRACBITS))
         return false; // mobj must lower itself to fit
      if(mw.tmfloorz - tmthing->z > 24*FRACUNIT)
         return false; // too big a step up
   }

   // the move is ok, so link the thing into its new position.
   tmthing->floorz   = mw.tmfloorz;
   tmthing->ceilingz = mw.tmceilingz;
   tmthing->x = mw.tmx;
   tmthing->y = mw.tmy;
   tmthing->isubsector = SS_TO_I(mw.newsubsec);

   return true;
}

static boolean P_CameraTryMove (ptrymove_t *tm, mobj_t *thing, fixed_t x, fixed_t y)
{
	tm->tmthing = thing;
	tm->tmx = x;
	tm->tmy = y;
	return P_CameraTryMove2 (tm, false);
}

static void P_ResetCamera(player_t *player, camera_t *thiscam)
{
	thiscam->x = player->mo->x;
	thiscam->y = player->mo->y;
	thiscam->z = player->mo->z + VIEWHEIGHT;
   thiscam->angle = player->mo->angle;
   thiscam->aiming = 0;
   thiscam->subsector = I_TO_SS(R_PointInSubsector2(thiscam->x, thiscam->y));
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

		momx = (thiscam->momx>>2);
		momy = (thiscam->momy>>2);

		mobj_t mo; // Our temporary dummy to pretend we are a mobj
		mo.flags = MF_SOLID;
      mo.flags2 = MF2_FLOAT;
		mo.x = thiscam->x;
		mo.y = thiscam->y;
		mo.z = thiscam->z;
		mo.floorz = thiscam->floorz;
		mo.ceilingz = thiscam->ceilingz;
		mo.momx = thiscam->momx;
		mo.momy = thiscam->momy;
		mo.momz = thiscam->momz;
		mo.theight = CAM_PHYS_HEIGHT >> FRACBITS;
      mo.angle = (angle_t)thiscam->distFromPlayer;
      mo.type = MT_CAMERA;
		sm.slidething = &mo;

		P_SlideMove(&sm);

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
      thiscam->subsector = I_TO_SS(R_PointInSubsector2(thiscam->x, thiscam->y));
   }

	// P_CameraZMovement()
	if (thiscam->momz)
	{
   	thiscam->z += thiscam->momz;

		// Don't go below the floor
		if (thiscam->z <= thiscam->floorz)
		{
			thiscam->z = thiscam->floorz;
			if (thiscam->z > player->viewz + CAM_PHYS_HEIGHT + (16 << FRACBITS))
			{
				// Camera got stuck above the player
				P_ResetCamera(player, thiscam);
			}
		}

		// Don't go above the ceiling
		if (thiscam->z + CAM_PHYS_HEIGHT > thiscam->ceilingz)
		{
			if (thiscam->momz > 0)
				thiscam->momz = 0;

			thiscam->z = thiscam->ceilingz - CAM_PHYS_HEIGHT;

			if (thiscam->z + CAM_PHYS_HEIGHT < player->mo->z - (player->mo->theight << FRACBITS))
			{
				// Camera got stuck below the player
				P_ResetCamera(player, thiscam);
			}
		}
	}

   if (thiscam->ceilingz - thiscam->z < CAM_PHYS_HEIGHT && thiscam->ceilingz >= thiscam->z)
	{
		thiscam->ceilingz = thiscam->z + CAM_PHYS_HEIGHT;
		thiscam->floorz = thiscam->z;
	}
#endif
}

mobj_t *camBossMobj = NULL;

void P_MoveChaseCamera(player_t *player, camera_t *thiscam)
{
	angle_t angle = 0;
	angle_t focusangle = 0;
	fixed_t x, y, z, viewpointx, viewpointy, camspeed, camdist, camheight, pviewheight;
	fixed_t dist;
	mobj_t *mo;
	VINT newsubsec;
   const mobjinfo_t *caminfo = &mobjinfo[MT_CAMERA];

	mo = player->mo;
   thiscam->distFromPlayer = P_AproxDistance(P_AproxDistance(thiscam->x - mo->x, thiscam->y - mo->y), thiscam->z - mo->z);

   // If there is a boss, should focus on the boss
   if (camBossMobj)
   {
      if (camBossMobj->health > 0 || (camBossMobj->flags2 & MF2_BOSSFLEE))
         angle = focusangle = R_PointToAngle2(thiscam->x, thiscam->y, camBossMobj->x, camBossMobj->y);
      else
         angle = focusangle = R_PointToAngle2(thiscam->x, thiscam->y, 0, 0);
   }
   else
   {
      if (!player->exiting && player->stillTimer > TICRATE/2 && !(player->buttons & (BT_CAMLEFT | BT_CAMRIGHT)))
         angle = focusangle = mo->angle;
      else
         angle = focusangle = R_PointToAngle2(thiscam->x, thiscam->y, mo->x, mo->y);
   }

	P_CameraThinker(player, thiscam);

	camspeed = FRACUNIT / 4;
	camdist = CAM_DIST;
	camheight = 20 << FRACBITS;

   if (!player->exiting && player->stillTimer > TICRATE/2)
      camspeed >>= 2;

	if (!player->exiting && (mo->flags2 & MF2_SHOOTABLE) && thiscam->distFromPlayer > camdist * 3)
	{
		// Camera is stuck, and the player has gone over twice as far away from it, so let's reset
		P_ResetCamera(player, thiscam);
	}

	dist = camdist;

	// If dead, camera is twice as close
	if (!(mo->flags2 & MF2_SHOOTABLE))
		dist >>= 1;
   else if (player->exiting)
   {
      dist *= 3; // Even farther away when exiting
      camspeed >>= 2;
   }

	// Destination XY
	x = mo->x - FixedMul(finecosine((angle>>ANGLETOFINESHIFT) & FINEMASK), dist);
	y = mo->y - FixedMul(finesine((angle>>ANGLETOFINESHIFT) & FINEMASK), dist);

	pviewheight = VIEWHEIGHT;

	if (player->pflags & PF_VERTICALFLIP)
		z = mo->z + FixedDiv(FixedMul(mobjinfo[mo->type].height,3),4) -
			(((mo->theight << FRACBITS) != mobjinfo[mo->type].height) ? mobjinfo[mo->type].height - (mo->theight << FRACBITS) : 0)
         - pviewheight - camheight;
	else
		z = mo->z + pviewheight + camheight;

	// Look at halfway between the camera and player. Is the ceiling lower? Then the camera should try to move down to fit under it
	newsubsec = R_PointInSubsector2(((mo->x>>FRACBITS) + (thiscam->x>>FRACBITS))<<(FRACBITS-1), ((mo->y>>FRACBITS) + (thiscam->y>>FRACBITS))<<(FRACBITS-1));
   const sector_t *newsec = SS_SECTOR(newsubsec);

	{
		// camera fit?
		if (newsec->ceilingheight != newsec->floorheight // Don't try to fit in sectors with equal floor and ceiling heights
			&& newsec->ceilingheight - caminfo->height < z)
			z = newsec->ceilingheight - caminfo->height-(11<<FRACBITS);
	}

	if (thiscam->z < thiscam->floorz)
		thiscam->z = thiscam->floorz;

   if (thiscam->z + caminfo->height > thiscam->ceilingz)
      thiscam->z = thiscam->ceilingz - caminfo->height-(11<<FRACBITS);

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

   if (player->buttons & BT_CAMLEFT)
      P_ThrustValues(thiscam->angle - ANG90, -16*FRACUNIT, &thiscam->momx, &thiscam->momy);
   if (player->buttons & BT_CAMRIGHT)
      P_ThrustValues(thiscam->angle - ANG90, 16*FRACUNIT, &thiscam->momx, &thiscam->momy);

   if (!(mo->flags2 & MF2_SHOOTABLE))
      thiscam->momx = thiscam->momy = thiscam->momz = 0;

   dist = P_AproxDistance(viewpointx - thiscam->x, viewpointy - thiscam->y);

   if (player->pflags & PF_VERTICALFLIP)
		angle = R_PointToAngle2(0, thiscam->z, dist,mo->z + (FixedDiv(FixedMul(mobjinfo[mo->type].height,3),4) >> 1)
			- (((mo->theight << FRACBITS) != mobjinfo[mo->type].height) ? (mobjinfo[mo->type].height - (mo->theight << FRACBITS)) >> 1 : 0));
	else
		angle = R_PointToAngle2(0, thiscam->z, dist, mo->z + (mobjinfo[mo->type].height >> 2));

   G_ClipAimingPitch((int*)&angle);

   if (player->playerstate == PST_DEAD || player->playerstate == PST_REBORN)
		thiscam->momz = 0;
   else
   {
      dist = thiscam->aiming - angle;
      thiscam->aiming -= (dist>>3);
   }

   const sector_t *camSec = I_TO_SEC(thiscam->subsector->isector);

   if (thiscam->z < camSec->floorheight)
      thiscam->z = camSec->floorheight;
   else if (thiscam->z + CAM_PHYS_HEIGHT > camSec->ceilingheight)
      thiscam->z = camSec->ceilingheight - CAM_PHYS_HEIGHT;
}
