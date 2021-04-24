/* s_sound.c */
#include "doomdef.h"
#include "music.h"

#define EXTERN_BUFFER_SIZE (EXTERNALQUADS*32/2)

sfxchannel_t	sfxchannels[SFXCHANNELS];

boolean			channelschanged;	/* set by S_StartSound to signal */
									/* update to remix speculative samples */

int				finalquad;			/* the last quad mixed by update. */
									
int 			sfxvolume = 128;	/* range 0 - 255 */
int 			musicvolume = 128;	/* range 0 - 255 */
int				oldsfxvolume = 128;	/* to detect transition to sound off */

int				soundtics;			/* time spent mixing sounds */
int				soundstarttics;		/* time S_Update started */

int				sfxsample;			/* the sample about to be output */
									/* by S_WriteOutSamples */

/*			 MUSIC VARIABLES */

#ifndef MARS
sfx_t 		*instruments[256];
channel_t       music_channels[10];	/* master music channel list */
#endif

int             musictime;			/* internal music time, follows samplecount */
int             next_eventtime;		/* when next event will occur */
unsigned char   *music;				/* pointer to current music data */
unsigned char   *music_start;		/* current music start pointer */
unsigned char   *music_end;			/* current music end pointer */
unsigned char	*music_memory;		/* current location of cached music */

int             samples_per_midiclock;	/* multiplier for midi clocks */

int				musictics = 0;


/*
==================
=
= S_Init
=
==================
*/

void S_Init(void)
{
#ifndef MARS
	int		i,l;
	int	lump, end;
	int instnum;

/*				SFX */
	
	for (i=1 ; i < NUMSFX ; i++)
	{
		l = W_CheckNumForName(S_sfxnames[i]);
		if (l != -1)
			S_sfx[i].md_data = W_POINTLUMPNUM(l);
	}	

/*				MUSIC */

 	D_memset(instruments, 0, 256 * 4);
 	lump = W_GetNumForName("inststrt");			/* get available instruments[] */
 	end	= W_GetNumForName("instend");
 	while (lump != end)
 	{
 		instnum = (lumpinfo[lump].name[1]-'0')*100
 				+ (lumpinfo[lump].name[2]-'0')*10
 				+ (lumpinfo[lump].name[3]-'0')
 				+ (lumpinfo[lump].name[0] == 'P' ? 128 : 0);
 		instruments[instnum] = (sfx_t *) (wadfileptr + lumpinfo[lump].filepos);
 		lump++;
 	}
 	/* hack test */

	D_memset(music_channels, 0, sizeof(music_channels));
#endif
	music_memory = 0;
	music = 0;
	musictime = 0;
	next_eventtime = 0;
/*	S_StartSong(1,1); */
  
}


/*
==================
=
= S_Clear
=
==================
*/

void S_Clear (void)
{
	D_memset (sfxchannels,0,sizeof(sfxchannels));
	D_memset (soundbuffer,0,sizeof(soundbuffer));
}

void S_RestartSounds (void)
{
}

/*
==================
=
= S_StartSound
=
==================
*/

