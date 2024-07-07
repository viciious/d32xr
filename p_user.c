/* P_user.c */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"
#include "p_camera.h"

#define ANG5 (ANG90 / 18)

#define SLOWTURNTICS 10
fixed_t angleturn[] =
	{300, 300, 500, 500, 600, 700, 800, 900, 900, 1000};
fixed_t fastangleturn[] =
	{800, 800, 900, 1000, 1000, 1200, 1200, 1300, 1300, 1400};

#define STOPSPEED FRACUNIT / 16
#define FRICTION 0xd240
#define MAXBOB 16 * FRACUNIT /* 16 pixels of bob */

/*
==================
=
= P_Thrust
=
= moves the given origin along a given angle
=
==================
*/

void P_ThrustValues(angle_t angle, fixed_t move, fixed_t *outX, fixed_t *outY)
{
	move *= vblsinframe;
	move /= TICVBLS;
	angle >>= ANGLETOFINESHIFT;
	*outX += FixedMul(move, finecosine(angle));
	*outY += FixedMul(move, finesine(angle));
}

#define P_Thrust(mo, angle, move) P_ThrustValues(angle, move, &mo->momx, &mo->momy)

void P_InstaThrust(mobj_t *mo, angle_t angle, fixed_t move)
{
	mo->momx = 0;
	mo->momy = 0;
	P_ThrustValues(angle, move, &mo->momx, &mo->momy);
}

boolean P_IsReeling(player_t *player)
{
	return player->mo->state == mobjinfo[player->mo->type].painstate && player->powers[pw_flashing];
}

void P_ResetPlayer(player_t *player)
{
	player->pflags &= ~PF_ONGROUND;
	player->pflags &= ~PF_SPINNING;
	player->pflags &= ~PF_STARTJUMP;
	player->pflags &= ~PF_JUMPED;
	player->pflags &= ~PF_THOKKED;
}

void P_PlayerRingBurst(player_t *player, int damage)
{
	int i;
	uint8_t num_rings = player->health - 1;

	if (num_rings > 32) // Cap # of rings at 32
		num_rings = 32;

	for (i = 0; i < num_rings; i++)
	{
		angle_t fa = (P_Random() << 5) & FINEMASK;
		fa <<= ANGLETOFINESHIFT;
		mobj_t *mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_FLINGRING);

		mo->threshold = (8-player->lossCount)*TICRATE;
		mo->target = player->mo;

		// Make rings spill out around the player in 16 directions like SA, but spill like Sonic 2.
		// Technically a non-SA way of spilling rings. They just so happen to be a little similar.
		fixed_t spd;
		if (i > 15)
		{
			spd = 3*FRACUNIT;

			P_SetObjectMomZ(mo, 4*FRACUNIT, false);

			if (i & 1)
				P_SetObjectMomZ(mo, 4*FRACUNIT, true);
		}
		else
		{
			spd = 2*FRACUNIT;

			P_SetObjectMomZ(mo, 3*FRACUNIT, false);

			if (i & 1)
				P_SetObjectMomZ(mo, 3*FRACUNIT, true);
		}

		P_Thrust(mo, fa, spd);
	}

	player->lossCount += 2;

	if (player->lossCount > 6)
		player->lossCount = 6;
}

/*============================================================================= */

void P_PlayerMove(mobj_t *mo)
{
	fixed_t momx, momy;
	int i;
	pslidemove_t sm;
	ptrymove_t tm;

	momx = vblsinframe * (mo->momx >> 2);
	momy = vblsinframe * (mo->momy >> 2);

	sm.slidething = mo;

	P_SlideMove(&sm);

	if (sm.slidex == mo->x && sm.slidey == mo->y)
		goto stairstep;

	if (P_TryMove(&tm, mo, sm.slidex, sm.slidey))
		goto dospecial;

stairstep:
	if (momx > MAXMOVE)
		momx = MAXMOVE;
	if (momx < -MAXMOVE)
		momx = -MAXMOVE;
	if (momy > MAXMOVE)
		momy = MAXMOVE;
	if (momy < -MAXMOVE)
		momy = -MAXMOVE;

	/* something fucked up in slidemove, so stairstep */

	if (P_TryMove(&tm, mo, mo->x, mo->y + momy))
	{
		mo->momx = 0;
		mo->momy = momy;
		goto dospecial;
	}

	if (P_TryMove(&tm, mo, mo->x + momx, mo->y))
	{
		mo->momx = momx;
		mo->momy = 0;
		goto dospecial;
	}

	mo->momx = mo->momy = 0;

dospecial:
	for (i = 0; i < sm.numspechit; i++)
		P_CrossSpecialLine(sm.spechit[i], mo);
}

