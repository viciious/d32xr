/* f_main.c -- finale */

#include "doomdef.h"
#include "r_local.h"

extern int mystrlen (char *string);

/*
==================
=
= BufferedDrawSprite
=
= Cache and draw a game sprite to the 8 bit buffered screen
==================
*/

void BufferedDrawSprite (int sprite, int frame, int rotation)
{
#ifndef MARS
	spritedef_t	*sprdef;
	spriteframe_t	*sprframe;
	patch_t		*patch;
	byte		*pixels, *src, *dest, pix;
	int			count;
	int			x, sprleft, sprtop;
	column_t	*column;
	int			lump;
	boolean		flip;
	int			texturecolumn;
	
	if ((unsigned)sprite >= NUMSPRITES)
		I_Error ("BufferedDrawSprite: invalid sprite number %i "
		,sprite);
	sprdef = &sprites[sprite];
	if ( (frame&FF_FRAMEMASK) >= sprdef->numframes )
		I_Error ("BufferedDrawSprite: invalid sprite frame %i : %i "
		,sprite,frame);
	sprframe = &sprdef->spriteframes[ frame & FF_FRAMEMASK];

	lump = sprframe->lump[rotation];
	flip = (boolean)sprframe->flip[rotation];

	patch = (patch_t *)W_POINTLUMPNUM(lump);
	pixels = Z_Malloc (lumpinfo[lump+1].size, PU_STATIC, NULL);
	W_ReadLump (lump+1,pixels);

	S_UpdateSounds ();
	 	
/* */
/* coordinates are in a 160*112 screen (doubled pixels) */
/* */
	sprtop = 90;
	sprleft = 80;
	
	sprtop -= patch->topoffset;
	sprleft -= patch->leftoffset;
	
/* */
/* draw it by hand */
/* */
	for (x=0 ; x<patch->width ; x++)
	{
		if (flip)
			texturecolumn = patch->width-1-x;
		else
			texturecolumn = x;
			
		column = (column_t *) ((byte *)patch +
		 BIGSHORT(patch->columnofs[texturecolumn]));

/* */
/* draw a masked column */
/* */
		for ( ; column->topdelta != 0xff ; column++) 
		{
	/* calculate unclipped screen coordinates for post */
			dest = bufferpage + (short)(sprtop+column->topdelta)*(short)640+(sprleft + x)*2;
			count = column->length;
			src = pixels + column->dataofs;
			while (count--)
			{
				pix = *src++;
				if (!pix)
					pix = 8;
				dest[0] = dest[1] = dest[320] = dest[321] = pix;
				dest += 640;
			}
		}
	}
	
	Z_Free (pixels);	
#endif
}


/*============================================================================ */

typedef struct
{
	char		*name;
	mobjtype_t	type;
} castinfo_t;

castinfo_t	castorder[] = {
{"zombieman", MT_POSSESSED},
{"shotgun guy", MT_SHOTGUY},
{"imp", MT_TROOP},
{"demon", MT_SERGEANT},
{"lost soul", MT_SKULL},
{"cacodemon", MT_HEAD},
{"baron of hell", MT_BRUISER},
{"our hero", MT_PLAYER},

{NULL,0}
};

int			castnum;
int			casttics;
state_t		*caststate;
boolean		castdeath;
int			castframes;
int			castonmelee;
boolean		castattacking;

typedef enum
{
	fin_endtext,
	fin_charcast
} final_e;

final_e	status;
#define TEXTTIME	4
#define STARTX		8
#define STARTY		8
boolean textprint;	/* is endtext done printing? */
int		textindex;
int		textdelay;
int		text_x;
int		text_y;
#define SPACEWIDTH	8
#define NUMENDOBJ	28
jagobj_t	*endobj[NUMENDOBJ];
#if 0
/* '*' = newline */
char	endtextstring[] =
	"you did it! by turning*"
	"the evil of the horrors*"
	"of hell in upon itself*"
	"you have destroyed the*"
	"power of the demons.**"
	"their dreadful invasion*"
	"has been stopped cold!*"
	"now you can retire to*"
	"a lifetime of frivolity.**"
	"  congratulations!";
#endif

