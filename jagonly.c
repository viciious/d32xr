/* marsonly.c */

#include "doomdef.h"
#include "r_local.h"

/*

JAGUAR MEMORY MAP

0x  4000		text/data/bss
0x 80000		start heap
0x1c8000		sbarback (0x3210)
0x1cb210		sbarfront (0x3200)
0x1ce410		debugscreen (0x1bf0)
0x1d0000		screens[0]
0x1e0000		screens[1]
0x1f0000		soundbuffer
0x1f4000		stack (8 bytes of screenshade)
0x200000		end of ram


at all times,

pixel_t *workingscreen; will point to the undisplayed screens[workpage]

*/

boolean		debugscreenstate = false;

boolean		debugscreenactive;

pixel_t	*screens[2];	/* [SCREENWIDTH*SCREENHEIGHT];  */
short	*palette8;		/* [256]	for translating 8 bit source to 16 bit */
int		*screenshade;	/* pixels for screen shifting */

byte		*debugscreen;
extern	jagobj_t	*sbar;
extern	byte		*sbartop;

int			workpage;	/* which frame is not being displayed */
int			worklist;	/* which listbuffer is not being used */

int	isrvmode = 0xC1+(7<<9);		/* vmode value set by ISR each screen */

extern	int		listbuffer[256];
extern	int		listbuffer1[256];
extern	int		listbuffer2[256];
extern	int		stopobj[2];

int		*listbuffers[2] = {listbuffer1,listbuffer2};

int		*displaylist_p;				/* list currently being displayed */
int		*readylist_p = stopobj;		/* list to display next frame */
int		*worklist_p, *work_p;		/* list currently being built */

int             joypad[32]; 
int             joystick1; 
int             ticcount; 
 
unsigned	branch1, branch2;
 
int             junk; 
int             spincount;  
int             ZERO = 0, zero = 0, zero2 = 0; 

pixel_t			*framebuffer;

extern	jagobj_t *sbar;

char	hexdigits[16] = "0123456789ABCDEF";

void InitDisplay (void);

void Spin (void)
{
	I_Print8 (1,24,"Spin...");

	while (1)
	;
}

void ReadEEProm (void);

/* 
================ 
= 
= Jag68k_main  
= 
================ 
*/ 
 
extern int gpubase,gpubase_init;
extern int dspbase,dspbase_init;
extern int enddata;

extern short video_height;
extern	short a_vdb, a_vde, a_hdb, a_hde;

unsigned BASEORGY;

void Jag68k_main (void)
{
	int		i;

	debugscreenactive = debugscreenstate;
	
/* clear screen */
	readylist_p = stopobj;
	*(int *)0xf1a114 = ZERO;                /* make sure it's stopped  */
	*(int *)0xf02114 = ZERO;                /* make sure it's stopped  */

/* clear bss and screens */
	D_memset (&enddata, 0, 0x1fc000 - (int)&enddata);


/* init vars  */

	sbar = (jagobj_t *)0x1c8000;
	sbartop = (byte *)0x1cb210;
	debugscreen = (byte *)0x1ce410;
	framebuffer = screens[0] = (pixel_t *)0x1d0000; 
	screens[1] = (pixel_t *)0x1e0000; 
	screenshade = (int *)0x1f4000;

#if 0
soundbuffer[0] = 0xffff;		/* debug click in sound buffer */
soundbuffer[1] = 0xffff;
soundbuffer[4] = 0xeeee;
soundbuffer[5] = 0xeeee;
#endif

	I_Print8 (1,0,"GPU_main");

/* */
/* copy dsp programs */
/* */
	*(int *)0xf1a10c = 0x00050005;          /* operate in correct endian mode  */
	*(int *)0xf1a114 = ZERO;                /* make sure it's stopped  */
	*(int *)0xf1a100 =  (31<<9)+(1<<17);    /* clear interrupt latches  */
	for (i=0 ; i< 128 ; i++) 
		((int *)0xf1b000)[i] = ((int *)&dspbase)[i]; 
	for ( ; i< 2048 ; i++) 
		((int *)0xf1b000)[i] = ZERO; 
	*(int *)0xf1a110 = (int)&dspbase_init;		/* set it up at init code  */
	*(int *)0xf1a114 = 1;                   /* start it  */

/* */
/* init NTSC/PAL stuff */
/* */
	if (video_height >= 256)	/* pal */
	{
		BASEORGY = 48;
		ticrate = 3;
	}
	else
	{
		BASEORGY = 24;		/* NTSC */
		ticrate = 4;
	}

		branch1 = 0x8003 + (a_vde<<3);
		branch2 = 0x4003 + (a_vdb<<3);

/* */
/* init sound hardware */
/* */

/*	*(int *)0xf1a148 = ZERO;	// R_DAC */
/*	*(int *)0xf1a14c = ZERO;	// L_DAC */
	*(int *)0xf14000 = 0x100;	/* JOYSTICK (unmute sound) */
	
	*(int *)0xf1a150 = 19;		/* SCLK (SSI Clock frequency) */
	*(int *)0xf1a154 = 0x15;	/* SMODE (SSI Control) */

/* copy gpu programs      */
	*(int *)0xf02114 = ZERO;                /* make sure it's stopped  */
	*(int *)0xf02100 = (31<<9);     		/* clear interrupt latches  */
	*(int *)0xf0211c = ZERO;                /* clear divide control   */
	for (i=0 ; i< 64 ; i++) 
		((int *)0xf03000)[i] = ((int *)&gpubase)[i]; 
	for ( ; i< 1024 ; i++) 
		((int *)0xf03000)[i] = 0; 
	*(int *)0xf02110 = (int)&gpubase_init;		/* set it up at init code  */
	*(int *)0xf02114 = 1;                   /* start it  */

/* set a two color palette */
	((pixel_t *)0xf00400)[0] = ZERO;
	for (i=1 ; i<256 ; i++)
		((pixel_t *)0xf00400)[i] = (pixel_t)0xffff;
		
	I_Update ();
	
/* */
/* load defaults */
/* */
	ReadEEProm ();

/* */
/* start doom */
/* */
	D_DoomMain ();
}



/*
==============================================================================

						DOOM INTERFACE CODE

==============================================================================
*/


