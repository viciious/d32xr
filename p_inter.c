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
= P_TouchSpecialThing2
=
= Everything didn't fit in one function on the dsp, so half were moved here
= Returns sound to play, or -1 if no sound
==================
*/

int P_TouchSpecialThing2 (mobj_t *special, mobj_t *toucher)
{/*
	player_t *player;
	
	player = toucher->player ? &players[toucher->player-1] : NULL;
*/
	switch (special->sprite)
	{
	default:
		I_Error ("P_SpecialThing: Unknown gettable thing");
	}
	
	return sfx_itemup;
}


/*
==================
=
= P_TouchSpecialThing
=
==================
*/

void P_TouchSpecialThing (mobj_t *special, mobj_t *toucher)
{
	player_t	*player;
	fixed_t		delta;
	int			sound;
		
	delta = special->z - toucher->z;
	if (delta > (toucher->theight << FRACBITS) || delta < -8*FRACUNIT)
		return;			/* out of reach */
	
	sound = sfx_itemup;	
	player = toucher->player ? &players[toucher->player - 1] : NULL;
	if (toucher->health <= 0)
		return;						/* can happen with a sliding player corpse */
	switch (special->sprite)
	{
/* */
/* bonus items */
/* */
	case SPR_RING:
		player->health+=2;		/* can go over 100% */
		if (player->health > 200)
			player->health = 200;
		player->mo->health = player->health;
		player->message = "You pick up a health bonus.";
		break;

	default:
		sound = P_TouchSpecialThing2 (special, toucher);
		if (sound == -1)
			return;
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

		if (target->health < -50)
		{
			stbar[target->player - 1].gotgibbed = true;
			S_StartSound (target, sfx_slop);
		}
		else
			S_StartSound (target, sfx_pldeth);
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

void P_DamageMobj (mobj_t *target, mobj_t *inflictor, mobj_t *source, int damage)
{
	unsigned	ang, an;
	int			saved;
	int			pnum = 0;
	player_t	*player;
	fixed_t		thrust;
	const mobjinfo_t* targinfo = &mobjinfo[target->type];

	if ( !(target->flags & MF_SHOOTABLE) )
		return;						/* shouldn't happen... */
		
	if (target->health <= 0)
		return;
	
	player = target->player ? &players[target->player - 1] : NULL;
	if (player)
		pnum = player - players;
	
/* */
/* kick away unless using the chainsaw */
/* */
	if (inflictor && (!source || !source->player))
	{
		ang = R_PointToAngle2 ( inflictor->x, inflictor->y
			,target->x, target->y);
		
		thrust = damage*(FRACUNIT>>2)*100/targinfo->mass;

		/* make fall forwards sometimes */
		if ( damage < 40 && damage > target->health && target->z - inflictor->z > 64*FRACUNIT && (P_Random ()&1) )
		{
			ang += ANG180;
			thrust *= 4;
		}
		
		an = ang >> ANGLETOFINESHIFT;
		thrust >>= 16;
		target->momx += thrust * finecosine(an);
		target->momy += thrust * finesine(an);
	}

/* */
/* player specific */
/* */
	if (player)
	{
		// telefrags ignore the godmode cheat and invulnerability
		if ( damage < 1000 )
		{
			if ( (player->cheats&CF_GODMODE)||player->powers[pw_invulnerability] )
				return;
		}

		if (damage > 0)
		{
			stbar[pnum].specialFace = damage > 20 ? f_hurtbad : f_mowdown;

			if (source && source != player->mo)
			{
				int right;
				ang = R_PointToAngle2 ( target->x, target->y,
					source->x, source->y );

				if (ang > target->angle)
				{
					an = ang - target->angle;
					right = an > ANG180; 
				}
				else
				{
					an = target->angle - ang;
					right = an <= ANG180; 
				}

				if (an >= ANG45)
					stbar[pnum].specialFace = right ? f_faceright : f_faceleft;
			}
		}
		if (player->armortype)
		{
			if (player->armortype == 1)
				saved = damage/3;
			else
				saved = damage/2;
			if (player->armorpoints <= saved)
			{	/* armor is used up */
				saved = player->armorpoints;
				player->armortype = 0;
			}
			player->armorpoints -= saved;
			damage -= saved;
		}
		S_StartSound (target,sfx_plpain);
		player->health -= damage;		/* mirror mobj health here for Dave */
		if (player->health < 0)
			player->health = 0;
		player->attacker = source;
		player->damagecount += (damage>>1);	/* add damage after armor / invuln */
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
			
	if (!target->threshold && source)
	{	/* if not intent on another player, chase after this one */
		target->target = source;
		target->threshold = BASETHRESHOLD;
		if (target->state == targinfo->spawnstate
		&& targinfo->seestate != S_NULL)
			P_SetMobjState (target, targinfo->seestate);
	}
			
}

