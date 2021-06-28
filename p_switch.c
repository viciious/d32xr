#include "doomdef.h"
#include "p_local.h"

/*================================================================== */
/* */
/*	CHANGE THE TEXTURE OF A WALL SWITCH TO ITS OPPOSITE */
/* */
/*================================================================== */
static const switchlist_t alphSwitchList[] =
{
	{"SW1BRN1",		"SW2BRN1"},
	{"SW1GARG",		"SW2GARG"},
	{"SW1GSTON",	"SW2GSTON"},
	{"SW1HOT",		"SW2HOT"},
	{"SW1STAR",		"SW2STAR"},
	{"SW1WOOD",		"SW2WOOD"},
};

VINT		switchlist[MAXSWITCHES * 2];
int			numswitches;
button_t	buttonlist[MAXBUTTONS];

/*
===============
=
= P_InitSwitchList
=
= Only called at game initialization
=
===============
*/

void P_InitSwitchList(void)
{
	int		i;
	int		index;
		
	numswitches = sizeof(alphSwitchList)/sizeof(alphSwitchList[0]);
	
	for (index = 0,i = 0;i < numswitches;i++)
	{
		if (!alphSwitchList[i].name1[0])
			break;
		
		switchlist[index] = R_TextureNumForName(alphSwitchList[i].name1);
		index++;
		switchlist[index] = R_TextureNumForName(alphSwitchList[i].name2);
		index++;
	}
	
	switchlist[index] = -1;
	
}

/*================================================================== */
/* */
/*	Start a button counting down till it turns off. */
/* */
/*================================================================== */
void P_StartButton(line_t *line,bwhere_e w,int texture,int time)
{
	int		i;
	
	for (i = 0;i < MAXBUTTONS;i++)
		if (!buttonlist[i].btimer)
		{
			buttonlist[i].line = line;
			buttonlist[i].where = w;
			buttonlist[i].btexture = texture;
			buttonlist[i].btimer = time;
			buttonlist[i].soundorg = (mobj_t *)&(LD_FRONTSECTOR(line))->soundorg;
			return;
		}
		
	I_Error("P_StartButton: no button slots left!");
}

/*================================================================== */
/* */
/*	Function that changes wall texture. */
/*	Tell it if switch is ok to use again (1=yes, it's a button). */
/* */
/*================================================================== */
void P_ChangeSwitchTexture(line_t *line,int useAgain)
{
	int	texTop;
	int	texMid;
	int	texBot;
	int	i;
	int	sound;
	
	if (!useAgain)
		line->special = 0;

	texTop = sides[line->sidenum[0]].toptexture;
	texMid = sides[line->sidenum[0]].midtexture;
	texBot = sides[line->sidenum[0]].bottomtexture;
	
	sound = sfx_swtchn;
	if (line->special == 11)		/* EXIT SWITCH? */
		sound = sfx_swtchx;
	
	for (i = 0;i < numswitches*2;i++)
		if (switchlist[i] == texTop)
		{
			S_StartSound(buttonlist->soundorg,sound);
			sides[line->sidenum[0]].toptexture = switchlist[i^1];
			if (useAgain)
				P_StartButton(line,top,switchlist[i],BUTTONTIME);
			return;
		}
		else
		if (switchlist[i] == texMid)
		{
			S_StartSound(buttonlist->soundorg,sound);
			sides[line->sidenum[0]].midtexture = switchlist[i^1];
			if (useAgain)
				P_StartButton(line, middle,switchlist[i],BUTTONTIME);
			return;
		}
		else
		if (switchlist[i] == texBot)
		{
			S_StartSound(buttonlist->soundorg,sound);
			sides[line->sidenum[0]].bottomtexture = switchlist[i^1];
			if (useAgain)
				P_StartButton(line, bottom,switchlist[i],BUTTONTIME);
			return;
		}
}

/*
==============================================================================
=
= P_UseSpecialLine
=
= Called when a thing uses a special line
= Only the front sides of lines are usable
===============================================================================
*/

