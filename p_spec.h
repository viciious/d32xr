/* P_spec.h */

/*
===============================================================================

							P_SPEC

===============================================================================
*/

/* */
/*	Animating textures and planes */
/* */
typedef struct
{
	char	istexture;
	VINT	picnum;
	VINT	basepic;
	VINT	numpics;
	VINT	current;
} anim_t;

/* */
/*	source animation definition */
/* */
typedef struct
{
	char	istexture;		/* if false, it's a flat */
	char	endname[9];
	char	startname[9];
} animdef_t;

#define	MAXANIMS		16

extern	anim_t	*anims/*[MAXANIMS]*/, * lastanim;


/* */
/*	Animating line specials */
/* */
#define	MAXLINEANIMS		64
extern	int		numlinespecials;
extern	line_t	**linespeciallist/*[MAXLINEANIMS]*/;


/*	Define values for map objects */
#define	MO_TELEPORTMAN		14


/* at game start */
void	P_InitPicAnims (void);

/* at map load */
void	P_SpawnSpecials (void);

/* every tic */
void 	P_UpdateSpecials (void);

void 	P_PlayerInSpecialSector (player_t *player);

int		twoSided(int sector,int line);
sector_t *getSector(int currentSector,int line,int side);
side_t	*getSide(int currentSector,int line, int side);
fixed_t	P_FindLowestFloorSurrounding(sector_t *sec);
fixed_t	P_FindHighestFloorSurrounding(sector_t *sec);
fixed_t	P_FindNextHighestFloor(sector_t *sec,int currentheight);
fixed_t	P_FindNextLowestFloor(sector_t *sec,int currentheight);
fixed_t	P_FindLowestCeilingSurrounding(sector_t *sec);
fixed_t	P_FindHighestCeilingSurrounding(sector_t *sec);
VINT P_FindSectorWithTag(VINT tag, int start);
int		P_FindSectorFromLineTag(line_t	*line,int start);
int		P_FindMinSurroundingLight(sector_t *sector,int max);
sector_t *getNextSector(line_t *line,sector_t *sec);

/* */
/*	SPECIAL */
/* */
int EV_DoDonut(line_t *line);

/*
===============================================================================

							P_LIGHTS

===============================================================================
*/
typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	int			count;
	VINT		maxlight;
	VINT		minlight;
	VINT		maxtime;
	VINT		mintime;
} lightflash_t;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	int			count;
	VINT		minlight;
	VINT		maxlight;
	VINT		darktime;
	VINT		brighttime;
} strobe_t;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	VINT		minlight;
	VINT		maxlight;
	int			direction;
} glow_t;

typedef struct
{
    thinker_t	thinker;
    sector_t*	sector;
    VINT		count;
    VINT		maxlight;
    VINT		minlight;
} fireflicker_t;

#define GLOWSPEED		16
#define	STROBEBRIGHT	3
#define	FASTDARK		8
#define	SLOWDARK		15

/*
===============================================================================

							P_SWITCH

===============================================================================
*/

#define	MAXSWITCHES	50		/* max # of wall switches in a level */
#define	MAXBUTTONS	16		/* 4 players, 4 buttons each at once, max. */
#define BUTTONTIME	15		/* 1 second */

/*
===============================================================================

							P_PLATS

===============================================================================
*/
typedef enum
{
	up,
	down,
	waiting,
	in_stasis
} plat_e;

typedef enum
{
	perpetualRaise,
	downWaitUpStay,
	raiseAndChange,
	raiseToNearestAndChange,
	blazeDWUS
} plattype_e;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	fixed_t		speed;
	fixed_t		low;
	fixed_t		high;
	VINT		wait;
	VINT		count;
	char		status;
	char		oldstatus;
	VINT		crush;
	VINT		tag;
	VINT		type;
} plat_t;

#define	PLATWAIT	3*2/THINKERS_TICS			/* seconds */
#define	PLATSPEED	(FRACUNIT*THINKERS_TICS)
#define	MAXPLATS	16

extern	plat_t	**activeplats/*[MAXPLATS]*/;

void	T_PlatRaise(plat_t	*plat);
int		EV_DoPlat(line_t *line,plattype_e type,int amount);
void	P_AddActivePlat(plat_t *plat);
void	P_RemoveActivePlat(plat_t *plat);
void	EV_StopPlat(line_t *line);
void	P_ActivateInStasis(int tag);

/*
===============================================================================

							P_CEILNG

===============================================================================
*/
typedef enum
{
	lowerToFloor,
	raiseToHighest,
	lowerAndCrush,
	crushAndRaise,
	fastCrushAndRaise,
	silentCrushAndRaise
} ceiling_e;

typedef struct
{
	thinker_t	thinker;
	ceiling_e	type;
	sector_t	*sector;
	fixed_t		bottomheight, topheight;
	fixed_t		speed;
	VINT		crush;
	VINT		direction;		/* 1 = up, 0 = waiting, -1 = down */
	VINT		olddirection;
	VINT		tag;			/* ID */
} ceiling_t;

#define	CEILSPEED		FRACUNIT*THINKERS_TICS
#define MAXCEILINGS		16

extern	ceiling_t	**activeceilings/*[MAXCEILINGS]*/;

int		EV_DoCeiling (line_t *line, ceiling_e  type);
void	T_MoveCeiling (ceiling_t *ceiling);
void	P_AddActiveCeiling(ceiling_t *c);
void	P_RemoveActiveCeiling(ceiling_t *c);
int		EV_CeilingCrushStop(line_t	*line);
void	P_ActivateInStasisCeiling(line_t *line);

/*
===============================================================================

							P_FLOOR

===============================================================================
*/
typedef enum
{
	lowerFloor,			/* lower floor to highest surrounding floor */
	lowerFloorToLowest,	/* lower floor to lowest surrounding floor */
	turboLower,			/* lower floor to highest surrounding floor VERY FAST */
	raiseFloor,			/* raise floor to lowest surrounding CEILING */
	raiseFloorToNearest,	/* raise floor to next highest surrounding floor */
	raiseToTexture,		/* raise floor to shortest height texture around it */
	lowerAndChange,		/* lower floor to lowest surrounding floor and change */
						/* floorpic */
	raiseFloor24,
	raiseFloor24AndChange,
	raiseFloorCrush,
	raiseFloorTurbo,	/* raise to next highest floor, turbo-speed */
	donutRaise,
	raiseFloor512,
	floorContinuous,
	eggCapsuleOuter,
	eggCapsuleInner,
	eggCapsuleOuterPop,
	eggCapsuleInnerPop,
} floor_e;

typedef struct
{
	thinker_t	thinker;
	VINT		type;
	VINT		crush;
	sector_t	*sector;
	sector_t    *controlSector;
	int			newspecial;
	VINT		direction;
	VINT		texture;
	VINT        floorwasheight;
	VINT		floordestheight;
	fixed_t     origSpeed;
	fixed_t		speed;
} floormove_t;

typedef enum
{
	build8,	// slowly build by 8
	turbo16	// quickly build by 16
} stair_e;

#define	FLOORSPEED	((FRACUNIT+(FRACUNIT>>1))*THINKERS_TICS)

typedef enum
{
	ok,
	crushed,
	pastdest
} result_e;

result_e	T_MovePlane(sector_t *sector,fixed_t speed,
			fixed_t dest,boolean crush,int floorOrCeiling,int direction);

int		EV_BuildStairs(line_t *line, int type);
int		EV_DoFloor(line_t *line,floor_e floortype);
void	T_MoveFloor(floormove_t *floor);


