#include "doomdef.h"
#include "mars.h"
#include "mars_ringbuf.h"
#include "sounds.h"

#define MAX_SAMPLES		735

#define SAMPLE_RATE		22050
#define SAMPLE_MIN         2
#define SAMPLE_CENTER    517
#define SAMPLE_MAX      1032

// when to clip out sounds
// Does not fit the large outdoor areas.

#define S_CLIPPING_DIST (1224 * FRACUNIT)

// Distance tp origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// In the source code release: (160*FRACUNIT).  Changed back to the
// Vanilla value of 200 (why was this changed?)

#define S_CLOSE_DIST (200 * FRACUNIT)

// The range over which sound attenuates

#define S_ATTENUATOR (S_CLIPPING_DIST - S_CLOSE_DIST)

// Stereo separation

#define S_STEREO_SWING (96 * FRACUNIT)

enum
{
	SNDCMD_NONE,
	SNDCMD_CLEAR,
	SNDCMD_STARTSND,
};

static uint8_t snd_bufidx = 0;
int16_t __attribute__((aligned(16))) snd_buffer[2][MAX_SAMPLES * 2];
static uint8_t	snd_init = 0, snd_stopmix = 0;

static VINT		vgm_start;

sfxchannel_t	sfxchannels[SFXCHANNELS];

VINT 			sfxvolume = 64;	/* range 0 - 64 */
VINT 			musicvolume = 64;	/* range 0 - 64 */

VINT			musictype = mustype_fm;

static VINT		curmusic, muslooping = 0, curcdtrack = cdtrack_none;
int             samplecount = 0;

static marsrb_t	soundcmds = { 0 };

void S_StartSound(mobj_t* origin, int sound_id) ATTR_OPTIMIZE_SIZE;
void S_StartSoundReal(mobj_t* origin, unsigned sound_id, int vol) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void S_PaintChannel(void* mixer, int16_t* buffer, int32_t cnt, int32_t scale) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void S_Update(int16_t* buffer) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void S_Spatialize(mobj_t* origin, int* pvol, int* psep) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

/*
==================
=
= S_Init
=
==================
*/

void S_Init(void)
{
	int		i, l;
	int		initmusictype;

	/* init sound effects */
	for (i=1 ; i < NUMSFX ; i++)
	{
		S_sfx[i].lump = W_CheckNumForName(S_sfxnames[i]);
	}

	/* init music */
	num_music = 0;
	curmusic = mus_none;
	curcdtrack = cdtrack_none;
	muslooping = 0;

	vgm_start = l = W_CheckNumForName("VGM_STRT");
	if (l != -1)
		num_music = W_GetNumForName("VGM_END") - l - 1;

	Mars_RB_ResetAll(&soundcmds);

	Mars_InitSoundDMA();

	// FIXME: this is ugly, get rid of global variables!

	// musictype is now set to value from SRAM.
	// force proper initialization by resetting it to 'none',
	// so that S_SetMusicType won't ignore the new value
	initmusictype = musictype;
	musictype = mustype_none;
	S_SetMusicType(initmusictype);
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
	uint16_t *p = (uint16_t*)Mars_RB_GetWriteBuf(&soundcmds, 8, false);
	if (!p)
		return;
	*p++ = SNDCMD_CLEAR;
	Mars_RB_CommitWrite(&soundcmds);
}

void S_RestartSounds (void)
{
}

