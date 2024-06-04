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
void	R_ResetResp(player_t* p);

/*
===============================================================================

							P_MOBJ

===============================================================================
*/

extern	degenmobj_t	mobjhead;
extern	degenmobj_t	freemobjhead, freestaticmobjhead;
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
boolean	P_SetMobjState (mobj_t *mobj, statenum_t state) ATTR_DATA_CACHE_ALIGN;
void 	P_MobjThinker (mobj_t *mobj);
void 	P_PreSpawnMobjs(int count, int staticcount);

void	P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z, fixed_t attackrange);
void 	P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage);
void	P_SpawnMissile (mobj_t *source, mobj_t *dest, mobjtype_t type);
void	P_SpawnPlayerMissile (mobj_t *source, mobjtype_t type);

void	P_RunMobjBase2 (void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
void	P_RunMobjLate(void) ATTR_DATA_CACHE_ALIGN;

void L_SkullBash (mobj_t *mo);
void L_MissileHit (mobj_t *mo);
void L_CrossSpecial (mobj_t *mo);

void P_ExplodeMissile (mobj_t *mo);

int 	P_MapThingSpawnsMobj(mapthing_t* mthing); /* 0 -- skip, 1 -- real thing, 2 -- static */
void	P_SpawnMapThing(mapthing_t* mthing, int thingid);

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
int 	P_BoxOnLineSide (fixed_t *tmbox, line_t *ld);

fixed_t	P_LineOpening (line_t *linedef);

void 	P_LineBBox(line_t* ld, fixed_t*bbox);

typedef boolean(*blocklinesiter_t)(line_t*, void*);
typedef boolean(*blockthingsiter_t)(mobj_t*, void*);

boolean P_BlockLinesIterator (int x, int y, blocklinesiter_t, void *userp );
boolean P_BlockThingsIterator (int x, int y, blockthingsiter_t, void *userp );

void 	P_UnsetThingPosition (mobj_t *thing);
void	P_SetThingPosition (mobj_t *thing);
void	P_SetThingPosition2 (mobj_t *thing, subsector_t *ss);

void	P_PlayerLand (mobj_t *mo);

void 	P_SectorOrg(mobj_t* sec, fixed_t *org);

/*
===============================================================================

							P_MAP

===============================================================================
*/

boolean P_CheckSight (mobj_t *t1, mobj_t *t2);
void 	P_UseLines (player_t *player);

boolean P_ChangeSector (sector_t *sector, boolean crunch);

typedef struct
{
   // input
   mobj_t  *shooter;
   angle_t  attackangle;
   fixed_t  attackrange;
   fixed_t  aimtopslope;
   fixed_t  aimbottomslope;

   // output
   line_t  *shootline;
   mobj_t  *shootmobj;				/* who got hit (or NULL) */
   fixed_t  shootslope;             // between aimtop and aimbottom
   fixed_t  shootx, shooty, shootz; // location for puff/blood
} lineattack_t;

fixed_t P_AimLineAttack (lineattack_t *la, mobj_t *t1, angle_t angle, fixed_t distance) ATTR_DATA_CACHE_ALIGN;

void P_LineAttack (lineattack_t *la, mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope, int damage) ATTR_DATA_CACHE_ALIGN;

void P_RadiusAttack (mobj_t *spot, mobj_t *source, int damage) ATTR_DATA_CACHE_ALIGN;

/*
===============================================================================

							P_SETUP

===============================================================================
*/

extern	byte		*rejectmatrix;			/* for fast sight rejection */
extern	short		*blockmaplump;		/* offsets in blockmap are from here */
extern	int			bmapwidth, bmapheight;	/* in mapblocks */
extern	fixed_t		bmaporgx, bmaporgy;		/* origin of block map */
extern	mobj_t		**blocklinks;			/* for thing chains */

extern	int			numthings;
extern	spawnthing_t* spawnthings;

extern	VINT		*validcount;		/* (0 - increment every time a check is made, [1..numlines]) x 2 */

/*
===============================================================================

							P_INTER

===============================================================================
*/

extern	VINT	maxammo[NUMAMMO];
extern	VINT	clipammo[NUMAMMO];

void P_TouchSpecialThing (mobj_t *special, mobj_t *toucher) __attribute__((noinline));

void P_DamageMobj (mobj_t *target, mobj_t *inflictor, mobj_t *source, int damage);

#include "p_spec.h"

extern	int			iquehead, iquetail;

extern	int playernum;

void P_RespawnSpecials (void);


/*
===============================================================================

							P_MOVE

===============================================================================
*/

// 
// keep track of special lines as they are hit,
// but don't process them until the move is proven valid
#define MAXSPECIALCROSS		8

typedef struct
{
	/*================== */
	/* */
	/* in */
	/* */
	/*================== */
	mobj_t		*tmthing;
	fixed_t		tmx, tmy;
	boolean		checkposonly;

	/*================== */
	/* */
	/* out */
	/* */
	/*================== */
	boolean		floatok;				/* if true, move would be ok if */
										/* within tmfloorz - tmceilingz */
	fixed_t		tmfloorz, tmceilingz, tmdropoffz;

	int    		numspechit;
 	line_t		*spechit[MAXSPECIALCROSS];

	line_t		*blockline;
} ptrymove_t;

boolean P_CheckPosition (ptrymove_t *tm, mobj_t *thing, fixed_t x, fixed_t y);
boolean P_TryMove (ptrymove_t *tm, mobj_t *thing, fixed_t x, fixed_t y);
void P_MoveCrossSpecials(mobj_t *tmthing, int numspechit, line_t **spechit, fixed_t oldx, fixed_t oldy);

typedef struct
{
	// input
	mobj_t *slidething;

	// output
	int numspechit;
	line_t *spechit[MAXSPECIALCROSS];
	fixed_t	slidex, slidey;
} pslidemove_t;

void P_SlideMove (pslidemove_t *sm);

typedef struct
{
	// input
	mobj_t *tmthing;
	fixed_t  tmx, tmy;

	fixed_t  tmfloorz;   // current floor z for P_TryMove2
	fixed_t  tmceilingz; // current ceiling z for P_TryMove2
	line_t *blockline;  // possibly a special to activate

	fixed_t tmbbox[4];
	int     tmflags;
	fixed_t tmdropoffz; // lowest point contacted

	int    	numspechit;
	line_t **spechit;

	subsector_t *newsubsec; // destination subsector
} pmovework_t;

typedef struct
{
	mobj_t *slidething;
	fixed_t slidex, slidey;     // the final position
	fixed_t slidedx, slidedy;   // current move for completable frac
	fixed_t blockfrac;          // the fraction of the move that gets completed
	fixed_t blocknvx, blocknvy; // the vector of the line that blocks move
	fixed_t endbox[4];          // final proposed position
	fixed_t nvx, nvy;           // normalized line vector

	vertex_t *p1, *p2; // p1, p2 are line endpoints
	fixed_t p3x, p3y, p4x, p4y; // p3, p4 are move endpoints

	int numspechit;
	line_t **spechit;
} pslidework_t;

boolean PM_BoxCrossLine(line_t *ld, pmovework_t *mw) ATTR_DATA_CACHE_ALIGN;
fixed_t P_CompletableFrac(pslidework_t *sw, fixed_t dx, fixed_t dy);

#endif	/* __P_LOCAL__ */