/*
===================
=
= P_PlayerXYMovement
=
===================
*/

void P_PlayerXYMovement(mobj_t *mo)
{
	player_t *player = &players[mo->player - 1];
	P_PlayerMove(mo);

	/* */
	/* slow down */
	/* */
	if (player->powers[pw_flashing] != FLASHINGTICS)
	{
		const fixed_t frc = FRACUNIT * 1;
		const fixed_t top = 30 * FRACUNIT;
		const angle_t speedDir = R_PointToAngle2(0, 0, mo->momx, mo->momy);
		fixed_t speed = P_AproxDistance(mo->momx, mo->momy);

		if (speed > STOPSPEED)
		{
			if (!(player->forwardmove || player->sidemove))
			{
				if (speed >= frc)
				{
					fixed_t newSpeedX = 0;
					fixed_t newSpeedY = 0;
					
					if (mo->z > mo->floorz)
						P_ThrustValues(speedDir, -frc / 2, &newSpeedX, &newSpeedY);
					else
						P_ThrustValues(speedDir, -frc, &newSpeedX, &newSpeedY);

					player->mo->momx += newSpeedX;
					player->mo->momy += newSpeedY;
				}
				else
				{
					player->mo->momx = FixedMul(player->mo->momx, FRACUNIT - (STOPSPEED*2));
					player->mo->momy = FixedMul(player->mo->momy, FRACUNIT - (STOPSPEED*2));
				}
			}

			speed = P_AproxDistance(mo->momx, mo->momy);
			if (speed > top)
			{
				const fixed_t diff = speed - top;
				const angle_t opposite = speedDir - ANG180;

				P_Thrust(player->mo, opposite, diff);
			}
		}
		else if (!(player->forwardmove || player->sidemove))
		{
			if (speed < STOPSPEED)
			{
				mo->momx = 0;
				mo->momy = 0;
			}
		}
	}
}

/*
===============
=
= P_PlayerZMovement
=
===============
*/

void P_PlayerZMovement(mobj_t *mo)
{
	player_t *player = &players[mo->player - 1];

	/* */
	/* check for smooth step up */
	/* */
	if (mo->z < mo->floorz)
	{
		player->viewheight -= mo->floorz - mo->z;
		player->deltaviewheight = (VIEWHEIGHT - player->viewheight) >> 2;
	}

	/* */
	/* adjust height */
	/* */
	mo->z += mo->momz;

	/* */
	/* clip movement */
	/* */
	if (mo->z <= mo->floorz)
	{ /* hit the floor */
		if (mo->momz < 0)
		{
			if (mo->momz < -GRAVITY * 2) /* squat down */
			{
				player->deltaviewheight = mo->momz >> 3;
				if (player->playerstate != PST_DEAD)
					S_StartSound(mo, sfx_None);
			}
			mo->momz = 0;
		}
		mo->z = mo->floorz;

		if (mo->momz <= 0)
			P_PlayerHitFloor(player);
	}
	else
	{
		fixed_t gravity = GRAVITY;
		if (mo->momz == 0)
			mo->momz = -gravity;
		else
			mo->momz -= gravity / 2;
	}

	if (mo->z + (mo->theight << FRACBITS) > mo->ceilingz)
	{ /* hit the ceiling */
		if (mo->momz > 0)
			mo->momz = 0;
		mo->z = mo->ceilingz - (mo->theight << FRACBITS);
	}
}

/*
================
=
= P_PlayerMobjThink
=
================
*/

