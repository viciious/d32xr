#include "doomdef.h"
#include "p_local.h"

/*================================================================== */
/*================================================================== */
/* */
/*							BROKEN LIGHT FLASHING */
/* */
/*================================================================== */
/*================================================================== */

/*================================================================== */
/* */
/*	T_LightFlash */
/* */
/*	After the map has been loaded, scan each sector for specials */
/*	that spawn thinkers */
/* */
/*================================================================== */
void T_LightFlash (lightflash_t *flash)
{
	if (gametic == prevgametic)
		return;
	if (--flash->count)
		return;
	
	if (flash->sector->lightlevel == flash->maxlight)
	{
		flash-> sector->lightlevel = flash->minlight;
		flash->count = (P_Random()&flash->mintime)+1;
	}
	else
	{
		flash-> sector->lightlevel = flash->maxlight;
		flash->count = (P_Random()&flash->maxtime)+1;
	}

}


/*================================================================== */
/* */
/*	P_SpawnLightFlash */
/* */
/*	After the map has been loaded, scan each sector for specials that spawn thinkers */
/* */
/*================================================================== */
void P_SpawnLightFlash (sector_t *sector)
{
	lightflash_t	*flash;
	
	sector->special = 0;		/* nothing special about it during gameplay */
	
	flash = Z_Malloc ( sizeof(*flash), PU_LEVSPEC);
	P_AddThinker (&flash->thinker);
	flash->thinker.function = T_LightFlash;
	flash->sector = sector;
	flash->maxlight = sector->lightlevel;

	flash->minlight = P_FindMinSurroundingLight(sector,sector->lightlevel);
	flash->maxtime = 64;
	flash->mintime = 7;
	flash->count = (P_Random()&flash->maxtime)+1;
}

/*================================================================== */
/* */
/*							STROBE LIGHT FLASHING */
/* */
/*================================================================== */

/*================================================================== */
/* */
/*	T_StrobeFlash */
/* */
/*	After the map has been loaded, scan each sector for specials that spawn thinkers */
/* */
/*================================================================== */
void T_StrobeFlash (strobe_t *flash)
{
	if (gametic == prevgametic)
		return;
	if (--flash->count)
		return;
	
	if (flash->sector->lightlevel == flash->minlight)
	{
		flash-> sector->lightlevel = flash->maxlight;
		flash->count = flash->brighttime;
	}
	else
	{
		flash-> sector->lightlevel = flash->minlight;
		flash->count =flash->darktime;
	}

}

/*================================================================== */
/* */
/*	P_SpawnLightFlash */
/* */
/*	After the map has been loaded, scan each sector for specials that spawn thinkers */
/* */
/*================================================================== */
void P_SpawnStrobeFlash (sector_t *sector,int fastOrSlow, int inSync)
{
	strobe_t	*flash;
	
	flash = Z_Malloc ( sizeof(*flash), PU_LEVSPEC);
	P_AddThinker (&flash->thinker);
	flash->sector = sector;
	flash->darktime = fastOrSlow;
	flash->brighttime = STROBEBRIGHT;
	flash->thinker.function = T_StrobeFlash;
	flash->maxlight = sector->lightlevel;
	flash->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);
		
	if (flash->minlight == flash->maxlight)
		flash->minlight = 0;
	sector->special = 0;		/* nothing special about it during gameplay */

	if (!inSync)
		flash->count = (P_Random()&7)+1;
	else
		flash->count = 1;
}

/*================================================================== */
/* */
/*	Start strobing lights (usually from a trigger) */
/* */
/*================================================================== */
void EV_StartLightStrobing(line_t *line)
{
	int	secnum;
	sector_t	*sec;
	int tag;
	
	tag = P_GetLineTag(line);
	secnum = -1;
	while ((secnum = P_FindSectorFromLineTagNum(tag,secnum)) >= 0)
	{
		sec = &sectors[secnum];
		if (sec->specialdata)
			continue;
	
		P_SpawnStrobeFlash (sec,SLOWDARK, 0);
	}
}

