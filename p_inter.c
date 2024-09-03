/* P_inter.c */


#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"

/*
===============================================================================

							GET STUFF

===============================================================================
*/

/*
==================
=
= P_TouchSpecialThing
=
==================
*/

boolean P_CanPickupItem(player_t *player)
{
	if (player->powers[pw_flashing] > (FLASHINGTICS/4)*3 && player->powers[pw_flashing] <= FLASHINGTICS)
		return false;

	return true;
}

static boolean P_DoSpring(mobj_t *spring, player_t *player)
{
	const mobjinfo_t *playerinfo = &mobjinfo[player->mo->type];
	const mobjinfo_t *springinfo = &mobjinfo[spring->type];
	fixed_t vertispeed = springinfo->mass << FRACBITS;
	fixed_t horizspeed = springinfo->damage << FRACBITS;

	if (player->pflags & PF_VERTICALFLIP)
		vertispeed *= -1;

	/*
	player->powers[pw_justsprung] = 5;
	if (horizspeed)
		player->powers[pw_noautobrake] = ((horizspeed*TICRATE)>>(FRACBITS+3))/9; // TICRATE at 72*FRACUNIT
	else if (P_MobjFlip(player->mo) == P_MobjFlip(spring))
		player->powers[pw_justsprung] |= (1<<15);
*/
	if ((horizspeed && vertispeed) || (player && player->homing)) // Mimic SA
	{
		player->mo->momx = player->mo->momy = player->mo->momz = 0;
		P_UnsetThingPosition(player->mo);
		player->mo->x = spring->x;
		player->mo->y = spring->y;
		P_SetThingPosition(player->mo);
		player->forwardmove = player->sidemove = 0;
		player->justSprung = TICRATE;
	}

	if (vertispeed > 0)
		player->mo->z = spring->z + (spring->theight << FRACBITS) + 1;
	else if (vertispeed < 0)
		player->mo->z = spring->z - (player->mo->theight << FRACBITS) - 1;
	else
	{
		fixed_t offx = 0;
		fixed_t offy = 0;
		// Horizontal springs teleport you in FRONT of them.
		player->mo->momx = player->mo->momy = 0;

		// Overestimate the distance to position you at
		P_ThrustValues(spring->angle, (springinfo->radius + playerinfo->radius + 1) * 2, &offx, &offy);

		// Make it square by clipping
		if (offx > (springinfo->radius + playerinfo->radius + 1))
			offx = springinfo->radius + playerinfo->radius + 1;
		else if (offx < -(springinfo->radius + playerinfo->radius + 1))
			offx = -(springinfo->radius + playerinfo->radius + 1);

		if (offy > (springinfo->radius + playerinfo->radius + 1))
			offy = springinfo->radius + playerinfo->radius + 1;
		else if (offy < -(springinfo->radius + playerinfo->radius + 1))
			offy = -(springinfo->radius + playerinfo->radius + 1);

		// Set position!
		P_UnsetThingPosition(player->mo);
		player->mo->x = spring->x + offx;
		player->mo->y = spring->y + offy;
		P_SetThingPosition(player->mo);
	}

	if (vertispeed)
		player->mo->momz = vertispeed;

	if (horizspeed)
		P_InstaThrust(player->mo, spring->angle, horizspeed);

	if (player)
	{
		int pflags;
		uint8_t secondjump;
		boolean washoming;

// TODO: Lots of stuff to update here in the future
//		if (spring->flags & MF_ENEMY) // Spring shells
//			P_SetTarget(&spring->target, object);

		if (horizspeed)
		{
			player->justSprung = 2*TICRATE;
			player->mo->angle = spring->angle;
		}

		{
			boolean wasSpindashing = false;//player->dashspeed > 0;

			pflags = player->pflags & (PF_STARTJUMP | PF_JUMPED | PF_SPINNING | PF_THOKKED); // Save these to restore later.

			if (wasSpindashing) // Ensure we're in the rolling state, and not spindash.
				P_SetMobjState(player->mo, S_PLAY_ATK1);
		}
//		secondjump = player->secondjump;
		washoming = player->homing;
		P_ResetPlayer(player);

		if (!vertispeed)
		{
			if (pflags & (PF_JUMPED|PF_SPINNING))
			{
				player->pflags |= pflags;
//				player->secondjump = secondjump;
			}
			else if (P_IsObjectOnGround(player->mo))
				P_SetMobjState(player->mo, S_PLAY_RUN1); // We'll break into a run on the next tic
			else
				P_SetMobjState(player->mo, (player->mo->momz > 0) ? S_PLAY_SPRING : S_PLAY_FALL1);
		}
		else if (P_MobjFlip(player->mo)*vertispeed > 0)
			P_SetMobjState(player->mo, S_PLAY_SPRING);
		else
			P_SetMobjState(player->mo, S_PLAY_FALL1);
	}

	S_StartSound(player->mo, springinfo->painsound);
	return true;
}

