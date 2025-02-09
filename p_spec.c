/* P_Spec.c */
#include "doomdef.h"
#include "p_local.h"

/*
===================
=
= P_InitPicAnims
=
===================
*/

static const animdef_t	animdefs[] =
{
	{false,	"BWATER08",	"BWATER01"},
	{false, "DWATER08", "DWATER01"},
	{false,	"FWATER4",	"FWATER1"},

	{true,	"GFALL4",	"GFALL1"},

	{-1}
};

anim_t	*anims/*[MAXANIMS]*/, * lastanim;


void P_InitPicAnims (void)
{
	int		i;
	
/* */
/*	Init animation */
/* */
	lastanim = anims;
	for (i=0 ; animdefs[i].istexture != -1 ; i++)
	{
		if (animdefs[i].istexture)
		{
			if (R_CheckTextureNumForName(animdefs[i].startname) == -1)
				continue;
			lastanim->picnum = R_TextureNumForName (animdefs[i].endname);
			lastanim->basepic = R_TextureNumForName (animdefs[i].startname);
		}
		else
		{
			if (W_CheckNumForName(animdefs[i].startname) == -1)
				continue;
			lastanim->picnum = R_FlatNumForName (animdefs[i].endname);
			lastanim->basepic = R_FlatNumForName (animdefs[i].startname);
		}
		lastanim->current = 0;
		lastanim->istexture = animdefs[i].istexture;
		lastanim->numpics = lastanim->picnum - lastanim->basepic + 1;
#if 0
/* FIXME */
		if (lastanim->numpics < 2)
			I_Error ("P_InitPicAnims: bad cycle from %s to %s"
			, animdefs[i].startname, animdefs[i].endname);
#endif
		lastanim++;
	}
	
}


/*
==============================================================================

							UTILITIES

==============================================================================
*/

/* */
/*	Will return a side_t* given the number of the current sector, */
/*		the line number, and the side (0/1) that you want. */
/* */
side_t *getSide(int currentSector,int line, int side)
{
	line_t *check = lines + sectors[currentSector].lines[line];
	return &sides[ check->sidenum[side] ];
}

/* */
/*	Will return a sector_t* given the number of the current sector, */
/*		the line number and the side (0/1) that you want. */
/* */
sector_t *getSector(int currentSector,int line,int side)
{
	line_t *check = lines + sectors[currentSector].lines[line];
	return &sectors[sides[ check->sidenum[side] ].sector];
}

/* */
/*	Given the sector number and the line number, will tell you whether */
/*		the line is two-sided or not. */
/* */
int	twoSided(int sector,int line)
{
	line_t *check = lines + sectors[sector].lines[line];
	return check->sidenum[1] != -1;
}

/*================================================================== */
/* */
/*	Return sector_t * of sector next to current. NULL if not two-sided line */
/* */
/*================================================================== */
sector_t *getNextSector(line_t *line,sector_t *sec)
{
	sector_t *front;

	if (!(line->sidenum[1] != -1))
		return NULL;
	
	front = LD_FRONTSECTOR(line);
	if (front == sec)
		return LD_BACKSECTOR(line);

	return front;
}

/*================================================================== */
/* */
/*	FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS */
/* */
/*================================================================== */
fixed_t	P_FindLowestFloorSurrounding(sector_t *sec)
{
	int			i;
	line_t		*check;
	sector_t	*other;
	fixed_t		floor = sec->floorheight;
	
	for (i=0 ;i < sec->linecount ; i++)
	{
		check = lines + sec->lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->floorheight < floor)
			floor = other->floorheight;
	}
	return floor;
}

/*================================================================== */
/* */
/*	FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS */
/* */
/*================================================================== */
fixed_t	P_FindHighestFloorSurrounding(sector_t *sec)
{
	int			i;
	line_t		*check;
	sector_t	*other;
	fixed_t		floor = -32000*FRACUNIT;
	
	for (i=0 ;i < sec->linecount ; i++)
	{
		check = lines + sec->lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;			
		if (other->floorheight > floor)
			floor = other->floorheight;
	}
	return floor;
}

