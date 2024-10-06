/* R_local.h */

#ifndef __R_LOCAL__
#define __R_LOCAL__

#include "doomdef.h"

extern int16_t viewportWidth, viewportHeight;
extern int16_t centerX, centerY;
extern fixed_t centerXFrac, centerYFrac;
extern fixed_t stretch;
extern fixed_t stretchX;
extern VINT weaponYpos;
extern fixed_t weaponXScale;

#define	PROJECTION			centerXFrac

#define	PSPRITEXSCALE		FRACUNIT	
#define	PSPRITEYSCALE		FRACUNIT
#define	PSPRITEXISCALE		FRACUNIT
#define	PSPRITEYISCALE		FRACUNIT

#define	ANGLETOSKYSHIFT		22		/* sky map is 256*128*4 maps */

#define	BASEYCENTER			100

#define	WINDOWHEIGHT		(viewportHeight-SBARHEIGHT)

#define	MINZ				(FRACUNIT*4)

#define	FIELDOFVIEW			2048   /* fineangles in the SCREENWIDTH wide window */

/* */
/* lighting constants */
/* */
#ifdef JAGUAR
#define	LIGHTLEVELS			256		/* number of diminishing */
#define	INVERSECOLORMAP		255
#else
#define	INVERSECOLORMAP		32*256
#endif

#ifdef MARS
#define HWLIGHT(light) ((((255 - (light)) >> 3) & 31) * 256)
#else
#define HWLIGHT(light) -((255 - (light)) << 14) & 0xffffff
#endif

#define MINLIGHT 0

typedef enum
{
	detmode_potato = -1,
	detmode_normal,

	MAXDETAILMODES
} detailmode_t;

extern VINT detailmode;
extern VINT viewportnum;
extern VINT anamorphicview;

/*
==============================================================================

					INTERNAL MAP TYPES

==============================================================================
*/

/*================ used by play and refresh */

typedef struct
{
	fixed_t		x,y;
} vertex_t;

struct line_s;

typedef	struct
{
	fixed_t		floorheight, ceilingheight;
	VINT		floorpic, ceilingpic;	/* if ceilingpic == -1,draw sky */

	uint8_t		lightlevel, special;

	VINT		validcount;			/* if == validcount, already checked */
	VINT		linecount;

	VINT		tag;

	mobj_t		*soundtarget;		/* thing that made a sound (or null) */
	
	VINT		blockbox[4];		/* mapblock bounding box for height changes */
	VINT		soundorg[2];		/* for any sounds played by the sector */

	mobj_t		*thinglist;			/* list of mobjs in sector */
	void		*specialdata;		/* thinker_t for reversable actions */
	VINT		*lines;				/* [linecount] size */
} sector_t;

typedef struct
{
	VINT		sector;
	uint8_t		toptexture, bottomtexture, midtexture;
	uint8_t		rowoffset;			/* add this to the calculated texture top */
	int16_t		textureoffset;		/* 8.4, add this to the calculated texture col */
} side_t;

typedef struct line_s
{
	VINT		sidenum[2];			/* sidenum[1] will be -1 if one sided */
	VINT 		v1, v2;
	uint8_t		flags, special;
	uint16_t	tag:14;
	uint16_t	moreflags:2;
} line_t;

#define LD_FRONTSECTOR(ld) (&sectors[sides[(ld)->sidenum[0]].sector])
#define LD_BACKSECTOR(ld) ((ld)->sidenum[1] != -1 ? &sectors[sides[ld->sidenum[1]].sector] : NULL)

#define LD_MFLAG_SEEN 		0x01
#define LD_MFLAG_POSITIVE 	0x02

typedef struct subsector_s
{
	VINT		numlines;
	VINT		firstline;
	sector_t	*sector;
} subsector_t;

typedef struct seg_s
{
	VINT 		v1, v2;
	VINT		sideoffset;
	VINT		linedef;
} seg_t;


