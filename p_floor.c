#include "doomdef.h"
#include "p_local.h"

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
result_e	T_MovePlane(mover_t *mover,fixed_t speed,
			fixed_t dest,boolean crush,int floorOrCeiling,int direction)
{
	boolean	flag;
	fixed_t	lastpos;
	sector_t *sector = mover->sector;
	
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
						flag = P_ChangeMover(mover,crush);
						if (flag == true)
						{
							sector->floorheight =lastpos;
							P_ChangeMover(mover,crush);
							/*return crushed; */
						}
						return pastdest;
					}
					else
					{
						lastpos = sector->floorheight;
						sector->floorheight -= speed;
						flag = P_ChangeMover(mover,crush);
						if (flag == true)
						{
							sector->floorheight = lastpos;
							P_ChangeMover(mover,crush);
							return crushed;
						}
					}
					break;
						
				case 1:		/* UP */
					if (sector->floorheight + speed > dest)
					{
						lastpos = sector->floorheight;
						sector->floorheight = dest;
						flag = P_ChangeMover(mover,crush);
						if (flag == true)
						{
							sector->floorheight = lastpos;
							P_ChangeMover(mover,crush);
							/*return crushed; */
						}
						return pastdest;
					}
					else	/* COULD GET CRUSHED */
					{
						lastpos = sector->floorheight;
						sector->floorheight += speed;
						flag = P_ChangeMover(mover,crush);
						if (flag == true)
						{
							if (crush == true)
								return crushed;
							sector->floorheight = lastpos;
							P_ChangeMover(mover,crush);
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
						flag = P_ChangeMover(mover,crush);
						if (flag == true)
						{
							sector->ceilingheight = lastpos;
							P_ChangeMover(mover,crush);
							/*return crushed; */
						}
						return pastdest;
					}
					else	/* COULD GET CRUSHED */
					{
						lastpos = sector->ceilingheight;
						sector->ceilingheight -= speed;
						flag = P_ChangeMover(mover,crush);
						if (flag == true)
						{
							if (crush == true)
								return crushed;
							sector->ceilingheight = lastpos;
							P_ChangeMover(mover,crush);
							return crushed;
						}
					}
					break;
						
				case 1:		/* UP */
					if (sector->ceilingheight + speed > dest)
					{
						lastpos = sector->ceilingheight;
						sector->ceilingheight = dest;
						flag = P_ChangeMover(mover,crush);
						if (flag == true)
						{
							sector->ceilingheight = lastpos;
							P_ChangeMover(mover,crush);
							/*return crushed; */
						}
						return pastdest;
					}
					else
					{
						lastpos = sector->ceilingheight;
						sector->ceilingheight += speed;
						flag = P_ChangeMover(mover,crush);
						#if 0
						if (flag == true)
						{
							sector->ceilingheight = lastpos;
							P_ChangeMover(mover,crush);
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
void T_MoveFloor(floormove_t *floor)
{
	result_e	res;
	
	res = T_MovePlane(&floor->m,floor->speed,
			floor->floordestheight,floor->crush,0,floor->direction);
	if (!(gametic&3))
		P_MoverSound(&floor->m,sfx_stnmov);
	if (res == pastdest)
	{
		floor->m.sector->specialdata = (SPTR)0;
		if (floor->direction == 1)
			switch(floor->type)
			{
				case donutRaise:
					floor->m.sector->special = floor->newspecial;
					floor->m.sector->floorpic = floor->texture;
				default:
					break;
			}
		else if (floor->direction == -1)
			switch(floor->type)
			{
				case lowerAndChange:
					floor->m.sector->special = floor->newspecial;
					floor->m.sector->floorpic = floor->texture;
				default:
					break;
			}
		P_RemoveThinker(&floor->thinker);
	}

}

/*================================================================== */
/* */
/*	HANDLE FLOOR TYPES */
/* */
/*================================================================== */
int EV_DoFloorTag(line_t *line,floor_e floortype, int tag)
{
	int 		k;
	int			secnum;
	int			rtn;
	int			i;
	sector_t	*sec;
	floormove_t	*floor;

	k = 0;
	rtn = 0;
	while ((secnum = P_FindNextSectorByTagNum(tag,&k)) >= 0)
	{
		sec = &sectors[secnum];
		
		/*	ALREADY MOVING?  IF SO, KEEP GOING... */
		if (sec->specialdata)
			continue;
			
		/* */
		/*	new floor thinker */
		/* */
		rtn = 1;
		floor = P_SpawnThinker (*floor);
		sec->specialdata = LPTR_TO_SPTR(floor);
		floor->thinker.function = T_MoveFloor;
		floor->type = floortype;
		floor->crush = false;
		floor->m.sector = sec;
		P_SectorBBox(sec, floor->m.secbbox);
		switch(floortype)
		{
			case lowerFloor:
				floor->direction = -1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindHighestFloorSurrounding(sec);
				break;
			case lowerFloorToLowest:
				floor->direction = -1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindLowestFloorSurrounding(sec);
				break;
			case turboLower:
				floor->direction = -1;
				floor->speed = FLOORSPEED * 4;
				floor->floordestheight = (8*FRACUNIT) + 
						P_FindHighestFloorSurrounding(sec);
				break;
			case raiseFloorCrush:
				floor->crush = true;
			case raiseFloor:
				floor->direction = 1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindLowestCeilingSurrounding(sec);
				if (floor->floordestheight > sec->ceilingheight)
					floor->floordestheight = sec->ceilingheight;
				break;
			case raiseFloorTurbo:
				floor->direction = 1;
				floor->speed = FLOORSPEED*4;
				floor->floordestheight = 
				P_FindNextHighestFloor(sec,sec->floorheight);
				break;
			case raiseFloorToNearest:
				floor->direction = 1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindNextHighestFloor(sec,sec->floorheight);
				break;
			case raiseFloor24:
				floor->direction = 1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = floor->m.sector->floorheight +
						24 * FRACUNIT;
				break;
			case raiseFloor512:
				floor->direction = 1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = floor->m.sector->floorheight +
					512 * FRACUNIT;
				break;
			case raiseFloor24AndChange:
				floor->direction = 1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = floor->m.sector->floorheight +
						24 * FRACUNIT;
				sec->floorpic = LD_FRONTSECTOR(line)->floorpic;
				sec->special = LD_FRONTSECTOR(line)->special;
				break;
			case raiseToTexture:
				{
					int	minsize = D_MAXINT;
					side_t	*side;
				
					floor->direction = 1;
					floor->speed = FLOORSPEED;
					for (i = 0; i < sec->linecount; i++)
						if (twoSided (secnum, i) )
						{
							side = getSide(secnum,i,0);
							if (side->bottomtexture > 0)
								if (
					(textures[side->bottomtexture].height<<FRACBITS)  < 
									minsize)
									minsize = 
										(textures[side->bottomtexture].height<<FRACBITS);
							side = getSide(secnum,i,1);
							if (side->bottomtexture > 0)
								if ((textures[side->bottomtexture].height<<FRACBITS) < 
									minsize)
									minsize = 
										(textures[side->bottomtexture].height<<FRACBITS);
						} 
					floor->floordestheight = floor->m.sector->floorheight + 
						minsize;
				}
				break;
			case lowerAndChange:
				floor->direction = -1;
				floor->speed = FLOORSPEED;
				floor->floordestheight = 
					P_FindLowestFloorSurrounding(sec);
				floor->texture = sec->floorpic;
				for (i = 0; i < sec->linecount; i++)
					if ( twoSided(secnum, i) )
					{
						if (getSide(secnum,i,0)->sector == secnum)
						{
							sec = getSector(secnum,i,1);
							if (sec->floorheight == floor->floordestheight)
							{
								floor->texture = sec->floorpic;
								floor->newspecial = sec->special;
								break;
							}
						}
						else
						{
							sec = getSector(secnum,i,0);
							if (sec->floorheight == floor->floordestheight)
							{
								floor->texture = sec->floorpic;
								floor->newspecial = sec->special;
								break;
							}
						}
					}
				break;
			default:
				floor->m.sector = NULL;
				sec->specialdata = (SPTR)0;
				P_RemoveThinker(&floor->thinker);
				break;
		}
	}
	return rtn;
}

int EV_DoFloor(line_t *line,floor_e floortype)
{
	return EV_DoFloorTag(line, floortype, P_GetLineTag(line));
}

/*================================================================== */
/* */
/*	BUILD A STAIRCASE! */
/* */
/*================================================================== */
int EV_BuildStairs(line_t *line, int type)
{
	int 	k;
	int		secnum;
	int		height;
	int		i;
	int		newsecnum;
	int		texture;
	int		ok;
	int		rtn;
	fixed_t speed, stairsize;
	sector_t	*sec, *tsec;
	floormove_t	*floor;
	int 	tag;

	k = 0;
	rtn = 0;
	tag = P_GetLineTag(line);
	while ((secnum = P_FindNextSectorByTagNum(tag,&k)) >= 0)
	{
		sec = &sectors[secnum];
		
		/* ALREADY MOVING?  IF SO, KEEP GOING... */
		if (sec->specialdata)
			continue;
			
		/* */
		/* new floor thinker */
		/* */
		rtn = 1;
		floor = P_SpawnThinker (*floor);
		sec->specialdata = LPTR_TO_SPTR(floor);
		floor->thinker.function = T_MoveFloor;
		floor->direction = 1;
		floor->m.sector = sec;
		P_SectorBBox(sec, floor->m.secbbox);
		switch (type)
		{
		case build8:
		default:
			speed = FLOORSPEED / 2;
			stairsize = 8 * FRACUNIT;
			break;
		case turbo16:
			speed = FLOORSPEED;
			stairsize = 16 * FRACUNIT;
			break;
		}
		height = sec->floorheight + stairsize;
		floor->speed = speed;
		floor->floordestheight = height;
        // Initialize
        floor->type = lowerFloor;

		texture = sec->floorpic;

		/* */
		/* Find next sector to raise */
		/* 1.	Find 2-sided line with same sector side[0] */
		/* 2.	Other side is the next sector to raise */
		/* */
		do
		{
			ok = 0;
			for (i = 0;i < sec->linecount;i++)
			{
				line_t *check = lines + sec->lines[i];
				boolean twoSided = check->sidenum[1] != -1;
				if ( !twoSided )
					continue;
					
				newsecnum = sides[check->sidenum[0]].sector;
				tsec = &sectors[newsecnum];
				if (secnum != newsecnum)
					continue;
				newsecnum = sides[check->sidenum[1]].sector;
				tsec = &sectors[newsecnum];
				if (tsec->floorpic != texture)
					continue;
					
				height += stairsize;
				if (tsec->specialdata)
					continue;
					
				sec = tsec;
				secnum = newsecnum;
				floor = P_SpawnThinker (*floor);
				sec->specialdata = LPTR_TO_SPTR(floor);
				floor->thinker.function = T_MoveFloor;
				floor->direction = 1;
				floor->m.sector = sec;
				P_SectorBBox(sec, floor->m.secbbox);
				floor->speed = speed;
				floor->floordestheight = height;
                // Initialize
                floor->type = lowerFloor;
				ok = 1;
				break;
			}
		} while(ok);
	}
	return rtn;
}

