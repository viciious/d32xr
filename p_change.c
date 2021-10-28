/* p_change.c */

#include "doomdef.h"
#include "p_local.h"

/* 
============================================================================== 

						SECTOR HEIGHT CHANGING

= After modifying a sectors floor or ceiling height, call this
= routine to adjust the positions of all things that touch the
= sector.
=
= If anything doesn't fit anymore, true will be returned.
= If crunch is true, they will take damage as they are being crushed
= If Crunch is false, you should set the sector height back the way it
= was and call P_ChangeSector again to undo the changes
============================================================================== 
*/ 
 
boolean		crushchange;
boolean		nofit;

/*
==================
=
= P_ThingHeightClip
=
= Takes a valid thing and adjusts the thing->floorz, thing->ceilingz,
= anf possibly thing->z
=
= This is called for all nearby monsters whenever a sector changes height
=
= If the thing doesn't fit, the z will be set to the lowest value and
= false will be returned
==================
*/

boolean P_ThingHeightClip (mobj_t *thing)
{
	boolean		onfloor;
	
	onfloor = (thing->z == thing->floorz);
	
	P_CheckPosition (thing, thing->x, thing->y);	
	/* what about stranding a monster partially off an edge? */
	
	thing->floorz = tmfloorz;
	thing->ceilingz = tmceilingz;
	
	if (onfloor)
	/* walking monsters rise and fall with the floor */
		thing->z = thing->floorz;
	else
	{	/* don't adjust a floating monster unless forced to */
		if (thing->z+thing->height > thing->ceilingz)
			thing->z = thing->ceilingz - thing->height;
	}
	
	if (thing->ceilingz - thing->floorz < thing->height)
		return false;
		
	return true;
}


/*
===============
=
= PIT_ChangeSector
=
===============
*/

boolean PIT_ChangeSector (mobj_t *thing)
{
	mobj_t		*mo;
	
	if (P_ThingHeightClip (thing))
		return true;		/* keep checking */

	/* crunch bodies to giblets */
	if (thing->health <= 0)
	{
		P_SetMobjState (thing, S_GIBS);
		thing->height = 0;
		thing->radius = 0;
		return true;		/* keep checking */
	}

	/* crunch dropped items */
	if (thing->flags & MF_DROPPED)
	{
		P_RemoveMobj (thing);
		return true;		/* keep checking */
	}

	if (! (thing->flags & MF_SHOOTABLE) )
		return true;				/* assume it is bloody gibs or something */
		
	nofit = true;
	if (crushchange && !(gametic&3) && (gametic!=prevgametic) )
	{
		P_DamageMobj(thing,NULL,NULL,10);
		/* spray blood in a random direction */
		mo = P_SpawnMobj (thing->x, thing->y, thing->z + thing->height/2, MT_BLOOD);
		mo->momx = (P_Random() - P_Random ())<<12;
		mo->momy = (P_Random() - P_Random ())<<12;
	}
		
	return true;		/* keep checking (crush other things)	 */
}

/*
===============
=
= P_ChangeSector
=
===============
*/

boolean P_ChangeSector (sector_t *sector, boolean crunch)
{
	int			x,y;
	int			i;
	
/* force next sound to reflood */
	for (i=0 ; i<MAXPLAYERS ; i++)
		players[i].lastsoundsector = NULL;
		
	nofit = false;
	crushchange = crunch;
	
/* recheck heights for all things near the moving sector */

	for (x=sector->blockbox[BOXLEFT] ; x<= sector->blockbox[BOXRIGHT] ; x++)
		for (y=sector->blockbox[BOXBOTTOM];y<= sector->blockbox[BOXTOP] ; y++)
			P_BlockThingsIterator (x, y, PIT_ChangeSector);
	
	
	return nofit;
}