typedef struct
{
	int16_t		x,y,dx,dy;			/* partition line */
	uint16_t	children[2];		/* if NF_SUBSECTOR its a subsector */
	uint16_t 	encbbox[2]; 		/* encoded bounding box for each child */
} node_t;

#define MIPLEVELS 1

typedef struct
{
	VINT	mincol, maxcol;
	VINT	minrow, maxrow;
	VINT 	texturenum;
} texdecal_t;

typedef struct
{
	char		name[8];		/* for switch changing, etc */
	VINT		width;
	VINT		height;
	VINT		lumpnum;
	uint16_t	decals;
#if MIPLEVELS > 1
	VINT		mipcount;
#endif
#ifdef MARS
	inpixel_t 	*data[MIPLEVELS];
#else
	pixel_t		*data[MIPLEVELS];			/* cached data to draw from */
#endif
#ifndef MARS
	int			usecount;		/* for precaching */
	int			pad;
#endif
} texture_t;

typedef struct
{
#ifdef MARS
	inpixel_t 	*data[MIPLEVELS];
#else
	pixel_t		*data[MIPLEVELS];			/* cached data to draw from */
#endif
} flattex_t;

/*
==============================================================================

						OTHER TYPES

==============================================================================
*/

/* Sprites are patches with a special naming convention so they can be  */
/* recognized by R_InitSprites.  The sprite and frame specified by a  */
/* thing_t is range checked at run time. */
/* a sprite is a patch_t that is assumed to represent a three dimensional */
/* object and may have multiple rotations pre drawn.  Horizontal flipping  */
/* is used to save space. Some sprites will only have one picture used */
/* for all views.   */

typedef struct
{
	VINT		lump;	/* lump to use for view angles 0-7 */
						/* if lump[1] == -1, use 0 for any position */
} spriteframe_t;

typedef struct
{
	short			numframes;
	short			firstframe; /* index in the spriteframes array */
} spritedef_t;

extern	spriteframe_t	*spriteframes;
extern	VINT 			*spritelumps;
extern	spritedef_t		sprites[NUMSPRITES];

/*
===============================================================================

							MAP DATA

===============================================================================
*/

extern	int			numvertexes;
extern	mapvertex_t	*vertexes;

extern	int			numsegs;
extern	seg_t		*segs;

extern	int			numsectors;
extern	sector_t	*sectors;

extern	int			numsubsectors;
extern	subsector_t	*subsectors;

extern	int			numnodes;
extern	node_t		*nodes;

extern	int			numlines;
extern	line_t		*lines;

extern	int			numsides;
extern	side_t		*sides;

extern 	int16_t 	worldbbox[4];

/*============================================================================= */

extern VINT viewportNum;
extern const VINT numViewports;

/*
===============================================================================
=
= R_PointOnSide
=
= Returns side 0 (front) or 1 (back)
===============================================================================
*/
ATTR_DATA_CACHE_ALIGN
static inline int R_PointOnSide (int x, int y, node_t *node)
{
	int32_t	dx,dy;
	int32_t r1, r2;

	dx = x - ((fixed_t)node->x << FRACBITS);
#ifdef MARS
	dx = (unsigned)dx >> FRACBITS;
	__asm volatile(
		"muls.w %0,%1\n\t"
		: : "r"(node->dy), "r"(dx) : "macl", "mach");
#else
	dx >>= FRACBITS;
	r1 = node->dy * dx;
#endif

	dy = y - ((fixed_t)node->y << FRACBITS);
#ifdef MARS
	dy = (unsigned)dy >> FRACBITS;
	__asm volatile(
		"sts macl, %0\n\t"
		"muls.w %2,%3\n\t"
		"sts macl, %1\n\t"
		: "=&r"(r1), "=&r"(r2) : "r"(dy), "r"(node->dx) : "macl", "mach");
#else
	dy >>= FRACBITS;
	r2 = dy * node->dx;
#endif

    return (r1 <= r2);
}

