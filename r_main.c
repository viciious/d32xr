/* r_main.c */

#include "doomdef.h"
#include "r_local.h"
#include "p_camera.h"
#ifdef MARS
#include "mars.h"
#include "marshw.h"
#endif

int16_t viewportWidth, viewportHeight;
int16_t centerX, centerY;
fixed_t centerXFrac, centerYFrac;
fixed_t stretch;
fixed_t stretchX;

VINT anamorphicview = 0;
VINT initmathtables = 2;

drawcol_t drawcol;
drawcol_t drawcolflipped;
drawcol_t drawcolnpo2;
drawcol_t drawcollow;
drawspan_t drawspan;
drawspancolor_t drawspancolor;

#ifdef MDSKY
drawskycol_t drawskycol;
#endif
drawcol_t draw32xskycol;

#ifdef HIGH_DETAIL_SPRITES
drawcol_t drawspritecol;
#endif

// Classic Sonic fade
const short md_palette_fade_table[32] =
{
	// Black
	0x000,

	// Fade in blue
	0x200, 0x400, 0x400, 0x600, 0x800,
	0x800, 0xA00, 0xC00, 0xC00, 0xE00,

	// Fade in green with blue already at max brightness
	0xE20, 0xE20, 0xE40, 0xE60, 0xE60,
	0xE80, 0xEA0, 0xEA0, 0xEC0, 0xEE0, 0xEE0,

	// Fade in red with blue and green already at max brightness
	0xEE2, 0xEE4, 0xEE4, 0xEE6, 0xEE8,
	0xEE8, 0xEEA, 0xEEC, 0xEEC, 0xEEE
};

/*===================================== */

const uint16_t visplane0open[SCREENWIDTH+2] = { 0 };

/*===================================== */

#ifndef MARS
boolean		phase1completed;

pixel_t		*workingscreen;
#endif

#ifdef MARS
static int16_t	curpalette = -1;

__attribute__((aligned(4)))
boolean line_table_effects;

__attribute__((aligned(4)))
boolean copper_effects;

__attribute__((aligned(4)))
int copper_color_index;

__attribute__((aligned(2)))
short copper_vertical_offset;

__attribute__((aligned(2)))
short copper_vertical_rate;

__attribute__((aligned(2)))
unsigned short copper_neutral_color;

__attribute__((aligned(2)))
unsigned short copper_table_height;

__attribute__((aligned(4)))
volatile unsigned short *copper_color_table = NULL;

__attribute__((aligned(16)))
pixel_t* viewportbuffer;

__attribute__((aligned(16)))
#endif
viewdef_t       vd;
player_t	*viewplayer;

VINT			framecount;		/* incremented every frame */

VINT		extralight;			/* bumped light from gun blasts */

/* */
/* precalculated math */
/* */
#ifndef MARS
fixed_t	*finecosine_ = &finesine_[FINEANGLES/4];
#endif

//added:10-02-98: yslopetab is what yslope used to be,
//                yslope points somewhere into yslopetab,
//                now (viewheight/2) slopes are calculated above and
//                below the original viewheight for mouselook
//                (this is to calculate yslopes only when really needed)
//                (when mouselookin', yslope is moving into yslopetab)
//                Check R_SetupFrame, R_SetViewSize for more...
fixed_t*                 yslopetab;
fixed_t*                yslope;
uint16_t *distscale/*[SCREENWIDTH]*/;

VINT *viewangletox/*[FINEANGLES/2]*/;

uint16_t *xtoviewangle/*[SCREENWIDTH+1]*/;

/* */
/* performance counters */
/* */
VINT t_ref_cnt = 0;
int t_ref_bsp[4], t_ref_prep[4], t_ref_segs[4], t_ref_planes[4], t_ref_sprites[4], t_ref_total[4];

r_texcache_t r_texcache;

texture_t *testtex;

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
	ans = ans <= SLOPERANGE ? ans : SLOPERANGE;
	return t2a[ans];
}

angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
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

VINT R_PointInSubsector2 (fixed_t x, fixed_t y)
{
	node_t	*node;
	int		side, nodenum;
	
	if (!numnodes)				/* single subsector is a special case */
		return 0;
		
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

	return nodenum & ~NF_SUBSECTOR;	
}

