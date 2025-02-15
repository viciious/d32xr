#include "doomdef.h"
#include "p_local.h"

//e6y
#define STAIRS_UNINITIALIZED_CRUSH_FIELD_VALUE 10

/*================================================================== */
/*================================================================== */
/* */
/*								FLOORS */
/* */
/*================================================================== */
/*================================================================== */



/*================================================================== */
/* */
/*	Move a plane (floor or ceiling) and check for crushing */
/* */
/*================================================================== */
result_e	T_MovePlane(sector_t *sector,fixed_t speed,
			fixed_t dest,boolean crush,int floorOrCeiling,int direction)
{
	boolean	flag;
	fixed_t	lastpos;
	
	switch(floorOrCeiling)
	{
		case 0:		/* FLOOR */
			switch(direction)
			{
				case -1:	/* DOWN */
					if (sector->floorheight - speed < dest)
					{
						lastpos = sector->floorheight;
						sector->floorheight = dest;
						flag = P_ChangeSector(sector,crush);
						if (flag == true)
						{
							sector->floorheight =lastpos;
							P_ChangeSector(sector,crush);
							/*return crushed; */
						}
						return pastdest;
					}
					else
					{
						lastpos = sector->floorheight;
						sector->floorheight -= speed;
						flag = P_ChangeSector(sector,crush);
						if (flag == true)
						{
							sector->floorheight = lastpos;
							P_ChangeSector(sector,crush);
							return crushed;
						}
					}
					break;
						
				case 1:		/* UP */
					if (sector->floorheight + speed > dest)
					{
						lastpos = sector->floorheight;
						sector->floorheight = dest;
						flag = P_ChangeSector(sector,crush);
						if (flag == true)
						{
							sector->floorheight = lastpos;
							P_ChangeSector(sector,crush);
							/*return crushed; */
						}
						return pastdest;
					}
					else	/* COULD GET CRUSHED */
					{
						lastpos = sector->floorheight;
						sector->floorheight += speed;
						flag = P_ChangeSector(sector,crush);
						if (flag == true)
						{
							if (crush == true)
								return crushed;
							sector->floorheight = lastpos;
							P_ChangeSector(sector,crush);
							return crushed;
						}
					}
					break;
			}
			break;
									
		case 1:		/* CEILING */
			switch(direction)
			{
				case -1:	/* DOWN */
					if (sector->ceilingheight - speed < dest)
					{
						lastpos = sector->ceilingheight;
						sector->ceilingheight = dest;
						flag = P_ChangeSector(sector,crush);
						if (flag == true)
						{
							sector->ceilingheight = lastpos;
							P_ChangeSector(sector,crush);
							/*return crushed; */
						}
						return pastdest;
					}
					else	/* COULD GET CRUSHED */
					{
						lastpos = sector->ceilingheight;
						sector->ceilingheight -= speed;
						flag = P_ChangeSector(sector,crush);
						if (flag == true)
						{
							if (crush == true)
								return crushed;
							sector->ceilingheight = lastpos;
							P_ChangeSector(sector,crush);
							return crushed;
						}
					}
					break;
						
				case 1:		/* UP */
					if (sector->ceilingheight + speed > dest)
					{
						lastpos = sector->ceilingheight;
						sector->ceilingheight = dest;
						flag = P_ChangeSector(sector,crush);
						if (flag == true)
						{
							sector->ceilingheight = lastpos;
							P_ChangeSector(sector,crush);
							/*return crushed; */
						}
						return pastdest;
					}
					else
					{
						lastpos = sector->ceilingheight;
						sector->ceilingheight += speed;
						flag = P_ChangeSector(sector,crush);
						#if 0
						if (flag == true)
						{
							sector->ceilingheight = lastpos;
							P_ChangeSector(sector,crush);
							return crushed;
						}
						#endif
					}
					break;
			}
			break;
		
	}
	return ok;
}

/*================================================================== */
/* */
/*	MOVE A FLOOR TO IT'S DESTINATION (UP OR DOWN) */
/* */
/*================================================================== */
#define ELEVATORSPEED (FRACUNIT / 4)
void T_MoveFloor(floormove_t *floor)
{
	result_e	res;

	if (floor->crush)
	{
		floor->crush--;
		return;
	}

	if (floor->type == floorContinuous)
	{
		const fixed_t wh = D_abs(floor->sector->floorheight - (floor->floorwasheight << FRACBITS));
		const fixed_t dh = D_abs(floor->sector->floorheight - (floor->floordestheight << FRACBITS));

		// Slow down when reaching destination Tails 12-06-2000
		if (wh < dh)
			floor->speed = FixedDiv(wh, 25*FRACUNIT) + FRACUNIT/4;
		else
			floor->speed = FixedDiv(dh, 25*FRACUNIT) + FRACUNIT/4;

		if (floor->origSpeed)
		{
			floor->speed = FixedMul(floor->speed, floor->origSpeed);
			if (floor->speed > floor->origSpeed)
				floor->speed = (floor->origSpeed);
			if (floor->speed < 1)
				floor->speed = 1;
		}
		else
		{
			if (floor->speed > 3*FRACUNIT)
				floor->speed = 3*FRACUNIT;
			if (floor->speed < 1)
				floor->speed = 1;
		}

		res = T_MovePlane(floor->sector,floor->speed,
				floor->floordestheight << FRACBITS,0,0,floor->direction);
	}
	else if (floor->type == eggCapsuleInner || floor->type == eggCapsuleOuter
		|| floor->type == eggCapsuleOuterPop || floor->type == eggCapsuleInnerPop)
	{
		res = T_MovePlane(floor->sector,floor->speed,
				floor->floordestheight << FRACBITS,floor->crush,0,floor->direction);
	}

	if (res == pastdest)
	{
		if (floor->type == floorContinuous)
		{
			if (floor->direction > 0)
			{
				floor->direction = -1;
				floor->speed = floor->origSpeed;
				floor->floorwasheight = floor->floordestheight;
				floor->floordestheight = P_FindNextLowestFloor(floor->controlSector, floor->sector->floorheight) >> FRACBITS;
			}
			else
			{
				floor->direction = 1;
				floor->speed = floor->origSpeed;
				floor->floorwasheight = floor->floordestheight;
				floor->floordestheight = P_FindNextHighestFloor(floor->controlSector, floor->sector->floorheight) >> FRACBITS;
			}
		}
		else
		{
			floor->sector->specialdata = NULL;
			if (floor->direction == 1)
				switch(floor->type)
				{
					case donutRaise:
						floor->sector->special = floor->newspecial;
						floor->sector->floorpic = floor->texture;
						break;
					case eggCapsuleInner:
						floor->sector->special = 2;
						break;
					default:
						break;
				}
			else if (floor->direction == -1)
				switch(floor->type)
				{
					case lowerAndChange:
						floor->sector->special = floor->newspecial;
						floor->sector->floorpic = floor->texture;
					default:
						break;
				}
			P_RemoveThinker(&floor->thinker);
		}
	}
}