/*
==================
=
= S_Spatialize
=
==================
*/
static void S_Spatialize(mobj_t* origin, int *pvol, int *psep)
{
	int dist_approx;
	int dx, dy;
	int	vol, sep;
	player_t* player = &players[consoleplayer];

	if (!origin || origin == player->mo)
	{
		vol = sfxvolume;
		sep = 128;
	}
	else
	{
		angle_t angle;
		mobj_t *listener = player->mo;

		dx = D_abs(origin->x - listener->x);
		dy = D_abs(origin->y - listener->y);
		dist_approx = dx + dy - ((dx < dy ? dx : dy) >> 1);
		if (dist_approx > S_CLIPPING_DIST)
		{
			vol = 0;
			sep = 128;
		}
		else
		{
			// angle of source to listener
			angle = R_PointToAngle2(listener->x, listener->y,
				origin->x, origin->y);

			if (angle > listener->angle)
				angle = angle - listener->angle;
			else
				angle = angle + (0xffffffff - listener->angle);
			angle >>= ANGLETOFINESHIFT;

			FixedMul2(sep, S_STEREO_SWING, finesine(angle));
			sep >>= FRACBITS;

			sep = 128 - sep;
			if (sep < 0)
				sep = 0;
			else if (sep > 255)
				sep = 255;

			if (dist_approx < S_CLOSE_DIST)
				vol = sfxvolume;
			else if (dist_approx >= S_CLIPPING_DIST)
				vol = 0;
			else
			{
				vol = sfxvolume * (S_CLIPPING_DIST - dist_approx);
				vol = (unsigned)vol / S_ATTENUATOR;
				if (vol > sfxvolume)
					vol = sfxvolume;
			}
		}
	}

	*pvol = vol;
	*psep = sep;
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
	int vol, sep;
	sfxinfo_t *sfx;

	/* Get sound effect data pointer */
	if (sound_id <= 0 || sound_id >= NUMSFX)
		return;

	sfx = &S_sfx[sound_id];
	if (sfx->lump < 0)
		return;

	/* */
	/* spatialize */
	/* */
	S_Spatialize(origin, &vol, &sep);
	if (!vol)
		return; /* too far away */

	// HACK: boost volume for item pickups
	if (sound_id == sfx_itemup)
		vol <<= 1;

	uint16_t* p = (uint16_t*)Mars_RB_GetWriteBuf(&soundcmds, 8, false);
	if (!p)
		return;
	*p++ = SNDCMD_STARTSND;
	*p++ = sound_id;
	*(int*)p = (intptr_t)origin, p += 2;
	*p++ = vol;
	Mars_RB_CommitWrite(&soundcmds);
}

/*
===================
=
= S_UpdateSounds
=
===================
*/
void S_UpdateSounds(void)
{
}

void S_SetMusicType(int newtype)
{
	int savemus, savecd;

	if (newtype < mustype_none || newtype > mustype_cd)
		return;
	if (musictype == newtype)
		return;
	if (newtype == mustype_cd && !S_CDAvailable())
		return;

	// restart the current track
	savemus = curmusic;
	savecd = curcdtrack;

	if (musictype != mustype_none)
		S_StopSong();

	curmusic = mus_none;
	musictype = newtype;
	curcdtrack = cdtrack_none;
	S_StartSong(savemus, muslooping, savecd);
}

boolean S_CDAvailable(void)
{
	return mars_cd_ok != 0;
}

int S_SongForLump(int lump)
{
	if (lump <= 0)
		return mus_none;
	if (lump <= vgm_start || lump > vgm_start + num_music)
		return mus_none;

	return lump - vgm_start;
}

int S_SongForMapnum(int mapnum)
{
	int i;
	VINT songs[100];
	VINT numsongs;

	numsongs = 0;
	for (i = 0; i < num_music; i++) {
		VINT mus = i + 1;

		if (mus == gameinfo.titleMus)
			continue;
		if (mus == gameinfo.intermissionMus)
			continue;
		if (mus == gameinfo.victoryMus)
			continue;

		songs[numsongs++] = mus;
		if (numsongs == sizeof(songs) / sizeof(songs[0]))
			break;
	}

	if (numsongs == 0)
		return mus_none;
	return songs[(mapnum - 1) % numsongs];
}

void S_StartSong(int music_id, int looping, int cdtrack)
{
	int playtrack = 0;

	if (musictype == mustype_cd)
	{
		if (cdtrack == cdtrack_none)
		{
			S_StopSong();
			return;
		}

		/* recheck cd and get number of tracks */
		Mars_UpdateCD();

		int num_map_tracks = mars_num_cd_tracks + cdtrack_lastmap;
		if ((mars_cd_ok & 0x0100) && (num_map_tracks > 0))
		{
			/* there is a disc with at least enough tracks */
			if (cdtrack <= cdtrack_title)
				playtrack = cdtrack + mars_num_cd_tracks;
			else
				playtrack = cdtrack % num_map_tracks;
		}
	}
	else if (musictype == mustype_fm)
	{
		if (music_id > num_music)
			return;

		if (music_id == mus_none)
		{
			S_StopSong();
			return;
		}

		if (music_id == curmusic)
			return;

		playtrack = music_id;
	}

	curmusic = music_id;
	curcdtrack = cdtrack;
	muslooping = looping;

	if (musictype == mustype_none)
		return;

	if (musictype == mustype_cd)
		Mars_PlayTrack(1, playtrack, NULL, looping);
	else
		Mars_PlayTrack(0, playtrack, W_POINTLUMPNUM(vgm_start + playtrack), looping);
}

