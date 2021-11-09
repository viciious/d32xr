/* P_user.c */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"


fixed_t 		forwardmove[2] = {0x40000, 0x60000}; 
fixed_t 		sidemove[2] = {0x38000, 0x58000}; 

#define SLOWTURNTICS    10
fixed_t			angleturn[] =
	{300,300,500,500,600,700,800,900,900,1000};
fixed_t			fastangleturn[] =
	{800,800,900,1000,1000,1200,1200,1300,1300,1400};

#define	STOPSPEED		FRACUNIT / 16
#define	FRICTION		0xd240
#define MAXBOB			16 * FRACUNIT		/* 16 pixels of bob */

/*============================================================================= */

mobj_t		*slidething;
extern	fixed_t		slidex, slidey;
extern	line_t	*specialline;

void P_SlideMove ();


void P_PlayerMove (mobj_t *mo)
{
	fixed_t		momx, momy;
	line_t		*latchedline;
	fixed_t		latchedx, latchedy;
		
	momx = vblsinframe[playernum]*(mo->momx>>2);
	momy = vblsinframe[playernum]*(mo->momy>>2);
	
	slidething = mo;
	
#ifdef JAGUAR
{
	extern	int p_slide_start;
	DSPFunction (&p_slide_start);
}
#else
	P_SlideMove ();
#endif
		
	latchedline = (line_t *)DSPRead (&specialline);
	latchedx = DSPRead (&slidex);
	latchedy = DSPRead (&slidey);
	
	if (latchedx == mo->x && latchedy == mo->y)
		goto stairstep;
		
	if ( P_TryMove (mo, latchedx, latchedy) )
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

	if (P_TryMove (mo, mo->x, mo->y + momy))
	{
		mo->momx = 0;
		mo->momy = momy;
		goto dospecial;
	}
	
	if (P_TryMove (mo, mo->x + momx, mo->y))
	{
		mo->momx = momx;
		mo->momy = 0;
		goto dospecial;
	}

	mo->momx = mo->momy = 0;

dospecial:
	if (latchedline)
		P_CrossSpecialLine (latchedline, mo);
}


/* 
=================== 
= 
= P_PlayerXYMovement  
=
=================== 
*/ 

void P_PlayerXYMovement (mobj_t *mo) 
{ 	
	P_PlayerMove (mo);

/* */
/* slow down */
/* */
	if (mo->z > mo->floorz)
		return;		/* no friction when airborne */

	if (mo->flags & MF_CORPSE)
		if (mo->floorz != mo->subsector->sector->floorheight)
			return;			/* don't stop halfway off a step */

	if (mo->momx > -STOPSPEED && mo->momx < STOPSPEED
	&& mo->momy > -STOPSPEED && mo->momy < STOPSPEED)
	{	
		mo->momx = 0;
		mo->momy = 0;
	}
	else
	{
		mo->momx = (mo->momx>>8)*(FRICTION>>8);
		mo->momy = (mo->momy>>8)*(FRICTION>>8);
	}
}


/*
===============
=
= P_PlayerZMovement
=
===============
*/

void P_PlayerZMovement (mobj_t *mo)
{	
	player_t* player = &players[mo->player - 1];

/* */
/* check for smooth step up */
/* */
	if (mo->z < mo->floorz)
	{
		player->viewheight -= mo->floorz-mo->z;
		player->deltaviewheight = (VIEWHEIGHT - player->viewheight)>>2;
	}

/* */
/* adjust height */
/* */
	mo->z += mo->momz;
	
/* */
/* clip movement */
/* */
	if (mo->z <= mo->floorz)
	{	/* hit the floor */
		if (mo->momz < 0)
		{
			if (mo->momz < -GRAVITY*2)	/* squat down */
			{
				player->deltaviewheight = mo->momz>>3;
				if (player->playerstate != PST_DEAD)
					S_StartSound (mo, sfx_oof);
			}
			mo->momz = 0;
		}
		mo->z = mo->floorz;
	}
	else
	{
		fixed_t gravity = (GRAVITY * vblsinframe[0]) / TICVBLS;
		if (mo->momz == 0)
			mo->momz = -gravity;
		else
			mo->momz -= gravity/2;
	}
	
	if (mo->z + mo->height > mo->ceilingz)
	{	/* hit the ceiling */
		if (mo->momz > 0)
			mo->momz = 0;
		mo->z = mo->ceilingz - mo->height;		
	}
	
} 
 

