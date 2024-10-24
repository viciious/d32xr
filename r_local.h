/* R_local.h */

#ifndef __R_LOCAL__
#define __R_LOCAL__

#include "doomdef.h"

extern int16_t viewportWidth, viewportHeight;
extern int16_t centerX, centerY;
extern boolean lowResMode;
extern fixed_t centerXFrac, centerYFrac;
extern fixed_t stretch;
extern fixed_t stretchX;

#define	PROJECTION			centerXFrac

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
#define	BOSSFLASHCOLORMAP		33*256
#define YELLOWTEXTCOLORMAP      34*256
#endif

#ifdef MARS
#define HWLIGHT(light) ((((255 - (light)) >> 3) & 31) * 256)
#else
#define HWLIGHT(light) -((255 - (light)) << 14) & 0xffffff
#endif

#define MINLIGHT 0

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
	VINT		validcount;			/* if == validcount, already checked */
	VINT		linecount;
	uint8_t		floorpic, ceilingpic;	/* if ceilingpic == (uint8_t)-1,draw sky */

	uint8_t		lightlevel, special;

	VINT		tag;
	// killough 3/7/98: support flat heights drawn at another sector's heights
  	VINT        heightsec;    // other sector, or -1 if no other sector

//	uint8_t     floor_xoffs;
//	uint8_t     floor_yoffs;

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
	VINT 		v1, v2;
	VINT		sidenum[2];			/* sidenum[1] will be -1 if one sided */
	uint16_t	flags;
	uint8_t		special, tag;
} line_t;

#define LD_FRONTSECTOR(ld) (&sectors[sides[(ld)->sidenum[0]].sector])
#define LD_BACKSECTOR(ld) ((ld)->sidenum[1] != -1 ? &sectors[sides[ld->sidenum[1]].sector] : NULL)

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
	fixed_t		x,y,dx,dy;			/* partition line */
	fixed_t		bbox[2][4];			/* bounding box for each child */
	int			children[2];		/* if NF_SUBSECTOR its a subsector */
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
	VINT size;
	VINT pad;
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
extern	vertex_t	*vertexes;

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
	fixed_t	dx,dy;
	fixed_t	left, right;

	dx = (x - node->x);
	dy = (y - node->y);

#ifdef MARS
   left = ((int64_t)node->dy*dx) >> 32;
   right = ((int64_t)dy*node->dx) >> 32;
#else
   left  = (node->dy>>FRACBITS) * (dx>>FRACBITS);
   right = (dy>>FRACBITS) * (node->dx>>FRACBITS);
#endif

   return (left <= right);
}

//
// To get a global angle from Cartesian coordinates, the coordinates are
// flipped until they are in the first octant of the coordinate system,
// then the y (<= x) is scaled and divided by x to get a tangent (slope)
// value which is looked up in the tantoangle table.
//
#define R_PointToAngle(x,y) R_PointToAngle2(vd.viewx,vd.viewy,x,y)
void	R_InitData (void);
void	R_SetViewportSize(int num);
int		R_DefaultViewportSize(void); // returns the viewport id for fullscreen, low detail mode
void	R_SetDrawMode(void);
void    R_SetFlatData(int f, uint8_t *start, int size);
void    R_ResetTextures(void);
void	R_SetupLevel(int gamezonemargin);
void	R_SetupTextureCaches(int gamezonemargin);
// killough 4/13/98: fake floors/ceilings for deep water / fake ceilings:
sector_t *R_FakeFlat(sector_t *, sector_t *, boolean) ATTR_DATA_CACHE_ALIGN;

typedef void (*drawcol_t)(int, int, int, int, fixed_t, fixed_t, inpixel_t*, int);
typedef void (*drawskycol_t)(int, int, int);
typedef void (*drawspan_t)(int, int, int, int, fixed_t, fixed_t, fixed_t, fixed_t, inpixel_t*, int);

extern drawcol_t drawcol;
extern drawcol_t drawcolnpo2;
extern drawcol_t drawcollow;
extern drawspan_t drawspan;

#ifdef MDSKY
extern drawskycol_t drawskycol;
#endif

#ifdef HIGH_DETAIL_SPRITES
extern drawcol_t drawspritecol;
#endif

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

extern  fixed_t *yslopetab;
extern	fixed_t *yslope/*[SCREENHEIGHT]*/;
extern	fixed_t *distscale/*[SCREENWIDTH]*/;

