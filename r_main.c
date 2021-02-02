/* r_main.c */

#include "doomdef.h"
#include "r_local.h"

/*===================================== */

/* */
/* subsectors */
/* */
subsector_t		**vissubsectors, **lastvissubsector;

/* */
/* walls */
/* */
viswall_t	*viswalls, *lastwallcmd;

/* */
/* planes */
/* */
visplane_t	visplanes[MAXVISPLANES], *lastvisplane;

/* */
/* sprites */
/* */
vissprite_t	*vissprites, *lastsprite_p, *vissprite_p;

/* */
/* openings / misc refresh memory */
/* */
unsigned short	openings[MAXOPENINGS], *lastopening;

/* holds *vissubsectors[MAXVISSEC], spropening[SCREENWIDTH+1], spanstart[256] */
intptr_t 	*r_workbuf;

/*===================================== */

boolean		phase1completed;

pixel_t		*workingscreen;


fixed_t		viewx, viewy, viewz;
angle_t		viewangle;
fixed_t		viewcos, viewsin;
player_t	*viewplayer;

VINT			validcount = 1;		/* increment every time a check is made */
int			framecount;		/* incremented every frame */


boolean		fixedcolormap;

int			lightlevel;			/* fixed light level */
int			extralight;			/* bumped light from gun blasts */

/* */
/* sky mapping */
/* */
int			skytexture;


/* */
/* precalculated math */
/* */
angle_t		clipangle,doubleclipangle;
#ifndef MARS
fixed_t	*finecosine_ = &finesine_[FINEANGLES/4];
#endif

int t_ref_bsp, t_ref_prep, t_ref_segs, t_ref_planes, t_ref_sprites, t_ref_total;

r_texcache_t r_flatscache, r_wallscache;


/*
===============================================================================
=
= R_PointToAngle
=
===============================================================================
*/


extern	int	tantoangle[SLOPERANGE+1];