/*============================================================================= */

const VINT viewports[][2][3] = {
	{ { 128, 144, true  }, {  80, 100, true  } },
	{ { 160, 180, true  }, {  80, 144, true  } },
	{ { 256, 144, false }, { 160, 128, false } },
	{ { 320, 180, false }, { 160, 144, false } },
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
	}
	else
	{
		/* proper screen size would be 160*100, stretched to 224 is 2.2 scale */
		//stretch = (fixed_t)((160.0f / width) * ((float)height / 180.0f) * 2.2f * FRACUNIT);
		stretch = ((FRACUNIT * 16 * height) / 180 * 22) / width;
	}
	stretchX = stretch * centerX;

	initmathtables = 2;
	clearscreen = 2;

	// refresh func pointers
	R_SetDrawMode();

	R_InitColormap();

#ifdef MARS
	Mars_CommSlaveClearCache();
#endif
}

void R_SetDrawMode(void)
{
	if (debugmode == DEBUGMODE_NODRAW)
	{
		drawcol = I_DrawColumnNoDraw;
		drawcolflipped = I_DrawColumnNoDraw;
		drawcolnpo2 = I_DrawColumnNoDraw;
		drawcollow = I_DrawColumnNoDraw;
		drawspan = I_DrawSpanNoDraw;
		drawspancolor = I_DrawSpanColorNoDraw;

		#ifdef MDSKY
		drawskycol = I_DrawSkyColumnNoDraw;
		#endif
		draw32xskycol = I_DrawColumnNoDraw;

		#ifdef HIGH_DETAIL_SPRITES
		drawspritecol = I_DrawColumnNoDraw;
		#endif

		return;
	}

	if (lowResMode)
	{
		drawcol = I_DrawColumnLow;
		drawcolnpo2 = I_DrawColumnNPo2Low;
		drawcollow = I_DrawColumnLow;

		#ifdef MDSKY
		drawskycol = I_DrawSkyColumnLow;
		#endif
		draw32xskycol = I_Draw32XSkyColumnLow;

		#ifdef HIGH_DETAIL_SPRITES
		drawspritecol = I_DrawColumn;
		drawcolflipped = I_DrawColumnFlipped;
		#endif
	}
	else
	{
		drawcol = I_DrawColumn;
		drawcolflipped = I_DrawColumnFlipped;
		drawcolnpo2 = I_DrawColumnNPo2;
		drawcollow = I_DrawColumnLow;

		#ifdef MDSKY
		drawskycol = I_DrawSkyColumn;
		#endif
		//draw32xskycol = I_Draw32XSkyColumn;	// This doesn't exist!

		#ifdef HIGH_DETAIL_SPRITES
		drawspritecol = I_DrawColumn;
		#endif
	}

	#ifdef POTATO_MODE
	drawspan = I_DrawSpanPotatoLow;
	#else
	drawspan = I_DrawSpanLow;
	#endif
	drawspancolor = I_DrawSpanColorLow;

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
D_printf ("R_InitData\n");
	R_InitData ();
D_printf ("Done\n");

	R_SetViewportSize(viewportNum);

	framecount = 0;
	viewplayer = &players[0];

	R_SetDrawMode();

	R_InitTexCache(&r_texcache);

	testtex = &textures[R_TextureNumForName("GFZROCK")];
}

VINT CalcFlatSize(int lumplength)
{
	if (lumplength < 2*2)
		return 1;
	
	if (lumplength < 4*4)
		return 2;

	if (lumplength < 8*8)
		return 4;

	if (lumplength < 16*16)
		return 8;

	if (lumplength < 32*32)
		return 16;
	
	if (lumplength < 64*64)
		return 32;

	return 64;
}


