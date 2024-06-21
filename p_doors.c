#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"

/*================================================================== */
/*================================================================== */
/* */
/*							VERTICAL DOORS */
/* */
/*================================================================== */
/*================================================================== */

/*================================================================== */
/* */
/*	T_VerticalDoor */
/* */
/*================================================================== */
void T_VerticalDoor (vldoor_t *door)
{
	result_e	res;
	
	switch(door->direction)
	{
		case 0:		/* WAITING */
			if (!--door->topcountdown)
				switch(door->type)
				{
					case normal:
						door->direction = -1; /* time to go back down */
						break;
					case blazeRaise:
						door->direction = -1; /* time to go back down */
						break;
					case close30ThenOpen:
						door->direction = 1;
						break;
					default:
						break;
				}
			break;
		case 2:		/*  INITIAL WAIT */
			if (!--door->topcountdown)
				switch(door->type)
				{
					case raiseIn5Mins:
						door->direction = 1;
						door->type = normal;
						break;
				default:
					break;
				}
			break;
		case -1:	/* DOWN */
			res = T_MovePlane(door->sector,door->speed,
				door->sector->floorheight,false,1,door->direction);
			if (res == pastdest)
				switch(door->type)
				{
					case normal:
					case close:
					case blazeRaise:
					case blazeClose:
						door->sector->specialdata = NULL;
						P_RemoveThinker (&door->thinker);  /* unlink and free */
						break;
					case close30ThenOpen:
						door->direction = 0;
						door->topcountdown = 15*30*THINKERS_TICS;
						break;
					default:
						break;
				}
			else if (res == crushed)
			{
				switch (door->type)
				{
					case blazeClose:
					case close:		/* DO NOT GO BACK UP! */
						break;
					case blazeRaise:
						door->direction = 1;
						break;
					default:
						door->direction = 1;
						break;
				}
			}
			break;
		case 1:		/* UP */
			res = T_MovePlane(door->sector,door->speed,
				door->topheight,false,1,door->direction);
			if (res == pastdest)
				switch(door->type)
				{
					case normal:
					case blazeRaise:
						door->direction = 0; /* wait at top */
						door->topcountdown = door->topwait;
						break;
					case close30ThenOpen:
					case open:
					case blazeOpen:
						door->sector->specialdata = NULL;
						P_RemoveThinker (&door->thinker);  /* unlink and free */
						break;
					default:
						break;
				}
			break;
	}
}


/*================================================================== */
/* */
/*		EV_DoDoor */
/*		Move a door up/down and all around! */
/* */
/*================================================================== */
int EV_DoDoor (line_t *line, vldoor_e  type)
{
	int			secnum,rtn;
	sector_t		*sec;
	vldoor_t		*door;
	
	secnum = -1;
	rtn = 0;
	while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
	{
		sec = &sectors[secnum];
		if (sec->specialdata)
			continue;
		
		/* */
		/* new door thinker */
		/* */
		rtn = 1;
		door = Z_Malloc (sizeof(*door), PU_LEVSPEC);
		P_AddThinker (&door->thinker);
		sec->specialdata = door;
		door->thinker.function = T_VerticalDoor;
		door->sector = sec;
		door->type = type;
		door->speed = VDOORSPEED;
		door->topwait = VDOORWAIT;
		switch(type)
		{
			case blazeClose:
				door->topheight = P_FindLowestCeilingSurrounding(sec);
				door->topheight -= 4*FRACUNIT;
				door->direction = -1;
				door->speed = VDOORSPEED*4;
				break;
			case close:
				door->topheight = P_FindLowestCeilingSurrounding(sec);
				door->topheight -= 4*FRACUNIT;
				door->direction = -1;
				break;
			case close30ThenOpen:
				door->topheight = sec->ceilingheight;
				door->direction = -1;
				break;
			case blazeRaise:
			case blazeOpen:
				door->direction = 1;
				door->topheight = P_FindLowestCeilingSurrounding(sec);
				door->topheight -= 4 * FRACUNIT;
				door->speed = VDOORSPEED * 4;
				break;
			case normal:
			case open:
				door->direction = 1;
				door->topheight = P_FindLowestCeilingSurrounding(sec);
				door->topheight -= 4*FRACUNIT;
				break;
			default:
				break;
		}
	}
	return rtn;
}

