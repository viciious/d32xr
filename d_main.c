/* D_main.c  */
 
#include "doomdef.h" 
#ifdef MARS
#include <string.h>
#endif

boolean		splitscreen = false;
VINT		controltype = 0;		/* determine settings for BT_* */
VINT		alwaysrun = 0;
VINT 		yabcdpad = 0;

int			gamevbls;		/* may not really be vbls in multiplayer */
int			vblsinframe;		/* range from ticrate to ticrate*2 */

VINT		ticsperframe = MINTICSPERFRAME;

int			maxlevel;			/* highest level selectable in menu (1-25) */
jagobj_t	*backgroundpic;

unsigned	*demo_p, *demobuffer;

boolean canwipe = false;

char 		cd_pwad_name[16];

int 		ticstart;

unsigned configuration[NUMCONTROLOPTIONS][3] =
{
	{BT_SPEED, BT_ATTACK, BT_USE},
	{BT_SPEED, BT_USE, BT_ATTACK},
	{BT_ATTACK, BT_SPEED, BT_USE},
	{BT_ATTACK, BT_USE, BT_SPEED},
	{BT_USE, BT_SPEED, BT_ATTACK},
	{BT_USE, BT_ATTACK, BT_SPEED}
};

/*============================================================================ */


#define WORDMASK	3

/*
====================
=
= D_memset
=
====================
*/

void D_memset (void *dest, int val, size_t count)
{
#ifdef MARS
	memset(dest, val, count);
#else
	byte	*p;
	int		*lp;

/* round up to nearest word */
	p = dest;
	while ((int)p & WORDMASK)
	{
		if (count-- == 0)
			return;
		*p++ = val;
	}
	
/* write 32 bytes at a time */
	lp = (int *)p;
	val = (val<<24) | (val<<16) | (val<<8) | val;
	while (count >= 32)
	{
		lp[0] = lp[1] = lp[2] = lp[3] = lp[4] = lp[5] = lp[6] = lp[7] = val;
		lp += 8;
		count -= 32;
	}
	
/* finish up */
	p = (byte *)lp;
	while (count > 0)
	{
		*p++ = val;
		count--;
	}
#endif
}

#ifdef MARS
void fast_memcpy(void *dest, const void *src, int count);
#endif

void D_memcpy (void *dest, const void *src, size_t count)
{
#ifdef MARS
	memcpy (dest, src, count);
#else
	byte	*d;
	const byte *s;

	if (dest == src)
		return;

#ifdef MARS
	if ( (((intptr_t)dest & 15) == ((intptr_t)src & 15)) && (count > 16)) {
		unsigned i;
		unsigned rem = ((intptr_t)dest & 15), wordbytes;

		d = (byte*)dest;
		s = (const byte*)src;
		for (i = 0; i < rem; i++)
			*d++ = *s++;

		wordbytes = (unsigned)(count - rem) >> 2;
		fast_memcpy(d, s, wordbytes);

		wordbytes <<= 2;
		d += wordbytes;
		s += wordbytes;

		for (i = rem + wordbytes; i < (unsigned)count; i++)
			*d++ = *s++;
		return;
	}
#endif

	d = (byte *)dest;
	s = (const byte *)src;
	while (count--)
		*d++ = *s++;
#endif
}


void D_strncpy (char *dest, const char *src, int maxcount)
{
	byte	*p1;
	const byte *p2;

	p1 = (byte *)dest;
	p2 = (const byte *)src;
	while (maxcount--)
		if (! (*p1++ = *p2++) )
			return;
}

int D_strncasecmp (const char *s1, const char *s2, int len)
{
	while (*s1 && *s2)
	{
		int c1 = *s1, c2 = *s2;

		if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
		if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';

		if (c1 != c2)
			return c1 - c2;

		s1++;
		s2++;

		if (!--len)
			return 0;
	}
	return *s1 - *s2;
}

// insertion sort
void D_isort(int* a, int len)
{
	int i, j;
	for (i = 1; i < len; i++)
	{
		int t = a[i];
		for (j = i - 1; j >= 0; j--)
		{
			if (a[j] <= t)
				break;
			a[j+1] = a[j];
		}
		a[j+1] = t;
	}
}



/*
===============
=
= M_Random
=
= Returns a 0-255 number
=
===============
*/

unsigned char rndtable[256] = {
	0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66 ,
	74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36 ,
	95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188 ,
	52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224 ,
	149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242 ,
	145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0 ,
	175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235 ,
	25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113 ,
	94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75 ,
	136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196 ,
	135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113 ,
	80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241 ,
	24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224 ,
	145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95 ,
	28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226 ,
	71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36 ,
	17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106 ,
	197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136 ,
	120, 163, 236, 249 
};
int	rndindex = 0;
int prndindex = 0;

