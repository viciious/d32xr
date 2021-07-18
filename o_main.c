/* o_main.c -- options menu */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"

#ifdef MARS
#define MOVEWAIT	3
#else
#define MOVEWAIT	5
#endif

#define ITEMSPACE	40
#define SLIDEWIDTH 90

extern 	int	cx, cy;
extern	int		sfxvolume;		/* range from 0 to 255 */

extern void print (int x, int y, const char *string);
extern void IN_DrawValue(int x,int y,int value);

typedef enum
{
	mi_audio,
	mi_video,
	mi_controls,

	mi_soundvol,

	mi_screensize,
	mi_detailmode,

	mi_controltype,

	NUMMENUITEMS
} menupos_t;

menupos_t	cursorpos;

typedef struct
{
	int	curval;
	int	maxval;
} slider_t;

slider_t slider[3];

typedef struct
{
	int		x;
	int		y;
	slider_t *slider;
	uint8_t screen;
	char 	name[20];
} menuitem_t;

menuitem_t menuitem[NUMMENUITEMS];

int		cursorframe, cursorcount;
int		movecount;

short	uchar;

short	o_cursor1, o_cursor2;
short	o_slider, o_slidertrack;

const char buttona[NUMCONTROLOPTIONS][8] =
		{"Speed","Speed","Fire","Fire","Use","Use"};
const char buttonb[NUMCONTROLOPTIONS][8] =
		{"Fire","Use ","Speed","Use","Speed","Fire"};
const char buttonc[NUMCONTROLOPTIONS][8] =
		{"Use","Fire","Use","Speed","Fire","Speed"};

typedef struct
{
	int firstitem;
	int numitems;
	char name[20];
} menuscreen_t;

typedef enum
{
	ms_none,
	ms_main,
	ms_video,
	ms_audio,
	ms_controls,

	NUMMENUSCREENS
} screenpos_t;

menuscreen_t menuscreen[NUMMENUSCREENS];
screenpos_t  screenpos;

/* */
/* Draw control value */
/* */
void O_DrawControl(void)
{
	//EraseBlock(menuitem[mi_controltype].x + 40, menuitem[mi_controltype].y + 20, 90, 80);
	print(menuitem[mi_controltype].x + 40, menuitem[mi_controltype].y + 20, buttona[controltype]);
	print(menuitem[mi_controltype].x + 40, menuitem[mi_controltype].y + 40, buttonb[controltype]);
	print(menuitem[mi_controltype].x + 40, menuitem[mi_controltype].y + 60, buttonc[controltype]);
/*	IN_DrawValue(30, 20, controltype); */
}

/*
===============
=
= O_Init
=
===============
*/

