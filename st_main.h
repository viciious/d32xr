/* the status bar consists of two 8 bit color objects: */
/* the static background pic and a transparent foreground object that */
/* all the stats are drawn onto when they change */
/* coordinates are status bar relative (0 is first line of sbar) */

#define	FLASHDELAY	4		/* # of tics delay (1/30 sec) */
#define FLASHTIMES	6		/* # of times to flash new frag amount (EVEN!) */

#define AMMOX		52
#define AMMOY		14

#define HEALTHX		104
#define HEALTHY		AMMOY

#define KEYX		124
#define REDKEYY		3
#define BLUKEYY		15
#define YELKEYY		27
#define KEYW		16
#define KEYH		11

#define FACEX		144
#define FACEY		5

#define ARMORX		226
#define ARMORY		HEALTHY

#define MAPX		316
#define MAPY		HEALTHY

#define HISFRAGX	MAPX
#define HISFRAGY	HEALTHY

#define YOURFRAGX	278
#define YOURFRAGY	HEALTHY

typedef enum
{
	f_none,
	f_faceleft,		/* turn face left */
	f_faceright,	/* turn face right */
	f_hurtbad,		/* surprised look when slammed hard */
	f_gotgat,		/* picked up a weapon smile */
	f_mowdown,		/* grimace while continuous firing */
	NUMSPCLFACES
} spclface_e;

#define	NUMFACES	48
typedef enum
{
	sbf_lookfwd,
	sbf_lookrgt,
	sbf_looklft,
	sbf_facelft,
	sbf_facergt,
	sbf_ouch,
	sbf_gotgat,
	sbf_mowdown
} faces_e;
#define	GODFACE		40
#define DEADFACE 	41
#define FIRSTSPLAT	42
#define GIBTIME		4
#define NUMSPLATS	6
#define	NUMMICROS	6		/* amount of micro-sized #'s (weapon armed) */

typedef enum
{
	sb_minus,
	sb_0,
	sb_percent = 11,
	sb_card_b,
	sb_card_y,
	sb_card_r,
	sb_skul_b,
	sb_skul_y,
	sb_skul_r,
	NUMSBOBJ
} sbobj_e;

typedef struct
{
	boolean	active;
	boolean	doDraw;
	int		delay;
	int		times;
	int		x;
	int		y;
	int		w;
	int		h;
} sbflash_t;

typedef struct
{
	int		ammo, health, armor;
	int		godmode;
	int		face;
	boolean	cards[NUMCARDS];
	
	int		yourFrags;
	int		hisFrags;
	int		currentMap;
	boolean	weaponowned[NUMMICROS];
	
	/* Messaging */
	spclface_e	specialFace;	/* Which type of special face to make */
	boolean	gotgibbed;			/* Got gibbed */
	boolean tryopen[NUMCARDS];	/* Tried to open a card or skull door */
} stbar_t;

extern	stbar_t	stbar;
extern void valtostr(char *string,int val);
extern int mystrlen(char *string);
void ST_DrawValue(int x,int y,int value);
void ST_Num (int x, int y, int num);
void valtostr(char *string,int val);
void ST_InitEveryLevel(void);