int P_Random (void)
{
	prndindex = (prndindex+1)&0xff;
	return rndtable[prndindex];
}

int M_Random (void)
{
	rndindex = (rndindex+1)&0xff;
	return rndtable[rndindex];
}

void M_ClearRandom (void)
{
	rndindex = prndindex = 0;
}

void P_RandomSeed(int seed)
{
	prndindex = seed & 0xff;
}


void M_ClearBox (fixed_t *box)
{
	box[BOXTOP] = box[BOXRIGHT] = D_MININT;
	box[BOXBOTTOM] = box[BOXLEFT] = D_MAXINT;
}

void M_AddToBox (fixed_t *box, fixed_t x, fixed_t y)
{
	if (x<box[BOXLEFT])
		box[BOXLEFT] = x;
	else if (x>box[BOXRIGHT])
		box[BOXRIGHT] = x;
	if (y<box[BOXBOTTOM])
		box[BOXBOTTOM] = y;
	else if (y>box[BOXTOP])
		box[BOXTOP] = y;
}

  
/*=============================================================================  */
 
static inline unsigned LocalToNet (unsigned cmd)
{
	return cmd;
}

static inline unsigned NetToLocal (unsigned cmd)
{
	return cmd;
}

 
/*=============================================================================  */

int		ticrate = 4;
int		ticsinframe;	/* how many tics since last drawer */
int		ticon;
int		frameon;
int		ticrealbuttons, oldticrealbuttons;
boolean	mousepresent;

extern	int	lasttics;

mobj_t	emptymobj;
 
/*
===============
=
= MiniLoop
=
===============
*/
__attribute((noinline))
static void D_Wipe(void)
{
	int b;
	short y[2][WIPEWIDTH];
	short yy[WIPEWIDTH];
	int wipestart[2], done[2] = { 0, 0 };
	int step = 0;

	wipe_InitMelt(y[0]);
	D_memcpy(y[1], y[0], sizeof(y[0])); // double buffered

	wipestart[0] = I_GetTime() - 1;
	wipestart[1] = wipestart[0];

	b = 0;
	step = 0;
	do {
		int nowtime = I_GetTime ();
		done[b] = wipe_ScreenWipe(y[b], yy, nowtime - wipestart[b], step++);
		wipestart[b] = nowtime;
		b ^= 1;
	} while (!done[0] || !done[1]);

	if (step & 1)
	{
		// exit the wipe on the same framebuffer id 
		UpdateBuffer();
	}

	wipe_ExitMelt();
}

