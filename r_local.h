/* R_local.h */

#ifndef __R_LOCAL__
#define __R_LOCAL__

/* proper screen size would be 160*100, stretched to 224 is 2.2 scale */
#define	STRETCH				(22*FRACUNIT/10)

#define	CENTERX				(SCREENWIDTH/2)
#define	CENTERY				(SCREENHEIGHT/2)
#define	CENTERXFRAC			(SCREENWIDTH/2*FRACUNIT)
#define	CENTERYFRAC			(SCREENHEIGHT/2*FRACUNIT)
#define	PROJECTION			CENTERXFRAC

#define	PSPRITEXSCALE		FRACUNIT	
#define	PSPRITEYSCALE		FRACUNIT
#define	PSPRITEXISCALE		FRACUNIT
#define	PSPRITEYISCALE		FRACUNIT

#define	ANGLETOSKYSHIFT		22		/* sky map is 256*128*4 maps */

#define	BASEYCENTER			100

#define	CENTERY				(SCREENHEIGHT/2)
#define	WINDOWHEIGHT		(SCREENHEIGHT-SBARHEIGHT)

#define	MINZ				(FRACUNIT*4)

#define	FIELDOFVIEW			2048   /* fineangles in the SCREENWIDTH wide window */

/* */
/* lighting constants */
/* */
#define	LIGHTLEVELS			256		/* number of diminishing */
#define	INVERSECOLORMAP		255

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
	VINT		lightlevel;
	VINT		special, tag;

	VINT		soundtraversed;		/* 0 = untraversed, 1,2 = sndlines -1 */
	mobj_t		*soundtarget;		/* thing that made a sound (or null) */
	
	VINT		blockbox[4];		/* mapblock bounding box for height changes */
	degenmobj_t	soundorg;			/* for any sounds played by the sector */

	int			validcount;			/* if == validcount, already checked */
	mobj_t		*thinglist;			/* list of mobjs in sector */
	void		*specialdata;		/* thinker_t for reversable actions */
	VINT		linecount;
	struct line_s	**lines;			/* [linecount] size */
} sector_t;

typedef struct
{
	fixed_t		textureoffset;		/* add this to the calculated texture col */
	fixed_t		rowoffset;			/* add this to the calculated texture top */
	VINT		toptexture, bottomtexture, midtexture;
	sector_t	*sector;
} side_t;

typedef enum {ST_HORIZONTAL, ST_VERTICAL, ST_POSITIVE, ST_NEGATIVE} slopetype_t;

typedef struct line_s
{
	vertex_t	*v1, *v2;
	fixed_t		dx,dy;				/* v2 - v1 for side checking */
	VINT		flags;
	VINT		special, tag;
	VINT		sidenum[2];			/* sidenum[1] will be -1 if one sided */
	fixed_t		bbox[4];
	slopetype_t	slopetype;			/* to aid move clipping */
	sector_t	*frontsector, *backsector;
	int			validcount;			/* if == validcount, already checked */
	void		*specialdata;		/* thinker_t for reversable actions */
	int			fineangle;			/* to get sine / cosine for sliding */
} line_t;

typedef struct subsector_s
{
	sector_t	*sector;
	VINT		numlines;
	VINT		firstline;
} subsector_t;

