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
	boolean	istexture;
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
	boolean	istexture;		/* if false, it's a flat */
	char	endname[9];
	char	startname[9];
	int		speed;
} animdef_t;

#define	MAXANIMS		32

extern	anim_t	anims[MAXANIMS], *lastanim;


/* */
/*	Animating line specials */
/* */
#define	MAXLINEANIMS		64
extern	int		numlinespecials;
extern	line_t	*linespeciallist[MAXLINEANIMS];


/*	Define values for map objects */
#define	MO_TELEPORTMAN		14


/* at game start */
void	P_InitPicAnims (void) ATTR_OPTIMIZE_SIZE;

/* at map load */
void	P_SpawnSpecials (void) ATTR_OPTIMIZE_SIZE;

/* every tic */
void 	P_UpdateSpecials (void) ATTR_OPTIMIZE_SIZE;

/* when needed */
boolean	P_UseSpecialLine ( mobj_t *thing, line_t *line);
void	P_ShootSpecialLine ( mobj_t *thing, line_t *line);
void P_CrossSpecialLine (line_t *line,mobj_t *thing);

void 	P_PlayerInSpecialSector (player_t *player);

int		twoSided(int sector,int line);
sector_t *getSector(int currentSector,int line,int side);
side_t	*getSide(int currentSector,int line, int side);
fixed_t	P_FindLowestFloorSurrounding(sector_t *sec);
fixed_t	P_FindHighestFloorSurrounding(sector_t *sec);
fixed_t	P_FindNextHighestFloor(sector_t *sec,int currentheight);
fixed_t	P_FindLowestCeilingSurrounding(sector_t *sec);
fixed_t	P_FindHighestCeilingSurrounding(sector_t *sec);
int		P_FindSectorFromLineTag(line_t	*line,int start);
int		P_FindMinSurroundingLight(sector_t *sector,int max);
sector_t *getNextSector(line_t *line,sector_t *sec);

/* */
/*	SPECIAL */
/* */
int EV_DoDonut(line_t *line) ATTR_OPTIMIZE_SIZE;

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

#define GLOWSPEED		16
#define	STROBEBRIGHT	3
#define	FASTDARK		8
#define	SLOWDARK		15

void	T_LightFlash (lightflash_t *flash);
void	P_SpawnLightFlash (sector_t *sector);
void	T_StrobeFlash (strobe_t *flash);
void 	P_SpawnStrobeFlash (sector_t *sector, int fastOrSlow, int inSync);
void	EV_StartLightStrobing(line_t *line);
void	EV_TurnTagLightsOff(line_t	*line);
void	EV_LightTurnOn(line_t *line, int bright);
void	T_Glow(glow_t *g);
void	P_SpawnGlowingLight(sector_t *sector);

/*
===============================================================================

							P_SWITCH

===============================================================================
*/
typedef struct
{
	char	name1[9];
	char	name2[9];
} switchlist_t;

typedef enum
{
	top,
	middle,
	bottom
} bwhere_e;

typedef struct
{
	line_t		*line;
	mobj_t* soundorg;
	VINT		btexture;
	VINT		btimer;
	bwhere_e	where;
} button_t;

#define	MAXSWITCHES	50		/* max # of wall switches in a level */
#define	MAXBUTTONS	16		/* 4 players, 4 buttons each at once, max. */
#define BUTTONTIME	15		/* 1 second */

extern	VINT		switchlist[MAXSWITCHES * 2];
extern	button_t buttonlist[MAXBUTTONS];

void	P_ChangeSwitchTexture(line_t *line,int useAgain);
void 	P_InitSwitchList(void) ATTR_OPTIMIZE_SIZE;

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
	raiseToNearestAndChange
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
#define	MAXPLATS	30

extern	plat_t	*activeplats[MAXPLATS];

void	T_PlatRaise(plat_t	*plat);
int		EV_DoPlat(line_t *line,plattype_e type,int amount) ATTR_OPTIMIZE_SIZE;
void	P_AddActivePlat(plat_t *plat);
void	P_RemoveActivePlat(plat_t *plat);
void	EV_StopPlat(line_t *line);
void	P_ActivateInStasis(int tag);

/*
===============================================================================

							P_DOORS

===============================================================================
*/
typedef enum
{
	normal,
	close30ThenOpen,
	close,
	open,
	raiseIn5Mins
} vldoor_e;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	fixed_t		topheight;
	fixed_t		speed;
	VINT		direction;		/* 1 = up, 0 = waiting at top, -1 = down */
	VINT		topwait;		/* tics to wait at the top */
								/* (keep in case a door going down is reset) */
	VINT		type;
	VINT		topcountdown;	/* when it reaches 0, start going down */
} vldoor_t;
	
#define	VDOORSPEED	FRACUNIT*3*THINKERS_TICS
#define	VDOORWAIT		140/THINKERS_TICS

void	EV_VerticalDoor (line_t *line, mobj_t *thing) ATTR_OPTIMIZE_SIZE;
int		EV_DoDoor (line_t *line, vldoor_e  type) ATTR_OPTIMIZE_SIZE;
void	T_VerticalDoor (vldoor_t *door);
void	P_SpawnDoorCloseIn30 (sector_t *sec);
void	P_SpawnDoorRaiseIn5Mins (sector_t *sec, int secnum);

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
	fastCrushAndRaise
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
#define MAXCEILINGS		30

extern	ceiling_t	*activeceilings[MAXCEILINGS];

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
	donutRaise
} floor_e;

typedef struct
{
	thinker_t	thinker;
	VINT		type;
	VINT		crush;
	sector_t	*sector;
	int			newspecial;
	VINT		direction;
	VINT		texture;
	fixed_t		floordestheight;
	fixed_t		speed;
} floormove_t;

#define	FLOORSPEED	((FRACUNIT+(FRACUNIT>>1))*THINKERS_TICS)

typedef enum
{
	ok,
	crushed,
	pastdest
} result_e;

result_e	T_MovePlane(sector_t *sector,fixed_t speed,
			fixed_t dest,boolean crush,int floorOrCeiling,int direction);

int		EV_BuildStairs(line_t *line) ATTR_OPTIMIZE_SIZE;
int		EV_DoFloor(line_t *line,floor_e floortype) ATTR_OPTIMIZE_SIZE;
void	T_MoveFloor(floormove_t *floor);

/*
===============================================================================

							P_TELEPT

===============================================================================
*/
int		EV_Teleport( line_t *line,mobj_t *thing );