void S_StartSound(mobj_t *origin, int sound_id)
{
#ifdef JAGUAR
	sfxchannel_t	*channel, *newchannel;
	int 			i;
	int 		dist_approx;
	player_t 	*player;
	int 		dx, dy;
	short		vol;
	sfxinfo_t	*sfx;

/* */
/* spatialize */
/* */
	player = &players[consoleplayer];

	if (!origin || origin == player->mo)
		vol = 127;
	else
	{
		dx = D_abs(origin->x - player->mo->x);
		dy = D_abs(origin->y - player->mo->y);
		dist_approx = dx + dy - ((dx < dy ? dx : dy) >> 1);
		vol = dist_approx >> 20;
		if (vol > 127)
			return;		/* too far away */
		vol = 127 - vol;
	}


/* Get sound effect data pointer */
	sfx = &S_sfx[sound_id];
	
	newchannel = NULL;
	
/* reject sounds started at the same instant and singular sounds */
	for (channel=sfxchannels,i=0 ; i<SFXCHANNELS ; i++,channel++)
	{
		if (channel->sfx == sfx)
		{
			if (channel->startquad == finalquad)
			{
				return;		/* exact sound allready started */
			}

			if (sfx->singularity)
			{
				newchannel = channel;	/* overlay this	 */
				goto gotchannel;
			}
		}
		if (channel->origin == origin)
		{	/* cut off whatever was coming from this origin */
			newchannel = channel;
			goto gotchannel;
		}
		
		if (channel->stopquad <= finalquad)
			newchannel = channel;	/* this is a dead channel, ok to reuse */
	}

/* if there weren't any dead channels, try to kill an equal or lower */
/* priority channel */

	if (!newchannel)
	{
		for (newchannel=sfxchannels,i=0 ; i<SFXCHANNELS ; i++, newchannel++)
			if (newchannel->sfx->priority >= sfx->priority)
				goto gotchannel;
		return;		/* couldn't override a channel */
	}


/* */
/* fill in the new values */
/* */
gotchannel:
	newchannel->sfx = sfx;
	newchannel->origin = origin;
	newchannel->startquad = finalquad;
	newchannel->stopquad = finalquad + (sfx->md_data->samples>>2);
	newchannel->source = (int *)&sfx->md_data->data;	
	newchannel->volume = vol * (short)sfxvolume;
	
#endif
}


/*
===================
=
= S_UpdateSounds
=
===================
*/

extern	int	sfx_start;
extern	int music_dspcode;

void S_UpdateSounds(void)
{
#ifdef JAGUAR

	int st;

/* */
/* if sound was just turned off, clear out the buffer */
/* */
	if (!sfxvolume)
	{
		if (oldsfxvolume)
		{
			oldsfxvolume = 0;
			S_Clear ();
		}
		return;
	}
	else
	{
		if (!oldsfxvolume)
			finalquad = (samplecount >> 3) - 100;	/* don't mix lots of junk */
		oldsfxvolume = sfxvolume;
	}
	
/* */
/* print debugging info */
/* */
#if 0
	for (i=0 ; i<SFXCHANNELS ; i++)
	{
		PrintNumber (1,1+i,sfxchannels[i].sfx - S_sfx);
	}

#endif

	soundstarttics = samplecount;		/* for timing calculations */

/* */
/* run the mixing in parallel on the dsp */
/*	 */

	if (music)
	{
		
		if (!musictime)
			musictime = next_eventtime = samplecount + EXTERN_BUFFER_SIZE/2;
		while (samplecount - musictime > EXTERN_BUFFER_SIZE)
		{
			musictime += EXTERN_BUFFER_SIZE;
			next_eventtime += EXTERN_BUFFER_SIZE;
		}
		st = samplecount;
		DSPFunction (&music_dspcode);
		musictics = samplecount - st;

	}
	else
	{
#if 0
((int *)0x1f8000)[ticon*2] = samplecount;
((int *)0x1f8000)[ticon*2+1] = sfxsample;
#endif
		dspfinished = 0x1234;
		dspcodestart = (int)&sfx_start;
	}

#endif
}

void S_StartSong(int music_id, int looping)
{
#ifndef MARS
	int lump;

/*	next_eventtime = musictime; */
	musictime = 0;
	samples_per_midiclock = 0;
	lump = W_GetNumForName(S_music[music_id].name);
	music_memory = music = 
		(unsigned char *) W_CacheLumpNum(lump, PU_STATIC);
	music_start = looping ? music : 0;
	music_end = (unsigned char *) music + lumpinfo[lump].size ;
#endif
}

void S_StopSong(void)
{
#ifndef MARS
	int i;
	int *ptr;

	Z_Free (music_memory);
	music = 0;							/* prevent the DSP from running */

	ptr = soundbuffer+1;				/* clear music output buffer */
	for (i=(EXTERNALQUADS*32)/4;i;i-=8)
	{
		ptr[0] = 0;
		ptr[2] = 0;
		ptr[4] = 0;
		ptr[6] = 0;
		ptr += 8;
	}
#endif
}

