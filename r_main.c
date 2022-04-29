/* r_main.c */

#include "doomdef.h"
#include "r_local.h"
#ifdef MARS
#include "mars.h"
#include "marshw.h"
#endif

int16_t viewportWidth, viewportHeight;
int16_t centerX, centerY;
fixed_t centerXFrac, centerYFrac;
fixed_t stretch;
fixed_t stretchX;
VINT weaponYpos;
fixed_t weaponXScale;

detailmode_t detailmode = detmode_high;
VINT anamorphicview = 0;

drawcol_t drawcol;
drawcol_t drawfuzzycol;
drawcol_t drawcolnpo2;
drawspan_t drawspan;

short fuzzoffset[FUZZTABLE] =
{
	1,-1,1,-1,1,1,-1,
	1,1,-1,1,1,1,-1,
	1,1,1,-1,-1,-1,-1,
	1,-1,-1,1,1,1,1,-1,
	1,-1,1,1,-1,-1,1,
	1,-1,-1,-1,-1,1,1,
	1,1,-1,1,1,-1,1
};

/*===================================== */

/* */
/* subsectors */
/* */
subsector_t		**vissubsectors/*[MAXVISSSEC]*/, ** lastvissubsector;

/* */
/* walls */
/* */
viswall_t	*viswalls/*[MAXWALLCMDS]*/, *lastwallcmd;

/* */
/* planes */
/* */
visplane_t	*visplanes/*[MAXVISPLANES]*/, *lastvisplane;
#ifdef MARS
uint16_t 		*sortedvisplanes;
#endif

#define NUM_VISPLANES_BUCKETS 64
static visplane_t **visplanes_hash;

/* */
/* sprites */
/* */
vissprite_t	*vissprites/*[MAXVISSPRITES]*/, * lastsprite_p, * vissprite_p;

/* */
/* openings / misc refresh memory */
/* */
unsigned short	*openings/*[MAXOPENINGS]*/, * lastopening;

unsigned short	*lastsegclip;

/*===================================== */

#ifndef MARS
boolean		phase1completed;

pixel_t		*workingscreen;
#endif

#ifdef MARS
static int16_t	curpalette = -1;

__attribute__((aligned(16)))
pixel_t* viewportbuffer;

__attribute__((aligned(16)))
#endif
viewdef_t       vd;
player_t	*viewplayer;

VINT			validcount = 1;		/* increment every time a check is made */
VINT			framecount;		/* incremented every frame */

VINT		extralight;			/* bumped light from gun blasts */
VINT		extralight2;		/* bumped light from global colormap */

/* */
/* precalculated math */
/* */
angle_t		clipangle,doubleclipangle;
#ifndef MARS
fixed_t	*finecosine_ = &finesine_[FINEANGLES/4];
#endif

fixed_t yslope[SCREENHEIGHT];
fixed_t distscale[SCREENWIDTH];

VINT viewangletox[FINEANGLES/2];

angle_t xtoviewangle[SCREENWIDTH+1];

/* */
/* performance counters */
/* */
VINT t_ref_cnt = 0;
int t_ref_bsp[4], t_ref_prep[4], t_ref_segs[4], t_ref_planes[4], t_ref_sprites[4], t_ref_total[4];

r_texcache_t r_texcache;

/*
===============================================================================
=
= R_PointToAngle
=
===============================================================================
*/


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
	
	dx = (x - node->x);
	dy = (y - node->y);
	
	left = (node->dy>>FRACBITS) * (dx>>FRACBITS);
	right = (dy>>FRACBITS) * (node->dx>>FRACBITS);

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
		side = R_PointOnSide(x, y, node);
		nodenum = node->children[side];
	}
	
	return &subsectors[nodenum & ~NF_SUBSECTOR];
	
}

//
// To get a global angle from Cartesian coordinates, the coordinates are
// flipped until they are in the first octant of the coordinate system,
// then the y (<= x) is scaled and divided by x to get a tangent (slope)
// value which is looked up in the tantoangle table.
//
angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
	x -= vd.viewx;
	y -= vd.viewy;

	if (!x && !y)
		return 0;

	if (x >= 0)
	{
		if (y >= 0)
		{
			if (x > y)
				return tantoangle[SlopeDiv(y, x)]; // octant 0
			else
				return ANG90 - 1 - tantoangle[SlopeDiv(x, y)]; // octant 1
		}
		else
		{
			y = -y;

			if (x > y)
				return -tantoangle[SlopeDiv(y, x)]; // octant 7
			else
				return ANG270 + tantoangle[SlopeDiv(x, y)]; // octant 6
		}
	}
	else
	{
		x = -x;

		if (y >= 0)
		{
			if (x > y)
				return ANG180 - 1 - tantoangle[SlopeDiv(y, x)]; // octant 3
			else
				return ANG90 + tantoangle[SlopeDiv(x, y)]; // octant 2
		}
		else
		{
			y = -y;

			if (x > y)
				return ANG180 + tantoangle[SlopeDiv(y, x)]; // octant 4
			else
				return ANG270 - 1 - tantoangle[SlopeDiv(x, y)]; // octant 5
		}
	}

	return 0;
}