/*================================================================== */
/* */
/*	FIND NEXT HIGHEST FLOOR IN SURROUNDING SECTORS */
/* */
/*================================================================== */
fixed_t	P_FindNextHighestFloor(sector_t *sec,int currentheight)
{
	int			i;
	int			h;
	int			min;
	line_t		*check;
	sector_t	*other;
	fixed_t		height = currentheight;
	fixed_t		heightlist[20];		/* 20 adjoining sectors max! */
	
	heightlist[0] = 0;
	for (i =0,h = 0 ;i < sec->linecount ; i++)
	{
		check = lines + sec->lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->floorheight > height)
			heightlist[h++] = other->floorheight;
		if (h == sizeof(heightlist) / sizeof(heightlist[0]))
			break;
	}
	
	if (h == 0)
		return currentheight;

	/* */
	/* Find lowest height in list */
	/* */
	min = heightlist[0];
	for (i = 1;i < h;i++)
		if (heightlist[i] < min)
			min = heightlist[i];
			
	return min;
}

fixed_t	P_FindNextLowestFloor(sector_t *sec,int currentheight)
{
	int			i;
	int			h;
	int			min;
	line_t		*check;
	sector_t	*other;
	fixed_t		height = currentheight;
	fixed_t		heightlist[20];		/* 20 adjoining sectors max! */
	
	heightlist[0] = 0;
	for (i =0,h = 0 ;i < sec->linecount ; i++)
	{
		check = lines + sec->lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->floorheight < height)
			heightlist[h++] = other->floorheight;
		if (h == sizeof(heightlist) / sizeof(heightlist[0]))
			break;
	}
	
	if (h == 0)
		return currentheight;

	/* */
	/* Find lowest height in list */
	/* */
	min = heightlist[0];
	for (i = 1;i < h;i++)
		if (heightlist[i] > min)
			min = heightlist[i];
			
	return min;
}

/*================================================================== */
/* */
/*	FIND LOWEST CEILING IN THE SURROUNDING SECTORS */
/* */
/*================================================================== */
fixed_t	P_FindLowestCeilingSurrounding(sector_t *sec)
{
	int			i;
	line_t		*check;
	sector_t	*other;
	fixed_t		height = 32000*FRACUNIT;
	
	for (i=0 ;i < sec->linecount ; i++)
	{
		check = lines + sec->lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->ceilingheight < height)
			height = other->ceilingheight;
	}
	return height;
}

/*================================================================== */
/* */
/*	FIND HIGHEST CEILING IN THE SURROUNDING SECTORS */
/* */
/*================================================================== */
fixed_t	P_FindHighestCeilingSurrounding(sector_t *sec)
{
	int	i;
	line_t	*check;
	sector_t	*other;
	fixed_t	height = -32000*FRACUNIT;
	
	for (i=0 ;i < sec->linecount ; i++)
	{
		check = lines + sec->lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->ceilingheight > height)
			height = other->ceilingheight;
	}
	return height;
}

/*================================================================== */
/* */
/*	RETURN NEXT SECTOR # THAT LINE TAG REFERS TO */
/* */
/*================================================================== */

VINT P_FindSectorWithTag(VINT tag, int start)
{
	int	i;
	
	for (i=start+1;i<numsectors;i++)
		if (sectors[i].tag == tag)
			return i;
	return -1;
}

int	P_FindSectorFromLineTag(line_t	*line, int start)
{
	return P_FindSectorWithTag(line->tag, start);
}

/*================================================================== */
/* */
/*	Find minimum light from an adjacent sector */
/* */
/*================================================================== */
int	P_FindMinSurroundingLight(sector_t *sector,int max)
{
	int			i;
	int			min;
	line_t		*line;
	sector_t	*check;
	
	min = max;
	for (i=0 ; i < sector->linecount ; i++)
	{
		line = lines + sector->lines[i];
		check = getNextSector(line,sector);
		if (!check)
			continue;
		if (check->lightlevel < min)
			min = check->lightlevel;
	}
	return min;
}

/*
==============================================================================

							EVENTS

Events are operations triggered by using, crossing, or shooting special lines, or by timed thinkers

==============================================================================
*/