typedef struct seg_s
{
	vertex_t	*v1, *v2;
	fixed_t		offset;
	angle_t		angle;				/* this is not used (keep for padding) */
	side_t		*sidedef;
	line_t		*linedef;
	sector_t	*frontsector;
	sector_t	*backsector;		/* NULL for one sided lines */
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
	int			width;
	int			height;
	pixel_t		*data;			/* cached data to draw from */
	int			lumpnum;
	int			usecount;		/* for precaching */
	int			pad;
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

#ifdef MARS

int spritelump[NUMSPRITES];	/* no rotations, so just add frame num... */

#else

typedef struct
{
	boolean		rotate;		/* if false use 0 for any position */
	int			lump[8];	/* lump to use for view angles 0-7 */
	byte		flip[8];	/* flip (1 = flip) to use for view angles 0-7 */
} spriteframe_t;

typedef struct
{
	int				numframes;
	spriteframe_t	*spriteframes;
} spritedef_t;

extern	spritedef_t		sprites[NUMSPRITES];

#endif

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

void	R_RenderBSPNode (int bspnum);
void	R_InitData (void);
void	R_InitSpriteDefs (char **namelist);


/* to get a global angle from cartesian coordinates, the coordinates are */
/* flipped until they are in the first octant of the coordinate system, then */
/* the y (<=x) is scaled and divided by x to get a tangent (slope) value */
/* which is looked up in the tantoangle[] table.  The +1 size is to handle */
/* the case when x==y without additional checking. */
#define	SLOPERANGE	2048
#define	SLOPEBITS	11
#define	DBITS		(FRACBITS-SLOPEBITS)

extern	int	tantoangle[SLOPERANGE+1];

extern	unsigned short	yslope[SCREENHEIGHT];		/* 6.10 frac */
extern	unsigned short	distscale[SCREENWIDTH];		/* 1.15 frac */

#define	HEIGHTBITS			6
#define	SCALEBITS			9
	
#define	FIXEDTOSCALE		(FRACBITS-SCALEBITS)
#define	FIXEDTOHEIGHT		(FRACBITS-HEIGHTBITS)


extern	fixed_t		viewx, viewy, viewz;
extern	angle_t		viewangle;
extern	fixed_t		viewcos, viewsin;

extern	player_t	*viewplayer;
extern	boolean		fixedcolormap;
extern	int			extralight;

extern	angle_t		clipangle, doubleclipangle;

/* The viewangletox[viewangle + FINEANGLES/4] lookup maps the visible view */
/* angles  to screen X coordinates, flattening the arc to a flat projection  */
/* plane.  There will be many angles mapped to the same X.  */
extern	int			viewangletox[FINEANGLES/2];

/* The xtoviewangleangle[] table maps a screen pixel to the lowest viewangle */
/* that maps back to x ranges from clipangle to -clipangle */
extern	angle_t		xtoviewangle[SCREENWIDTH+1];

extern	fixed_t		finetangent[FINEANGLES/2];

extern	int			validcount;
extern	int			framecount;

extern	int		phasetime[9];




/* */
/* R_data.c */
/* */
#define	MAXTEXTURES	128

extern	texture_t	*skytexturep;

extern	int			numtextures;
extern	texture_t	textures[MAXTEXTURES];

extern	int			*flattranslation;		/* for global animation */
extern	int			*texturetranslation;	/* for global animation */

extern	int			firstflat, numflats;

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
/* */
/* filled in by bsp */
/* */
	seg_t		*seg;
	int			start;
	int			stop;					/* inclusive x coordinates */
	int			angle1;					/* polar angle to start */

/* */
/* filled in by late prep */
/* */
	pixel_t		*floorpic;
	pixel_t		*ceilingpic;

/* */
/* filled in by early prep */
/* */
	unsigned	actionbits;
	
	int			t_topheight;
	int			t_bottomheight;
	int			t_texturemid;
	texture_t	*t_texture;
	
	int			b_topheight;
	int			b_bottomheight;
	int			b_texturemid;
	texture_t	*b_texture;
	
	int			floorheight;
	int			floornewheight;

	int			ceilingheight;
	int			ceilingnewheight;	
	
	byte		*topsil;
	byte		*bottomsil;

	unsigned	scalefrac;
	unsigned	scale2;
	int			scalestep;
	
	unsigned	centerangle;
	unsigned	offset;
	unsigned	distance;
	unsigned	seglightlevel;
	
} viswall_t;

#define	MAXWALLCMDS		128
extern	viswall_t	viswalls[MAXWALLCMDS], *lastwallcmd;



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
	fixed_t		gx,gy,gz,gzt;	/* global coordinates */
	pixel_t		*pixels;		/* data patch header references */
} vissprite_t;

#define	MAXVISSPRITES	128
extern	vissprite_t	vissprites[MAXVISSPRITES], *lastsprite_p, *vissprite_p;

#define	MAXOPENINGS		SCREENWIDTH*64
extern	unsigned short	openings[MAXOPENINGS], *lastopening;

#define	MAXVISSSEC		256
extern	subsector_t		*vissubsectors[MAXVISSSEC], **lastvissubsector;


typedef struct
{
	fixed_t		height;
	pixel_t		*picnum;
	int			lightlevel;
	int			minx, maxx;
	int			pad1;						/* leave pads for [minx-1]/[maxx+1] */
	unsigned short	open[SCREENWIDTH];		/* top<<8 | bottom */
	int			pad2;
} visplane_t;

#define	MAXVISPLANES	64
extern	visplane_t		visplanes[MAXVISPLANES], *lastvisplane;


#endif		/* __R_LOCAL__ */

