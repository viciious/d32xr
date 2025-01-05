/* o_main.c -- options menu */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"
#ifdef MARS
#include "mars.h"
#endif

#define MOVEWAIT		(I_IsPAL() ? TICVBLS*5 : TICVBLS*6)
#define STARTY		38
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
	mi_help,
	mi_quit,

	mi_soundvol,
	mi_music,
	mi_musicvol,
	mi_spcmpack,
#ifndef DISABLE_DMA_SOUND
	mi_sfxdriver,
#endif

	mi_resolution,
	mi_anamorphic,
	mi_detailmode,
	mi_lowres,

	mi_controltype,
	mi_yabcdpad,
	mi_alwaysrun,
	mi_strafebtns,

	mi_quityes,
	mi_quitno,

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
	char 	name[16];
} menuitem_t;

static menuitem_t *menuitem;

static VINT	cursorframe;
static VINT cursordelay;
static VINT	movecount;

static VINT	uchar;

static VINT	o_cursor1, o_cursor2;
static VINT	o_slider, o_sliderml, o_slidermm, o_slidermr;

static VINT m_help;

VINT	o_musictype, o_sfxdriver;

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
	ms_help,
	ms_quit,

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
	int cd_avail;

/* cache all needed graphics */
	o_cursor1 = W_CheckNumForName("M_SKULL1");
	o_cursor2 = W_CheckNumForName("M_SKULL2");
	o_slider = W_CheckNumForName("O_SLIDER");
	o_sliderml = W_CheckNumForName("O_STRAML");
	o_slidermm = W_CheckNumForName("O_STRAMM");
	o_slidermr = W_CheckNumForName("O_STRAMR");

	o_musictype = musictype;
	o_sfxdriver = sfxdriver;

	uchar = W_CheckNumForName("CHAR_065");

	m_help = W_CheckNumForName("M_HELP");

/*	initialize variables */

	menuitem = Z_Malloc(sizeof(*menuitem)*NUMMENUITEMS, PU_STATIC);

	cursorframe = -1;
	cursordelay = MOVEWAIT;
	cursorpos = 0;
	screenpos = ms_none;

	D_memset(menuitem, 0, sizeof(*menuitem)*NUMMENUITEMS);
	D_memset(sliders, 0, sizeof(sliders));

	D_memcpy(menuitem[mi_game].name, "Game", 5);
	menuitem[mi_game].x = ITEMX;
	menuitem[mi_game].y = STARTY;
	menuitem[mi_game].screen = ms_game;

	D_memcpy(menuitem[mi_audio].name, "Audio", 6);
	menuitem[mi_audio].x = ITEMX;
	menuitem[mi_audio].y = STARTY+ITEMSPACE;
	menuitem[mi_audio].screen = ms_audio;

	D_memcpy(menuitem[mi_video].name, "Video", 6);
	menuitem[mi_video].x = ITEMX;
	menuitem[mi_video].y = STARTY+ITEMSPACE*2;
	menuitem[mi_video].screen = ms_video;

	D_memcpy(menuitem[mi_controls].name, "Controls", 9);
	menuitem[mi_controls].x = ITEMX;
	menuitem[mi_controls].y = STARTY+ITEMSPACE*3;
	menuitem[mi_controls].screen = ms_controls;

	D_memcpy(menuitem[mi_help].name, "Help", 4);
	menuitem[mi_help].x = ITEMX;
	menuitem[mi_help].y = STARTY+ITEMSPACE*4;
	menuitem[mi_help].screen = ms_help;

	D_memcpy(menuitem[mi_quit].name, "Quit", 4);
	menuitem[mi_quit].x = ITEMX;
	menuitem[mi_quit].y = STARTY+ITEMSPACE*5;
	menuitem[mi_quit].screen = ms_quit;

	D_memcpy(menuitem[mi_soundvol].name, "Sfx volume", 11);
	menuitem[mi_soundvol].x = ITEMX;
	menuitem[mi_soundvol].y = STARTY;
	menuitem[mi_soundvol].slider = si_sfxvolume+1;
	sliders[si_sfxvolume].maxval = 4;
	sliders[si_sfxvolume].curval = 4*sfxvolume/64;

	D_memcpy(menuitem[mi_music].name, "Music", 6);
	menuitem[mi_music].x = ITEMX;
	menuitem[mi_music].y = STARTY+ITEMSPACE*2;

	D_memcpy(menuitem[mi_musicvol].name, "CD volume", 10);
	menuitem[mi_musicvol].x = ITEMX;
	menuitem[mi_musicvol].y = STARTY + ITEMSPACE * 3;
	menuitem[mi_musicvol].slider = si_musvolume+1;
	sliders[si_musvolume].maxval = 8;
	sliders[si_musvolume].curval = 8*musicvolume/64;

	D_memcpy(menuitem[mi_spcmpack].name, "SPCM", 4);
	menuitem[mi_spcmpack].x = ITEMX;
	menuitem[mi_spcmpack].y = STARTY+ITEMSPACE*5;