void O_Init (void)
{
/* cache all needed graphics */
	o_cursor1 = W_CheckNumForName("M_SKULL1");
	o_cursor2 = W_CheckNumForName("M_SKULL2");
	o_slider = W_CheckNumForName("O_SLIDER");
	o_slidertrack = W_CheckNumForName("O_STRACK");

	uchar = W_CheckNumForName("CHAR_065");

/*	initialize variables */

	cursorcount = 0;
	cursorframe = 0;
	cursorpos = 0;
	screenpos = ms_main;

	D_strncpy(menuitem[mi_audio].name, "Audio", 6);
	menuitem[mi_audio].x = 94;
	menuitem[mi_audio].y = 46;
	menuitem[mi_audio].slider = NULL;
	menuitem[mi_audio].screen = ms_audio;

	D_strncpy(menuitem[mi_video].name, "Video", 6);
	menuitem[mi_video].x = 94;
	menuitem[mi_video].y = 76;
	menuitem[mi_video].slider = NULL;
	menuitem[mi_video].screen = ms_video;

	D_strncpy(menuitem[mi_controls].name, "Controls", 8);
	menuitem[mi_controls].x = 94;
	menuitem[mi_controls].y = 106;
	menuitem[mi_controls].slider = NULL;
	menuitem[mi_controls].screen = ms_controls;


    D_strncpy(menuitem[mi_soundvol].name, "Volume", 6);
	menuitem[mi_soundvol].x = 94;
	menuitem[mi_soundvol].y = 46;
	menuitem[mi_soundvol].slider = &slider[0];
 	slider[0].maxval = 4;
	slider[0].curval = 4*sfxvolume/64;


	D_strncpy(menuitem[mi_screensize].name, "Screen size", 11);
	menuitem[mi_screensize].x = 94;
	menuitem[mi_screensize].y = 46;
	menuitem[mi_screensize].slider = &slider[1];
	slider[1].maxval = numViewports - 1;
	slider[1].curval = 0;

	D_strncpy(menuitem[mi_detailmode].name, "Level of detail", 15);
	menuitem[mi_detailmode].x = 94;
	menuitem[mi_detailmode].y = 86;
	menuitem[mi_detailmode].slider = &slider[2];
	slider[2].maxval = MAXDETAILMODES;
	slider[2].curval = detailmode + 1;


	D_strncpy(menuitem[mi_controltype].name, "Gamepad", 7);
	menuitem[mi_controltype].x = 94;
	menuitem[mi_controltype].y = 46;
	menuitem[mi_controltype].slider = NULL;


	D_strncpy(menuscreen[ms_main].name, "Options", 7);
	menuscreen[ms_main].firstitem = mi_audio;
	menuscreen[ms_main].numitems = mi_controls - mi_audio + 1;

	D_strncpy(menuscreen[ms_audio].name, "Audio", 6);
	menuscreen[ms_audio].firstitem = mi_soundvol;
	menuscreen[ms_audio].numitems = mi_soundvol - mi_soundvol + 1;

	D_strncpy(menuscreen[ms_video].name, "Video", 6);
	menuscreen[ms_video].firstitem = mi_screensize;
	menuscreen[ms_video].numitems = mi_detailmode - mi_screensize + 1;

	D_strncpy(menuscreen[ms_controls].name, "Controls", 8);
	menuscreen[ms_controls].firstitem = mi_controltype;
	menuscreen[ms_controls].numitems = 1;
}

/*
==================
=
= O_Control
=
= Button bits can be eaten by clearing them in ticbuttons[playernum]
==================
*/

