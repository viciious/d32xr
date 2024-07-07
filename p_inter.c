/* P_inter.c */


#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"

#define	BONUSADD		4

/*
===============================================================================

							GET STUFF

===============================================================================
*/
 
/* 
=================== 
= 
= P_GiveBody
=
= Returns false if the body isn't needed at all
=================== 
*/ 

boolean P_GiveBody (player_t *player, int num)
{
	if (player->health >= MAXHEALTH)
		return false;
		
	player->health += num;
	if (player->health > MAXHEALTH)
		player->health = MAXHEALTH;
	player->mo->health = player->health;
	
	return true;
}

 
/* 
=================== 
= 
= P_GiveArmor
=
= Returns false if the armor is worse than the current armor
=================== 
*/ 

boolean P_GiveArmor (player_t *player, int armortype)
{
	int		hits;
	
	hits = armortype*100;
	if (player->armorpoints >= hits)
		return false;		/* don't pick up */
		
	player->armortype = armortype;
	player->armorpoints = hits;
	
	return true;
}
 
/* 
=================== 
= 
= P_GivePower
=
=================== 
*/ 

boolean P_GivePower (player_t *player, powertype_t power)
{
	if (power == pw_invulnerability)
	{
		player->powers[power] = INVULNTICS;
		return true;
	}
	if (power == pw_ironfeet)
	{
		player->powers[power] = IRONTICS;
		return true;
	}
	if (power == pw_strength)
	{
		P_GiveBody (player, 100);
		player->powers[power] = 1;
		return true;
	}
	if (power == pw_infrared)
	{
		player->powers[power] = INFRATICS;
		return true;
	}

	if (player->powers[power])
		return false;		/* already got it */
		
	player->powers[power] = 1;
	return true;
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

void P_TouchSpecialThing (mobj_t *special, mobj_t *toucher)
{
	player_t	*player;
	int			sound;

	if (!toucher->player)
		return;

	fixed_t sheight;
	if (special->flags & MF_RINGMOBJ)
		sheight = mobjinfo[special->type].height;
	else
		sheight = special->theight << FRACBITS;
		
	if (toucher->z > (special->z + sheight))
		return;
	if (special->z > (toucher->z + (toucher->theight << FRACBITS)))
		return;

	sound = sfx_None;	
	player = &players[toucher->player - 1];
	if (toucher->health <= 0)
		return;						/* can happen with a sliding player corpse */

	if ((special->flags & MF_ENEMY) && !(special->flags & MF_MISSILE))
	{
		if ((player->pflags & PF_JUMPED) || (player->pflags & PF_SPINNING))
		{
			if (((player->pflags & PF_VERTICALFLIP) && toucher->momz > 0)
				|| (!(player->pflags & PF_VERTICALFLIP) && toucher->momz < 0))
				toucher->momz = -toucher->momz;

			P_DamageMobj(special, toucher, toucher, 1);
		}
		else
			P_DamageMobj(toucher, special, special, 1);

		return;
	}

	if (!P_CanPickupItem(player))
		return;

	switch(special->type)
	{
		case MT_RING:
			player->lossCount = 0;
		case MT_FLINGRING:
			player->health++;
			player->mo->health = player->health;
			P_SpawnMobj(special->x, special->y, special->z, MT_SPARK);
			sound = mobjinfo[special->type].deathsound;
		break;

		default:
			break;
	}
	
	if (special->type == MT_RING)
		player->itemcount++;
	P_RemoveMobj (special);
	player->bonuscount += BONUSADD;

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

	if (target->health < -targinfo->spawnhealth
		&& targinfo->xdeathstate)
		P_SetMobjState (target, targinfo->xdeathstate);
	else
		P_SetMobjState (target, targinfo->deathstate);
	target->tics -= P_Random()&1;
	if (target->tics < 1)
		target->tics = 1;

	// TODO: 'Pop!' sprite?		
	S_StartSound(target, targinfo->deathsound);
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

	player->mo->momz = FixedMul(player->mo->momz, FRACUNIT + FRACUNIT/3);

	ang = ((player->mo->momx || player->mo->momy) ? R_PointToAngle2(player->mo->momx, player->mo->momy, 0, 0) : player->mo->angle);

	P_InstaThrust(player->mo, ang, 7*FRACUNIT);

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
			
	if (!target->threshold && source)
	{	/* if not intent on another player, chase after this one */
		target->target = source;
		target->threshold = BASETHRESHOLD;
		if (target->state == targinfo->spawnstate
		&& targinfo->seestate != S_NULL)
			P_SetMobjState (target, targinfo->seestate);
	}
}