void S_StopSong(void)
{
	Mars_StopTrack();
	curmusic = mus_none;
	curcdtrack = cdtrack_none;
}

static void S_Update(int16_t* buffer)
{
	int i;
	int16_t* b;
	player_t* player;
	const size_t 
		xoff = offsetof(mobj_t, x) & ~15, 
		yoff = offsetof(mobj_t, y) & ~15;

	player = &players[consoleplayer];

	Mars_ClearCacheLines((intptr_t)player, (sizeof(player_t) + 15) / 16);
	Mars_ClearCacheLines((intptr_t)player->mo, (sizeof(mobj_t) + 15) / 16);

	{
		int32_t *b2 = (int32_t *)buffer;
		for (i = 0; i < MAX_SAMPLES / 4; i++)
			*b2++ = 0, *b2++ = 0, *b2++ = 0, *b2++ = 0;
		for (i *= 4; i < MAX_SAMPLES; i++)
			*b2++ = 0;
	}

	for (i = 0; i < SFXCHANNELS; i++)
	{
		sfxchannel_t* ch = &sfxchannels[i];

		if (!ch->data)
			continue;

		if (ch->origin)
		{
			int vol, sep;

			Mars_ClearCacheLines((intptr_t)ch->origin + xoff, 1);
			Mars_ClearCacheLines((intptr_t)ch->origin + yoff, 1);

			/* */
			/* spatialize */
			/* */
			S_Spatialize(ch->origin, &vol, &sep);

			if (!vol)
			{
				// inaudible
				ch->data = NULL;
				continue;
			}

			ch->volume = vol;
			ch->pan = sep;
		}

		S_PaintChannel(ch, buffer, MAX_SAMPLES, 64);
	}

	// convert buffer from s16 pcm samples to u16 pwm samples
	b = buffer;
	for (i = 0; i < MAX_SAMPLES; i++)
	{
		int16_t s;

		s = *b + SAMPLE_CENTER;
		*b++ = (s < 0) ? SAMPLE_MIN : (s > SAMPLE_MAX) ? SAMPLE_MAX : s;

		s = *b + SAMPLE_CENTER;
		*b++ = (s < 0) ? SAMPLE_MIN : (s > SAMPLE_MAX) ? SAMPLE_MAX : s;
	}
}

void sec_dma1_handler(void)
{
	SH2_DMA_CHCR1; // read TE
	SH2_DMA_CHCR1 = 0; // clear TE

	// start DMA on buffer and fill the other one
	SH2_DMA_SAR1 = ((intptr_t)snd_buffer[snd_bufidx]) | 0x20000000;
	SH2_DMA_TCR1 = MAX_SAMPLES; // number longs
	SH2_DMA_CHCR1 = 0x18E5; // dest fixed, src incr, size long, ext req, dack mem to dev, dack hi, dack edge, dreq rising edge, cycle-steal, dual addr, intr enabled, clear TE, dma enabled

	snd_bufidx ^= 1; // flip audio buffer

	if (snd_stopmix)
		return;

	Mars_Sec_ReadSoundCmds();

	S_Update(snd_buffer[snd_bufidx]);
}