typedef enum
{
	CF_RETURN   = 1,    // Return after crumbling
	CF_FLOATBOB = 2,    // Float on water
	CF_REVERSE  = 4,    // Reverse gravity
} crumbleflag_t;

typedef struct
{
	thinker_t thinker;
	line_t *sourceline;
	sector_t *sector;
	sector_t *actionsector; // The sector the rover action is taking place in.
	player_t *player; // Player who initiated the thinker (used for airbob)
	int16_t direction;
	int16_t timer;
	fixed_t speed;
	fixed_t floorwasheight; // Height the floor WAS at
	fixed_t ceilingwasheight; // Height the ceiling WAS at
	uint8_t flags;
} crumble_t;

/*
===============================================================================
=
= P_PlayerInSpecialSector
=
= Called every tic frame that the player origin is in a special sector
=
===============================================================================
*/

void P_PlayerInSpecialSector (player_t *player)
{
	sector_t	*sector;
	
	sector = subsectors[player->mo->isubsector].sector;
		
	switch (sector->special)
	{
		case 255: // ignore
			break;
		case 1: // Clear the map
			P_DoPlayerExit(player);
			break;
		case 2:
			if (player->mo->z <= sector->floorheight && !sector->specialdata)
				P_DoPlayerExit(player);
			break;
		case 9:		/* SECRET SECTOR */
			player->secretcount++;
			sector->special = 255;
			break;
			
		default:
			break;
	};
}


/*
===============================================================================
=
= P_UpdateSpecials
=
= Animate planes, scroll walls, etc
===============================================================================
*/

void P_UpdateSpecials (void)
{
	anim_t	*anim;
	int		i;
	line_t	*line;
	
	/* */
	/*	ANIMATE FLATS AND TEXTURES GLOBALY */
	/* */
	if (! (gametic&1) )
	{
		for (anim = anims ; anim < lastanim ; anim++)
		{
			int pic;

			anim->current++;
			if (anim->current < 0)
				anim->current = 0;
			if (anim->current >= anim->numpics)
				anim->current = 0;

			pic = anim->basepic + anim->current;
			for (i = 0; i < anim->numpics; i++)
			{
				if (anim->istexture)
					texturetranslation[anim->basepic+i] = pic;
				else
					flattranslation[anim->basepic+i] = pic;

				pic++;
				if (pic > anim->picnum)
					pic -= anim->numpics;
			}
		}
	}
	
	/* */
	/*	ANIMATE LINE SPECIALS */
	/* */
	for (i = 0; i < numlinespecials; i++)
	{
		side_t *side;
		int16_t textureoffset, rowoffset;
		line = linespeciallist[i];
		side = &sides[line->sidenum[0]];
		switch(line->special)
		{
			case 48:	/* EFFECT FIRSTCOL SCROLL + */
				// 12-bit texture offset + 4-bit rowoffset
				textureoffset = side->textureoffset;
				rowoffset = textureoffset & 0xf000;
				textureoffset <<= 4;
				textureoffset += 1<<4;
				textureoffset >>= 4;
				textureoffset |= rowoffset;
				side->textureoffset = textureoffset;
				break;

			case 142:	/* MODERATE VERT SCROLL */
				// 12-bit texture offset + 4-bit rowoffset
				textureoffset = side->textureoffset;
				rowoffset = ((textureoffset & 0xf000)>>4) | side->rowoffset;
				rowoffset -= 3;
				side->rowoffset = rowoffset & 0xff;
				side->textureoffset = (textureoffset & 0xfff) | (rowoffset & 0xf00);
				break;
		}
	}	
}

