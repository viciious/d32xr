/* P_inter.c */


#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"

/*
===============================================================================

							GET STUFF

===============================================================================
*/

void P_SetStarPosts(uint8_t starpostnum)
{
	for (mobj_t *node = mobjhead.next; node != (void*)&mobjhead; node = node->next)
    {
		if (node->type != MT_STARPOST)
			continue;

		if (node->health >= starpostnum)
			continue;

		P_SetMobjState(node, mobjinfo[MT_STARPOST].seestate);
    }
}

void P_TouchStarPost(mobj_t *starpost, player_t *player)
{
	if (player->starpostnum >= starpost->health)
		return; // Already hit

	player->starpostnum = starpost->health;
	player->starpostx = player->mo->x >> FRACBITS;
	player->starposty = player->mo->y >> FRACBITS;
	player->starpostz = starpost->z;
	player->starpostangle = starpost->angle >> ANGLETOFINESHIFT;

	P_SetStarPosts(starpost->health);

	P_SetMobjState(starpost, mobjinfo[starpost->type].painstate);
	S_StartSound(starpost, mobjinfo[starpost->type].painsound);
}

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
	if ((horizspeed && vertispeed) || (player && player->homingTimer > 0)) // Mimic SA
	{
		player->mo->momx = player->mo->momy = player->mo->momz = 0;
		P_UnsetThingPosition(player->mo);
		player->mo->x = spring->x;
		player->mo->y = spring->y;
		P_SetThingPosition(player->mo);
		player->forwardmove = player->sidemove = 0;

		if (player->homingTimer > 0 && spring->type == MT_SPRINGSHELL)
			player->justSprung = TICRATE / 4;
		else
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
			player->justSprung = TICRATE;
			player->mo->angle = spring->angle;
		}

		{
			boolean wasSpindashing = false;//player->dashspeed > 0;

			pflags = player->pflags & (PF_STARTJUMP | PF_JUMPED | PF_SPINNING | PF_THOKKED); // Save these to restore later.

			if (wasSpindashing) // Ensure we're in the rolling state, and not spindash.
				P_SetMobjState(player->mo, S_PLAY_ATK1);
		}
