/* o_main.c -- options menu */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"
#include "v_font.h"
#ifdef MARS
#include "mars.h"
#endif

#define MOVEWAIT		(I_IsPAL() ? TICVBLS*5 : TICVBLS*6)
#define STARTY		48
#define CURSORX		(112)
#define CURSORWIDTH	24
#define ITEMX		(CURSORX+CURSORWIDTH)
#define ITEMSPACE	20
#define CURSORY(y)	(STARTY+ITEMSPACE*(y))
#define SLIDEWIDTH 90

typedef enum
{
	mi_game, 
	mi_audio,
	mi_video,
	mi_help,

	mi_soundvol,
	mi_music,
	mi_musicvol,
//	mi_sfxdriver,

	mi_resolution,
	mi_anamorphic,

	mi_controltype,
	mi_strafebtns,

	NUMMENUITEMS
} menupos_t;

typedef enum
{
	si_resolution,
	si_sfxvolume,
	si_lod,
	si_musvolume,

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
	const char 	*name;
} menuitem_t;

static menuitem_t menuitem[NUMMENUITEMS];

static VINT	cursorframe;
static VINT cursordelay;
static VINT	movecount;

static VINT	uchar;

static VINT	o_cursor1;
static VINT	o_slider, o_slidertrack;

VINT	o_musictype, o_sfxdriver;

typedef struct
{
	VINT firstitem;
	VINT numitems;
	const char *name;
} menuscreen_t;

typedef enum
{
	ms_none,
	ms_main,
	ms_game,
	ms_audio,
	ms_video,
	ms_controls,
	ms_help,

	NUMMENUSCREENS
} screenpos_t;

static menuscreen_t menuscreen[NUMMENUSCREENS];
static VINT screenpos;

/*
===============
=
= O_Init
=
===============
*/

