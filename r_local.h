/* R_local.h */

#ifndef __R_LOCAL__
#define __R_LOCAL__

#include "doomdef.h"

extern unsigned short viewportWidth, viewportHeight;
extern unsigned short centerX, centerY;
extern fixed_t centerXFrac, centerYFrac;
extern fixed_t stretch;
extern fixed_t stretchX;
extern fixed_t weaponScale;

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
#define	LIGHTLEVELS			256		/* number of diminishing */
#define	INVERSECOLORMAP		255

#ifdef MARS
#define HWLIGHT(light) ((((255 - (light)) >> 3) & 31) * 256)
#else
#define HWLIGHT(light) -((255 - (light)) << 14) & 0xffffff
#endif

#define MINLIGHT 0

typedef enum
{
	detmode_potato = -1,
	detmode_medium,
	detmode_high,

	MAXDETAILMODES
} detailmode_t;

extern detailmode_t detailmode;
extern VINT viewportnum;
extern VINT palstretch;

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
	VINT		floorpic, ceilingpic;	/* if ceilingpic == -1,draw sky */
	VINT		lightlevel;
	VINT		special, tag;

	VINT		soundtraversed;		/* 0 = untraversed, 1,2 = sndlines -1 */
	VINT		validcount;			/* if == validcount, already checked */
	VINT		linecount;

	fixed_t		floorheight, ceilingheight;
	mobj_t		*soundtarget;		/* thing that made a sound (or null) */
	
	VINT		blockbox[4];		/* mapblock bounding box for height changes */
	degenmobj_t	soundorg;			/* for any sounds played by the sector */

	mobj_t		*thinglist;			/* list of mobjs in sector */
	void		*specialdata;		/* thinker_t for reversable actions */
	struct line_s	**lines;			/* [linecount] size */
} sector_t;

typedef struct
{
	VINT		sector;
	VINT		toptexture, bottomtexture, midtexture;
	VINT		textureoffset;		/* add this to the calculated texture col */
	VINT		rowoffset;			/* add this to the calculated texture top */
} side_t;

typedef enum {ST_HORIZONTAL, ST_VERTICAL, ST_POSITIVE, ST_NEGATIVE} slopetype_t;

typedef struct line_s
{
	VINT            slopetype;                      /* to aid move clipping */
	VINT		validcount;			/* if == validcount, already checked */
	VINT		flags;
	VINT		special, tag;
	VINT		sidenum[2];			/* sidenum[1] will be -1 if one sided */
	VINT		fineangle;			/* to get sine / eosine for sliding */
	vertex_t 	*v1, *v2;
} line_t;

#define LD_FRONTSECTORNUM(ld) (sides[(ld)->sidenum[0]].sector)
#define LD_BACKSECTORNUM(ld) ((ld)->sidenum[1] != -1 ? sides[ld->sidenum[1]].sector : -1)
#define LD_FRONTSECTOR(ld) (&sectors[LD_FRONTSECTORNUM(ld)])
#define LD_BACKSECTOR(ld) ((ld)->sidenum[1] != -1 ? &sectors[LD_BACKSECTORNUM(ld)] : NULL)

typedef struct subsector_s
{
	VINT		numlines;
	VINT		firstline;
	sector_t	*sector;
} subsector_t;

typedef struct seg_s
{
	VINT 		side;
	VINT            v1, v2;
	VINT 		angle;
	VINT		offset;
	VINT		linedef;
} seg_t;


typedef struct
{
	fixed_t		x,y,dx,dy;			/* partition line */
	fixed_t		bbox[2][4];			/* bounding box for each child */
	int			children[2];		/* if NF_SUBSECTOR its a subsector */
} node_t;


typedef struct
{
	char		name[8];		/* for switch changing, etc */
	VINT			width;
	VINT			height;
	int			lumpnum;
#ifdef MARS
	inpixel_t 	*data;
#else
	pixel_t		*data;			/* cached data to draw from */
#endif
#ifndef MARS
	int			usecount;		/* for precaching */
	int			pad;
#endif
} texture_t;

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
	short		lump[8];	/* lump to use for view angles 0-7 */
							/* if lump[1] == -1, use 0 for any position */
} spriteframe_t;

typedef struct
{
	short			numframes;
	short			firstframe; /* index in the spriteframes array */
} spritedef_t;