//
// To get a global angle from Cartesian coordinates, the coordinates are
// flipped until they are in the first octant of the coordinate system,
// then the y (<= x) is scaled and divided by x to get a tangent (slope)
// value which is looked up in the tantoangle table.
//
void	R_InitData (void);
void	R_SetViewportSize(int num);
int		R_DefaultViewportSize(void); // returns the viewport id for fullscreen, low detail mode
void	R_SetDrawFuncs(void);
void 	R_SetTextureData(texture_t *tex, uint8_t *start, int size, boolean skipheader);
void 	R_SetFlatData(int f, uint8_t *start, int size);
void	R_ResetTextures(void);
void	R_SetupLevel(int gamezonemargin);

// how much memory should be left free in the main zone after allocating the texture cache
// can be increased for the Icon of Sin
#define DEFAULT_GAME_ZONE_MARGIN 8*1024

typedef void (*drawcol_t)(int, int, int, int, fixed_t, fixed_t, inpixel_t*, int);
typedef void (*drawspan_t)(int, int, int, int, fixed_t, fixed_t, fixed_t, fixed_t, inpixel_t*, int);

extern drawcol_t drawcol;
extern drawcol_t drawfuzzcol;
extern drawcol_t drawcolnpo2;
extern drawspan_t drawspan;

#define FUZZTABLE		64
#define FUZZMASK		(FUZZTABLE-1)
extern short fuzzoffset[FUZZTABLE];

/* to get a global angle from cartesian coordinates, the coordinates are */
/* flipped until they are in the first octant of the coordinate system, then */
/* the y (<=x) is scaled and divided by x to get a tangent (slope) value */
/* which is looked up in the tantoangle[] table.  The +1 size is to handle */
/* the case when x==y without additional checking. */
#define	SLOPERANGE	2048
#define	SLOPEBITS	11
#define	DBITS		(FRACBITS-SLOPEBITS)

#ifdef MARS
extern angle_t* const tantoangle;
#else
extern const angle_t tantoangle[SLOPERANGE + 1];
#endif

extern	fixed_t *yslope/*[SCREENHEIGHT]*/;
extern	uint16_t *distscale/*[SCREENWIDTH]*/;

#define OPENMARK 0xff00
#ifdef MARS
#define MARKEDOPEN(x) ((int8_t)((x)>>8) == -1)
#else
#define MARKEDOPEN(x) ((x) == OPENMARK)
#endif

extern	VINT		extralight;

#ifdef MARS
__attribute__((aligned(16)))
#endif
extern pixel_t* viewportbuffer;

/* The viewangletox[viewangle + FINEANGLES/4] lookup maps the visible view */
/* angles  to screen X coordinates, flattening the arc to a flat projection  */
/* plane.  There will be many angles mapped to the same X.  */
extern	VINT	*viewangletox/*[FINEANGLES/2]*/;

/* The xtoviewangleangle[] table maps a screen pixel to the lowest viewangle */
/* that maps back to x ranges from clipangle to -clipangle */
extern	uint16_t		*xtoviewangle/*[SCREENWIDTH+1]*/;

#ifdef MARS
extern	const fixed_t		finetangent_[FINEANGLES/4];
fixed_t finetangent(angle_t angle) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_EXTREME;
#else
extern	const fixed_t		finetangent_[FINEANGLES/2];
#define finetangent(x)		finetangent_[x]
#endif

extern	VINT			*validcount;
extern	VINT			framecount;

#ifndef MARS
extern	int		phasetime[9];
#endif



/* */
/* R_data.c */
/* */
extern	inpixel_t	*skytexturep;
extern 	int8_t 		*skycolormaps;
extern 	VINT 		col2sky;

extern	VINT		numtextures;
extern	texture_t	*textures;
extern 	boolean 	texmips;

extern 	VINT 		numdecals;
extern 	texdecal_t  *decals;

extern	uint8_t			*flattranslation;		/* for global animation */
extern	uint8_t			*texturetranslation;	/* for global animation */
extern	flattex_t		*flatpixels;

extern	VINT		firstflat, numflats, col2flat;