void P_PlayerMobjThink(mobj_t *mobj)
{
	const state_t *st;
	int state;

	/* */
	/* momentum movement */
	/* */
	if (mobj->momx || mobj->momy)
		P_PlayerXYMovement(mobj);

	if ((mobj->z != mobj->floorz) || mobj->momz)
		P_PlayerZMovement(mobj);

	//	if (gametic == prevgametic)
	//		return;

	/* */
	/* cycle through states, calling action functions at transitions */
	/* */
	if (mobj->tics == -1)
		return; /* never cycle */

	mobj->tics--;

	if (mobj->tics > 0)
		return; /* not time to cycle yet */

	state = states[mobj->state].nextstate;
	st = &states[state];

	mobj->state = state;
	mobj->tics = st->tics;
	mobj->frame = st->frame;

	player_t *player = &players[mobj->player - 1];

	if (mobj->state >= S_PLAY_ATK1 && mobj->state <= S_PLAY_ATK5)
	{
		if (player->speed > (13 << FRACBITS))
			mobj->tics = 1;
		else
			mobj->tics = 2;
	}
	else if (P_IsObjectOnGround(mobj))
	{
		if (player->speed >= (25 << FRACBITS) && mobj->state >= S_PLAY_RUN1 && mobj->state <= S_PLAY_RUN8 && !(mobj->state >= S_PLAY_SPD1 && mobj->state <= S_PLAY_SPD4))
			P_SetMobjState(mobj, S_PLAY_SPD1);

		if (mobj->state >= S_PLAY_RUN1 && mobj->state <= S_PLAY_RUN8)
		{
			if (player->speed > (20 << FRACBITS))
				mobj->tics = 1;
			else if (player->speed > (10 << FRACBITS))
				mobj->tics = 2;
			else if (player->speed > (4 << FRACBITS))
				mobj->tics = 3;
			else
				mobj->tics = 4;
		}
		else if (mobj->state >= S_PLAY_SPD1 && mobj->state <= S_PLAY_SPD4)
		{
			if (player->speed > (44 << FRACBITS))
				mobj->tics = 1;
			else
				mobj->tics = 2;
		}

		if (mobj->state >= S_PLAY_SPD1 && mobj->state <= S_PLAY_SPD4 && player->speed < (25 << FRACBITS))
			P_SetMobjState(mobj, S_PLAY_RUN1);
	}
}

/*============================================================================= */

/*
====================
=
= P_BuildMove
=
====================
*/

void P_BuildMove(player_t *player)
{
	int buttons, oldbuttons;
	mobj_t *mo;
	int vbls;

	buttons = ticbuttons[playernum];
	oldbuttons = oldticbuttons[playernum];
	vbls = vblsinframe;

	if (mousepresent && !demoplayback)
	{
		int mx = ticmousex[playernum];
		int my = ticmousey[playernum];

		if ((buttons & BT_RMBTN) && (oldbuttons & BT_RMBTN))
		{
			// holding RMB - mouse dodge mode
			player->sidemove = (mx * 0x1000 * vbls) / TICVBLS;
			player->forwardmove = (my * 0x1000 * vbls) / TICVBLS;
			player->angleturn = 0;
		}
		else
		{
			// normal mouse mode - mouse turns, dpad moves forward/back/sideways
			player->angleturn = (-mx * 0x200000 * vbls) / TICVBLS;

			player->forwardmove = player->sidemove = 0;

			if (buttons & BT_RIGHT)
				player->sidemove += FRACUNIT;
			if (buttons & BT_LEFT)
				player->sidemove -= FRACUNIT;

			if (buttons & BT_UP)
				player->forwardmove += FRACUNIT;
			if (buttons & BT_DOWN)
				player->forwardmove += -FRACUNIT;
		}
	}
	else
	{
		/*  */
		/* use two stage accelerative turning on the joypad  */
		/*  */
		if ((buttons & BT_LEFT) && (oldbuttons & BT_LEFT))
			player->turnheld++;
		else if ((buttons & BT_RIGHT) && (oldbuttons & BT_RIGHT))
			player->turnheld++;
		else
			player->turnheld = 0;
		if (player->turnheld >= SLOWTURNTICS)
			player->turnheld = SLOWTURNTICS - 1;

		player->forwardmove = player->sidemove = player->angleturn = 0;

		if (true) // buttons & BT_STRAFE)
		{
			if (buttons & BT_RIGHT)
				player->sidemove += FRACUNIT;
			if (buttons & BT_LEFT)
				player->sidemove -= FRACUNIT;
		}
		else
		{
			fixed_t *turnspeed;

			if (buttons & BT_STRAFERIGHT)
				player->sidemove += FRACUNIT;
			if (buttons & BT_STRAFELEFT)
				player->sidemove -= FRACUNIT;

			turnspeed = angleturn;
			if (buttons & BT_FASTTURN)
				turnspeed = fastangleturn;

			if (buttons & BT_RIGHT)
				player->angleturn = ((-turnspeed[player->turnheld] * vbls) / TICVBLS) << 17;
			if (buttons & BT_LEFT)
				player->angleturn = ((turnspeed[player->turnheld] * vbls) / TICVBLS) << 17;
		}

		if (buttons & BT_UP)
			player->forwardmove += FRACUNIT;
		if (buttons & BT_DOWN)
			player->forwardmove -= FRACUNIT;
	}

	/* */
	/* if slowed down to a stop, change to a standing frame */
	/* */
	mo = player->mo;

	if (!mo->momx && !mo->momy && player->forwardmove == 0 && player->sidemove == 0)
	{ /* if in a walking frame, stop moving */
		if (mo->state >= S_PLAY_RUN1 && mo->state <= S_PLAY_RUN8)
			P_SetMobjState(mo, S_PLAY_STND);
	}

	if (!(player->forwardmove || player->sidemove))
	{
		if (!(player->mo->momx > STOPSPEED || player->mo->momx < -STOPSPEED || player->mo->momy > STOPSPEED || player->mo->momy < -STOPSPEED || player->mo->momz > STOPSPEED || player->mo->momz < -STOPSPEED))
			player->stillTimer++;
		else
			player->stillTimer = 0;
	}
	else
		player->stillTimer = 0;

	if (P_IsReeling(player))
		player->forwardmove = player->sidemove = 0;
}