extern	spriteframe_t	*spriteframes;
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

int     R_PointOnSide(int x, int y, node_t *node) ATTR_DATA_CACHE_ALIGN;
int     SlopeDiv(unsigned int num, unsigned int den) ATTR_DATA_CACHE_ALIGN;
void	R_InitData (void);
void	R_SetViewportSize(int num);
void	R_SetDetailMode(int mode);
void	R_SetupLevel(void);
void	R_SetupTextureCaches(void) ATTR_OPTIMIZE_SIZE;

typedef void (*drawcol_t)(int, int, int, int, fixed_t, fixed_t, inpixel_t*, int, int *);
typedef void (*drawspan_t)(int, int, int, int, fixed_t, fixed_t, fixed_t, fixed_t, inpixel_t*);

extern drawcol_t drawcol;
extern drawcol_t drawfuzzycol;
extern drawcol_t drawcolnpo2;
extern drawspan_t drawspan;

#define FUZZTABLE		50 
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

extern	fixed_t yslope[SCREENHEIGHT];
extern	fixed_t distscale[SCREENWIDTH];

#define	HEIGHTBITS			6
#define	FIXEDTOHEIGHT		(FRACBITS-HEIGHTBITS)

#define OPENMARK 0xff00

typedef struct
#ifdef MARS
__attribute__((aligned(16)))
#endif
{
	fixed_t		viewx, viewy, viewz;
	angle_t		viewangle;
	fixed_t		viewcos, viewsin;
} viewdef_t;

extern	viewdef_t	vd;
extern	player_t	*viewplayer;
extern	int			extralight;

extern	angle_t		clipangle, doubleclipangle;

/* The viewangletox[viewangle + FINEANGLES/4] lookup maps the visible view */
/* angles  to screen X coordinates, flattening the arc to a flat projection  */
/* plane.  There will be many angles mapped to the same X.  */
extern	unsigned char	viewangletox[FINEANGLES/2];

/* The xtoviewangleangle[] table maps a screen pixel to the lowest viewangle */
/* that maps back to x ranges from clipangle to -clipangle */
extern	angle_t		xtoviewangle[SCREENWIDTH+1];

#ifdef MARS
extern	const fixed_t		finetangent_[FINEANGLES/4];
fixed_t finetangent(angle_t angle) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_EXTREME;
#else
extern	const fixed_t		finetangent_[FINEANGLES/2];
#define finetangent(x)		finetangent_(x)
#endif

extern	VINT			validcount;
extern	VINT			framecount;

#ifndef MARS
extern	int		phasetime[9];
#endif



/* */
/* R_data.c */
/* */
#define	MAXTEXTURES	200

extern	texture_t	*skytexturep;

extern	int			numtextures;
extern	texture_t	*textures;

extern	VINT			*flattranslation;		/* for global animation */
extern	VINT			*texturetranslation;	/* for global animation */
extern	void			** flatpixels;

extern	int			firstflat, numflats;

void R_InitTextures(void) ATTR_OPTIMIZE_SIZE;
void R_InitFlats(void) ATTR_OPTIMIZE_SIZE;
int	R_FlatNumForName(const char* name) ATTR_OPTIMIZE_SIZE;
int	R_CheckTextureNumForName(const char* name) ATTR_OPTIMIZE_SIZE;
void	R_InitMathTables(void) ATTR_OPTIMIZE_SIZE;
void	R_InitSpriteDefs(const char** namelist) ATTR_OPTIMIZE_SIZE;

/*
==============================================================================

					TEXTURE CACHING

==============================================================================
*/
typedef struct
{
	unsigned short* pixcount; /* capped at 0xffff */
	VINT* framecount;

	int maxobjects;
	int objectsize;

	int bestobj;
	int bestcount;
	int reqcount_le;
	int reqfreed;

	int zonesize;
	void* zone;
} r_texcache_t;

typedef struct
{
	VINT id;
	VINT pixelcount;
	VINT lumpnum;
	VINT lifecount;
	void** userp;
} texcacheblock_t;

extern r_texcache_t r_texcache;

