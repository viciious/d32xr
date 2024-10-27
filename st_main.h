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

#define KEYX		124
#define REDKEYY		3
#define BLUKEYY		15
#define YELKEYY		27
#define KEYW		16
#define KEYH		12

#define FACEX		144
#define FACEY		5
#define FACEW		32
#define FACEH		32

#define ARMORX		226
#define ARMORY		HEALTHY

#define MAPX		316
#define MAPY		HEALTHY

#define YOURFRAGX	(MAPX - 28)
#define YOURFRAGY	8

#define HISFRAGX	(MAPX - 28)
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
	stc_drawmap,
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
	int     score;
	int     exiting;
	VINT    deadTimer;
	VINT    rings;
	VINT    lives;

	VINT    msgTics;
	char    msg[128];

	boolean intermission;
} stbar_t;

extern	stbar_t	*stbar;
extern	int stbar_tics;
extern void valtostr(char *string,int val);
void ST_DrawValue(int x,int y,int value);
void ST_Num (int x, int y, int num);
void ST_InitEveryLevel(void);