byte font8[] =
{0,0,0,0,0,0,0,0,24,24,24,24,24,0,24,0,
54,54,54,0,0,0,0,0,108,108,254,108,254,108,108,0,
48,124,192,120,12,248,48,0,198,204,24,48,102,198,0,0,
56,108,56,118,220,204,118,0,24,24,48,0,0,0,0,0,
6,12,24,24,24,12,6,0,48,24,12,12,12,24,48,0,
0,102,60,255,60,102,0,0,0,24,24,126,24,24,0,0,
0,0,0,0,48,48,96,0,0,0,0,0,126,0,0,0,
0,0,0,0,0,96,96,0,6,12,24,48,96,192,128,0,
56,108,198,198,198,108,56,0,24,56,24,24,24,24,60,0,
248,12,12,56,96,192,252,0,248,12,12,56,12,12,248,0,
28,60,108,204,254,12,12,0,252,192,248,12,12,12,248,0,
60,96,192,248,204,204,120,0,252,12,24,48,96,192,192,0,
120,204,204,120,204,204,120,0,120,204,204,124,12,12,120,0,
0,96,96,0,96,96,0,0,0,96,96,0,96,96,192,0,
12,24,48,96,48,24,12,0,0,0,252,0,0,252,0,0,
96,48,24,12,24,48,96,0,124,6,6,28,24,0,24,0,
124,198,222,222,222,192,120,0,48,120,204,204,252,204,204,0,
248,204,204,248,204,204,248,0,124,192,192,192,192,192,124,0,
248,204,204,204,204,204,248,0,252,192,192,248,192,192,252,0,
252,192,192,248,192,192,192,0,124,192,192,192,220,204,124,0,
204,204,204,252,204,204,204,0,60,24,24,24,24,24,60,0,
12,12,12,12,12,12,248,0,198,204,216,240,216,204,198,0,
192,192,192,192,192,192,252,0,198,238,254,214,198,198,198,0,
198,230,246,222,206,198,198,0,120,204,204,204,204,204,120,0,
248,204,204,248,192,192,192,0,120,204,204,204,204,216,108,0,
248,204,204,248,240,216,204,0,124,192,192,120,12,12,248,0,
252,48,48,48,48,48,48,0,204,204,204,204,204,204,124,0,
204,204,204,204,204,120,48,0,198,198,198,214,254,238,198,0,
198,198,108,56,108,198,198,0,204,204,204,120,48,48,48,0,
254,12,24,48,96,192,254,0,120,96,96,96,96,96,120,0,
192,96,48,24,12,6,0,0,120,24,24,24,24,24,120,0,
24,60,102,66,0,0,0,0,0,0,0,0,0,0,255,0,
192,192,96,0,0,0,0,0,0,0,120,12,124,204,124,0,
192,192,248,204,204,204,248,0,0,0,124,192,192,192,124,0,
12,12,124,204,204,204,124,0,0,0,120,204,252,192,124,0,
60,96,96,248,96,96,96,0,0,0,124,204,124,12,248,0,
192,192,248,204,204,204,204,0,24,0,56,24,24,24,60,0,
24,0,24,24,24,24,240,0,192,192,204,216,240,216,204,0,
56,24,24,24,24,24,60,0,0,0,236,214,214,214,214,0,
0,0,248,204,204,204,204,0,0,0,120,204,204,204,120,0,
0,0,248,204,204,248,192,0,0,0,124,204,204,124,12,0,
0,0,220,224,192,192,192,0,0,0,124,192,120,12,248,0,
96,96,252,96,96,96,60,0,0,0,204,204,204,204,124,0,
0,0,204,204,204,120,48,0,0,0,214,214,214,214,110,0,
0,0,198,108,56,108,198,0,0,0,204,204,124,12,248,0,
0,0,252,24,48,96,252,0,28,48,48,224,48,48,28,0,
24,24,24,0,24,24,24,0,224,48,48,28,48,48,224,0,
118,220,0,0,0,0,0,0,252,252,252,252,252,252,252,0};




/*
=================
=
= I_Print8
=
=================
*/

void I_Print8 (int x, int y, char *string)
{
	int		c;
	byte	*source,*dest;
	
	dest = debugscreen + (y<<8) + x;
	
	if (y > 224/8)
		return;
		
	while ((c=*string++) && x<32)
	{
		if (c < 32 || c>= 128)
			continue;
		source = font8 + ((c-32)<<3);
		
		dest[0] = *source++;
		dest[32] = *source++;
		dest[32*2] = *source++;
		dest[32*3] = *source++;
		dest[32*4] = *source++;
		dest[32*5] = *source++;
		dest[32*6] = *source;
		
		dest++;
		x++;
	}
}


/*
================
=
= I_Error
=
================
*/

char errormessage[80];

void I_Error (char *error, ...) 
{
	D_vsprintf (errormessage,error,((int *)&error)+1);
	I_Print8 (0,25,errormessage);
	debugscreenactive = true;
	I_Update ();
	while (1)
	;
} 

/* 
================ 
= 
= I_Init  
=
= Called after all other subsystems have been started
================ 
*/ 
 
void I_Init (void) 
{	 
	int	i;
	
	palette8 = W_CacheLumpName ("CRYPAL",PU_STATIC);
	
	for (i=0 ; i<256 ; i++)
		((pixel_t *)0xf00400)[i] = palette8[i];

} 


void I_DrawSbar (void)
{
	jagobj_t	*frag;
	int			x,y;
	unsigned	*source, *dest;
	
	W_ReadLump (W_GetNumForName ("STBAR"), sbar);	/* background */
	if (netgame != gt_deathmatch)
		return;
		
	frag = W_CacheLumpName ("STBARNET",PU_STATIC);
	
	dest = (unsigned *)((byte *)sbar->data + 240);
	source = (unsigned *)frag->data;
	
	for (y=0 ; y< 40 ; y++)
	{
		for (x=0 ; x<20 ; x++)
			dest[x] = source[x];
		source += 20;
		dest += 80;
	}
	
	Z_Free (frag);
}




boolean	I_RefreshCompleted (void)
{
	return gpufinished;
}

boolean	I_RefreshLatched (void)
{
	return phasetime[3] != 0;
}



