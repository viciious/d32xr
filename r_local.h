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

#define SF_FOF_INVISIBLE_TANGIBLE    1
#define SF_FOF_SWAPHEIGHTS           2
#define SF_FLOATBOB                  4
#define SF_AIRBOB                    8
#define SF_CRUMBLE                  16
#define SF_RESPAWN                  32

typedef	struct
{
	fixed_t		floorheight, ceilingheight;
	void		*specialdata;		/* thinker_t for reversable actions */
	VINT		validcount;			/* if == validcount, already checked */
	uint8_t		floorpic, ceilingpic;	/* if ceilingpic == (uint8_t)-1,draw sky */

	uint8_t		lightlevel, special;

	uint8_t		tag;
	uint8_t     flags;
	VINT     floor_xoffs; // Upper X, Lower Y.

	// killough 3/7/98: support flat heights drawn at another sector's heights
  	VINT        heightsec;    // other sector, or -1 if no other sector

	VINT        fofsec;

	SPTR		thinglist;			/* list of mobjs in sector */
} sector_t;

typedef struct sectorBBox_s
{
	struct sectorBBox_s *prev;
	struct sectorBBox_s *next;

	sector_t *sector;
	VINT bbox[4];
} sectorBBox_t;

sectorBBox_t *P_AddSectorBBox(sector_t *sector_, VINT bbox[4]);
VINT *P_GetSectorBBox(sector_t *sector);

typedef struct
{
	VINT		sector;
	uint8_t		texIndex;
	uint8_t		rowoffset;			/* add this to the calculated texture top */
	int16_t		textureoffset;		/* 8.4, add this to the calculated texture col */
} side_t;

typedef struct
{
	uint8_t toptexture;
	uint8_t midtexture;
	uint8_t bottomtexture;
} sidetex_t;

#define SIDETEX(side) (&sidetexes[(side)->texIndex])

typedef struct line_s
{
	VINT 		v1, v2;
	VINT		sidenum[2];			/* sidenum[1] will be -1 if one sided */
} line_t;

#define LD_FRONTSECTOR(ld) (&sectors[sides[(ld)->sidenum[0]].sector])
#define LD_BACKSECTOR(ld) ((ld)->sidenum[1] != -1 ? &sectors[sides[ld->sidenum[1]].sector] : NULL)

typedef struct subsector_s
{
	VINT		firstline;
	VINT        isector;
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

#ifdef USE_DECALS
typedef struct
{
	VINT	mincol, maxcol;
	VINT	minrow, maxrow;
	VINT 	texturenum;
} texdecal_t;
#endif

typedef struct
{
	char		name[8];		/* for switch changing, etc */
	VINT		width;
	VINT		height;
	VINT		lumpnum;
#ifdef USE_DECALS
	uint16_t	decals;
#endif
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
	VINT wavy;
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

#define LINETAGS_HASH_BSHIFT 	4
#define LINETAGS_HASH_SIZE 		(1<<LINETAGS_HASH_BSHIFT)
#define LINESPECIALS_HASH_BSHIFT 	4
#define LINESPECIALS_HASH_SIZE 		(1<<LINESPECIALS_HASH_BSHIFT)

extern	uint16_t			numvertexes;
extern	uint16_t			numsegs;
extern	uint16_t			numsectors;
extern	uint16_t			numsubsectors;
extern	uint16_t			numnodes;
extern	uint16_t			numlines;
extern	uint16_t			numsides;

extern 	uint16_t 		numlinetags;
extern 	uint16_t 		*linetags;
extern  uint16_t        numlinespecials;
extern  uint16_t        *linespecials;

extern	mapvertex_t	*vertexes;
extern	seg_t		*segs;
extern	sector_t	*sectors;
extern	subsector_t	*subsectors;
extern	node_t		*nodes;
extern	line_t		*lines;
extern  uint16_t    *ldflags;
extern	side_t		*sides;
extern  sidetex_t   *sidetexes;

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
typedef void (*drawspancolor_t)(int, int, int, int);

extern drawcol_t drawcol;
extern drawcol_t drawcolflipped;
extern drawcol_t drawcolnpo2;
extern drawcol_t drawcollow;
extern drawspan_t drawspan;
extern drawspancolor_t drawspancolor;

#ifdef MDSKY
extern drawskycol_t drawskycol;
#endif
extern drawcol_t draw32xskycol;

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
extern	uint16_t *distscale/*[SCREENWIDTH]*/;

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
extern boolean line_table_effects;

#ifdef MARS
__attribute__((aligned(4)))
#endif
extern boolean copper_effects;

#ifdef MARS
__attribute__((aligned(4)))
#endif
extern int copper_color_index;

#ifdef MARS
__attribute__((aligned(2)))
#endif
extern short copper_vertical_offset;

#ifdef MARS
__attribute__((aligned(2)))
#endif
extern short copper_vertical_rate;

#ifdef MARS
__attribute__((aligned(2)))
#endif
extern unsigned short copper_neutral_color;

#ifdef MARS
__attribute__((aligned(2)))
#endif
extern unsigned short copper_table_height;

#ifdef MARS
__attribute__((aligned(4)))
#endif
extern volatile unsigned short *copper_color_table;

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

#ifdef USE_DECALS
extern 	VINT 		numdecals;
extern 	texdecal_t  *decals;
#endif

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
#ifdef USE_DECALS
boolean R_CompositeColumn(int colnum, int numdecals, texdecal_t *decals, inpixel_t *src, inpixel_t *dst, int height, int miplevel) ATTR_DATA_CACHE_ALIGN;
#endif

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
#define	AC_ADDFLOORSKY		4096

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

#ifdef FLOOR_OVER_FLOOR
	int16_t     fofSector;
#else
	uint16_t	newmiplevels; // 0 is lower, 1 is upper
#endif

	fixed_t			scalestep;		/* polar angle to start at phase1, then scalestep after phase2 */
	fixed_t	scalefrac;
	fixed_t	scale2;

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

	fixed_t			floorheight;

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
	fixed_t 	floorheight, floornewheight, ceilnewheight;
	uint32_t    fofInfo;
} viswallextra_t;

#define	MAXWALLCMDS		150

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
	angle_t		viewangle,aimingangle;
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