void O_Init (void)
{
	int cd_avail;

/* cache all needed graphics */
	o_cursor1 = W_CheckNumForName("M_CURSOR");
	o_slider = W_CheckNumForName("O_SLIDER");
	o_slidertrack = W_CheckNumForName("O_STRACK");

	o_musictype = musictype;
	o_sfxdriver = sfxdriver;

	uchar = W_CheckNumForName("STCFN065");

/*	initialize variables */

	cursorframe = -1;
	cursordelay = MOVEWAIT;
	cursorpos = 0;
	screenpos = ms_none;

	D_memset(menuitem, 0, sizeof(*menuitem)*NUMMENUITEMS);
	D_memset(sliders, 0, sizeof(sliders));

	menuitem[mi_game].name = "GAME";
	menuitem[mi_game].x = ITEMX;
	menuitem[mi_game].y = STARTY;
	menuitem[mi_game].screen = ms_game;

	menuitem[mi_audio].name = "AUDIO";
	menuitem[mi_audio].x = ITEMX;
	menuitem[mi_audio].y = STARTY+ITEMSPACE;
	menuitem[mi_audio].screen = ms_audio;

	menuitem[mi_video].name = "VIDEO";
	menuitem[mi_video].x = ITEMX;
	menuitem[mi_video].y = STARTY+ITEMSPACE*2;
	menuitem[mi_video].screen = ms_video;
/*
	menuitem[mi_controls].name = "Controls";
	menuitem[mi_controls].x = ITEMX;
	menuitem[mi_controls].y = STARTY+ITEMSPACE*3;
	menuitem[mi_controls].screen = ms_controls;
*/
	menuitem[mi_help].name = "HELP / ABOUT";
	menuitem[mi_help].x = ITEMX;
	menuitem[mi_help].y = STARTY+ITEMSPACE*3;
	menuitem[mi_help].screen = ms_help;

	menuitem[mi_soundvol].name = "Sfx volume";
	menuitem[mi_soundvol].x = ITEMX - 32;
	menuitem[mi_soundvol].y = STARTY;
	menuitem[mi_soundvol].slider = si_sfxvolume+1;
	sliders[si_sfxvolume].maxval = 4;
	sliders[si_sfxvolume].curval = 4*sfxvolume/64;

	menuitem[mi_music].name = "Music";
	menuitem[mi_music].x = ITEMX - 32;
	menuitem[mi_music].y = STARTY+ITEMSPACE*2;

	menuitem[mi_musicvol].name = "CDA volume";
	menuitem[mi_musicvol].x = ITEMX - 32;
	menuitem[mi_musicvol].y = STARTY + ITEMSPACE * 3;
	menuitem[mi_musicvol].slider = si_musvolume+1;
	sliders[si_musvolume].maxval = 8;
	sliders[si_musvolume].curval = 8*musicvolume/64;
/*
	menuitem[mi_sfxdriver].name = "SFX driver";
	menuitem[mi_sfxdriver].x = ITEMX;
	menuitem[mi_sfxdriver].y = STARTY+ITEMSPACE*5;
*/
	menuitem[mi_resolution].name = "Resolution";
	menuitem[mi_resolution].x = ITEMX;
	menuitem[mi_resolution].y = STARTY;
	menuitem[mi_resolution].slider = si_resolution+1;
	sliders[si_resolution].maxval = numViewports - 1;
	sliders[si_resolution].curval = viewportNum;

	menuitem[mi_anamorphic].name = "WIDESCREEN";
	menuitem[mi_anamorphic].x = ITEMX - 32;
	menuitem[mi_anamorphic].y = STARTY + ITEMSPACE * 2;


	menuitem[mi_controltype].name = "Gamepad";
	menuitem[mi_controltype].x = ITEMX;
	menuitem[mi_controltype].y = STARTY;

	menuitem[mi_strafebtns].name = "LR Strafe";
	menuitem[mi_strafebtns].x = ITEMX;
	menuitem[mi_strafebtns].y = STARTY+ITEMSPACE*5;


	menuscreen[ms_main].name = "OPTIONS";
	menuscreen[ms_main].firstitem = mi_game;
	menuscreen[ms_main].numitems = mi_help - mi_game + 1;

	menuscreen[ms_audio].name = "AUDIO";
	menuscreen[ms_audio].firstitem = mi_soundvol;
	menuscreen[ms_audio].numitems = mi_music - mi_soundvol + 1;

	cd_avail = S_CDAvailable();
	if (cd_avail) /* CDA or MD+ */
	{
//		menuscreen[ms_audio].numitems++;
		if (cd_avail & 0x1) /* CD, not MD+ */
			menuscreen[ms_audio].numitems++;
	}

	menuscreen[ms_video].name = "VIDEO";
	menuscreen[ms_video].firstitem = mi_anamorphic;
	menuscreen[ms_video].numitems = mi_anamorphic - mi_anamorphic + 1;

	menuscreen[ms_controls].name = "CONTROLS";
	menuscreen[ms_controls].firstitem = mi_controltype;
	menuscreen[ms_controls].numitems = mi_strafebtns - mi_controltype + 1;

	menuscreen[ms_help].name = "HELP / ABOUT";
	menuscreen[ms_help].firstitem = 0;
	menuscreen[ms_help].numitems = 0;
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
	int curplayer = consoleplayer;

	if (splitscreen && playernum != curplayer)
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

	buttons = ticbuttons[playernum] & MENU_BTNMASK;
	oldbuttons = oldticbuttons[playernum] & MENU_BTNMASK;
	
	if ((buttons & BT_START) && !(oldbuttons & BT_START) && !(buttons & BT_MODE))
	{
		optionsMenuOn = !optionsMenuOn;

		if (playernum == curplayer)
		{
			if (screenpos == ms_game)
				M_Stop();
			if (netgame == gt_single)
				gamepaused ^= 1;
			movecount = 0;
			cursorpos = 0;
			screenpos = ms_main;
			S_StartSound(NULL, sfx_None);
			if (optionsMenuOn)
#ifndef MARS
				DoubleBufferSetup();
#else
				;
#endif
			else
				WriteEEProm ();		/* save new settings */
		}
	}

	if (!optionsMenuOn)
		return;

/* clear buttons so player isn't moving aroung */
	ticbuttons[playernum] &= BT_START;	/* leave option status alone */

	if (playernum != curplayer)
		return;

	if (screenpos == ms_game)
	{
		int mtick = M_Ticker();
		if (mtick == ga_nothing)
			return;

		clearscreen = 2;

		switch (mtick)
		{
		case ga_completed:
			movecount = 0;
			cursorpos = 0;
			screenpos = ms_main;
			return;
		case ga_startnew:
			gameaction = ga_startnew;
		case ga_died:
			return;
		case ga_warped:
			gameaction = ga_warped;
			return;
		}
		return;
	}

/* animate skull */
	if ((gametic&3) == 0)
	{
		cursorframe ^= 1;
	}

	buttons = ticrealbuttons & MENU_BTNMASK;
	oldbuttons = oldticrealbuttons & MENU_BTNMASK;

	if (buttons & (BT_B | BT_LMBTN) && !(oldbuttons & (BT_B | BT_LMBTN)))
	{
		int itemno = menuscr->firstitem + cursorpos;
		if (menuscr->numitems > 0 && menuitem[itemno].screen != ms_none)
		{
			movecount = 0;
			cursorpos = 0;
			screenpos = menuitem[itemno].screen;
			clearscreen = 2;

			if (screenpos == ms_game)
				M_Start2(false);
			else
				S_StartSound(NULL, sfx_None);
			return;
		}
	}

	if (buttons & (BT_A | BT_RMBTN) && !(oldbuttons & (BT_A | BT_RMBTN)))
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
			S_StartSound(NULL, sfx_None);
			return;
		}
	}

	if (buttons == 0)
	{
		cursordelay = 0;
		return;
	}

	cursordelay -= vblsinframe;
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
				case mi_musicvol:
					musicvolume = 64 * slider->curval / slider->maxval;
					break;
				case mi_resolution:
					R_SetViewportSize(slider->curval);
					break;
				case mi_anamorphic:
					anamorphicview = slider->curval;
					R_SetViewportSize(viewportNum);
					break;
				default:
					break;

				}

				sound = sfx_None;
			}
		}

		movecount = 0;		/* repeat move */

		if (++movecount == 1)
		{
			if (menuscr->numitems > 0)
			{
				if (buttons & BT_DOWN)
				{
					if (++cursorpos == menuscr->numitems)
					{
						cursorpos = 0;
//						S_StartSound(NULL, sfx_s3k_5b);
					}
				}

				if (buttons & BT_UP)
				{
					if (--cursorpos == -1)
					{
						cursorpos = menuscr->numitems-1;
//						S_StartSound(NULL, sfx_s3k_5b);
					}
				}
			}

			if (screenpos == ms_audio)
			{
				int oldmusictype = o_musictype;
				int oldsfxdriver = o_sfxdriver;

				if (buttons & BT_RIGHT)
				{
					switch (itemno) {
					case mi_music:
						if (++o_musictype > mustype_cd)
							o_musictype = mustype_cd;
						if (o_musictype == mustype_cd && !S_CDAvailable())
							o_musictype = mustype_fm;
						break;
/*					case mi_sfxdriver:
						if (++o_sfxdriver > sfxdriver_pwm)
							o_sfxdriver = sfxdriver_pwm;
						break;*/
					}
				}

				if (buttons & BT_LEFT)
				{
					switch (itemno) {
					case mi_music:
						if (--o_musictype < mustype_none)
							o_musictype = mustype_none;
						break;
/*					case mi_sfxdriver:
						if (--o_sfxdriver < sfxdriver_auto)
							o_sfxdriver = sfxdriver_auto;
						break;*/
					}
				}

				if (oldmusictype != o_musictype)
				{
					S_SetMusicType(o_musictype);
					sound = sfx_None;
				}

				if (oldsfxdriver != o_sfxdriver)
				{
					S_SetSoundDriver(o_sfxdriver);
				}
			}

			if (screenpos == ms_video)
			{
				int oldanamorphicview = anamorphicview;

				if (buttons & BT_RIGHT)
				{
					switch (itemno) {
					case mi_anamorphic:
						if (++anamorphicview > 1)
							anamorphicview = 1;
						break;
					}
				}

				if (buttons & BT_LEFT)
				{
					switch (itemno) {
					case mi_anamorphic:
						if (--anamorphicview < 0)
							anamorphicview = 0;
						break;
					}
				}

				if (oldanamorphicview != anamorphicview)
				{
					R_SetViewportSize(viewportNum);
					sound = sfx_None;
				}
			}

			newcursor = cursorpos != oldcursorpos;
			if (newcursor)
				sound = sfx_None;
		}

		if (sound != sfx_None)
		{
			S_StartSound(NULL, sound);
			clearscreen = 2;
		}
	}

	if (newcursor)
		clearscreen = 2;
}