void R_InitTexCache(r_texcache_t* c, int maxobjects);
void R_InitTexCacheZone(r_texcache_t* c, int zonesize);
void R_SetupTexCacheFrame(r_texcache_t* c);
void R_TestTexCacheCandidate(r_texcache_t* c, int id);
void R_AddPixelsToTexCache(r_texcache_t* c, int id, int pixels);
void R_PostTexCacheFrame(r_texcache_t* c);
void R_AddToTexCache(r_texcache_t* c, int id, int pixels, int lumpnum, void **userp);
void R_ClearTexCache(r_texcache_t* c);

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
#define	AC_ADDSKY			64
#define	AC_CALCTEXTURE		128
#define	AC_TOPSIL			256
#define	AC_BOTTOMSIL		512
#define	AC_SOLIDSIL			1024

typedef struct
{
	unsigned short	actionbits;
	unsigned short	seglightlevel;

	unsigned	scalefrac;
	unsigned	scale2;
	int			scalestep;
	
	unsigned	centerangle;
	unsigned	offset;
	unsigned	distance;

/* */
/* filled in by bsp */
/* */
	VINT			start;
	VINT			stop;					/* inclusive x coordinates */
	int			angle1;					/* polar angle to start */
	seg_t* seg;

/* */
/* filled in by early prep */
/* */
	int			t_topheight;
	int			t_bottomheight;
	int			t_texturemid;
	//texture_t	*t_texture;
	
	int			b_topheight;
	int			b_bottomheight;
	int			b_texturemid;
	//texture_t	*b_texture;
	
	int			floorheight;
	int			floornewheight;

	int			ceilingheight;
	int			ceilingnewheight;	
	
	VINT			t_texturenum;	
	VINT			b_texturenum;

	VINT         floorpicnum;   // floorpic #   - CALICO: avoid type ambiguity w/extra field
	VINT         ceilingpicnum; // ceilingpic # - CALICO: avoid type ambiguity w/extra field

	byte		*topsil;
	byte		*bottomsil;
} viswall_t;

#define	MAXWALLCMDS		100
extern	viswall_t *viswalls/*[MAXWALLCMDS] __attribute__((aligned(16)))*/;
extern	viswall_t *lastwallcmd;

/* A vissprite_t is a thing that will be drawn during a refresh */
typedef struct vissprite_s
{
	int			x1, x2;			/* clipped to screen edges column range */
	fixed_t		startfrac;		/* horizontal position of x1 */
	fixed_t		xscale;
	fixed_t		xiscale;		/* negative if flipped */
	fixed_t		yscale;
	fixed_t		yiscale;
	fixed_t		texturemid;
	patch_t		*patch;
	int			colormap;		/* -1 = shadow draw */
	fixed_t		gx,gy,gz;	/* global coordinates */
#ifdef MARS
	inpixel_t 	*pixels;
#else
	pixel_t		*pixels;		/* data patch header references */
#endif
	drawcol_t   drawcol;
} vissprite_t;

#define	MAXVISSPRITES	128
extern	vissprite_t	*vissprites/*[MAXVISSPRITES]*/, * lastsprite_p, * vissprite_p;

#define	MAXOPENINGS		SCREENWIDTH*8
extern	unsigned short	*openings/*[MAXOPENINGS]*/;

#define	MAXVISSSEC		256
extern	subsector_t		**vissubsectors/*[MAXVISSSEC]*/, ** lastvissubsector;

typedef struct visplane_s
{
	VINT		minx, maxx;
	VINT 		flatnum;
	VINT		runopen;
	fixed_t		height;
	int			lightlevel;
	unsigned short		*open/*[SCREENWIDTH+2]*/;		/* top<<8 | bottom */ /* leave pads for [minx-1]/[maxx+1] */
	struct visplane_s *next;
} visplane_t;

#define	MAXVISPLANES	48
extern	visplane_t		*visplanes/*[MAXVISPLANES]*/, *lastvisplane;

int R_PlaneHash(fixed_t height, unsigned flatnum, unsigned lightlevel)
ATTR_DATA_CACHE_ALIGN
;

void R_MarkOpenPlane(visplane_t* pl)
ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE
;

visplane_t *R_FindPlane(visplane_t *ignore, int hash, fixed_t height, unsigned flatnum,
                               unsigned lightlevel, int start, int stop)
ATTR_DATA_CACHE_ALIGN
;

#endif		/* __R_LOCAL__ */