#ifndef DISABLE_DMA_SOUND
	D_memcpy(menuitem[mi_sfxdriver].name, "SFX driver", 11);
	menuitem[mi_sfxdriver].x = ITEMX;
	menuitem[mi_sfxdriver].y = STARTY+ITEMSPACE*6;
#endif

	D_memcpy(menuitem[mi_resolution].name, "Resolution", 11);
	menuitem[mi_resolution].x = ITEMX;
	menuitem[mi_resolution].y = STARTY;
	menuitem[mi_resolution].slider = si_resolution+1;
	sliders[si_resolution].maxval = numViewports - 1;
	sliders[si_resolution].curval = viewportNum;

	D_memcpy(menuitem[mi_anamorphic].name, "Widescreen", 11);
	menuitem[mi_anamorphic].x = ITEMX;
	menuitem[mi_anamorphic].y = STARTY + ITEMSPACE * 2;

	D_memcpy(menuitem[mi_detailmode].name, "Flats", 6);
	menuitem[mi_detailmode].x = ITEMX;
	menuitem[mi_detailmode].y = STARTY + ITEMSPACE * 3;

	D_memcpy(menuitem[mi_lowres].name, "Low detail", 11);
	menuitem[mi_lowres].x = ITEMX;
	menuitem[mi_lowres].y = STARTY + ITEMSPACE * 4;

	D_memcpy(menuitem[mi_controltype].name, "Gamepad", 8);
	menuitem[mi_controltype].x = ITEMX;
	menuitem[mi_controltype].y = STARTY;

	D_memcpy(menuitem[mi_yabcdpad].name, "YABC D-pad", 11);
	menuitem[mi_yabcdpad].x = ITEMX;
	menuitem[mi_yabcdpad].y = STARTY+ITEMSPACE*4;

	D_memcpy(menuitem[mi_alwaysrun].name, "Always run", 11);
	menuitem[mi_alwaysrun].x = ITEMX;
	menuitem[mi_alwaysrun].y = STARTY+ITEMSPACE*5;

	D_memcpy(menuitem[mi_strafebtns].name, "LR Strafe", 10);
	menuitem[mi_strafebtns].x = ITEMX;
	menuitem[mi_strafebtns].y = STARTY+ITEMSPACE*6;


	D_memcpy(menuitem[mi_quityes].name, "Yes", 4);
	menuitem[mi_quityes].x = ITEMX;
	menuitem[mi_quityes].y = STARTY;

	D_memcpy(menuitem[mi_quitno].name, "No", 3);
	menuitem[mi_quitno].x = ITEMX;
	menuitem[mi_quitno].y = STARTY+ITEMSPACE*1;

	D_memcpy(menuscreen[ms_main].name, "Options", 8);
	menuscreen[ms_main].firstitem = mi_game;
	menuscreen[ms_main].numitems = mi_quit - mi_game + 1;

	D_memcpy(menuscreen[ms_audio].name, "Audio", 6);
	menuscreen[ms_audio].firstitem = mi_soundvol;
	menuscreen[ms_audio].numitems = mi_music - mi_soundvol + 1;

	cd_avail = S_CDAvailable();
	if (cd_avail) /* CDA or MD+ */
	{
		menuscreen[ms_audio].numitems++;
		menuscreen[ms_audio].numitems++; // SPCM
#ifndef DISABLE_DMA_SOUND
		if (cd_avail & 0x1) /* CD, not MD+ */
			menuscreen[ms_audio].numitems++;
#endif
	}

	D_memcpy(menuscreen[ms_video].name, "Video", 6);
	menuscreen[ms_video].firstitem = mi_resolution;
	menuscreen[ms_video].numitems = mi_lowres - mi_resolution + 1;

	D_memcpy(menuscreen[ms_controls].name, "Controls", 9);
	menuscreen[ms_controls].firstitem = mi_controltype;
	menuscreen[ms_controls].numitems = mi_strafebtns - mi_controltype + 1;

	D_memcpy(menuscreen[ms_help].name, "Help", 5);
	menuscreen[ms_help].firstitem = 0;
	menuscreen[ms_help].numitems = 0;

	D_memcpy(menuscreen[ms_quit].name, "Quit?", 6);
	menuscreen[ms_quit].firstitem = mi_quityes;
	menuscreen[ms_quit].numitems = mi_quitno - mi_quityes + 1;
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
	int playernum = player - players;
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

	buttons = player->ticbuttons & MENU_BTNMASK;
	oldbuttons = player->oldticbuttons & MENU_BTNMASK;
	
	if ( ( (buttons & BT_OPTION) && !(oldbuttons & BT_OPTION) )
#ifdef MARS
		|| ( (buttons & BT_START) && !(oldbuttons & BT_START) && !(buttons & BT_MODE) )
#endif
		)
	{
		player->automapflags ^= AF_OPTIONSACTIVE;

		if (playernum == curplayer)
		{
			if (screenpos == ms_game)
				M_Stop();
			if (netgame == gt_single)
				gamepaused ^= 1;
			o_musictype = musictype;
			o_sfxdriver = sfxdriver;
			movecount = 0;
			cursorpos = 0;
			screenpos = ms_main;
			S_StartSound(NULL, sfx_swtchn);
			if (player->automapflags & AF_OPTIONSACTIVE)
#ifndef MARS
				DoubleBufferSetup();
#else
				;
#endif
			else
				WriteEEProm ();		/* save new settings */
		}
	}

	if (!(player->automapflags & AF_OPTIONSACTIVE))
		return;

