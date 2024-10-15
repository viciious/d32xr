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

VINT detailmode;

VINT anamorphicview = 0;
VINT initmathtables = 2;

drawcol_t drawcol;
drawcol_t drawfuzzcol;
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
	1,1,-1,1,1,-1,1,
	-1,-1,1,-1,1,1,1,
	1,-1,1,-1,-1,1,1
};

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
viewdef_t       *vd;

VINT			framecount;		/* incremented every frame */

VINT		extralight;			/* bumped light from gun blasts */

/* */
/* precalculated math */
/* */
#ifndef MARS
fixed_t	*finecosine_ = &finesine_[FINEANGLES/4];
#endif

fixed_t *yslope/*[SCREENHEIGHT]*/;
uint16_t *distscale/*[SCREENWIDTH]*/;

VINT *viewangletox/*[FINEANGLES/2]*/;

uint16_t *xtoviewangle/*[SCREENWIDTH+1]*/;

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

static int SlopeAngle (unsigned int num, unsigned int den) ATTR_DATA_CACHE_ALIGN;

static int SlopeAngle (unsigned num, unsigned den)
{
	unsigned ans;
	angle_t *t2a;

	den >>= 8;
#ifdef MARS
	SH2_DIVU_DVSR = den;
	SH2_DIVU_DVDNT = num << 3;

    __asm volatile (
      "mov.l %1, %0"
      : "=r" (t2a)
      : "m" (tantoangle)
   );

   if (den < 2)
	  ans = SLOPERANGE;
   else
      ans = SH2_DIVU_DVDNT;
#else
	if (den < 2)
		ans = SLOPERANGE;
	else
	{
		ans = (num<<3)/den;
		if (ans > SLOPERANGE)
			ans = SLOPERANGE;
	}
	t2a = tantoangle;
#endif
	return t2a[ans];
}

angle_t R_PointToAngle (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{	
	int		x;
	int		y;
	int 	base = 0;
	int 	num = 0, den = 0, n = 1;
	
	x = x2 - x1;
	y = y2 - y1;
	
	if ( (!x) && (!y) )
		return 0;
	if (x>= 0)
	{	/* x >=0 */
		if (y>= 0)
		{	/* y>= 0 */
			if (x>y)
			{
				/* octant 0 */
				num = y, den = x;
			}
			else
			{
				/* octant 1 */
				base = ANG90-1, n = -1,	num = x, den = y;
			}
		}
		else
		{	/* y<0 */
			y = -y;
			if (x>y)
			{
				n = -1, num = y, den = x; /* octant 8 */
			}
			else
			{
				base = ANG270, n = 1, num = x, den = y; /* octant 7 */
			}
		}
	}
	else
	{	/* x<0 */
		x = -x;
		if (y>= 0)
		{	/* y>= 0 */
			if (x>y)
			{
				base = ANG180-1, n = -1, num = y, den = x; /* octant 3 */
			}
			else
			{
				base = ANG90, num = x, den = y; /* octant 2 */
			}
		}
		else
		{	/* y<0 */
			y = -y;
			if (x>y)
			{
				base = ANG180, num = y, den = x; /* octant 4 */
			}
			else
			{
				base = ANG270-1, n = -1, num = x, den = y; /* octant 5 */
			}
		}
	}
	return base + n * SlopeAngle(num, den);
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

	do
	{
		node = &nodes[nodenum];
		side = R_PointOnSide(x, y, node);
		nodenum = node->children[side];
	}
	#ifdef MARS
	while ( (int16_t)nodenum >= 0 );
#else
	while (! (nodenum & NF_SUBSECTOR) );
#endif

	return &subsectors[nodenum & ~NF_SUBSECTOR];
	
}

/*============================================================================= */

const VINT viewports[][2][3] = {
	{ { 160, 90, false }, { 160, 100, false } },
	{ { 224, 128, false }, { 160, 100, false } },
	{ { 256, 144, true }, { 160, 128, true } },
	{ { 320, 180, false }, { 160, 144, false } },
};

VINT viewportNum;
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
	}
	else
	{
		/* proper screen size would be 160*100, stretched to 224 is 2.2 scale */
		//stretch = (fixed_t)((160.0f / width) * ((float)height / 180.0f) * 2.2f * FRACUNIT);
		stretch = ((FRACUNIT * 16 * height) / 180 * 22) / width;
	}
	weaponXScale = FRACUNIT;
	stretchX = stretch * centerX;

	weaponYpos = 180;
	if (viewportHeight < 128) {
		weaponYpos = 120;
	} else if (viewportHeight == 128) {
		weaponYpos = 144;
	} else if (viewportHeight <= 144) {
		weaponYpos = 162;
	}
	weaponYpos = (viewportHeight - weaponYpos) / 2;

	initmathtables = 2;
	clearscreen = 2;

	// refresh func pointers
	R_SetDrawFuncs();