/*
===============================================================================

						movement

===============================================================================
*/

boolean onground;

/*
==================
=
= P_CalcHeight
=
= Calculate the walking / running height adjustment
=
==================
*/

void P_CalcHeight(player_t *player)
{
	int angle;
	fixed_t bob;

	// Regular movement bobbing
	// (needs to be calculated for gun swing
	// even if not on ground)
	// OPTIMIZE: tablify angle
	// Note: a LUT allows for effects
	//  like a ramp with low health.

	player->bob = FixedMul(player->mo->momx, player->mo->momx) + FixedMul(player->mo->momy, player->mo->momy);
	player->bob >>= 2;

	if (player->bob > MAXBOB) //   |
		player->bob = MAXBOB; // phares 2/26/98

	if (!onground)
	{
		player->viewz = player->mo->z + VIEWHEIGHT;

		if (player->viewz > player->mo->ceilingz - 4 * FRACUNIT)
			player->viewz = player->mo->ceilingz - 4 * FRACUNIT;

		// The following line was in the Id source and appears      // phares 2/25/98
		// to be a bug. player->viewz is checked in a similar
		// manner at a different exit below.

		//  player->viewz = player->mo->z + player->viewheight;
		return;
	}

	angle = (FINEANGLES / 20 * gametic30) & FINEMASK;
	bob = FixedMul(player->bob / 2, finesine(angle));

	// move viewheight

	if (player->playerstate == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;

		if (player->viewheight > VIEWHEIGHT)
		{
			player->viewheight = VIEWHEIGHT;
			player->deltaviewheight = 0;
		}

		if (player->viewheight < VIEWHEIGHT / 2)
		{
			player->viewheight = VIEWHEIGHT / 2;
			if (player->deltaviewheight <= 0)
				player->deltaviewheight = 1;
		}

		if (player->deltaviewheight)
		{
			player->deltaviewheight += FRACUNIT / 4;
			if (!player->deltaviewheight)
				player->deltaviewheight = 1;
		}
	}

	player->viewz = player->mo->z + player->viewheight + bob;

	if (player->viewz > player->mo->ceilingz - 4 * FRACUNIT)
		player->viewz = player->mo->ceilingz - 4 * FRACUNIT;
}

static void P_DoTeeter(player_t *player)
{
	boolean teeter = false;
	const fixed_t tiptop = 24 * FRACUNIT; // Gotta be farther than the step height

	subsector_t *a = R_PointInSubsector(player->mo->x + 5 * FRACUNIT, player->mo->y + 5 * FRACUNIT);
	subsector_t *b = R_PointInSubsector(player->mo->x - 5 * FRACUNIT, player->mo->y + 5 * FRACUNIT);
	subsector_t *c = R_PointInSubsector(player->mo->x + 5 * FRACUNIT, player->mo->y - 5 * FRACUNIT);
	subsector_t *d = R_PointInSubsector(player->mo->x - 5 * FRACUNIT, player->mo->y - 5 * FRACUNIT);

	if (a->sector->floorheight < player->mo->floorz - tiptop || b->sector->floorheight < player->mo->floorz - tiptop || c->sector->floorheight < player->mo->floorz - tiptop || d->sector->floorheight < player->mo->floorz - tiptop)
		teeter = true;

	if (teeter)
	{
		if (player->mo->state >= S_PLAY_STND && player->mo->state <= S_PLAY_TAP2)
			P_SetMobjState(player->mo, S_PLAY_TEETER1);
	}
	else if (player->mo->state == S_PLAY_TEETER1 || player->mo->state == S_PLAY_TEETER2)
		P_SetMobjState(player->mo, S_PLAY_STND);
}