/* 
==================== 
= 
= I_WadBase  
=
= Return a pointer to the wadfile.  In a cart environment this will
= just be a pointer to rom.  In a simulator, load the file from disk.
==================== 
*/ 
 
byte *I_WadBase (void)
{
	return (byte *)0x840000; 
}


/* 
==================== 
= 
= I_ZoneBase  
=
= Return the location and size of the heap for dynamic memory allocation
==================== 
*/ 
 
#define	STARTHEAP	0x80000
#define	ENDHEAP		0x1c8000

byte *I_ZoneBase (int *size)
{
	*size = ENDHEAP-STARTHEAP;               /* leave 64k for stack  */
	return (byte *)STARTHEAP; 
}


/* 
==================== 
= 
= I_ReadControls 
= 
==================== 
*/ 
 
#define TICSCALE        2
 
int I_ReadControls (void) 
{ 
	static int oldticcount;
	int		stoptic, i, cumulative;
	
	stoptic = ticcount;
	if (stoptic - oldticcount > ticrate)
		oldticcount = stoptic - ticrate;
	if (oldticcount >= stoptic)
		oldticcount = stoptic - 1;
	cumulative = 0;	
	for (i=oldticcount ; i<stoptic ; i++)
	{
		cumulative |= joypad[i&31];
	}

	oldticcount = stoptic;
	
	return cumulative;
	
/*	return joystick1; */
} 

int	I_GetTime (void)
{
	return ticcount;
}
 
/*
==============================================================================

							MATH CODE

==============================================================================
*/

int __mulsi3 (int a, int b)
{
	int             sign;
	unsigned        short a1,a2,b1,b2;
    unsigned    c;
	
	sign = a^b;
	if (a&0x80000000)
		a = -a;
	if (b&0x80000000)
		b = -b;
	a1 = a&0xffff;
	a2 = a>>16;
	b1 = b&0xffff;
	b2 = b>>16;
	c = a1*b1;
	c += (a2*b1)<<16;
	c += (b2*a1)<<16;
	if (sign < 0)
		c = -c;
    return c;
}

unsigned __udivsi3 (unsigned aa, unsigned bb)
{
	unsigned        bit;
	unsigned        c;
	
	if ( (aa>>30) >= bb)
		return 0x7fffffff;
		
	bit = 1;
	while (aa > bb && bb < 0x80000000)
	{
		bb <<= 1;
		bit <<= 1;
	}
	
	c = 0;
	
	do
	{
		if (aa >= bb)
		{
			aa -=bb;
			c |= bit;
		}
		bb >>=1;
		bit >>= 1;
	} while (bit && aa);
	
	return c;
}


int     __divsi3 (int a, int b)
{
	unsigned        aa,bb,c;
	int                     sign;
	
	sign = a^b;
	if (a<0)
		aa = -a;
	else
		aa = a;
	if (b<0)
		bb = -b;
	else
		bb = b;
	
	c = __udivsi3 (aa,bb);
	
	if (sign < 0)
		c = -(int)c;
    return c;
}

unsigned __umodsi3 (unsigned aa, unsigned bb)
{
	unsigned        div;
	
	div = aa/bb;
	return aa - (div*bb);
}

unsigned __modsi3 (unsigned aa, unsigned bb)
{
	return __umodsi3 (aa,bb);
}


#if 1


/* 
================ 
= 
= FixedMul  
=
= Perform a signed 16.16 by 16.16 mutliply
================ 
*/ 
 
fixed_t FixedMul (fixed_t a, fixed_t b) 
{ 
/* this code is very slow, but exactly simulates the proper assembly */
/* operation that C doesn't let you represent well */
	int             sign; 
	unsigned        a1,a2,b1,b2; 
    unsigned    	c; 
	 
	sign = a^b; 
	if (a<0) 
		a = -a; 
	if (b<0) 
		b = -b; 
	a1 = a&0xffff; 
	a2 = (unsigned)a>>16; 
	b1 = b&0xffff; 
	b2 = (unsigned)b>>16; 
	c = (a1*b1) >> 16; 
	c += a2*b1; 
	c += b2*a1; 
	c += (b2*a2)<<16; 
	if (sign < 0) 
		c = -c; 
    return c; 
} 
 
/* 
================ 
= 
= FixedDiv  
= 
= Perform a signed 16.16 by 16.16 divide (a/b)
================ 
*/ 
 
fixed_t FixedDiv (fixed_t a, fixed_t b) 
{ 
/* this code is VERY slow, but exactly simulates the proper assembly */
/* operation that C doesn't let you represent well */
	unsigned        aa,bb,c; 
	unsigned        bit; 
	int             sign; 
	 
	sign = a^b; 
	if (a<0) 
		aa = -a; 
	else 
		aa = a; 
	if (b<0) 
		bb = -b; 
	else 
		bb = b; 
	if ( (aa>>14) >= bb) 
		return sign<0 ? MININT : MAXINT; 
	bit = 0x10000; 
	while (aa > bb) 
	{ 
		bb <<= 1; 
		bit <<= 1; 
	} 
	c = 0; 
	 
	do 
	{ 
		if (aa >= bb) 
		{ 
			aa -=bb; 
			c |= bit; 
		} 
		aa <<=1; 
		bit >>= 1; 
	} while (bit && aa); 
	 
	if (sign < 0) 
		c = -c; 
    return c; 
} 
 
#endif

/*
==============================================================================

						TEXTURE MAPPING CODE

==============================================================================
*/

byte	*framebuffer_p;

/* 
================== 
= 
= I_DrawColumn 
= 
= Source is the top of the column to scale 
= 
================== 
*/ 
 
void I_DrawColumn (int dc_x, int dc_yl, int dc_yh, int light, fixed_t frac, fixed_t fracstep, inpixel_t *dc_source) 
{ 
	int			count; 
	pixel_t		*dest;
 
	count = dc_yh - dc_yl; 
	if (count < 0) 
		return; 
				 
#ifdef RANGECHECK 
	if ((unsigned)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT) 
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x); 
#endif 
 
	dest = (pixel_t *)(framebuffer_p + dc_yl*320 + dc_x*2);  
	
	do 
	{ 
		*dest = dc_source[(frac>>FRACBITS)&127]; 
		dest += SCREENWIDTH; 
		frac += fracstep; 
	} while (count--); 
} 