/*============================================================================= */

const VINT viewports[][2][3] = {
	{ { 128, 144, true  }, {  80, 100, true  } },
	{ { 128, 160, true  }, {  80, 128, true  } },
	{ { 160, 180, true  }, {  80, 144, true  } },
	{ { 224, 128, false }, { 160, 100, false } },
	{ { 252, 144, false }, { 160, 128, false } },
	{ { 320, 184, false }, { 160, 128, false } },
};

VINT viewportNum;
boolean lowResMode;
const VINT numViewports = sizeof(viewports) / sizeof(viewports[0]);

/*
================
=
= R_SetViewportSize
=
================
*/
void R_SetViewportSize(int num)
{
	int width, height;

	while (!I_RefreshCompleted())
		;

	num %= numViewports;

	width = viewports[num][splitscreen][0];
	height = viewports[num][splitscreen][1];
	lowResMode = viewports[num][splitscreen][2];

	viewportNum = num;
	viewportWidth = width;
	viewportHeight = height;

	centerX = viewportWidth / 2;
	centerY = viewportHeight / 2;

	centerXFrac = centerX * FRACUNIT;
	centerYFrac = centerY * FRACUNIT;

	if (anamorphicview)
	{
		stretch = ((FRACUNIT * 16 * height) / 180 * 28) / width;
		weaponXScale = 1000 * FRACUNIT / 1100;
	}
	else
	{
		/* proper screen size would be 160*100, stretched to 224 is 2.2 scale */
		//stretch = (fixed_t)((160.0f / width) * ((float)height / 180.0f) * 2.2f * FRACUNIT);
		stretch = ((FRACUNIT * 16 * height) / 180 * 22) / width;
		weaponXScale = FRACUNIT;
	}
	stretchX = stretch * centerX;

	weaponYpos = 180;
	if (viewportWidth < 128 || (viewportWidth <= 160 && !lowResMode)) {
		weaponYpos = 144;
	}
	weaponYpos = (viewportHeight - weaponYpos) / 2;

	R_InitMathTables();

	// refresh func pointers
	R_SetDetailMode(detailmode);

	clipangle = xtoviewangle[0];
	doubleclipangle = clipangle * 2;
	clearscreen = 2;

#ifdef MARS
	Mars_CommSlaveClearCache();
#endif
}

void R_SetDetailMode(int mode)
{
	if (mode < detmode_potato)
		return;
	if (mode >= MAXDETAILMODES)
		return;

	detailmode = mode;
	if (lowResMode)
	{
		drawcol = I_DrawColumnLow;
		drawcolnpo2 = I_DrawColumnNPo2Low;
		drawfuzzycol = I_DrawFuzzColumnLow;
		drawspan = detailmode == detmode_potato ? I_DrawSpanPotatoLow : I_DrawSpanLow;
	}
	else
	{
		drawcol = I_DrawColumn;
		drawcolnpo2 = I_DrawColumnNPo2;
		drawfuzzycol = I_DrawFuzzColumn;
		drawspan = I_DrawSpan;
		drawspan = detailmode == detmode_potato ? I_DrawSpanPotato : I_DrawSpan;
	}
}

int R_DefaultViewportSize(void)
{
	int i;

	for (i = 0; i < numViewports; i++)
	{
		const VINT* vp = viewports[i][0];
		if (vp[0] == 160 && vp[2] == true)
			return i;
	}

	return 0;
}

/*
==============
=
= R_Init
=
==============
*/

