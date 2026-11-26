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

// Returns (h << FRACBITS) / frac, where frac = (num << FRACBITS) / den is the
// fractional intercept point along the trace. The two shifts cancel, so this is
// h * den / num: a single 64/32 divide instead of computing frac first.
static ATTR_FORCE_INLINE
fixed_t P_SlopeDiv(fixed_t h, int32_t den, int32_t num)
{
#ifdef MARS
   int32_t lo, divunit;
   fixed_t slope;
   __asm volatile (
      "dmuls.l %3, %4\n\t"
      "mov #-128, %2\n\t"
      "add %2, %2 /* %2 is now 0xFFFFFF00 */ \n\t"
      "sts mach, %0\n\t"
      "sts macl, %1\n\t"
      "mov.l %5, @(0,%2) /* set 32-bit divisor */ \n\t"
      "mov.l %0, @(16,%2) /* set high bits of the 64-bit dividend */ \n\t"
      "mov.l %1, @(20,%2) /* set low bits of the 64-bit dividend, start divide */ \n\t"
      "mov.l @(20,%2), %0 /* get 32-bit quotient */ \n\t"
      : "=&r" (slope), "=&r" (lo), "=&r" (divunit)
      : "r" (h), "r" (den), "r" (num)
      : "mach", "macl"
   );
   return slope;
#else
   return FixedDiv(h, FixedDiv(num, den));
#endif
}


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
	DI_NODIRWAIT,
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
void *P_SpawnThinker_ (int size, int thinker_offset);
#define P_SpawnThinker(t) P_SpawnThinker_(sizeof(t),offsetof(__typeof__(t),thinker))
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

boolean P_CanSelectWeapon(player_t* player, int weaponnum);
boolean P_CanFireWeapon(player_t* player, int weaponnum);
void	P_PlayerThink (player_t *player);
void	P_RestoreResp(player_t* p);
void	P_UpdateResp(player_t* p);
void	P_ResetResp(player_t* p);

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
mobj_t *P_SpawnMobj2 (fixed_t x, fixed_t y, fixed_t z, mobjtype_t type, subsector_t *ss);

void 	P_RemoveMobj (mobj_t *th);
void	P_FreeMobj(mobj_t* mobj);
boolean	P_SetMobjState (mobj_t *mobj, statenum_t state) ATTR_DATA_CACHE_ALIGN;
void 	P_MobjThinker (mobj_t *mobj);
void 	P_PreSpawnMobjs(int count, int staticcount);

void 	P_ApplyFriction(mobj_t *mo);

void	P_SpawnPuff2 (fixed_t x, fixed_t y, fixed_t z, fixed_t attackrange, subsector_t *ss);
void	P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z, fixed_t attackrange);

void 	P_SpawnBlood2 (fixed_t x, fixed_t y, fixed_t z, int damage, subsector_t *ss);
void 	P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage);

mobj_t	*P_SpawnMissile (mobj_t *source, mobj_t *dest, mobjtype_t type);
void	P_SpawnPlayerMissile (mobj_t *source, mobjtype_t type);

void	P_RunMobjBase2 (void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
void	P_RunMobjLate(void) ATTR_DATA_CACHE_ALIGN;

void L_SkullBash (mobj_t *mo, mobj_t *target);
void L_MissileHit (mobj_t *mo, mobj_t *target);

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

// 
// keep track of special lines as they are hit,
// but don't process them until the move is proven valid
#define MAXSPECIALCROSS		8

typedef struct
{
	fixed_t	x,y, dx, dy;
} divline_t;

typedef boolean(*blocklinesiter_t)(line_t*, void*);
typedef boolean(*blockthingsiter_t)(mobj_t*, void*);

typedef struct
{
	// input
	mobj_t  *tmthing;
	fixed_t tmx, tmy;
	boolean checkposonly;

	// output
	fixed_t tmbbox[4];

	int    	numspechit;
 	line_t	*spechit[MAXSPECIALCROSS];
	
	fixed_t tmfloorz;   // current floor z for PM_TryMove
	fixed_t tmceilingz; // current ceiling z for PM_TryMove
	fixed_t tmdropoffz; // lowest point contacted

	boolean	floatok;	/* if true, move would be ok if */
						/* within tmfloorz - tmceilingz */

	line_t  *ceilingline;
	subsector_t *newsubsec;

	mobj_t  *hitthing;
} pmovework_t;

typedef struct {
	sector_t	*sector;
	fixed_t		secbbox[4];
} mover_t;

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);
int 	P_PointOnLineSide (fixed_t x, fixed_t y, line_t *line);
int 	P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t *line);
boolean P_BoxCrossLine (line_t *ld, fixed_t testbbox[4]);

