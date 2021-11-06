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
	{false,	"NUKAGE3",	"NUKAGE1",	4},
	{false,	"FWATER4",	"FWATER1",	4},
	{false,	"LAVA4",	"LAVA1",	4},
	{-1}
};

anim_t	anims[MAXANIMS], *lastanim;


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
		lastanim->current = lastanim->basepic;
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
	return &sides[ (sectors[currentSector].lines[line])->sidenum[side] ];
}

/* */
/*	Will return a sector_t* given the number of the current sector, */
/*		the line number and the side (0/1) that you want. */
/* */
sector_t *getSector(int currentSector,int line,int side)
{
	return &sectors[sides[ (sectors[currentSector].lines[line])->sidenum[side] ].sector];
}

/* */
/*	Given the sector number and the line number, will tell you whether */
/*		the line is two-sided or not. */
/* */
int	twoSided(int sector,int line)
{
	return (sectors[sector].lines[line])->flags & ML_TWOSIDED;
}

/*================================================================== */
/* */
/*	Return sector_t * of sector next to current. NULL if not two-sided line */
/* */
/*================================================================== */
sector_t *getNextSector(line_t *line,sector_t *sec)
{
	sector_t *front;

	if (!(line->flags & ML_TWOSIDED))
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
		check = sec->lines[i];
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
	fixed_t		floor = -500*FRACUNIT;
	
	for (i=0 ;i < sec->linecount ; i++)
	{
		check = sec->lines[i];
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
		check = sec->lines[i];
		other = getNextSector(check,sec);
		if (!other)
			continue;
		if (other->floorheight > height)
			heightlist[h++] = other->floorheight;
	}
	
	/* */
	/* Find lowest height in list */
	/* */
	min = heightlist[0];
	for (i = 1;i < h;i++)
		if (heightlist[i] < min)
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
	fixed_t		height = D_MAXINT;
	
	for (i=0 ;i < sec->linecount ; i++)
	{
		check = sec->lines[i];
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
	fixed_t	height = 0;
	
	for (i=0 ;i < sec->linecount ; i++)
	{
		check = sec->lines[i];
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
int	P_FindSectorFromLineTag(line_t	*line,int start)
{
	int	i;
	
	for (i=start+1;i<numsectors;i++)
		if (sectors[i].tag == line->tag)
			return i;
	return -1;
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
		line = sector->lines[i];
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



/*
===============================================================================
=
= P_CrossSpecialLine - TRIGGER
=
= Called every time a thing origin is about to cross
= a line with a non 0 special
=
===============================================================================
*/

void P_CrossSpecialLine (line_t *line,mobj_t *thing)
{
	int			ok;

	/* */
	/*	Triggers that other things can activate */
	/* */
	if (!thing->player)
	{
		ok = 0;
		switch(line->special)
		{
			case 39:	/* TELEPORT TRIGGER */
			case 97:	/* TELEPORT RETRIGGER */
			case 4:		/* RAISE DOOR */
			case 10:	/* PLAT DOWN-WAIT-UP-STAY TRIGGER */
			case 88:	/* PLAT DOWN-WAIT-UP-STAY RETRIGGER */
				ok = 1;
				break;
		}
		if (!ok)
			return;
	}
	
	switch (line->special)
	{
		/*==================================================== */
		/* TRIGGERS */
		/*==================================================== */
		case 2:			/* Open Door */
			EV_DoDoor(line,open);
			line->special = 0;
			break;
		case 3:			/* Close Door */
			EV_DoDoor(line,close);
			line->special = 0;
			break;
		case 4:			/* Raise Door */
			EV_DoDoor(line,normal);
			line->special = 0;
			break;
		case 5:			/* Raise Floor */
			EV_DoFloor(line,raiseFloor);
			line->special = 0;
			break;
		case 6:			/* Fast Ceiling Crush & Raise */
			EV_DoCeiling(line,fastCrushAndRaise);
			line->special = 0;
			break;
		case 8:			/* Build Stairs */
			EV_BuildStairs(line);
			line->special = 0;
			break;
		case 10:		/* PlatDownWaitUp */
			EV_DoPlat(line,downWaitUpStay,0);
			line->special = 0;
			break;
		case 12:		/* Light Turn On - brightest near */
			EV_LightTurnOn(line,0);
			line->special = 0;
			break;
		case 13:		/* Light Turn On 255 */
			EV_LightTurnOn(line,255);
			line->special = 0;
			break;
		case 16:		/* Close Door 30 */
			EV_DoDoor(line,close30ThenOpen);
			line->special = 0;
			break;
		case 17:		/* Start Light Strobing */
			EV_StartLightStrobing(line);
			line->special = 0;
			break;
		case 19:		/* Lower Floor */
			EV_DoFloor(line,lowerFloor);
			line->special = 0;
			break;
		case 22:		/* Raise floor to nearest height and change texture */
			EV_DoPlat(line,raiseToNearestAndChange,0);
			line->special = 0;
			break;
		case 25:		/* Ceiling Crush and Raise */
			EV_DoCeiling(line,crushAndRaise);
			line->special = 0;
			break;
		case 30:		/* Raise floor to shortest texture height */
						/* on either side of lines */
			EV_DoFloor(line,raiseToTexture);
			line->special = 0;
			break;
		case 35:		/* Lights Very Dark */
			EV_LightTurnOn(line,35);
			line->special = 0;
			break;
		case 36:		/* Lower Floor (TURBO) */
			EV_DoFloor(line,turboLower);
			line->special = 0;
			break;
		case 37:		/* LowerAndChange */
			EV_DoFloor(line,lowerAndChange);
			line->special = 0;
			break;
		case 38:		/* Lower Floor To Lowest */
			EV_DoFloor( line, lowerFloorToLowest );
			line->special = 0;
			break;
		case 39:		/* TELEPORT! */
			EV_Teleport( line, thing );
			line->special = 0;
			break;
		case 40:		/* RaiseCeilingLowerFloor */
			EV_DoCeiling( line, raiseToHighest );
			EV_DoFloor( line, lowerFloorToLowest );
			line->special = 0;
			break;
		case 44:		/* Ceiling Crush */
			EV_DoCeiling( line, lowerAndCrush );
			line->special = 0;
			break;
		case 52:		/* EXIT! */
			G_ExitLevel ();
			line->special = 0;
			break;
		case 53:		/* Perpetual Platform Raise */
			EV_DoPlat(line,perpetualRaise,0);
			line->special = 0;
			break;
		case 54:		/* Platform Stop */
			EV_StopPlat(line);
			line->special = 0;
			break;
		case 56:		/* Raise Floor Crush */
			EV_DoFloor(line,raiseFloorCrush);
			line->special = 0;
			break;
		case 57:		/* Ceiling Crush Stop */
			EV_CeilingCrushStop(line);
			line->special = 0;
			break;
		case 58:		/* Raise Floor 24 */
			EV_DoFloor(line,raiseFloor24);
			line->special = 0;
			break;
		case 59:		/* Raise Floor 24 And Change */
			EV_DoFloor(line,raiseFloor24AndChange);
			line->special = 0;
			break;
		case 104:		/* Turn lights off in sector(tag) */
			EV_TurnTagLightsOff(line);
			line->special = 0;
			break;
	/*==================================================== */
	/* RE-DOABLE TRIGGERS */
	/*==================================================== */
		case 72:		/* Ceiling Crush */
			EV_DoCeiling( line, lowerAndCrush );
			break;
		case 73:		/* Ceiling Crush and Raise */
			EV_DoCeiling(line,crushAndRaise);
			break;
		case 74:		/* Ceiling Crush Stop */
			EV_CeilingCrushStop(line);
			break;
		case 75:			/* Close Door */
			EV_DoDoor(line,close);
			break;
		case 76:		/* Close Door 30 */
			EV_DoDoor(line,close30ThenOpen);
			break;
		case 77:			/* Fast Ceiling Crush & Raise */
			EV_DoCeiling(line,fastCrushAndRaise);
			break;
		case 79:		/* Lights Very Dark */
			EV_LightTurnOn(line,35);
			break;
		case 80:		/* Light Turn On - brightest near */
			EV_LightTurnOn(line,0);
			break;
		case 81:		/* Light Turn On 255 */
			EV_LightTurnOn(line,255);
			break;
		case 82:		/* Lower Floor To Lowest */
			EV_DoFloor( line, lowerFloorToLowest );
			break;
		case 83:		/* Lower Floor */
			EV_DoFloor(line,lowerFloor);
			break;
		case 84:		/* LowerAndChange */
			EV_DoFloor(line,lowerAndChange);
			break;
		case 86:			/* Open Door */
			EV_DoDoor(line,open);
			break;
		case 87:		/* Perpetual Platform Raise */
			EV_DoPlat(line,perpetualRaise,0);
			break;
		case 88:		/* PlatDownWaitUp */
			EV_DoPlat(line,downWaitUpStay,0);
			break;
		case 89:		/* Platform Stop */
			EV_StopPlat(line);
			break;
		case 90:			/* Raise Door */
			EV_DoDoor(line,normal);
			break;
		case 91:			/* Raise Floor */
			EV_DoFloor(line,raiseFloor);
			break;
		case 92:		/* Raise Floor 24 */
			EV_DoFloor(line,raiseFloor24);
			break;
		case 93:		/* Raise Floor 24 And Change */
			EV_DoFloor(line,raiseFloor24AndChange);
			break;
		case 94:		/* Raise Floor Crush */
			EV_DoFloor(line,raiseFloorCrush);
			break;
		case 95:		/* Raise floor to nearest height and change texture */
			EV_DoPlat(line,raiseToNearestAndChange,0);
			break;
		case 96:		/* Raise floor to shortest texture height */
						/* on either side of lines */
			EV_DoFloor(line,raiseToTexture);
			break;
		case 97:		/* TELEPORT! */
			EV_Teleport( line, thing );
			break;
		case 98:		/* Lower Floor (TURBO) */
			EV_DoFloor(line,turboLower);
			break;
			
	}
}



/*
===============================================================================
=
= P_ShootSpecialLine - IMPACT SPECIALS
=
= Called when a thing shoots a special line
=
===============================================================================
*/

void	P_ShootSpecialLine ( mobj_t *thing, line_t *line)
{
	int		ok;
	
	/* */
	/*	Impacts that other things can activate */
	/* */
	if (!thing->player)
	{
		ok = 0;
		switch(line->special)
		{
			case 46:		/* OPEN DOOR IMPACT */
				ok = 1;
				break;
		}
		if (!ok)
			return;
	}

	switch(line->special)
	{
		case 24:		/* RAISE FLOOR */
			EV_DoFloor(line,raiseFloor);
			P_ChangeSwitchTexture(line,0);
			break;
		case 46:		/* OPEN DOOR */
			EV_DoDoor(line,open);
			P_ChangeSwitchTexture(line,1);
			break;
		case 47:		/* RAISE FLOOR NEAR AND CHANGE */
			EV_DoPlat(line,raiseToNearestAndChange,0);
			P_ChangeSwitchTexture(line,0);
			break;
	}
}


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
	
	sector = player->mo->subsector->sector;
	if (player->mo->z != sector->floorheight)
		return;		/* not all the way down yet */
		
	switch (sector->special)
	{
		case 5:		/* HELLSLIME DAMAGE */
			if (!player->powers[pw_ironfeet])
				if ((gametic != prevgametic) && !(gametic&0xf))
					P_DamageMobj (player->mo, NULL, NULL, 10);
			break;
		case 7:		/* NUKAGE DAMAGE */
			if (!player->powers[pw_ironfeet])
				if ((gametic != prevgametic) && !(gametic&0xf))
					P_DamageMobj (player->mo, NULL, NULL, 5);
			break;
		case 16:	/* SUPER HELLSLIME DAMAGE */
		case 4:		/* STROBE HURT */
			if (!player->powers[pw_ironfeet] || (P_Random()<5) )
				if ((gametic != prevgametic) && !(gametic&0xf))
					P_DamageMobj (player->mo, NULL, NULL, 20);
			break;
			
		case 9:		/* SECRET SECTOR */
			player->secretcount++;
			sector->special = 0;
			break;
			
		default:
			I_Error ("P_PlayerInSpecialSector: "
					"unknown special %i",sector->special);
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
	if (! (gametic&3) )
	{
		for (anim = anims ; anim < lastanim ; anim++)
		{
			anim->current++;
			if (anim->current > anim->picnum)
				anim->current = anim->basepic;
			flattranslation[anim->picnum] = anim->current;
		}
	}
	
	/* */
	/*	ANIMATE LINE SPECIALS */
	/* */
	for (i = 0; i < numlinespecials; i++)
	{
		line = linespeciallist[i];
		switch(line->special)
		{
			case 48:	/* EFFECT FIRSTCOL SCROLL + */
				sides[line->sidenum[0]].textureoffset += 1;
				break;
		}
	}
	
	/* */
	/*	DO BUTTONS */
	/* */
	for (i = 0; i < MAXBUTTONS; i++)
		if (buttonlist[i].btimer)
		{
			buttonlist[i].btimer--;
			if (!buttonlist[i].btimer)
			{
				switch(buttonlist[i].where)
				{
					case top:
						sides[buttonlist[i].line->sidenum[0]].toptexture =
							buttonlist[i].btexture;
						break;
					case middle:
						sides[buttonlist[i].line->sidenum[0]].midtexture =
							buttonlist[i].btexture;
						break;
					case bottom:
						sides[buttonlist[i].line->sidenum[0]].bottomtexture =
							buttonlist[i].btexture;
						break;
				}
				S_StartSound((mobj_t *)&buttonlist[i].soundorg,sfx_swtchn);
				D_memset(&buttonlist[i],0,sizeof(button_t));
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
		s1 = &sectors[secnum];
		
		/*	ALREADY MOVING?  IF SO, KEEP GOING... */
		if (s1->specialdata)
			continue;
			
		rtn = 1;
		s2 = getNextSector(s1->lines[0],s1);
		for (i = 0;i < s2->linecount;i++)
		{
			s3 = LD_BACKSECTOR(s2->lines[i]);
			if (!(s2->lines[i]->flags & ML_TWOSIDED) ||
				(s3 == s1))
				continue;

			/* */
			/*	Spawn rising slime */
			/* */
			floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
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
			floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
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

int		numlinespecials = 0;
line_t	*linespeciallist[MAXLINEANIMS];

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
			case 1:		/* FLICKERING LIGHTS */
				P_SpawnLightFlash (sector);
				break;
			case 2:		/* STROBE FAST */
				P_SpawnStrobeFlash(sector,FASTDARK,0);
				break;
			case 3:		/* STROBE SLOW */
				P_SpawnStrobeFlash(sector,SLOWDARK,0);
				break;
			case 8:		/* GLOWING LIGHT */
				P_SpawnGlowingLight(sector);
				break;
			case 9:		/* SECRET SECTOR */
				totalsecret++;
				break;
			case 10:	/* DOOR CLOSE IN 30 SECONDS */
				P_SpawnDoorCloseIn30 (sector);
				break;
			case 12:	/* SYNC STROBE SLOW */
				P_SpawnStrobeFlash (sector, SLOWDARK, 1);
				break;
			case 13:	/* SYNC STROBE FAST */
				P_SpawnStrobeFlash (sector, FASTDARK, 1);
				break;
			case 14:	/* DOOR RAISE IN 5 MINUTES */
				P_SpawnDoorRaiseIn5Mins (sector, i);
				break;
		}
	}
		
	
	/* */
	/*	Init line EFFECTs */
	/* */
	numlinespecials = 0;
	for (i = 0;i < numlines; i++)
		switch(lines[i].special)
		{
			case 48:	/* EFFECT FIRSTCOL SCROLL+ */
				linespeciallist[numlinespecials] = &lines[i];
				numlinespecials++;
				break;
		}
		
	/* */
	/*	Init other misc stuff */
	/* */
	for (i = 0;i < MAXCEILINGS;i++)
		activeceilings[i] = NULL;
	for (i = 0;i < MAXPLATS;i++)
		activeplats[i] = NULL;
	for (i = 0;i < MAXBUTTONS;i++)
		D_memset(&buttonlist[i],0,sizeof(button_t));
	
}