boolean P_UseSpecialLine ( mobj_t *thing, line_t *line)
{		
	/* */
	/*	Switches that other things can activate */
	/* */
	if (!thing->player)
	{
		if (line->flags & ML_SECRET)
			return false;		/* never open secret doors */
		switch(line->special)
		{
			case 1:		/* MANUAL DOOR RAISE */
/*			case 32:	// MANUAL BLUE */
/*			case 33:	// MANUAL RED */
/*			case 34:	// MANUAL YELLOW */
				break;
			default:
				return false;
		}
	}
	
	/* */
	/* do something */
	/*	 */
	switch (line->special)
	{
		/*=============================================== */
		/*	MANUALS */
		/*=============================================== */
		case 1:			/* Vertical Door */
		case 31:		/* Manual door open */
		case 26:		/* Blue Card Door Raise */
		case 32:		/* Blue Card door open */
		case 99:		/* Blue Skull Door Open */
		case 106:		/* Blue Skull Door Raise */
		case 27:		/* Yellow Card Door Raise */
		case 34:		/* Yellow Card door open */
		case 105:		/* Yellow Skull Door Open */
		case 108:		/* Yellow Skull Door Raise */
		case 28:		/* Red Card Door Raise */
		case 33:		/* Red Card door open */
		case 100:		/* Red Skull Door Open */
		case 107:		/* Red Skull Door Raise */
			EV_VerticalDoor (line, thing);
			break;
		/*=============================================== */
		/*	BUTTONS */
		/*=============================================== */
		case 42:		/* Close Door */
			if (EV_DoDoor(line,close))
				P_ChangeSwitchTexture(line,1);
			break;
		case 43:		/* Lower Ceiling to Floor */
			if (EV_DoCeiling(line,lowerToFloor))
				P_ChangeSwitchTexture(line,1);
			break;
		case 45:		/* Lower Floor to Surrounding floor height */
			if (EV_DoFloor(line,lowerFloor))
				P_ChangeSwitchTexture(line,1);
			break;
		case 60:		/* Lower Floor to Lowest */
			if (EV_DoFloor(line,lowerFloorToLowest))
				P_ChangeSwitchTexture(line,1);
			break;
		case 61:		/* Open Door */
			if (EV_DoDoor(line,open))
				P_ChangeSwitchTexture(line,1);
			break;
		case 62:		/* PlatDownWaitUpStay */
			if (EV_DoPlat(line,downWaitUpStay,1))
				P_ChangeSwitchTexture(line,1);
			break;
		case 63:		/* Raise Door */
			if (EV_DoDoor(line,normal))
				P_ChangeSwitchTexture(line,1);
			break;
		case 64:		/* Raise Floor to ceiling */
			if (EV_DoFloor(line,raiseFloor))
				P_ChangeSwitchTexture(line,1);
			break;
		case 66:		/* Raise Floor 24 and change texture */
			if (EV_DoPlat(line,raiseAndChange,24))
				P_ChangeSwitchTexture(line,1);
			break;
		case 67:		/* Raise Floor 32 and change texture */
			if (EV_DoPlat(line,raiseAndChange,32))
				P_ChangeSwitchTexture(line,1);
			break;
		case 65:		/* Raise Floor Crush */
			if (EV_DoFloor(line,raiseFloorCrush))
				P_ChangeSwitchTexture(line,1);
			break;
		case 68:		/* Raise Plat to next highest floor and change texture */
			if (EV_DoPlat(line,raiseToNearestAndChange,0))
				P_ChangeSwitchTexture(line,1);
			break;
		case 69:		/* Raise Floor to next highest floor */
			if (EV_DoFloor(line, raiseFloorToNearest))
				P_ChangeSwitchTexture(line,1);
			break;
		case 70:		/* Turbo Lower Floor */
			if (EV_DoFloor(line,turboLower))
				P_ChangeSwitchTexture(line,1);
			break;
		/*=============================================== */
		/*	SWITCHES */
		/*=============================================== */
		case 7:			/* Build Stairs */
			if (EV_BuildStairs(line))
				P_ChangeSwitchTexture(line,0);
			break;
		case 9:			/* Change Donut */
			if (EV_DoDonut(line))
				P_ChangeSwitchTexture(line,0);
			break;
		case 11:		/* Exit level */
			G_ExitLevel ();
			P_ChangeSwitchTexture(line,0);
			break;
		case 14:		/* Raise Floor 32 and change texture */
			if (EV_DoPlat(line,raiseAndChange,32))
				P_ChangeSwitchTexture(line,0);
			break;
		case 15:		/* Raise Floor 24 and change texture */
			if (EV_DoPlat(line,raiseAndChange,24))
				P_ChangeSwitchTexture(line,0);
			break;
		case 18:		/* Raise Floor to next highest floor */
			if (EV_DoFloor(line, raiseFloorToNearest))
				P_ChangeSwitchTexture(line,0);
			break;
		case 20:		/* Raise Plat next highest floor and change texture */
			if (EV_DoPlat(line,raiseToNearestAndChange,0))
				P_ChangeSwitchTexture(line,0);
			break;
		case 21:		/* PlatDownWaitUpStay */
			if (EV_DoPlat(line,downWaitUpStay,0))
				P_ChangeSwitchTexture(line,0);
			break;
		case 23:		/* Lower Floor to Lowest */
			if (EV_DoFloor(line,lowerFloorToLowest))
				P_ChangeSwitchTexture(line,0);
			break;
		case 29:		/* Raise Door */
			if (EV_DoDoor(line,normal))
				P_ChangeSwitchTexture(line,0);
			break;
		case 41:		/* Lower Ceiling to Floor */
			if (EV_DoCeiling(line,lowerToFloor))
				P_ChangeSwitchTexture(line,0);
			break;
		case 71:		/* Turbo Lower Floor */
			if (EV_DoFloor(line,turboLower))
				P_ChangeSwitchTexture(line,0);
			break;
		case 49:		/* Lower Ceiling And Crush */
			if (EV_DoCeiling(line,lowerAndCrush))
				P_ChangeSwitchTexture(line,0);
			break;
		case 50:		/* Close Door */
			if (EV_DoDoor(line,close))
				P_ChangeSwitchTexture(line,0);
			break;
		case 51:		/* Secret EXIT */
			G_SecretExitLevel ();
			P_ChangeSwitchTexture(line,0);
			break;
		case 55:		/* Raise Floor Crush */
			if (EV_DoFloor(line,raiseFloorCrush))
				P_ChangeSwitchTexture(line,0);
			break;
		case 101:		/* Raise Floor */
			if (EV_DoFloor(line,raiseFloor))
				P_ChangeSwitchTexture(line,0);
			break;
		case 102:		/* Lower Floor to Surrounding floor height */
			if (EV_DoFloor(line,lowerFloor))
				P_ChangeSwitchTexture(line,0);
			break;
		case 103:		/* Open Door */
			if (EV_DoDoor(line,open))
				P_ChangeSwitchTexture(line,0);
			break;
	}
	
	return true;
}

