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

#define S_CLIPPING_DIST (1200 * FRACUNIT)

// Distance tp origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// In the source code release: (160*FRACUNIT).  Changed back to the
// Vanilla value of 200 (why was this changed?)

#define S_CLOSE_DIST (200 * FRACUNIT)

// The range over which sound attenuates

#define S_ATTENUATOR ((S_CLIPPING_DIST - S_CLOSE_DIST) >> FRACBITS)

// Stereo separation

#define S_STEREO_SWING (96 * FRACUNIT)

enum
{
	SNDCMD_NONE,
	SNDCMD_CLEAR,
	SNDCMD_STARTSND,
	SNDCMD_BEGINFRAME,
	SNDCMD_ENDFRAME,
};

int16_t __attribute__((aligned(16))) snd_buffer[2][MAX_SAMPLES * 2];

sfxchannel_t	sfxchannels[SFXCHANNELS];

int 			sfxvolume;	/* range 0 - 64 */
int 			musicvolume;	/* range 0 - 64 */
int				oldsfxvolume;	/* to detect transition to sound off */

int             samplecount = 0;

static marsrb_t	soundcmds;

static int		sndinit = 0;

extern short	use_cd;
extern short	cd_ok;

void S_PaintChannel(void* mixer, int16_t* buffer, int32_t cnt, int32_t scale) ATTR_DATA_CACHE_ALIGN;
static void S_Update(int16_t* buffer) ATTR_DATA_CACHE_ALIGN;
static void S_Spatialize(mobj_t* origin, int* pvol, int* psep) ATTR_DATA_CACHE_ALIGN;
void slave_dma1_handler(void) ATTR_DATA_CACHE_ALIGN;

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

	/* init sound effects */
	for (i=1 ; i < NUMSFX ; i++)
	{
		l = W_CheckNumForName(S_sfxnames[i]);
		if (l != -1)
			S_sfx[i].md_data = W_POINTLUMPNUM(l);
		else
			S_sfx[i].md_data = NULL;
	}

	/* init music */
	num_music = 0;
	mus_intro = 0;
	l = W_CheckNumForName("VGM_STRT");
	if (l != -1)
	{
		int e, n;

		e = W_GetNumForName("VGM_END");
		n = e - ++l;

		S_music = Z_Malloc(sizeof(*S_music) * n, PU_STATIC, 0);
		for (i = 0; i < n; i++) {
			S_music[i].name = (char *)W_GetNameForNum(l);
			S_music[i].md_data = W_POINTLUMPNUM(l);

			if (!D_strncasecmp(S_music[i].name, "mus_ntro", 8)) {
				mus_intro = i + 1;
				num_music = i;
			}

			l++;
		}
	}

	if (num_music == 0)
		num_music = 1; // so that num % num_music works

	sfxvolume = 64;
	musicvolume = 64;
	oldsfxvolume = 64;

	Mars_RB_ResetAll(&soundcmds);

	Mars_InitSoundDMA();

	/* play intro music once */
	S_StartSong(mus_intro,0);
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
	uint16_t *p = (uint16_t*)Mars_RB_GetWriteBuf(&soundcmds, 8);
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
	player_t* player;
	int dx, dy;
	int	vol, sep;

	player = &players[consoleplayer];
	player = (void*)((intptr_t)player | 0x20000000);

	if (!origin || origin == player->mo)
	{
		vol = sfxvolume;
		sep = 128;
	}
	else
	{
		angle_t angle;
		mobj_t* listener = player->mo;

		origin = (void*)((intptr_t)origin | 0x20000000);
		listener = (void*)((intptr_t)listener | 0x20000000);

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

			sep = 128 - (FixedMul(S_STEREO_SWING, finesine(angle)) >> FRACBITS);
			if (sep < 0)
				sep = 0;
			else if (sep > 255)
				sep = 255;

			if (dist_approx < S_CLOSE_DIST)
				vol = sfxvolume;
			else
			{
				vol = (sfxvolume * ((S_CLIPPING_DIST - dist_approx) >> FRACBITS))
					/ S_ATTENUATOR;
				if (vol < 0)
					vol = 0;
				else if (vol > sfxvolume)
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
	if (!sfx->md_data)
		return;

	/* */
	/* spatialize */
	/* */
	S_Spatialize(origin, &vol, &sep);
	if (!vol)
		return; /* too far away */

	uint16_t* p = (uint16_t*)Mars_RB_GetWriteBuf(&soundcmds, 8);
	if (!p)
		return;
	*p++ = SNDCMD_STARTSND;
	*p++ = sound_id;
	*(int*)p = (intptr_t)origin, p += 2;
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

void S_StartSong(int music_id, int looping)
{
	S_StopSong();

	while (MARS_SYS_COMM0) ;

	if (music_id == mus_none) {
		return;
	}

	MARS_SYS_COMM2 = music_id | (looping ? 0x8000:0x0000);
	*(volatile intptr_t *)&MARS_SYS_COMM12 = (intptr_t)S_music[music_id - 1].md_data;
	MARS_SYS_COMM0 = 0x0300; /* start music */
}

void S_StopSong(void)
{
	while (MARS_SYS_COMM0) ;
	MARS_SYS_COMM0 = 0x0400; /* stop music */
}

static void S_Update(int16_t* buffer)
{
	int i;
	int16_t* b;

	b = buffer;
	for (i = 0; i < MAX_SAMPLES * 2; i++)
		*b++ = 0;

	for (i = 0; i < SFXCHANNELS; i++)
	{
		sfxchannel_t* ch = &sfxchannels[i];
		int vol, sep;

		if (!ch->data)
			continue;

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

		S_PaintChannel(ch, buffer, MAX_SAMPLES, 64);
	}

	// convert buffer from s16 pcm samples to u16 pwm samples
	b = buffer;
	for (i = 0; i < MAX_SAMPLES * 2; i++)
	{
		int16_t s = *b + SAMPLE_CENTER;
		*b++ = (s < 0) ? SAMPLE_MIN : (s > SAMPLE_MAX) ? SAMPLE_MAX : s;
	}
}

void slave_dma1_handler(void)
{
	static uint8_t idx = 0;

	SH2_DMA_CHCR1; // read TE
	SH2_DMA_CHCR1 = 0; // clear TE

	// start DMA on buffer and fill the other one
	SH2_DMA_SAR1 = ((intptr_t)snd_buffer[idx]) | 0x20000000;
	SH2_DMA_TCR1 = MAX_SAMPLES; // number longs
	SH2_DMA_CHCR1 = 0x18E5; // dest fixed, src incr, size long, ext req, dack mem to dev, dack hi, dack edge, dreq rising edge, cycle-steal, dual addr, intr enabled, clear TE, dma enabled

	idx ^= 1; // flip audio buffer

	S_Update(snd_buffer[idx]);
}

static void S_StartSoundReal(mobj_t* origin, unsigned sound_id)
{
	sfxchannel_t* channel, * newchannel;
	int i;
	int length, loop_length;
	sfxinfo_t* sfx;

	newchannel = NULL;
	sfx = &S_sfx[sound_id];
	length = sfx->md_data->samples;
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
				return;		/* exact sound allready started */
			}

			if (sfx->singularity)
			{
				newchannel = channel;	/* overlay this	 */
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
	newchannel->data = &sfx->md_data->data[0];

	// volume and panning will be updated later in S_Spatialize
	newchannel->volume = 64;
	newchannel->pan = 128;
}

void Mars_Slave_ReadSoundCmds(void)
{
	int i;

	if (!sndinit)
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
			S_StartSoundReal((void*)(*(intptr_t*)&p[2]), p[1]);
			break;
		}

		Mars_RB_CommitRead(&soundcmds);
	}
}

void Mars_Slave_InitSoundDMA(void)
{
	uint16_t sample, ix;

	Mars_ClearCache();

	Mars_RB_ResetRead(&soundcmds);

	// init DMA
	SH2_DMA_SAR0 = 0;
	SH2_DMA_DAR0 = 0;
	SH2_DMA_TCR0 = 0;
	SH2_DMA_CHCR0 = 0;
	SH2_DMA_DRCR0 = 0;
	SH2_DMA_SAR1 = 0;
	SH2_DMA_DAR1 = 0x20004038; // storing a word here will the MONO channel
	SH2_DMA_TCR1 = 0;
	SH2_DMA_CHCR1 = 0;
	SH2_DMA_DRCR1 = 0;
	SH2_DMA_DMAOR = 1; // enable DMA

	SH2_DMA_VCR1 = 72; // set exception vector for DMA channel 1
	SH2_INT_IPRA = (SH2_INT_IPRA & 0xF0FF) | 0x0F00; // set DMA INT to priority 15

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

	sndinit = 1;

	SetSH2SR(2);

	// fill first buffer
	S_Update(snd_buffer[0]);

	// start DMA
	slave_dma1_handler();
}