//		secondjump = player->secondjump;
		washoming = player->homingTimer > 0;
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

	if (special->flags & MF_RINGMOBJ)
	{
		if (!P_CanPickupItem(player))
			return;

		if (special->type == MT_RING)
		{
			ringmobj_t *ring = (ringmobj_t*)special;
			player->lossCount = 0;
			P_SpawnMobj(ring->x << FRACBITS, ring->y << FRACBITS, ring->z << FRACBITS, MT_SPARK);
			P_GivePlayerRings(player, 1);
			sound = mobjinfo[special->type].deathsound;

			player->itemcount++;
		}
		
		special->flags &= ~MF_SPECIAL;
		P_RemoveMobj (special);

		if (sound <= sfx_None)
			return;

		if (player == &players[consoleplayer] || (splitscreen && player == &players[consoleplayer^1]))
			toucher = NULL;
		S_StartSound (toucher, sound);
		return;
	}

	// Ignore eggman in "ouchie" mode
	if ((special->type == MT_EGGMOBILE) && (special->flags2 & MF2_FRET))
		return;

	if ((special->flags2 & MF2_SHOOTABLE) && !(special->flags2 & MF2_MISSILE))
	{
		if (special->flags2 & MF2_ENEMY) // enemy rules
		{
			if (special->type == MT_SPRINGSHELL && special->health > 0)
			{
				fixed_t tmz = toucher->z - toucher->momz;
				fixed_t tmznext = toucher->z;
				fixed_t shelltop = special->z + (special->theight << FRACBITS);
				fixed_t sprarea = 12*FRACUNIT;

				if ((tmznext <= shelltop && tmz > shelltop) || (tmznext > shelltop - sprarea && tmznext < shelltop))
				{
					P_DoSpring(special, player);
					return;
				}
				else if (tmz > shelltop - sprarea && tmz < shelltop) // Don't damage people springing up / down
					return;
			}

			if ((player->pflags & PF_JUMPED) || (player->pflags & PF_SPINNING) || player->powers[pw_invulnerability])
			{
				if (((player->pflags & PF_VERTICALFLIP) && toucher->momz > 0)
					|| (!(player->pflags & PF_VERTICALFLIP) && toucher->momz < 0))
					toucher->momz = -toucher->momz;

				P_DamageMobj(special, toucher, toucher, 1);
			}
			else
				P_DamageMobj(toucher, special, special, 1);

			if (mobjinfo[special->type].spawnhealth > 1) // Multi-hit? Bounce back!
			{
				toucher->momx = -toucher->momx;
				toucher->momy = -toucher->momy;
			}
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

	if (special->type == MT_STARPOST)
	{
		P_TouchStarPost(special, player);
		return;
	}

	if (!P_CanPickupItem(player))
		return;

	switch(special->type)
	{
		case MT_ATTRACTRING:
		case MT_FLINGRING:
			P_SpawnMobj(special->x, special->y, special->z, MT_SPARK);
			P_GivePlayerRings(player, 1);
			sound = mobjinfo[special->type].deathsound;
		break;
		case MT_EXTRALARGEBUBBLE:
			if (player->shield == SH_ELEMENTAL)
				return;
			
			if (special->state != S_EXTRALARGEBUBBLE)
				return;

			if (special->z < toucher->z
				|| special->z > toucher->z + ((toucher->theight << FRACBITS)*2/3))
				return; // Only go in the mouth

			if (player->powers[pw_underwater] <= 12*TICRATE + 1)
				P_RestoreMusic(player);

			if (player->powers[pw_underwater] < UNDERWATERTICS + 1)
				player->powers[pw_underwater] = UNDERWATERTICS + 1;

			P_SetMobjState(special, S_POP1);
			P_SetMobjState(toucher, S_PLAY_GASP);
			S_StartSound(toucher, sfx_s3k_38);
			P_ResetPlayer(player);
			toucher->momx = toucher->momy = toucher->momz = 0;
			return;

		default:
			break;
	}

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

	target->flags2 &= ~(MF2_SHOOTABLE|MF2_FLOAT);
	
	if (target->player)
	{
		player_t* player = &players[target->player - 1];
		target->flags &= ~MF_SOLID;
		player->playerstate = PST_DEAD;
		player->deadTimer = 1;

		if (netgame == gt_coop)
			R_ResetResp(player);

		target->momx = target->momy = 0;

		if (!(player->pflags & PF_DROWNED))
			P_SetObjectMomZ(target, 14*FRACUNIT, false);
		else
			P_SetObjectMomZ(target, FRACUNIT/8, false);
	}

	if (source && source->player)
	{
		// Monitors need to know who killed them
		// TODO: Not multiplayer compatible. I don't care right now
//		target->target = source;
	}

	if (target->player && (players[target->player-1].pflags & PF_DROWNED))
	{
		P_SetMobjState(target, targinfo->xdeathstate);
		S_StartSound(target, targinfo->attacksound);
	}
	else
	{
		P_SetMobjState (target, targinfo->deathstate);
		S_StartSound(target, targinfo->deathsound);
	}

	if (source && source->player && (target->flags2 & MF2_ENEMY))
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

		if (target->type == MT_EGGMOBILE)
		{
			score = 1000;
			scoreState = mobjinfo[MT_SCORE].spawnstate + 3;
		}

		P_SetMobjState(scoremobj, scoreState);
		P_AddPlayerScore(player, score);

		target->momx = target->momy = target->momz = 0;

		if (target->type != MT_EGGMOBILE)
		{
			// Spawn a flicky
			const mobjtype_t flickies[4] = { MT_FLICKY_01, MT_FLICKY_02, MT_FLICKY_03, MT_FLICKY_12 };
			mobj_t *flicky = P_SpawnMobj(target->x, target->y, target->z + (target->theight << (FRACBITS-1)), flickies[P_Random() % 4]);
			flicky->momz = 4*FRACUNIT;
			flicky->target = player->mo;
		}
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

	ang = ((player->mo->momx || player->mo->momy) ? R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy) : player->mo->angle);

	P_InstaThrust(player->mo, ang, -6*FRACUNIT);

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

void P_BlackOw(player_t *player)
{
	S_StartSound(player->mo, sfx_s3k_4e);

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && P_AproxDistance(player->mo->x - players[i].mo->x,
			player->mo->y - players[i].mo->y) < 1536*FRACUNIT)
			players[i].whiteFlash = 4;
	}

	for (mobj_t *node = mobjhead.next; node != (void*)&mobjhead; node = node->next)
    {
		if ((node->flags2 & MF2_ENEMY) && P_AproxDistance(node->x - player->mo->x, node->y - player->mo->y) < 1536*FRACUNIT)
			P_DamageMobj(node, player->mo, player->mo, 1);
    }

	player->shield = 0;
}

