/* P_pspr.c */
#include "doomdef.h"

void P_NoiseAlert (player_t *player)
{

}


/* 
================== 
= 
= P_SetPsprite
= 
================== 
*/ 
 
void P_SetPsprite (player_t *player, int position, statenum_t stnum) 
{
	
}


/*
===============================================================================

						PSPRITE ACTIONS

===============================================================================
*/

/*
================
=
= P_BringUpWeapon
=
= Starts bringing the pending weapon up from the bottom of the screen
= Uses player
================
*/

void P_BringUpWeapon (player_t *player)
{
	
}

/*
================
=
= P_CheckAmmo
=
= returns true if there is enough ammo to shoot
= if not, selects the next weapon to use
================
*/

boolean P_CheckAmmo (player_t *player)
{
	return false;	
}


/*
================
=
= P_FireWeapon
=
================
*/

void P_FireWeapon (player_t *player)
{

}


/*
================
=
= P_DropWeapon
=
= Player died, so put the weapon away
================
*/

void P_DropWeapon (player_t *player)
{
}

/*============================================================================= */

/* 
================== 
= 
= P_SetupPsprites
= 
= Called at start of level for each player
================== 
*/ 
 
void P_SetupPsprites (player_t *player) 
{
}



/* 
================== 
= 
= P_MovePsprites 
= 
= Called every tic by player thinking routine
================== 
*/ 
 
 VINT ticremainder[MAXPLAYERS];

void P_MovePsprites (player_t *player) 
{

}