int SlopeDiv (unsigned num, unsigned den)
{
	unsigned ans;
	if (den < 512)
		return SLOPERANGE;
	ans = (num<<3)/(den>>8);
	return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{	
	int		x;
	int		y;
	
	x = x2 - x1;
	y = y2 - y1;
	
	if ( (!x) && (!y) )
		return 0;
	if (x>= 0)
	{	/* x >=0 */
		if (y>= 0)
		{	/* y>= 0 */
			if (x>y)
				return tantoangle[ SlopeDiv(y,x)];     /* octant 0 */
			else
				return ANG90-1-tantoangle[ SlopeDiv(x,y)];  /* octant 1 */
		}
		else
		{	/* y<0 */
			y = -y;
			if (x>y)
				return -tantoangle[SlopeDiv(y,x)];  /* octant 8 */
			else
				return ANG270+tantoangle[ SlopeDiv(x,y)];  /* octant 7 */
		}
	}
	else
	{	/* x<0 */
		x = -x;
		if (y>= 0)
		{	/* y>= 0 */
			if (x>y)
				return ANG180-1-tantoangle[ SlopeDiv(y,x)]; /* octant 3 */
			else
				return ANG90+ tantoangle[ SlopeDiv(x,y)];  /* octant 2 */
		}
		else
		{	/* y<0 */
			y = -y;
			if (x>y)
				return ANG180+tantoangle[ SlopeDiv(y,x)];  /* octant 4 */
			else
				return ANG270-1-tantoangle[ SlopeDiv(x,y)];  /* octant 5 */
		}
	}	
#ifndef LCC	
	return 0;
#endif
}


/*
===============================================================================
=
= R_PointOnSide
=
= Returns side 0 (front) or 1 (back)
===============================================================================
*/

int	R_PointOnSide (int x, int y, node_t *node)
{
	fixed_t	dx,dy;
	fixed_t	left, right;

	if (!node->dx)
	{
		if (x <= node->x)
			return node->dy > 0;
		return node->dy < 0;
	}
	if (!node->dy)
	{
		if (y <= node->y)
			return node->dx < 0;
		return node->dx > 0;
	}
	
	dx = (x - node->x);
	dy = (y - node->y);
	
	left = (node->dy>>16) * (dx>>16);
	right = (dy>>16) * (node->dx>>16);
	
	if (right < left)
		return 0;		/* front side */
	return 1;			/* back side */
}



/*
==============
=
= R_PointInSubsector
=
==============
*/

struct subsector_s *R_PointInSubsector (fixed_t x, fixed_t y)
{
	node_t	*node;
	int		side, nodenum;
	
	if (!numnodes)				/* single subsector is a special case */
		return subsectors;
		
	nodenum = numnodes-1;

	while (! (nodenum & NF_SUBSECTOR) )
	{
		node = &nodes[nodenum];
		side = R_PointOnSide (x, y, node);
		nodenum = node->children[side];
	}
	
	return &subsectors[nodenum & ~NF_SUBSECTOR];
	
}

/*============================================================================= */


/*
==============
=
= R_Init
=
==============
*/

void R_Init (void)
{
	size_t workbufsize;

D_printf ("R_InitData\n");
	R_InitData ();
D_printf ("Done\n");

	clipangle = xtoviewangle[0];
	doubleclipangle = clipangle*2;
	
	framecount = 0;
	viewplayer = &players[0];

	workbufsize = 0;
	if (MAXVISSSEC > workbufsize)
		workbufsize = MAXVISSSEC;
	if (SCREENWIDTH + 1 > workbufsize)
		workbufsize = SCREENWIDTH + 1;
	if (256 > workbufsize)
		workbufsize = 256;
	workbufsize *= sizeof(intptr_t);

	r_workbuf = Z_Malloc(workbufsize, PU_STATIC, NULL);

	R_InitTexCache(&r_flatscache, numflats);

	R_InitTexCache(&r_wallscache, numtextures);
}

/*
==============
=
= R_SetupTextureCaches
=
==============
*/
void R_SetupTextureCaches(void)
{
	int i;
	int numcflats, numcwalls;
	int zonefree, cachezonesize;
	void *margin;

	const int zonemargin = 4*1024;
	const int flatblocksize = sizeof(memblock_t) + sizeof(texcacheblock_t) + 64*64 + 100;
	const int texblocksize = sizeof(memblock_t) + sizeof(texcacheblock_t) + 64*128 + 100;

	// reset pointers from previous level
	for (i = 0; i < numtextures; i++)
		textures[i].data = NULL;
	for (i = 0; i < numflats; i++)
		flatpixels[i] = NULL;

	// functioning texture cache requires at least 8kb of ram
	zonefree = Z_LargestFreeBlock(mainzone);
	if (zonefree < zonemargin+flatblocksize)
		return;

	// see how many flats we can store
	cachezonesize = zonefree - zonemargin; // give the main zone some slack

	numcflats = cachezonesize / flatblocksize;
	if (numcflats > 4)
		numcflats = 4;

	numcwalls = (cachezonesize - numcflats*flatblocksize) / texblocksize;
	if (numcwalls <= 0 && numcflats > 2)
	{
		numcflats /= 2;
		numcwalls = (cachezonesize - numcflats * flatblocksize) / texblocksize;
	}
	if (numcwalls < 0)
		numcwalls = 0;

	if (numcflats + numcwalls == 0)
	{
		R_InitTexCacheZone(&r_flatscache, 0);
		R_InitTexCacheZone(&r_wallscache, 0);
		return;
	}

	cachezonesize = numcflats * flatblocksize + texblocksize * numcwalls;
	
	margin = Z_Malloc(zonemargin, PU_LEVEL, 0);

	R_InitTexCacheZone(&r_flatscache, numcflats * flatblocksize);

	R_InitTexCacheZone(&r_wallscache, numcwalls * texblocksize);

	Z_Free(margin);
}

/*============================================================================= */

int shadepixel;
#ifndef MARS
extern	int	workpage;
extern	pixel_t	*screens[2];	/* [SCREENWIDTH*SCREENHEIGHT];  */
#endif

/*
==================
=
= R_Setup
=
==================
*/

void R_Setup (void)
{
	int 		i;
	int		damagecount, bonuscount;
	player_t *player;
	int		shadex, shadey, shadei;
	unsigned short  *tempbuf;
	
/* */
/* set up globals for new frame */
/* */
#ifndef MARS
	workingscreen = screens[workpage];

	*(pixel_t  **)0xf02224 = workingscreen;	/* a2 base pointer */
	*(int *)0xf02234 = 0x10000;				/* a2 outer loop add (+1 y) */
	*(int *)0xf0226c = *(int *)0xf02268 = 0;		/* pattern compare */
#endif

	framecount++;	
	validcount++;
		
	viewplayer = player = &players[displayplayer];
	viewx = player->mo->x;
	viewy = player->mo->y;
	viewz = player->viewz;
	viewangle = player->mo->angle;

	viewsin = finesine(viewangle>>ANGLETOFINESHIFT);
	viewcos = finecosine(viewangle>>ANGLETOFINESHIFT);
		
	extralight = player->extralight << 6;
	fixedcolormap = player->fixedcolormap;
		
/* */
/* calc shadepixel */
/* */
	player = &players[consoleplayer];

	damagecount = player->damagecount;
	bonuscount = player->bonuscount;
	
	if (damagecount)
		damagecount += 10;
	if (bonuscount)
		bonuscount += 2;
	damagecount >>= 2;
	shadex = (bonuscount>>1) + damagecount;
	shadey = (bonuscount>>1) - damagecount;
	shadei = (bonuscount + damagecount)<<2;

	shadei += player->extralight<<3;

/* */
/* pwerups */
/* */
	if (player->powers[pw_invulnerability] > 60
	|| (player->powers[pw_invulnerability]&4) )
	{
		shadex -= 8;
		shadei += 32;
	}

	if (player->powers[pw_ironfeet] > 60
	|| (player->powers[pw_ironfeet]&4) )
		shadey += 7;

	if (player->powers[pw_strength] 
	&& (player->powers[pw_strength]< 64) )
		shadex += (8 - (player->powers[pw_strength]>>3) );


/* */
/* bound and store shades */
/* */
	if (shadex > 7)
		shadex = 7;
	else if (shadex < -8)
		shadex = -8;
	if (shadey > 7)
		shadey = 7;
	else if (shadey < -8)
		shadey = -8;
	if (shadei > 127)
		shadei = 127;
	else if (shadei < -128)
		shadei = -128;
		
	shadepixel = ((shadex<<12)&0xf000) + ((shadey<<8)&0xf00) + (shadei&0xff);

	tempbuf = (unsigned short *)I_WorkBuffer();

/* */
/* plane filling */
/*	 */
	tempbuf = (unsigned short *)(((int)tempbuf+2)&~1);
	tempbuf++; // padding
	for (i = 0; i < MAXVISPLANES; i++) {
		visplanes[i].open = tempbuf;
		tempbuf += SCREENWIDTH+2;
	}

	lastvisplane = visplanes+1;		/* visplanes[0] is left empty */

	tempbuf = (unsigned short *)(((int)tempbuf+4)&~3);
	viswalls = (void *)tempbuf;
	tempbuf += sizeof(*viswalls)*MAXWALLCMDS/sizeof(*tempbuf);

	lastwallcmd = viswalls;			/* no walls added yet */

	vissubsectors = (subsector_t **)&r_workbuf[0];
	lastvissubsector = vissubsectors;	/* no subsectors visible yet */
	
/*	 */
/* clear sprites */
/* */
	tempbuf = (unsigned short *)(((int)tempbuf+4)&~3);
	vissprites = (void *)tempbuf;
	tempbuf += sizeof(*vissprites)*MAXVISSPRITES/sizeof(*tempbuf);
	vissprite_p = vissprites;

	//tempbuf = (unsigned short*)(((int)tempbuf + 4) & ~3);
	//openings = tempbuf;
	//tempbuf += sizeof(*openings)*MAXOPENINGS/sizeof(*tempbuf);

	lastopening = openings;
#ifndef MARS
	phasetime[0] = samplecount;
#endif

	R_SetupTexCacheFrame(&r_flatscache);
	R_SetupTexCacheFrame(&r_wallscache);
}


void R_BSP (void);
void R_WallPrep (void);
void R_SpritePrep (void);
boolean R_LatePrep (void);
void R_Cache (void);
void R_SegCommands (void);
void R_DrawPlanes (void);
void R_Sprites (void);
void R_Update (void);


/*
==============
=
= R_RenderView
=
==============
*/

#ifndef MARS
int		phasetime[9] = {1,2,3,4,5,6,7,8,9};

extern	ref1_start;
extern	ref2_start;
extern	ref3_start;
extern	ref4_start;
extern	ref5_start;
extern	ref6_start;
extern	ref7_start;
extern	ref8_start;
#endif

extern	boolean	debugscreenactive;

void R_RenderPlayerView (void)
{

/* make sure its done now */
#if defined(JAGUAR) || defined(MARS)
	while (!I_RefreshCompleted ())
	;
#endif

/* */
/* initial setup */
/* */
	if (debugscreenactive)
		I_DebugScreen ();

	t_ref_total = I_GetTime();

	R_Setup ();

#ifndef JAGUAR
	t_ref_bsp = I_GetTime();
	R_BSP ();
	t_ref_bsp = I_GetTime() - t_ref_bsp;

	t_ref_prep = I_GetTime();
	R_WallPrep ();
	R_SpritePrep ();
/* the rest of the refresh can be run in parallel with the next game tic */
	if (R_LatePrep ())
		R_Cache ();
	t_ref_prep = I_GetTime() - t_ref_prep;

	t_ref_segs = I_GetTime();
	R_SegCommands ();
	t_ref_segs = I_GetTime() - t_ref_segs;

	t_ref_planes = I_GetTime();
	R_DrawPlanes ();
	t_ref_planes = I_GetTime() - t_ref_planes;

	t_ref_sprites = I_GetTime();
	R_Sprites ();
	t_ref_sprites = I_GetTime() - t_ref_sprites;

	t_ref_total = I_GetTime() - t_ref_total;

	R_Update();
#else

/* start the gpu running the refresh */
	phasetime[1] = 0;	
	phasetime[2] = 0;	
	phasetime[3] = 0;	
	phasetime[4] = 0;	
	phasetime[5] = 0;	
	phasetime[6] = 0;	
	phasetime[7] = 0;	
	phasetime[8] = 0;	
	gpufinished = zero;
	gpucodestart = (int)&ref1_start;

#if 0
	while (!I_RefreshCompleted () )
	;		/* wait for refresh to latch all needed data before */
			/* running the next tick */
#endif

#endif
}

