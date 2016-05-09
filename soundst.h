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
	char	*name;		/* up to 6-character name */
	boolean singularity;	/* Sfx singularity (only one at a time) */
	int		priority;		/* Sfx priority */
	struct sfxinfo_s *link;	/* referenced sound if a link */
	int		pitch;		/* pitch if a link */
	int		volume;		/* volume if a link */
	sfx_t	*md_data;	/* machine-dependent sound data */
} sfxinfo_t;

typedef struct
{
  char *name;		/* up to 6-character name */
  void *md_data;	/* machine-dependent music data */
} musicinfo_t;

/*============================================================================ */

#define	INTERNALQUADS	256			/* 4k / 16 bytes per quad (64 bits) */
#define	EXTERNALQUADS	512			/* 16k  / 32 bytes per quad (16 bits+music) */
#define	SFXCHANNELS		4

typedef struct
{
	unsigned	*source;			/* work in groups of 4 8 bit samples */
	int			startquad;
	int			stopquad;
	int			volume;				/* range from 0-32k */
	sfxinfo_t	*sfx;
	mobj_t		*origin;
} sfxchannel_t;

extern	sfxchannel_t	sfxchannels[SFXCHANNELS];

extern	int		finalquad;			/* the last quad mixed by update. */
									
extern	int		sfxvolume;			/* range 0 - 255 */
extern	int 	musicvolume;		/* range 0 - 255 */
extern	int		oldsfxvolume;		/* to detect transition to sound off */

extern	int		soundtics;			/* time spent mixing sounds */
extern	int		soundstarttics;		/* time S_Update started */

extern	int		sfxsample;			/* the sample about to be output */
									/* by S_WriteOutSamples */
									
/* external buffer for sfx and music */
extern	int		soundbuffer[EXTERNALQUADS*16]; 

extern	int		samplecount;		/* 22khz sample counter in DSP memory */

/*============================================================================ */

void S_Init(void);
void S_Clear (void);
void S_StartSound(mobj_t *origin, int sound_id);
void S_UpdateSounds(void);