void R_Init (void)
{
	int	i, br;
	const byte* doompalette, * row;

D_printf ("R_InitData\n");
	R_InitData ();
D_printf ("Done\n");

	R_SetViewportSize(viewportNum);

	framecount = 0;
	viewplayer = &players[0];

	R_SetDetailMode(detailmode);

	R_InitTexCache(&r_texcache, numflats+numtextures);

	// test avarage mid brightness, which is around 40960
	// with the standard colormap
	//
	// in case of a non-standard colormap, we may need to darken 
	// the game a bit when diminishing lighting if turn off
	doompalette = (byte *)dc_playpals;

	row = (byte*)(dc_colormaps + 256 * 16);
	br = 0;
	for (i = 0; i < 512; i += 2) {
		br += doompalette[row[i] * 3 + 0];
		br += doompalette[row[i] * 3 + 1];
		br += doompalette[row[i] * 3 + 2];
	}

	br = br - 40960;
	if (br < 0)
		extralight2 = 0;
	else
	{
		br /= (512 * 3);
		extralight2 = -1 * ((br + 15) & ~15);
	}
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
	int zonefree;
	int cachezonesize;
	void *margin;
	const int zonemargin = 12*1024;
	const int flatblocksize = sizeof(memblock_t) + ((sizeof(texcacheblock_t) + 15) & ~15) + 64*64 + 32;

	// reset pointers from previous level
	for (i = 0; i < numtextures; i++)
		textures[i].data = R_CheckPixels(textures[i].lumpnum);
	for (i=0 ; i<numflats ; i++)
		flatpixels[i] = R_CheckPixels(firstflat + i);

	// functioning texture cache requires at least 8kb of ram
	zonefree = Z_LargestFreeBlock(mainzone);
	if (zonefree < zonemargin+flatblocksize)
		goto nocache;

	cachezonesize = zonefree - zonemargin - 128; // give the main zone some slack
	if (cachezonesize < flatblocksize)
		goto nocache;
	
	margin = Z_Malloc(zonemargin, PU_LEVEL, 0);

	R_InitTexCacheZone(&r_texcache, cachezonesize);

	Z_Free(margin);
	return;

nocache:
	R_InitTexCacheZone(&r_texcache, 0);
}

void R_SetupLevel(void)
{
	R_SetupTextureCaches();

	R_SetViewportSize(viewportNum);
#ifdef MARS
	curpalette = -1;
#endif
}

/*============================================================================= */

#ifndef MARS
int shadepixel;
extern	int	workpage;
extern	pixel_t	*screens[2];	/* [viewportWidth*viewportHeight];  */
#endif

/*
==================
=
= R_Setup
=
==================
*/