/* '*' = newline */
char	endtextstring[] =
	"     id software*"
	"     salutes you!*"
	"*"
	"  the horrors of hell*"
	"  could not kill you.*"
	"  their most cunning*"
	"  traps were no match*"
	"  for you. you have*"
	"  proven yourself the*"
	"  best of all!*"
	"*"
	"  congratulations!";

/*=============================================== */
/* */
/* Print a string in big font - LOWERCASE INPUT ONLY! */
/* */
/*=============================================== */
void F_PrintString(char *string)
{
	int		index;
	int		val;
	
	index = 0;
	while(1)
	{
		switch(string[index])
		{
			case 0: return;
			case ' ':
				text_x += SPACEWIDTH;
				val = 30;
				break;
			case '.':
				val = 26;
				break;
			case '!':
				val = 27;
				break;
			case '*':
				val = 30;
				text_x = STARTX;
				text_y += endobj[0]->height + 4;
				break;
			default:
				val = string[index] - 'a';
				break;
		}
		if (val < NUMENDOBJ)
		{
			DrawJagobj(endobj[val],text_x,text_y);
			text_x += endobj[val]->width;
			if (text_x > 316)
			{
				text_x = STARTX;
				text_y += endobj[val]->height + 4;
			}
		}
		index++;
	}
}

/*=============================================== */
/* */
/* Print character cast strings */
/* */
/*=============================================== */
void F_CastPrint(char *string)
{
	int		i,width,slen;
	
	width = 0;
	slen = mystrlen(string);
	for (i = 0;i < slen; i++)
		switch(string[i])
		{
			case ' ': width += SPACEWIDTH; break;
			default : width += endobj[string[i] - 'a']->width;
		}

	text_x = 160 - (width >> 1);
	text_y = 20;
	F_PrintString(string);
}

/*
=======================
=
= F_Start
=
=======================
*/

void F_Start (void)
{
	int	i;
	int	l;
	
	S_StartSong(2, 1);

	status = fin_endtext;		/* END TEXT PRINTS FIRST */
	textprint = false;
	textindex = 0;
	textdelay = TEXTTIME;
	text_x = STARTX;
	text_y = STARTY;
	l = W_GetNumForName ("CHAR_097");
	for (i = 0; i < NUMENDOBJ; i++)
		endobj[i] = W_CacheLumpNum(l+i, PU_STATIC);

	castnum = 0;
	caststate = &states[mobjinfo[castorder[castnum].type].seestate];
	casttics = caststate->tics;
	castdeath = false;
	castframes = 0;
	castonmelee = 0;
	castattacking = false;

	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));

	DoubleBufferSetup ();
}

void F_Stop (void)
{
	int	i;
	
	for (i = 0;i < NUMENDOBJ; i++)
		Z_Free(endobj[i]);
}




/*
=======================
=
= F_Ticker
=
=======================
*/

