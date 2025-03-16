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

typedef struct
{
	boolean		crushchange;
	boolean		nofit;
	fixed_t 	bbox[4];
} changetest_t;

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
	pmovework_t tm;
	fixed_t 	height;
	
	onfloor = (thing->z == thing->floorz);
	
	P_CheckPosition (&tm, thing, thing->x, thing->y);	
	/* what about stranding a monster partially off an edge? */
	
	thing->floorz = tm.tmfloorz;
	thing->ceilingz = tm.tmceilingz;
	
	height = thing->height*FRACUNIT;

	if (onfloor)
	/* walking monsters rise and fall with the floor */
		thing->z = thing->floorz;
	else
	{	/* don't adjust a floating monster unless forced to */
		if (thing->z+height > thing->ceilingz)
			thing->z = thing->ceilingz - height;
	}
	
	if (thing->ceilingz - thing->floorz < height)
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

boolean PIT_ChangeSector (mobj_t *thing, changetest_t *ct)
{
	mobj_t		*mo;
	fixed_t 	thbbox[4];

	thbbox[BOXTOP   ] = thing->y + (thing->radius*FRACUNIT);
	thbbox[BOXBOTTOM] = thing->y - (thing->radius*FRACUNIT);
	thbbox[BOXRIGHT ] = thing->x + (thing->radius*FRACUNIT);
	thbbox[BOXLEFT  ] = thing->x - (thing->radius*FRACUNIT);

	if (thbbox[BOXTOP] < ct->bbox[BOXBOTTOM])
		return true;
	if (thbbox[BOXBOTTOM] > ct->bbox[BOXTOP])
		return true;
	if (thbbox[BOXRIGHT] < ct->bbox[BOXLEFT])
		return true;
	if (thbbox[BOXLEFT] > ct->bbox[BOXRIGHT])
		return true;

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
		
	ct->nofit = true;
	if (ct->crushchange && !(gametic&3) && (gametic!=prevgametic) )
	{
		P_DamageMobj(thing,NULL,NULL,10);
		/* spray blood in a random direction */
		mo = P_SpawnMobj2 (thing->x, thing->y, thing->z + (thing->height*FRACUNIT)/2, MT_BLOOD, thing->subsector);
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
	changetest_t ct;
	fixed_t 	block;
	VINT 		blockbox[4];

	/* force next sound to reflood */
	for (i=0 ; i<MAXPLAYERS ; i++)
		players[i].lastsoundsector = NULL;
		
	ct.nofit = false;
	ct.crushchange = crunch;

	/* adjust bounding box to map blocks */
	block = sector->bbox[BOXTOP] << FRACBITS;
	ct.bbox[BOXTOP] = block;
	block = (unsigned)(block-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapheight ? bmapheight-1 : block;
	blockbox[BOXTOP]=block;

	block = sector->bbox[BOXBOTTOM] << FRACBITS;
	ct.bbox[BOXBOTTOM] = block;
	block = (block-bmaporgy-MAXRADIUS);
	block = block < 0 ? 0 : (unsigned)block>>MAPBLOCKSHIFT;
	blockbox[BOXBOTTOM]=block;

	block = sector->bbox[BOXRIGHT] << FRACBITS;
	ct.bbox[BOXRIGHT] = block;
	block = (unsigned)(block-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapwidth ? bmapwidth-1 : block;
	blockbox[BOXRIGHT]=block;

	block = sector->bbox[BOXLEFT] << FRACBITS;
	ct.bbox[BOXLEFT] = block;
	block = (block-bmaporgx-MAXRADIUS);
	block = block < 0 ? 0 : (unsigned)block>>MAPBLOCKSHIFT;
	blockbox[BOXLEFT]=block;

/* recheck heights for all things near the moving sector */

	for (x=blockbox[BOXLEFT] ; x<= blockbox[BOXRIGHT] ; x++)
		for (y=blockbox[BOXBOTTOM];y<= blockbox[BOXTOP] ; y++)
			P_BlockThingsIterator (x, y, (blockthingsiter_t)PIT_ChangeSector, &ct);
	
	return ct.nofit;
}


