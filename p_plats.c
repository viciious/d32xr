/*================================================================== */
/*================================================================== */
/* */
/*							PLATFORM RAISING */
/* */
/*================================================================== */
/*================================================================== */
#include "doomdef.h"
#include "p_local.h"

plat_t	**activeplats/*[MAXPLATS]*/ = NULL;

/*================================================================== */
/* */
/*	Move a plat up and down */
/* */
/*================================================================== */
void	T_PlatRaise(plat_t	*plat)
{
	result_e	res;
	
	switch(plat->status)
	{
		case	up:
			res = T_MovePlane(&plat->m,plat->speed,
					plat->high,plat->crush,0,1);
					
			if (plat->type == raiseAndChange ||
				plat->type == raiseToNearestAndChange)
				if (!(gametic&7))
					P_MoverSound(&plat->m,sfx_stnmov);
				
			if (res == crushed && (!plat->crush))
			{
				plat->count = plat->wait;
				plat->status = down;
				P_MoverSound(&plat->m,sfx_pstart);
			}
			else
			if (res == pastdest)
			{
				plat->count = plat->wait;
				plat->status = waiting;
				P_MoverSound(&plat->m,sfx_pstop);
				switch(plat->type)
				{
					case blazeDWUS:
					case downWaitUpStay:
						P_RemoveActivePlat(plat);
						break;
					case raiseAndChange:
					case raiseToNearestAndChange:
						P_RemoveActivePlat(plat);
						break;
					default:
						break;
				}
			}
			break;
		case	down:
			res = T_MovePlane(&plat->m,plat->speed,plat->low,false,0,-1);
			if (res == pastdest)
			{
				plat->count = plat->wait;
				plat->status = waiting;
				P_MoverSound(&plat->m,sfx_pstop);
			}
			break;
		case	waiting:
			if (!--plat->count)
			{
				if (plat->m.sector->floorheight == plat->low)
					plat->status = up;
				else
					plat->status = down;
				P_MoverSound(&plat->m,sfx_pstart);
			}
		case	in_stasis:
			break;
	}
}

/*================================================================== */
/* */
/*	Do Platforms */
/*	"amount" is only used for SOME platforms. */
/* */
/*================================================================== */
int	EV_DoPlat(line_t *line,plattype_e type,int amount)
{
	int 		k;
	plat_t		*plat;
	int			secnum;
	int			rtn;
	sector_t	*sec;
	int 		tag;
	
	tag = P_GetLineTag(line);
	rtn = 0;
	
	/* */
	/*	Activate all <type> plats that are in_stasis */
	/* */
	switch(type)
	{
		case perpetualRaise:
			P_ActivateInStasis(tag);
			break;
		default:
			break;
	}
	
	k = 0;
	while ((secnum = P_FindNextSectorByTagNum(tag,&k)) >= 0)
	{
		sec = &sectors[secnum];
		if (sec->specialdata)
			continue;
	
		/* */
		/* Find lowest & highest floors around sector */
		/* */
		rtn = 1;
		plat = Z_Malloc( sizeof(*plat), PU_LEVSPEC);
		P_AddThinker(&plat->thinker);
		
		plat->type = type;
		plat->m.sector = sec;
		sec->specialdata = LPTR_TO_SPTR(plat);
		P_SectorBBox(sec, plat->m.secbbox);
		plat->thinker.function = T_PlatRaise;
		plat->crush = false;
		plat->tag = tag;
		switch(type)
		{
			case raiseToNearestAndChange:
				plat->speed = PLATSPEED/2;
				sec->floorpic = LD_FRONTSECTOR(line)->floorpic;
				plat->high = P_FindNextHighestFloor(sec,sec->floorheight);
				plat->wait = 0;
				plat->status = up;
				sec->special = 0;		/* NO MORE DAMAGE, IF APPLICABLE */
				P_MoverSound(&plat->m,sfx_stnmov);
				break;
			case raiseAndChange:
				plat->speed = PLATSPEED/2;
				sec->floorpic = LD_FRONTSECTOR(line)->floorpic;
				plat->high = sec->floorheight + amount*FRACUNIT;
				plat->wait = 0;
				plat->status = up;
				P_MoverSound(&plat->m,sfx_stnmov);
				break;
			case downWaitUpStay:
			case blazeDWUS:
				plat->speed = PLATSPEED * 4 * (type == blazeDWUS ? 2 : 1);
				plat->low = P_FindLowestFloorSurrounding(sec);
				if (plat->low > sec->floorheight)
					plat->low = sec->floorheight;
				plat->high = sec->floorheight;
				plat->wait = 35*PLATWAIT;
				plat->status = down;
				P_MoverSound(&plat->m,sfx_pstart);
				break;
			case perpetualRaise:
				plat->speed = PLATSPEED;
				plat->low = P_FindLowestFloorSurrounding(sec);
				if (plat->low > sec->floorheight)
					plat->low = sec->floorheight;
				plat->high = P_FindHighestFloorSurrounding(sec);
				if (plat->high < sec->floorheight)
					plat->high = sec->floorheight;
				plat->wait = 35*PLATWAIT;
				plat->status = P_Random()&1;
				P_MoverSound(&plat->m,sfx_pstart);
				break;
		}
		P_AddActivePlat(plat);
	}
	return rtn;
}

void P_ActivateInStasis(int tag)
{
	int		i;
	
	for (i = 0;i < MAXPLATS;i++)
		if (activeplats[i] &&
			(activeplats[i])->tag == tag &&
			(activeplats[i])->status == in_stasis)
		{
			(activeplats[i])->status = (activeplats[i])->oldstatus;
			(activeplats[i])->thinker.function = T_PlatRaise;
		}
}

void EV_StopPlat(line_t *line)
{
	int		j;
	int 	tag;
	
	tag = P_GetLineTag(line);
	for (j = 0;j < MAXPLATS;j++)
		if (activeplats[j] && ((activeplats[j])->status != in_stasis) &&
			((activeplats[j])->tag == tag))
		{
			(activeplats[j])->oldstatus = (activeplats[j])->status;
			(activeplats[j])->status = in_stasis;
			(activeplats[j])->thinker.function = NULL;
		}
}

void P_AddActivePlat(plat_t *plat)
{
	int		i;
	for (i = 0;i < MAXPLATS;i++)
		if (activeplats[i] == NULL)
		{
			activeplats[i] = plat;
			return;
		}
	I_Error ("P_AddActivePlat: no more plats!");
}

void P_RemoveActivePlat(plat_t *plat)
{
	int		i;
	for (i = 0;i < MAXPLATS;i++)
		if (plat == activeplats[i])
		{
			(activeplats[i])->m.sector->specialdata = (SPTR)0;
			P_RemoveThinker(&(activeplats[i])->thinker);
			activeplats[i] = NULL;
			return;
		}
	I_Error ("P_RemoveActivePlat: can't find plat!");
}