void O_Control (player_t *player)
{
	int		buttons, oldbuttons;
	char	newframe = false;
	menuscreen_t* menuscr = &menuscreen[screenpos];

	buttons = ticbuttons[playernum];
	oldbuttons = oldticbuttons[playernum];
	
	if ( ( (buttons & BT_OPTION) && !(oldbuttons & BT_OPTION) )
#ifdef MARS
		|| ( (buttons & BT_START) && !(oldbuttons & BT_START) ) 
#endif
		)
	{
		gamepaused ^= 1;
		cursorpos = 0;	
		screenpos = ms_main;
		player->automapflags ^= AF_OPTIONSACTIVE;
#ifndef MARS
		if (player->automapflags & AF_OPTIONSACTIVE)
			DoubleBufferSetup ();
		else
			WriteEEProm ();		/* save new settings */
#endif
	}
	if ( !(player->automapflags & AF_OPTIONSACTIVE) )
		return;

/* clear buttons so game player isn't moving aroung */
	ticbuttons[playernum] &= (BT_OPTION|BT_START);	/* leave option status alone */

	if (playernum != consoleplayer)
		return;

/* animate skull */
	if (++cursorcount == ticrate)
	{
		cursorframe ^= 1;
		cursorcount = 0;
		newframe = true;
	}

	if (!newframe)
		return;

	if (buttons & (BT_A|BT_LMBTN))
	{
		int itemno = menuscr->firstitem + cursorpos;
		if (menuitem[itemno].screen != ms_none)
		{
			cursorpos = 0;
			screenpos = menuitem[itemno].screen;
			clearscreen = 2;
			return;
		}
	}

	if (buttons & (BT_C|BT_RMBTN))
	{
		if (screenpos != ms_main)
		{
			int i;
			menuscreen_t* mainscr = &menuscreen[ms_main];

			cursorpos = 0;
			for (i = mainscr->firstitem; i < mainscr->firstitem + mainscr->numitems; i++)
			{
				if (menuitem[i].screen == screenpos) {
					cursorpos = i - mainscr->firstitem;
					break;
				}
			}
			screenpos = ms_main;
			clearscreen = 2;
			return;
		}
	}

/* check for movement */
	if (! (buttons & (BT_UP| BT_DOWN| BT_LEFT| BT_RIGHT) ) )
		movecount = 0;		/* move immediately on next press */
	else
	{
		int itemno = menuscr->firstitem + cursorpos;
		slider_t*slider = menuitem[itemno].slider;

		if (slider && (buttons & (BT_RIGHT|BT_LEFT)))
		{
			if (buttons & BT_RIGHT)
			{
				slider->curval++;
					if (slider->curval > slider->maxval)
						slider->curval = slider->maxval;
			}
			if (buttons & BT_LEFT)
			{
				slider->curval--;
					if (slider->curval < 0)
						slider->curval = 0;
			}

			switch (itemno)
			{
			case mi_soundvol:
				sfxvolume = 64* slider->curval / slider->maxval;
				S_StartSound (NULL, sfx_pistol);
				break;
			case mi_screensize:
				R_SetViewportSize(slider->curval);
				break;
			case mi_detailmode:
				R_SetDetailMode(slider->curval - 1);
				break;
			default:
				break;
			}
		}

		if (movecount == MOVEWAIT)
			movecount = 0;		/* repeat move */

		if (++movecount == 1)
		{
			if (buttons & BT_DOWN)
			{
				cursorpos++;
				if (cursorpos == menuscr->numitems)
					cursorpos = 0;
			}
		
			if (buttons & BT_UP)
			{
				cursorpos--;
				if (cursorpos == -1)
					cursorpos = menuscr->numitems-1;
			}

			if (screenpos == ms_controls)
			{
				if (buttons & BT_RIGHT)
				{
					{
						controltype++;
						if (controltype == NUMCONTROLOPTIONS)
							controltype = (NUMCONTROLOPTIONS - 1);
					}
				}
				if (buttons & BT_LEFT)
				{
					{
						controltype--;
						if (controltype == -1)
							controltype = 0;
					}
				}
			}
		}
	}
}

void O_Drawer (void)
{
	int		i;
	int		offset;
	int		y;
	menuscreen_t* menuscr = &menuscreen[screenpos];
	menuitem_t* items = &menuitem[menuscr->firstitem];

	if (o_cursor1 < 0)
		return;

/* Erase old and Draw new cursor frame */
	//EraseBlock(56, 40, o_cursor1->width, 200);
	if(cursorframe)
		DrawJagobjLump(o_cursor1, 60, items[cursorpos].y - 2, NULL, NULL);
	else
		DrawJagobjLump(o_cursor2, 60, items[cursorpos].y - 2, NULL, NULL);

/* Draw menu */

	y = 10;
	print(104, y, menuscr->name);
	
	for (i = 0; i < menuscr->numitems; i++)
	{
		y = items[i].y;
		print(items[i].x, y, items[i].name);

		if(items[i].slider)
		{
			DrawJagobjLump(o_slidertrack, items[i].x + 2, items[i].y + 20, NULL, NULL);
			offset = (items[i].slider->curval * SLIDEWIDTH) / items[i].slider->maxval;
			DrawJagobjLump(o_slider, items[i].x + 7 + offset, items[i].y + 20, NULL, NULL);
/*			ST_Num(menuitem[i].x + o_slider->width + 10,	 */
/*			menuitem[i].y + 20,slider[i].curval);  */
		}			 
	}
	
/* Draw control info */
	if (screenpos == ms_controls)
	{
		print(items[0].x + 10, items[0].y + 20, "A");
		print(items[0].x + 10, items[0].y + 40, "B");
		print(items[0].x + 10, items[0].y + 60, "C");
		O_DrawControl();
	}

/* debug stuff */
#if 0
	cx = 30;
	cy = 40;
	D_printf("Speed = %d", BT_SPEED);
	cy = 60;
	D_printf("Use/Strafe = %d", BT_SPEED);
	cy = 80;
	D_printf("Fire = %d", BT_SPEED);
#endif
/* end of debug stuff */

#ifndef MARS
	UpdateBuffer();
#endif
}