void O_DrawHelp (VINT yPos)
{
	V_DrawStringCenter(&menuFont, 160, yPos - 96, "TIP: Use gas pedal for easier control");

	V_DrawStringCenterWithColormap(&menuFont, 160, yPos - 32, "SONIC ROBO BLAST 32X", YELLOWTEXTCOLORMAP);
	V_DrawStringCenterWithColormap(&menuFont, 160, yPos - 20, "v0.1a DEMO", YELLOWTEXTCOLORMAP);

	V_DrawStringRight(&menuFont, 160-8, yPos, "JUMP ");
	V_DrawStringLeft(&menuFont, 160, yPos, "= B");
	V_DrawStringRight(&menuFont, 160-8, yPos + (12*1), "SPIN ");
	V_DrawStringLeft(&menuFont, 160, yPos + (12*1), "= A or C");
	V_DrawStringRight(&menuFont, 160-8, yPos + (12*2), "GAS PEDAL ");
	V_DrawStringLeft(&menuFont, 160, yPos + (12*2), "= Y");
	V_DrawStringRight(&menuFont, 160-8, yPos + (12*3), "MOVE CAMERA ");
	V_DrawStringLeft(&menuFont, 160, yPos + (12*3), "= X and Z");

	V_DrawStringCenterWithColormap(&menuFont, 160, yPos + (12*5), "INTENDED ONLY FOR NTSC SYSTEMS AND", YELLOWTEXTCOLORMAP);
	V_DrawStringCenterWithColormap(&menuFont, 160, yPos + (12*5) + 8, "PICODRIVE 2.04 & JGENESIS 0.10.0", YELLOWTEXTCOLORMAP);

	V_DrawStringCenter(&menuFont, 160, yPos + 80, "ssntails.srb2.org/srb32x");
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
	if (screenpos != ms_help)
	{
		VINT cursorX = CURSORX;

		if (screenpos == ms_audio)
			cursorX -= 32;
		else if (screenpos == ms_video)
			cursorX -= 32;

		DrawJagobjLump(o_cursor1, cursorX, items[cursorpos].y - 2, NULL, NULL);
	}

/* Draw menu */

	y = 10;
	V_DrawStringCenter(&menuFont, 160, y, menuscr->name);
	
	for (i = 0; i < menuscr->numitems; i++)
	{
		y = items[i].y;
		if (i == cursorpos)
			V_DrawStringLeftWithColormap(&menuFont, items[i].x, y, items[i].name, YELLOWTEXTCOLORMAP);
		else
			V_DrawStringLeft(&menuFont, items[i].x, y, items[i].name);

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
	
	if (screenpos == ms_audio)
	{
		switch (o_musictype) {
		case mustype_none:
			V_DrawStringLeft(&menuFont, menuitem[mi_music].x + 85, menuitem[mi_music].y, "off");
			break;
		case mustype_fm:
			V_DrawStringLeft(&menuFont, menuitem[mi_music].x + 85, menuitem[mi_music].y, "fm");
			break;
		case mustype_cd:
			V_DrawStringLeft(&menuFont, menuitem[mi_music].x + 85, menuitem[mi_music].y, "cd");
			break;
		}
/*
		if (menuscreen[ms_audio].numitems > 3)
		{
			switch (o_sfxdriver) {
			case sfxdriver_auto:
				V_DrawStringLeft(&menuFont, menuitem[mi_sfxdriver].x + 150, menuitem[mi_sfxdriver].y, "auto");
				break;
			case sfxdriver_mcd:
				V_DrawStringLeft(&menuFont, menuitem[mi_sfxdriver].x + 150, menuitem[mi_sfxdriver].y, "mcd");
				break;
			case sfxdriver_pwm:
				V_DrawStringLeft(&menuFont, menuitem[mi_sfxdriver].x + 150, menuitem[mi_sfxdriver].y, "pwm");
				break;
			}
		}*/
	}

	if (screenpos == ms_video)
	{
/*		char tmp[32];
		D_snprintf(tmp, sizeof(tmp), "%dx%d", viewportWidth, viewportHeight);
		I_Print8(menuitem[mi_resolution].x + 114, (unsigned)menuitem[mi_resolution].y/8 + 3, tmp);
*/
		switch (anamorphicview) {
		case 0:
			V_DrawStringLeft(&menuFont, menuitem[mi_anamorphic].x + 150, menuitem[mi_anamorphic].y, "off");
			break;
		case 1:
			V_DrawStringLeft(&menuFont, menuitem[mi_anamorphic].x + 150, menuitem[mi_anamorphic].y, "on");
			break;
		}
	}

	if (screenpos == ms_help)
		O_DrawHelp(80);
}