/*
================
=
= P_PlayerMobjThink
=
================
*/

void P_PlayerMobjThink (mobj_t *mobj)
{
	const state_t	*st;
	int		state;
	
/* */
/* momentum movement */
/* */
	if (mobj->momx || mobj->momy)
		P_PlayerXYMovement (mobj);

	if ( (mobj->z != mobj->floorz) || mobj->momz)
		P_PlayerZMovement (mobj);
	
	if (gametic == prevgametic)
		return;

/* */
/* cycle through states, calling action functions at transitions */
/* */
	if (mobj->tics == -1)
		return;				/* never cycle */
		
	mobj->tics--;
	
	if (mobj->tics > 0)
		return;				/* not time to cycle yet */
		
	state = states[mobj->state].nextstate;	
	st = &states[state];

	mobj->state = state;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;
}

/*============================================================================= */


/* 
==================== 
= 
= P_BuildMove 
= 
==================== 
*/ 
 
void P_BuildMove (player_t *player) 
{ 
	boolean     strafe;
	int         speed;
	int			buttons, oldbuttons;
	mobj_t		*mo;
	int			vbls;
	int			alwrun;

	buttons = ticbuttons[playernum];
	oldbuttons = oldticbuttons[playernum];
	vbls = vblsinframe[playernum];
	alwrun = demoplayback || demorecording ? 0 : alwaysrun;

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

			speed = ((buttons & BT_SPEED) > 0) ^ alwrun;
 
			player->forwardmove = player->sidemove = 0;
	
			if (buttons & BT_RIGHT) 
				player->sidemove += (sidemove[speed] * vbls) / TICVBLS;
			if (buttons & BT_LEFT) 
				player->sidemove += (-sidemove[speed] * vbls) / TICVBLS;
 
			if (buttons & BT_UP) 
				player->forwardmove += (forwardmove[speed] * vbls) / TICVBLS;
			if (buttons & BT_DOWN) 
				player->forwardmove += (-forwardmove[speed] * vbls) / TICVBLS;
		}
	}
	else
	{
		speed = ((buttons & BT_SPEED) > 0) ^ alwrun;

		/*  */
		/* use two stage accelerative turning on the joypad  */
		/*  */
		if ((buttons & BT_LEFT) && (oldbuttons & BT_LEFT))
			player->turnheld++;
		else
			if ((buttons & BT_RIGHT) && (oldbuttons & BT_RIGHT))
				player->turnheld++;
			else
				player->turnheld = 0;
		if (player->turnheld >= SLOWTURNTICS)
			player->turnheld = SLOWTURNTICS - 1;

		player->forwardmove = player->sidemove = player->angleturn = 0;

		if (strafebtns)
		{
			if (buttons & BT_STRAFERIGHT)
				player->sidemove += (sidemove[speed] * vbls) / TICVBLS;
			if (buttons & BT_STRAFELEFT)
				player->sidemove += (-sidemove[speed] * vbls) / TICVBLS;
			goto turn;
		}
		else
		{
			strafe = (buttons & BT_STRAFE) > 0;

			if (strafe)
			{
				if (buttons & BT_RIGHT)
					player->sidemove += (sidemove[speed] * vbls) / TICVBLS;
				if (buttons & BT_LEFT)
					player->sidemove += (-sidemove[speed] * vbls) / TICVBLS;
			}
			else
			{
turn:
				if (speed && !(buttons & (BT_UP | BT_DOWN)) && !alwrun)
				{
					if (buttons & BT_RIGHT)
						player->angleturn = ((-fastangleturn[player->turnheld] * vbls) / TICVBLS) << 17;
					if (buttons & BT_LEFT)
						player->angleturn = ((fastangleturn[player->turnheld] * vbls) / TICVBLS) << 17;
				}
				else
				{
					if (buttons & BT_RIGHT)
						player->angleturn = ((-angleturn[player->turnheld] * vbls) / TICVBLS) << 17;
					if (buttons & BT_LEFT)
						player->angleturn = ((angleturn[player->turnheld] * vbls) / TICVBLS) << 17;
				}
			}
		}

		if (buttons & BT_UP)
			player->forwardmove += (forwardmove[speed] * vbls) / TICVBLS;
		if (buttons & BT_DOWN)
			player->forwardmove += (-forwardmove[speed] * vbls) / TICVBLS;
	}