static void R_Setup (int displayplayer, unsigned short *openings_, 
	subsector_t **vissubsectors_, visplane_t *visplanes_, visplane_t **visplanes_hash_,
	uint32_t *sortedvisplanes_)
{
	int 		i;
	int		damagecount, bonuscount;
	player_t *player;
#ifdef JAGUAR
	int		shadex, shadey, shadei;
#endif
	unsigned short  *tempbuf;
#ifdef MARS
	int		palette = 0;
#endif

/* */
/* set up globals for new frame */
/* */
#ifndef MARS
	workingscreen = screens[workpage];

	*(pixel_t  **)0xf02224 = workingscreen;	/* a2 base pointer */
	*(int *)0xf02234 = 0x10000;				/* a2 outer loop add (+1 y) */
	*(int *)0xf0226c = *(int *)0xf02268 = 0;		/* pattern compare */
#else
	if (debugmode == 3)
		I_ClearFrameBuffer();
#endif

	framecount++;	
	validcount++;
		
	player = &players[displayplayer];

	vd.viewplayer = player;
	vd.viewx = player->mo->x;
	vd.viewy = player->mo->y;
	vd.viewz = player->viewz;
	vd.viewangle = player->mo->angle;

	vd.viewsin = finesine(vd.viewangle>>ANGLETOFINESHIFT);
	vd.viewcos = finecosine(vd.viewangle>>ANGLETOFINESHIFT);

	vd.displayplayer = displayplayer;
	vd.lightlevel = player->mo->subsector->sector->lightlevel;
	vd.fixedcolormap = 0;

	damagecount = player->damagecount;
	bonuscount = player->bonuscount;
	
#ifdef JAGUAR
	vd.extralight = player->extralight << 6;

/* */
/* calc shadepixel */
/* */
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
#endif

#ifdef MARS
	if (detailmode == detmode_high)
		vd.extralight = player->extralight << 4;
	else
		vd.extralight = 0;
	if (detailmode != detmode_high)
		vd.extralight += extralight2;

	if (player->powers[pw_invulnerability] > 60
		|| (player->powers[pw_invulnerability] & 4))
		vd.fixedcolormap = INVERSECOLORMAP;

	viewportbuffer = (pixel_t*)I_ViewportBuffer();

	palette = 0;

	i = 0;
	if (player->powers[pw_strength] > 0)
		i = 12 - player->powers[pw_strength] / 64;
	if (i < damagecount)
		i = damagecount;

	if (gamepaused)
		palette = 14;
	else if (!splitscreen)
	{
		if (i)
		{
			palette = (i + 7) / 8;
			if (palette > 7)
				palette = 7;
			palette += 1;
		}
		else if (bonuscount)
		{
			palette = (bonuscount + 7) / 8;
			if (palette > 3)
				palette = 3;
			palette += 9;
		}
		else if (player->powers[pw_ironfeet] > 60
			|| (player->powers[pw_ironfeet] & 4))
			palette = 13;
	}
	
	if (palette != curpalette) {
		curpalette = palette;
		I_SetPalette(dc_playpals+palette*768);
	}

	if (vd.fixedcolormap == INVERSECOLORMAP)
		vd.fuzzcolormap = INVERSECOLORMAP;
	else
		vd.fuzzcolormap = (6 - extralight2 / 2) * 256;
#endif

	tempbuf = (unsigned short *)I_WorkBuffer();

	tempbuf = (unsigned short*)(((intptr_t)tempbuf + 15) & ~15);
	viswalls = (void*)tempbuf;
	tempbuf += sizeof(*viswalls) * MAXWALLCMDS / sizeof(*tempbuf);
	lastwallcmd = viswalls;			/* no walls added yet */

	visplanes = visplanes_;
	visplanes_hash = visplanes_hash_;
	lastvisplane = visplanes + 1;		/* visplanes[0] is left empty */

/* */
/* plane filling */
/*	 */
	tempbuf = (unsigned short *)(((intptr_t)tempbuf+1)&~1);
	tempbuf++; // padding
	for (i = 0; i < MAXVISPLANES; i++) {
		visplanes[i].open = tempbuf;
		tempbuf += SCREENWIDTH+2;
	}

	lastsegclip = tempbuf;
	tempbuf += MAXOPENINGS;

	tempbuf = (unsigned short*)(((intptr_t)tempbuf + 3) & ~3);
	vissprites = (void*)tempbuf;
	tempbuf += sizeof(*vissprites) * MAXVISSPRITES / sizeof(*tempbuf);

	sortedvisplanes = (uint16_t *)sortedvisplanes_;

	//I_Error("%d", (uint16_t *)I_FrameBuffer() + 320*200 - tempbuf);

	/* */
	/* clear sprites */
	/* */
	vissprite_p = vissprites;
	lastsprite_p = vissprite_p;

	openings = openings_;
	lastopening = openings;

	vissubsectors = vissubsectors_;
	lastvissubsector = vissubsectors;	/* no subsectors visible yet */

	for (i = 0; i < NUM_VISPLANES_BUCKETS; i++)
		visplanes_hash[i] = NULL;

#ifndef MARS
	phasetime[0] = samplecount;
#endif

	R_SetupTexCacheFrame(&r_texcache);
}

#ifdef MARS

void Mars_Sec_R_Setup(void)
{
	int i;

	Mars_ClearCacheLines(&vd, (sizeof(vd) + 15) / 16);
	Mars_ClearCacheLine(&viewportbuffer);

	Mars_ClearCacheLine(&viswalls);
	Mars_ClearCacheLine(&vissprites);
	Mars_ClearCacheLine(&visplanes);
	Mars_ClearCacheLine(&lastvisplane);
	Mars_ClearCacheLine(&visplanes_hash);
	Mars_ClearCacheLine(&lastsegclip);

	Mars_ClearCacheLine(&openings);
	Mars_ClearCacheLine(&lastopening);

	Mars_ClearCacheLines(visplanes, (sizeof(visplane_t)*MAXVISPLANES+15)/16);

    Mars_ClearCacheLine(&sortedvisplanes);

	for (i = 0; i < NUM_VISPLANES_BUCKETS; i++)
		visplanes_hash[i] = NULL;
}

#endif

//
// Check for a matching visplane in the visplanes array, or set up a new one
// if no compatible match can be found.
//
int R_PlaneHash(fixed_t height, unsigned flatnum, unsigned lightlevel) {
	return ((((unsigned)height >> 8) + lightlevel) ^ flatnum) & (NUM_VISPLANES_BUCKETS - 1);
}

void R_MarkOpenPlane(visplane_t* pl)
{
	int i;
	unsigned short* open = pl->open;
	for (i = 0; i < viewportWidth / 4; i++)
	{
		*open++ = OPENMARK;
		*open++ = OPENMARK;
		*open++ = OPENMARK;
		*open++ = OPENMARK;
	}
}

