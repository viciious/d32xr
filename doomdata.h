/* DoomData.h */

/* all external data is defined here */
/* most of the data is loaded into different structures at run time */

#ifndef __DOOMDATA__
#define __DOOMDATA__

#ifndef __BYTEBOOL__
#define __BYTEBOOL__
typedef enum {false, true} boolean;
typedef unsigned char byte;
#endif

/*
===============================================================================

						map level types

===============================================================================
*/

/* lump order in a map wad */
enum {ML_LABEL, ML_THINGS, ML_LINEDEFS, ML_SIDEDEFS, ML_SIDETEX, ML_VERTEXES, ML_SEGS,
ML_SSECTORS, ML_NODES, ML_SECTORS , ML_REJECT, ML_BLOCKMAP};


typedef struct
{
	short		x,y;
} mapvertex_t;

typedef struct
{
	int16_t sector;
	uint8_t texIndex;
	uint8_t rowoffset;     // add this to the calculated texture top
	int16_t textureoffset; // 8.4, add this to the calculated texture col
} mapsidedef_t;

typedef struct
{
	short		v1, v2;
	short		sidenum[2];			/* sidenum[1] will be -1 if one sided */
	uint16_t	flags;
	uint8_t		special, tag;
} maplinedef_t;

#define	ML_BLOCKING			1
#define	ML_BLOCKMONSTERS	2
#define ML_HAS_SPECIAL_OR_TAG 4 // Have to reference hash tables to find special/tag

/* if a texture is pegged, the texture will have the end exposed to air held */
/* constant at the top or bottom of the texture (stairs or pulled down things) */
/* and will move with a height change of one of the neighbor sectors */
/* Unpegged textures allways have the first row of the texture at the top */
/* pixel of the line for both top and bottom textures (windows) */
#define	ML_DONTPEGTOP		8
#define	ML_DONTPEGBOTTOM	16
#define ML_CULLING			32	/* Cull this line by distance */
#define ML_NOCLIMB			64
#define	ML_MIDTEXTUREBLOCK  512	/* Collide with midtexture (fences, etc.) */

/* to aid move clipping */
#define ML_ST_HORIZONTAL 	4096
#define ML_ST_VERTICAL	 	8192
#define ML_ST_POSITIVE	 	16384
#define ML_ST_NEGATIVE	 	32768

typedef	struct
{
	int16_t		floorheight, ceilingheight;
	uint8_t		floorpic, ceilingpic;
	uint8_t     lightlevel;
	uint8_t     special, tag;
} mapsector_t;

typedef struct
{
//	short		numsegs;
	short		firstseg;			/* segs are stored sequentially */
} mapsubsector_t;

typedef struct
{
	short		v1, v2;
	short		angle;			/* ???: make this a sidedef? */
	short		linedef, side;
	short		offset;
} mapseg_t;

enum {BOXTOP,BOXBOTTOM,BOXLEFT,BOXRIGHT};	/* bbox coordinates */

#define	NF_SUBSECTOR	0x8000
typedef struct
{
	short		x,y,dx,dy;			/* partition line */
	short		bbox[2][4];			/* bounding box for each child */
	unsigned short	children[2];		/* if NF_SUBSECTOR its a subsector */
} mapnode_t;

typedef struct
{
	short		x,y;
	short		angle;
	short		type;
	short		options;
} mapthing_t;

/*
===============================================================================

						texture definition

===============================================================================
*/

typedef struct
{
	short	originx;
	short	originy;
	short	patch;
	short	stepdir;
	short	colormap;
} mappatch_t;

typedef struct
{
	char		name[8];
	boolean		masked;	
	short		width;
	short		height;
	void		**columndirectory;	/* OBSOLETE */
	short		patchcount;
	mappatch_t	patches[1];
} maptexture_t;


/*
===============================================================================

							graphics

===============================================================================
*/

/* shorts are stored BIG ENDIAN */


/* column_t are runs of non masked source pixels */
typedef struct
{
	byte			topdelta;	/* 0xff is the last post in a column */
	byte			length;
	unsigned short	dataofs;	/* from data start in patch_t */
} column_t;

/* a patch holds one or more columns */
/* patches are used for sprites and all masked pictures */
typedef struct 
{ 
	short		width;				/* bounding box size  */
	short		height; 
	short		leftoffset;			/* pixels to the left of origin  */
	short		topoffset;			/* pixels below the origin  */
	unsigned short	columnofs[8];	/* only [width] used */
									/* the [0] is &columnofs[width]  */
} patch_t; 


/*
===============================================================================

							status

===============================================================================
*/




#endif			/* __DOOMDATA__ */

