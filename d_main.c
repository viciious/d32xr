/* D_main.c  */
 
#include "doomdef.h" 
 
unsigned	BT_ATTACK = BT_B;
unsigned	BT_USE = BT_C;
unsigned	BT_STRAFE =	BT_C;
unsigned	BT_SPEED = BT_A;

int			controltype;		/* determine settings for BT_* */

int			gamevbls;			/* may not really be vbls in multiplayer */
int			vblsinframe;		/* range from 4 to 8 */

int			maxlevel;			/* highest level selectable in menu (1-25) */
jagobj_t	*backgroundpic;

int				*demo_p, *demobuffer;

/*============================================================================ */


#define WORDMASK	3

int abs (int x)
{
	if (x<0)
		return -x;
	return x;
}

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
	while (count--)
		*p++ = val;
}


void D_memcpy (void *dest, void *src, int count)
{
	byte	*d, *s;
	
	d = (byte *)dest;
	s = (byte *)src;
	while (count--)
		*d++ = *s++;
}


void D_strncpy (char *dest, char *src, int maxcount)
{
	byte	*p1,*p2;
	p1 = (byte *)dest;
	p2 = (byte *)src;
	while (maxcount--)
		if (! (*p1++ = *p2++) )
			return;
}

int D_strncasecmp (char *s1, char *s2, int len)
{
	while (*s1 && *s2)
	{
		if (*s1 != *s2)
			return 1;
		s1++;
		s2++;
		if (!--len)
			return 0;
	}
	if (*s1 != *s2)
		return 1;
	return 0;	
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


void M_ClearBox (fixed_t *box)
{
	box[BOXTOP] = box[BOXRIGHT] = MININT;
	box[BOXBOTTOM] = box[BOXLEFT] = MAXINT;
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
	int		a,b,c;

	a = cmd & BT_SPEED;
	b = cmd & BT_ATTACK;
	c = cmd & BT_USE;
	
	cmd &= ~(JP_A|JP_B|JP_C);
	
	if (a)
		cmd |= BT_A;
	if (b)
		cmd |= BT_B;
	if (c)
		cmd |= BT_C;
		
	return cmd;
}

unsigned NetToLocal (unsigned cmd)
{
	int		a,b,c;

	a = cmd & JP_A;
	b = cmd & JP_B;
	c = cmd & JP_C;
	
	cmd &= ~(JP_A|JP_B|JP_C);
	
	if (a)
		cmd |= BT_SPEED;
	if (b)
		cmd |= BT_ATTACK;
	if (c)
		cmd |= BT_USE;
		
	return cmd;
}



unsigned GetDemoCmd (void)
{
	unsigned	cmd;
	
	cmd = *demo_p++;
	
	return NetToLocal (cmd);
}
 
/*=============================================================================  */
 
int		ticsinframe;	/* how many tics since last drawer */
int		ticon;
int		frameon;
int		ticbuttons[MAXPLAYERS];
int		oldticbuttons[MAXPLAYERS];

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
		
/* */
/* setup (cache graphics, etc) */
/* */
	start ();
	exit = 0;
	
	ticon = 0;
	frameon = 0;
	
	gameaction = 0;
	gamevbls = 0;
	vblsinframe = 4;
	
	ticbuttons[0] = ticbuttons[1] = oldticbuttons[0] = oldticbuttons[1] = 0;

	do
	{
/* */
/* run the tic immediately */
/* */
		gamevbls += vblsinframe;
		exit = ticker ();

/* */
/* adaptive timing based on previous frame */
/* */
		if (demoplayback || demorecording)
			vblsinframe = 4;
		else
		{
			vblsinframe = lasttics;
			if (vblsinframe > 8)
				vblsinframe = 8;
#if 0
			else if (vblsinframe < 4)
				vblsinframe = 4;
#endif
		}
					
/* */
/* get buttons for next tic */
/* */
		oldticbuttons[0] = ticbuttons[0];
		oldticbuttons[1] = ticbuttons[1];

		buttons = I_ReadControls ();
		ticbuttons[consoleplayer] = buttons;
		if (demoplayback)
		{
			if (buttons & (BT_A|BT_B|BT_C) )
			{
				exit = ga_exitdemo;
				break;
			}
			ticbuttons[consoleplayer] = buttons = GetDemoCmd ();
		}

		if (netgame)	/* may also change vblsinframe */
			ticbuttons[!consoleplayer]
			= NetToLocal( I_NetTransfer ( LocalToNet (ticbuttons[consoleplayer]) ) );

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

/* */
/* sync up with the refresh */
/* */
		while (!I_RefreshCompleted ())
		;
		S_UpdateSounds ();
		drawer ();

#if 0
while (!I_RefreshCompleted ())
;	/* DEBUG */
#endif

		while ( DSPRead(&dspfinished) != 0xdef6 )
		;

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
	jagobj_t	*pl;
	
	if	(ticon >= 8*15)
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

	if ( (ticbuttons[0] & BT_A) && !(oldticbuttons[0] & BT_A) )
		return ga_exitdemo;
	if ( (ticbuttons[0] & BT_B) && !(oldticbuttons[0] & BT_B) )
		return ga_exitdemo;
	if ( (ticbuttons[0] & BT_C) && !(oldticbuttons[0] & BT_C) )
		return ga_exitdemo;
	return 0;
}


/*============================================================================= */

jagobj_t	*titlepic;

void START_Title (void)
{
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
	DoubleBufferSetup ();
	titlepic = W_CacheLumpName ("title",PU_STATIC);
	S_StartSong(mus_intro, 0);
}

void STOP_Title (void)
{
	Z_Free (titlepic);
	S_StopSong();
}

void DRAW_Title (void)
{
	DrawJagobj (titlepic, 0, 0);
	UpdateBuffer ();
}

/*============================================================================= */


void START_Credits (void)
{
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
	DoubleBufferSetup ();
	titlepic = W_CacheLumpName ("credits",PU_STATIC);
}

void STOP_Credits (void)
{
	Z_Free (titlepic);
}

int TIC_Credits (void)
{
	if	(ticon >= 10*15)
		return 1;		/* go on to next demo */
		
	if ( (ticbuttons[0] & BT_A) && !(oldticbuttons[0] & BT_A) )
		return ga_exitdemo;
	if ( (ticbuttons[0] & BT_B) && !(oldticbuttons[0] & BT_B) )
		return ga_exitdemo;
	if ( (ticbuttons[0] & BT_C) && !(oldticbuttons[0] & BT_C) )
		return ga_exitdemo;
	return 0;
}

void DRAW_Credits (void)
{
	DrawJagobj (titlepic, 0, 0);
	UpdateBuffer ();
}

/*============================================================================ */

void RunMenu (void);

void RunTitle (void)
{
	int		exit;
	
	exit = MiniLoop (START_Title, STOP_Title, TIC_Abortable, DRAW_Title);
	if (exit == ga_exitdemo)
		RunMenu ();
}

void RunCredits (void)
{
	int		exit;
	
	exit = MiniLoop (START_Credits, STOP_Credits, TIC_Credits, DRAW_Credits);
	if (exit == ga_exitdemo)
		RunMenu ();
}

void RunDemo (char *demoname)
{
	int	*demo;
	int	exit;
		
	demo = W_CacheLumpName (demoname, PU_STATIC);
	exit = G_PlayDemoPtr (demo);
	Z_Free (demo);
	if (exit == ga_exitdemo)
		RunMenu ();
}


void RunMenu (void)
{
reselect:
	MiniLoop (M_Start, M_Stop, M_Ticker, M_Drawer);
	if (starttype != gt_single)
	{
		I_NetSetup ();
		if (starttype == gt_single)
			goto reselect;		/* aborted net startup */
	}
	
	G_InitNew (startskill, startmap, starttype);
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
 
void DM_main (void);
 
void testgpu (void);

int			checkit;
skill_t		startskill = sk_medium;
int			startmap = 3;
gametype_t	starttype = gt_single;

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
#ifndef MARS
D_printf ("S_Init\n");
	S_Init ();
	ST_Init ();
	O_Init ();
#endif

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

	while (1)
	{
		RunTitle ();
		RunDemo ("DEMO1");
		RunCredits ();
		RunDemo ("DEMO2");
	}

} 
 
