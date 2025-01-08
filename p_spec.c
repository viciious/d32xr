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
	{false,	"NUKAGE3",	"NUKAGE1"},
	{false,	"FWATER4",	"FWATER1"},
	{false,	"LAVA4",	"LAVA1"},
	{false,	"BLOOD3",	"BLOOD1"},

	// DOOM II flat animations.
	{false,	"RROCK08",	"RROCK05"},
	{false,	"SLIME04",	"SLIME01"},
	{false,	"SLIME08",	"SLIME05"},
	{false,	"SLIME12",	"SLIME09"},

	{true,	"BLODGR4",	"BLODGR1"},
	{true,	"SLADRIP3",	"SLADRIP1"},

	{true,	"BLODRIP4",	"BLODRIP1"},
	{true,	"FIREWALL",	"FIREWALA"},
	{true,	"GSTFONT3",	"GSTFONT1"},
	{true,	"FIRELAVA",	"FIRELAV3"},
	{true,	"FIREMAG3",	"FIREMAG1"},
	{true,	"FIREBLU2",	"FIREBLU1"},
	{true,	"ROCKRED3",	"ROCKRED1"},

	{true,	"BFALL4",	"BFALL1"},
	{true,	"SFALL4",	"SFALL1"},
	{true,	"WFALL4",	"WFALL1"},
	{true,	"DBRAIN4",	"DBRAIN1"},

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
int	P_FindSectorFromLineTag(line_t	*line,int start)
{
	return P_FindSectorFromLineTagNum(P_GetLineTag(line), start);
}

/*================================================================== */
/* */
/*	RETURN NEXT SECTOR # THAT LINE TAG REFERS TO */
/* */
/*================================================================== */
int	P_FindSectorFromLineTagNum(int tag,int start)
{
	int	i;

	for (i=start+1;i<numsectors;i++)
		if (sectors[i].tag == tag)
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
			case 125:
			case 126:
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
			EV_BuildStairs(line, build8);
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
		case 108:
			/* Blazing Door Raise(faster than TURBO!) */
			EV_DoDoor(line, blazeRaise);
			line->special = 0;
			break;
		case 109:
			/* Blazing Door Open(faster than TURBO!) */
			EV_DoDoor(line, blazeOpen);
			line->special = 0;
			break;
		case 100:
			/* Build Stairs Turbo 16 */
			EV_BuildStairs(line, turbo16);
			line->special = 0;
			break;
		case 110:
			/* Blazing Door Close(faster than TURBO!) */
			EV_DoDoor(line, blazeClose);
			line->special = 0;
			break;
		case 119:
			/* Raise floor to nearest surr.floor */
			EV_DoFloor(line, raiseFloorToNearest);
			line->special = 0;
			break;
		case 121:
			/* Blazing PlatDownWaitUpStay */
			EV_DoPlat(line, blazeDWUS, 0);
			line->special = 0;
			break;
		case 124:
			// Secret EXIT
			G_SecretExitLevel();
			break;

		case 125:
			// TELEPORT MonsterONLY
			if (!thing->player)
			{
				EV_Teleport(line, thing);
				line->special = 0;
			}
			break;

		case 130:
			/* Raise Floor Turbo */
			EV_DoFloor(line, raiseFloorTurbo);
			line->special = 0;
			break;

		case 141:
			/* Silent Ceiling Crush & Raise */
			EV_DoCeiling(line, silentCrushAndRaise);
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

		case 105:
			/* Blazing Door Raise(faster than TURBO!) */
			EV_DoDoor(line, blazeRaise);
			break;

		case 106:
			/* Blazing Door Open (faster than TURBO!) */
			EV_DoDoor(line, blazeOpen);
			break;

		case 107:
			/* Blazing Door Close (faster than TURBO!) */
			EV_DoDoor(line, blazeClose);
			break;

		case 120:
			/* Blazing PlatDownWaitUpStay. */
			EV_DoPlat(line, blazeDWUS, 0);
			break;

		case 126:
			/* TELEPORT MonsterONLY. */
			if (!thing->player)
				EV_Teleport(line, thing);
			break;

		case 128:
			/* Raise To Nearest Floor */
			EV_DoFloor(line, raiseFloorToNearest);
			break;

		case 129:
			/* Raise Floor Turbo */
			EV_DoFloor(line, raiseFloorTurbo);
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
			S_StartSound(player->mo, sfx_secret);
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
	int 		tag;

	secnum = -1;
	rtn = 0;
	tag = P_GetLineTag(line);
	while ((secnum = P_FindSectorFromLineTagNum(tag,secnum)) >= 0)
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

VINT	numlinespecials = 0;
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
			case 17:
				P_SpawnFireFlicker(sector);
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
		}
	}
done_speciallist:

	/* */
	/*	Init other misc stuff */
	/* */
	D_memset(activeceilings, 0, sizeof(*activeceilings)*MAXCEILINGS);
	D_memset(activeplats, 0, sizeof(*activeplats)*MAXCEILINGS);
	for (i = 0;i < MAXBUTTONS;i++)
		D_memset(&buttonlist[i],0,sizeof(button_t));
}