/*================================================================== */
/* */
/*	HANDLE FLOOR TYPES */
/* */
/*================================================================== */
int EV_DoFloorTag(line_t *line,floor_e floortype, uint8_t tag)
{
	int			secnum;
	int			rtn;
	int			i;
	sector_t	*sec;
	floormove_t	*floor;

	secnum = -1;
	rtn = 0;
	while ((secnum = P_FindSectorFromLineTagNum(tag,secnum)) >= 0)
	{
		sec = &sectors[secnum];
		
		/*	ALREADY MOVING?  IF SO, KEEP GOING... */
		if (sec->specialdata)
			continue;
			
		/* */
		/*	new floor thinker */
		/* */
		rtn = 1;
		floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC);
		P_AddThinker (&floor->thinker);
		sec->specialdata = floor;
		floor->thinker.function = T_MoveFloor;
		floor->type = floortype;
		floor->crush = false;
		switch(floortype)
		{
			case floorContinuous:
				floor->sector = sec;
				floor->controlSector = &sectors[sides[line->sidenum[0]].sector];
				floor->origSpeed = P_AproxDistance((vertexes[line->v1].x - vertexes[line->v2].x) << FRACBITS,
												(vertexes[line->v1].y - vertexes[line->v2].y) << FRACBITS) / 4;
				floor->speed = floor->origSpeed;

				if (line->flags & ML_NOCLIMB)
				{
					floor->direction = 1;
					floor->floordestheight = P_FindNextHighestFloor(floor->controlSector, sec->floorheight) >> FRACBITS;
				}
				else
				{
					floor->direction = -1;
					floor->floordestheight = P_FindNextLowestFloor(floor->controlSector, sec->floorheight) >> FRACBITS;
				}

				floor->floorwasheight = sec->floorheight >> FRACBITS;
				floor->crush = sides[line->sidenum[0]].rowoffset; // initial delay
				break;
			case lowerFloor:
				floor->direction = -1;
				floor->sector = sec;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindHighestFloorSurrounding(sec);
				break;
			case lowerFloorToLowest:
				floor->direction = -1;
				floor->sector = sec;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindLowestFloorSurrounding(sec);
				break;
			case turboLower:
				floor->direction = -1;
				floor->sector = sec;
				floor->speed = FLOORSPEED * 4;
				floor->floordestheight = (8*FRACUNIT) + 
						P_FindHighestFloorSurrounding(sec);
				break;
			case raiseFloorCrush:
				floor->crush = true;
			case raiseFloor:
				floor->direction = 1;
				floor->sector = sec;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindLowestCeilingSurrounding(sec);
				if (floor->floordestheight > sec->ceilingheight)
					floor->floordestheight = sec->ceilingheight;
				break;
			case raiseFloorToNearest:
				floor->direction = 1;
				floor->sector = sec;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindNextHighestFloor(sec,sec->floorheight);
				break;
			case raiseFloor24:
				floor->direction = 1;
				floor->sector = sec;
				floor->speed = FLOORSPEED;
				floor->floordestheight = floor->sector->floorheight +
						24 * FRACUNIT;
				break;
			case raiseFloor512:
				floor->direction = 1;
				floor->sector = sec;
				floor->speed = FLOORSPEED;
				floor->floordestheight = floor->sector->floorheight +
					512 * FRACUNIT;
				break;
			case raiseFloor24AndChange:
				floor->direction = 1;
				floor->sector = sec;
				floor->speed = FLOORSPEED;
				floor->floordestheight = floor->sector->floorheight +
						24 * FRACUNIT;
				sec->floorpic = LD_FRONTSECTOR(line)->floorpic;
				sec->special = LD_FRONTSECTOR(line)->special;
				break;
			default:
				break;
		}
	}
	return rtn;
}

int EV_DoFloor(line_t *line,floor_e floortype)
{
	return EV_DoFloorTag(line, floortype, P_GetLineTag(line));
}