/* */
/* if slowed down to a stop, change to a standing frame */
/* */
	mo = player->mo;
	
	if (!mo->momx && !mo->momy
	&& player->forwardmove== 0 && player->sidemove == 0 )
	{	/* if in a walking frame, stop moving */
		if (mo->state == S_PLAY_RUN1 
		|| mo->state == S_PLAY_RUN2
		|| mo->state == S_PLAY_RUN3
		|| mo->state == S_PLAY_RUN4)
			P_SetMobjState (mo, S_PLAY);
	}
	
} 
 

/*
===============================================================================

						movement

===============================================================================
*/

boolean		onground;

/* 
================== 
= 
= P_Thrust 
= 
= moves the given origin along a given angle 
= 
================== 
*/ 
 
void P_Thrust (player_t *player, angle_t angle, fixed_t move) 
{
	angle >>= ANGLETOFINESHIFT;
	move >>= 8;
	player->mo->momx += move*(finecosine(angle)>>8);
	player->mo->momy += move*(finesine(angle)>>8);
}



/* 
================== 
= 
= P_CalcHeight 
= 
= Calculate the walking / running height adjustment
=
================== 
*/ 

void P_CalcHeight (player_t *player) 
{
	int			angle;
	fixed_t		bob;
	fixed_t		movx, movy;

/* */
/* regular movement bobbing (needs to be calculated for gun swing even */
/* if not on ground) */
/* OPTIMIZE: tablify angle  */
	movx = player->mo->momx;
	movy = player->mo->momy;

	if (vblsinframe[playernum] <= 2)
	{
		movx = FixedDiv(movx, FRICTION);
		movy = FixedDiv(movy, FRICTION);
	}
	movx >>= 8;
	movy >>= 8;

	player->bob += movx*movx + movy*movy;
	player->bob >>= 4;
	if (player->bob>MAXBOB)
	{
		player->bob = MAXBOB;
	}

	if (!onground)
	{
		player->viewz = player->mo->z + VIEWHEIGHT;
		if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
			player->viewz = player->mo->ceilingz-4*FRACUNIT;
		return;
	}
		
	angle = (FINEANGLES/40*gamevbls)&(FINEANGLES-1);
	bob = (( player->bob )>>17) * finesine(angle);
	
/* */
/* move viewheight */
/* */
	if (player->playerstate == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;
		if (player->viewheight > VIEWHEIGHT)
		{
			player->viewheight = VIEWHEIGHT;
			player->deltaviewheight = 0;
		}
		if (player->viewheight < VIEWHEIGHT/2)
		{
			player->viewheight = VIEWHEIGHT/2;
			if (player->deltaviewheight <= 0)
				player->deltaviewheight = 1;
		}
	
		if (player->deltaviewheight)	
		{
			player->deltaviewheight += FRACUNIT/2;
			if (!player->deltaviewheight)
				player->deltaviewheight = 1;
		}
	}
	player->viewz = player->mo->z + player->viewheight + bob;
	if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
		player->viewz = player->mo->ceilingz-4*FRACUNIT;
}

/*
=================
=
= P_MovePlayer
=
=================
*/

void P_MovePlayer (player_t *player)
{	
	player->mo->angle += player->angleturn;

	/* don't let the player control movement if not onground */
	onground = (player->mo->z <= player->mo->floorz);
	
	if (player->forwardmove && onground)
		P_Thrust (player, player->mo->angle, player->forwardmove);
	if (player->sidemove && onground)
		P_Thrust (player, player->mo->angle-ANG90, player->sidemove);

	if ( (player->forwardmove || player->sidemove) 
	&& player->mo->state == S_PLAY )
		P_SetMobjState (player->mo, S_PLAY_RUN1);

}	


/*
=================
=
= P_DeathThink
=
=================
*/

#define		ANG5	(ANG90/18)

