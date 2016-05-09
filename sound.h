#define SAMPLERATE 22050
#define NUM_OUTPUT_CHANNELS	1	/* 1 for mono, 2 for stereo, 4 for quadraphonic */
/*#define EXTERN_BUFFER_SIZE	4096 */
/*#define EXTERN_MASK	(EXTERN_BUFFER_SIZE-1) */
/*#define INTERNAL_BUFFER_SIZE 256 */
#define INTERNAL_BUFFER_SIZE 256	/* JAG MOD */
#define INTERNAL_MASK	(INTERNAL_BUFFER_SIZE-1)
#define NUM_OLD_CHANNELS (EXTERNAL_BUFFER_SIZE/INTERNAL_BUFFER_SIZE)

typedef struct sampleset_struct
{
	short			output[NUM_OUTPUT_CHANNELS];
} sampleset_t;

typedef struct unsat_sampleset_struct
{
	int				output[NUM_OUTPUT_CHANNELS];
} unsat_sampleset_t;

/* */
/* masks for the info field of sfx_t */
/* */

#define PERCUSSION 1
#define LOOPING 2

/*
typedef struct sfx_struct
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
*/

typedef struct channel_struct
{
	sfx_t			*sfx;
	unsigned char	*data;
	int				vol[NUM_OUTPUT_CHANNELS];
	int				step;
	int				step_leftover;
	int				decay_step;
	unsigned int	end_time;
} channel_t;

/* WRITE (before I_InitSound) or READ */

extern channel_t		internal_channels[];
extern channel_t		*channels;
extern int				new_channels;

/*
#ifdef GPU
#define extern_buffer ((int*)0x1f0000)	// JAG MOD
#else
extern	int				 extern_buffer[];
#endif
*/

extern	sampleset_t addin_buffer[];

/* variables passed to S_PaintSound */

extern	int			startchannel;
extern	int			endchannel;
extern	int			starttime;
extern	int			endtime;

extern	int			deadchannels;

extern	channel_t	old_channels[];
extern	int			old_channels_time;
extern	int			sfxtime;

void S_InitSound(void);
void S_PaintSound(void);
void S_PaintUnsatSound(int samples);

