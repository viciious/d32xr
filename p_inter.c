/* P_inter.c */


#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"

#define	BONUSADD		4

/* a weapon is found with two clip loads, a big item has five clip loads */
int		maxammo[NUMAMMO] = {200, 50, 300, 50};
int		clipammo[NUMAMMO] = {10, 4, 20, 1};


/*
===============================================================================

							GET STUFF

===============================================================================
*/

/* 
=================== 
= 
= P_GiveAmmo
=
= Num is the number of clip loads, not the individual count (0= 1/2 clip)
= Returns false if the ammo can't be picked up at all
=================== 
*/ 

boolean P_GiveAmmo (player_t *player, ammotype_t ammo, int num)
{
	int		oldammo;
	
	if (ammo == am_noammo)
		return false;
		
	if (ammo > NUMAMMO)
		I_Error ("P_GiveAmmo: bad type %i", ammo);
		
	if ( player->ammo[ammo] == player->maxammo[ammo]  )
		return false;
			
	if (num)
		num *= clipammo[ammo];
	else
		num = clipammo[ammo]/2;
	if (gameskill == sk_baby)
		num <<= 1;			/* give double ammo in trainer mode */
		
	oldammo = player->ammo[ammo];
	player->ammo[ammo] += num;
	if (player->ammo[ammo] > player->maxammo[ammo])
		player->ammo[ammo] = player->maxammo[ammo];
	
	if (oldammo)
		return true;		/* don't change up weapons, player was lower on */
							/* purpose */

	switch (ammo)
	{
	case am_clip:
		if (player->readyweapon == wp_fist)
		{
			if (player->weaponowned[wp_chaingun])
				player->pendingweapon = wp_chaingun;
			else
				player->pendingweapon = wp_pistol;
		}
		break;
	case am_shell:
		if (player->readyweapon == wp_fist || player->readyweapon == wp_pistol)
		{
			if (player->weaponowned[wp_shotgun])
				player->pendingweapon = wp_shotgun;
		}
		break;
	case am_cell:
		if (player->readyweapon == wp_fist || player->readyweapon == wp_pistol)
		{
			if (player->weaponowned[wp_plasma])
				player->pendingweapon = wp_plasma;
		}
		break;
	case am_misl:
		if (player->readyweapon == wp_fist)
		{
			if (player->weaponowned[wp_missile])
				player->pendingweapon = wp_missile;
		}
	default:
		break;
	}
	
	return true;
}


/* 
=================== 
= 
= P_GiveWeapon
=
= The weapon name may have a MF_DROPPED flag ored in
=================== 
*/ 

boolean P_GiveWeapon (player_t *player, weapontype_t weapon, boolean dropped)
{
	boolean		gaveammo, gaveweapon;
	
	if (netgame == gt_coop && !dropped)
	{	/* leave placed weapons forever on cooperative net games */
		if (player->weaponowned[weapon])
			return false;
		player->weaponowned[weapon] = true;
		P_GiveAmmo (player, weaponinfo[weapon].ammo, 2);
		player->pendingweapon = weapon;
		S_StartSound (player->mo, sfx_wpnup);
		return false;
	}
	
	if (weaponinfo[weapon].ammo != am_noammo)
	{	/* give one clip with a dropped weapon, two clips with a found weapon */
		if (dropped)
			gaveammo = P_GiveAmmo (player, weaponinfo[weapon].ammo, 1);
		else
			gaveammo = P_GiveAmmo (player, weaponinfo[weapon].ammo, 2);
	}
	else
		gaveammo = false;
	
	if (player->weaponowned[weapon])
		gaveweapon = false;
	else
	{
		gaveweapon = true;
		player->weaponowned[weapon] = true;
		player->pendingweapon = weapon;
		if (player == &players[consoleplayer])
			stbar.specialFace = f_gotgat;
	}
	
	return gaveweapon || gaveammo;
}

 
 
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
= P_GiveCard
=
=================== 
*/ 