void P_TouchSpecialThing (mobj_t *special, mobj_t *toucher)
{
	player_t	*player;
	int			sound;

	if (!toucher->player)
		return;		

	sound = sfx_None;	
	player = &players[toucher->player - 1];
	if (toucher->health <= 0)
		return;						/* can happen with a sliding player corpse */

	if ((special->flags & MF_SHOOTABLE) && !(special->flags & MF_MISSILE))
	{
		if (special->flags & MF_ENEMY) // enemy rules
		{
			if ((player->pflags & PF_JUMPED) || (player->pflags & PF_SPINNING) || player->powers[pw_invulnerability])
			{
				if (((player->pflags & PF_VERTICALFLIP) && toucher->momz > 0)
					|| (!(player->pflags & PF_VERTICALFLIP) && toucher->momz < 0))
					toucher->momz = -toucher->momz;

				P_DamageMobj(special, toucher, toucher, 1);
			}
			else
				P_DamageMobj(toucher, special, special, 1);
		}
		else // A monitor or something else we can pop
		{
			if ((player->pflags & PF_JUMPED) || (player->pflags & PF_SPINNING))
			{
				if (((player->pflags & PF_VERTICALFLIP) && toucher->momz > 0)
					|| (!(player->pflags & PF_VERTICALFLIP) && toucher->momz < 0))
					toucher->momz = -toucher->momz;

				P_DamageMobj(special, toucher, toucher, 1);
			}
		}
		
		return;
	}
	
	if (special->type == MT_YELLOWSPRING || special->type == MT_YELLOWDIAG || special->type == MT_YELLOWHORIZ
		|| special->type == MT_REDSPRING || special->type == MT_REDDIAG || special->type == MT_REDHORIZ)
	{
		P_DoSpring(special, player);
		return;
	}

	if (!P_CanPickupItem(player))
		return;

	switch(special->type)
	{
		case MT_RING:
			player->lossCount = 0;
		case MT_FLINGRING:
			P_SpawnMobj(special->x, special->y, special->z, MT_SPARK);
			P_GivePlayerRings(player, 1);
			sound = mobjinfo[special->type].deathsound;
		break;

		default:
			break;
	}
	
	if (special->type == MT_RING)
		player->itemcount++;
	P_RemoveMobj (special);

	if (sound <= sfx_None)
		return;

	if (player == &players[consoleplayer] || (splitscreen && player == &players[consoleplayer^1]))
		toucher = NULL;
	S_StartSound (toucher, sound);
}

/*
==============
=
= KillMobj
=
==============
*/

