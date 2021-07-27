/* newsfx.h */

typedef struct
{
    int     samples;
    int     loop_start;
    int     loop_end;
    int     info;
    int     unity;
	int		pitch_correction;
	int		decay_step;
    unsigned char data[1];
} sfx_t;

typedef struct sfxinfo_s
{
	char singularity;	/* Sfx singularity (only one at a time) */
	unsigned char priority;		/* Sfx priority */
#ifdef MARS
	short lump;
#else
	int	pitch;		/* pitch if a link */
	int	volume;		/* volume if a link */
	struct sfxinfo_s* link;	/* referenced sound if a link */
	sfx_t* md_data;	/* machine-dependent sound data */
#endif
} sfxinfo_t;

typedef struct
{
#ifdef MARS
	short lump;
#else
  void *md_data;	/* machine-dependent music data */
#endif
} musicinfo_t;

/*============================================================================ */

#define	INTERNALQUADS	256			/* 4k / 16 bytes per quad (64 bits) */
#define	EXTERNALQUADS	512			/* 16k  / 32 bytes per quad (16 bits+music) */
#define	SFXCHANNELS		4

typedef struct
{
#ifdef MARS
	uint8_t		*data;
	int			position;
	int			increment;
	int			length;
	int			loop_length;
	uint8_t		volume;
	uint8_t		pan;
#else
	unsigned* source;			/* work in groups of 4 8 bit samples */
	int			startquad;
	int			stopquad;
	int			volume;				/* range from 0-32k */
#endif

	sfxinfo_t	*sfx;
	mobj_t		*origin;
} sfxchannel_t;

enum
{
	mustype_none,
	mustype_fm,
	mustype_cd,
};

extern	sfxchannel_t	sfxchannels[SFXCHANNELS];

extern	int		finalquad;			/* the last quad mixed by update. */
									
extern	VINT	sfxvolume;			/* range 0 - 255 */
extern	VINT 	musicvolume;		/* range 0 - 255 */
extern	VINT	oldsfxvolume;		/* to detect transition to sound off */

extern	int		soundtics;			/* time spent mixing sounds */
extern	int		soundstarttics;		/* time S_Update started */

extern	int		sfxsample;			/* the sample about to be output */
									/* by S_WriteOutSamples */
									
/* external buffer for sfx and music */
extern	int		soundbuffer[EXTERNALQUADS*16]; 

extern	int		samplecount;		/* 22khz sample counter in DSP memory */

extern	VINT	musictype;

/*============================================================================ */

void S_Init(void) ATTR_OPTIMIZE_SIZE;
void S_Clear (void);
void S_StartSound(mobj_t *origin, int sound_id);
void S_UpdateSounds(void);
boolean S_CDAvailable(void);
void S_SetMusicType(int t);