// Side values of two vertices against a trace with a precomputed setup:
// a = sc[0]*v->x + sc[1]*v->y + k, with sc = {tdy, -tdx, tdy, -tdx} and k
// chosen by the caller, so that the sign bit marks the back side. On MARS
// mac.w reads the vertex coordinates straight from memory; *v1 and *v2 are
// stepped back while the multiplier finishes, so they're unchanged on return.
static ATTR_FORCE_INLINE
void P_VertexSides(const int16_t *sc, fixed_t k,
   mapvertex_t **v1, mapvertex_t **v2, fixed_t *a1, fixed_t *a2)
{
#ifdef MARS
   const int16_t *c = sc;
   __asm (
      "lds %5, macl\n\t"
      "mac.w @%2+, @%3+\n\t"
      "mac.w @%2+, @%3+\n\t"
      "add #-4, %3\n\t"
      "sts macl, %0\n\t"
      "lds %5, macl\n\t"
      "mac.w @%2+, @%4+\n\t"
      "mac.w @%2+, @%4+\n\t"
      "add #-4, %4\n\t"
      "sts macl, %1\n\t"
      : "=&r" (*a1), "=r" (*a2), "+r" (c), "+r" (*v1), "+r" (*v2)
      : "r" (k), "m" (*(const int16_t (*)[4])sc), "m" (**v1), "m" (**v2)
      : "mach", "macl"
   );
#else
   *a1 = sc[0] * (*v1)->x + sc[1] * (*v1)->y + k;
   *a2 = sc[0] * (*v2)->x + sc[1] * (*v2)->y + k;
#endif
}

fixed_t	P_LineOpening (line_t *linedef);

static ATTR_FORCE_INLINE
void P_LineBBox(line_t* ld, fixed_t *bbox)
{
	mapvertex_t* v1 = &vertexes[ld->v1], * v2 = &vertexes[ld->v2];

	if (v1->x < v2->x)
	{
		bbox[BOXLEFT] = v1->x;
		bbox[BOXRIGHT] = v2->x;
	}
	else
	{
		bbox[BOXLEFT] = v2->x;
		bbox[BOXRIGHT] = v1->x;
	}
	if (v1->y < v2->y)
	{
		bbox[BOXBOTTOM] = v1->y;
		bbox[BOXTOP] = v2->y;
	}
	else
	{
		bbox[BOXBOTTOM] = v2->y;
		bbox[BOXTOP] = v1->y;
	}

	bbox[BOXTOP] <<= FRACBITS;
	bbox[BOXBOTTOM] <<= FRACBITS;
	bbox[BOXLEFT] <<= FRACBITS;
	bbox[BOXRIGHT] <<= FRACBITS;
}

// the userp must conform to pmovework_t interface
boolean P_BlockLinesIterator (int x, int y, blocklinesiter_t, void *userp );
// the userp must conform to pmovework_t interface
boolean P_BlockThingsIterator (int x, int y, blockthingsiter_t, void *userp );

void 	P_UnsetThingPosition (mobj_t *thing);
void	P_SetThingPosition (mobj_t *thing);
void	P_SetThingPosition2 (mobj_t *thing, subsector_t *ss);

void 	P_SectorBBox(sector_t* sector, fixed_t *bbox);
void 	P_SectorOrg(sector_t* sec, fixed_t *org);
void 	P_StartSectorSound(sector_t* sec, int sound_id);

int 	P_GetLineTag (line_t *line);
void 	P_SetLineTag (int ld, int tag);

int 	P_GetSectorTag (sector_t *sec);
void 	P_SetSectorTag (int ld, int tag);
int 	P_FindNextSectorByTagNum(int tag, int *start);

/*
===============================================================================

							P_MAP

===============================================================================
*/

boolean P_CheckSight (mobj_t *t1, mobj_t *t2);
void 	P_UseLines (player_t *player);
boolean P_ChangeMover (mover_t *sector, boolean crunch);
void 	P_MoverSound(mover_t* mover, int sound_id);

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

   int 		firstsplit;             // first BSP node that generated a split
} lineattack_t;

fixed_t P_AimLineAttack (lineattack_t *la, mobj_t *t1, angle_t angle, fixed_t distance) ATTR_DATA_CACHE_ALIGN;

void P_LineAttack (lineattack_t *la, mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope, int damage) ATTR_DATA_CACHE_ALIGN;

void P_RadiusAttack (mobj_t *spot, mobj_t *source, int damage) ATTR_DATA_CACHE_ALIGN;


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
extern	SPTR		*blocklinks;			/* for thing chains */
extern	const boolean		blocklinksadjust;		/* use larger blocks if 1 */

extern	VINT		numthings;
extern	spawnthing_t* spawnthings;

extern	VINT		*validcount;		/* 0 - increment every time a check is made, [1..numlines] */

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

void P_RespawnSpecials (void);


/*
===============================================================================

							P_MOVE

===============================================================================
*/

boolean P_CheckPosition (pmovework_t *tm, mobj_t *thing, fixed_t x, fixed_t y);
boolean P_TryMove (pmovework_t *tm, mobj_t *thing, fixed_t x, fixed_t y);
// same as above, with ss = the subsector containing (x, y) if already known, or NULL
boolean P_CheckPosition2 (pmovework_t *tm, mobj_t *thing, fixed_t x, fixed_t y, subsector_t *ss);
boolean P_TryMove2 (pmovework_t *tm, mobj_t *thing, fixed_t x, fixed_t y, subsector_t *ss);
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

#endif	/* __P_LOCAL__ */


