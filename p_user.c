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

void P_KillMobj(mobj_t *source, mobj_t *target);

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

void P_RestoreMusic(player_t *player)
{
	if (player->powers[pw_invulnerability] > 1 && player->powers[pw_extralife] <= 1)
		S_StartSong(gameinfo.invincMus, 0, cdtrack_invincibility);
	else if (player->powers[pw_sneakers] > 1)
		S_StartSong(gameinfo.sneakerMus, 0, cdtrack_sneakers);
	else if (!(player->powers[pw_extralife] > 1))
	{
		int music = gamemapinfo.musicLump;
		if (music <= 0)
			music = S_SongForMapnum(gamemapinfo.mapNumber);
		S_StartSong(music, 1, gamemapinfo.mapNumber);
	}
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
	player->pflags &= ~PF_ELEMENTALBOUNCE;
	player->homingTimer = 0;
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

		// Make rings spill out around the player in 16 directions like SA, but spill like Sonic 2.
		// Technically a non-SA way of spilling rings. They just so happen to be a little similar.
		fixed_t spd;
		if (i > 15)
		{
			spd = 1*FRACUNIT + (FRACUNIT/2);

			P_SetObjectMomZ(mo, 5*FRACUNIT, false);

			if (i & 1)
				P_SetObjectMomZ(mo, 5*FRACUNIT, true);
		}
		else
		{
			spd = 1*FRACUNIT;

			P_SetObjectMomZ(mo, 4*FRACUNIT, false);

			if (i & 1)
				P_SetObjectMomZ(mo, 4*FRACUNIT, true);
		}

		P_Thrust(mo, fa, spd);
	}

	player->lossCount += 2;

	if (player->lossCount > 6)
		player->lossCount = 6;
}

void P_GivePlayerRings(player_t *player, int num_rings)
{
	player->health += num_rings;

	if (player->health > 999)
		player->health = 999;

	if ((player->mo->health < 101 && player->health >= 101 && !player->xtralife)
		|| (player->mo->health < 201 && player->health >= 201 && player->xtralife < 2))
	{
		player->lives++;
		S_StopSong();
		S_StartSong(gameinfo.xtlifeMus, 0, cdtrack_xtlife);
		player->powers[pw_extralife] = EXTRALIFETICS + 1;
		player->xtralife++;
	}
	
	if (player->health >= 256)
		player->mo->health = 255;
	else
		player->mo->health = player->health; // We are limited to mobj_t byte size
}

/*============================================================================= */