/* 
================ 
= 
= I_DrawSpan 
= 
================ 
*/ 
 
void I_DrawSpan (int ds_y, int ds_x1, int ds_x2, int light, fixed_t ds_xfrac, fixed_t ds_yfrac, fixed_t ds_xstep, fixed_t ds_ystep, inpixel_t *ds_source) 
{ 
	fixed_t		xfrac, yfrac; 
	pixel_t		*dest; 
	int			count, spot; 
	
#ifdef RANGECHECK 
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=SCREENWIDTH  
	|| (unsigned)ds_y>SCREENHEIGHT) 
		I_Error ("R_DrawSpan: %i to %i at %i",ds_x1,ds_x2,ds_y); 
#endif 
	 
	xfrac = ds_xfrac; 
	yfrac = ds_yfrac; 
	 
	dest = (pixel_t *)(framebuffer_p + ds_y*320 + ds_x1*2);
	count = ds_x2 - ds_x1; 

	do 
	{ 
		spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63); 
		*dest++ = ds_source[spot]; 
		xfrac += ds_xstep; 
		yfrac += ds_ystep; 
	} while (count--); 
} 

/*=========================================================================== */

/* 
==================== 
= 
= I_Update 
=
= Display the current framebuffer
= If < 1/15th second has passed since the last display, busy wait.
= 15 fps is the maximum frame rate, and any faster displays will
= only look ragged.
=
= When displaying the automap, use full resolution, otherwise use
= wide pixels
==================== 
*/ 

#define	GPULINE		(BASEORGY+SCREENHEIGHT+1)
int		lastticcount;
int		lasttics;

extern	pixel_t			shadepixel;

void I_Update (void) 
{
	int				*worklist_p;		/* list currently being built */
	unsigned		link;
	unsigned		pwidth;
	unsigned		temp;
	int				delta;
			
/* */
/* set up new list */
/* */
	workingscreen = screens[workpage];
	worklist_p = listbuffers[worklist];
	workpage ^= 1;
	worklist ^= 1;
	
/* make working visible now */

	delta = (byte *)worklist_p - (byte *)listbuffer;
	
/* */
/* stupid branch objects */
/* */
	link = (int)stopobj>>3;
	worklist_p[0] =	link>>8;
	worklist_p[1] =	(link<<24) + 0x00008fdb;	/* first branch object */
	
	worklist_p[2] =	link>>8;
	worklist_p[3] =	(link<<24) + 0x000040cb;	/* second branch object */

/* */
/* branch < gpuline */
/* */
	link = (int)( (byte *)&worklist_p[12] - delta)>>3;
	worklist_p[4] = 
		(link>>8);							/* link */
		
	worklist_p[5] =
		(link<<24)							/* link */
		+ (1<<14)							/* branch ypos > VC */
		+ (224<<14)							/* height */
		+ (GPULINE<<4)						/* ypos */
		+ 3;								/* branch type */

/* */
/* branch > gpuline */
/* */
	worklist_p[6] = 
		(link>>8);							/* link */
		
	worklist_p[7] =
		(link<<24)							/* link */
		+ (2<<14)							/* branch ypos < VC */
		+ (224<<14)							/* height */
		+ (GPULINE<<4)						/* ypos */
		+ 3;								/* branch type */

/* */
/* gpu object */
/* */
	worklist_p[8] = 0;		
	worklist_p[9] =
		((GPULINE)<<4)						/* ypos */
		+ 2;								/* gpu type */

/* */
/* nop branch */
/* */
	link = (int)stopobj>>3;
	worklist_p[10] =	link>>8;
	worklist_p[11] =	(link<<24) + 0x00008fdb;	/* first branch object */

/* */
/* background object */
/* */

/* skip the color add object if it doesn't do anything */
	if (shadepixel)
		link = (int)( (byte *)&worklist_p[16] - delta)>>3;
	else
		link = (int)( (byte *)&worklist_p[20] - delta)>>3;
	pwidth = SCREENWIDTH/4;

	worklist_p[12] = 
		((int)workingscreen<<8)			/* data pointer */
		+ (link>>8);						/* link */
		
	worklist_p[13] =
		(link<<24)							/* link */
		+ (SCREENHEIGHT<<14)				/* height */
		+ (BASEORGY<<4)						/* ypos */
		+ 0;								/* bitmap type */
		
		worklist_p[14] =
			(0<<(49-32))						/* firstpix */
			+ (0<<(48-32))						/* release */
			+ (0<<(47-32))						/* transparent */
			+ (0<<(46-32))						/* add to buffer */
			+ (0<<(45-32))						/* reflect */
			+ (0<<(38-32))						/* color index */
			+ ((pwidth)>>4);					/* iwidth */
	
		temp =
			((pwidth)<<28)						/* iwidth */
			+ ((pwidth)<<18)					/* dwidth */
			+ (1<<15)							/* pitch */
			+ (4<<12)							/* depth */
			+ BASEORGX;							/* xpos */

		worklist_p[15] = temp;

/* */
/* color add object */
/* */
	link = (int)( (byte *)&worklist_p[20] - delta)>>3;

	worklist_p[16] = 
		((int)screenshade<<8)				/* data pointer */
		+ (link>>8);						/* link */
		
	worklist_p[17] =
		(link<<24)							/* link */
		+ (180<<14)							/* height */
		+ ((BASEORGY)<<4)					/* ypos */
		+ 0;								/* bitmap type */
		
	worklist_p[18] =
		(0<<(49-32))						/* firstpix */
		+ (0<<(48-32))						/* release */
		+ (0<<(47-32))						/* transparent */
		+ (1<<(46-32))						/* add to buffer */
		+ (0<<(45-32))						/* reflect */
		+ (0<<(38-32))						/* color index */
		+ (40>>4);							/* iwidth */

	worklist_p[19] =
		(40<<28)							/* iwidth */
		+ (0<<18)							/* dwidth */
		+ (0<<15)							/* pitch */
		+ (4<<12)							/* depth */
		+ BASEORGX;							/* xpos */
	
/* */
/* status bar background */
/* */
	link = (int)( (byte *)&worklist_p[24] - delta)>>3;
	pwidth = 320/8;

	worklist_p[20] = 
		((int)sbar->data<<8)				/* data pointer */
		+ (link>>8);						/* link */
		
	worklist_p[21] =
		(link<<24)							/* link */
		+ (sbar->height<<14)				/* height */
		+ ((BASEORGY+SCREENHEIGHT+1)<<4)	/* ypos */
		+ 0;								/* bitmap type */
		
	worklist_p[22] =
		(0<<(49-32))						/* firstpix */
		+ (0<<(48-32))						/* release */
		+ (0<<(47-32))						/* transparent */
		+ (0<<(46-32))						/* add to buffer */
		+ (0<<(45-32))						/* reflect */
		+ (0<<(38-32))						/* color index */
		+ ((pwidth)>>4);					/* iwidth */

	worklist_p[23] =
		((pwidth)<<28)						/* iwidth */
		+ ((pwidth)<<18)					/* dwidth */
		+ (1<<15)							/* pitch */
		+ (3<<12)							/* depth */
		+ BASEORGX*2;						/* xpos */

/* */
/* status bar foreground */
/* */

/* skip debug screen if not active */
	if (debugscreenactive)
		link = (int)( (byte *)&worklist_p[28] - delta)>>3;
	else
		link = (int)stopobj>>3;

	worklist_p[24] = 
		((int)sbartop<<8)					/* data pointer */
		+ (link>>8);						/* link */
		
	worklist_p[25] =
		(link<<24)							/* link */
		+ (sbar->height<<14)				/* height */
		+ ((BASEORGY+SCREENHEIGHT+1)<<4)	/* ypos */
		+ 0;								/* bitmap type */
		
	worklist_p[26] =
		(0<<(49-32))						/* firstpix */
		+ (0<<(48-32))						/* release */
		+ (1<<(47-32))						/* transparent */
		+ (0<<(46-32))						/* add to buffer */
		+ (0<<(45-32))						/* reflect */
		+ (0<<(38-32))						/* color index */
		+ ((pwidth)>>4);					/* iwidth */

	worklist_p[27] =
		((pwidth)<<28)						/* iwidth */
		+ ((pwidth)<<18)					/* dwidth */
		+ (1<<15)							/* pitch */
		+ (3<<12)							/* depth */
		+ BASEORGX*2;						/* xpos */

/* */
/* debug screen object */
/* */

	link = (int)stopobj>>3;
	pwidth = 256/64;

	worklist_p[28] = 
		((int)debugscreen<<8)				/* data pointer */
		+ (link>>8);						/* link */
		
	worklist_p[29] =
		(link<<24)							/* link */
		+ (216<<14)							/* height */
		+ ((BASEORGY)<<4)					/* ypos */
		+ 0;								/* bitmap type */
		
	worklist_p[30] =
		(0<<(49-32))						/* firstpix */
		+ (0<<(48-32))						/* release */
		+ (1<<(47-32))						/* transparent */
		+ (0<<(46-32))						/* add to buffer */
		+ (0<<(45-32))						/* reflect */
		+ (0x70<<(38-32))					/* color index */
		+ (pwidth>>4);						/* iwidth */

	worklist_p[31] =
		(pwidth<<28)						/* iwidth */
		+ (pwidth<<18)						/* dwidth */
		+ (1<<15)							/* pitch */
		+ (0<<12)							/* depth */
		+ BASEORGX;							/* xpos */


/* */
/* wait until on the third tic after last display */
/* */
	do
	{
		junk = ticcount;
	} while (junk-lastticcount < 3);

/* */
/* start using the list */
/* */
	isrvmode = 0xc1 + (7<<9);		/* 160 * 224, 16 bit color */
	readylist_p = worklist_p;
	do
	{
		junk = (int)displaylist_p;
	} while (junk != (int)readylist_p);
	lasttics = ticcount - lastticcount;
	lastticcount = ticcount;
}
 