/*================================================================== */
/* */
/*	EV_VerticalDoor : open a door manually, no tag value */
/* */
/*================================================================== */
int EV_DoLockedDoor(line_t* line, vldoor_e type, mobj_t* thing)
{
	return EV_DoDoor(line, type);
}

/*================================================================== */
/* */
/*	EV_VerticalDoor : open a door manually, no tag value */
/* */
/*================================================================== */
void EV_VerticalDoor (line_t *line, mobj_t *thing)
{
	int				secnum;
	sector_t		*sec;
	vldoor_t		*door;
	int				side;
	
	side = 0;			/* only front sides can be used */
/* */
/*	Check for locks */
/* */
	if (line->sidenum[side ^ 1] < 0)
	{
		I_Error("EV_VerticalDoor: DR special type on 1-sided linedef");
		return;
	}

	/* if the sector has an active thinker, use it */
	secnum = sides[ line->sidenum[side^1]] .sector;
	sec = &sectors[secnum];
	if (sec->specialdata)
	{
		door = sec->specialdata;
		switch(line->special)
		{
			case	1:		/* ONLY FOR "RAISE" DOORS, NOT "OPEN"s */
			case	26:		/* BLUE CARD */
			case	27:		/* YELLOW CARD */
			case	28:		/* RED CARD */
			case	117:
				if (door->direction == -1)
					door->direction = 1;	/* go back up */
				else
				{
					if (!thing->player)
						return;				/* JDC: bad guys never close doors */
					door->direction = -1;	/* start going down immediately */
				}
				return;
		}
	}
	
	/* */
	/* new door thinker */
	/* */
	door = Z_Malloc (sizeof(*door), PU_LEVSPEC);
	P_AddThinker (&door->thinker);
	sec->specialdata = door;
	door->thinker.function = T_VerticalDoor;
	door->sector = sec;
	door->direction = 1;
	door->speed = VDOORSPEED;
	door->topwait = VDOORWAIT;
	switch(line->special)
	{
		case 1:
		case 26:
		case 27:
		case 28:
			door->type = normal;
			break;
		case 31:
		case 32:
		case 33:
		case 34:
			door->type = open;
			line->special = 0;
			break;
		case 117:
			door->type = blazeRaise;
			door->speed = VDOORSPEED * 4;
			break;
		case 118:
			door->type = blazeOpen;
			line->special = 0;
			door->speed = VDOORSPEED * 4;
			break;
	}
	
	/* */
	/* find the top and bottom of the movement range */
	/* */
	door->topheight = P_FindLowestCeilingSurrounding(sec);
	door->topheight -= 4*FRACUNIT;
}

/*================================================================== */
/* */
/*	Spawn a door that closes after 30 seconds */
/* */
/*================================================================== */
void P_SpawnDoorCloseIn30 (sector_t *sec)
{
	vldoor_t	*door;
	
	door = Z_Malloc ( sizeof(*door), PU_LEVSPEC);
	P_AddThinker (&door->thinker);
	sec->specialdata = door;
	sec->special = 0;
	door->thinker.function = T_VerticalDoor;
	door->sector = sec;
	door->direction = 0;
	door->type = normal;
	door->speed = VDOORSPEED;
	door->topcountdown = 30 * 15 * THINKERS_TICS;
}

/*================================================================== */
/* */
/*	Spawn a door that opens after 5 minutes */
/* */
/*================================================================== */
void P_SpawnDoorRaiseIn5Mins (sector_t *sec, int secnum)
{
	vldoor_t	*door;
	
	door = Z_Malloc ( sizeof(*door), PU_LEVSPEC);
	P_AddThinker (&door->thinker);
	sec->specialdata = door;
	sec->special = 0;
	door->thinker.function = T_VerticalDoor;
	door->sector = sec;
	door->direction = 2;
	door->type = raiseIn5Mins;
	door->speed = VDOORSPEED;
	door->topheight = P_FindLowestCeilingSurrounding(sec);
	door->topheight -= 4*FRACUNIT;
	door->topwait = VDOORWAIT;
	door->topcountdown = 5 * 60 * 15 * THINKERS_TICS;
}

