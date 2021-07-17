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

extern void print (int x, int y, char *string);
extern void IN_DrawValue(int x,int y,int value);

typedef enum
{
	mi_soundvol,
	mi_screensize,
	mi_detailmode,
#ifndef MARS
	mi_controls,
#endif
	NUMMENUITEMS
} menupos_t;

menupos_t	cursorpos;

typedef struct
{
	int		x;
	int		y;
	boolean	hasslider;
	char 		name[20];
} menuitem_t;

menuitem_t menuitem[NUMMENUITEMS];
 
typedef struct
{
	int	curval;
	int	maxval;
} slider_t;

slider_t slider[3];

int		cursorframe, cursorcount;
int		movecount;

short	uchar;

short	o_cursor1, o_cursor2;
short	o_slider, o_slidertrack;

char buttona[NUMCONTROLOPTIONS][8] =
		{"Speed","Speed","Fire","Fire","Use","Use"};
char buttonb[NUMCONTROLOPTIONS][8] = 
		{"Fire","Use ","Speed","Use","Speed","Fire"};
char buttonc[NUMCONTROLOPTIONS][8] =
		{"Use","Fire","Use","Speed","Fire","Speed"};

void DrawJagobjLump(int lumpnum, int x, int y, int* ow, int* oh);

/* */
/* Draw control value */
/* */
void O_DrawControl(void)
{
#ifndef MARS
	//EraseBlock(menuitem[mi_controls].x + 40, menuitem[mi_controls].y + 20, 90, 80);
	print(menuitem[mi_controls].x + 40, menuitem[mi_controls].y + 20, buttona[controltype]);
	print(menuitem[mi_controls].x + 40, menuitem[mi_controls].y + 40, buttonb[controltype]);
	print(menuitem[mi_controls].x + 40, menuitem[mi_controls].y + 60, buttonc[controltype]);
/*	IN_DrawValue(30, 20, controltype); */
#endif
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

/*    strcpy(menuitem[0].name, "Volume"); */
    D_strncpy(menuitem[mi_soundvol].name, "Volume", 6);
	menuitem[mi_soundvol].x = 94;
	menuitem[mi_soundvol].y = 46;
	menuitem[mi_soundvol].hasslider = true;

 	slider[mi_soundvol].maxval = 4;
	slider[mi_soundvol].curval = 4*sfxvolume/64;

	D_strncpy(menuitem[mi_screensize].name, "Screen size", 11);
	menuitem[mi_screensize].x = 94;
	menuitem[mi_screensize].y = 80;
	menuitem[mi_screensize].hasslider = true;

	slider[mi_screensize].maxval = numViewports - 1;
	slider[mi_screensize].curval = 0;

	D_strncpy(menuitem[mi_detailmode].name, "Level of detail", 15);
	menuitem[mi_detailmode].x = 94;
	menuitem[mi_detailmode].y = 114;
	menuitem[mi_detailmode].hasslider = true;

	slider[mi_detailmode].maxval = MAXDETAILMODES;
	slider[mi_detailmode].curval = detailmode + 1;

#ifndef MARS
/*    strcpy(menuitem[mi_controls].name, "Controls"); */
    D_strncpy(menuitem[mi_controls].name, "Controls", 8); /* Fixed CEF */
	menuitem[mi_controls].x = 94;
	menuitem[mi_controls].y = 100;
	menuitem[mi_controls].hasslider = false;
#endif
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
	
	buttons = ticbuttons[playernum];
	oldbuttons = oldticbuttons[playernum];
	
	if ( (buttons & BT_OPTION) && !(oldbuttons & BT_OPTION) )
	{
		gamepaused ^= 1;
		cursorpos = 0;	
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
	ticbuttons[playernum] &= BT_OPTION;	/* leave option status alone */

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

/* check for movement */
	if (! (buttons & (BT_UP| BT_DOWN| BT_LEFT| BT_RIGHT) ) )
		movecount = 0;		/* move immediately on next press */
	else
	{
			if (buttons & BT_RIGHT)
			{
				if (menuitem[cursorpos].hasslider)
				{
					slider[cursorpos].curval++;
					if (slider[cursorpos].curval > slider[cursorpos].maxval)
						slider[cursorpos].curval = slider[cursorpos].maxval;
					switch (cursorpos)
					{
					case mi_soundvol:
						sfxvolume = 64*slider[0].curval / slider[0].maxval;
						S_StartSound (NULL, sfx_pistol);
						break;
					case mi_screensize:
						R_SetViewportSize(slider[cursorpos].curval);
						break;
					case mi_detailmode:
						R_SetDetailMode(slider[cursorpos].curval - 1);
						break;
					default:
						break;
					}
				}
			}
			if (buttons & BT_LEFT)
			{
				if (menuitem[cursorpos].hasslider)
				{
					slider[cursorpos].curval--;
					if (slider[cursorpos].curval < 0)
						slider[cursorpos].curval = 0;
					switch (cursorpos)
					{
					case mi_soundvol:
						sfxvolume = 64*slider[0].curval / slider[0].maxval;
						S_StartSound (NULL, sfx_pistol);
						break;
					case mi_screensize:
						R_SetViewportSize(slider[cursorpos].curval);
						break;
					case mi_detailmode:
						R_SetDetailMode(slider[cursorpos].curval - 1);
						break;
					default:
						break;
					}
				}
			}

		if (movecount == MOVEWAIT)
			movecount = 0;		/* repeat move */

		if (++movecount == 1)
		{
			if (buttons & BT_DOWN)
			{
				cursorpos++;
				if (cursorpos == NUMMENUITEMS)
					cursorpos = 0;
			}
		
			if (buttons & BT_UP)
			{
				cursorpos--;
				if (cursorpos == -1)
					cursorpos = NUMMENUITEMS-1;
			}
#ifndef MARS
			if (buttons & BT_RIGHT)
			{
				if (cursorpos == controls)
				{
					controltype++;
					if(controltype == NUMCONTROLOPTIONS)
						controltype = (NUMCONTROLOPTIONS - 1); 
				}			
			}
			if (buttons & BT_LEFT)
			{
				if (cursorpos == controls)
				{
					controltype--;
					if(controltype == -1)
						controltype = 0; 
				}
			}
#endif
		}
	}
}

void O_Drawer (void)
{
	int		i;
	int		offset;

	if (o_cursor1 < 0)
		return;

/* Erase old and Draw new cursor frame */
	//EraseBlock(56, 40, o_cursor1->width, 200);
	if(cursorframe)
		DrawJagobjLump(o_cursor1, 60, menuitem[cursorpos].y - 2, NULL, NULL);
	else
		DrawJagobjLump(o_cursor2, 60, menuitem[cursorpos].y - 2, NULL, NULL);

/* Draw menu */

	print(104, 10, "Options");
	
	for (i = 0; i < NUMMENUITEMS; i++)
	{
		print(menuitem[i].x, menuitem[i].y, menuitem[i].name);		

		if(menuitem[i].hasslider == true)
		{
			DrawJagobjLump(o_slidertrack , menuitem[i].x + 2, menuitem[i].y + 20, NULL, NULL);
			offset = (slider[i].curval * SLIDEWIDTH) / slider[i].maxval;
			DrawJagobjLump(o_slider, menuitem[i].x + 7 + offset, menuitem[i].y + 20, NULL, NULL);
/*			ST_Num(menuitem[i].x + o_slider->width + 10,	 */
/*			menuitem[i].y + 20,slider[i].curval);  */
		}			 
	}	
	
/* Draw control info */
#ifndef MARS
	print(menuitem[controls].x + 10, menuitem[controls].y + 20, "A");
	print(menuitem[controls].x + 10, menuitem[controls].y + 40, "B");
	print(menuitem[controls].x + 10, menuitem[controls].y + 60, "C");

	O_DrawControl();
#endif

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