void P_KillMobj (mobj_t *source, mobj_t *target)
{
	const mobjinfo_t* targinfo = &mobjinfo[target->type];

	target->flags &= ~(MF_SHOOTABLE|MF_FLOAT);
	
	if (target->player)
	{
		player_t* player = &players[target->player - 1];
		target->flags &= ~MF_SOLID;
		player->playerstate = PST_DEAD;

		if (netgame == gt_coop)
			R_ResetResp(player);
	}

	if (source->player)
	{
		// Monitors need to know who killed them
		target->target = source;
	}

	if (target->health < -targinfo->spawnhealth
		&& targinfo->xdeathstate)
		P_SetMobjState (target, targinfo->xdeathstate);
	else
		P_SetMobjState (target, targinfo->deathstate);

	target->tics -= P_Random()&1;
	if (target->tics < 1)
		target->tics = 1;

	S_StartSound(target, targinfo->deathsound);

	if (source->player && (target->flags & MF_ENEMY))
	{
		VINT score = 100;
		mobj_t *scoremobj = P_SpawnMobj(target->x, target->y, target->z + (target->theight << (FRACBITS-1)), MT_SCORE);
		statenum_t scoreState = mobjinfo[MT_SCORE].spawnstate;
		player_t* player = &players[source->player - 1];

		player->scoreAdd++;
		if (player->scoreAdd == 1)
		{
			score = 100; // Score! Tails 03-01-2000
		}
		else if (player->scoreAdd == 2)
		{
			score = 200; // Score! Tails 03-01-2000
			scoreState += 1;
		}
		else if (player->scoreAdd == 3)
		{
			score = 500; // Score! Tails 03-01-2000
			scoreState += 2;
		}
		else// if (player->scoreAdd >= 4)
		{
			score = 1000; // Score! Tails 03-01-2000
			scoreState += 3;
		}

		P_SetMobjState(scoremobj, scoreState);
		P_AddPlayerScore(player, score);

		// Spawn a flicky
		const mobjtype_t flickies[4] = { MT_FLICKY_01, MT_FLICKY_02, MT_FLICKY_03, MT_FLICKY_12 };
		mobj_t *flicky = P_SpawnMobj(target->x, target->y, target->z + (target->theight << (FRACBITS-1)), flickies[P_Random() % 4]);
		flicky->momz = 4*FRACUNIT;
		flicky->target = player->mo;
	}
}



/*
=================
=
= P_DamageMobj
=
= Damages both enemies and players
= inflictor is the thing that caused the damage
= 		creature or missile, can be NULL (slime, etc)
= source is the thing to target after taking damage
=		creature or NULL
= Source and inflictor are the same for melee attacks
= source can be null for barrel explosions and other environmental stuff
==================
*/

static void P_DoPlayerPain(player_t *player, mobj_t *source, mobj_t *inflictor)
{
	angle_t ang;

	player->mo->z++; // Lift off the ground a little

	if (player->pflags & PF_UNDERWATER)
		player->mo->momz = FixedDiv(10511*FRACUNIT, 2600*FRACUNIT) * P_MobjFlip(player->mo);
	else
		player->mo->momz = FixedDiv(69*FRACUNIT, 10*FRACUNIT) * P_MobjFlip(player->mo);

	ang = ((player->mo->momx || player->mo->momy) ? R_PointToAngle2(player->mo->momx, player->mo->momy, 0, 0) : player->mo->angle);

	P_InstaThrust(player->mo, ang, 6*FRACUNIT);

	P_ResetPlayer(player);
	P_SetMobjState(player->mo, mobjinfo[player->mo->type].painstate);
	player->powers[pw_flashing] = FLASHINGTICS;
}

static void P_RingDamage(player_t *player, mobj_t *inflictor, mobj_t *source, int damage)
{
	P_PlayerRingBurst(player, damage);
	P_DoPlayerPain(player, source, inflictor);
	S_StartSound(player->mo, mobjinfo[player->mo->type].painsound);
	player->mo->health = player->health = 1;
}

void P_DamageMobj (mobj_t *target, mobj_t *inflictor, mobj_t *source, int damage)
{
	player_t	*player;
	const mobjinfo_t* targinfo = &mobjinfo[target->type];

	if ( !(target->flags & MF_SHOOTABLE) )
		return;						/* shouldn't happen... */
		
	if (target->health <= 0)
		return;
	
	player = target->player ? &players[target->player - 1] : NULL;
	
/* */
/* player specific */
/* */
	if (player)
	{
		if (player->exiting)
			return;

		if (damage == 10000) // magic number for instant death
			P_KillMobj(source, target);
		else if ( damage < 10000 )
		{
			if ( (player->cheats&CF_GODMODE)|| player->powers[pw_invulnerability] || player->powers[pw_flashing] )
				return;

			if (player->mo->health > 1) // Rings without shield
				P_RingDamage(player, inflictor, source, damage);
		}
		else
			return;

		if (player->health < 0)
			player->health = 0;

		return;	
	}

/* */
/* do the damage */
/*		 */
	target->health -= damage;
	if (target->health <= 0)
	{
		P_KillMobj (source, target);
		return;
	}

	S_StartSound(target, targinfo->painsound);
			
	if (source && source != target)
	{	/* if not intent on another player, chase after this one */
		target->target = source;
		target->threshold = BASETHRESHOLD;
		if (target->state == targinfo->spawnstate
		&& targinfo->seestate != S_NULL)
			P_SetMobjState (target, targinfo->seestate);
	}
}