void P_DeathThink (player_t *player)
{
	angle_t		angle, delta;

	P_MovePsprites (player);
	
/* fall to the ground */
	if (player->viewheight > 8*FRACUNIT)
		player->viewheight -= FRACUNIT;
	onground = (player->mo->z <= player->mo->floorz);
	P_CalcHeight (player);
	
	if (player->attacker && player->attacker != player->mo)
	{
		angle = R_PointToAngle2 (player->mo->x, player->mo->y
		, player->attacker->x, player->attacker->y);
		delta = angle - player->mo->angle;
		if (delta < ANG5 || delta > (unsigned)-ANG5)
		{	/* looking at killer, so fade damage flash down */
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
	

	if ( (ticbuttons[playernum] & BT_USE) && player->viewheight <= 8*FRACUNIT)
		player->playerstate = PST_REBORN;
}

//
// CALICO: Returns false if:
// * player doesn't own the weapon in question -or-
// * the weapon is the fist and the player also has the chainsaw,
// * unless the player also has the berserk pack.
// Returns true otherwise.
//
boolean P_CanSelecteWeapon(player_t* player, int weaponnum)
{
	if (!player->weaponowned[weaponnum])
		return false;

	if (weaponnum == wp_fist
		&& player->weaponowned[wp_chainsaw]
		&& !player->powers[pw_strength])
		return false;

	return true;
}

//
// CALICO: Returns false if:
// * player can not select the weapon in question -or-
// * the weapon uses ammo and does not have sufficient ammo
// Returns true otherwise.
//
boolean P_CanFireWeapon(player_t* player, int weaponnum)
{
	if (!P_CanSelecteWeapon(player, weaponnum))
		return false;

	if (weaponinfo[weaponnum].ammo == am_noammo)
		return true;

	int neededAmmo = (weaponnum == wp_bfg) ? 40 : 1;
	return player->ammo[weaponinfo[weaponnum].ammo] >= neededAmmo;
}

/*
=================
=
= P_PlayerThink
=
=================
*/

extern int ticphase;

void P_PlayerThink (player_t *player)
{
	int		buttons;
	
	buttons = ticbuttons[playernum];

ticphase = 20;
	P_PlayerMobjThink (player->mo);
	
ticphase = 21;
	P_BuildMove (player);
	
/* */
/* chain saw run forward */
/* */
	if (player->mo->flags & MF_JUSTATTACKED)
	{
		player->angleturn = 0;
		player->forwardmove = 25 * FRACUNIT / 32;
		player->sidemove = 0;
		player->mo->flags &= ~MF_JUSTATTACKED;
	}
			
	
	if (player->playerstate == PST_DEAD)
	{
		P_DeathThink (player);
		return;
	}
		
/* */
/* move around */
/* reactiontime is used to prevent movement for a bit after a teleport */
ticphase = 22;
	if (player->mo->reactiontime)
		player->mo->reactiontime--;
	else
		P_MovePlayer (player);
	P_CalcHeight (player);
	if (player->mo->subsector->sector->special)
		P_PlayerInSpecialSector (player);
		
/* */
/* check for weapon change */
/* */
ticphase = 23;
	if (player->pendingweapon == wp_nochange)
	{
		int oldbuttons = oldticbuttons[playernum];

#ifdef JAGUAR
		if ( buttons & BT_1 )
		{
			if (player->weaponowned[wp_chainsaw] &&
			!(player->readyweapon == wp_chainsaw && player->powers[pw_strength]))
				player->pendingweapon = wp_chainsaw;
			else
				player->pendingweapon = wp_fist;
		}
		if ( buttons & BT_2 )
			player->pendingweapon = wp_pistol;
		if ( (buttons & BT_3) && player->weaponowned[wp_shotgun] )
			player->pendingweapon = wp_shotgun;
		if ( (buttons & BT_4) && player->weaponowned[wp_chaingun] )
			player->pendingweapon = wp_chaingun;
		if ( (buttons & BT_5) && player->weaponowned[wp_missile] )
			player->pendingweapon = wp_missile;
		if ( (buttons & BT_6) && player->weaponowned[wp_plasma] )
			player->pendingweapon = wp_plasma;
		if ( (buttons & BT_7) && player->weaponowned[wp_bfg] )
			player->pendingweapon = wp_bfg;
#elif defined(MARS)
		if ((buttons & (BT_MODE | BT_START)) == (BT_MODE | BT_START))
			player->pendingweapon = wp_fist;
		if ((buttons & (BT_MODE | BT_A)) == (BT_MODE | BT_A))
			player->pendingweapon = wp_pistol;
		if ((buttons & (BT_MODE | BT_B)) == (BT_MODE | BT_B) && player->weaponowned[wp_shotgun])
			player->pendingweapon = wp_shotgun;
		if ((buttons & (BT_MODE | BT_C)) == (BT_MODE | BT_C) && player->weaponowned[wp_chaingun])
			player->pendingweapon = wp_chaingun;
		if ((buttons & (BT_MODE | BT_X)) == (BT_MODE | BT_X) && player->weaponowned[wp_missile])
			player->pendingweapon = wp_missile;
		if ((buttons & (BT_MODE | BT_Y)) == (BT_MODE | BT_Y) && player->weaponowned[wp_plasma])
			player->pendingweapon = wp_plasma;
		if ((buttons & (BT_MODE | BT_Z)) == (BT_MODE | BT_Z) && player->weaponowned[wp_bfg])
			player->pendingweapon = wp_bfg;
#endif

		if ((buttons & BT_RMBTN) && (oldbuttons & BT_RMBTN))
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
			int wp = player->readyweapon;
			do
			{
				if (--wp < 0)
					wp = NUMWEAPONS - 1;
			} while (wp != player->readyweapon && !P_CanFireWeapon(player, wp));
			player->pendingweapon = wp;
		}
		else if (buttons & BT_NWEAPN)
		{
			int wp = player->readyweapon;
			do
			{
				if (++wp == NUMWEAPONS)
					wp = 0;
			} while (wp != player->readyweapon && !P_CanFireWeapon(player, wp));
			player->pendingweapon = wp;
		}

		if (player->pendingweapon == player->readyweapon)
			player->pendingweapon = wp_nochange;
	}
	
/* */
/* check for use */
/* */
	if (buttons & BT_USE)
	{
		if (!player->usedown)
		{
			P_UseLines (player);
			player->usedown = true;
		}
	}
	else
		player->usedown = false;

ticphase = 24;
	if (buttons & BT_ATTACK)
	{
		player->attackdown++;
		if (player->attackdown > 30 && playernum == consoleplayer &&
		(player->readyweapon == wp_chaingun || player->readyweapon == wp_plasma) )
			stbar.specialFace = f_mowdown;			
	}
	else
		player->attackdown = 0;
			
/* */
/* cycle psprites */
/* */
ticphase = 25;
	P_MovePsprites (player);
ticphase = 26;
	
	
/* */
/* counters */
/* */
	if (gametic != prevgametic)
	{
		if (player->powers[pw_strength])
			player->powers[pw_strength]++;	/* strength counts up to diminish fade */

		if (player->powers[pw_invulnerability])
			player->powers[pw_invulnerability]--;

		if (player->powers[pw_ironfeet])
			player->powers[pw_ironfeet]--;

		if (player->damagecount)
			player->damagecount--;

		if (player->bonuscount)
			player->bonuscount--;
	}
}

void P_RestoreResp(player_t* p)
{
	int i;
	int pnum = p - players;
	playerresp_t* resp = &playersresp[pnum];

	for (i = 0; i < NUMAMMO; i++)
	{
		p->ammo[i] = resp->ammo[i];
		p->maxammo[i] = resp->maxammo[i];
	}
	for (i = 0; i < NUMWEAPONS; i++)
	{
		p->weaponowned[i] = resp->weaponowned[i];
	}
	p->health = resp->health;
	p->readyweapon = p->pendingweapon = resp->weapon;
	p->armorpoints = resp->armorpoints;
	p->armortype = resp->armortype;
	p->backpack = resp->backpack;
	p->cheats = resp->cheats;
}

void P_UpdateResp(player_t* p)
{
	int i;
	int pnum = p - players;
	playerresp_t* resp = &playersresp[pnum];

	for (i = 0; i < NUMAMMO; i++)
	{
		resp->ammo[i] = p->ammo[i];
		resp->maxammo[i] = p->maxammo[i];
	}
	for (i = 0; i < NUMWEAPONS; i++)
	{
		resp->weaponowned[i] = p->weaponowned[i];
	}
	resp->health = p->health;
	resp->weapon = p->readyweapon;
	resp->armorpoints = p->armorpoints;
	resp->armortype = p->armortype;
	resp->backpack = p->backpack;
	resp->cheats = p->cheats;
}
