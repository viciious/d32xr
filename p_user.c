/* P_user.c */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"
#include "p_camera.h"


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


void P_PlayerMove (mobj_t *mo)
{
	fixed_t		momx, momy;
	int 		i;
	pslidemove_t sm;
	ptrymove_t	tm;

	momx = vblsinframe*(mo->momx>>2);
	momy = vblsinframe*(mo->momy>>2);

	sm.slidething = mo;
	
	P_SlideMove(&sm);

	if (sm.slidex == mo->x && sm.slidey == mo->y)
		goto stairstep;
		
	if ( P_TryMove (&tm, mo, sm.slidex, sm.slidey) )
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

	if (P_TryMove (&tm, mo, mo->x, mo->y + momy))
	{
		mo->momx = 0;
		mo->momy = momy;
		goto dospecial;
	}
	
	if (P_TryMove (&tm, mo, mo->x + momx, mo->y))
	{
		mo->momx = momx;
		mo->momy = 0;
		goto dospecial;
	}

	mo->momx = mo->momy = 0;

dospecial:
	for (i = 0; i < sm.numspechit; i++)
		P_CrossSpecialLine (sm.spechit[i], mo);
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
		fixed_t gravity = (GRAVITY * vblsinframe) / TICVBLS;
		if (mo->momz == 0)
			mo->momz = -gravity;
		else
			mo->momz -= gravity/2;
	}
	
	if (mo->z + (mo->theight << FRACBITS) > mo->ceilingz)
	{	/* hit the ceiling */
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
	int         speed;
	int			buttons, oldbuttons;
	mobj_t		*mo;
	int			vbls;

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

			speed = (buttons & BT_SPEED) > 0;
 
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
		speed = (buttons & BT_SPEED) > 0;

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

		if (buttons & BT_STRAFE)
		{
			if (buttons & BT_RIGHT)
				player->sidemove += (sidemove[speed] * vbls) / TICVBLS;
			if (buttons & BT_LEFT)
				player->sidemove += (-sidemove[speed] * vbls) / TICVBLS;
		}
		else
		{
			fixed_t *turnspeed;

			if (buttons & BT_STRAFERIGHT)
				player->sidemove += (sidemove[speed] * vbls) / TICVBLS;
			if (buttons & BT_STRAFELEFT)
				player->sidemove += (-sidemove[speed] * vbls) / TICVBLS;

			turnspeed = angleturn;
			if (buttons & BT_FASTTURN)
				turnspeed = fastangleturn;

			if (buttons & BT_RIGHT)
				player->angleturn = ((-turnspeed[player->turnheld] * vbls) / TICVBLS) << 17;
			if (buttons & BT_LEFT)
				player->angleturn = ((turnspeed[player->turnheld] * vbls) / TICVBLS) << 17;
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
		if (mo->state >= S_PLAY_RUN1 
			&& mo->state <= S_PLAY_RUN8)
			P_SetMobjState (mo, S_PLAY_STND);
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
	int			maxbob;
	fixed_t		bob;
	fixed_t		movx, movy;

/* */
/* regular movement bobbing (needs to be calculated for gun swing even */
/* if not on ground) */
/* OPTIMIZE: tablify angle  */
	movx = player->mo->momx;
	movy = player->mo->momy;

	if (vblsinframe <= 2)
	{
		movx = FixedDiv(movx, FRICTION);
		movy = FixedDiv(movy, FRICTION);
	}
	movx >>= 8;
	movy >>= 8;

	player->bob += movx*movx + movy*movy;
	player->bob >>= 4;

	maxbob = MAXBOB;

	if (player->bob > maxbob)
		player->bob = maxbob;

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

static void P_DoTeeter(player_t *player)
{
	boolean teeter = false;
	const fixed_t tiptop = 24*FRACUNIT; // Gotta be farther than the step height

	subsector_t *a = R_PointInSubsector(player->mo->x + 5*FRACUNIT, player->mo->y + 5*FRACUNIT);
	subsector_t *b = R_PointInSubsector(player->mo->x - 5*FRACUNIT, player->mo->y + 5*FRACUNIT);
	subsector_t *c = R_PointInSubsector(player->mo->x + 5*FRACUNIT, player->mo->y - 5*FRACUNIT);
	subsector_t *d = R_PointInSubsector(player->mo->x - 5*FRACUNIT, player->mo->y - 5*FRACUNIT);

	if (a->sector->floorheight < player->mo->floorz - tiptop
			|| b->sector->floorheight < player->mo->floorz - tiptop
			|| c->sector->floorheight < player->mo->floorz - tiptop
			|| d->sector->floorheight < player->mo->floorz - tiptop)
		teeter = true;

	if (teeter)
	{
		if (player->mo->state >= S_PLAY_STND && player->mo->state <= S_PLAY_TAP2)
			P_SetMobjState(player->mo, S_PLAY_TEETER1);
	}
	else if (player->mo->state == S_PLAY_TEETER1 || player->mo->state == S_PLAY_TEETER2)
		P_SetMobjState(player->mo, S_PLAY_STND);
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
	
	if (player->forwardmove)
		P_Thrust (player, player->mo->angle, player->forwardmove*2);
	if (player->sidemove)
		P_Thrust (player, player->mo->angle-ANG90, player->sidemove*2);

	if ( (player->forwardmove || player->sidemove) 
	&& (player->mo->state >= S_PLAY_STND && player->mo->state <= S_PLAY_TAP2) )
		P_SetMobjState (player->mo, S_PLAY_RUN1);

	// Make sure you're not teetering when you shouldn't be.
	if ((player->mo->state == S_PLAY_TEETER1 || player->mo->state == S_PLAY_TEETER2)
		&& (player->mo->momx || player->mo->momy || player->mo->momz))
		P_SetMobjState(player->mo, S_PLAY_STND);

	if (!player->mo->momz &&
	((!(player->mo->momx || player->mo->momy) && (player->mo->state == S_PLAY_STND
	|| player->mo->state == S_PLAY_TAP1 || player->mo->state == S_PLAY_TAP2
	|| player->mo->state == S_PLAY_TEETER1 || player->mo->state == S_PLAY_TEETER2))))
		P_DoTeeter(player);
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

	if (player->playerstate == PST_DEAD)
	{
		if (player == &players[consoleplayer])
			P_MoveChaseCamera(player, &camera);
		P_DeathThink (player);
		return;
	}
		
/* */
/* move around */
ticphase = 22;
	P_MovePlayer (player);
	P_CalcHeight (player);

	if (player == &players[consoleplayer])
		P_MoveChaseCamera(player, &camera);

	if (player->mo->subsector->sector->special)
		P_PlayerInSpecialSector (player);
		
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
		if (!player->usedown)
		{
			P_UseLines (player);
			player->usedown = true;
			player->mo->momz = 16 << FRACBITS;
		}
	}
	else
		player->usedown = false;

ticphase = 24;
	if (buttons & BT_ATTACK)
	{
		player->attackdown++;
	}
	else
		player->attackdown = 0;
			
ticphase = 25;
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

		if (player->powers[pw_infrared])
			player->powers[pw_infrared]--;

		if (player->damagecount)
			player->damagecount--;

		if (player->bonuscount)
			player->bonuscount--;
	}
}

void R_ResetResp(player_t* p)
{
	int pnum = p - players;
	playerresp_t* resp = &playersresp[pnum];

	D_memset(resp, 0, sizeof(playerresp_t));
	resp->health = MAXHEALTH;
}

void P_RestoreResp(player_t* p)
{
	int pnum = p - players;
	playerresp_t* resp = &playersresp[pnum];

	p->health = resp->health;
	p->armorpoints = resp->armorpoints;
	p->armortype = resp->armortype;
	p->cheats = resp->cheats;
}

void P_UpdateResp(player_t* p)
{
	int pnum = p - players;
	playerresp_t* resp = &playersresp[pnum];

	resp->health = p->health;
	resp->armorpoints = p->armorpoints;
	resp->armortype = p->armortype;
	resp->cheats = p->cheats;
}