void P_GiveCard (player_t *player, card_t card)
{
	if (player->cards[card])
		return;		
	player->bonuscount = BONUSADD;
	player->cards[card] = 1;
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
{
	player_t *player;
	
	player = toucher->player ? &players[toucher->player-1] : NULL;

	switch (special->sprite)
	{
/* */
/* armor */
/* */
	case SPR_ARM1:
		if (!P_GiveArmor (player, 1))
			return -1;
		player->message = "You pick up the armor.";
		break;
		
	case SPR_ARM2:
		if (!P_GiveArmor (player, 2))
			return -1;
		player->message = "You got the MegaArmor!";
		break;

/* */
/* cards */
/* leave cards for everyone */
	case SPR_BKEY:
		if (!player->cards[it_bluecard])
			player->message = "You pick up a blue keycard.";
		P_GiveCard (player, it_bluecard);
		if (!netgame)
			break;
		return -1;
	case SPR_YKEY:
		if (!player->cards[it_yellowcard])
			player->message = "You pick up a yellow keycard.";
		P_GiveCard (player, it_yellowcard);
		if (!netgame)
			break;
		return -1;
	case SPR_RKEY:
		if (!player->cards[it_redcard])
			player->message = "You pick up a red keycard.";
		P_GiveCard (player, it_redcard);
		if (!netgame)
			break;
		return -1;
	case SPR_BSKU:
		if (!player->cards[it_blueskull])
			player->message = "You pick up a blue skull key.";
		P_GiveCard (player, it_blueskull);
		if (!netgame)
			break;
		return -1;
	case SPR_YSKU:
		if (!player->cards[it_yellowskull])
			player->message = "You pick up a yellow skull key.";
		P_GiveCard (player, it_yellowskull);
		if (!netgame)
			break;
		return -1;
	case SPR_RSKU:
		if (!player->cards[it_redskull])
			player->message = "You pick up a red skull key.";
		P_GiveCard (player, it_redskull);
		if (!netgame)
			break;
		return -1;

/* */
/* heals */
/* */
	case SPR_STIM:
		if (!P_GiveBody (player, 10))
			return -1;
		player->message = "You pick up a stimpack.";
		break;
	case SPR_MEDI:
		if (!P_GiveBody (player, 25))
			return -1;
		if (player->health < 25)
			player->message = "You pick up a medikit that you REALLY need!";
		else
			player->message = "You pick up a medikit.";
		break;
	
/* */
/* power ups */
/* */
	case SPR_PINV:
		if (!P_GivePower (player, pw_invulnerability))
			return -1;
		player->message = "Invulnerability!";
		break;
	case SPR_PSTR:
		if (!P_GivePower (player, pw_strength))
			return -1;
		player->message = "Berserk!";
		if (player->readyweapon != wp_fist)
			player->pendingweapon = wp_fist;
		break;
	case SPR_PINS:
		break;
	case SPR_SUIT:
		if (!P_GivePower (player, pw_ironfeet))
			return -1;
		player->message = "Radiation Shielding Suit";
		break;
	case SPR_PMAP:
		if (!P_GivePower (player, pw_allmap))
			return -1;
		player->message = "Computer Area Map";
		break;
	case SPR_PVIS:
		break;
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
	int			i;
	fixed_t		delta;
	int			sound;
		
	delta = special->z - toucher->z;
	if (delta > toucher->height || delta < -8*FRACUNIT)
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
	case SPR_BON1:
		player->health+=2;		/* can go over 100% */
		if (player->health > 200)
			player->health = 200;
		player->mo->health = player->health;
		player->message = "You pick up a health bonus.";
		break;
	case SPR_BON2:
		player->armorpoints+=2;		/* can go over 100% */
		if (player->armorpoints > 200)
			player->armorpoints = 200;
		if (!player->armortype)
			player->armortype = 1;
		player->message = "You pick up an armor bonus.";
		break;
	case SPR_SOUL:
		player->health += 100;
		if (player->health > 200)
			player->health = 200;
		player->mo->health = player->health;
		player->message = "Supercharge!";
		break;
				

		
/* */
/* ammo */
/* */
	case SPR_CLIP:
		if (special->flags & MF_DROPPED)
		{
			if (!P_GiveAmmo (player,am_clip,0))
				return;
		}
		else
		{
			if (!P_GiveAmmo (player,am_clip,1))
				return;
		}
		player->message = "Picked up a clip.";
		break;
	case SPR_AMMO:
		if (!P_GiveAmmo (player, am_clip,5))
			return;
		player->message = "Picked up a box of bullets.";
		break;
	case SPR_ROCK:
		if (!P_GiveAmmo (player, am_misl,1))
			return;
		player->message = "Picked up a rocket.";
		break;	
	case SPR_BROK:
		if (!P_GiveAmmo (player, am_misl,5))
			return;
		player->message = "Picked up a box of rockets.";
		break;
	case SPR_CELL:
		if (!P_GiveAmmo (player, am_cell,1))
			return;
		player->message = "Picked up an energy cell.";
		break;
	case SPR_CELP:
		if (!P_GiveAmmo (player, am_cell,5))
			return;
		player->message = "Picked up an energy cell pack.";
		break;
	case SPR_SHEL:
		if (!P_GiveAmmo (player, am_shell,1))
			return;
		player->message = "Picked up 4 shotgun shells.";
		break;
	case SPR_SBOX:
		if (!P_GiveAmmo (player, am_shell,5))
			return;
		player->message = "Picked up a box of shotgun shells.";
		break;		
	case SPR_BPAK:
		if (!player->backpack)
		{
			for (i=0 ; i<NUMAMMO ; i++)
				player->maxammo[i] *= 2;
			player->backpack = true;
		}
		for (i=0 ; i<NUMAMMO ; i++)
			P_GiveAmmo (player, i, 1);
		player->message = "Picked up a backpack full of ammo!";
		break;

		
/* */
/* weapons */
/* */
	case SPR_BFUG:
		if (!P_GiveWeapon (player, wp_bfg, false) )
			return;
		player->message = "You got the BFG9000!  Oh, yes.";
		sound = sfx_wpnup;	
		break;
	case SPR_MGUN:
		if (!P_GiveWeapon (player, wp_chaingun, false) )
			return;
		player->message = "You got the chaingun!";
		sound = sfx_wpnup;	
		break;
	case SPR_CSAW:
		if (!P_GiveWeapon (player, wp_chainsaw, false) )
			return;
		player->message = "A chainsaw!  Find some meat!";
		sound = sfx_wpnup;	
		break;
	case SPR_LAUN:
		if (!P_GiveWeapon (player, wp_missile, false) )
			return;
		player->message = "You got the rocket launcher!";
		sound = sfx_wpnup;	
		break;
	case SPR_PLAS:
		if (!P_GiveWeapon (player, wp_plasma, false) )
			return;
		player->message = "You got the plasma gun!";
		sound = sfx_wpnup;	
		break;
	case SPR_SHOT:
		if (!P_GiveWeapon (player, wp_shotgun, special->flags&MF_DROPPED ) )
			return;
		player->message = "You got the shotgun!";
		sound = sfx_wpnup;	
		break;
		
	default:
		sound = P_TouchSpecialThing2 (special, toucher);
		if (sound == -1)
			return;
	}
	
	if (special->flags & MF_COUNTITEM)
		player->itemcount++;
	P_RemoveMobj (special);
	player->bonuscount += BONUSADD;
	if (player == &players[consoleplayer])
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
	mobjtype_t		item;
	mobj_t			*mo;
	const mobjinfo_t* targinfo = &mobjinfo[target->type];

	target->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);
	if (target->type != MT_SKULL)
		target->flags &= ~MF_NOGRAVITY;
	target->flags |= MF_CORPSE|MF_DROPOFF;
	target->height >>= 2;
	
	if (target->player)
	{	/* a frag of one sort or another */
		if (!source || !source->player || source->player == target->player)
		{	/* killed self somehow */
			player_t* player = &players[target->player - 1];
			player->frags--;
			if (player->frags < 0)
				player->frags = 0;
		}
		else 
		{	/* killed by other player */
			players[source->player - 1].frags++;
		}
		
		/* else just killed by a monster */
	}
	else if (source && source->player && (target->flags & MF_COUNTKILL) )
	{	/* a deliberate kill by a player */
		players[source->player - 1].killcount++;		/* count for intermission */
	}
	else if (!netgame && (target->flags & MF_COUNTKILL) )
		players[0].killcount++;			/* count all monster deaths, even */
										/* those caused by other monsters */
	
	if (target->player)
	{
		player_t* player = &players[target->player - 1];
		target->flags &= ~MF_SOLID;
		player->playerstate = PST_DEAD;
		P_DropWeapon (player);
		if (target->health < -50)
		{
			if (player == &players[consoleplayer])
				stbar.gotgibbed = true;
			S_StartSound (target, sfx_slop);
		}
		else
			S_StartSound (target, sfx_pldeth);
	}

	if (target->health < -targinfo->spawnhealth
	&& targinfo->xdeathstate)
		P_SetMobjState (target, targinfo->xdeathstate);
	else
		P_SetMobjState (target, targinfo->deathstate);
	target->tics -= P_Random()&1;
	if (target->tics < 1)
		target->tics = 1;
		
/* */
/* drop stuff */
/* */
	switch (target->type)
	{
	case MT_POSSESSED:
		item = MT_CLIP;
		break;
	case MT_SHOTGUY:
		item = MT_SHOTGUN;
		break;
	default:
		return;
	}

	mo = P_SpawnMobj (target->x,target->y,ONFLOORZ, item);
	mo->flags |= MF_DROPPED;		/* special versions of items */
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
	player_t	*player;
	fixed_t		thrust;
	const mobjinfo_t* targinfo = &mobjinfo[target->type];

	if ( !(target->flags & MF_SHOOTABLE) )
		return;						/* shouldn't happen... */
		
	if (target->health <= 0)
		return;

	if ( target->flags & MF_SKULLFLY )
	{
		target->momx = target->momy = target->momz = 0;
	}
	
	player = target->player ? &players[target->player - 1] : NULL;
	if (player && gameskill == sk_baby)
		damage >>= 1;				/* take half damage in trainer mode */
	
	if (player && (damage > 30) && player == &players[consoleplayer])
		stbar.specialFace = f_hurtbad;
/* */
/* kick away unless using the chainsaw */
/* */
	if (inflictor && (!source || !source->player 
		|| players[source->player - 1].readyweapon != wp_chainsaw))
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
	else
		ang = target->angle;
	
/* */
/* player specific */
/* */
	if (player)
	{
		if ( (player->cheats&CF_GODMODE)||player->powers[pw_invulnerability] )
			return;
		if (player == &players[consoleplayer])
		{
			ang -= target->angle;
			if (ang > 0x30000000 && ang <0x80000000)
				stbar.specialFace = f_faceright;
			else if (ang >0x80000000 && ang < 0xd0000000)
				stbar.specialFace = f_faceleft;
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

	if ( (P_Random () < targinfo->painchance) && !(target->flags&MF_SKULLFLY) )
	{
		target->flags |= MF_JUSTHIT;		/* fight back! */
		P_SetMobjState (target, targinfo->painstate);
	}
			
	target->reactiontime = 0;		/* we're awake now...	 */
	if (!target->threshold && source
	&& spr_rotations	/* don't fight amongst each other without rotations */
	)
	{	/* if not intent on another player, chase after this one */
		target->target = source;
		target->threshold = BASETHRESHOLD;
		if (target->state == targinfo->spawnstate
		&& targinfo->seestate != S_NULL)
			P_SetMobjState (target, targinfo->seestate);
	}
			
}

