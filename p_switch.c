#include "doomdef.h"
#include "p_local.h"

/*================================================================== */
/* */
/*	CHANGE THE TEXTURE OF A WALL SWITCH TO ITS OPPOSITE */
/* */
/*================================================================== */
static const switchlist_t alphSwitchList[] =
{
	{"SW1BRN1", 	"SW2BRN1"},
	{"SW1CMT",      "SW2CMT"},
	{"SW1DBLU",     "SW2DBLU"},
	{"SW1DRED",     "SW2DRED"},
	{"SW1DYEL",     "SW2DYEL"},
	{"SW1GARG", 	"SW2GARG"},
	{"SW1GSTON", 	"SW2GSTON"},
	{"SW1HOT", 		"SW2HOT"},
	{"SW1STAR", 	"SW2STAR"},
	{"SW1WOOD", 	"SW2WOOD"},
	{"SW1BLUE", 	"SW2BLUE"},
	{"SW1BRCOM", 	"SW2BRCOM"},
	{"SW1BRIK", 	"SW2BRIK"},
	{"SW1BRN2", 	"SW2BRN2"},
	{"SW1BRNGN", 	"SW2BRNGN"},
	{"SW1BROWN", 	"SW2BROWN"},
	{"SW1CMT", 		"SW2CMT"},
	{"SW1COMM", 	"SW2COMM"},
	{"SW1COMP", 	"SW2COMP"},
	{"SW1GORSK", 	"SW2GORSK"},
	{"SW1GRAY", 	"SW2GRAY"},
	{"SW1GRAY1", 	"SW2GRAY1"},
	{"SW1MET2", 	"SW2MET2"},
	{"SW1METAL", 	"SW2METAL"},
	{"SW1MOD1", 	"SW2MOD1"},
	{"SW1PANEL", 	"SW2PANEL"},
	{"SW1PIPE", 	"SW2PIPE"},
	{"SW1ROCK", 	"SW2ROCK"},
	{"SW1SLAD", 	"SW2SLAD"},
	{"SW1STON1", 	"SW2STON1"},
	{"SW1STON6", 	"SW2STON6"},
	{"SW1STONM",    "SW2STONM"},
	{"SW1TEK", 		"SW2TEK"},
	{"SW1WDMET", 	"SW2WDMET"},
	{"SW1WUD", 		"SW2WUD"},
	{"SW1ZIM", 		"SW2ZIM"},
	{"SW1S0", 		"SW1S1"},
	{"SW2S0", 		"SW2S1"},
	{"SW3S0", 		"SW3S1"},
	{"SW4S0", 		"SW4S1"}
};

uint8_t		*switchlist/*[MAXSWITCHES * 2]*/ = NULL;
VINT		numswitches;
button_t	*buttonlist/*[MAXBUTTONS]*/ = NULL;

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
	
	switchlist[index] = 0;
}