int F_Ticker (void)
{
	int		st;
	int		buttons, oldbuttons;

	
/* */
/* check for press a key to kill actor */
/* */
	buttons = ticbuttons[consoleplayer];
	oldbuttons = oldticbuttons[consoleplayer];
	
	if (status == fin_endtext)
	{
		if (textindex == (3*15)/TEXTTIME)
			textprint = true;
			
		if (( ((buttons & BT_A) && !(oldbuttons & BT_A) )
		|| ((buttons & BT_B) && !(oldbuttons & BT_B) )
		|| ((buttons & BT_C) && !(oldbuttons & BT_C) ) ) &&
		textprint == true)
		{
			status = fin_charcast;
/*			S_StartSound (NULL, mobjinfo[castorder[castnum].type].seesound); */
		}
		return 0;
	}
	
	if (!castdeath)
	{
		if ( ((buttons & BT_A) && !(oldbuttons & BT_A) )
		|| ((buttons & BT_B) && !(oldbuttons & BT_B) )
		|| ((buttons & BT_C) && !(oldbuttons & BT_C) ) )
		{
		/* go into death frame */
			castdeath = true;
			caststate = &states[mobjinfo[castorder[castnum].type].deathstate];
			casttics = caststate->tics;
			castframes = 0;
			castattacking = false;
		}
	}
	

/* */
/* advance state */
/* */
	if (--casttics > 0)
		return 0;			/* not time to change state yet */
		
	if (caststate->tics == -1 || caststate->nextstate == S_NULL)
	{	/* switch from deathstate to next monster */
		castnum++;
		castdeath = false;
		if (castorder[castnum].name == NULL)
			castnum = 0;
/*		if (mobjinfo[castorder[castnum].type].seesound) */
/*			S_StartSound (NULL, mobjinfo[castorder[castnum].type].seesound); */
		caststate = &states[mobjinfo[castorder[castnum].type].seestate];
		castframes = 0;
	}
	else
	{	/* just advance to next state in animation */
		if (caststate == &states[S_PLAY_ATK1])
			goto stopattack;	/* Oh, gross hack! */
		st = caststate->nextstate;
		caststate = &states[st];
		castframes++;
#if 0
/*============================================== */
/* sound hacks.... */
{
	int sfx;
	
		switch (st)
		{
		case S_PLAY_ATK1: sfx = sfx_dshtgn; break;
		case S_POSS_ATK2: sfx = sfx_pistol; break;
		case S_SPOS_ATK2: sfx = sfx_shotgn; break;
		case S_VILE_ATK2: sfx = sfx_vilatk; break;
		case S_SKEL_FIST2: sfx = sfx_skeswg; break;
		case S_SKEL_FIST4: sfx = sfx_skepch; break;
		case S_SKEL_MISS2: sfx = sfx_skeatk; break;
		case S_FATT_ATK8:
		case S_FATT_ATK5:
		case S_FATT_ATK2: sfx = sfx_firsht; break;
		case S_CPOS_ATK2:
		case S_CPOS_ATK3:
		case S_CPOS_ATK4: sfx = sfx_shotgn; break;
		case S_TROO_ATK3: sfx = sfx_claw; break;
		case S_SARG_ATK2: sfx = sfx_sgtatk; break;
		case S_BOSS_ATK2: 
		case S_BOS2_ATK2:
		case S_HEAD_ATK2: sfx = sfx_firsht; break;
		case S_SKULL_ATK2: sfx = sfx_sklatk; break;
		case S_SPID_ATK2:
		case S_SPID_ATK3: sfx = sfx_shotgn; break;
		case S_BSPI_ATK2: sfx = sfx_plasma; break;
		case S_CYBER_ATK2:
		case S_CYBER_ATK4:
		case S_CYBER_ATK6: sfx = sfx_rlaunc; break;
		case S_PAIN_ATK3: sfx = sfx_sklatk; break;
		default: sfx = 0; break;
		}
		
/*		if (sfx) */
/*			S_StartSound (NULL, sfx); */
}
#endif
/*============================================== */
	}
	
	if (castframes == 12)
	{	/* go into attack frame */
		castattacking = true;
		if (castonmelee)
			caststate=&states[mobjinfo[castorder[castnum].type].meleestate];
		else
			caststate=&states[mobjinfo[castorder[castnum].type].missilestate];
		castonmelee ^= 1;
		if (caststate == &states[S_NULL])
		{
			if (castonmelee)
				caststate=
				&states[mobjinfo[castorder[castnum].type].meleestate];
			else
				caststate=
				&states[mobjinfo[castorder[castnum].type].missilestate];
		}
	}
	
	if (castattacking)
	{
		if (castframes == 24 
		||	caststate == &states[mobjinfo[castorder[castnum].type].seestate] )
		{
stopattack:
			castattacking = false;
			castframes = 0;
			caststate = &states[mobjinfo[castorder[castnum].type].seestate];
		}
	}
	
	casttics = caststate->tics;
	if (casttics == -1)
		casttics = 15;
		
	return 0;		/* finale never exits */
}



/*
=======================
=
= F_Drawer
=
=======================
*/

void F_Drawer (void)
{
		
	switch(status)
	{
		case fin_endtext:
			if (!--textdelay)
			{
				char	str[2];
				
				str[1] = 0;
				str[0] = endtextstring[textindex];
				if (!str[0])
					return;
				F_PrintString(str);
				textdelay = TEXTTIME;
				textindex++;
			}
			break;
			
		case fin_charcast:
			EraseBlock (0,0,320,200);
			F_CastPrint (castorder[castnum].name);
				
			BufferedDrawSprite (caststate->sprite,
					caststate->frame&FF_FRAMEMASK,0);
			break;
	}
	UpdateBuffer ();
}