int MiniLoop ( void (*start)(void),  void (*stop)(void)
		,  int (*ticker)(void), void (*drawer)(void)
		,  void (*update)(void) )
{
	int		i;
	int		exit;
	int		buttons;
	int		mx, my;
	boolean wipe = canwipe;
	boolean firstdraw = true;

	if (wipe)
	{
		wipe_StartScreen();
	}

/* */
/* setup (cache graphics, etc) */
/* */
	start ();
	exit = 0;
	
	ticon = 0;
	frameon = 0;
	
	gametic = 0;
	prevgametic = 0;

	gameaction = 0;
	gamevbls = 0;
	vblsinframe = 0;
	lasttics = 0;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		players[i].ticbuttons = players[i].oldticbuttons = 0;
		players[i].ticmousex = players[i].ticmousey = 0;
	}

	do
	{
		ticstart = I_GetFRTCounter();

/* */
/* adaptive timing based on previous frame */
/* */
		if (demoplayback || demorecording)
			vblsinframe = TICVBLS;
		else
		{
			vblsinframe = lasttics;
			if (vblsinframe > TICVBLS*2)
				vblsinframe = TICVBLS*2;
#if 0
			else if (vblsinframe < TICVBLS)
				vblsinframe = TICVBLS;
#endif
		}

/* */
/* get buttons for next tic */
/* */
		for (i = 0; i < MAXPLAYERS; i++)
		{
			players[i].oldticbuttons = players[i].ticbuttons;
		}
		oldticrealbuttons = ticrealbuttons;

		buttons = I_ReadControls();
		buttons |= I_ReadMouse(&mx, &my);
		if (demoplayback)
		{
			players[consoleplayer].ticmousex = 0;
			players[consoleplayer].ticmousey = 0;
		}
		else
		{
			players[consoleplayer].ticmousex = mx;
			players[consoleplayer].ticmousey = my;
		}

		players[consoleplayer].ticbuttons = buttons;
		ticrealbuttons = buttons;

		if (demoplayback)
		{
	#ifndef MARS
			if (buttons & (BT_ATTACK|BT_SPEED|BT_USE) )
			{
				exit = ga_exitdemo;
				break;
			}
	#endif
			players[consoleplayer].ticbuttons = buttons = *demo_p++;
		}

		if (splitscreen && !demoplayback)
			players[consoleplayer ^ 1].ticbuttons = I_ReadControls2();
		else if (netgame)	/* may also change vblsinframe */
			players[consoleplayer ^ 1].ticbuttons
				= NetToLocal(I_NetTransfer(LocalToNet(players[consoleplayer].ticbuttons)));

		gamevbls += vblsinframe;

		if (demorecording)
		{
#ifdef MARS_USE_SRAM_DEMO
			I_WriteU32SRAM((intptr_t)demo_p, buttons);
#else
			*demo_p = buttons;
#endif
			demo_p++;
		}
		
		if ((demorecording || demoplayback) && (buttons & BT_PAUSE) )
		{
			exit = ga_exitdemo; // Demo finished by choice, not by death
			break;
		}

		if (gameaction == ga_warped || gameaction == ga_startnew)
		{
			exit = gameaction;	/* hack for NeXT level reloading and net error */
			break;
		}

		if (!firstdraw)
		{
			if (update)
				update();
		}
		firstdraw = false;

		S_PreUpdateSounds();

		ticon++;
		if (gamevbls / TICVBLS > gametic)
			gametic++;
		exit = ticker();

		S_UpdateSounds();

		/* */
		/* sync up with the refresh */
		/* */
		while (!I_RefreshCompleted())
			;

		drawer();

		if (!exit && wipe)
		{
			wipe_EndScreen();
			D_Wipe();
			wipe = false;
		}

		prevgametic = gametic;

#if 0
while (!I_RefreshCompleted ())
;	/* DEBUG */
#endif

#ifdef JAGUAR
		while ( DSPRead(&dspfinished) != 0xdef6 )
		;
#endif
	} while (!exit);

	stop ();
	S_Clear ();
	
	for (i = 0; i < MAXPLAYERS; i++)
		players[i].mo = &emptymobj;	/* for net consistency checks */

	return exit;
} 
 


/*=============================================================================  */

void ClearEEProm (void);
void DrawSinglePlaque (jagobj_t *pl);

jagobj_t	*titlepic;

static char* credits[2];
static short creditspage;

int TIC_Abortable (void)
{
#ifdef JAGUAR
	jagobj_t	*pl;
#endif
	int buttons = players[consoleplayer].ticbuttons;
	int oldbuttons = players[consoleplayer].oldticbuttons;

	if (titlepic == NULL)
		return 1;
	if (ticon < TICVBLS)
		return 0;
	if (ticon >= (gameinfo.titleTime + gameinfo.creditsTime))
		return 1;		/* go on to next demo */
	if (ticon >= gameinfo.titleTime && creditspage != 0 && !credits[creditspage-1])
		return 1;

#ifdef JAGUAR
	if (ticbuttons[0] == (BT_OPTION|BT_STAR|BT_HASH) )
	{	/* reset eeprom memory */
		void Jag68k_main (void);

		ClearEEProm ();
		pl = W_CacheLumpName ("defaults", PU_STATIC);	
		DrawSinglePlaque (pl);
		Z_Free (pl);
		S_Clear ();
		ticcount = 0;
		while ( (junk = ticcount) < 240)
		;
		Jag68k_main ();
	}
#endif

#ifdef MARS
	if ( (buttons & BT_A) && !(oldbuttons & BT_A) )
		return ga_exitdemo;
	if ( (buttons & BT_B) && !(oldbuttons & BT_B) )
		return ga_exitdemo;
	if ( (buttons & BT_C) && !(oldbuttons & BT_C) )
		return ga_exitdemo;
	if ( (buttons & BT_START) && !(oldbuttons & BT_START) )
		return ga_exitdemo;
#else
	if ( (buttons & BT_ATTACK) && !(oldbuttons & BT_ATTACK) )
		return ga_exitdemo;
	if ( (buttons & BT_USE) && !(oldbuttons & BT_USE) )
		return ga_exitdemo;
	if ( (buttons & BT_OPTION) && !(oldbuttons & BT_OPTION) )
		return ga_exitdemo;
	if ( (buttons & BT_START) && !(oldbuttons & BT_START) )
		return ga_exitdemo;
#endif

	return 0;
}


/*============================================================================= */