void S_StartSoundReal(mobj_t* origin, unsigned sound_id, int vol)
{
	sfxchannel_t* channel, * newchannel;
	int i;
	int length, loop_length;
	sfxinfo_t* sfx;
	sfx_t* md_data;

	newchannel = NULL;
	sfx = &S_sfx[sound_id];
	md_data = W_POINTLUMPNUM(sfx->lump);
	length = md_data->samples;
	loop_length = 0;

	if (length < 4)
		return;

	/* reject sounds started at the same instant and singular sounds */
	for (channel = sfxchannels, i = 0; i < SFXCHANNELS; i++, channel++)
	{
		if (channel->sfx == sfx)
		{
			if (channel->position == 0)
			{
				if (channel->volume < vol)
				{
					newchannel = channel;
					goto gotchannel;	/* overlay this	*/
				}
				return;		/* exact sound allready started */
			}

			if (sfx->singularity)
			{
				newchannel = channel;	/* overlay this	*/
				goto gotchannel;
			}
		}
		if (origin && channel->origin == origin)
		{	/* cut off whatever was coming from this origin */
			newchannel = channel;
			goto gotchannel;
		}

		if (channel->data == NULL)
			newchannel = channel;	/* this is a dead channel, ok to reuse */
	}

	/* if there weren't any dead channels, try to kill an equal or lower */
	/* priority channel */

	if (!newchannel)
	{
		for (newchannel = sfxchannels, i = 0; i < SFXCHANNELS; i++, newchannel++)
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
	newchannel->position = 0;
	newchannel->increment = (11025 << 14) / SAMPLE_RATE;
	newchannel->length = length << 14;
	newchannel->loop_length = loop_length << 14;
	newchannel->data = &md_data->data[0];

	// volume and panning will be updated later in S_Spatialize
	newchannel->volume = vol;
	newchannel->pan = 128;
}

void Mars_Sec_ReadSoundCmds(void)
{
	int i;

	if (!snd_init)
		return;

	while (Mars_RB_Len(&soundcmds) >= 8) {
		short* p = Mars_RB_GetReadBuf(&soundcmds, 8);

		int cmd = *p;
		switch (cmd) {
		case SNDCMD_CLEAR:
			for (i = 0; i < SFXCHANNELS; i++)
				sfxchannels[i].data = NULL;
			break;
		case SNDCMD_STARTSND:
			S_StartSoundReal((void*)(*(intptr_t*)&p[2]), p[1], p[4]);
			break;
		}

		Mars_RB_CommitRead(&soundcmds);
	}
}

void Mars_Sec_InitSoundDMA(void)
{
	uint16_t sample, ix;

	Mars_ClearCache();

	Mars_RB_ResetRead(&soundcmds);

	// init DMA
	SH2_DMA_SAR1 = 0;
	SH2_DMA_DAR1 = (uint32_t)&MARS_PWM_STEREO; // storing a long here will set the left and right channels
	SH2_DMA_TCR1 = 0;
	SH2_DMA_CHCR1 = 0;
	SH2_DMA_DRCR1 = 0;

	// init the sound hardware
	MARS_PWM_MONO = 1;
	MARS_PWM_MONO = 1;
	MARS_PWM_MONO = 1;
	if (MARS_VDP_DISPMODE & MARS_NTSC_FORMAT)
		MARS_PWM_CYCLE = (((23011361 << 1) / (SAMPLE_RATE)+1) >> 1) + 1; // for NTSC clock
	else
		MARS_PWM_CYCLE = (((22801467 << 1) / (SAMPLE_RATE)+1) >> 1) + 1; // for PAL clock
	MARS_PWM_CTRL = 0x0185; // TM = 1, RTP, RMD = right, LMD = left

	sample = SAMPLE_MIN;

	// ramp up to SAMPLE_CENTER to avoid click in audio (real 32X)
	while (sample < SAMPLE_CENTER)
	{
		for (ix = 0; ix < (SAMPLE_RATE * 2) / (SAMPLE_CENTER - SAMPLE_MIN); ix++)
		{
			while (MARS_PWM_MONO & 0x8000); // wait while full
			MARS_PWM_MONO = sample;
		}
		sample++;
	}

	snd_bufidx = 0;
	snd_init = 1;
	snd_stopmix = 0;

	Mars_Sec_StartSoundMixer();
}

void Mars_Sec_StopSoundMixer(void)
{
	SH2_DMA_CHCR1; // read TE
	SH2_DMA_CHCR1 = 0; // clear TE

	snd_stopmix = 1;
}

void Mars_Sec_StartSoundMixer(void)
{
	snd_stopmix = 0;

	// fill first buffer
	S_Update(snd_buffer[snd_bufidx]);

	// start DMA
	sec_dma1_handler();
}
