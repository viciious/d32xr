/* P_local.h */

#ifndef __P_LOCAL__
#define __P_LOCAL__

#ifndef __R_LOCAL__
#include "r_local.h"
#endif

#define	FLOATSPEED		(FRACUNIT*8)

#define	GRAVITY			(FRACUNIT)
#define	MAXMOVE			(30*FRACUNIT)


#define	MAXHEALTH			100
#define	VIEWHEIGHT			(41*FRACUNIT)

/* mapblocks are used to check movement against lines and things */
#define MAPBLOCKUNITS	256
#define	MAPBLOCKSIZE	(MAPBLOCKUNITS*FRACUNIT)
#define	MAPBLOCKSHIFT	(FRACBITS+8)
#define	MAPBMASK		(MAPBLOCKSIZE-1)
#define	MAPBTOFRAC		(MAPBLOCKSHIFT-FRACBITS)

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

							P_USER

===============================================================================
*/

fixed_t P_GetPlayerSpinHeight();
void P_ThrustValues(angle_t angle, fixed_t move, fixed_t *outX, fixed_t *outY);
void P_InstaThrust(mobj_t *mo, angle_t angle, fixed_t move);
fixed_t P_ReturnThrustX(angle_t angle, fixed_t move);
fixed_t P_ReturnThrustY(angle_t angle, fixed_t move);
void P_RestoreMusic(player_t *player);
boolean P_IsReeling(player_t *player);
void    P_AddPlayerScore(player_t *player, int amount);
void    P_ResetPlayer(player_t *player);
void    P_PlayerRingBurst(player_t *player, int damage);
void    P_GivePlayerRings(player_t *player, int num_rings);
void	P_PlayerThink (player_t *player);
void	P_RestoreResp(player_t* p);
void	P_UpdateResp(player_t* p);
void	R_ResetResp(player_t* p);
void    P_DoPlayerExit(player_t *player);

/*
===============================================================================

							P_MOBJ

===============================================================================
*/

extern	degenmobj_t	mobjhead;
extern	degenmobj_t	freemobjhead, freestaticmobjhead;
extern	degenmobj_t	limbomobjhead;
extern scenerymobj_t *scenerymobjlist;
extern ringmobj_t *ringmobjlist;
extern VINT numscenerymobjs;
extern VINT numringmobjs;
extern VINT numstaticmobjs;
extern VINT numregmobjs;

extern	int			activethinkers;	/* debug count */
extern	int			activemobjs;	/* debug count */
extern  int         thingmem; // bytes in use for things (at spawn)

extern VINT ringmobjstates[NUMMOBJTYPES];
extern VINT ringmobjtics[NUMMOBJTYPES];

#define ONFLOORZ	D_MININT
#define	ONCEILINGZ	D_MAXINT

fixed_t FloorZAtPos(sector_t *sec, fixed_t z, fixed_t height);
fixed_t CeilingZAtPos(sector_t *sec, fixed_t z, fixed_t height);

mobj_t *P_FindFirstMobjOfType(uint16_t type);
void P_BlackOw(player_t *player);
void P_Attract(mobj_t *source, mobj_t *dest);
fixed_t GetWatertopSec(const sector_t *sector);
fixed_t GetWatertopMo(const mobj_t *mo);

boolean Mobj_HasFlags2(mobj_t *mo, VINT value) ATTR_DATA_CACHE_ALIGN;
void P_SetObjectMomZ(mobj_t *mo, fixed_t value, boolean relative);
mobj_t *P_SpawnMobjNoSector (fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);
mobj_t *P_SpawnMobj (fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);

void 	P_RemoveMobj (mobj_t *th);
void	P_FreeMobj(mobj_t* mobj);
boolean	P_SetMobjState (mobj_t *mobj, statenum_t state) ATTR_DATA_CACHE_ALIGN;
void 	P_MobjThinker (mobj_t *mobj);
void 	P_PreSpawnMobjs(int count, int staticcount, int ringcount, int scenerycount);