void START_Title(void)
{
#ifdef MARS
	int		i;

	for (i = 0; i < 2; i++)
	{
		I_ClearFrameBuffer();
		UpdateBuffer();
	}
#else
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
	DoubleBufferSetup();
#endif

	titlepic = NULL;
	credits[0] = credits[1] = NULL;
	creditspage = 0;

	if (!(gameinfo.titleTime + gameinfo.creditsTime))
		return;

	if (*gameinfo.titlePage || *gameinfo.creditsPage) {
		int l, numl;
		VINT lumps[3];
		lumpinfo_t li[3];

		W_LoadPWAD(PWAD_CD);

		numl = 0;
		if (*gameinfo.titlePage)
		{
			lumps[numl++] = W_CheckNumForName(gameinfo.titlePage);
		}

		if (*gameinfo.creditsPage)
		{
			/* build a temp in-memory PWAD */
			for (i = 0; i < 2; i++)
			{
				char name[9];
				D_strncpy(name, gameinfo.creditsPage, 8);
				name[7]+= i;
				name[8] = '\0';

				lumps[numl++] = W_CheckNumForName(name);
			}

			W_CacheWADLumps(li, numl, lumps, true);
		}

		l = 0;
		if (*gameinfo.titlePage)
		{
			if (li[l].name[0])
				titlepic = W_CacheLumpName(li[l++].name, PU_STATIC);
		}

		if (*gameinfo.creditsPage)
		{
			for (i = 0; i < 2; i++)
			{
				if (!li[l].name[0])
					break;
				credits[i] = W_CacheLumpName(li[l++].name, PU_STATIC);
			}
		}

		W_LoadPWAD(PWAD_NONE);
	}

	S_StartSongByName(gameinfo.titleMus, 0, gameinfo.titleCdTrack);

	I_InitMenuFire(titlepic);
}

void STOP_Title (void)
{
	int i;

	if (titlepic != NULL)
	{
		Z_Free (titlepic);
		titlepic = NULL;
	}
	for (i = 0; i < 2; i++)
	{
		if (credits[i])
			Z_Free(credits[i]);
		credits[i] = NULL;
	}

	if (!gameinfo.titleTime)
		return;

#ifdef MARS
	I_StopMenuFire();
#endif

	S_StopSong();
}

static void DRAW_RightString(int x, int y, const char* str)
{
	int len = I_Print8Len(str);
	I_Print8(x - len * 8, y, str);
}

static void DRAW_CenterString(int y, const char* str)
{
	int len = I_Print8Len(str);
	I_Print8((320 - len * 8) / 2, y, str);
}

static void DRAW_LineCmds(char *lines)
{
	int y;
	static const char* dots = "...";
	int dots_len = mystrlen(dots);
	int x_right = (320 - dots_len*8) / 2 - 2;
	int x_left = (320 + dots_len*8) / 2;
	char* p = lines;

	y = 0;
	while (1) {
		char *end = D_strchr(p, '\n');
		char* str = p, *nextl;
		char cmd = str[0];
		char inc = str[1];
		char bak = 0;

		if (!cmd || !inc)
			break;

		str += 3;
		if (end)
		{
			nextl = end + 1;
			if (*(end - 1) == '\r')
				--end;
			bak = *end;
			*end = '\0';
		}
		else
		{
			nextl = NULL;
		}

		switch (cmd) {
		case 'c':
			DRAW_CenterString(y, str);
			break;
		case 'r':
			DRAW_RightString(x_right, y, str);
			break;
		case 'l':
			I_Print8(x_left, y, str);
			break;
		}

		if (inc == 'i')
		{
			y++;
		}

		if (nextl)
			*end = bak;
		else
			break;
		p = nextl;
	}
}

void DRAW_Title (void)
{
	int tic = ticon;
	int page;

#ifdef MARS
	if (gameinfo.titleTime && tic < gameinfo.titleTime)
	{
		I_DrawMenuFire();
		return;
	}
#endif

	tic -= gameinfo.titleTime;
	page = tic >= gameinfo.creditsTime/2 ? 2 : 1;
	if (page != creditspage)
	{
		creditspage = page;
		if (credits[page-1])
			DoubleBufferSetup();
	}

	if (credits[creditspage-1])
		DRAW_LineCmds(credits[creditspage-1]);
}

/*============================================================================ */
void RunMenu (void);

void RunTitle (void)
{
	startskill = sk_medium;
	startmap = 1;
	starttype = gt_single;
	consoleplayer = 0;
	startsave = -1;
	startsplitscreen = 0;
	canwipe = false;
	MiniLoop (START_Title, STOP_Title, TIC_Abortable, DRAW_Title, UpdateBuffer);
}

#define MAX_ATTRACT_DEMOS 10