/* clear buttons so player isn't moving aroung */
	player->ticbuttons &= (BT_OPTION|BT_START);	/* leave option status alone */

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
			S_StartSound(NULL, sfx_swtchn);
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
	if (gametic != prevgametic && (gametic&3) == 0)
	{
		cursorframe ^= 1;
	}

	buttons = ticrealbuttons & MENU_BTNMASK;
	oldbuttons = oldticrealbuttons & MENU_BTNMASK;

	if (buttons & (BT_A | BT_LMBTN) && !(oldbuttons & (BT_A | BT_LMBTN)))
	{
		int itemno = menuscr->firstitem + cursorpos;

		if (itemno == mi_quityes)
		{
			clearscreen = 2;

			S_StartSound(NULL, sfx_slop);

			// let the exit sound play
			int ticcount = I_GetTime();
			int lastticcount = ticcount;
			int ticwait = 150;
			do {
				ticcount = I_GetTime();
				S_UpdateSounds();
			} while (ticcount - lastticcount < ticwait);

			S_StopSong();

			gameaction = ga_quit;
			return;
		}

		if (itemno == mi_quitno)
		{
			goto goback;
		}

		if (menuscr->numitems > 0 && menuitem[itemno].screen != ms_none)
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
goback:
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
				default:
					break;

				}

				sound = sfx_stnmov;
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
						cursorpos = 0;
				}

				if (buttons & BT_UP)
				{
					if (--cursorpos == -1)
						cursorpos = menuscr->numitems-1;
				}
			}

			if (screenpos == ms_controls)
			{
				int oldcontroltype = controltype;
				int oldalwaysrun = alwaysrun;
				int oldstrafebtns = strafebtns;
				int oldyabcdpad = yabcdpad;

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
					case mi_yabcdpad:
						if (++yabcdpad > 1)
							yabcdpad = 1;
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
					case mi_yabcdpad:
						if (--yabcdpad < 0)
							yabcdpad = 0;
						break;
					}
				}

				if (oldcontroltype != controltype ||
					oldalwaysrun != alwaysrun ||
					oldstrafebtns != strafebtns ||
					oldyabcdpad != yabcdpad)
					sound = sfx_stnmov;
			}

			if (screenpos == ms_audio)
			{
				int i;
				int oldmusictype = o_musictype;
				int oldsfxdriver = o_sfxdriver;
				int oldspcmpack, curpack, numpacks;

				curpack = -1;
				numpacks = 0;
				for (i = 0; i < MAX_SPCM_PACKS; i++) {
					if (!gameinfo.spcmDirList[i][0]) {
						numpacks = i;
						break;
					}
					if (!D_strcasecmp(gameinfo.spcmDirList[i], spcmDir)) {
						curpack = i;
					}
				}
				if (i == MAX_SPCM_PACKS)
					numpacks = MAX_SPCM_PACKS;
				oldspcmpack = curpack;

				if (buttons & BT_RIGHT)
				{
					switch (itemno) {
					case mi_music:
						if (++o_musictype > mustype_spcm)
							o_musictype = mustype_spcm;
						if (o_musictype >= mustype_cd && !S_CDAvailable())
							o_musictype = mustype_fm;
						break;
					case mi_spcmpack:
						if (++curpack >= numpacks)
							curpack = numpacks-1;
						break;
#ifndef DISABLE_DMA_SOUND
					case mi_sfxdriver:
						if (++o_sfxdriver > sfxdriver_pwm)
							o_sfxdriver = sfxdriver_pwm;
						break;
#endif
					}
				}

				if (buttons & BT_LEFT)
				{
					switch (itemno) {
					case mi_music:
						if (--o_musictype < mustype_none)
							o_musictype = mustype_none;
						break;
					case mi_spcmpack:
						if (--curpack < 0)
							curpack = 0;
						break;
#ifndef DISABLE_DMA_SOUND
					case mi_sfxdriver:
						if (--o_sfxdriver < sfxdriver_auto)
							o_sfxdriver = sfxdriver_auto;
						break;
#endif
					}
				}

				if (oldmusictype != o_musictype)
				{
					S_SetMusicType(o_musictype);
					sound = sfx_stnmov;
				}

				if (oldsfxdriver != o_sfxdriver)
				{
					S_SetSoundDriver(o_sfxdriver);
				}

				if (oldspcmpack != curpack && curpack != -1)
				{
					S_SetSPCMDir(gameinfo.spcmDirList[curpack]);
					if (musictype == mustype_spcm)
						S_SetMusicType(mustype_spcmhack); // force refresh of the current dir
					clearscreen = 2;
				}
			}

			if (screenpos == ms_video)
			{
				int oldanamorphicview = anamorphicview;
				int olddetailmode = detailmode;

				if (buttons & BT_RIGHT)
				{
					switch (itemno) {
					case mi_anamorphic:
						if (++anamorphicview > 1)
							anamorphicview = 1;
						break;
					case mi_detailmode:
						if (++detailmode > detmode_normal)
							detailmode = detmode_normal;
						break;
					case mi_lowres:
						if (!lowres)
						{
							lowres = true;
							R_SetViewportSize(viewportNum);
						}
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
					case mi_detailmode:
						if (--detailmode < detmode_potato)
							detailmode = detmode_potato;
						break;
					case mi_lowres:
						if (lowres)
						{
							lowres = false;
							R_SetViewportSize(viewportNum);
						}
						break;
					}
				}

				if (olddetailmode != detailmode)
				{
					R_SetDrawFuncs();
					sound = sfx_stnmov;
				}
				if (oldanamorphicview != anamorphicview)
				{
					R_SetViewportSize(viewportNum);
					sound = sfx_stnmov;
				}
			}

			newcursor = cursorpos != oldcursorpos;
			if (newcursor)
				sound = sfx_pistol;
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
		if(cursorframe)
			DrawJagobjLump(o_cursor1, CURSORX, items[cursorpos].y - 2, NULL, NULL);
		else
			DrawJagobjLump(o_cursor2, CURSORX, items[cursorpos].y - 2, NULL, NULL);
	}