__attribute((noinline))
static void R_SetupSkyGradient(void)
{
	// Retrieve lump for drawing the sky gradient.
	uint8_t *sky_gradient_ptr;

	copper_effects = false;
	
	if (copper_color_table)
	{
		Z_Free(copper_color_table);
		copper_color_table = NULL;
	}

	//uint32_t sky_gradient_size;
	
	int lump;

	char lumpname[9];

	D_snprintf(lumpname, 8, "%sGRA", gamemapinfo.sky);
	lump = W_CheckNumForName(lumpname);
	if (lump == -1) {
		return;
	}

	// This map uses a gradient.
	copper_effects = true;
	sky_gradient_ptr = (uint8_t *)W_POINTLUMPNUM(lump);
	//sky_gradient_size = W_LumpLength(lump);

	copper_neutral_color = (sky_gradient_ptr[0] << 8) | (sky_gradient_ptr[1] & 0xFF);
	copper_table_height = 1 << sky_gradient_ptr[2];
	//copper_table_count = sky_gradient_ptr[3];
	copper_vertical_offset = (sky_gradient_ptr[4] << 8) | (sky_gradient_ptr[5] & 0xFF);
	copper_vertical_rate = sky_gradient_ptr[6];

	int section_count = sky_gradient_ptr[7];

	int section_format = sky_gradient_ptr[8];

	unsigned char *data = &sky_gradient_ptr[9];

	int table_index = 0;

	copper_color_table = Z_Malloc(sizeof(unsigned short) * copper_table_height, PU_STATIC); // Put it on the heap

#ifndef SHOW_DISCLAIMER
	CONS_Printf("Free memory: %d (LFB: %d)", Z_FreeMemory(mainzone), Z_LargestFreeBlock(mainzone));
#endif

	switch (section_format)
	{
		case 0:
			// Flat color sections
			for (int section=0; section < section_count; section++)
			{
				unsigned short color = (data[0] << 8) | data[1];
				unsigned short height = data[2] + 1;

				for (int line=0; line < height; line++) {
					copper_color_table[table_index + line] = color;
				}

				table_index += height;
				data += 3;
			}
			break;

		case 1:
			// Graded color sections
			for (int section=0; section < section_count; section++)
			{
				unsigned short red = (data[0] << 8);
				unsigned short green = (data[1] << 8);
				unsigned short blue = (data[2] << 8);
				signed short inc_red = (data[3] << 8) | data[4];
				signed short inc_green = (data[5] << 8) | data[6];
				signed short inc_blue = (data[7] << 8) | data[8];
				unsigned short interval_iterations = data[9] + 1;
				unsigned short interval_height = data[10] + 1;

				for (int interval = 0; interval < interval_iterations; interval++) {
					for (int line=0; line < interval_height; line++) {
						copper_color_table[table_index + line] =
								(((*(unsigned char *)&blue) & 0xF8) << 7)
								| (((*(unsigned char *)&green) & 0xF8) << 2)
								| ((*(unsigned char *)&red) >> 3);
					}

					table_index += interval_height;

					red += inc_red;
					green += inc_green;
					blue += inc_blue;
				}

				data += 11;
			}
	}

	unsigned short color = (data[0] << 8) | data[1];

	while (table_index < copper_table_height) {
		copper_color_table[table_index] = color;
		table_index++;
	}
}