void P_PlayerMove(mobj_t *mo)
{
	fixed_t momx, momy;
	int i;
	pslidemove_t sm;
	ptrymove_t tm;

	momx = (mo->momx >> 2);
	momy = (mo->momy >> 2);

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
	fixed_t speed;
	const angle_t speedDir = R_PointToAngle2(0, 0, mo->momx, mo->momy);
	const fixed_t top = (player->pflags & PF_SPINNING) ? 60 * FRACUNIT : 30 *FRACUNIT;
	P_PlayerMove(mo);

	/* */
	/* slow down */
	/* */
	if (player->powers[pw_flashing] != FLASHINGTICS && player->mo->z <= player->mo->floorz)
	{
		const fixed_t frc = (player->pflags & PF_SPINNING) ? FRACUNIT >> 2 : FRACUNIT * 1;
		speed = P_AproxDistance(mo->momx, mo->momy);

		if (speed > STOPSPEED)
		{
			if (!(player->forwardmove || player->sidemove || (player->pflags & PF_GASPEDAL)))
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
		}
		else if (!(player->forwardmove || player->sidemove || (player->pflags & PF_GASPEDAL)))
		{
			if (speed < STOPSPEED)
			{
				mo->momx = 0;
				mo->momy = 0;
			}
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

	if (player->playerstate == PST_DEAD)
	{
		if (player->pflags & PF_UNDERWATER)
			mo->momz -= GRAVITY / 2 / 3;
		else
			mo->momz -= GRAVITY / 2;
		return;
	}

	/* */
	/* clip movement */
	/* */
	if (mo->z <= mo->floorz)
	{ /* hit the floor */
		if (mo->momz < 0)
		{
			if (mo->momz < -GRAVITY * 2) /* squat down */
				player->deltaviewheight = mo->momz >> 3;

			mo->momz = 0;
			P_PlayerHitFloor(player);
		}
		mo->z = mo->floorz;
	}
	else
	{
		if (player->pflags & PF_UNDERWATER)
			mo->momz -= GRAVITY / 2 / 3;
		else
			mo->momz -= GRAVITY / 2;
	}

	if (mo->z + (mo->theight << FRACBITS) > mo->ceilingz)
	{ /* hit the ceiling */
		if (mo->momz > 0)
			mo->momz = 0;
		mo->z = mo->ceilingz - (mo->theight << FRACBITS);
	}

	if (mo->state == S_PLAY_SPRING && mo->momz < 0)
		P_SetMobjState(mo, S_PLAY_FALL1);
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
	player_t *player = &players[mobj->player - 1];
	const state_t *st;
	int state;

	// Make sure player shows dead
	if (mobj->health == 0)
	{
		if (player->pflags & PF_DROWNED)
			P_SetMobjState(mobj, S_PLAY_DROWN);
		else
			P_SetMobjState(mobj, S_PLAY_DIE);

		mobj->flags2 &= ~MF2_DONTDRAW;
		P_PlayerZMovement(mobj);
		if (mobj->z < mobj->floorz - 1024*FRACUNIT)
			mobj->flags2 |= MF2_DONTDRAW;
		return;
	}

	/* */
	/* momentum movement */
	/* */
	if (mobj->momx || mobj->momy)
		P_PlayerXYMovement(mobj);
	else if (!(mobj->flags & MF_NOCLIP))
 		P_PlayerCheckForStillPickups(mobj);

	if ((mobj->z != mobj->floorz) || mobj->momz)
		P_PlayerZMovement(mobj);

	P_MobjCheckWater(mobj);

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

	if (mobj->state >= S_PLAY_FALL1 && mobj->state <= S_PLAY_FALL2)
	{
		const fixed_t absmomz = D_abs(mobj->momz);
		if (absmomz < (20 << FRACBITS))
			mobj->tics = 3;
		else if (absmomz < (30 << FRACBITS))
			mobj->tics = 2;
		else
			mobj->tics = 1;
	}
	else if (mobj->state >= S_PLAY_ATK1 && mobj->state <= S_PLAY_ATK5)
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

	buttons = ticbuttons[playernum];
	oldbuttons = oldticbuttons[playernum];

	if (mousepresent && !demoplayback)
	{
		int mx = ticmousex[playernum];
		int my = ticmousey[playernum];

		if ((buttons & BT_RMBTN) && (oldbuttons & BT_RMBTN))
		{
			// holding RMB - mouse dodge mode
			player->sidemove = (mx * 0x1000);
			player->forwardmove = (my * 0x1000);
			player->angleturn = 0;
		}
		else
		{
			// normal mouse mode - mouse turns, dpad moves forward/back/sideways
			player->angleturn = (-mx * 0x200000);

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
		player->forwardmove = player->sidemove = player->angleturn = 0;

		if (buttons & BT_RIGHT)
			player->sidemove += FRACUNIT;
		if (buttons & BT_LEFT)
			player->sidemove -= FRACUNIT;

		if (buttons & BT_X)
			player->pflags |= PF_GASPEDAL;
		else
			player->pflags &= ~PF_GASPEDAL;

		if (buttons & BT_UP)
			player->forwardmove += FRACUNIT;
		if (buttons & BT_DOWN)
			player->forwardmove -= FRACUNIT;
	}

	/* */
	/* if slowed down to a stop, change to a standing frame */
	/* */
	mo = player->mo;

	if (!mo->momx && !mo->momy && player->forwardmove == 0 && player->sidemove == 0 && !(player->pflags & PF_GASPEDAL) && !(player->pflags & PF_SPINNING))
	{ /* if in a walking frame, stop moving */
		if (mo->state >= S_PLAY_RUN1 && mo->state <= S_PLAY_RUN8)
			P_SetMobjState(mo, S_PLAY_STND);
	}

	const int delaytime = gamemapinfo.act == 3 ? 2*TICRATE : 3*TICRATE;
	if (leveltime > delaytime)
	{
		if (!(player->forwardmove || player->sidemove || (player->pflags & PF_GASPEDAL)))
		{
			if (!(player->mo->momx > STOPSPEED || player->mo->momx < -STOPSPEED || player->mo->momy > STOPSPEED || player->mo->momy < -STOPSPEED || player->mo->momz > STOPSPEED || player->mo->momz < -STOPSPEED))
				player->stillTimer++;
			else
				player->stillTimer = 0;
		}
		else
			player->stillTimer = 0;
	}
	else if (leveltime > delaytime - TICRATE)
		player->stillTimer++;

	player->buttons = buttons;

	boolean killControls = P_IsReeling(player) || player->justSprung
		|| player->exiting || leveltime < delaytime + TICRATE;

	if (killControls)
	{
		player->forwardmove = player->sidemove = 0;
		player->pflags &= ~PF_GASPEDAL;
		player->buttons = 0;
	}
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

	angle = (FINEANGLES / 20 * gametic) & FINEMASK;
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

void P_ResetScore(player_t *player)
{
	player->scoreAdd = 0;
}

static void P_GivePlayerLives(player_t *player, int lives)
{
	player->lives += lives;

	if (player->lives > 99)
		player->lives = 99;
}

void P_AddPlayerScore(player_t *player, int amount)
{
	int oldScore = player->score;

	// check for extra lives every 50,000 points
	if (player->score > oldScore && player->score % 50000 < amount)
	{
		P_GivePlayerLives(player, (player->score / 50000) - (oldScore / 50000));

		// Start extra life music
		S_StopSong();
		S_StartSong(gameinfo.xtlifeMus, 0, cdtrack_xtlife);
		player->powers[pw_extralife] = EXTRALIFETICS + 1;
	}

	player->score += amount;
}

static inline fixed_t P_GetPlayerSpinHeight()
{
	return FixedMul(mobjinfo[MT_PLAYER].height, 2*FRACUNIT/3);
}

static inline fixed_t P_GetPlayerHeight()
{
	return mobjinfo[MT_PLAYER].height;
}

static void P_DoSpinDash(player_t *player)
{
	const int buttons = player->buttons;

	if (!player->exiting && !(player->mo->state == mobjinfo[player->mo->type].painstate && player->powers[pw_flashing]))
	{
		if ((buttons & BT_USE) && player->speed < 5*FRACUNIT && !player->mo->momz && onground && !(player->pflags & PF_USEDOWN) && !(player->pflags & PF_SPINNING))
		{
			P_ResetScore(player);
			player->mo->momx = 0; // TODO: cmomx/cmomy
			player->mo->momy = 0;
			player->pflags |= PF_STARTDASH;
			player->pflags |= PF_SPINNING;
			player->dashSpeed = 1;
			P_SetMobjState(player->mo, S_PLAY_DASH1);
			S_StartSound(player->mo, sfx_s3k_ab);
			player->pflags |= PF_USEDOWN;
			if (player->pflags & PF_VERTICALFLIP)
				player->mo->z = player->mo->ceilingz - P_GetPlayerSpinHeight();
		}
		else if ((buttons & BT_USE) && (player->pflags & PF_STARTDASH))
		{
			if (player->speed > 5*FRACUNIT)
			{
				player->pflags &= ~PF_STARTDASH;
				player->pflags |= PF_SPINNING;
				P_SetMobjState(player->mo, S_PLAY_DASH1);
				S_StartSound(player->mo, sfx_s3k_3c);
				if (player->pflags & PF_VERTICALFLIP)
					player->mo->z = player->mo->ceilingz - P_GetPlayerSpinHeight();

				return;
			}

			int soundplay = (6*player->dashSpeed) / 60;
			player->dashSpeed++;
			if (player->dashSpeed > 60)
				player->dashSpeed = 60;

			if ((6*player->dashSpeed) / 60 != soundplay)
				S_StartSound(player->mo, sfx_s3k_ab);
		}
		// If not moving up or down, and travelling faster than a speed of four while not holding
		// down the spin button and not spinning.
		// AKA Just go into a spin on the ground, you idiot. ;)
		else if ((buttons & BT_USE) && !player->mo->momz && onground && player->speed > 5*FRACUNIT && !(player->pflags & PF_USEDOWN) && !(player->pflags & PF_SPINNING))
		{
			P_ResetScore(player);
			player->pflags |= PF_SPINNING;
			P_SetMobjState(player->mo, S_PLAY_ATK1);
			S_StartSound(player->mo, sfx_s3k_3c);
			player->pflags |= PF_USEDOWN;
			if (player->pflags & PF_VERTICALFLIP)
				player->mo->z = player->mo->ceilingz - P_GetPlayerSpinHeight();
		}

		if (onground && (player->pflags & PF_SPINNING) && !(player->pflags & PF_STARTDASH)
			&& player->speed < 5*FRACUNIT)
		{
			if (player->mo->ceilingz - player->mo->floorz < P_GetPlayerHeight())
				P_InstaThrust(player->mo, player->mo->angle, 8*FRACUNIT);
			else
			{
				player->pflags &= ~PF_SPINNING;
				P_SetMobjState(player->mo, S_PLAY_STND);
//				player->mo->momx = 0; // TODO: cmomx/cmomy
//				player->mo->momy = 0;
				P_ResetScore(player);
				if (player->pflags & PF_VERTICALFLIP)
					player->mo->z = player->mo->ceilingz - P_GetPlayerHeight();
			}
		}

		// Catapult the player from a spindash rev!
		if (onground && !(player->pflags & PF_USEDOWN) && player->dashSpeed && (player->pflags & PF_STARTDASH) && (player->pflags & PF_SPINNING))
		{
			player->pflags &= ~PF_STARTDASH;
			P_InstaThrust(player->mo, player->mo->angle, player->dashSpeed << FRACBITS); // catapult forward ho!!
			S_StartSound(player->mo, sfx_s3k_b6);
			player->dashSpeed = 0;
		}

		if (onground && (player->pflags & PF_SPINNING) && !(player->pflags & PF_STARTDASH)
			&& !(player->mo->state >= S_PLAY_ATK1
			&& player->mo->state <= S_PLAY_ATK5))
			P_SetMobjState(player->mo, S_PLAY_ATK1);

		if (onground && (player->pflags & PF_STARTDASH) && (leveltime & 7))
		{
			fixed_t dustx = player->mo->x;
			fixed_t dusty = player->mo->y;
			P_ThrustValues(player->mo->angle + ANG90 + ANG45, mobjinfo[MT_PLAYER].radius, &dustx, &dusty);

			mobj_t *dust = P_SpawnMobj(dustx, dusty, player->mo->z, MT_GHOST);
			dust->flags2 |= MF2_NARROWGFX;
			P_SetMobjState(dust, S_SPINDUST1);
			P_Thrust(dust, player->mo->angle, -4*FRACUNIT);

			if (player->pflags & PF_VERTICALFLIP)
				dust->z = player->mo->z + (player->mo->theight << FRACBITS) - 8*FRACUNIT;

			dustx = player->mo->x;
			dusty = player->mo->y;
			P_ThrustValues(player->mo->angle - ANG90 - ANG45, mobjinfo[MT_PLAYER].radius, &dustx, &dusty);

			dust = P_SpawnMobj(dustx, dusty, player->mo->z, MT_GHOST);
			dust->flags2 |= MF2_NARROWGFX;
			P_SetMobjState(dust, S_SPINDUST1);
			P_Thrust(dust, player->mo->angle, -4*FRACUNIT);

			if (player->pflags & PF_VERTICALFLIP)
				dust->z = player->mo->z + (player->mo->theight << FRACBITS) - 8*FRACUNIT;
		}
	}
}

void P_PlayerHitFloor(player_t *player)
{
	if (player->pflags & PF_JUMPED)
		player->pflags &= ~PF_SPINNING;
	else if (!(player->pflags & PF_USEDOWN))
		player->pflags &= ~PF_SPINNING;

	if (!((player->pflags & PF_SPINNING) && (player->pflags & PF_USEDOWN)))
	{
		P_ResetScore(player);
		P_SetMobjState(player->mo, S_PLAY_RUN1);
	}

	player->pflags &= ~PF_JUMPED;
	player->pflags &= ~PF_STARTJUMP;
	player->pflags &= ~PF_THOKKED;
	player->pflags &= ~PF_ELEMENTALBOUNCE;

	if (!(player->pflags & PF_SPINNING) && !(player->forwardmove || player->sidemove || (player->pflags & PF_GASPEDAL)))
	{
		player->mo->momx >>= 1;
		player->mo->momy >>= 1;
	}

	player->homingTimer = 0;
}

static void P_DoJump(player_t *player)
{
	if (player->mo->ceilingz - player->mo->floorz <= (player->mo->theight << FRACBITS) - 1)
		return;

	if (!(player->pflags & PF_SPINNING))
		P_ResetScore(player);

	fixed_t jumpStrength = 12*FRACUNIT;

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

boolean P_LookForTarget(player_t *player)
{
	const fixed_t targetMaxDist = 512*FRACUNIT;
	const angle_t oldAngle = player->mo->angle;
	mobj_t *node;
	mobj_t *best = NULL;
	fixed_t bestDist = D_MAXINT;

	for (node = mobjhead.next; node != (void*)&mobjhead; node = node->next)
    {
		if (node->flags & MF_RINGMOBJ)
			continue;

		if (node->type == MT_PLAYER)
			continue;

		if (node->flags & MF_STATIC)
		{
			if (!(node->type >= MT_YELLOWSPRING && node->type <= MT_REDHORIZ)
				&& !(node->flags2 & MF2_SHOOTABLE))
				continue;
		}
		else
		{
			// Enemies and springs only (for now)
			if (!((node->flags2 & MF2_ENEMY) || (node->flags2 & MF2_SHOOTABLE)))
				continue;

			// Ignore fretting bosses
			if (node->flags2 & MF2_FRET)
				continue;

			if (node->health <= 0)
				continue; // dead
		}

		if (node->z > player->mo->z + (player->mo->theight << FRACBITS))
			continue;

		fixed_t dist = P_AproxDistance(P_AproxDistance(node->x - player->mo->x, node->y - player->mo->y), node->z - player->mo->z);

		if (dist > targetMaxDist)
			continue;

		if (dist > bestDist)
			continue;

		// Well, it's the closest, but is it in front of us?
		angle_t ang = R_PointToAngle2(player->mo->x, player->mo->y, node->x, node->y) - player->mo->angle;

		if (ang > ANG90 && ang < ANG270)
			continue;

		player->mo->angle = R_PointToAngle2(player->mo->x, player->mo->y, node->x, node->y);

//		if (!P_CheckSight(player->mo, node))
//			continue; // Can't see it

		best = node;
		bestDist = dist;
    }

	if (!best)
	{
		player->mo->angle = oldAngle;
		return false;
	}

	player->mo->target = best;
	return true;
}

static void P_HomingAttack(mobj_t *source, mobj_t *dest)
{
	if (!dest)
		return;

	if (dest->health <= 0)
		return;

	source->angle = R_PointToAngle2(source->x, source->y, dest->x, dest->y);

	fixed_t dist = P_AproxDistance(P_AproxDistance(dest->x - source->x, dest->y - source->y), dest->z - source->z);

	if (dist < 1) // Don't divide by zero
		dist = 1;

	const fixed_t ns = 30*FRACUNIT;
	source->momx = FixedMul(FixedDiv(dest->x - source->x, dist), ns);
	source->momy = FixedMul(FixedDiv(dest->y - source->y, dist), ns);
	source->momz = FixedMul(FixedDiv(dest->z - source->z, dist), ns);
}

static void P_DoJumpStuff(player_t *player)
{
	const int buttons = player->buttons;

	if (buttons & BT_ATTACK)
	{
		if (!(player->pflags & PF_JUMPDOWN) && player->mo->state != S_PLAY_PAIN && P_IsObjectOnGround(player->mo) && !P_IsReeling(player))
		{
			P_DoJump(player);
			player->pflags &= ~PF_THOKKED;
			player->pflags &= ~PF_ELEMENTALBOUNCE;
		}
		else if (!(player->pflags & PF_JUMPDOWN) && (player->pflags & PF_JUMPED) && !(player->pflags & PF_THOKKED))
		{
			// Find a nearby enemy.
			if (P_LookForTarget(player))
			{
				player->homingTimer = 2*TICRATE * 2;
				S_StartSound(player->mo, sfx_thok);
				player->pflags &= ~PF_SPINNING;
				player->pflags &= ~PF_STARTDASH;
				player->pflags |= PF_THOKKED;
			}
			else
				S_StartSound(player->mo, sfx_ngskid);
		}

		player->pflags |= PF_JUMPDOWN;
	}
	else
	{
		// If letting go of the jump button while still on ascent, cut the jump height.
		if ((player->pflags & (PF_JUMPED | PF_STARTJUMP)) == (PF_JUMPED | PF_STARTJUMP) && player->mo->momz > 0)
		{
			player->mo->momz >>= 1;
			player->pflags &= ~PF_STARTJUMP;
		}

		player->pflags &= ~PF_JUMPDOWN;
	}
}

void P_DoPlayerExit(player_t *player)
{
	if (player->exiting)
		return;

	// Begin the exiting timer so the player doesn't move around
	player->exiting = 1;

	if (gamemapinfo.act == 3)
	{
		// Find the egg capsule, and pop it
		VINT outerNum = P_FindSectorWithTag(254, -1);
		VINT innerNum = P_FindSectorWithTag(255, -1);
		mobj_t *spawnPoint = P_FindFirstMobjOfType(MT_EGGTRAP);

		if (innerNum == -1 || outerNum == -1 || !spawnPoint)
			return;

		sector_t *inner = &sectors[innerNum];
		sector_t *outer = &sectors[outerNum];
		spawnPoint->movecount = 1;
		
		floormove_t *floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC);
		P_AddThinker (&floor->thinker);
		inner->specialdata = floor;
		floor->thinker.function = T_MoveFloor;
		floor->type = eggCapsuleInnerPop;
		floor->crush = false;
		floor->direction = -1;
		floor->sector = inner;
		floor->speed = 2*FRACUNIT;
		floor->floordestheight = 
			(inner->floorheight>>FRACBITS) - 8;

		outer->floorheight -= 64*FRACUNIT;

		for (int i = 0; i < numsides; i++)
		{
			if (sides[i].sector != outer-sectors)
				continue;

			// Found a sidedef for outer.
			// Find a linedef so we can also get the back sidedef,
			// which is what we need to change.
			for (int j = 0; j < numlines; j++)
			{
				if (lines[j].sidenum[0] != i)
					continue;

				if (lines[j].sidenum[1] == -1)
					continue;

				// Get the other side
				lines[j].flags &= ~ML_DONTPEGTOP;
				lines[j].flags |= ML_DONTPEGBOTTOM;
				sides[lines[j].sidenum[1]].rowoffset = 96;
			}
		}

		return;		
	}

	// Find the MT_SIGN
	mobj_t *sign = P_FindFirstMobjOfType(MT_SIGN);
	if (!sign)
		return;

	if (sign->state != mobjinfo[sign->type].spawnstate)
		return;

	sign->target = player->mo;
	
	// Chuck it up into the air
	P_SetObjectMomZ(sign, 12*FRACUNIT, false);
	P_SetMobjState(sign, mobjinfo[sign->type].seestate);
	S_StartSound(sign, mobjinfo[sign->type].seesound);
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
	fixed_t speed = P_AproxDistance(player->mo->momx, player->mo->momy);

	if (player->homingTimer || (player->powers[pw_sneakers] && speed > 20*FRACUNIT))
	{
		mobj_t *ghost = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_GHOST);
		P_SetMobjState(ghost, player->mo->state);
		ghost->angle = player->mo->angle;
		ghost->reactiontime = 6;
	}

	/* don't let the player control movement if not onground */
	onground = (player->mo->z <= player->mo->floorz);

	if (player->forwardmove || player->sidemove || (player->pflags & PF_GASPEDAL))
	{
		camera_t *thiscam = &camera;
		fixed_t controlX = 0;
		fixed_t controlY = 0;

		P_ThrustValues(thiscam->angle, player->forwardmove, &controlX, &controlY);
		P_ThrustValues(thiscam->angle - ANG90, player->sidemove, &controlX, &controlY);

		angle_t controlAngle = R_PointToAngle2(0, 0, controlX, controlY);

		if (player->pflags & PF_STARTDASH)
		{
			player->mo->angle = controlAngle;
		}
		else if (player->pflags & PF_SPINNING)
		{
			// Gas pedal does not apply here
			if (player->forwardmove || player->sidemove)
			{
				fixed_t acc = FRACUNIT >> 3;
				if (player->powers[pw_sneakers])
					acc <<= 1;

				if (player->powers[pw_underwater])
					acc >>= 2;

				controlAngle = R_PointToAngle2(0, 0, controlX, controlY);

				P_ThrustValues(controlAngle, acc, &player->mo->momx, &player->mo->momy);
				angle_t moveAngle = R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);
				
				// Clip to current speed
				if (P_AproxDistance(player->mo->momx, player->mo->momy) > speed)
				{
					const fixed_t diff = P_AproxDistance(player->mo->momx, player->mo->momy) - speed;

					P_ThrustValues(moveAngle - ANG180, diff, &player->mo->momx, &player->mo->momy);
				}
			}
		}
		else
		{
			if (player->pflags & PF_GASPEDAL)
			{
				// When pressing a directional control, the gas has quarter influence.
				if (!(player->forwardmove || player->sidemove))
				{
					P_ThrustValues(thiscam->angle/*player->mo->angle*/, FRACUNIT, &controlX, &controlY);
					controlAngle = R_PointToAngle2(0, 0, controlX, controlY);
				}
				else
				{
					P_ThrustValues(thiscam->angle/*player->mo->angle*/, FRACUNIT / 4, &controlX, &controlY);
					controlAngle = R_PointToAngle2(0, 0, controlX, controlY);
				}
			}

			/*		controlX = controlY = 0;
			P_ThrustValues(controlAngle, FRACUNIT, &controlX, &controlY);
			controlX and controlY are now a unit vector */

			/*		P_Thrust(player, thiscam->angle, player->forwardmove);
					P_Thrust(player, thiscam->angle-ANG90, player->sidemove);

					player->mo->angle = R_PointToAngle2(player->mo->x, player->mo->y, player->mo->x + player->mo->momx, player->mo->y + player->mo->momy);*/

			fixed_t acc = 6144 * 4;//FRACUNIT / 2;
			if (player->powers[pw_sneakers])
				acc <<= 1;
			if (player->powers[pw_underwater])
				acc >>= 2;
	//		angle_t speedDir = R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);

			if (!(player->pflags & PF_GASPEDAL) && !(player->pflags & PF_JUMPED))
			{
				VINT controlDirection = ControlDirection(player);
				if (controlDirection == 2)
					acc *= 2;
			}

			//		CONS_Printf("Controldirection is %d", controlDirection);

			if (player->pflags & PF_JUMPED)
			{
				player->mo->angle = controlAngle;
			}
			else if (onground && speed > 0)
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
	}

	if ((player->forwardmove || player->sidemove || (player->pflags & PF_GASPEDAL)) && (player->mo->state >= S_PLAY_STND && player->mo->state <= S_PLAY_TAP2))
		P_SetMobjState(player->mo, S_PLAY_RUN1);

	// Make sure you're not teetering when you shouldn't be.
	if ((player->mo->state == S_PLAY_TEETER1 || player->mo->state == S_PLAY_TEETER2) && (player->mo->momx || player->mo->momy || player->mo->momz))
		P_SetMobjState(player->mo, S_PLAY_STND);

	if (!player->mo->momz &&
		((!(player->mo->momx || player->mo->momy) && (player->mo->state == S_PLAY_STND || player->mo->state == S_PLAY_TAP1 || player->mo->state == S_PLAY_TAP2 || player->mo->state == S_PLAY_TEETER1 || player->mo->state == S_PLAY_TEETER2))))
		P_DoTeeter(player);

	////////////////////////////
	//SPINNING AND SPINDASHING//
	////////////////////////////

	// If the player isn't on the ground, make sure they aren't in a "starting dash" position.
	if (!onground)
	{
		player->pflags &= ~PF_STARTDASH;
		player->dashSpeed = 0;
	}

	P_DoSpinDash(player);

	P_DoJumpStuff(player);

	// Less height while spinning. Good for spinning under things...?
	if ((player->mo->state == mobjinfo[MT_PLAYER].painstate)
		|| ((player->pflags & PF_SPINNING) || (player->pflags & PF_JUMPED)))
		player->mo->theight = P_GetPlayerSpinHeight() >> FRACBITS;
	else
		player->mo->theight = P_GetPlayerHeight() >> FRACBITS;

	// Check for crushing
	if (player->mo->ceilingz - player->mo->floorz < player->mo->theight << FRACBITS)
	{
		if (!(player->pflags & PF_SPINNING))
		{
			P_ResetScore(player);
			player->pflags |= PF_SPINNING;
			P_SetMobjState(player->mo, S_PLAY_ATK1);
		}
		else
		{
			// TODO: Die!

			if (player->playerstate == PST_DEAD)
				return;
		}
	}

	if ((player->pflags & PF_THOKKED) && player->homingTimer > 0)
	{
		if (player->mo->target)
		{
			if (player->mo->target->health <= 0)
			{
				player->mo->momx >>= 1;
				player->mo->momy >>= 1;
				player->mo->momz >>= 1;
				player->homingTimer = 0;
				player->pflags &= ~PF_THOKKED;
				player->pflags &= ~PF_ELEMENTALBOUNCE;

				if (player->mo->momz < 8*FRACUNIT)
					player->mo->momz = 8*FRACUNIT;
			}
			else
				P_HomingAttack(player->mo, player->mo->target);
		}
		else
			player->pflags &= ~PF_THOKKED;
	}

	player->speed = P_AproxDistance(player->mo->momx, player->mo->momy);

	// If you're running fast enough, you can create splashes as you run in shallow water.
	if (subsectors[player->mo->isubsector].sector->heightsec != -1)
	{
		const fixed_t watertop = GetWatertopMo(player->mo);

		if (player->mo->z + (player->mo->theight << FRACBITS) >= watertop && player->mo->z <= watertop && (player->speed > (25<<FRACBITS) || (player->pflags & PF_STARTDASH))
			&& leveltime % (TICRATE/6) == 0 && player->mo->momz == 0)
		{
			mobj_t *water = P_SpawnMobj(player->mo->x, player->mo->y, watertop, MT_SPLISH);
			water->flags2 |= MF2_FORWARDOFFSET;
			S_StartSound(water, sfx_wslap);
		}
	}

	// Underwater bubbles
	if (player->pflags & PF_UNDERWATER)
	{
		const fixed_t zh = player->mo->z + FixedDiv(player->mo->theight << FRACBITS, 5*(FRACUNIT/4));

		if (!(P_Random() % 16))
			P_SpawnMobj(player->mo->x, player->mo->y, zh, MT_SMALLBUBBLE);
		else if (!(P_Random() % 96))
			P_SpawnMobj(player->mo->x, player->mo->y, zh, MT_MEDIUMBUBBLE);
	}

	if (player->buttons & BT_USE)
	{
		if (!(player->pflags & PF_USEDOWN))
		{
			player->pflags |= PF_USEDOWN;

			if (player->pflags & PF_JUMPED)
			{
				if (player->shield == SH_ARMAGEDDON)
					P_BlackOw(player);
				else if (!(player->pflags & PF_THOKKED) && player->shield == SH_WHIRLWIND)
				{
					player->pflags |= PF_THOKKED;
					player->pflags &= ~(PF_JUMPED|PF_SPINNING|PF_STARTJUMP);
					player->mo->momz = 12*FRACUNIT;
					P_SetMobjState(player->mo, S_PLAY_FALL1);
					S_StartSound(player->mo, sfx_wdjump);
				}
				else if (!(player->pflags & PF_THOKKED) && (player->shield == SH_FORCE2 || player->shield == SH_FORCE1))
				{
					player->pflags |= PF_THOKKED;
					player->mo->momx = player->mo->momy = 0;
					S_StartSound(player->mo, sfx_ngskid);
					mobj_t *ghost = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_GHOST);
					ghost->flags2 |= MF2_NARROWGFX;
					P_SetMobjState(ghost, S_FORCESTOP);
				}
				else if (!(player->pflags & PF_ELEMENTALBOUNCE) && player->shield == SH_ELEMENTAL)
				{
					player->pflags |= PF_THOKKED;
					player->pflags |= PF_ELEMENTALBOUNCE;
					player->pflags &= ~PF_SPINNING;
					player->mo->momx = player->mo->momy = 0;
					P_SetObjectMomZ(player->mo, -28*FRACUNIT, false);

					S_StartSound(player->mo, sfx_s3k_43);
				}
			}
		}
	}
	else
		player->pflags &= ~PF_USEDOWN;

	// Fly cheat
	if (player->buttons & BT_SPEED)
		player->mo->momz = 8 << FRACBITS;
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

	if (player->deadTimer == 2*TICRATE)
	{
		player->lives--;

		if (player->lives == 0)
			S_StartSong(gameinfo.gameoverMus, false, cdtrack_gameover);
	}

	if (player->lives == 0 && player->deadTimer > 15*TICRATE - (TICRATE/3))
	{
		fadetime = (TICRATE/3) - (15*TICRATE - player->deadTimer);
		if (fadetime > TICRATE/3)
			fadetime = TICRATE/3;
	}

	if (player->lives > 0 && player->deadTimer > 5*TICRATE - (TICRATE/3))
	{
		fadetime = (TICRATE/3) - (5*TICRATE - player->deadTimer);
		if (fadetime > TICRATE/3)
			fadetime = TICRATE/3;
	}

	if (player->deadTimer > 5*TICRATE && player->lives > 0 && player->mo->z < player->mo->floorz)
		player->playerstate = PST_REBORN;
	else if (player->deadTimer > 15*TICRATE && player->lives == 0)
	{
		gameaction = ga_backtotitle;
	}

	player->deadTimer++;
}

/*
=================
=
= P_PlayerThink
=
=================
*/

//
// P_CheckInvincibilityTimer
//
// Restores music from invincibility, and handles invincibility sparkles
//
static void P_CheckInvincibilityTimer(player_t *player)
{
	if (!player->powers[pw_invulnerability])
		return;

	if (leveltime % (TICRATE/6) == 0)
		P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_IVSP);

	// Resume normal music stuff.
	if (player->powers[pw_invulnerability] == 1)
		P_RestoreMusic(player);
}

//
// P_CheckUnderwaterAndSpaceTimer
//
// Restores music from underwater and space warnings, and handles number generation
//
static void P_CheckUnderwaterAndSpaceTimer(player_t *player)
{
	VINT timeleft = ((player->powers[pw_spacetime]) ? player->powers[pw_spacetime] : player->powers[pw_underwater]) - 1;

	if (player->exiting)
		player->powers[pw_underwater] = player->powers[pw_spacetime] = 0;

	if ((timeleft == 11*TICRATE) // 5
	 || (timeleft ==  9*TICRATE) // 4
	 || (timeleft ==  7*TICRATE) // 3
	 || (timeleft ==  5*TICRATE) // 2
	 || (timeleft ==  3*TICRATE) // 1
	 || (timeleft ==  1*TICRATE)) // 0
	 {
		fixed_t height = (player->pflags & PF_VERTICALFLIP)
		? player->mo->z - 8*FRACUNIT + mobjinfo[MT_DROWNNUMBERS].height
		: player->mo->z + (player->mo->theight << FRACBITS) + 8*FRACUNIT;

		mobj_t *numbermobj = P_SpawnMobj(player->mo->x, player->mo->y, height, MT_DROWNNUMBERS);
		
		timeleft /= (2*TICRATE);

		S_StartSound(player->mo, sfx_dwnind);

		if (timeleft) // Don't waste time setting the state if the time is 0.
			P_SetMobjState(numbermobj, mobjinfo[numbermobj->type].spawnstate+timeleft);

		numbermobj->target = player->mo;
		numbermobj->threshold = 40;
	}
	// Underwater timer runs out
	else if (timeleft == 1)
	{
		player->pflags |= PF_DROWNED;
		P_KillMobj(NULL, player->mo);
	}

	if (!(player->pflags & PF_UNDERWATER) && player->powers[pw_underwater])
	{
		if (player->powers[pw_underwater] <= 12*TICRATE + 1)
			P_RestoreMusic(player);

		player->powers[pw_underwater] = 0;
	}

//	if (player->powers[pw_spacetime] > 1 && !P_InSpaceSector(player->mo))
//		player->powers[pw_spacetime] = 0;

	// Underwater audio cues
	if ((player->powers[pw_underwater] == 25*TICRATE + 1)
		|| (player->powers[pw_underwater] == 20*TICRATE + 1)
		|| (player->powers[pw_underwater] == 15*TICRATE + 1))
		S_StartSound(NULL, sfx_s3k_a9);

	// Drowning music
	if (player->powers[pw_underwater] == 12*TICRATE + 1)
	{
		S_StopSong();
		S_StartSong(gameinfo.drowningMus, 0, cdtrack_drowning);
	}
}

extern int ticphase;
void P_RingMagnet(mobj_t *spot);

void P_PlayerThink(player_t *player)
{
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

	if (subsectors[player->mo->isubsector].sector->special)
		P_PlayerInSpecialSector(player);

	ticphase = 23;
	ticphase = 24;
	ticphase = 25;
	ticphase = 26;

	if (player->powers[pw_flashing] && player->powers[pw_flashing] < FLASHINGTICS)
		player->powers[pw_flashing]--;

	if (player->powers[pw_flashing] < FLASHINGTICS && (player->powers[pw_flashing] & 1))
		player->mo->flags2 |= MF2_DONTDRAW;
	else
		player->mo->flags2 &= ~MF2_DONTDRAW;

	if (player->powers[pw_extralife] > 0)
	{
		player->powers[pw_extralife]--;
		if (player->powers[pw_extralife] == 1)
			P_RestoreMusic(player);
	}

	/* */
	/* counters */
	/* */
	if (player->powers[pw_sneakers])
	{
		player->powers[pw_sneakers]--;
		if (player->powers[pw_sneakers] == 1)
			P_RestoreMusic(player);
	}

	if (player->powers[pw_invulnerability])
		player->powers[pw_invulnerability]--;

	if (player->powers[pw_underwater])
		player->powers[pw_underwater]--;

	if (player->damagecount)
		player->damagecount--;

	if (player->whiteFlash && (leveltime & 1))
		player->whiteFlash--;

	if (player->justSprung)
		player->justSprung--;

	if (player->homingTimer)
		player->homingTimer--;

	if (player->exiting)
		player->exiting++;

	if (player->shield == SH_ATTRACT)
		P_RingMagnet(player->mo);
	else if (player->shield == SH_ELEMENTAL)
		player->powers[pw_underwater] = UNDERWATERTICS + 1;

	P_CheckUnderwaterAndSpaceTimer(player);
	P_CheckInvincibilityTimer(player);
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
	p->cheats = resp->cheats;
}

void P_UpdateResp(player_t *p)
{
	int pnum = p - players;
	playerresp_t *resp = &playersresp[pnum];

	resp->health = p->health;
	resp->cheats = p->cheats;
}