/* Draw menu */

	y = 10;
	print(104, y, menuscr->name);
	
	for (i = 0; i < menuscr->numitems; i++)
	{
		y = items[i].y;
		print(items[i].x, y, items[i].name);

		if(items[i].slider)
		{
			int j;
			slider_t* slider = &sliders[items[i].slider-1];

			DrawJagobjLump(o_sliderml, items[i].x + 2, items[i].y + ITEMSPACE, NULL, NULL);
			for (j = 0; j < SLIDEWIDTH; j+=8)
				DrawJagobjLump(o_slidermm, items[i].x + 2 + 6 + j, items[i].y + ITEMSPACE, NULL, NULL);
			DrawJagobjLump(o_slidermr, items[i].x + 2 + 6 + j, items[i].y + ITEMSPACE, NULL, NULL);

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

		print(menuitem[mi_alwaysrun].x + 150, menuitem[mi_yabcdpad].y, yabcdpad ? "ON" : "OFF");
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
			print(menuitem[mi_music].x + 85, menuitem[mi_music].y, "fm");
			break;
		case mustype_cd:
			print(menuitem[mi_music].x + 85, menuitem[mi_music].y, "cda");
			break;
		case mustype_spcm:
			print(menuitem[mi_music].x + 85, menuitem[mi_music].y, "spcm");
			break;
		}

		if (spcmDir[0] != '\0') {
			print(menuitem[mi_spcmpack].x + 85, menuitem[mi_spcmpack].y, spcmDir);
		} else {
			print(menuitem[mi_spcmpack].x + 85, menuitem[mi_spcmpack].y, "NONE");
		}

#ifndef DISABLE_DMA_SOUND
		if (menuscreen[ms_audio].numitems > 3)
		{
			switch (o_sfxdriver) {
			case sfxdriver_auto:
				print(menuitem[mi_sfxdriver].x + 150, menuitem[mi_sfxdriver].y, "auto");
				break;
			case sfxdriver_mcd:
				print(menuitem[mi_sfxdriver].x + 150, menuitem[mi_sfxdriver].y, "mcd");
				break;
			case sfxdriver_pwm:
				print(menuitem[mi_sfxdriver].x + 150, menuitem[mi_sfxdriver].y, "pwm");
				break;
			}
		}
#endif
	}

	if (screenpos == ms_video)
	{
		char tmp[32];
		D_snprintf(tmp, sizeof(tmp), "%dx%d", viewportWidth, viewportHeight);
		I_Print8(menuitem[mi_resolution].x + 114, (unsigned)menuitem[mi_resolution].y/8 + 3, tmp);

		switch (anamorphicview) {
		case 0:
			print(menuitem[mi_anamorphic].x + 160, menuitem[mi_anamorphic].y, "off");
			break;
		case 1:
			print(menuitem[mi_anamorphic].x + 150, menuitem[mi_anamorphic].y, "on");
			break;
		}

		switch (detailmode) {
		case detmode_potato:
			print(menuitem[mi_detailmode].x + 90, menuitem[mi_detailmode].y, "simple");
			break;
		case detmode_lowres:
			print(menuitem[mi_detailmode].x + 90, menuitem[mi_detailmode].y, "low-res");
			break;
		default:
			print(menuitem[mi_detailmode].x + 90, menuitem[mi_detailmode].y, "high-res");
			break;
		}

		switch (lowres) {
		case true:
			print(menuitem[mi_lowres].x + 160, menuitem[mi_lowres].y, "on");
			break;
		default:
			print(menuitem[mi_lowres].x + 160, menuitem[mi_lowres].y, "off");
			break;
		}
	}

	if (screenpos == ms_help)
	{
		int x, x2, x3, l, l2;

		x = 10;
		y = CURSORY(0)-10;
		l = y/8;
		l += 1;

		x2 = 88;
		l2 = l;
		x3 = x2+7*8+4;
		I_Print8(x, l, "Prev weap");
		I_Print8(x2, l, "START+A");
		l++;
		I_Print8(x, l, "Next weap");
		I_Print8(x2, l, "START+B");
		l++;
		I_Print8(x, l, "Automap");
		I_Print8(x2, l, "START+C");
		if (strafebtns)
		{
			switch (strafebtns)
			{
			default:
			case 1:
			case 2:
				I_Print8(x3, l2, "or");
				I_Print8(x3+16+4, l2, "X");
				break;
			case 3:
				I_Print8(x3, l2, "or");
				I_Print8(x3+16+4, l2, "Y");
				break;
			}
		}
		else
		{
			I_Print8(x3, l2, "or");
			I_Print8(x3+16+4, l2, "X");
			I_Print8(x3, l2+1, "or");
			I_Print8(x3+16+4, l2+1, "Y");
			I_Print8(x3, l2+2, "or");
			I_Print8(x3+16+4, l2+2, "Z");
		}
		l++;

		l++;

		x2 = 138;
		I_Print8(x, l++, "^E5Hold MODE and press a");
		I_Print8(x, l++, "^E5button to switch to:");
		I_Print8(x, l, "Fists/Chainsaw");
		I_Print8(x2, l++, "START");
		I_Print8(x, l, "Pistol");
		I_Print8(x2, l++, "A");
		I_Print8(x, l, "Shotgun");
		I_Print8(x2, l++, "B");
		I_Print8(x, l, "Chaingun");
		I_Print8(x2, l++, "C");
		I_Print8(x, l, "Rocket launcher");
		I_Print8(x2, l++, "X");
		I_Print8(x, l, "Plasmagun");
		I_Print8(x2, l++, "Y");
		I_Print8(x, l, "BFG");
		I_Print8(x2, l++, "Z");

		l++;

		x2 = 62;
		I_Print8(x, l++, "^E5Automap");
		I_Print8(x, l, "Scale");
		I_Print8(x2, l++, "Hold B+UP/DOWN");
		I_Print8(x, l, "Lock");
		I_Print8(x2, l++, "Hold C");

		if (m_help >= 0)
		{
			x = 182;
			l = y/8;
			I_Print8(x, l, "^E5Scan the QR code");
			I_Print8(x, l+1, "^E5for more info");
			DrawJagobjLump(m_help, x, y+16, NULL, NULL);
		}
	}
}