/*
====================
=
= I_TempBuffer
=
= return a pointer to a 64k or so temp work buffer for level setup uses
= (non-displayed frame buffer)
=
====================
*/

byte	*I_TempBuffer (void)
{
	byte *buf = (byte *)screens[workpage];
	D_memset(buf, 0, 64*1024);
	return buf;
}

pixel_t	*I_FrameBuffer (void)
{
	return screens[!workpage];
}

pixel_t* I_OverwriteBuffer(void)
{
	return NULL;
}

pixel_t	*I_WorkBuffer (void)
{
	return screens[workpage];
}

/*
===============================================================================

					DOUBLE BUFFERED DRAWING FUNCTIONS

===============================================================================
*/

/* double buffered display support functions */

byte	*bufferpage;		/* draw here */
byte	*displaypage;		/* copied to here when finished */

extern	int             ZERO,zero; 

/*
=======================
=
= DoubleBufferSetup
=
= Set up the double buffered work screens
=======================
*/

extern int cy;

void DoubleBufferSetup (void)
{
/* */
/* set up new list */
/*	 */
	while (!I_RefreshCompleted () )
	;
	
	bufferpage = (byte *)workingscreen = screens[workpage];
	displaypage = (byte *)screens[!workpage];

	D_memset (bufferpage,0,320*200);
	D_memset (debugscreen,0,32*224);
	cy = 4;
}

/*
===============
=
= EraseBlock
=
===============
*/

void EraseBlock (int x, int y, int width, int height)
{
	if (x<0)
	{
		width += x;
		x = 0;
	}
	if (y<0)
	{
		height += y;
		y = 0;
	}
	if (x+width > 320)
		width = 320-x;
	if (y+height > 200)
		height = 200-y;
	
	if (width < 1 || height < 1)
		return;

#ifdef JAGUAR
		
	*(int *)0xf02200 = (int)bufferpage;	/* a1 base pointer */
		
	*(int *)0xf02204 = (3<<3)		 	/* 8 bit pixels */
					+ (33<<9)			/* 320 wide */
					+ (1<<16)			/* add 1 x */
					;					/* a1 flags */
					
	*(int *)0xf0220c = (y<<16)+x;		/* a1 pixel pointers */

	*(int *)0xf02210 = (1<<16)+ ((-width)&0xffff);	/* a1 pixel step */

	*(int *)0xf02240 = zero;
	*(int *)0xf02244 = ZERO;	/* source data register */
	
	*(int *)0xf0223c = (height<<16) + width;	/* count */

	*(int *)0xf02238 =
				(1<<9)					/* add a1 step value in outer */
				+ (12<<21)				/* copy source */
				;
#else
{
	byte	*dest;

	dest = bufferpage+320*y+x;
	for ( ; height ; height--)
	{
		D_memset (dest,0,width);
		dest += 320;
	}
}
#endif

}