void P_PlayerHitFloor(player_t *player)
{
	player->pflags &= ~PF_JUMPED;
	player->pflags &= ~PF_STARTJUMP;
	P_SetMobjState(player->mo, S_PLAY_RUN1);

	if (!(player->forwardmove || player->sidemove))
	{
		player->mo->momx >>= 1;
		player->mo->momy >>= 1;
	}
}

static void P_DoJump(player_t *player)
{
	if (player->mo->ceilingz - player->mo->floorz <= (player->mo->theight << FRACBITS) - 1)
		return;

	fixed_t jumpStrength = FixedMul(39 * (FRACUNIT / 4), 1 * FRACUNIT + (FRACUNIT / 2));

	player->mo->momz = jumpStrength;

	// Reduce jump strength when underwater
	if (player->pflags & PF_UNDERWATER)
		player->mo->momz = FixedMul(player->mo->momz, FixedDiv(117 * FRACUNIT, 200 * FRACUNIT));

	player->mo->z++;

	P_SetMobjState(player->mo, S_PLAY_ATK1);
	player->pflags |= PF_JUMPED;
	player->pflags |= PF_STARTJUMP;

	S_StartSound(player->mo, sfx_s3k_62);
}

static void P_DoJumpStuff(player_t *player)
{
	int buttons;

	buttons = ticbuttons[playernum];

	if (buttons & BT_ATTACK)
	{
		if (!(player->downbits & DB_JUMPDOWN) && player->mo->state != S_PLAY_PAIN && P_IsObjectOnGround(player->mo) && !P_IsReeling(player))
		{
			P_DoJump(player);
			player->pflags &= ~PF_THOKKED;
		}
		player->downbits |= DB_JUMPDOWN;
	}
	else
	{
		// If letting go of the jump button while still on ascent, cut the jump height.
		if ((player->pflags & (PF_JUMPED | PF_STARTJUMP)) == (PF_JUMPED | PF_STARTJUMP) && player->mo->momz > 0)
		{
			player->mo->momz >>= 1;
			player->pflags &= ~PF_STARTJUMP;
		}

		player->downbits &= ~DB_JUMPDOWN;
	}
}

fixed_t P_ReturnThrustX(angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;
	return FixedMul(move, finecosine(angle));
}

fixed_t P_ReturnThrustY(angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;
	return FixedMul(move, finesine(angle));
}

void P_ReturnThrust(angle_t angle, fixed_t move, fixed_t *thrustX, fixed_t *thrustY)
{
	angle >>= ANGLETOFINESHIFT;
	*thrustX = FixedMul(move, finecosine(angle));
	*thrustY = FixedMul(move, finesine(angle));
}

// 0 = no controls, or no movement
// 1 = pressing in direction of movement
// 2 = pressing opposite of movement
VINT ControlDirection(player_t *player)
{
	if (!(player->forwardmove || player->sidemove) || !(player->mo->momx || player->mo->momy))
		return 0;

	camera_t *thiscam = &camera;

	angle_t camToPlayer = R_PointToAngle2(thiscam->x, thiscam->y, player->mo->x, player->mo->y);
	fixed_t movedx, movedy;
	P_ReturnThrust(camToPlayer, FRACUNIT, &movedx, &movedy);

	angle_t controlAngle = R_PointToAngle2(player->mo->x, player->mo->y, player->mo->x + movedx, player->mo->y + movedy);
	angle_t angleDiff = controlAngle - player->mo->angle;

	if (angleDiff > ANG180)
		angleDiff = -angleDiff;

	if (angleDiff > ANG90)
		return 2; // backwards
	else
		return 1; // forwards
}

/*
=================
=
= P_MovePlayer
=
=================
*/

