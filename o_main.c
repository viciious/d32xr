/* o_main.c -- options menu */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"
#ifdef MARS
#include "mars.h"
#endif

#define MOVEWAIT		TICVBLS*6
#define CURSORX		50
#define ITEMSPACE	40
#define SLIDEWIDTH 90

extern void print (int x, int y, const char *string);
extern void IN_DrawValue(int x,int y,int value);

typedef enum
{
	mi_game, 
	mi_audio,
	mi_video,
	mi_controls,

	mi_soundvol,
	mi_music,

	mi_resolution,
	mi_detailmode,

	mi_controltype,
	mi_alwaysrun,
	mi_strafebtns,

	NUMMENUITEMS
} menupos_t;

menupos_t	cursorpos;

typedef struct
{
	char	curval;
	char	maxval;
} slider_t;

static slider_t slider[3];

typedef struct
{
	VINT	x;
	VINT	y;
	slider_t *slider;
	char 	name[16];
	uint8_t screen;
} menuitem_t;

static menuitem_t menuitem[NUMMENUITEMS];

static VINT	cursorframe;
static VINT cursordelay;
static VINT	movecount;

static VINT	uchar;

static VINT	o_cursor1, o_cursor2;
static VINT	o_slider, o_slidertrack;

VINT	o_musictype;

static const char buttona[NUMCONTROLOPTIONS][8] =
		{"Speed","Speed","Fire","Fire","Use","Use"};
static const char buttonb[NUMCONTROLOPTIONS][8] =
		{"Fire","Use ","Speed","Use","Speed","Fire"};
static const char buttonc[NUMCONTROLOPTIONS][8] =
		{"Use","Fire","Use","Speed","Fire","Speed"};

typedef struct
{
	VINT firstitem;
	VINT numitems;
	char name[10];
} menuscreen_t;

typedef enum
{
	ms_none,
	ms_main,
	ms_game,
	ms_audio,
	ms_video,
	ms_controls,

	NUMMENUSCREENS
} screenpos_t;

static menuscreen_t menuscreen[NUMMENUSCREENS];
static screenpos_t  screenpos;

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

	o_musictype = musictype;

	uchar = W_CheckNumForName("CHAR_065");

/*	initialize variables */

	cursorframe = -1;
	cursordelay = MOVEWAIT;
	cursorpos = 0;
	screenpos = ms_none;

	D_memset(menuitem, 0, sizeof(menuitem));
	D_memset(slider, 0, sizeof(slider));

	D_strncpy(menuitem[mi_game].name, "Game", 4);
	menuitem[mi_game].x = 74;
	menuitem[mi_game].y = 36;
	menuitem[mi_game].slider = NULL;
	menuitem[mi_game].screen = ms_game;

	D_strncpy(menuitem[mi_audio].name, "Audio", 6);
	menuitem[mi_audio].x = 74;
	menuitem[mi_audio].y = 66;
	menuitem[mi_audio].slider = NULL;
	menuitem[mi_audio].screen = ms_audio;

	D_strncpy(menuitem[mi_video].name, "Video", 6);
	menuitem[mi_video].x = 74;
	menuitem[mi_video].y = 96;
	menuitem[mi_video].slider = NULL;
	menuitem[mi_video].screen = ms_video;

	D_strncpy(menuitem[mi_controls].name, "Controls", 8);
	menuitem[mi_controls].x = 74;
	menuitem[mi_controls].y = 126;
	menuitem[mi_controls].slider = NULL;
	menuitem[mi_controls].screen = ms_controls;


	D_strncpy(menuitem[mi_soundvol].name, "Sfx volume", 10);
	menuitem[mi_soundvol].x = 74;
	menuitem[mi_soundvol].y = 36;
	menuitem[mi_soundvol].slider = &slider[0];
 	slider[0].maxval = 4;
	slider[0].curval = 4*sfxvolume/64;

	D_strncpy(menuitem[mi_music].name, "Music", 5);
	menuitem[mi_music].x = 74;
	menuitem[mi_music].y = 76;
	menuitem[mi_music].slider = NULL;


	D_strncpy(menuitem[mi_resolution].name, "Resolution", 10);
	menuitem[mi_resolution].x = 74;
	menuitem[mi_resolution].y = 36;
	menuitem[mi_resolution].slider = &slider[1];
	slider[1].maxval = numViewports - 1;
	slider[1].curval = viewportNum;

	D_strncpy(menuitem[mi_detailmode].name, "Level of detail", 15);
	menuitem[mi_detailmode].x = 74;
	menuitem[mi_detailmode].y = 76;
	menuitem[mi_detailmode].slider = &slider[2];
	slider[2].maxval = MAXDETAILMODES;
	slider[2].curval = detailmode + 1;


	D_strncpy(menuitem[mi_controltype].name, "Gamepad", 7);
	menuitem[mi_controltype].x = 74;
	menuitem[mi_controltype].y = 36;
	menuitem[mi_controltype].slider = NULL;

	D_strncpy(menuitem[mi_alwaysrun].name, "Always run", 10);
	menuitem[mi_alwaysrun].x = 74;
	menuitem[mi_alwaysrun].y = 116;

	D_strncpy(menuitem[mi_strafebtns].name, "LR Strafe", 10);
	menuitem[mi_strafebtns].x = 74;
	menuitem[mi_strafebtns].y = 136;


	D_strncpy(menuscreen[ms_main].name, "Options", 7);
	menuscreen[ms_main].firstitem = mi_game;
	menuscreen[ms_main].numitems = mi_controls - mi_game + 1;

	D_strncpy(menuscreen[ms_audio].name, "Audio", 6);
	menuscreen[ms_audio].firstitem = mi_soundvol;
	menuscreen[ms_audio].numitems = mi_music - mi_soundvol + 1;

	D_strncpy(menuscreen[ms_video].name, "Video", 6);
	menuscreen[ms_video].firstitem = mi_resolution;
	menuscreen[ms_video].numitems = mi_detailmode - mi_resolution + 1;

	D_strncpy(menuscreen[ms_controls].name, "Controls", 8);
	menuscreen[ms_controls].firstitem = mi_controltype;
	menuscreen[ms_controls].numitems = mi_strafebtns - mi_controltype + 1;
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
	menuscreen_t* menuscr;

	if (cursorframe == -1)
	{
		cursorframe = 0;
		cursordelay = MOVEWAIT+MOVEWAIT/2;
	}
	if (screenpos == ms_none)
	{
		screenpos = ms_main;
	}
	menuscr = &menuscreen[screenpos];

	buttons = ticbuttons[playernum];
	oldbuttons = oldticbuttons[playernum];
	
	if ( ( (buttons & BT_OPTION) && !(oldbuttons & BT_OPTION) )
#ifdef MARS
		|| ( (buttons & BT_START) && !(oldbuttons & BT_START) && !(buttons & BT_MODE) )
#endif
		)
	{
exit:
		if (screenpos == ms_game)
		{
			M_Stop();
		}
		gamepaused ^= 1;
		movecount = 0;
		cursorpos = 0;	
		screenpos = ms_main;
		player->automapflags ^= AF_OPTIONSACTIVE;
		if (player->automapflags & AF_OPTIONSACTIVE)
#ifdef MARS
			;
#else
			DoubleBufferSetup();
#endif
		else
			WriteEEProm ();		/* save new settings */
		S_StartSound(NULL, sfx_swtchn);
	}
	if (!(player->automapflags & AF_OPTIONSACTIVE))
		return;