#ifdef MDSKY
__attribute((noinline))
static void R_SetupMDSky(void)
{
	// Retrieve lumps for drawing the sky on the MD.
	uint8_t *sky_metadata_ptr;
	uint8_t *sky_names_a_ptr;
	uint8_t *sky_names_b_ptr;
	uint8_t *sky_palettes_ptr;
	uint8_t *sky_tiles_ptr;

	//uint32_t sky_metadata_size;
	uint32_t sky_names_a_size;
	uint32_t sky_names_b_size;
	uint32_t sky_palettes_size;
	uint32_t sky_tiles_size;
	
	int lump;

	char lumpname[9];

	D_snprintf(lumpname, 8, "%sMD", gamemapinfo.sky);
	lump = W_CheckNumForName(lumpname);
	if (lump != -1) {
		// This map uses an MD sky.
		sky_md_layer = true;
		sky_metadata_ptr = (uint8_t *)W_POINTLUMPNUM(lump);
		//sky_metadata_size = W_LumpLength(lump);
	}
	else {
		// This map uses a 32X sky.
		sky_md_layer = false;
		return;
	}

	D_snprintf(lumpname, 8, "%sA", gamemapinfo.sky);
	lump = W_CheckNumForName(lumpname);
	if (lump != -1) {
		sky_names_a_ptr = (uint8_t *)W_POINTLUMPNUM(lump);
		sky_names_a_size = W_LumpLength(lump);
	}
	else {
		return;
	}

	D_snprintf(lumpname, 8, "%sB", gamemapinfo.sky);
	lump = W_CheckNumForName(lumpname);
	if (lump != -1) {
		sky_names_b_ptr = (uint8_t *)W_POINTLUMPNUM(lump);
		sky_names_b_size = W_LumpLength(lump);
	}
	else {
		return;
	}

	D_snprintf(lumpname, 8, "%sPAL", gamemapinfo.sky);
	lump = W_CheckNumForName(lumpname);
	if (lump != -1) {
		sky_palettes_ptr = (uint8_t *)W_POINTLUMPNUM(lump);
		sky_palettes_size = W_LumpLength(lump);
	}
	else {
		return;
	}

	D_snprintf(lumpname, 8, "%sTIL", gamemapinfo.sky);
	lump = W_CheckNumForName(lumpname);
	if (lump != -1) {
		sky_tiles_ptr = (uint8_t *)W_POINTLUMPNUM(lump);
		sky_tiles_size = W_LumpLength(lump);
	}
	else {
		return;
	}

	// Get the thru-pixel color from the metadata.
	mars_thru_rgb_reference = sky_metadata_ptr[0];
	extended_sky = (sky_metadata_ptr[2] & 0x81);	// false = H32 mode; true = H40 mode
	

	Mars_LoadMDSky(sky_metadata_ptr,
			sky_names_a_ptr, sky_names_a_size, 
			sky_names_b_ptr, sky_names_b_size, 
			sky_palettes_ptr, sky_palettes_size, 
			sky_tiles_ptr, sky_tiles_size);
}
#endif


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

	boolean masked = tex->lumpnum >= firstsprite && tex->lumpnum < firstsprite + numsprites;

    if (!masked && texmips)
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