#define OPENMARK 0xff00
#ifdef MARS
#define MARKEDOPEN(x) ((int8_t)((x)>>8) == -1)
#else
#define MARKEDOPEN(x) ((x) == OPENMARK)
#endif

extern	VINT		extralight;

#ifdef MARS
__attribute__((aligned(4)))
#endif
extern unsigned int phi_line_bit_shift[8];	// Last index unused; only for making the compiler happy.

#ifdef MARS
__attribute__((aligned(4)))
#endif
extern unsigned int phi_line;

#ifdef MARS
__attribute__((aligned(4)))
#endif
extern boolean phi_effects;

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
extern	texture_t	*skytexturep;

extern	VINT		numtextures;
extern	texture_t	*textures;
extern 	boolean 	texmips;

extern 	VINT 		numdecals;
extern 	texdecal_t  *decals;

extern	uint8_t			*flattranslation;		/* for global animation */
extern	uint8_t			*texturetranslation;	/* for global animation */
extern	flattex_t		*flatpixels;
VINT CalcFlatSize(int lumplength);

extern	VINT		firstflat, numflats;

extern	VINT		firstsprite, numsprites;

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
void R_InitColormap();
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
boolean R_InTexCache(r_texcache_t* c, void *p) ATTR_DATA_CACHE_ALIGN;
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
	unsigned	offset;
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

	VINT 	m_texturenum;
	uint16_t     tb_texturenum; // t_texturenum top word, b_texturenum bottom word

	uint16_t     floorceilpicnum; // ceilingpicnum top word, floorpicnum bottom word (just like a ceiling and floor!)

	uint16_t	newmiplevels; // 0 is lower, 1 is upper

	int			scalestep;		/* polar angle to start at phase1, then scalestep after phase2 */
	unsigned	scalefrac;
	unsigned	scale2;

	short	actionbits;
	short	seglightlevel;

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

#define UPPER8(x) ((uint8_t)((uint16_t)x >> 8))
#define LOWER8(x) ((uint8_t)((uint16_t)x & 0xff))
#define SETUPPER8(x, y) {\
x &= 0x00ff; \
x |= ((uint16_t)y << 8); \
}
#define SETLOWER8(x, y) {\
x &= 0xff00; \
x |= ((uint16_t)y & 0xff); \
}
#define UPPER16(x) ((uint16_t)((uint32_t)x >> 16))
#define LOWER16(x) ((uint16_t)((uint32_t)x & 0xffff))
#define SETUPPER16(x, y) {\
x &= 0x0000ffff; \
x |= ((uint32_t)y << 16); \
}
#define SETLOWER16(x, y) {\
x &= 0xffff0000; \
x |= ((uint32_t)y & 0xffff); \
}

typedef struct
{
	fixed_t 	floorheight, floornewheight, ceilnewheight, pad;
} viswallextra_t;

#define	MAXWALLCMDS		140

/* A vissprite_t is a thing that will be drawn during a refresh */
typedef struct vissprite_s
{
	VINT		x1, x2;			/* clipped to screen edges column range */
	fixed_t		startfrac;		/* horizontal position of x1 */
	fixed_t		xscale;
	fixed_t		xiscale;		/* negative if flipped */
	fixed_t		yscale;
	fixed_t		texturemid;
	uint16_t 	patchnum;		// upper byte indicates high detail draw
	uint16_t	colormap;
	short		gx,gy;	/* global coordinates */
	VINT        heightsec;
	VINT		patchheight;
//	void 		*colormaps;
#ifndef MARS
	pixel_t		*pixels;		/* data patch header references */
#endif
} vissprite_t;

#define	MAXVISSPRITES	MAXWALLCMDS

#define	MAXOPENINGS		SCREENWIDTH*20

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
	subsector_t *viewsubsector;
	fixed_t		viewcos, viewsin;
	player_t	*viewplayer;
	VINT		lightlevel;
	VINT		extralight;
	VINT		displayplayer;
	VINT		fixedcolormap;
	angle_t		clipangle, doubleclipangle;
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
	visplane_t * volatile * visplanes_hash;

	/* */
	/* openings / misc refresh memory */
	/* */
	unsigned short	* volatile segclip, * volatile lastsegclip;

	int * volatile gsortedsprites;

	uint8_t *columncache[2]; // composite column cache for both CPUs
} viewdef_t;

extern	viewdef_t	vd;

extern texture_t *testtex;

#endif		/* __R_LOCAL__ */