void	P_SpawnMissile (mobj_t *source, mobj_t *dest, mobjtype_t type);
void	P_SpawnPlayerMissile (mobj_t *source, mobjtype_t type);

void	P_RunMobjBase2 (void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
void	P_RunMobjLate(void) ATTR_DATA_CACHE_ALIGN;

void P_MobjCheckWater(mobj_t *mo);
void L_SkullBash (mobj_t *mo);
void L_MissileHit (mobj_t *mo);
void L_CrossSpecial (mobj_t *mo);

void P_ExplodeMissile (mobj_t *mo);

int 	P_MapThingSpawnsMobj(mapthing_t* mthing); /* 0 -- skip, 1 -- real thing, 2 -- static */
void	P_SpawnMapThing(mapthing_t* mthing, int thingid);

fixed_t Mobj_GetHeight(mobj_t *mo);
fixed_t Mobj_GetHalfHeight(mobj_t *mo);

/*
===============================================================================

							P_ENEMY

===============================================================================
*/

void A_MissileExplode (mobj_t *mo);
void A_SkullBash (mobj_t *mo);
boolean P_LookForPlayers (mobj_t *actor, fixed_t distLimit, boolean allaround, boolean nothreshold) ATTR_DATA_CACHE_ALIGN;

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
int     P_DivlineSide(fixed_t x, fixed_t y, divline_t *node);
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

uint8_t P_GetLineTag(line_t *line);
uint8_t P_GetLineSpecial(line_t *line);

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

__attribute((noinline))
fixed_t P_AimLineAttack (lineattack_t *la, mobj_t *t1, angle_t angle, fixed_t distance);

void P_RadiusAttack (mobj_t *spot, mobj_t *source, int damage) ATTR_DATA_CACHE_ALIGN;
void P_XYMovement(mobj_t* mo) ATTR_DATA_CACHE_ALIGN;
void P_ZMovement(mobj_t* mo) ATTR_DATA_CACHE_ALIGN;

/*
===============================================================================
							P_SIGHT
===============================================================================
*/
void P_CheckSights (void);

/*
===============================================================================

							P_SETUP

===============================================================================
*/

extern	byte		*rejectmatrix;			/* for fast sight rejection */
extern	short		*blockmaplump;		/* offsets in blockmap are from here */
extern	VINT		bmapwidth, bmapheight;	/* in mapblocks */
extern	fixed_t		bmaporgx, bmaporgy;		/* origin of block map */
extern	mobj_t		**blocklinks;			/* for thing chains */

extern	uint16_t			numthings;

extern	VINT		*validcount;		/* (0 - increment every time a check is made, [1..numlines]) x 2 */

/*
===============================================================================

							P_INTER

===============================================================================
*/

void P_SetStarPosts(uint8_t starpostnum);
void P_TouchSpecialThing (mobj_t *special, mobj_t *toucher) __attribute__((noinline));

void P_DamageMobj (mobj_t *target, mobj_t *inflictor, mobj_t *source, int damage);

#include "p_spec.h"

extern	int playernum;

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

	line_t		*blockline;
} ptrymove_t;

void P_PlayerCheckForStillPickups(mobj_t *mobj);
boolean P_CheckPosition (ptrymove_t *tm, mobj_t *thing, fixed_t x, fixed_t y);
boolean P_TryMove (ptrymove_t *tm, mobj_t *thing, fixed_t x, fixed_t y);

typedef struct
{
	// input
	mobj_t *slidething;

	// output
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

	vertex_t p1, p2; // p1, p2 are line endpoints
	fixed_t p3x, p3y, p4x, p4y; // p3, p4 are move endpoints
} pslidework_t;

boolean PM_BoxCrossLine(line_t *ld, pmovework_t *mw) ATTR_DATA_CACHE_ALIGN;
fixed_t P_CompletableFrac(pslidework_t *sw, fixed_t dx, fixed_t dy);

#endif	/* __P_LOCAL__ */