static boolean IsWavyFlat(byte flatnum)
{
    return (flatnum >= 9 && flatnum <= 16) // BWATER
        || (flatnum >= 24 && flatnum <= 27) // CHEMG
        || (flatnum >= 43 && flatnum <= 50) // DWATER
        || (flatnum == 74) // RLAVA1
        ;
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
	int w = CalcFlatSize(size);
	uint8_t *data = start;

#ifdef FLATMIPS
	for (j = 0; j < MIPLEVELS; j++)
	{
		flatpixels[f].data[j] = data;
		flatpixels[f].size = w;
		flatpixels[f].wavy = IsWavyFlat(f);
		if (texmips) {
			data += w * w;
			w >>= 1;

			if (w < 1)
				w = 1;
		}
	}
#else
	for (j = 0; j < 1; j++)
	{
		flatpixels[f].data[j] = data;
		flatpixels[f].size = w;
		flatpixels[f].wavy = IsWavyFlat(f);
		if (texmips) {
			data += w * w;
			w >>= 1;

			if (w < 1)
				w = 1;
		}
	}
#endif
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

#ifndef SHOW_DISCLAIMER
	if (copper_color_table == NULL) {
		CONS_Printf("Free memory: %d (LFB: %d)", Z_FreeMemory(mainzone), Z_LargestFreeBlock(mainzone));
	}
#endif

	// functioning texture cache requires at least 8kb of ram
	zonefree = Z_LargestFreeBlock(mainzone);
//	CONS_Printf("Free memory: %d", zonefree);
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
	#ifdef MDSKY
	R_SetupSkyGradient();

	R_SetupMDSky();
	#endif

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
#define AIMINGTODY(a) ((finetangent((2048+(((int)a)>>ANGLETOFINESHIFT)) & FINEMASK)*160)>>FRACBITS)

static void R_Setup (int displayplayer, visplane_t *visplanes_,
	visplane_t **visplanes_hash_, sector_t **vissectors_, viswallextra_t *viswallex_)
{
	boolean waterpal = false;
	int 		i;
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

	if (!demoplayback)
	{
		const camera_t *thiscam = NULL;
		thiscam = &camera;

		vd.viewx = thiscam->x;
		vd.viewy = thiscam->y;
		vd.viewz = thiscam->z + (20 << FRACBITS);
		vd.viewangle = thiscam->angle;
		vd.lightlevel = sectors[thiscam->subsector->isector].lightlevel;
		vd.aimingangle = thiscam->aiming;
		vd.viewsubsector = thiscam->subsector;

		if (sectors[thiscam->subsector->isector].heightsec != -1
			&& GetWatertopSec(&sectors[thiscam->subsector->isector]) > vd.viewz)
			waterpal = true;
	}
	else
	{
		vd.viewx = player->mo->x;
		vd.viewy = player->mo->y;
		vd.viewz = player->viewz;
		vd.viewangle = player->mo->angle;
		vd.aimingangle = 0;
		vd.lightlevel = SS_SECTOR(player->mo->isubsector)->lightlevel;
		vd.viewsubsector = &subsectors[player->mo->isubsector];
	}

	vd.viewplayer = player;

	vd.viewsin = finesine(vd.viewangle>>ANGLETOFINESHIFT);
	vd.viewcos = finecosine(vd.viewangle>>ANGLETOFINESHIFT);

	// look up/down
	int dy;
	if (demoplayback && gamemapinfo.mapNumber == TITLE_MAP_NUMBER) {
		// The viewport for the title screen is aligned with the bottom of
		// the screen. Therefore we shift the angle to center the horizon.
		dy = -27;
	}
	else {
		G_ClipAimingPitch((int*)&vd.aimingangle);
		dy = AIMINGTODY(vd.aimingangle);
	}

	yslope = &yslopetab[(3*viewportHeight/2) - dy];
	centerY = (viewportHeight / 2) + dy;
	centerYFrac = centerY << FRACBITS;

	vd.displayplayer = displayplayer;
	vd.fixedcolormap = 0;

	vd.clipangle = xtoviewangle[0]<<FRACBITS;
	vd.doubleclipangle = vd.clipangle * 2;
	vd.viewangletox = viewangletox;

	if (gamemapinfo.mapNumber != TITLE_MAP_NUMBER && (gamemapinfo.mapNumber < SSTAGE_START || gamemapinfo.mapNumber > SSTAGE_END))
	{
		if (leveltime < 62)
		{
			if (leveltime < 30) {
				// Set the fade degree to black.
				vd.fixedcolormap = HWLIGHT(0);	// 32X VDP
				#ifdef MDSKY
				if (leveltime == 0 && sky_md_layer) {
					Mars_FadeMDPaletteFromBlack(0);	// MD VDP
				}
				#endif
			}
			else {
				// Set the fade degree based on leveltime.
				vd.fixedcolormap = HWLIGHT((leveltime-30)*8);	// 32X VDP
				#ifdef MDSKY
				if (sky_md_layer) {
					Mars_FadeMDPaletteFromBlack(md_palette_fade_table[leveltime-30]);	// MD VDP
				}
				#endif
			}
		}
		else if (fadetime > 0)
		{
			// Set the fade degree based on leveltime.
//			vd.fixedcolormap = HWLIGHT((TICRATE-fadetime)*8);	// 32X VDP
			#ifdef MDSKY
			if (sky_md_layer) {
				Mars_FadeMDPaletteFromBlack(md_palette_fade_table[TICRATE-(fadetime*3)]);	// MD VDP
			}
			#endif
		}
	}

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
	vd.extralight = 0;

	viewportbuffer = (pixel_t*)I_ViewportBuffer();

	if (demoplayback && gamemapinfo.mapNumber == TITLE_MAP_NUMBER) {
		viewportbuffer += (160*22);
	}

	palette = 0;

	i = 0;

	if (gamepaused)
		palette = 12;
	else if (fadetime > 0)
	{
		if (gamemapinfo.mapNumber >= SSTAGE_START && gamemapinfo.mapNumber <= SSTAGE_END)
		{
			palette = 1 + (fadetime / 2);
			if (palette > 5)
				palette = 5;
		}
		else
		{
			palette = 6 + (fadetime / 2);
			if (palette > 10)
				palette = 10;
		}
	}
	else if (gamemapinfo.mapNumber >= SSTAGE_START && gamemapinfo.mapNumber <= SSTAGE_END
		&& gametic < 15)
	{
		palette = 5 - (gametic / 3);
		if (palette < 0)
			palette = 0;

		#ifdef MDSKY
		if (sky_md_layer) {
			Mars_FadeMDPaletteFromBlack(0xEEE); //TODO: Replace with Mars_FadeMDPaletteFromWhite()
		}
		#endif
	}
	else if (leveltime < 15 && demoplayback && gamemapinfo.mapNumber == TITLE_MAP_NUMBER) {
		palette = 5 - (leveltime / 3);

		#ifdef MDSKY
		if (sky_md_layer) {
			Mars_FadeMDPaletteFromBlack(0xEEE); //TODO: Replace with Mars_FadeMDPaletteFromWhite()
		}
		#endif
	}
	else if (player->whiteFlash)
		palette = player->whiteFlash + 1;
	else if (waterpal) {
		palette= 11;

		distortion_action = DISTORTION_ADD;
	}

	if (gametic <= 1 && gamemapinfo.mapNumber != TITLE_MAP_NUMBER)
	{
		curpalette = palette = 10;
		I_SetPalette(dc_playpals+10*768);
	}
	
	if (palette != curpalette) {
		curpalette = palette;
		I_SetPalette(dc_playpals+palette*768);

		if (!waterpal) {
			RemoveDistortionFilters();
			distortion_action = DISTORTION_REMOVE; // Necessary to normalize the next frame buffer.
		}
	}
#endif

	vd.visplanes = visplanes_;
	vd.visplanes[0].flatandlight = 0;

	tempbuf = (unsigned short *)I_WorkBuffer();

/* */
/* plane filling */
/*	 */
	vd.visplanes[0].open = (uint16_t *)visplane0open + 1;

	tempbuf = (unsigned short*)(((intptr_t)tempbuf + 3) & ~3);
	tempbuf += 2; // padding
	for (i = 1; i < MAXVISPLANES; i++) {
		vd.visplanes[i].open = tempbuf;
		tempbuf += SCREENWIDTH+2;
	}

	vd.segclip = tempbuf;
	tempbuf += MAXOPENINGS;

	tempbuf = (unsigned short*)(((intptr_t)tempbuf + 3) & ~3);
	vd.viswalls = (void*)tempbuf;
	tempbuf += sizeof(*vd.viswalls) * MAXWALLCMDS / sizeof(*tempbuf);

	vd.viswallextras = viswallex_ + 1;

	// re-use the openings array in VRAM
	vd.gsortedsprites = (void *)(((intptr_t)vd.visplanes[1].open + 3) & ~3);

	vd.vissprites = (void *)vd.viswalls;

	vd.lastwallcmd = vd.viswalls;			/* no walls added yet */
	vd.lastsegclip = vd.segclip;

	vd.lastvisplane = vd.visplanes + 1;		/* visplanes[0] is left empty */
	vd.visplanes_hash = visplanes_hash_;

	vd.gsortedvisplanes = NULL;

	vd.columncache[0] = (uint8_t*)(((intptr_t)tempbuf + 3) & ~3);
	tempbuf += sizeof(uint8_t) * COLUMN_CACHE_SIZE * 2 / sizeof(*tempbuf);
	vd.columncache[1] = (uint8_t*)(((intptr_t)tempbuf + 3) & ~3);
	tempbuf += sizeof(uint8_t) * COLUMN_CACHE_SIZE * 2 / sizeof(*tempbuf);

	//I_Error("%d", ((uint16_t *)I_FrameBuffer() + 64*1024-0x100 - tempbuf) * 2);

	I_SetThreadLocalVar(DOOMTLS_COLUMNCACHE, vd.columncache[0]);

	/* */
	/* clear sprites */
	/* */
	vd.vissprite_p = vd.vissprites;
	vd.lastsprite_p = vd.vissprite_p;

	vd.vissectors = vissectors_;
	vd.lastvissector = vd.vissectors;	/* no subsectors visible yet */

	for (i = 0; i < NUM_VISPLANES_BUCKETS; i++)
		vd.visplanes_hash[i] = NULL;

#ifndef MARS
	phasetime[0] = samplecount;
#endif
}

#ifdef MARS

void Mars_Sec_R_Setup(void)
{
	int i;

	Mars_ClearCacheLines(&vd, (sizeof(vd) + 31) / 16);
	Mars_ClearCacheLine(&viewportbuffer);

	Mars_ClearCacheLines(vd.visplanes, (sizeof(visplane_t)*MAXVISPLANES+31)/16);

	for (i = 0; i < NUM_VISPLANES_BUCKETS; i++)
		vd.visplanes_hash[i] = NULL;

	I_SetThreadLocalVar(DOOMTLS_COLUMNCACHE, vd.columncache[1]);
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
	int i;
	int longs = (viewportWidth + 1) / 2;
	uint32_t * open = (uint32_t *)pl->open;
	const uint32_t v = ((uint32_t)OPENMARK << 16) | OPENMARK;
	for (i = 0; i < longs/2; i++)
	{
		*open++ = v;
		*open++ = v;
	}
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

	tail = vd.visplanes_hash[hash];
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

	if (vd.lastvisplane == vd.visplanes + MAXVISPLANES)
		return vd.visplanes;

	// make a new plane
	check = vd.lastvisplane;
	++vd.lastvisplane;

	check->height = height;
	check->flatandlight = flatandlight;
	check->minx = start;
	check->maxx = stop;

	R_MarkOpenPlane(check);

	check->next = tail;
	vd.visplanes_hash[hash] = check;

	return check;
}

visplane_t* R_FindPlane2(fixed_t height, 
	int flatandlight)
{
	visplane_t *check, *tail, *next;
	int hash = R_PlaneHash(height, flatandlight);

	tail = vd.visplanes_hash[hash];
	for (check = tail; check; check = next)
	{
		next = check->next;

		if (height == check->height && // same plane as before?
			flatandlight == check->flatandlight)
			return check; // use the same one as before
	}

	if (vd.lastvisplane == vd.visplanes + MAXVISPLANES)
		return vd.visplanes;

	// make a new plane
	check = vd.lastvisplane;
	++vd.lastvisplane;

	check->height = height;
	check->flatandlight = flatandlight;
	check->minx = 320;
	check->maxx = -1;

	R_MarkOpenPlane(check);

	check->next = tail;
	vd.visplanes_hash[hash] = check;

	return check;
}

void R_BSP (void) __attribute__((noinline));
void R_WallPrep (void) __attribute__((noinline));
void R_SpritePrep (void) __attribute__((noinline));
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
	visplane_t *visplanes_hash_[NUM_VISPLANES_BUCKETS];
	sector_t *vissectors_[MAXVISSSEC];
	viswallextra_t viswallex_[MAXWALLCMDS + 1] __attribute__((aligned(16)));

	if (leveltime < 30) // Whole screen is black right now anyway
		return;

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

	R_Setup(displayplayer, visplanes_, visplanes_hash_, vissectors_, viswallex_);

#ifndef JAGUAR
	R_BSP();

	R_WallPrep();
	/* the rest of the refresh can be run in parallel with the next game tic */

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
	boolean drawworld = true;//!(optionsMenuOn);
	__attribute__((aligned(16)))
		visplane_t visplanes_[MAXVISPLANES];
	__attribute__((aligned(16)))
		visplane_t *visplanes_hash_[NUM_VISPLANES_BUCKETS];
	sector_t *vissectors_[(MAXVISSSEC > MAXVISSPRITES ? MAXVISSSEC : MAXVISSPRITES) + 1];
	viswallextra_t viswallex_[MAXWALLCMDS + 1] __attribute__((aligned(16)));

	t_total = I_GetFRTCounter();

	R_Setup(displayplayer, visplanes_, visplanes_hash_, vissectors_, viswallex_);

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

	t_prep = I_GetFRTCounter() - t_prep;

	t_segs = I_GetFRTCounter();
	R_SegCommands();
	t_segs = I_GetFRTCounter() - t_segs;

	Mars_ClearCacheLine(&vd.lastsegclip);

	if (vd.lastsegclip - vd.segclip > MAXOPENINGS)
		I_Error("lastsegclip > MAXOPENINGS: %d", vd.lastsegclip - vd.segclip);
	if (vd.lastvissector - vd.vissectors > MAXVISSSEC)
		I_Error("lastvissector > MAXVISSSEC: %d", vd.lastvissector - vd.vissectors);

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