void DrawJagobj (jagobj_t *jo, int x, int y)
{
	int		srcx, srcy;
	int		width, height;
	int		rowsize;
	
	width = rowsize = BIGSHORT(jo->width);
	height = BIGSHORT(jo->height);
	srcx = 0;
	srcy = 0;
	
	if (x<0)
	{
		width += x;
		srcx = -x;
		x = 0;
	}
	if (y<0)
	{
		srcy = -y;
		height += y;
		y = 0;
	}
	if (x+width > 320)
		width = 320-x;
	if (y+height > 200)
		height = 200-y;
	
	if (width < 1 || height < 1)
		return;
	
#ifdef JAGUAR
		
	*(int *)0xf02200 = (int)bufferpage;	/* a1 base pointer */
	
	*(int *)0xf02204 = (3<<3)		 	/* 8 bit pixels */
					+ (33<<9)			/* 320 wide */
					+ (1<<16)			/* add 1 x */
					;					/* a1 flags */
					
	*(int *)0xf0220c = (y<<16)+x;		/* a1 pixel pointers */

	*(int *)0xf02210 = (1<<16)+ ((-width)&0xffff);	/* a1 pixel step */


	*(int *)0xf02224 = (int)jo->data;	/* a2 base pointer */
	
	*(int *)0xf02228 = (3<<3)		 	/* 8 bit pixels */
					+ (1<<16)			/* add 1 x */
					;					/* a1 flags */
					
	*(int *)0xf02230 = srcy*rowsize+srcx;		/* a2 pixel pointers */

	*(int *)0xf02234 = rowsize-width;			/* a2 pixel step */


	
	*(int *)0xf0223c = (height<<16) + width;	/* count */

	*(int *)0xf02238 =
				(1<<0)					/* source read */
				+(1<<9)					/* add a1 step value in outer */
				+(1<<10)				/* add a2 step value in outer */
				+ (12<<21)				/* copy source */
				;

#else
{
	byte	*dest;
	byte	*source;

	source = jo->data + srcx + srcy*rowsize;

	dest = bufferpage+320*y+x;
	for ( ; height ; height--)
	{
		D_memcpy (dest,source,width);
		source += rowsize;
		dest += 320;
	}
}
#endif
}

void DoubleBufferObjList (void);

/*
===================
=
= UpdateBuffer
=
===================
*/

void UpdateBuffer (void)
{
/* copy entire page with blitter */
	
	*(int *)0xf02200 = (int)displaypage;/* a1 base pointer */
	*(int *)0xf02204 = (5<<3)		 	/* 24 bit pixels, phrase mode */
					;					/* a1 flags */
	*(int *)0xf0220c = ZERO;			/* a1 pixel pointers */
		
	*(int *)0xf02224 = (int)bufferpage;	/* a2 base pointer */
	*(int *)0xf02228 = (5<<3)		 	/* 24 bit pixels, phrase mode */
					;					/* a2 flags */
	*(int *)0xf02230 = zero;			/* a2 pixel pointers */
		
	
	*(int *)0xf0223c = 0x10000 + (320*200/4);	/* 32000 words */


DoubleBufferObjList ();

#if 1
	*(int *)0xf02238 = (1<<0)			/* read source */
				+ (12<<21)				/* copy source */
				;
					
#else
	D_memcpy (displaypage, bufferpage, 320*200);
#endif
}



void DoubleBufferObjList (void)
{	
	int		link;
	int		pwidth;
	int		delta;
	jagobj_t	*backgroundpic;

	worklist_p = listbuffers[worklist];
		
/* */
/* background object */
/* */
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));

	work_p = worklist_p + 4;
	delta = (byte *)worklist_p - (byte *)listbuffer;
	
	link = (int)(16+(byte *)work_p -  delta)>>3;
	pwidth = backgroundpic->width/16;

	*work_p++ = 
		((int)backgroundpic->data<<8)		/* data pointer */
		+ (link>>8);						/* link */
		
	*work_p++ =
		(link<<24)							/* link */
		+ (backgroundpic->height<<14)		/* height */
		+ ((BASEORGY-8)<<4)					/* ypos */
		+ 0;								/* bitmap type */
		
	*work_p++ =
		(0<<(49-32))						/* firstpix */
		+ (0<<(48-32))						/* release */
		+ (0<<(47-32))						/* transparent */
		+ (0<<(46-32))						/* add to buffer */
		+ (0<<(45-32))						/* reflect */
		+ (40<<(38-32))						/* color index */
		+ ((pwidth)>>4);					/* iwidth */

	*work_p++ =
		((pwidth)<<28)						/* iwidth */
		+ ((pwidth)<<18)					/* dwidth */
		+ (1<<15)							/* pitch */
		+ (2<<12)							/* depth */
		+ (0);								/* xpos */

/* */
/* transparent foreground object */
/* */
	if (debugscreenactive)
		link = (int)(16+(byte *)work_p -  delta)>>3;
	else
		link = (int)stopobj>>3;
	pwidth = 320/8;

	*work_p++ = 
		(((int)displaypage)<<8)				/* data pointer */
		+ (link>>8);						/* link */
		
	*work_p++ =
		(link<<24)							/* link */
		+ (200<<14)							/* height */
		+ ((BASEORGY+8)<<4)					/* ypos */
		+ 0;								/* bitmap type */
		
	*work_p++ =
		(0<<(49-32))						/* firstpix */
		+ (0<<(48-32))						/* release */
		+ (1<<(47-32))						/* transparent */
		+ (0<<(46-32))						/* add to buffer */
		+ (0<<(45-32))						/* reflect */
		+ (0<<(38-32))						/* color index */
		+ (pwidth>>4);						/* iwidth */

	*work_p++ =
		(pwidth<<28)						/* iwidth */
		+ (pwidth<<18)						/* dwidth */
		+ (1<<15)							/* pitch */
		+ (3<<12)							/* depth */
		+ 15;								/* xpos */