void P_MovePlayer(player_t *player)
{
	//	player->mo->angle += player->angleturn;

	P_DoJumpStuff(player);

	/* don't let the player control movement if not onground */
	onground = (player->mo->z <= player->mo->floorz);

	if (player->forwardmove || player->sidemove)
	{
		camera_t *thiscam = &camera;
		fixed_t controlX = 0;
		fixed_t controlY = 0;

		P_ThrustValues(thiscam->angle, player->forwardmove, &controlX, &controlY);
		P_ThrustValues(thiscam->angle - ANG90, player->sidemove, &controlX, &controlY);

		angle_t controlAngle = R_PointToAngle2(0, 0, controlX, controlY);
		//		controlX = controlY = 0;
		//		P_ThrustValues(controlAngle, FRACUNIT, &controlX, &controlY);

		// controlX and controlY are now a unit vector

		/*		P_Thrust(player, thiscam->angle, player->forwardmove);
				P_Thrust(player, thiscam->angle-ANG90, player->sidemove);

				player->mo->angle = R_PointToAngle2(player->mo->x, player->mo->y, player->mo->x + player->mo->momx, player->mo->y + player->mo->momy);*/

		fixed_t acc = 2*FRACUNIT;
//		angle_t speedDir = R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);
		fixed_t speed = P_AproxDistance(player->mo->momx, player->mo->momy);

		//		VINT controlDirection = ControlDirection(player);
		//		if (controlDirection == 2)
		//			acc *= 2;

		//		CONS_Printf("Controldirection is %d", controlDirection);

		if (onground && speed > 0)
		{
			// Here we take your current mom and influence it to slowly change to the direction you wish to travel
			// This avoids the feeling of sliding on ice
			fixed_t moveVecX = 0;
			fixed_t moveVecY = 0;
			P_ThrustValues(controlAngle, speed, &moveVecX, &moveVecY);

			player->mo->momx = FixedMul(player->mo->momx, 28*FRACUNIT);
			player->mo->momy = FixedMul(player->mo->momy, 28*FRACUNIT);
			player->mo->momx += moveVecX;
			player->mo->momy += moveVecY;
			player->mo->momx = FixedDiv(player->mo->momx, 29*FRACUNIT);
			player->mo->momy = FixedDiv(player->mo->momy, 29*FRACUNIT);

			player->mo->angle = R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);
		}

		if (speed < 10*FRACUNIT)
		{
			fixed_t addAmt = FixedDiv(10*FRACUNIT - speed, 10*FRACUNIT);
			acc = FixedMul(acc, FRACUNIT + addAmt);
//			CONS_Printf("AddAmt: %d, speed: %d", addAmt, speed >> FRACBITS);
		}

		fixed_t moveVecX = 0;
		fixed_t moveVecY = 0;
		P_ThrustValues(controlAngle, acc, &moveVecX, &moveVecY);
		player->mo->momx += moveVecX;
		player->mo->momy += moveVecY;

//		CONS_Printf("Acc: %d, MomX: %d, MomY: %d", acc, player->mo->momx >> FRACBITS, player->mo->momy >> FRACBITS);
	}

	if ((player->forwardmove || player->sidemove) && (player->mo->state >= S_PLAY_STND && player->mo->state <= S_PLAY_TAP2))
		P_SetMobjState(player->mo, S_PLAY_RUN1);

	// Make sure you're not teetering when you shouldn't be.
	if ((player->mo->state == S_PLAY_TEETER1 || player->mo->state == S_PLAY_TEETER2) && (player->mo->momx || player->mo->momy || player->mo->momz))
		P_SetMobjState(player->mo, S_PLAY_STND);

	if (!player->mo->momz &&
		((!(player->mo->momx || player->mo->momy) && (player->mo->state == S_PLAY_STND || player->mo->state == S_PLAY_TAP1 || player->mo->state == S_PLAY_TAP2 || player->mo->state == S_PLAY_TEETER1 || player->mo->state == S_PLAY_TEETER2))))
		P_DoTeeter(player);

	player->speed = P_AproxDistance(player->mo->momx, player->mo->momy);
}

/*
=================
=
= P_DeathThink
=
=================
*/