#ifdef MARS
	Mars_CommSlaveClearCache();
#endif
}

void R_SetDrawFuncs(void)
{
	if (debugmode == DEBUGMODE_NODRAW)
	{
		drawcol = I_DrawColumnNoDraw;
		drawcolnpo2 = I_DrawColumnNoDraw;
		drawfuzzcol = I_DrawColumnNoDraw;
		drawspan = I_DrawSpanNoDraw;
		return;
	}

	if (detailmode < detmode_potato || detailmode >= MAXDETAILMODES)
		detailmode = detmode_normal;

	drawcol = I_DrawColumn;
	drawcolnpo2 = I_DrawColumnNPo2;
	drawfuzzcol = I_DrawFuzzColumn;
	drawspan = detailmode == detmode_potato ? I_DrawSpanPotato : I_DrawSpan;

#ifdef MARS
	Mars_CommSlaveClearCache();
#endif
}

int R_DefaultViewportSize(void)
{
	int i;

	for (i = 0; i < numViewports; i++)
	{
		const VINT* vp = viewports[i][0];
		if (vp[2] == true)
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
D_printf ("R_InitData\n");
	R_InitData ();
D_printf ("Done\n");

	R_SetViewportSize(viewportNum);

	framecount = 0;

	R_SetDrawFuncs();

	R_InitTexCache(&r_texcache);
}

/*
==============
=
= R_SetTextureData
=
==============
*/
void R_SetTextureData(texture_t *tex, uint8_t *start, int size, boolean skipheader)
{
	int j;
	int mipcount = 1;
	int w = tex->width, h = tex->height;
	uint8_t *data = skipheader ? R_SkipJagObjHeader(start, size, w, h) : start;
#if MIPLEVELS > 1
	uint8_t *end = start + size;
	boolean masked = tex->lumpnum >= firstsprite && tex->numnum < firstsprite + numsprites;

	if (texmips && !masked)
		mipcount = MIPLEVELS;
#endif

	for (j = 0; j < mipcount; j++)
	{
		int size = w * h;

#if MIPLEVELS > 1
		if (j && data+size > end) {
			// no mipmaps
			tex->mipcount = 1;
			break;
		}
#endif

		tex->data[j] = data;
		data += size;

		w >>= 1;
		if (w < 1)
			w = 1;

		h >>= 1;
		if (h < 1)
			h = 1;
	}
}

/*
==============
=
= R_SetFlatData
=
==============
*/
void R_SetFlatData(int f, uint8_t *start, int size)
{
	int j;
	int w = 64;
	uint8_t *data = start;

	for (j = 0; j < MIPLEVELS; j++)
	{
		flatpixels[f].data[j] = data;
		if (texmips) {
			data += w * w;
			w >>= 1;
		}
	}
}

void R_ResetTextures(void)
{
	int i;

	// reset pointers from previous level
	for (i = 0; i < numtextures; i++)
	{
		int length;
		boolean skipheader;
		uint8_t *data;
		int lump = textures[i].lumpnum;

		if (lump >= firstsprite && lump < firstsprite + numsprites) {
			data = W_POINTLUMPNUM(lump+1);
			length = W_LumpLength(lump+1);
			skipheader = false;
		} else {
			data = R_CheckPixels(lump);
			length = W_LumpLength(lump);
			skipheader = true;
		}

		R_SetTextureData(&textures[i], data, length, skipheader);
	}

	for (i=0 ; i<numflats ; i++)
	{
		int length;
		uint8_t *data;
		int lump = firstflat + i;

		data = R_CheckPixels(lump);
		length = W_LumpLength(lump);

		R_SetFlatData(i, data, length);
	}
}

/*
==============
=
= R_SetupTextureCaches
=
==============
*/
void R_SetupTextureCaches(int gamezonemargin)
{
	int zonefree;
	int cachezonesize;
	void *margin;
	const int zonemargin = gamezonemargin;
	const int flatblocksize = sizeof(memblock_t) + ((sizeof(texcacheblock_t) + 15) & ~15) + 64*64 + 32;

	// functioning texture cache requires at least 8kb of ram
	zonefree = Z_LargestFreeBlock(mainzone);
	if (zonefree < zonemargin+flatblocksize)
		goto nocache;

	cachezonesize = zonefree - zonemargin - 128; // give the main zone some slack
	if (cachezonesize < flatblocksize)
		goto nocache;
	
	margin = Z_Malloc(zonemargin, PU_LEVEL);

	R_InitTexCacheZone(&r_texcache, cachezonesize);

	Z_Free(margin);
	return;

nocache:
	R_InitTexCacheZone(&r_texcache, 0);
}

void R_SetupLevel(int gamezonemargin)
{
	R_SetupTextureCaches(gamezonemargin);

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

static void R_Setup (int displayplayer, visplane_t *visplanes_,
	sector_t **vissectors_, viswallextra_t *viswallex_)
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
	if (debugmode == DEBUGMODE_NODRAW)
		I_ClearFrameBuffer();
#endif

	framecount++;	
	validcount[0]++;

	player = &players[displayplayer];

	vd->psprites = player->psprites;
	vd->viewx = player->mo->x;
	vd->viewy = player->mo->y;
	vd->viewz = player->viewz;
	vd->viewangle = player->mo->angle;

	vd->viewsin = finesine(vd->viewangle>>ANGLETOFINESHIFT);
	vd->viewcos = finecosine(vd->viewangle>>ANGLETOFINESHIFT);

	vd->displayplayer = displayplayer;
	vd->lightlevel = player->mo->subsector->sector->lightlevel;
	vd->fixedcolormap = 0;

	vd->clipangle = xtoviewangle[0]<<FRACBITS;
	vd->viewangletox = viewangletox;

	damagecount = player->damagecount;
	bonuscount = player->bonuscount;

#ifdef JAGUAR
	vd->extralight = player->extralight << 6;

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
	vd->extralight = player->extralight << 4;

	if (player->powers[pw_invulnerability] > 0)
	{
		if (player->powers[pw_invulnerability] > 60
			|| (player->powers[pw_invulnerability] & 4))
			vd->fixedcolormap = INVERSECOLORMAP;
	}
    else if (player->powers[pw_infrared] > 0)
    {
		if (player->powers[pw_infrared] > 4*32
			|| (player->powers[pw_infrared]&8) )
		{
			// almost full bright
			vd->fixedcolormap = 1*256;
		}
	}

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

	if (vd->fixedcolormap == INVERSECOLORMAP)
		vd->fuzzcolormap = INVERSECOLORMAP;
	else
		vd->fuzzcolormap = 12 * 256;
#endif

	vd->visplanes = visplanes_;
	vd->visplanes[0].flatandlight = 0;

	tempbuf = (unsigned short *)I_WorkBuffer();

/* */
/* plane filling */
/*	 */
	tempbuf = (unsigned short*)(((intptr_t)tempbuf + 3) & ~3);
	tempbuf += 2; // padding
	for (i = 0; i < MAXVISPLANES; i++) {
		vd->visplanes[i].open = tempbuf;
		tempbuf += SCREENWIDTH+2;
	}

	vd->segclip = tempbuf;
	tempbuf += MAXOPENINGS;

	tempbuf = (unsigned short*)(((intptr_t)tempbuf + 3) & ~3);
	vd->viswalls = (void*)tempbuf;
	tempbuf += sizeof(*vd->viswalls) * MAXWALLCMDS / sizeof(*tempbuf);

	vd->viswallextras = viswallex_ + 1;

	// re-use the openings array in VRAM
	vd->gsortedsprites = (void *)(((intptr_t)vd->visplanes[1].open + 3) & ~3);

	vd->vissprites = (void *)vd->viswalls;

	vd->lastwallcmd = vd->viswalls;			/* no walls added yet */
	vd->lastsegclip = vd->segclip;

	vd->lastvisplane = vd->visplanes + 1;		/* visplanes[0] is left empty */
	vd->visplanes_hash = NULL;

	vd->gsortedvisplanes = NULL;

	vd->columncache[0] = (uint8_t*)(((intptr_t)tempbuf + 3) & ~3);
	tempbuf += sizeof(uint8_t) * COLUMN_CACHE_SIZE * 2 / sizeof(*tempbuf);
	vd->columncache[1] = (uint8_t*)(((intptr_t)tempbuf + 3) & ~3);
	tempbuf += sizeof(uint8_t) * COLUMN_CACHE_SIZE * 2 / sizeof(*tempbuf);

	//I_Error("%d", ((uint16_t *)I_FrameBuffer() + 64*1024-0x100 - tempbuf) * 2);

	I_SetThreadLocalVar(DOOMTLS_COLUMNCACHE, vd->columncache[0]);

	/* */
	/* clear sprites */
	/* */
	vd->vissprite_p = vd->vissprites;
	vd->lastsprite_p = vd->vissprite_p;

	vd->vissectors = vissectors_;
	vd->lastvissector = vd->vissectors;	/* no subsectors visible yet */

#ifndef MARS
	phasetime[0] = samplecount;
#endif
}

#ifdef MARS

void Mars_Sec_R_Setup(void)
{
	Mars_ClearCacheLines(&vd, 1);
	Mars_ClearCacheLines(vd, (sizeof(*vd) + 31) / 16);
	Mars_ClearCacheLine(&viewportbuffer);

	Mars_ClearCacheLines(vd->visplanes, (sizeof(visplane_t)*MAXVISPLANES+31)/16);

	I_SetThreadLocalVar(DOOMTLS_COLUMNCACHE, vd->columncache[1]);
}

#endif

//
// Check for a matching visplane in the visplanes array, or set up a new one
// if no compatible match can be found.
//
#define R_PlaneHash(height, lightlevel) \
	((((unsigned)(height) >> 8) + (flatandlight>>16)) ^ (flatandlight&0xffff)) & (NUM_VISPLANES_BUCKETS - 1)

void R_MarkOpenPlane(visplane_t* pl)
{
	unsigned longs = (unsigned)(viewportWidth + 1) / 2;
	uint32_t * open = (uint32_t *)pl->open;
	const uint32_t v = ((uint32_t)OPENMARK << 16) | OPENMARK;
	unsigned longlongs = longs / 2;

	do {
		*open++ = v;
		*open++ = v;
	} while (--longlongs > 0);
}

void R_InitClipBounds(uint32_t *clipbounds)
{
	// initialize the clipbounds array
	int i;
	int longs = (viewportWidth + 1) / 2;
	uint32_t* clip = clipbounds;
	const uint32_t v = ((uint32_t)viewportHeight << 16) | viewportHeight;
	for (i = 0; i < longs/2; i++)
	{
		*clip++ = v;
		*clip++ = v;
	}
}

visplane_t* R_FindPlane(fixed_t height, 
	int flatandlight, int start, int stop)
{
	visplane_t *check, *tail, *next;
	int hash = R_PlaneHash(height, flatandlight);

	tail = vd->visplanes_hash[hash];
	for (check = tail; check; check = next)
	{
		next = check->next;

		if (height == check->height && // same plane as before?
			flatandlight == check->flatandlight)
		{
			if (MARKEDOPEN(check->open[start]))
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

	if (vd->lastvisplane == vd->visplanes + MAXVISPLANES)
		return vd->visplanes;

	// make a new plane
	check = vd->lastvisplane;
	++vd->lastvisplane;

	check->height = height;
	check->flatandlight = flatandlight;
	check->minx = start;
	check->maxx = stop;

	R_MarkOpenPlane(check);

	check->next = tail;
	vd->visplanes_hash[hash] = check;

	return check;
}

void R_BSP (void) __attribute__((noinline));
void R_WallPrep (void) __attribute__((noinline));
void R_SpritePrep (void) __attribute__((noinline));
boolean R_LatePrep (void) __attribute__((noinline));
void R_Cache (void) __attribute__((noinline));
void R_SegCommands (void) __attribute__((noinline));
void R_DrawPlanes (void) __attribute__((noinline));
void R_Sprites (void) __attribute__((noinline));
void R_Update (void) __attribute__((noinline));

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

void R_RenderPlayerView(int displayplayer)
{
	visplane_t visplanes_[MAXVISPLANES];
	sector_t *vissectors_[MAXVISSSEC];
	viswallextra_t viswallex_[MAXWALLCMDS + 1] __attribute__((aligned(16)));

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

	R_Setup(displayplayer, visplanes_, vissectors_, viswallex_);

#ifndef JAGUAR
	R_BSP();

	R_WallPrep();
	/* the rest of the refresh can be run in parallel with the next game tic */
	if (R_LatePrep())
		R_Cache();

	R_SegCommands();

	R_DrawPlanes();

	R_SpritePrep();

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

void R_RenderPlayerView(int displayplayer)
{
	int t_bsp, t_prep, t_segs, t_planes, t_sprites, t_total;
	boolean drawworld = !(players[consoleplayer].automapflags & AF_ACTIVE);
	__attribute__((aligned(16)))
		visplane_t visplanes_[MAXVISPLANES];
	sector_t *vissectors_[(MAXVISSSEC > MAXVISSPRITES ? MAXVISSSEC : MAXVISSPRITES) + 1];
	viswallextra_t viswallex_[MAXWALLCMDS + 1] __attribute__((aligned(16)));

	t_total = I_GetFRTCounter();

	R_Setup(displayplayer, visplanes_, vissectors_, viswallex_);

	Mars_R_BeginWallPrep(drawworld);

	t_bsp = I_GetFRTCounter();
	R_BSP();
	t_bsp = I_GetFRTCounter() - t_bsp;

	Mars_R_EndWallPrep();

	if (!drawworld)
	{
		Mars_R_SecWait();
		return;
	}

	t_prep = I_GetFRTCounter();
	R_Cache();
	t_prep = I_GetFRTCounter() - t_prep;

	t_segs = I_GetFRTCounter();
	R_SegCommands();
	t_segs = I_GetFRTCounter() - t_segs;

	Mars_ClearCacheLine(&vd->lastsegclip);

	if (vd->lastsegclip - vd->segclip > MAXOPENINGS)
		I_Error("lastsegclip > MAXOPENINGS: %d", vd->lastsegclip - vd->segclip);
	if (vd->lastvissector - vd->vissectors > MAXVISSSEC)
		I_Error("lastvissector > MAXVISSSEC: %d", vd->lastvissector - vd->vissectors);

	t_planes = I_GetFRTCounter();
	R_DrawPlanes();
	t_planes = I_GetFRTCounter() - t_planes;

	t_sprites = I_GetFRTCounter();
	R_SpritePrep();
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
