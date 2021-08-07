/* P_local.h */

#ifndef __P_LOCAL__
#define __P_LOCAL__

#ifndef __R_LOCAL__
#include "r_local.h"
#endif

#define	FLOATSPEED		(FRACUNIT*8)

#define	GRAVITY			(FRACUNIT*4)
#define	MAXMOVE			(16*FRACUNIT)


#define	MAXHEALTH			100
#define	VIEWHEIGHT			(41*FRACUNIT)

/* mapblocks are used to check movement against lines and things */
#define MAPBLOCKUNITS	128
#define	MAPBLOCKSIZE	(MAPBLOCKUNITS*FRACUNIT)
#define	MAPBLOCKSHIFT	(FRACBITS+7)
#define	MAPBMASK		(MAPBLOCKSIZE-1)
#define	MAPBTOFRAC		(MAPBLOCKSHIFT-FRACBITS)


/* player radius for movement checking */
#define	PLAYERRADIUS	16*FRACUNIT

/* MAXRADIUS is for precalculated sector block boxes */
/* the spider demon is larger, but we don't have any moving sectors */
/* nearby */
#define	MAXRADIUS		32*FRACUNIT


#define	USERANGE		(70*FRACUNIT)
#define	MELEERANGE		(70*FRACUNIT)
#define	MISSILERANGE	(32*64*FRACUNIT)


typedef enum
{
	DI_EAST,
	DI_NORTHEAST,
	DI_NORTH,
	DI_NORTHWEST,
	DI_WEST,
	DI_SOUTHWEST,
	DI_SOUTH,
	DI_SOUTHEAST,
	DI_NODIR,
	NUMDIRS
} dirtype_t;

#define	BASETHRESHOLD	100		/* follow a player exlusively for 3 seconds */



/*
===============================================================================

							P_TICK

===============================================================================
*/

extern	thinker_t	thinkercap;	/* both the head and tail of the thinker list */


void P_InitThinkers (void);
void P_AddThinker (thinker_t *thinker);
void P_RemoveThinker (thinker_t *thinker);

/*
===============================================================================

							P_PSPR

===============================================================================
*/

void P_SetupPsprites (player_t *curplayer);
void P_MovePsprites (player_t *curplayer);

void P_DropWeapon (player_t *player);

/*
===============================================================================

							P_USER

===============================================================================
*/

boolean P_CanSelecteWeapon(player_t* player, int weaponnum);
boolean P_CanFireWeapon(player_t* player, int weaponnum);
void	P_PlayerThink (player_t *player);
void	P_RestoreResp(player_t* p);
void	P_UpdateResp(player_t* p);

/*
===============================================================================

							P_MOBJ

===============================================================================
*/

extern	degenmobj_t	mobjhead;
extern	degenmobj_t	freemobjhead;
extern	degenmobj_t	limbomobjhead;

extern	int			activethinkers;	/* debug count */
extern	int			activemobjs;	/* debug count */

#define ONFLOORZ	D_MININT
#define	ONCEILINGZ	D_MAXINT

#define		ITEMQUESIZE	32
extern	mapthing_t	*itemrespawnque;
extern	int			*itemrespawntime;

mobj_t *P_SpawnMobj (fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);

void 	P_RemoveMobj (mobj_t *th);
void	P_FreeMobj(mobj_t* mobj);
boolean	P_SetMobjState (mobj_t *mobj, statenum_t state) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void 	P_MobjThinker (mobj_t *mobj);
void 	P_PreSpawnMobjs(int count);

void	P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z);
void 	P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage);
void	P_SpawnMissile (mobj_t *source, mobj_t *dest, mobjtype_t type);
void	P_SpawnPlayerMissile (mobj_t *source, mobjtype_t type);

void	P_RunMobjBase2 (void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void	P_RunMobjLate(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

void L_SkullBash (mobj_t *mo);
void L_MissileHit (mobj_t *mo);
void L_CrossSpecial (mobj_t *mo);

void P_ExplodeMissile (mobj_t *mo);

boolean P_MapThingSpawnsMobj(mapthing_t* mthing) ATTR_OPTIMIZE_SIZE;
void	P_SpawnMapThing(mapthing_t* mthing) ATTR_OPTIMIZE_SIZE;

/*
===============================================================================

							P_ENEMY

===============================================================================
*/

void A_MissileExplode (mobj_t *mo);
void A_SkullBash (mobj_t *mo);

/*
===============================================================================

							P_MAPUTL

===============================================================================
*/

typedef struct
{
	fixed_t	x,y, dx, dy;
} divline_t;


fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);
int 	P_PointOnLineSide (fixed_t x, fixed_t y, line_t *line);
int 	P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t *line);
void 	P_MakeDivline (line_t *li, divline_t *dl);
fixed_t P_InterceptVector (divline_t *v2, divline_t *v1);
int 	P_BoxOnLineSide (fixed_t *tmbox, line_t *ld);

extern	fixed_t opentop, openbottom, openrange;
extern	fixed_t	lowfloor;
void 	P_LineOpening (line_t *linedef);

fixed_t* P_LineBBox(line_t* ld);

boolean P_BlockLinesIterator (int x, int y, boolean(*func)(line_t*) );
boolean P_BlockThingsIterator (int x, int y, boolean(*func)(mobj_t*) );

extern	divline_t 	trace;

void 	P_UnsetThingPosition (mobj_t *thing);
void	P_SetThingPosition (mobj_t *thing);

void	P_PlayerLand (mobj_t *mo);

/*
===============================================================================

							P_MAP

===============================================================================
*/

extern	boolean		floatok;				/* if true, move would be ok if */
extern	fixed_t		tmfloorz, tmceilingz;	/* within tmfloorz - tmceilingz */

extern	line_t	*specialline;
extern	mobj_t	*movething;


boolean P_CheckPosition (mobj_t *thing, fixed_t x, fixed_t y);
boolean P_TryMove (mobj_t *thing, fixed_t x, fixed_t y);
boolean P_CheckSight (mobj_t *t1, mobj_t *t2);
void 	P_UseLines (player_t *player);

boolean P_ChangeSector (sector_t *sector, boolean crunch);

extern	mobj_t		*linetarget;			/* who got hit (or NULL) */
fixed_t P_AimLineAttack (mobj_t *t1, angle_t angle, fixed_t distance) ATTR_DATA_CACHE_ALIGN;

void P_LineAttack (mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope, int damage) ATTR_DATA_CACHE_ALIGN;

void P_RadiusAttack (mobj_t *spot, mobj_t *source, int damage) ATTR_DATA_CACHE_ALIGN;

/*
===============================================================================

							P_SETUP

===============================================================================
*/

extern	byte		*rejectmatrix;			/* for fast sight rejection */
extern	short		*blockmaplump;		/* offsets in blockmap are from here */
extern	short		*blockmap;
extern	int			bmapwidth, bmapheight;	/* in mapblocks */
extern	fixed_t		bmaporgx, bmaporgy;		/* origin of block map */
extern	mobj_t		**blocklinks;			/* for thing chains */

/*
===============================================================================

							P_INTER

===============================================================================
*/

extern	int		maxammo[NUMAMMO];
extern	int		clipammo[NUMAMMO];

void P_TouchSpecialThing (mobj_t *special, mobj_t *toucher);

void P_DamageMobj (mobj_t *target, mobj_t *inflictor, mobj_t *source, int damage);

#include "p_spec.h"

extern	int			iquehead, iquetail;

extern	int playernum;

void P_RespawnSpecials (void);

#endif	/* __P_LOCAL__ */