/* clear buttons so game player isn't moving aroung */
	ticbuttons[playernum] &= (BT_OPTION|BT_START);	/* leave option status alone */

	if (playernum != consoleplayer)
		return;

	if (screenpos == ms_game)
	{
		int mtick = M_Ticker();
		if (mtick == ga_nothing)
			return;

		M_Stop();

		movecount = 0;
		cursorpos = 0;
		screenpos = ms_main;
		clearscreen = 2;

		switch (mtick)
		{
		case ga_completed:
			S_StartSound(NULL, sfx_swtchn);
			return;
		case ga_startnew:
			starttype = gt_single;
			gameaction = ga_startnew;
		case ga_died:
			goto exit;
		}
		return;
	}

/* animate skull */
	if (gametic != prevgametic && (gametic&3) == 0)
	{
		cursorframe ^= 1;
	}

	buttons = ticrealbuttons;
	oldbuttons = oldticrealbuttons;

	if (buttons & (BT_A | BT_LMBTN) && !(oldbuttons & (BT_A | BT_LMBTN)))
	{
		int itemno = menuscr->firstitem + cursorpos;
		if (menuitem[itemno].screen != ms_none)
		{
			movecount = 0;
			cursorpos = 0;
			screenpos = menuitem[itemno].screen;
			clearscreen = 2;

			if (screenpos == ms_game)
				M_Start2(false);
			else
				S_StartSound(NULL, sfx_pistol);
			return;
		}
	}

	if (buttons & (BT_C | BT_RMBTN) && !(oldbuttons & (BT_C | BT_RMBTN)))
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
			movecount = 0;
			screenpos = ms_main;
			clearscreen = 2;
			S_StartSound(NULL, sfx_swtchn);
			return;
		}
	}

	if (buttons == 0)
	{
		cursordelay = 0;
		return;
	}

	cursordelay -= vblsinframe[0];
	if (cursordelay > 0)
		return;

	cursordelay = MOVEWAIT;