/*================================================================== */
/* */
/*	TURN LINE'S TAG LIGHTS OFF */
/* */
/*================================================================== */
void EV_TurnTagLightsOff(line_t	*line)
{
	int			i;
	int			j;
	int			min;
	sector_t	*sector;
	sector_t	*tsec;
	line_t		*templine;
	int 		tag;
	
	tag	= P_GetLineTag(line);
	sector = sectors;
	for (j = 0;j < numsectors; j++, sector++)
		if (sector->tag == tag)
		{
			min = sector->lightlevel;
			for (i = 0;i < sector->linecount; i++)
			{
				templine = lines + sector->lines[i];
				tsec = getNextSector(templine,sector);
				if (!tsec)
					continue;
				if (tsec->lightlevel < min)
					min = tsec->lightlevel;
			}
			sector->lightlevel = min;
		}
}

/*================================================================== */
/* */
/*	TURN LINE'S TAG LIGHTS ON */
/* */
/*================================================================== */
void EV_LightTurnOn(line_t *line, int bright)
{
	int			i;
	int			j;
	sector_t	*sector;
	sector_t	*temp;
	line_t		*templine;
	int tag;
	
	tag = P_GetLineTag(line);
	sector = sectors;
	
	for (i=0;i<numsectors;i++, sector++)
		if (sector->tag == tag)
		{
			/* */
			/* bright = 0 means to search for highest */
			/* light level surrounding sector */
			/* */
			if (!bright)
			{
				for (j = 0;j < sector->linecount; j++)
				{
					templine = lines + sector->lines[j];
					temp = getNextSector(templine,sector);
					if (!temp)
						continue;
					if (temp->lightlevel > bright)
						bright = temp->lightlevel;
				}
			}
			sector->lightlevel = bright;
		}
}

/*================================================================== */
/* */
/*	Spawn glowing light */
/* */
/*================================================================== */
void T_Glow(glow_t *g)
{
	if (gametic == prevgametic)
		return;

	int lightlevel = g->sector->lightlevel;
	switch(g->direction)
	{
		case -1:		/* DOWN */
			lightlevel -= GLOWSPEED;
			if (lightlevel <= g->minlight)
			{
				lightlevel += GLOWSPEED;
				g->direction = 1;
			}
			break;
		case 1:			/* UP */
			lightlevel += GLOWSPEED;
			if (lightlevel >= g->maxlight)
			{
				lightlevel -= GLOWSPEED;
				g->direction = -1;
			}
			break;
	}

	if (lightlevel < 0) lightlevel = 0;
	else if (lightlevel > 255) lightlevel = 255;
	g->sector->lightlevel = lightlevel;
}

void P_SpawnGlowingLight(sector_t *sector)
{
	glow_t	*g;
	
	g = Z_Malloc( sizeof(*g), PU_LEVSPEC);
	P_AddThinker(&g->thinker);
	g->sector = sector;
	g->minlight = P_FindMinSurroundingLight(sector,sector->lightlevel);
	g->maxlight = sector->lightlevel;
	g->thinker.function = T_Glow;
	g->direction = -1;

	sector->special = 0;
}

//
// FIRELIGHT FLICKER
//

//
// T_FireFlicker
//
void T_FireFlicker (fireflicker_t *flick)
{
	int	amount;

	if (--flick->count)
		return;

	amount = (P_Random()&3)*16;
	if (flick->sector->lightlevel - amount < flick->minlight)
		flick->sector->lightlevel = flick->minlight;
	else
		flick->sector->lightlevel = flick->maxlight - amount;
	flick->count = 4;
}

//
// P_SpawnFireFlicker
//
void P_SpawnFireFlicker (sector_t *sector)
{
	fireflicker_t	*flick;

	// Note that we are resetting sector attributes.
	// Nothing special about it during gameplay.
	sector->special = 0;

	flick = Z_Malloc ( sizeof(*flick), PU_LEVSPEC);
	P_AddThinker (&flick->thinker);
	flick->thinker.function = T_FireFlicker;
	flick->sector = sector;
	flick->maxlight = sector->lightlevel;
	flick->minlight = P_FindMinSurroundingLight(sector,sector->lightlevel)+16;
	flick->count = 4;
}