extern	VINT		firstsprite, numsprites, numspriteframes;

extern int8_t* dc_colormaps;
extern int8_t* dc_colormaps2;

extern uint8_t* dc_playpals;

#ifdef MARS
#define R_CheckPixels(lumpnum) (void *)(W_POINTLUMPNUM(lumpnum))

// auto-detect presence of jagobj_t header
void *R_SkipJagObjHeader(void *data, int size, int width, int height);
#endif

void R_InitTextures(void);
void R_InitFlats(void);
int	R_FlatNumForName(const char* name);
int	R_CheckTextureNumForName(const char* name);
void	R_InitMathTables(void);
void	R_InitSpriteDefs(const char** namelist);
void R_InitColormap(void);
boolean R_CompositeColumn(int colnum, int numdecals, texdecal_t *decals, inpixel_t *src, inpixel_t *dst, int height, int miplevel) ATTR_DATA_CACHE_ALIGN;

/*
==============================================================================

					TEXTURE CACHING

==============================================================================
*/
typedef struct
{
	int reqcount_le;
	int reqfreed;

	int zonesize;
	void* zone;
} r_texcache_t;

typedef struct
{
	uint16_t pixels;
	VINT lifecount;
	void** userp;
	void *userpold;
} texcacheblock_t;

extern r_texcache_t r_texcache;

#define CACHE_FRAMES_DEFAULT 15

void R_InitTexCache(r_texcache_t* c);
void R_InitTexCacheZone(r_texcache_t* c, int zonesize);
void R_AddToTexCache(r_texcache_t* c, int id, int pixels, void **userp);
void R_ClearTexCache(r_texcache_t* c);
int R_InTexCache(r_texcache_t* c, void *p) ATTR_DATA_CACHE_ALIGN;
boolean R_TouchIfInTexCache(r_texcache_t* c, void *p);
void R_PostTexCacheFrame(r_texcache_t* c);

/*
==============================================================================

					COMMAND QUE STRUCTURES

==============================================================================
*/

#define	AC_ADDFLOOR			1
#define	AC_ADDCEILING		2
#define	AC_TOPTEXTURE		4
#define	AC_BOTTOMTEXTURE	8
#define	AC_NEWCEILING		16
#define	AC_NEWFLOOR			32
#define	AC_TOPSIL			64
#define	AC_BOTTOMSIL		128
#define	AC_SOLIDSIL			256
#define	AC_ADDSKY			512
#define	AC_DRAWN			1024
#define	AC_MIDTEXTURE		2048

typedef struct
{
/* */
/* filled in by early prep */
/* */
	/* !!! THE FOLLOWING SECTION MUST BE LARGE ENOUGH */
	/* !!! TO ACCOMODATE VISSPRITE_T STRUCTURE, GETS */
	/* !!! OVERWRITTEN AFTER PHASE 7 - BEGIN */
	unsigned	centerangle;
	fixed_t 	offset;
	unsigned	distance;

	int			t_topheight;
	int			t_bottomheight;
	int			t_texturemid;

	int			b_bottomheight;
	int			b_texturemid;
	int			b_topheight;

	/* !!! THE SECTION ABOVE MUST BE LARGE ENOUGH */
	/* !!! TO ACCOMODATE VISSPRITE_T STRUCTURE, GETS */
	/* !!! OVERWRITTEN AFTER PHASE 7 - END */

	int 		m_texturemid;

	VINT 		m_texturenum;
	VINT		t_texturenum;
	VINT		b_texturenum;

	VINT        floorpicnum;
	VINT        ceilingpicnum;

	fixed_t		scalestep;		/* polar angle to start at phase1, then scalestep after phase2 */
	fixed_t		scalefrac;
	fixed_t		scale2;

	short	actionbits;
	short	seglightlevel;
	int16_t miplevels[2];

/* */
/* filled in by bsp */
/* */
	VINT			start, realstart;
	VINT			stop, realstop;					/* inclusive x coordinates */
	union
	{
		seg_t			*seg;
		mapvertex_t		v1;
	};

	union
	{
		fixed_t			ceilingheight;
		mapvertex_t		v2;
	};

	uint16_t 		*clipbounds;
} viswall_t;

