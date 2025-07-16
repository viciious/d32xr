/* the status bar consists of two 8 bit color objects: */
/* the static background pic and a transparent foreground object that */
/* all the stats are drawn onto when they change */
/* coordinates are status bar relative (0 is first line of sbar) */

#define	FLASHDELAY	2		/* # of tics delay (1/30 sec) */
#define FLASHTIMES	6		/* # of times to flash new frag amount (EVEN!) */

#define AMMOX		52
#define AMMOY		14

#define HEALTHX		104
#define HEALTHY		AMMOY

#define KEYX		260
#define REDKEYY		4
#define BLUKEYY		14
#define YELKEYY		24
#define KEYW		9
#define KEYH		7

#define FACEX		156
#define FACEY		2
#define FACEW		32
#define FACEH		32

#define ARMORX		240
#define ARMORY		HEALTHY

#define STATSX		308
#define STATSY		4

#define YOURFRAGX	(STATSX + 6)
#define YOURFRAGY	(AMMOY)

#define HISFRAGX	(STATSX - 28)
#define HISFRAGY	(HEALTHY+12)

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
#define GIBTIME		2
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
	char	active;
	char	doDraw;
	short	delay;
	short	times;
	short	x;
	short	w;
	uint8_t	y;
	uint8_t	h;
} sbflash_t;

typedef struct {
	short id;
	short ind;
	short value;
} stbarcmd_t;

typedef enum
{
	stc_drawammo,
	stc_drawhealth,
	stc_drawarmor,
	stc_drawcard,
	stc_drawammocount,
	stc_drawmicro,
	stc_drawyourfrags,
	stc_drawhisfrags,
	stc_flashinitial,
	stc_drawflashcard,
	stc_drawgibhead,
	stc_drawhead,

	STC_NUMCMDTYPES
} stbarcmdtype_t;

typedef struct
{
	VINT	ammo, health, armor;
	VINT	godmode;
	VINT	face;
	char	cards[NUMCARDS];

	sbflash_t	flashCards[NUMCARDS];	/* INFO FOR FLASHING CARDS & SKULLS */

	VINT	yourFragsCount;
	VINT	hisFragsCount;
	VINT	drawface;
	char	weaponowned[NUMMICROS];

	VINT	facetics;
	VINT	newface;

	VINT	gibframe;
	VINT	gibdelay;
	char	gibdraw;

	char	doSpclFace;
	VINT	spclFaceType;

	short numstbarcmds;
	stbarcmd_t stbarcmds[STC_NUMCMDTYPES + NUMMICROS + NUMCARDS * 2 + NUMAMMO];

	VINT 	ammocount[NUMAMMO];
	VINT 	maxammocount[NUMAMMO];

	sbflash_t	yourFrags;			/* INFO FOR YOUR FRAG FLASHING */
	sbflash_t	hisFrags;
	char flashInitialDraw;		/* INITIALLY DRAW FRAG AMOUNTS (flag) */

	/* Messaging */
	char	specialFace;	/* Which type of special face to make */
	char	gotgibbed;			/* Got gibbed */
	char	tryopen[NUMCARDS];	/* Tried to open a card or skull door */
	char	forcedraw;
} stbar_t;

extern	stbar_t	*stbar;
extern	int stbar_tics;
extern void valtostr(char *string,int val);
void ST_DrawValue(int x,int y,int value);
void ST_DrawMicroValue(int x,int y,int value);
void ST_Num (int x, int y, int num);
void ST_InitEveryLevel(void);