/* */
/* debug screen object */
/* */
	link = (int)(stopobj)>>3;
	pwidth = 256/64;

	*work_p++ =
		((int)debugscreen<<8)				/* data pointer */
		+ (link>>8);						/* link */
		
	*work_p++ =
		(link<<24)							/* link */
		+ (216<<14)							/* height */
		+ ((BASEORGY)<<4)					/* ypos */
		+ 0;								/* bitmap type */
		
	*work_p++ =
		(0<<(49-32))						/* firstpix */
		+ (0<<(48-32))						/* release */
		+ (1<<(47-32))						/* transparent */
		+ (0<<(46-32))						/* add to buffer */
		+ (0<<(45-32))						/* reflect */
		+ (0x70<<(38-32))					/* color index */
		+ (pwidth>>4);						/* iwidth */

	*work_p++ =
		(pwidth<<28)						/* iwidth */
		+ (pwidth<<18)						/* dwidth */
		+ (1<<15)							/* pitch */
		+ (0<<12)							/* depth */
		+ BASEORGX;							/* xpos */
	
/* */
/* stupid branch objects */
/* */
	
	worklist_p[0] =	link>>8;
	worklist_p[1] =	(link<<24) + branch1;	/* first branch object */
	
	worklist_p[2] =	link>>8;
	worklist_p[3] =	(link<<24) + branch2;	/* second branch object */

/* */
/* start using the list */
/* */
	isrvmode = 0xc1 + (3<<9);		/* 320 * 224, 16 bit color */
	readylist_p = worklist_p;
	while (displaylist_p != readylist_p)
	;
	
	worklist ^= 1;
}


/*
===============================================================================

						EEPROM

===============================================================================
*/


#define	IDWORD	(('D'<<8)+'1')


unsigned short eeread (int address);
int eewrite (int data, int address);

#define	EEWORDS	8		/* MUST BE EVEN!!!!!! */

unsigned short eeprombuffer[EEWORDS];

void ClearEEProm (void)
{
	startskill = sk_medium;
	startmap = 1;
	sfxvolume = 200;
	musicvolume = 128;
	controltype = 0;
	maxlevel = 1;

	WriteEEProm ();	
}

void vsync(void)
{
	int	start;
	
	start = ticcount;
	do
	{
		junk = ticcount;
	} while (junk == start);
}

void ReadEEProm (void)
{
	int		i;
	unsigned short		checksum;
	
	checksum = 12345;
	
	vsync ();			/* so joystick reads don't interfere */
	for (i=0 ; i<EEWORDS ; i++)
	{
		eeprombuffer[i] = eeread(i);
		if (i != EEWORDS-1)
			checksum += eeprombuffer[i];
	}
	
	if (checksum != eeprombuffer[EEWORDS-1])
	{	/* checksum failure, clear eeprom */
		ClearEEProm ();
	}
	
	startskill = eeprombuffer[1];
	if (startskill > sk_nightmare)
		ClearEEProm ();
	startmap = eeprombuffer[2];
	if (startmap > 26)
		ClearEEProm ();
	sfxvolume = eeprombuffer[3];
	if (sfxvolume > 255)
		ClearEEProm ();
	musicvolume = eeprombuffer[4];
	if (musicvolume > 255)
		ClearEEProm ();
	controltype = eeprombuffer[5];
	if (controltype > 5)
		ClearEEProm ();
	maxlevel = eeprombuffer[6];
	if (maxlevel > 25)
		ClearEEProm ();
}


void WriteEEProm (void)
{
	int		i;

	eeprombuffer[0] = IDWORD;
	eeprombuffer[1] = startskill;
	eeprombuffer[2] = startmap;
	eeprombuffer[3] = sfxvolume;
	eeprombuffer[4] = musicvolume;
	eeprombuffer[5] = controltype;
	eeprombuffer[6] = maxlevel;

	eeprombuffer[EEWORDS-1] = 12345;
	
	for (i=0 ; i<EEWORDS-1 ; i++)
		eeprombuffer[EEWORDS-1] += eeprombuffer[i];
	
	for (i=0 ; i<EEWORDS ; i++)
	{
		if (!eewrite(eeprombuffer[i],i))
		{
			vsync ();
			eewrite(eeprombuffer[i],i);
		}
	}
}


/*
===============================================================================

						NETWORKING

===============================================================================
*/


#define	ASICLK	(*(volatile unsigned short *)0xf10034)
#define ASIDATA	(*(volatile unsigned short *)0xf10030)
#define ASICTRL	(*(volatile unsigned short *)0xf10032)
#define ASISTAT	(*(volatile unsigned short *)0xf10032)

#define	PCLK		26593900
#define	UCLK_9600	((PCLK/(16*9600))-1)
#define	UCLK_19200	((PCLK/(16*19200))-1)
#define	UCLK_115200	((PCLK/(16*115200))-1)

int GetSerialChar (void)
{
	unsigned	val;
	
reget:
	val = ASISTAT;
	if (val & (1<<15) )
	{	/* error */
		ASICTRL = (1<<6);	/* serial control: clear error */
		goto reget;
	}
	if (! (val & (1<<7) ) )
		return -1;			/* nothing available */
	
	val = ASIDATA;
	
	return val;
}

int WaitGetSerialChar (void)
{
	int		val;
	int		vblstop;
	
	vblstop = ticcount + 120;
	
	do
	{
		if (ticcount >= vblstop)
			return -1;				/* timeout */
		val = GetSerialChar ();
	} while (val == -1);
	
	return val;
}


void PutSerialChar (int data)
{
	unsigned	val;
	
	do
	{
		val = ASISTAT;
	} while (! (val & (1<<8)) );	/* wait for TBE */

	ASIDATA = data;
}

void wait (int tics)
{
	int		start;
	
	start = ticcount;
	do
	{
		junk = ticcount;
	} while (junk < start + tics);
}

/*
======================
=
= Player0Setup
=
======================
*/

void Player0Setup (void)
{
	int		val;
	int		idbyte;
	int		sendcount;
	
	sendcount = 0;
	idbyte = startmap + 24*startskill + 128*(starttype==2);
	
I_Print8 (1,1,"waiting...");
	consoleplayer = 0;
	
	/* wait until we see a 0x22 from other side */
	do
	{
		if (joystick1 == JP_OPTION)
		{
			starttype = gt_single;
			return;		/* abort */
		}
		
		wait (1);
		val = GetSerialChar ();
PrintHex (20,5,val);
		if (val == 0x22)
			return;		/* ready to go */

		PutSerialChar (idbyte);
PrintHex (20,6, idbyte);
sendcount++;
PrintHex (20,7, sendcount);
	} while (1);
}