/*============================================================ */
/* */
/*	Special Stuff that can't be categorized */
/* */
/*============================================================ */
int EV_DoDonut(line_t *line)
{
	sector_t	*s1;
	sector_t	*s2;
	sector_t	*s3;
	int			secnum;
	int			rtn;
	int			i;
	floormove_t		*floor;
	
	secnum = -1;
	rtn = 0;
	while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
	{
		line_t *line;
		s1 = &sectors[secnum];
		
		/*	ALREADY MOVING?  IF SO, KEEP GOING... */
		if (s1->specialdata)
			continue;
			
		rtn = 1;
		line = lines + s1->lines[0];
		s2 = getNextSector(line,s1);
		if (!s2)
			continue;
		for (i = 0;i < s2->linecount;i++)
		{
			line = lines + s2->lines[i];
			if (!(line->sidenum[1] != -1))
				continue;
			s3 = &sectors[sides[line->sidenum[1]].sector];
			if (s3 == s1)
				continue;

			/* */
			/*	Spawn rising slime */
			/* */
			floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC);
			P_AddThinker (&floor->thinker);
			s2->specialdata = floor;
			floor->thinker.function = T_MoveFloor;
			floor->type = donutRaise;
			floor->crush = false;
			floor->direction = 1;
			floor->sector = s2;
			floor->speed = FLOORSPEED / 2;
			floor->texture = s3->floorpic;
			floor->newspecial = 0;
			floor->floordestheight = s3->floorheight;
			
			/* */
			/*	Spawn lowering donut-hole */
			/* */
			floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC);
			P_AddThinker (&floor->thinker);
			s1->specialdata = floor;
			floor->thinker.function = T_MoveFloor;
			floor->type = lowerFloor;
			floor->crush = false;
			floor->direction = -1;
			floor->sector = s1;
			floor->speed = FLOORSPEED / 2;
			floor->floordestheight = s3->floorheight;
			break;
		}
	}
	return rtn;
}

/*
==============================================================================

							SPECIAL SPAWNING

==============================================================================
*/
/*
================================================================================
= P_SpawnSpecials
=
= After the map has been loaded, scan for specials that
= spawn thinkers
=
===============================================================================
*/

VINT		numlinespecials = 0;
line_t	**linespeciallist = NULL;

void P_SpawnSpecials (void)
{
	sector_t	*sector;
	int		i;

	/* */
	/*	Init special SECTORs */
	/* */
	sector = sectors;
	for (i=0 ; i<numsectors ; i++, sector++)
	{
		if (!sector->special)
			continue;
		switch (sector->special)
		{
			default:
				break;
		}
	}
		
	
	/* */
	/*	Init line EFFECTs */
	/* */
	numlinespecials = 0;
	for (i = 0; i < numlines; i++)
	{
		switch (lines[i].special)
		{
		case 48:	/* EFFECT FIRSTCOL SCROLL+ */
		case 142:	/* MODERATE VERT SCROLL */
			linespeciallist[numlinespecials] = &lines[i];
			numlinespecials++;
			if (numlinespecials == MAXLINEANIMS)
				goto done_speciallist;
			break;
		case 60: // Moving platform
			EV_DoFloor(&lines[i], floorContinuous);
			break;
		case 120: // Water, but kind of boom-style
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
				sectors[s].heightsec = sec;
			break;
		}
		case 100: // 'FOF' sector
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				sectors[s].fofsec = sec;

			// A sector that has FOF collision, but for rendering it will swap the floor/ceiling
			// heights depending on the camera height.
			// Should that be the halfheight of the control sector?
			// Or maybe even configurable somehow, by using the control sector's texture offset value...
				if (lines[i].flags & ML_BLOCKMONSTERS)
					sectors[s].flags |= SF_FOF_SWAPHEIGHTS;
			}
			break;
		}
		case 105: // FOF that is invisible but solid
		{
			VINT sec = sides[*lines[i].sidenum].sector;
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				sectors[s].fofsec = sec;
				sectors[s].flags |= SF_FOF_INVISIBLE_TANGIBLE;
			}
			break;
		}
		case 178: // Crumbling, respawn, floating
		{
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				sectors[s].flags |= SF_CRUMBLE;
				sectors[s].flags |= SF_FLOATBOB;
				sectors[s].flags |= SF_RESPAWN;
			}

			break;
		}
		case 179: // Crumbling, no-respawn, floating
		{
			for (int s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
			{
				sectors[s].flags |= SF_CRUMBLE;
				sectors[s].flags |= SF_FLOATBOB;
			}

			break;
		}
		}
	}
done_speciallist:

	/* */
	/*	Init other misc stuff */
	/* */
	D_memset(activeceilings, 0, sizeof(*activeceilings)*MAXCEILINGS);
	D_memset(activeplats, 0, sizeof(*activeplats)*MAXCEILINGS);
}