static void RunAttractDemos (void)
{
	int i;
	int exit = ga_exitdemo;
	boolean first = true;

	if (gameinfo.noAttractDemo)
		return;

	do {
		for (i = 0; i < MAX_ATTRACT_DEMOS; i++)
		{
			int l;
			unsigned *demo;
			char demoname[9];

			if (!first)
			{
				// loading demo from CD is going to write some data into
				// the framebuffer, so store a copy of the framebuffer,
				// then flip and blit the stored copy after we finished
				// loading the demo
				I_StoreScreenCopy();
				UpdateBuffer();
			}

			// avoid zone memory fragmentation which is due to happen
			// if the demo lump cache is tucked after the level zone.
			// this will cause shrinking of the zone area available
			// for the level data after each demo playback and eventual
			// Z_Malloc failure
			Z_FreeTags(mainzone);

			W_LoadPWAD(PWAD_CD);

			demo = NULL;
			D_snprintf(demoname, sizeof(demoname), "DEMO%1d", i+1);

			l = W_CheckNumForName(demoname);
			if (l >= 0)
				demo = W_CacheLumpNum(l, PU_STATIC);

			W_LoadPWAD(PWAD_NONE);

			if (!first)
				I_RestoreScreenCopy();

			if (!demo && first)
				return;
			if (!demo)
				break;

			first = false;
			exit = G_PlayDemoPtr (demo);
			Z_Free(demo);

			if (exit == ga_exitdemo)
				break;
		}
	} while (exit != ga_exitdemo);
}


void RunMenu (void)
{
#ifdef MARS
	M_Start();

	RunAttractDemos();

	M_Stop();
#else
reselect:
	MiniLoop(M_Start, M_Stop, M_Ticker, M_Drawer, NULL);
#endif

	if (consoleplayer == 0)
	{
		if (starttype != gt_single && !startsplitscreen)
		{
			I_NetSetup();
#ifndef MARS
			if (starttype == gt_single)
				goto reselect;		/* aborted net startup */
#endif
		}
	}

	if (startsave != -1)
		G_LoadGame(startsave);
	else
		G_InitNew(startskill, startmap, starttype, startsplitscreen);

	G_RunGame ();
}

/*============================================================================ */


 
/* 
============= 
= 
= D_DoomMain 
= 
============= 
*/ 
 
skill_t		startskill = sk_medium;
int			startmap = 1;
gametype_t	starttype = gt_single;
int			startsave = -1;
boolean 	startsplitscreen = 0;

void D_DoomMain (void) 
{    
D_printf ("C_Init\n");
	C_Init ();		/* set up object list / etc	  */
D_printf ("Z_Init\n");
	Z_Init (); 
D_printf ("W_Init\n");
	W_Init ();
D_printf ("I_Init\n");
	I_Init (); 
D_printf ("S_Init\n");
	S_Init ();
D_printf ("R_Init\n");
	R_Init (); 
D_printf ("P_Init\n");
	P_Init (); 
D_printf("ST_Init\n");
	ST_Init ();
D_printf("O_Init\n");
	O_Init ();

gameselect:
	if (gamemaplist)
	{
		int i;
		for (i = 0; gamemaplist[i]; i++)
			Z_Free(gamemaplist[i]);
		Z_Free(gamemaplist);
		gamemaplist = NULL;
	}
	gamemapcount = 0;

#ifdef MARS
	canwipe = false;
	MiniLoop(GS_Start, GS_Stop, GS_Ticker, GS_Drawer, I_Update);
D_printf ("W_Init2\n");
    W_InitCDPWAD(PWAD_CD, cd_pwad_name);
#endif

D_printf ("S_InitMusic\n");
	S_InitMusic();
D_printf("G_Init\n");
	G_Init();

/*========================================================================== */

D_printf ("DM_Main\n");

/*	while (1)	RunDemo ("DEMO1"); */

/*	MiniLoop (F_Start, F_Stop, F_Ticker, F_Drawer, UpdateBuffer); */

/*G_InitNew (startskill, startmap, gt_deathmatch, false); */
/*G_RunGame (); */

#ifdef NeXT
	if (M_CheckParm ("-dm") )
	{
		I_NetSetup ();
		G_InitNew (startskill, startmap, gt_deathmatch, false);
	}
	else if (M_CheckParm ("-dial") || M_CheckParm ("-answer") )
	{
		I_NetSetup ();
		G_InitNew (startskill, startmap, gt_coop, false);
	}
	else
		G_InitNew (startskill, startmap, gt_single, false);
	G_RunGame ();
#endif

	RunTitle();

	RunMenu();

	gameaction = 0;
	goto gameselect;
}