/*
=================
=
= Player1Setup
=
=================
*/

void Player1Setup (void)
{	
	int			val, oldval;
/* */
/* wait for two identical id bytes, then start game */
/* */
I_Print8 (1,1,"heard");
	oldval = 999;
	do
	{
		if (joystick1 == JP_OPTION)
		{
			starttype = gt_single;
			return;		/* abort */
		}
		val = GetSerialChar ();
		if (val == -1)
			continue;
PrintHex (5,10,oldval);
PrintHex (15,10,val);
		if (val == oldval)
			break;
		oldval = val;
	} while (1);
	
	
	if (val > 128)
	{
		starttype = 2;
		val -= 128;
	}
	else
		starttype = 1;
	
	startskill = val/24;
	val %= 24;
	startmap = val;
	
/* we are player 1.  send an acknowledge byte */
	consoleplayer = 1;	

	PutSerialChar (0x22);
	PutSerialChar (0x22);
	
}


/*
======================
=
= I_NetSetup
=
======================
*/

void DrawSinglePlaque (jagobj_t *pl);

int		listen1, listen2;

void I_NetSetup (void)
{
	jagobj_t	*pl;
	
	DoubleBufferSetup ();
	UpdateBuffer ();
	
	pl = W_CacheLumpName ("connect", PU_STATIC);	
	DrawSinglePlaque (pl);
	Z_Free (pl);

	ASICLK = UCLK_115200;
	ASICTRL = (1<<6);
	ASICLK = UCLK_115200;
	ASICTRL = (1<<6);

/*	ASICLK = 0xffff;		// very slow */

	GetSerialChar ();
	GetSerialChar ();
	wait (1);
	GetSerialChar ();
	GetSerialChar ();

/* wait a bit  */
	wait (4);
	
/* if a character is allready waiting, we are player 1 */
	listen1 = GetSerialChar ();
	listen2 = GetSerialChar ();
		
	if (listen1 == -1 && listen2 == -1)
		Player0Setup ();
	else
		Player1Setup ();


/* wait a while and flush out the receive que */
	wait (5);
	
	GetSerialChar ();
	GetSerialChar ();
	GetSerialChar ();
	DoubleBufferSetup ();
	UpdateBuffer ();

}


/*
======================
=
= I_NetTransfer
=
======================
*/

void G_PlayerReborn (int player);

unsigned I_NetTransfer (unsigned buttons)
{
	int		val;
	byte	inbytes[6];
	byte	outbytes[6];
	byte	consistancy;
	int		i;
	
/* don't transmit during heavy blitter action */
#if 0
	do
	{
		junk = phasetime[8];
	} while (!junk);
#endif

	outbytes[0] = buttons>>24;
	outbytes[1] = buttons>>16;
	outbytes[2] = buttons>>8;
	outbytes[3] = buttons;
	
	consistancy = players[0].mo->x ^ players[0].mo->y ^ players[1].mo->x ^ players[1].mo->y;
	consistancy = (consistancy>>8) ^ consistancy ^ (consistancy>>16);
	
	outbytes[4] = consistancy;
	outbytes[5] = vblsinframe[consoleplayer];
	
	if (consoleplayer)
	{
/* player 1 waits before sending */
		for (i=0 ; i<=5 ; i++)
		{
			val = WaitGetSerialChar ();
			if (val == -1)
				goto reconnect;
			
			inbytes[i] = val;
			PutSerialChar (outbytes[i]);
		}
		vblsinframe[0] = inbytes[5];		/* take gamevbls from other player */
	}
	else
	{
/* player 0 sends first */
		for (i=0 ; i<=5 ; i++)
		{
			PutSerialChar (outbytes[i]);
			val = WaitGetSerialChar ();
			if (val == -1)
				goto reconnect;
			inbytes[i] = val;
		}
	}
	
/* */
/* check for consistancy error */
/* */
	if (inbytes[4] != outbytes[4])
	{
		jagobj_t	*pl;
	
		S_Clear ();
		pl = W_CacheLumpName ("neterror", PU_STATIC);	
		DrawPlaque (pl);
		Z_Free (pl);

		wait (200);
		goto reconnect;
	}
		
	val = (inbytes[0]<<24) + (inbytes[1]<<16) + (inbytes[2]<<8) + inbytes[3];
	
	return val;
	
/* */
/* reconnect */
/* */
reconnect:
	S_Clear ();

	if (consoleplayer)
		wait (15);		/* let player 0 wait again */
		
	I_NetSetup ();
	if (starttype == gt_single)
		Jag68k_main ();
	
	G_PlayerReborn (0);
	G_PlayerReborn (1);
	
	gameaction = ga_warped;
	ticbuttons[0] = ticbuttons[1] = oldticbuttons[0] = oldticbuttons[1] = 0;
	return 0;
}

/*============================================================================= */

extern int ticsinframe;

extern int checkpostics, shoottics;
extern int lasttics;

extern	int	playertics, thinkertics, sighttics, basetics, latetics;
extern	int	tictics;

extern	int		soundtics;

/*
===================
=
= I_DebugScreen
=
===================
*/

void I_DebugScreen(void)
{
#ifdef JAGUAR

#if 1
	PrintNumber(15, 1, vblsinframe);

	PrintNumber(15, 2, phasetime[8] - phasetime[0]);
	PrintNumber(15, 3, tictics);
	PrintNumber(15, 4, soundtics);
#endif

#if 1
	PrintNumber(15, 6, playertics);
	PrintNumber(15, 7, thinkertics);
	PrintNumber(15, 8, sighttics);
	PrintNumber(15, 9, basetics);
	PrintNumber(15, 10, latetics);
#endif

#if 0
	int	i;

	PrintNumber(15, 1, phasetime[8] - phasetime[0]);
	for (i = 0; i < 8; i++)
		PrintNumber(15, 3 + i, phasetime[i + 1] - phasetime[i]);
#endif


#endif

}