void P_DeathThink(player_t *player)
{
	angle_t angle, delta;

	/* fall to the ground */
	if (player->viewheight > 8 * FRACUNIT)
		player->viewheight -= FRACUNIT;
	onground = (player->mo->z <= player->mo->floorz);
	P_CalcHeight(player);

	if (player->attacker && player->attacker != player->mo)
	{
		angle = R_PointToAngle2(player->mo->x, player->mo->y, player->attacker->x, player->attacker->y);
		delta = angle - player->mo->angle;
		if (delta < ANG5 || delta > (unsigned)-ANG5)
		{ /* looking at killer, so fade damage flash down */
			player->mo->angle = angle;
			if (player->damagecount)
				player->damagecount--;
		}
		else if (delta < ANG180)
			player->mo->angle += ANG5;
		else
			player->mo->angle -= ANG5;
	}
	else if (player->damagecount)
		player->damagecount--;

	if ((ticbuttons[playernum] & BT_USE) && player->viewheight <= 8 * FRACUNIT)
		player->playerstate = PST_REBORN;
}

/*
=================
=
= P_PlayerThink
=
=================
*/

extern int ticphase;

void P_PlayerThink(player_t *player)
{
	int buttons;

	buttons = ticbuttons[playernum];

	ticphase = 20;
	P_PlayerMobjThink(player->mo);

	ticphase = 21;
	P_BuildMove(player);

	if (player->playerstate == PST_DEAD)
	{
		if (player == &players[consoleplayer])
			P_MoveChaseCamera(player, &camera);
		P_DeathThink(player);
		return;
	}

	/* */
	/* move around */
	ticphase = 22;
	P_MovePlayer(player);
	P_CalcHeight(player);

	if (player == &players[consoleplayer])
		P_MoveChaseCamera(player, &camera);

	if (player->mo->subsector->sector->special)
		P_PlayerInSpecialSector(player);

	/* */
	/* check for weapon change */
	/* */
	ticphase = 23;
	if ((buttons & BT_RMBTN))
	{
		// holding the RMB - swap the next and previous weapon actions
		if (buttons & BT_NWEAPN)
		{
			buttons = (buttons ^ BT_NWEAPN) | BT_PWEAPN;
		}
	}

	// CALICO: added support for explicit next weapon and previous weapon actions
	if (buttons & BT_PWEAPN)
	{
		// previous weapon
	}
	else if (buttons & BT_NWEAPN)
	{
		// Next weapon
	}

	/* */
	/* check for use */
	/* */
	if (buttons & BT_USE)
	{
		if (!(player->downbits & DB_SPINDOWN))
		{
			P_UseLines(player);
			player->mo->momz = 16 << FRACBITS;
		}
	}
	else
		player->downbits &= ~DB_SPINDOWN;

	ticphase = 24;
	ticphase = 25;
	ticphase = 26;

	if (player->powers[pw_flashing] && player->powers[pw_flashing] < FLASHINGTICS)
		player->powers[pw_flashing]--;

	if (player->powers[pw_flashing] < FLASHINGTICS && (player->powers[pw_flashing] & 1))
		player->mo->flags2 |= MF2_DONTDRAW;
	else
		player->mo->flags2 &= ~MF2_DONTDRAW;

	/* */
	/* counters */
	/* */
	if (gametic != prevgametic)
	{
		if (player->powers[pw_strength])
			player->powers[pw_strength]++; /* strength counts up to diminish fade */

		if (player->powers[pw_invulnerability])
			player->powers[pw_invulnerability]--;

		if (player->powers[pw_ironfeet])
			player->powers[pw_ironfeet]--;

		if (player->powers[pw_infrared])
			player->powers[pw_infrared]--;

		if (player->damagecount)
			player->damagecount--;

		if (player->bonuscount)
			player->bonuscount--;
	}
}

void R_ResetResp(player_t *p)
{
	int pnum = p - players;
	playerresp_t *resp = &playersresp[pnum];

	D_memset(resp, 0, sizeof(playerresp_t));
	resp->health = 1;
}

void P_RestoreResp(player_t *p)
{
	int pnum = p - players;
	playerresp_t *resp = &playersresp[pnum];

	p->health = resp->health;
	p->armorpoints = resp->armorpoints;
	p->armortype = resp->armortype;
	p->cheats = resp->cheats;
}

void P_UpdateResp(player_t *p)
{
	int pnum = p - players;
	playerresp_t *resp = &playersresp[pnum];

	resp->health = p->health;
	resp->armorpoints = p->armorpoints;
	resp->armortype = p->armortype;
	resp->cheats = p->cheats;
}