static void P_ShieldDamage(player_t *player, mobj_t *inflictor, mobj_t *source, int damage)
{
	P_DoPlayerPain(player, source, inflictor);

	if (player->shield == SH_FORCE2)
		player->shield = SH_FORCE1;
	else if (player->shield == SH_ARMAGEDDON)
	{
		P_BlackOw(player);
		player->shield = 0;
		return;
	}
	else
		player->shield = 0;

	S_StartSound(player->mo, mobjinfo[player->mo->type].deathsound);
}

void P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source, int damage)
{
	player_t	*player;
	const mobjinfo_t* targinfo = &mobjinfo[target->type];

	if (target->flags & MF_RINGMOBJ)
		return;

	if ( !(target->flags2 & MF2_SHOOTABLE) )
		return;						/* shouldn't happen... */

	if (target->health <= 0)
		return;

	if (target->type == MT_EGGMOBILE)
	{
		if (target->flags2 & MF2_FRET) // Currently flashing from being hit
			return;

		if (target->health > 1)
			target->flags2 |= MF2_FRET;

		if (inflictor && inflictor->player)
		{
			if (players[inflictor->player - 1].homingTimer)
				players[inflictor->player - 1].homingTimer = 0;
		}
	}
	
	player = target->player ? &players[target->player - 1] : NULL;

/* */
/* player specific */
/* */
	if (player)
	{
		if (player->exiting)
			return;

		P_ResetPlayer(player);

		if (damage == 10000) // magic number for instant death
			P_KillMobj(source, target);
		else if ( damage < 10000 )
		{
			if ( (player->cheats&CF_GODMODE)|| player->powers[pw_invulnerability] || player->powers[pw_flashing] )
				return;

			if (player->shield)
				P_ShieldDamage(player, inflictor, source, damage);
			else if (player->mo->health > 1) // Rings without shield
				P_RingDamage(player, inflictor, source, damage);
			else
				P_KillMobj(source, player->mo);
		}
		else
			return;

		if (player->health < 0)
			player->health = 0;

		return;	
	}
	else if (targinfo->painstate)
		P_SetMobjState(target, targinfo->painstate);

/* */
/* do the damage */
/*		 */
	while (damage > 0)
	{
		target->health--;
		damage--;
		if (target->health == 0)
		{
			P_KillMobj (source, target);
			return;
		}
	}

//	S_StartSound(target, targinfo->painsound);
			
	if (source && source != target)
	{	/* if not intent on another player, chase after this one */

		target->target = source;
		if (target->state == targinfo->spawnstate && targinfo->seestate != S_NULL)
			P_SetMobjState(target, targinfo->seestate);
	}
}