void R_InitClipBounds(unsigned *clipbounds)
{
	// initialize the clipbounds array
	int i;
    int longs = (viewportWidth + 1) / 2;
    unsigned* clip = clipbounds;
    unsigned clipval = (unsigned)viewportHeight << 16 | viewportHeight;
    for (i = 0; i < longs; i++)
        *clip++ = clipval;
}

visplane_t* R_FindPlane(int hash, fixed_t height, 
	int flatnum, int lightlevel, int start, int stop)
{
	visplane_t *check, *tail, *next;

	tail = visplanes_hash[hash];
	for (check = tail; check; check = next)
	{
		next = check->next ? &visplanes[check->next - 1] : NULL;

		if (height == check->height && // same plane as before?
			flatnum == check->flatnum &&
			lightlevel == check->lightlevel)
		{
			if (check->open[start] == OPENMARK)
			{
				// found a plane, so adjust bounds and return it
				if (start < check->minx)
					check->minx = start; // mark the new edge
				if (stop > check->maxx)
					check->maxx = stop;  // mark the new edge
				return check; // use the same one as before
			}
		}
	}

	if (lastvisplane == visplanes + MAXVISPLANES)
		return visplanes;

	// make a new plane
	check = lastvisplane;
	++lastvisplane;

	check->height = height;
	check->flatnum = flatnum;
	check->lightlevel = lightlevel;
	check->minx = start;
	check->maxx = stop;

	R_MarkOpenPlane(check);

	check->next = tail ? tail - visplanes + 1 : 0;
	visplanes_hash[hash] = check;

	return check;
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

extern	boolean	debugscreenactive;

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

void R_RenderPlayerView(int displayplayer, unsigned short* openings_)
{
	subsector_t* vissubsectors_[MAXVISSSEC];

	/* make sure its done now */
#if defined(JAGUAR)
	while (!I_RefreshCompleted())
		;
#endif

	/* */
	/* initial setup */
	/* */
	if (debugscreenactive)
		I_DebugScreen();

	R_Setup(displayplayer, openings_, vissubsectors_);

#ifndef JAGUAR
	R_BSP();

	R_WallPrep();
	R_SpritePrep();
	/* the rest of the refresh can be run in parallel with the next game tic */
	if (R_LatePrep())
		R_Cache();

	R_SegCommands();

	R_DrawPlanes();

	R_Sprites();

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

#endif
}

#else

void R_RenderPlayerView(int displayplayer, unsigned short *openings_)
{
	int t_bsp, t_prep, t_segs, t_planes, t_sprites, t_total;
	boolean drawworld = !(players[consoleplayer].automapflags & AF_ACTIVE);
	subsector_t* vissubsectors_[MAXVISSSEC];
	visplane_t visplanes_[MAXVISPLANES], *visplanes_hash_[NUM_VISPLANES_BUCKETS];
	uint32_t sortedvisplanes_[MAXVISPLANES];

	while (!I_RefreshCompleted())
		;

	t_total = I_GetFRTCounter();

	Mars_R_SecWait();

	R_Setup(displayplayer, openings_, vissubsectors_, visplanes_, visplanes_hash_, sortedvisplanes_);

	Mars_R_SecSetup();

	Mars_R_BeginWallPrep(drawworld);

	t_bsp = I_GetFRTCounter();
	R_BSP();
	t_bsp = I_GetFRTCounter() - t_bsp;

	Mars_R_EndWallPrep(MAXVISSSEC);

	if (!drawworld)
		return;

	t_prep = I_GetFRTCounter();
	R_SpritePrep();
	R_Cache();
	t_prep = I_GetFRTCounter() - t_prep;

	t_segs = I_GetFRTCounter();
	R_SegCommands();
	t_segs = I_GetFRTCounter() - t_segs;

	Mars_R_SecWait();

	t_planes = I_GetFRTCounter();
	R_DrawPlanes();
	t_planes = I_GetFRTCounter() - t_planes;

	t_sprites = I_GetFRTCounter();
	R_Sprites();
	t_sprites = I_GetFRTCounter() - t_sprites;
	
	R_Update();

	t_total = I_GetFRTCounter() - t_total;

	t_ref_cnt = (t_ref_cnt + 1) & 3;
	t_ref_bsp[t_ref_cnt] = t_bsp;
	t_ref_prep[t_ref_cnt] = t_prep;
	t_ref_segs[t_ref_cnt] = t_segs;
	t_ref_planes[t_ref_cnt] = t_planes;
	t_ref_sprites[t_ref_cnt] = t_sprites;
	t_ref_total[t_ref_cnt] = t_total;
}

#endif