/*================================================================== */
/* */
/*	Start a button counting down till it turns off. */
/* */
/*================================================================== */
void P_StartButton(line_t *line,bwhere_e w,int texture,int time,mobj_t *soundord)
{
	int		i;

	// see if the button is already pressed
	for (i = 0; i < MAXBUTTONS; i++)
		if (buttonlist[i].btimer && buttonlist[i].line == line)
			return;

	for (i = 0;i < MAXBUTTONS;i++)
		if (!buttonlist[i].btimer)
		{
			buttonlist[i].line = line;
			buttonlist[i].where = w;
			buttonlist[i].btexture = texture;
			buttonlist[i].btimer = time;
			buttonlist[i].soundorg = soundord;
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
	mobj_t *soundorg = (void *)LD_FRONTSECTOR(line);

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
			S_StartPositionedSound(soundorg,sound,&P_SectorOrg);
			sides[line->sidenum[0]].toptexture = switchlist[i^1];
			if (useAgain)
				P_StartButton(line,top,switchlist[i],BUTTONTIME,soundorg);
			return;
		}
		else
		if (switchlist[i] == texMid)
		{
			S_StartPositionedSound(soundorg,sound,&P_SectorOrg);
			sides[line->sidenum[0]].midtexture = switchlist[i^1];
			if (useAgain)
				P_StartButton(line, middle,switchlist[i],BUTTONTIME,soundorg);
			return;
		}
		else
		if (switchlist[i] == texBot)
		{
			S_StartPositionedSound(soundorg,sound,&P_SectorOrg);
			sides[line->sidenum[0]].bottomtexture = switchlist[i^1];
			if (useAgain)
				P_StartButton(line, bottom,switchlist[i],BUTTONTIME,soundorg);
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
			case 32:	/* MANUAL BLUE */
			case 33:	/* MANUAL RED */
			case 34:	/* MANUAL YELLOW */
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
		case 26:		/* Blue Card Door Raise */
		case 27:		/* Yellow Card Door Raise */
		case 28:		/* Red Card Door Raise */

		case 31:		/* Manual door open */
		case 32:		/* Blue Card door open */
		case 33:		/* Red Card door open */
		case 34:		/* Yellow Card door open */

		case 117:		/* Blazing door raise */
		case 118:		/* Blazing door open */
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
		case 114:
			/* Blazing Door Raise(faster than TURBO!) */
			if (EV_DoDoor(line, blazeRaise))
				P_ChangeSwitchTexture(line, 1);
			break;
		case 115:
			/* Blazing Door Open(faster than TURBO!) */
			if (EV_DoDoor(line, blazeOpen))
				P_ChangeSwitchTexture(line, 1);
			break;
		case 116:
			/* Blazing Door Close(faster than TURBO!) */
			if (EV_DoDoor(line, blazeClose))
				P_ChangeSwitchTexture(line, 1);
			break;
		case 123:
			/* Blazing PlatDownWaitUpStay */
			if (EV_DoPlat(line, blazeDWUS, 0))
				P_ChangeSwitchTexture(line, 1);
			break;
		case 132:
			/* Raise Floor Turbo */
			if (EV_DoFloor(line, raiseFloorTurbo))
				P_ChangeSwitchTexture(line, 1);
			break;
		case 99:
			/* BlzOpenDoor BLUE */
		case 134:
			/* BlzOpenDoor RED */
		case 136:
			/* BlzOpenDoor YELLOW */
			if (EV_DoLockedDoor(line, blazeOpen, thing))
				P_ChangeSwitchTexture(line, 1);
			break;
		case 138:
			/* Light Turn On */
			EV_LightTurnOn(line, 255);
			P_ChangeSwitchTexture(line, 1);
			break;
		case 139:
			/* Light Turn Off */
			EV_LightTurnOn(line, 35);
			P_ChangeSwitchTexture(line, 1);
			break;
		/*=============================================== */
		/*	SWITCHES */
		/*=============================================== */
		case 7:			/* Build Stairs */
			if (EV_BuildStairs(line, build8))
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
			if (EV_DoCeiling(line,crushAndRaise))
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
		case 111:
			/* Blazing Door Raise (faster than TURBO!) */
			if (EV_DoDoor(line, blazeRaise))
				P_ChangeSwitchTexture(line, 0);
			break;
		case 112:
			/* Blazing Door Open(faster than TURBO!) */
			if (EV_DoDoor(line, blazeOpen))
				P_ChangeSwitchTexture(line, 0);
			break;
		case 113:
			/* Blazing Door Close(faster than TURBO!) */
			if (EV_DoDoor(line, blazeClose))
				P_ChangeSwitchTexture(line, 0);
			break;
		case 122:
			/* Blazing PlatDownWaitUpStay */
			if (EV_DoPlat(line, blazeDWUS, 0))
				P_ChangeSwitchTexture(line, 0);
			break;
		case 127:
			/* Build Stairs Turbo 16 */
			if (EV_BuildStairs(line, turbo16))
				P_ChangeSwitchTexture(line, 0);
			break;
		case 131:
			/* Raise Floor Turbo */
			if (EV_DoFloor(line, raiseFloorTurbo))
				P_ChangeSwitchTexture(line, 0);
			break;
		case 133:
			/* BlzOpenDoor BLUE */
		case 135:
			/* BlzOpenDoor RED */
		case 137:
			/* BlzOpenDoor YELLOW */
			if (EV_DoLockedDoor(line, blazeOpen, thing))
				P_ChangeSwitchTexture(line, 0);
			break;
		case 140:
			/* Raise Floor 512 */
			if (EV_DoFloor(line, raiseFloor512))
				P_ChangeSwitchTexture(line, 0);
			break;
	}
	
	return true;
}

