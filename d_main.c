/* D_main.c  */
 
#include "doomdef.h" 

boolean		splitscreen = false;
VINT		controltype = 0;		/* determine settings for BT_* */
VINT		alwaysrun = 0;
VINT		colormapopt = 1;

int			gamevbls;			/* may not really be vbls in multiplayer */
int			vblsinframe[MAXPLAYERS];		/* range from ticrate to ticrate*2 */

VINT		ticsperframe = MINTICSPERFRAME;

int			maxlevel;			/* highest level selectable in menu (1-25) */
jagobj_t	*backgroundpic;

int				*demo_p, *demobuffer;

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

void D_memset (void *dest, int val, int count)
{
	byte	*p;
	int		*lp;

/* round up to nearest word */
	p = dest;
	while ((int)p & WORDMASK)
	{
		if (--count < 0)
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
}

#ifdef MARS
void fast_memcpy(void *dest, const void *src, int count);
#endif

void D_memcpy (void *dest, const void *src, int count)
{
	byte	*d;
	const byte *s;

#ifdef MARS
	if ( (((intptr_t)dest & 15) == ((intptr_t)src & 15)) && (count > 16)) {
		unsigned i;
		unsigned rem = 16 - ((intptr_t)dest & 15), wordbytes;

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
		for (j = i; j > 0; j--)
		{
			if (a[j - 1] <= a[j])
				break;
			int t = a[j - 1];
			a[j - 1] = a[j];
			a[j] = t;
		}
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
 
unsigned LocalToNet (unsigned cmd)
{
	return cmd & 0xffff;
}

unsigned NetToLocal (unsigned cmd)
{
	return cmd;
}

 
/*=============================================================================  */

int		ticrate = 4;
int		ticsinframe;	/* how many tics since last drawer */
int		ticon;
int		frameon;
int		ticbuttons[MAXPLAYERS];
int		oldticbuttons[MAXPLAYERS];
int		ticmousex[MAXPLAYERS], ticmousey[MAXPLAYERS];
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

int MiniLoop ( void (*start)(void),  void (*stop)(void)
		,  int (*ticker)(void), void (*drawer)(void) )
{
	int		exit;
	int		buttons;
	int		mx, my;
		
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
	vblsinframe[0] = vblsinframe[1] = 0;
	lasttics = 0;

	ticbuttons[0] = ticbuttons[1] = oldticbuttons[0] = oldticbuttons[1] = 0;
	ticmousex[0] = ticmousex[1] = ticmousey[0] = ticmousey[1] = 0;

	do
	{
/* */
/* adaptive timing based on previous frame */
/* */
		if (demoplayback || demorecording)
			vblsinframe[consoleplayer] = TICVBLS;
		else
		{
			vblsinframe[consoleplayer] = lasttics;
			if (vblsinframe[consoleplayer] > TICVBLS*2)
				vblsinframe[consoleplayer] = TICVBLS*2;
#if 0
			else if (vblsinframe[consoleplayer] < TICVBLS)
				vblsinframe[consoleplayer] = TICVBLS;
#endif
		}

/* */
/* get buttons for next tic */
/* */
		oldticbuttons[0] = ticbuttons[0];
		oldticbuttons[1] = ticbuttons[1];
		oldticrealbuttons = ticrealbuttons;

		buttons = I_ReadControls();
		buttons |= I_ReadMouse(&mx, &my);
		if (demoplayback)
		{
			ticmousex[consoleplayer] = 0;
			ticmousey[consoleplayer] = 0;
		}
		else
		{
			ticmousex[consoleplayer] = mx;
			ticmousey[consoleplayer] = my;
		}

		ticbuttons[consoleplayer] = buttons;
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
			ticbuttons[consoleplayer] = buttons = *demo_p++;
		}

		if (splitscreen && !demoplayback)
		{
			vblsinframe[consoleplayer ^ 1] = vblsinframe[consoleplayer];
			ticbuttons[consoleplayer ^ 1] = I_ReadControls2();
		}
		else if (netgame)	/* may also change vblsinframe */
			ticbuttons[!consoleplayer]
				= NetToLocal(I_NetTransfer(LocalToNet(ticbuttons[consoleplayer])));

		gamevbls += vblsinframe[0];

		if (demorecording)
			*demo_p++ = buttons;
		
		if ((demorecording || demoplayback) && (buttons & BT_PAUSE) )
			exit = ga_completed;

		if (gameaction == ga_warped)
		{
			exit = ga_warped;	/* hack for NeXT level reloading and net error */
			break;
		}

		ticon++;
		if (gamevbls / TICVBLS > gametic)
			gametic++;
		exit = ticker();

		if (gametic > prevgametic)
			S_UpdateSounds();

		/* */
		/* sync up with the refresh */
		/* */
		while (!I_RefreshCompleted())
			;

		if (!exit)
			drawer();

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


	while (!I_RefreshCompleted ())
	;
	stop ();
	S_Clear ();
	
	players[0].mo = players[1].mo = &emptymobj;	/* for net consistancy checks */
	
	return exit;
} 
 


/*=============================================================================  */

void ClearEEProm (void);
void DrawSinglePlaque (jagobj_t *pl);

int TIC_Abortable (void)
{
#ifdef JAGUAR
	jagobj_t	*pl;
#endif

	if (ticon < TICVBLS)
		return 0;
	if (ticon >= gameinfo.titleTime)
		return 1;		/* go on to next demo */

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

	if ( (ticbuttons[0] & BT_ATTACK) && !(oldticbuttons[0] & BT_ATTACK) )
		return ga_exitdemo;
	if ( (ticbuttons[0] & BT_SPEED) && !(oldticbuttons[0] & BT_SPEED) )
		return ga_exitdemo;
	if ( (ticbuttons[0] & BT_USE) && !(oldticbuttons[0] & BT_USE) )
		return ga_exitdemo;
	if ( (ticbuttons[0] & BT_OPTION) && !(oldticbuttons[0] & BT_OPTION) )
		return ga_exitdemo;
	if ( (ticbuttons[0] & BT_START) && !(oldticbuttons[0] & BT_START) )
		return ga_exitdemo;

	return 0;
}


/*============================================================================= */

jagobj_t	*titlepic;

void START_Title(void)
{
	int l;
#ifdef MARS
	int		i;

	for (i = 0; i < 2; i++)
	{
		I_ClearFrameBuffer();
		I_Update();
	}
#else
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
	DoubleBufferSetup();
#endif

	l = gameinfo.titlePage;
	titlepic = l != -1 ? W_CacheLumpNum(l, PU_STATIC) : NULL;

	S_StartSong(gameinfo.titleMus, 0, cdtrack_title);

#ifdef MARS
	I_InitMenuFire(titlepic);
#endif
}

void STOP_Title (void)
{
	if (titlepic != NULL)
		Z_Free (titlepic);
#ifdef MARS
	I_StopMenuFire();
#endif
	S_StopSong();
}

void DRAW_Title (void)
{
#ifdef MARS
	I_DrawMenuFire();
#endif
	UpdateBuffer();
}

/*============================================================================= */

#ifdef MARS
static char* credits;
static short creditspage;
#endif

static void START_Credits (void)
{
#ifdef MARS
	titlepic = NULL;
	creditspage = 1;
	credits = W_CacheLumpNum(gameinfo.creditsPage, PU_STATIC);
#else
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
	titlepic = W_CacheLumpName("credits", PU_STATIC);
#endif
	DoubleBufferSetup();
}

void STOP_Credits (void)
{
#ifdef MARS
	if (credits)
		Z_Free(credits);
#endif
	if (titlepic)
		Z_Free(titlepic);
}

static int TIC_Credits (void)
{
	if (ticon >= gameinfo.creditsTime)
		return 1;		/* go on to next demo */
		
	if ( (ticbuttons[0] & BT_ATTACK) && !(oldticbuttons[0] & BT_ATTACK) )
		return ga_exitdemo;
	if ( (ticbuttons[0] & BT_SPEED) && !(oldticbuttons[0] & BT_SPEED) )
		return ga_exitdemo;
	if ( (ticbuttons[0] & BT_USE) && !(oldticbuttons[0] & BT_USE) )
		return ga_exitdemo;
	if ( (ticbuttons[0] & BT_START) && !(oldticbuttons[0] & BT_START) )
		return ga_exitdemo;

	if (ticon * 2 >= gameinfo.creditsTime)
	{
		if (creditspage != 2)
		{
			char name[9];

			D_memcpy(name, W_GetNameForNum(gameinfo.creditsPage), 8);
			name[7]+= creditspage;
			name[8] = '\0';

			int l = W_CheckNumForName(name);
			if (l >= 0)
			{
				Z_Free(credits);
				credits = W_CacheLumpNum(l, PU_STATIC);
			}
			creditspage = 2;

			DoubleBufferSetup();
		}
	}

	return 0;
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

static void DRAW_Credits(void)
{
#ifdef MARS
	DRAW_LineCmds(credits);
#else
	DrawJagobj(titlepic, 0, 0);
#endif

	UpdateBuffer();
}

/*============================================================================ */
void RunMenu (void);
void RunCredits(void);

void RunTitle (void)
{
	int		exit;

	exit = MiniLoop (START_Title, STOP_Title, TIC_Abortable, DRAW_Title);
#ifdef MARS
	if (exit == ga_exitdemo)
		RunMenu ();
	else
#endif
		RunCredits();
}

void RunCredits (void)
{
	int		l;
	int		exit;
	
	l = gameinfo.creditsPage;
	if (l > 0)
		exit = MiniLoop(START_Credits, STOP_Credits, TIC_Credits, DRAW_Credits);
	else
		exit = ga_exitdemo;

	if (exit == ga_exitdemo)
		RunMenu ();
}

int  RunDemo (char *demoname)
{
	int	*demo;
	int	exit;
	int lump;

	lump = W_CheckNumForName(demoname);
	if (lump == -1)
		return ga_exitdemo;

	// avoid zone memory fragmentation which is due to happen
	// if the demo lump cache is tucked after the level zone.
	// this will cause shrinking of the zone area available
	// for the level data after each demo playback and eventual
	// Z_Malloc failure
	Z_FreeTags(mainzone);

	demo = W_CacheLumpNum(lump, PU_STATIC);
	exit = G_PlayDemoPtr (demo);
	Z_Free(demo);

#ifndef MARS
	if (exit == ga_exitdemo)
		RunMenu ();
#endif
	return exit;
}


void RunMenu (void)
{
#ifdef MARS
reselect:
	M_Start();

	while (1) {
		if (RunDemo("DEMO1") == ga_exitdemo)
			break;
		if (RunDemo("DEMO2") == ga_exitdemo)
			break;
	}

	M_Stop();

	if (starttype != gt_single)
	{
		I_NetSetup();
		if (starttype == gt_single)
			goto reselect;		/* aborted net startup */
	}
#else
reselect:
	MiniLoop(M_Start, M_Stop, M_Ticker, M_Drawer);

	if (starttype != gt_single)
	{
		I_NetSetup ();
		if (starttype == gt_single)
			goto reselect;		/* aborted net startup */
	}
#endif

	if (startsave != -1)
		G_LoadGame(startsave);
	else
		G_InitNew(startskill, startmap, starttype);

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
D_printf ("R_Init\n");
	R_Init (); 
D_printf ("P_Init\n");
	P_Init (); 
D_printf ("S_Init\n");
	S_Init ();
D_printf("ST_Init\n");
	ST_Init ();
D_printf("O_Init\n");
	O_Init ();
D_printf("G_Init\n");
	G_Init();

/*========================================================================== */

D_printf ("DM_Main\n");

/*	while (1)	RunDemo ("DEMO1"); */

/*G_RecordDemo ();	// set startmap and startskill */

/*	MiniLoop (F_Start, F_Stop, F_Ticker, F_Drawer); */

/*G_InitNew (startskill, startmap, gt_deathmatch); */
/*G_RunGame (); */

#ifdef NeXT
	if (M_CheckParm ("-dm") )
	{
		I_NetSetup ();
		G_InitNew (startskill, startmap, gt_deathmatch);
	}
	else if (M_CheckParm ("-dial") || M_CheckParm ("-answer") )
	{
		I_NetSetup ();
		G_InitNew (startskill, startmap, gt_coop);
	}
	else
		G_InitNew (startskill, startmap, gt_single);
	G_RunGame ();
#endif

#ifdef MARS
	while (1)
	{
		RunTitle();
		RunMenu();
	}
#else
	while (1)
	{
		RunTitle();
		RunDemo("DEMO1");
		RunCredits ();
		RunDemo("DEMO2");
	}
#endif
}
 