typedef struct
{
	fixed_t 	floorheight, floornewheight, ceilnewheight, pad;
} viswallextra_t;

#define	MAXWALLCMDS		165

/* A vissprite_t is a thing that will be drawn during a refresh */
typedef struct vissprite_s
{
	VINT		x1, x2;			/* clipped to screen edges column range */
	fixed_t		startfrac;		/* horizontal position of x1 */
	fixed_t		xscale;
	fixed_t		xiscale;		/* negative if flipped */
	fixed_t		yscale;
	fixed_t		texturemid;
	VINT 		patchnum;
	VINT		colormap;		/* < 0 = shadow draw */
	short		gx,gy;	/* global coordinates */
	void 		*colormaps;
#ifndef MARS
	pixel_t		*pixels;		/* data patch header references */
#endif
} vissprite_t;

#define	MAXVISSPRITES	MAXWALLCMDS

#define	MAXOPENINGS		SCREENWIDTH*18

#define	MAXVISSSEC		128

typedef struct visplane_s
{
	fixed_t		height;
	VINT		minx, maxx;
	int 		flatandlight;
	struct visplane_s	*next;
	unsigned short		*open/*[SCREENWIDTH+2]*/;		/* top<<8 | bottom */ /* leave pads for [minx-1]/[maxx+1] */
} visplane_t;

#define	MAXVISPLANES	32

#define NUM_VISPLANES_BUCKETS 32

void R_MarkOpenPlane(visplane_t* pl)
ATTR_DATA_CACHE_ALIGN
;

visplane_t *R_FindPlane(fixed_t height, int flatandlight,
	int start, int stop)
ATTR_DATA_CACHE_ALIGN
;

void R_InitClipBounds(uint32_t *clipbounds)
ATTR_DATA_CACHE_ALIGN
;

#define MAX_COLUMN_LENGTH 128
#if MIPLEVELS > 1
#define COLUMN_CACHE_SIZE MAX_COLUMN_LENGTH * 2
#else
#define COLUMN_CACHE_SIZE MAX_COLUMN_LENGTH
#endif

typedef struct
#ifdef MARS
__attribute__((aligned(16)))
#endif
{
	fixed_t		viewx, viewy, viewz;
	angle_t		viewangle;
	fixed_t		viewcos, viewsin;
	pspdef_t	*psprites;
	VINT		lightlevel;
	VINT		extralight;
	VINT		displayplayer;
	VINT		fixedcolormap;
	VINT		fuzzcolormap;
	angle_t		clipangle;
	VINT 		*viewangletox;

	/* */
	/* walls */
	/* */
	viswall_t * volatile viswalls/*[MAXWALLCMDS] __attribute__((aligned(16)))*/;
	viswall_t * volatile lastwallcmd;
	viswallextra_t * volatile viswallextras;

	/* */
	/* sprites */
	/* */
	viswall_t	* volatile vissprites/*[MAXVISSPRITES]*/, * volatile  lastsprite_p, * volatile vissprite_p;

	/* */
	/* subsectors */
	/* */
	sector_t		**vissectors/*[MAXVISSSEC]*/, **lastvissector;

	/* */
	/* planes */
	/* */
	visplane_t		* volatile visplanes/*[MAXVISPLANES]*/, * volatile lastvisplane;
	int * volatile gsortedvisplanes;
	visplane_t **visplanes_hash; /* only accessible by the second SH2! */

	/* */
	/* openings / misc refresh memory */
	/* */
	unsigned short	* volatile segclip, * volatile lastsegclip;

	int * volatile gsortedsprites;

	uint8_t *columncache[2]; // composite column cache for both CPUs
} viewdef_t;

extern	viewdef_t	*vd;

#endif		/* __R_LOCAL__ */

