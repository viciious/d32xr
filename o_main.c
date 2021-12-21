/* o_main.c -- options menu */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"
#ifdef MARS
#include "mars.h"
#endif

#define MOVEWAIT		TICVBLS*6
#define STARTY		48
#define CURSORX		(80)
#define CURSORWIDTH	24
#define ITEMX		(CURSORX+CURSORWIDTH)
#define ITEMSPACE	20
#define CURSORY(y)	(STARTY+ITEMSPACE*(y))
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

typedef enum
{
	si_resolution,
	si_sfxvolume,
	si_lod,

	NUMMENUSLIDERS
} sliderid_t;

VINT cursorpos;

typedef struct
{
	char	curval;
	char	maxval;
} slider_t;

static slider_t sliders[NUMMENUSLIDERS];

typedef struct
{
	VINT	x;
	VINT	y;
	uint8_t	slider;
	uint8_t screen;
	char 	name[16];
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
static VINT screenpos;

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
	D_memset(sliders, 0, sizeof(sliders));

	D_memcpy(menuitem[mi_game].name, "Game", 5);
	menuitem[mi_game].x = ITEMX;
	menuitem[mi_game].y = STARTY;
	menuitem[mi_game].screen = ms_game;

	D_memcpy(menuitem[mi_audio].name, "Audio", 7);
	menuitem[mi_audio].x = ITEMX;
	menuitem[mi_audio].y = STARTY+ITEMSPACE;
	menuitem[mi_audio].screen = ms_audio;

	D_memcpy(menuitem[mi_video].name, "Video", 7);
	menuitem[mi_video].x = ITEMX;
	menuitem[mi_video].y = STARTY+ITEMSPACE*2;
	menuitem[mi_video].slider = 0;
	menuitem[mi_video].screen = ms_video;

	D_memcpy(menuitem[mi_controls].name, "Controls", 9);
	menuitem[mi_controls].x = ITEMX;
	menuitem[mi_controls].y = STARTY+ITEMSPACE*3;
	menuitem[mi_controls].screen = ms_controls;


	D_memcpy(menuitem[mi_soundvol].name, "Sfx volume", 11);
	menuitem[mi_soundvol].x = ITEMX;
	menuitem[mi_soundvol].y = STARTY;
	menuitem[mi_soundvol].slider = si_resolution+1;
 	sliders[si_resolution].maxval = 4;
	sliders[si_resolution].curval = 4*sfxvolume/64;

	D_memcpy(menuitem[mi_music].name, "Music", 6);
	menuitem[mi_music].x = ITEMX;
	menuitem[mi_music].y = STARTY+ITEMSPACE*2;


	D_memcpy(menuitem[mi_resolution].name, "Resolution", 11);
	menuitem[mi_resolution].x = ITEMX;
	menuitem[mi_resolution].y = STARTY;
	menuitem[mi_resolution].slider = si_resolution+1;
	sliders[si_resolution].maxval = numViewports - 1;
	sliders[si_resolution].curval = viewportNum;

	D_memcpy(menuitem[mi_detailmode].name, "Level of detail", 16);
	menuitem[mi_detailmode].x = ITEMX;
	menuitem[mi_detailmode].y = STARTY+ITEMSPACE*2;
	menuitem[mi_detailmode].slider = si_lod+1;
	sliders[si_lod].maxval = MAXDETAILMODES;
	sliders[si_lod].curval = detailmode + 1;


	D_memcpy(menuitem[mi_controltype].name, "Gamepad", 8);
	menuitem[mi_controltype].x = ITEMX;
	menuitem[mi_controltype].y = STARTY;

	D_memcpy(menuitem[mi_alwaysrun].name, "Always run", 11);
	menuitem[mi_alwaysrun].x = ITEMX;
	menuitem[mi_alwaysrun].y = STARTY+ITEMSPACE*4;

	D_memcpy(menuitem[mi_strafebtns].name, "LR Strafe", 11);
	menuitem[mi_strafebtns].x = ITEMX;
	menuitem[mi_strafebtns].y = STARTY+ITEMSPACE*5;


	D_memcpy(menuscreen[ms_main].name, "Options", 8);
	menuscreen[ms_main].firstitem = mi_game;
	menuscreen[ms_main].numitems = mi_controls - mi_game + 1;

	D_memcpy(menuscreen[ms_audio].name, "Audio", 7);
	menuscreen[ms_audio].firstitem = mi_soundvol;
	menuscreen[ms_audio].numitems = mi_music - mi_soundvol + 1;

	D_memcpy(menuscreen[ms_video].name, "Video", 7);
	menuscreen[ms_video].firstitem = mi_resolution;
	menuscreen[ms_video].numitems = mi_detailmode - mi_resolution + 1;

	D_memcpy(menuscreen[ms_controls].name, "Controls", 9);
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
	boolean newcursor = false;

	if (splitscreen && playernum != consoleplayer)
		return;

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
		if (playernum == consoleplayer)
		{
			if (screenpos == ms_game)
				M_Stop();
			gamepaused ^= 1;
			movecount = 0;
			cursorpos = 0;
			screenpos = ms_main;
			S_StartSound(NULL, sfx_swtchn);
		}

		player->automapflags ^= AF_OPTIONSACTIVE;
		if (player->automapflags & AF_OPTIONSACTIVE)
#ifndef MARS
			DoubleBufferSetup();
#else
			;
#endif
		else
			WriteEEProm ();		/* save new settings */
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

	if (buttons & (BT_B | BT_RMBTN) && !(oldbuttons & (BT_B | BT_RMBTN)))
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
		slider_t*slider = menuitem[itemno].slider ? &sliders[menuitem[itemno].slider-1] : NULL;
		int oldcursorpos = cursorpos;

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
				if (++cursorpos == menuscr->numitems)
					cursorpos = 0;
			}
		
			if (buttons & BT_UP)
			{
				if (--cursorpos == -1)
					cursorpos = menuscr->numitems-1;
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

			newcursor = cursorpos != oldcursorpos;
			if (newcursor)
				sound = sfx_pistol;
		}

		if (sound != sfx_noway)
		{
			S_StartSound(NULL, sound);
			clearscreen = 2;
		}
	}

	if (newcursor)
		clearscreen = 2;
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
			slider_t* slider = &sliders[items[i].slider-1];
			DrawJagobjLump(o_slidertrack, items[i].x + 2, items[i].y + ITEMSPACE, NULL, NULL);
			offset = (slider->curval * SLIDEWIDTH) / slider->maxval;
			DrawJagobjLump(o_slider, items[i].x + 7 + offset, items[i].y + ITEMSPACE, NULL, NULL);
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

		print(items[0].x + 10, items[0].y + ITEMSPACE, "A");
		print(items[0].x + 10, items[0].y + ITEMSPACE*2, "B");
		print(items[0].x + 10, items[0].y + ITEMSPACE*3, "C");

		O_DrawControl();

		print(menuitem[mi_alwaysrun].x + 150, menuitem[mi_alwaysrun].y, alwaysrun ? "ON" : "OFF");
		print(menuitem[mi_strafebtns].x + 150, menuitem[mi_strafebtns].y, strabtnstr);
	}

	if (screenpos == ms_audio)
	{
		switch (o_musictype) {
		case mustype_none:
			print(menuitem[mi_music].x + 85, menuitem[mi_music].y, "off");
			break;
		case mustype_fm:
			print(menuitem[mi_music].x + 85, menuitem[mi_music].y, "fm synth");
			break;
		case mustype_cd:
			print(menuitem[mi_music].x + 85, menuitem[mi_music].y, "cd");
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