/* check for movement */
	if (! (buttons & (BT_UP| BT_DOWN| BT_LEFT| BT_RIGHT) ) )
		movecount = 0;		/* move immediately on next press */
	else
	{
		int sound = sfx_None;
		int itemno = menuscr->firstitem + cursorpos;
		slider_t*slider = menuitem[itemno].slider;

		if (slider && (buttons & (BT_RIGHT|BT_LEFT)))
		{
			boolean sliderchange = false;

			if (buttons & BT_RIGHT)
			{
				if (++slider->curval > slider->maxval)
					slider->curval = slider->maxval;
				else
					sliderchange = true;
			}
			if (buttons & BT_LEFT)
			{
				if (--slider->curval < 0)
					slider->curval = 0;
				else
					sliderchange = true;
			}

			if (sliderchange)
			{
				switch (itemno)
				{
				case mi_soundvol:
					sfxvolume = 64 * slider->curval / slider->maxval;
					break;
				case mi_resolution:
					R_SetViewportSize(slider->curval);
					break;
				case mi_detailmode:
					R_SetDetailMode(slider->curval - 1);
					break;
				default:
					break;

				}

				sound = sfx_stnmov;
			}
		}

		movecount = 0;		/* repeat move */

		if (++movecount == 1)
		{
			if (buttons & BT_DOWN)
			{
				int oldpos = cursorpos;
				cursorpos++;
				if (cursorpos == menuscr->numitems)
					cursorpos = 0;
				if (cursorpos != oldpos)
					sound = sfx_pistol;
			}
		
			if (buttons & BT_UP)
			{
				int oldpos = cursorpos;
				cursorpos--;
				if (cursorpos == -1)
					cursorpos = menuscr->numitems-1;
				if (cursorpos != oldpos)
					sound = sfx_pistol;
			}

			if (screenpos == ms_controls)
			{
				int oldcontroltype = controltype;
				int oldalwaysrun = alwaysrun;
				int oldstrafebtns = strafebtns;

				if (buttons & BT_RIGHT)
				{
					switch (itemno) {
					case mi_controltype:
						if (++controltype == NUMCONTROLOPTIONS)
							controltype = (NUMCONTROLOPTIONS - 1);
						break;
					case mi_alwaysrun:
						if (++alwaysrun > 1)
							alwaysrun = 1;
						break;
					case mi_strafebtns:
						if (++strafebtns > 3)
							strafebtns = 3;
						break;

					}
				}
				if (buttons & BT_LEFT)
				{
					switch (itemno) {
					case mi_controltype:
						if (--controltype == -1)
							controltype = 0;
						break;
					case mi_alwaysrun:
						if (--alwaysrun < 0)
							alwaysrun = 0;
						break;
					case mi_strafebtns:
						if (--strafebtns < 0)
							strafebtns = 0;
						break;
					}
				}

				if (oldcontroltype != controltype ||
					oldalwaysrun != alwaysrun ||
					oldstrafebtns != strafebtns)
					sound = sfx_stnmov;
			}

			if (screenpos == ms_audio)
			{
				int oldmusictype = o_musictype;

				if (buttons & BT_RIGHT)
				{
					switch (itemno) {
					case mi_music:
						if (++o_musictype > mustype_cd)
							o_musictype = mustype_cd;
						if (!S_CDAvailable() && o_musictype == mustype_cd)
							o_musictype = mustype_fm;
						break;
					}
				}

				if (buttons & BT_LEFT)
				{
					switch (itemno) {
					case mi_music:
						if (--o_musictype < mustype_none)
							o_musictype = mustype_none;
						break;
					}
				}

				if (oldmusictype != o_musictype)
				{
					S_SetMusicType(o_musictype);
					sound = sfx_stnmov;
				}
			}
		}

		if (sound != sfx_noway)
			S_StartSound(NULL, sound);
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
	if (screenpos == ms_game)
	{
		M_Drawer();
		return;
	}

/* Erase old and Draw new cursor frame */
	//EraseBlock(56, 40, o_cursor1->width, 200);
	if(cursorframe)
		DrawJagobjLump(o_cursor1, CURSORX, items[cursorpos].y - 2, NULL, NULL);
	else
		DrawJagobjLump(o_cursor2, CURSORX, items[cursorpos].y - 2, NULL, NULL);

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
		const char* strabtnstr = "OFF";

		switch (strafebtns) {
		case 1:
			strabtnstr = "YZ";
			break;
		case 2:
			strabtnstr = "ZC";
			break;
		case 3:
			strabtnstr = "XZ";
			break;
		}

		print(items[0].x + 10, items[0].y + 20, "A");
		print(items[0].x + 10, items[0].y + 40, "B");
		print(items[0].x + 10, items[0].y + 60, "C");

		O_DrawControl();

		print(menuitem[mi_alwaysrun].x + 150, menuitem[mi_alwaysrun].y, alwaysrun ? "ON" : "OFF");
		print(menuitem[mi_strafebtns].x + 150, menuitem[mi_strafebtns].y, strabtnstr);
	}

	if (screenpos == ms_audio)
	{
		switch (o_musictype) {
		case mustype_none:
			print(menuitem[mi_music].x, menuitem[mi_music].y + 20, "off");
			break;
		case mustype_fm:
			print(menuitem[mi_music].x, menuitem[mi_music].y + 20, "fm synth");
			break;
		case mustype_cd:
			print(menuitem[mi_music].x, menuitem[mi_music].y + 20, "cd");
			break;
		}
	}

	if (screenpos == ms_video)
	{
		char tmp[32];
		D_snprintf(tmp, sizeof(tmp), "%dx%d", viewportWidth, viewportHeight);
		I_Print8(menuitem[mi_resolution].x + 114, (unsigned)menuitem[mi_resolution].y/8 + 3, tmp);
	}

#ifndef MARS
	UpdateBuffer();
#endif
}

