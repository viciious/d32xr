#include "doomdef.h"
#include "mars.h"
#ifndef DISABLE_DMA_SOUND
#include "mars_ringbuf.h"
#endif
#include "sounds.h"

#define MAX_SAMPLES        316		// 70Hz
//#define MAX_SAMPLES      440		// 50Hz
//#define MAX_SAMPLES      734		// 30Hz

#define SAMPLE_RATE      22050
#define SAMPLE_MIN       2
#define SAMPLE_MAX       1032
#define SAMPLE_CENTER    (SAMPLE_MAX-SAMPLE_MIN)/2

#define SPATIALIZATION_RATE		15 // Hz
// this many samples between each spatialization call
#define SPATIALIZATION_SRATE	(int)(MAX_SAMPLES*(((float)SAMPLE_RATE/MAX_SAMPLES)/SPATIALIZATION_RATE))

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

// WAV

#define S_LE_SHORT(chunk) (((chunk)[0]>>8)|(((chunk)[0]&0xff) << 8))
#define S_LE_LONG(chunk)  (S_LE_SHORT(chunk) | (S_LE_SHORT(chunk+1)<<16))

#define S_WAV_FORMAT_PCM         0x1
#define S_WAV_FORMAT_IMA_ADPCM   0x11
#define S_WAV_FORMAT_EXTENSIBLE  0xfffe

#define S_SEP_VOL_TO_MCD(sep,vol) do { if ((sep) > 254) (sep) = 254; (vol) <<= 2; /* 64 -> 256 max */ if ((vol) > 255) (vol) = 255; } while (0)

#ifdef DISABLE_DMA_SOUND
#define S_USE_MEGACD_DRV() (mcd_avail)
#else
#define S_USE_MEGACD_DRV() (mcd_avail && (sfxdriver == sfxdriver_auto || sfxdriver == sfxdriver_mcd))
#endif

enum
{
	SNDCMD_NONE,
	SNDCMD_CLEAR,
	SNDCMD_STARTSND,
	SNDCMD_STARTORGSND
};

#ifndef DISABLE_DMA_SOUND
static uint8_t snd_bufidx = 0;
int16_t __attribute__((aligned(16))) snd_buffer[2][MAX_SAMPLES * 2];

sfxchannel_t	pcmchannel;

int __attribute__((aligned(16))) pcm_data[4];

static marsrb_t	soundcmds = { 0 };

static unsigned snd_nopaintcount = 0;
#endif

static uint8_t	snd_init = 0;

static lumpinfo_t *vgm_tracks = NULL;
uint8_t			*vgm_ptr;

char 			spcmDir[9] = { 0 };

sfxchannel_t	sfxchannels[SFXCHANNELS];

VINT 			sfxvolume = 64;	/* range 0 - 64 */
VINT 			musicvolume = 64;	/* range 0 - 64 */

static VINT 	oldmusvol = -1;

VINT			musictype = mustype_fm;

static VINT		curmusic, muslooping = 0, curcdtrack = cdtrack_none;
int             samplecount = 0;

VINT 			sfxdriver = sfxdriver_auto, mcd_avail = 0; // 0 - auto, 2 - megacd, 2 - 32x

static sfxchannel_t *S_AllocateChannel(mobj_t* mobj, unsigned sound_id, int vol, int freq, getsoundpos_t getpos);
#ifndef DISABLE_DMA_SOUND
static void S_SetChannelData(sfxchannel_t* channel);
int S_PaintChannel4IMA(void* mixer, int16_t* buffer, int32_t cnt, int32_t scale) ATTR_DATA_CACHE_ALIGN;
int S_PaintChannel4IMA2x(void* mixer, int16_t* buffer, int32_t cnt, int32_t scale) ATTR_DATA_CACHE_ALIGN;
void S_PaintChannel8(void* mixer, int16_t* buffer, int32_t cnt, int32_t scale) ATTR_DATA_CACHE_ALIGN;
static int S_PaintChannel(sfxchannel_t *ch, int16_t* buffer, int painted) ATTR_DATA_CACHE_ALIGN;
static void S_Update(int16_t* buffer) ATTR_DATA_CACHE_ALIGN;
static void S_UpdatePCM(void) ATTR_DATA_CACHE_ALIGN;
static void S_Sec_DMA1Handler(void);
#endif
static void S_StartSoundEx(mobj_t *mobj, int sound_id, getsoundpos_t getpos);
static void S_SpatializeAt(fixed_t*origin, mobj_t* listener, int* pvol, int* psep) ATTR_DATA_CACHE_ALIGN;
static void S_Spatialize(mobj_t* mobj, int* pvol, int* psep, getsoundpos_t getpos) ATTR_DATA_CACHE_ALIGN;
static void S_SpatializeAll(void) ATTR_DATA_CACHE_ALIGN;

static void S_Pri_CmdHandler(void);

/*
==================
=
= S_Init
=
==================
*/

void S_Init(void)
{
	int		i;
	int 	start, end;
	VINT 	lumps[NUMSFX > 99 ? NUMSFX : 99];

	for (i = 0; i < SFXCHANNELS; i++)
		sfxchannels[i].data = NULL;

	// check if CD is available, reset the driver option if not
	mcd_avail = S_CDAvailable() & 0x1;
	if (!mcd_avail)
		sfxdriver = sfxdriver_auto;
	else
		sfxdriver = sfxdriver_mcd;

	/* init sound effects */
	for (i=1 ; i < NUMSFX ; i++)
	{
		S_sfx[i].lump = -1;
	}

	W_LoadPWAD(PWAD_CD);

	/* build an in-memory PWAD with all SFX */
	start = W_CheckNumForName("DS_START");
	end = W_CheckNumForName("DS_END");
	if (start >= 0 && end > start + 1)
	{
		int numsfx = end - start - 1;
		if (numsfx > NUMSFX)
			numsfx = NUMSFX;

		for (i=1 ; i < NUMSFX ; i++)
		{
			S_sfx[i].lump = W_CheckRangeForName(S_sfxnames[i], start, end);
		}

		if (mcd_avail)
		{
			lumpinfo_t li[NUMSFX];
			int sfxol[NUMSFX*2];

			for (i = 0; i < NUMSFX; i++)
			{
 				if (S_sfx[i].lump >= 0)
					S_sfx[i].lump -= start;
			}

			for (i = 0; i < numsfx; i++)
				lumps[i] = start + 1 + i;

			W_CacheWADLumps(li, numsfx, lumps, false);

			/* load all SFX in a single batch */
			for (i = 0; i < numsfx; i++)
			{
				sfxol[i*2] = li[i].filepos;
				sfxol[i*2+1] = li[i].size;
			}

			Mars_MCDLoadSfxFileOfs(1, numsfx, SOUNDS_PWAD_NAME, sfxol);
		}
	}

	W_LoadPWAD(PWAD_NONE);

	Mars_SetPriCmdCallback(&S_Pri_CmdHandler);

#ifndef DISABLE_DMA_SOUND
	Mars_RB_ResetAll(&soundcmds);
#endif

	Mars_InitSoundDMA(1);
}

/*
==================
=
= S_InitMusic
=
==================
*/

void S_InitMusic(void)
{
	int		i;
	int		initmusictype;
	int 	start, end;
	VINT 	lumps[99];

	/* init music */
	num_music = 0;
	muslooping = 0;
	S_StopSong();

	S_DeInitMusic();

	W_LoadPWAD(PWAD_CD);

	/* build an in-memory PWAD with all music */
	start = W_CheckNumForName("M_START");
	end = W_CheckNumForName("M_END");
	if (start >= 0 && end > start + 1)
	{
		num_music = end - start - 1;
		if (num_music > 99)
			num_music = 99;
		for (i = 0; i < num_music; i++)
			lumps[i] = start + i + 1;
		vgm_tracks = Z_Malloc(sizeof(*vgm_tracks) * num_music, PU_STATIC);
		W_CacheWADLumps(vgm_tracks, num_music, lumps, false);
	}

	W_LoadPWAD(PWAD_NONE);

	// FIXME: this is ugly, get rid of global variables!

	// musictype is now set to value from SRAM.
	// force proper initialization by resetting it to 'none',
	// so that S_SetMusicType won't ignore the new value
	initmusictype = musictype;
	musictype = mustype_none;
	S_SetMusicType(initmusictype);
}

void S_DeInitMusic(void)
{
	if (vgm_tracks)
		Z_Free(vgm_tracks);
	vgm_tracks = NULL;
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
	if (S_USE_MEGACD_DRV())
	{
		D_memset(sfxchannels, 0, sizeof(*sfxchannels) * SFXCHANNELS);
		Mars_MCDClearSfx();
		Mars_MCDFlushSfx();
		return;
	}

#ifndef DISABLE_DMA_SOUND
	volatile int tic;
	uint16_t *p = (uint16_t*)Mars_RB_GetWriteBuf(&soundcmds, 8, false);
	if (!p)
		return;
	*p++ = SNDCMD_CLEAR;

	Mars_RB_CommitWrite(&soundcmds);

	/* do not wait the reader indefinitely */
	/* a hack to get Gens KMod working */
	for (tic = I_GetTime(); I_GetTime() < tic + 10; )
	{
		if (Mars_RB_Len(&soundcmds) == 0)
			break;
	}
#endif
}

void S_RestartSounds (void)
{
}

void S_SetSoundDriver (int newdrv)
{
	S_Clear();

#ifndef DISABLE_DMA_SOUND
	sfxdriver = newdrv;
	switch (newdrv) {
		case sfxdriver_pwm:
			Mars_RB_ResetAll(&soundcmds);
			Mars_InitSoundDMA(0);
			break;
	}
#endif
}

/*
==================
=
= S_SpatializeAt
=
==================
*/
static void S_SpatializeAt(fixed_t* origin, mobj_t* listener, int* pvol, int* psep)
{
	int dist_approx;
	int dx, dy;
	angle_t angle;
	int	vol, sep;

	dx = D_abs(origin[0] - listener->x);
	dy = D_abs(origin[1] - listener->y);
	dist_approx = dx + dy - ((dx < dy ? dx : dy) >> 1);
	if (dist_approx > S_CLIPPING_DIST)
	{
		vol = 0;
		sep = 128;
	}
	else
	{
		// angle of source to listener
		angle = R_PointToAngle(listener->x, listener->y,
			origin[0], origin[1]);

		if (angle > listener->angle)
			angle = angle - listener->angle;
		else
			angle = angle + (0xffffffff - listener->angle);
		angle >>= ANGLETOFINESHIFT;

		sep = FixedMul(S_STEREO_SWING, finesine(angle));
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

	*pvol = vol;
	*psep = sep;
}

/*
==================
=
= S_Spatialize
=
==================
*/
static void S_Spatialize(mobj_t* mobj, int *pvol, int *psep, getsoundpos_t getpos)
{
	int	vol, sep;
	player_t* player = &players[consoleplayer];
	player_t* player2 = &players[consoleplayer^1];

	vol = sfxvolume;
	sep = 128;

	if (mobj)
	{
		fixed_t origin[2];

		if (getpos)
		{
			getpos(mobj, origin);
		}
		else
		{
			origin[0] = mobj->x, origin[1] = mobj->y;
		}

		if (mobj != player->mo)
			S_SpatializeAt(origin, player->mo, &vol, &sep);

		if (splitscreen && player2->mo)
		{
			int vol2, sep2;

			vol2 = sfxvolume;
			sep2 = 255;

			if (mobj != player2->mo)
				S_SpatializeAt(origin, player2->mo, &vol2, &sep2);

			sep = 128 + (vol2 - vol) * 2;
			vol = vol2 > vol ? vol2 : vol;
			if (sep < 0) sep = 0;
			else if (sep > 255) sep = 255;
		}
	}

	*pvol = vol;
	*psep = sep;
}

/*
==================
=
= S_SpatializeAll
=
==================
*/

static void S_SpatializeAll(void)
{
	int i;
	mobj_t *mo;

	for (i = 0; i < SFXCHANNELS; i++)
	{
		sfxchannel_t* ch = &sfxchannels[i];

		if (!ch->data)
			continue;

		mo = ch->mobj;
		if (mo)
		{
			int vol, sep;

			/* */
			/* spatialize */
			/* */
			if (!ch->getpos)
			{
				Mars_ClearCacheLine(&mo->x);
				Mars_ClearCacheLine(&mo->y);
			}

			S_Spatialize(mo, &vol, &sep, ch->getpos);

			if (!vol)
			{
				// inaudible
				ch->data = NULL;
				continue;
			}

			ch->volume = vol;
			ch->pan = sep;
		}
	}
}

/*
==================
=
= S_StartSoundEx
=
==================
*/
static void S_StartSoundEx(mobj_t *mobj, int sound_id, getsoundpos_t getpos)
{
	int vol, sep;
	sfxinfo_t *sfx;
	int freq = 0;
	boolean nosep = false;

	/* Get sound effect data pointer */
	if (sound_id <= 0 || sound_id >= NUMSFX)
		return;

	sfx = &S_sfx[sound_id];
	if (sfx->lump <= 0)
	{
		switch (sound_id)
		{
			// re-use general monster sound effects, but play them at a lower frequency
			case sfx_bospn:
				nosep = true;
				sound_id = sfx_popain;
				freq = 5512;
				break;
			case sfx_bosdth:
				nosep = true;
				sound_id = sfx_podth1;
				freq = 5512;
				break;
			case sfx_secret:
				sound_id = sfx_getpow;
				freq = 7350;
				break;
		}
		sfx = &S_sfx[sound_id];
	}

	if (sfx->lump <= 0)
		return;

	/* */
	/* spatialize */
	/* */
	S_Spatialize(mobj, &vol, &sep, getpos);
	if (!vol)
		return; /* too far away */

	// HACK: boost volume for item pickups
	if (sound_id == sfx_itemup)
		vol <<= 1;

	if (S_USE_MEGACD_DRV())
	{
		sfxchannel_t *ch;

		ch = S_AllocateChannel(mobj, sound_id, vol, freq, getpos);
		if (!ch)
			return;

		S_SEP_VOL_TO_MCD(sep, vol);
		if (sound_id == sfx_itemup && sep == 128)
			nosep = true;
		if (nosep)
			sep = 255; // full volume from both channels

		Mars_MCDPlaySfx((ch - sfxchannels) + 1, sfx->lump, sep, vol, freq);
		return;
	}

#ifndef DISABLE_DMA_SOUND
	uint16_t* p = (uint16_t*)Mars_RB_GetWriteBuf(&soundcmds, 8, false);
	if (!p)
		return;

	if (getpos)
	{
		*p++ = SNDCMD_STARTORGSND;
		*p++ = sound_id;
		*(int*)p = (intptr_t)mobj, p += 2;
		*(int*)p = (intptr_t)getpos, p += 2;
		*p++ = vol;
	}
	else
	{
		*p++ = SNDCMD_STARTSND;
		*p++ = sound_id;
		*(int*)p = (intptr_t)mobj, p += 2;
		*p++ = vol;
	}

	Mars_RB_CommitWrite(&soundcmds);
#endif
}

void S_StartSound(mobj_t* mobj, int sound_id)
{
	S_StartSoundEx(mobj, sound_id, NULL);
}

void S_StartPositionedSound(mobj_t* mobj, int sound_id, getsoundpos_t getpos)
{
	S_StartSoundEx(mobj, sound_id, getpos);
}

/*
===================
=
= S_UpdateSounds
=
===================
*/
void S_PreUpdateSounds(void)
{
	if (S_USE_MEGACD_DRV())
	{
		int i;
		sfxchannel_t *ch;
		int status, bit;

		bit = 1;
		status = Mars_MCDGetSfxPlaybackStatus();
		for (ch = sfxchannels, i = 0; i < SFXCHANNELS; i++, ch++, bit <<= 1)
		{
			if (!ch->data)
				continue;

			if (!(status & bit)) {
				// stopped
				ch->data = NULL;
			} else {
				ch->position = 1;
			}
		}
	}
}

void S_UpdateSounds(void)
{
	if (oldmusvol != musicvolume) {
		if (S_CDAvailable()) {
			int vol = musicvolume*4;
			Mars_SetMusicVolume(vol > 255 ? 255 : vol);
		}
		oldmusvol = musicvolume;
	}

	if (S_USE_MEGACD_DRV())
	{
		int i;
		sfxchannel_t *ch;

		// spatalize
		S_SpatializeAll();

		for (ch = sfxchannels, i = 0; i < SFXCHANNELS; i++, ch++)
		{
			int vol, sep;

			if (!ch->data)
				continue;

			sep = ch->pan;
			vol = ch->volume;
			S_SEP_VOL_TO_MCD(sep, vol);

			Mars_MCDUpdateSfx(i + 1, sep, vol, 0);
		}

		Mars_MCDFlushSfx();
	}
}

void S_SetSPCMDir(const char *dir)
{
	D_snprintf(spcmDir, sizeof(spcmDir), "%s", dir);
}

void S_SetMusicType(int newtype)
{
	int savemus, savecd, saveloop;

	if (newtype < mustype_none || newtype > mustype_spcmhack)
		return;
	if (musictype == newtype)
		return;
	if (newtype == mustype_spcmhack)
		newtype = mustype_spcm;
	if (newtype >= mustype_cd && !S_CDAvailable())
		return;
	if (newtype == mustype_spcm && *spcmDir == '\0')
		return;

	// restart the current track
	savemus = curmusic;
	savecd = curcdtrack;
	saveloop = muslooping;

	if (musictype != mustype_none)
		S_StopSong();

	curmusic = mus_none;
	musictype = newtype;
	muslooping = -1;
	curcdtrack = cdtrack_none;

	S_StartSong(savemus, saveloop, savecd);
}

int S_CDAvailable(void)
{
	/* recheck cd and get number of tracks */
	Mars_UpdateCD();
	return mars_cd_ok;
}

int S_SongForMapnum(int mapnum)
{
	int numsongs;
	numsongs = num_music;
	if (numsongs <= 0)
		return mus_none;
	return (mapnum - 1) % numsongs + 1;
}

int S_SongForName(const char *str)
{
	int i;
	char name[9];
	int v[2];

	if (num_music == 0)
		return 0;
	if (!str || !*str)
		return 0;

	D_memset (name, 0, 8);
	D_strncpy (name, str, 8);
	name[8] = 0;

	for (i = 0; i < 8; i++) {
		if (name[i] >= 'a' && name[i] <= 'z') {
			name[i] = 'A' + name[i] - 'a';
		}
	}

	D_memcpy(v, name, 8);

	for (i = 0; i < num_music; i++) {
		if (*(int*)&vgm_tracks[i].name[0] == v[0] &&
			*(int*)&vgm_tracks[i].name[4] == v[1])
			return i + 1;
	}

	return mus_none;
}

void S_StartSong(int musiclump, int looping, int cdtrack)
{
	int playtrack = 0;
	char filename[64];

	if (musiclump < 0)
		musiclump = mus_none;

	if (musictype == mustype_cd)
	{
		// ignore the first data track
		int num_cd_tracks = (int)mars_num_cd_tracks - 1;

		if (S_CDAvailable())
		{
			/* there is a disc with at least enough tracks */
			if (num_cd_tracks == 0)
				playtrack = -1;
			else if (cdtrack < 0)
				playtrack = num_cd_tracks + cdtrack;
			else
				playtrack = 1 + (cdtrack - 1) % num_cd_tracks;
			playtrack++; // track no 1 is the data track
		}

		if (playtrack < 0)
			return;

		if (curcdtrack == cdtrack && muslooping == looping)
			return;
	}
	else if (musictype == mustype_fm)
	{
		if (musiclump <= num_music)
			playtrack = musiclump;

		if (musiclump == mus_none)
		{
			S_StopSong();
			return;
		}

		if (curmusic == musiclump && muslooping == looping)
			return;
	}
	else if (musictype == mustype_spcm)
	{
		char name[9];

		if (musiclump <= num_music)
			playtrack = musiclump;

		if (musiclump == mus_none)
		{
			S_StopSong();
			return;
		}
		if (*spcmDir == '\0')
			return;

		D_memcpy(name, vgm_tracks[playtrack-1].name, 8);
		name[8] = 0;
		D_snprintf(filename, sizeof(filename), "%s/%s.PCM", spcmDir, name);

		if (curmusic == musiclump && muslooping == looping)
		{
			Mars_MCDResumeSPCMTrack();
			return;
		}
	}

	curmusic = musiclump;
	curcdtrack = cdtrack;
	muslooping = looping;
	oldmusvol = -1; // force-update music volume

	if (musictype == mustype_none || playtrack == 0)
		return;

	Mars_StopTrack();

	if (musictype == mustype_spcm)
	{
		Mars_PlayTrack(0, playtrack, filename, -1, 0, looping);
		return;
	}

	if (musictype == mustype_cd)
	{
		Mars_PlayTrack(1, playtrack, cd_pwad_name, -1, 0, looping);
		return;
	}

	Mars_PlayTrack(0, playtrack, cd_pwad_name, vgm_tracks[playtrack-1].filepos, vgm_tracks[playtrack-1].size, looping);
}

void S_StartSongByName(const char *name, int looping, int cdtrack)
{
	int song;

	song = S_SongForName(name);
	if (cdtrack == cdtrack_none)
		cdtrack = song;

	S_StartSong(song, looping, cdtrack);
}

void S_StopSong(void)
{
	Mars_StopTrack();
	curmusic = mus_none;
	curcdtrack = cdtrack_none;
}

#ifndef DISABLE_DMA_SOUND

static void S_ClearPCM(void)
{
	D_memset(pcm_data, 0, sizeof(pcm_data));
	D_memset(&pcmchannel, 0, sizeof(pcmchannel));
	pcmchannel.volume = /*musicvolume*/40; /* 64 seems to be too loud */
	pcmchannel.pan = 128;
}

static void S_UpdatePCM(void)
{
	int inc, len, offs, flag;

	Mars_ClearCacheLine(pcm_data);
	inc = pcm_data[0]; /* cache read line loads all vars at once to cache */
	len = pcm_data[1];
	offs = pcm_data[2];
	flag = pcm_data[3];

	if (flag & 1)
	{
		if (offs == -1)
		{
			pcmchannel.data = NULL;
			((volatile short *)pcm_data)[7] = 0; // unset bit 0 for flag
			return;
		}

		/* do once */
		pcmchannel.position = 0;
		pcmchannel.increment = inc;
		pcmchannel.length = len;
		pcmchannel.data = vgm_ptr + offs;
		((volatile short *)pcm_data)[7] = 0; // unset bit 0 for flag
	}
}

static int S_PaintChannel(sfxchannel_t *ch, int16_t* buffer, int painted)
{
	if (!ch->data)
		return 0;

	if (ch->width == 4)
	{
		int i = MAX_SAMPLES;
		int32_t *end = (int32_t *)buffer + MAX_SAMPLES;
		int (*paintch)(void* mixer, int16_t* buffer, int32_t cnt, int32_t scale);

		// general mixing routine
		paintch = S_PaintChannel4IMA;

		if ((ch->increment < (1 << 14)) && !(ch->increment & (ch->increment-1))) {
			// optimized 2x upsampling mixer: from 11kHz to 22kHz or 44kHz
			paintch = S_PaintChannel4IMA2x;
		}

		do
		{
			if (ch->position >= ch->length)
			{
				// advance to next block
				ch->data += ch->block_size-3;
				ch->length = 0;
			}

			if (!ch->length)
			{
				uint8_t *block = (uint8_t *)ch->data;
				int block_size = ch->block_size;

				if (block_size > ch->remaining_bytes)
					block_size = ch->remaining_bytes;
				if (block_size < 4)
				{
					// EOF
					ch->data = NULL;
					break;
				}

				ch->position = 1 << 14;
				ch->prev_pos = -1; // force output of initial predictor
				ch->length = ((block_size-3) << 1) << 14;
				// initial step_index : initial predictor
				ch->loop_length = (((unsigned)block[2]*2) << 16) | ((unsigned)block[1] << 8) | block[0];
				ch->data += 3;
				ch->remaining_bytes -= block_size;
			}

			i = paintch(ch, (int16_t *)(end - i), i, 64);
		} while (i > 0);

		painted = MAX_SAMPLES - i;
	}
	else
	{
		S_PaintChannel8(ch, buffer, MAX_SAMPLES, 64);
		if (ch->position >= ch->length)
		{
			ch->data = NULL;
		}

		painted = MAX_SAMPLES;
	}

	return painted;
}

static void S_Update(int16_t* buffer)
{
	int i;
	int32_t *b2;
	int c, l, h;
	mobj_t* mo;
	int painted = 0;

	S_UpdatePCM();

	Mars_ClearCacheLine(&sfxvolume);
	Mars_ClearCacheLine(&musicvolume);
	Mars_ClearCacheLine(&sfxdriver);

	if (pcmchannel.data)
	{
		snd_nopaintcount = 0;
	}
	else
	{
		if (S_USE_MEGACD_DRV())
		{
			snd_nopaintcount++;
		}
		else
		{
			for (i = 0; i < SFXCHANNELS; i++)
			{
				if (sfxchannels[i].data)
					break;
			}
			if (i == SFXCHANNELS)
				snd_nopaintcount++;
			else
				snd_nopaintcount = 0;
		}
	}

	if (snd_nopaintcount > 2)
	{
		// we haven't painted the sound channels to the output buffer
		// for over two frames, both buffers are cleared and safe to
		// point the DMA to
		return;
	}

	b2 = (int32_t *)buffer;
	for (i = 0; i < MAX_SAMPLES; i++)
	{
		*b2++ = 0;
	}

	/* keep updating the channel until done */
	painted += S_PaintChannel(&pcmchannel, buffer, painted);

	if (!S_USE_MEGACD_DRV())
	{
		boolean spatialize = samplecount >= SPATIALIZATION_SRATE;

		if (spatialize)
		{
			for (i = 0; i < MAXPLAYERS; i++)
			{
				player_t* player = &players[i];
				if (!playeringame[i])
					continue;

				Mars_ClearCacheLine(&player->mo);

				mo = player->mo;
				Mars_ClearCacheLine(&mo->x);
				Mars_ClearCacheLine(&mo->y);
				Mars_ClearCacheLine(&mo->angle);
			}

			S_SpatializeAll();
		}

		for (i = 0; i < SFXCHANNELS; i++)
		{
			sfxchannel_t* ch = &sfxchannels[i];

			if (!ch->data)
				continue;

			painted += S_PaintChannel(ch, buffer, painted);
		}
	}

	if (samplecount >= SPATIALIZATION_SRATE)
		samplecount -= SPATIALIZATION_SRATE;
	samplecount += MAX_SAMPLES;

#ifdef MARS
	// force GCC into keeping constants in registers as it 
	// is stupid enough to reload them on each loop iteration
	__asm volatile("mov %1,%0\n\t" : "=&r" (c) : "r"(SAMPLE_CENTER));
	__asm volatile("mov %1,%0\n\t" : "=&r" (l) : "r"(SAMPLE_MIN));
	__asm volatile("mov %1,%0\n\t" : "=&r" (h) : "r"(SAMPLE_MAX));
#else
	c = SAMPLE_CENTER, l = SAMPLE_MIN, h = SAMPLE_MAX;
#endif

	// convert buffer from s16 pcm samples to u16 pwm samples
	b2 = (int32_t *)buffer;
	for (i = 0; i < MAX_SAMPLES; i++)
	{
		int s, s1, s2;
		s = (int16_t)(*b2 >> 16) + c;
		s1 = (s < l) ? l : (s > h) ? h : s;
		s = (int16_t)(*b2      ) + c;
		s2 = (s < l) ? l : (s > h) ? h : s;
		*b2++ = (s1 << 16) | s2;
	}
}

static void S_Sec_DMA1Handler(void)
{
	Mars_ClearCacheLine(&sfxdriver);

	if (snd_nopaintcount > 2 && S_USE_MEGACD_DRV())
	{
		snd_init = 0;
		Mars_RB_ResetRead(&soundcmds);
		return;
	}

	// start DMA on buffer and fill the other one
	SH2_DMA_SAR1 = (uintptr_t)snd_buffer[snd_bufidx];
	SH2_DMA_TCR1 = MAX_SAMPLES; // number longs
	SH2_DMA_CHCR1 = 0x18E5; // dest fixed, src incr, size long, ext req, dack mem to dev, dack hi, dack edge, dreq rising edge, cycle-steal, dual addr, intr enabled, clear TE, dma enabled

	snd_bufidx ^= 1; // flip audio buffer

	Mars_Sec_ReadSoundCmds();

	S_Update(snd_buffer[snd_bufidx]);
}

#endif

/*
==================
=
= S_AllocateChannel
=
==================
*/
static sfxchannel_t *S_AllocateChannel(mobj_t* mobj, unsigned sound_id, int vol, int freq, getsoundpos_t getpos)
{
	sfxchannel_t* channel, * newchannel;
	int i;
#ifndef DISABLE_DMA_SOUND
	int length;
#endif
	sfxinfo_t* sfx;
	sfx_t* md_data;

	newchannel = NULL;
	sfx = &S_sfx[sound_id];
	if (sfx->lump <= 0)
		return NULL;

	if (S_USE_MEGACD_DRV())
	{
		// dummy data to trick NULL pointer checks
		md_data = (void *)sfx;
	}
	else
	{
#ifndef DISABLE_DMA_SOUND
		md_data = W_POINTLUMPNUM(sfx->lump);
		length = md_data->samples;
		if (length < 4)
#endif
			return NULL;
	}

	/* reject sounds started at the same instant and singular sounds */
	for (channel = sfxchannels, i = 0; i < SFXCHANNELS; i++, channel++)
	{
		if (channel->sfx == sfx && channel->freq == freq)
		{
			if (channel->position <= 0) /* ADPCM has the position set to -1 initially */
			{
				if (channel->volume < vol)
				{
					newchannel = channel;
					goto gotchannel;	/* overlay this	*/
				}
				return NULL;		/* exact sound already started */
			}

			if (sfx->singularity)
			{
				newchannel = channel;	/* overlay this	*/
				goto gotchannel;
			}
		}
		if (mobj && channel->mobj == mobj)
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
		return NULL;		/* couldn't override a channel */
	}

	/* */
	/* fill in the new values */
	/* */
gotchannel:
	newchannel->freq = freq;
#ifndef DISABLE_DMA_SOUND
	newchannel->increment = (11025 << 14) / SAMPLE_RATE;
	newchannel->length = length << 14;
	newchannel->loop_length = 0;
	newchannel->width = 8;
#endif
	newchannel->data = (void *)md_data;
	newchannel->position = 0;

	newchannel->sfx = sfx;
	newchannel->mobj = mobj;
	newchannel->getpos = getpos;

	// volume and panning will be updated later in S_Spatialize
	newchannel->volume = vol;
	newchannel->pan = 128;

	return newchannel;
}

#ifndef DISABLE_DMA_SOUND

static void S_SetChannelData(sfxchannel_t* channel)
{
	int length;
	sfx_t* md_data;

	md_data = (void*)channel->data;
	length = md_data->samples;

	if (length != 0x52494646) { // RIFF
		channel->data = &md_data->data[0];
		return;
	}

	// find the format chunk
	uint16_t *chunk = (uint16_t *)((char *)md_data + 12);
	uint16_t *end = (uint16_t *)((char *)md_data + 0x40 - 4);
	int format = 0;

	// set default sample rate and block size
	channel->increment = (11025 << 14) / SAMPLE_RATE;
	channel->block_size = 256;

	while (chunk < end) {
		// a long value in little endian format
		length = S_LE_LONG(&chunk[2]);

		if (chunk[0] == 0x6461 && chunk[1] == 0x7461) // 'data'
			break;

		if (chunk[0] == 0x666D && chunk[1] == 0x7420) // 'fmt '
		{
			int sample_rate = S_LE_LONG(&chunk[6]);
			int block_align = S_LE_SHORT(&chunk[10]);

			format = S_LE_SHORT(&chunk[4]);
			if (format == S_WAV_FORMAT_EXTENSIBLE && length == 40) {
				format = S_LE_LONG(&chunk[16]); // sub-format
			}

			// increment = (SampleRate << 14) / mixer sample rate
			if (sample_rate > SAMPLE_RATE)
				channel->increment = 1 << 14; // limit increment to max of 1.0
			else
				channel->increment = (sample_rate << 14) / SAMPLE_RATE;
			channel->block_size = block_align;
		}

		chunk += 4 + ((length + 1) >> 1);
	}

	if (chunk >= end) {
		channel->data = NULL;
		return;
	}

	channel->data = (uint8_t *)&chunk[4];
	if (format == S_WAV_FORMAT_PCM) {
		channel->length = length << 14;
		channel->loop_length = 0;
		channel->width = 8;
		channel->position = 0;
	}
	else if (format == S_WAV_FORMAT_IMA_ADPCM) {
		channel->remaining_bytes = length;
		channel->length = 0;
		channel->loop_length = 0;
		channel->width = 4;
		channel->position = -1;
	}
	else {
		channel->data = NULL;
		return;
	}
}

void Mars_Sec_ReadSoundCmds(void)
{
	if (!snd_init)
		return;

	while (Mars_RB_Len(&soundcmds) >= 8) {
		short* p = Mars_RB_GetReadBuf(&soundcmds, 8);
		sfxchannel_t *ch = NULL;

		int cmd = *p;
		switch (cmd) {
		case SNDCMD_CLEAR:
			D_memset(sfxchannels, 0, sizeof(*sfxchannels) * SFXCHANNELS);
			snd_nopaintcount = 0;
			S_ClearPCM();
			break;
		case SNDCMD_STARTSND:
			ch = S_AllocateChannel((void*)(*(intptr_t*)&p[2]), p[1], p[4], 0, NULL);
			break;
		case SNDCMD_STARTORGSND:
			ch = S_AllocateChannel((void*)(*(intptr_t*)&p[2]), p[1], p[6], 0, (getsoundpos_t)(*(intptr_t*)&p[4]));
			break;
		}

		Mars_RB_CommitRead(&soundcmds);

		if (ch) {
			S_SetChannelData(ch);
		}
	}
}

#endif

void Mars_Sec_InitSoundDMA(int initfull)
{
	uint16_t sample, ix;

	Mars_ClearCache();

	if (initfull)
	{
#ifndef DISABLE_DMA_SOUND
		// init DMA
		SH2_DMA_SAR1 = 0;
		SH2_DMA_DAR1 = (uint32_t)&MARS_PWM_STEREO; // storing a long here will set the left and right channels
		SH2_DMA_TCR1 = 0;
		SH2_DMA_CHCR1 = 0;
		SH2_DMA_DRCR1 = 0;
#endif

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
	}

#ifndef DISABLE_DMA_SOUND
	Mars_RB_ResetRead(&soundcmds);

	Mars_SetSecDMA1Callback(&S_Sec_DMA1Handler);

	snd_bufidx = 0;

	snd_nopaintcount = 0;
	snd_init = 1;

	S_ClearPCM();

	// fill first buffer
	S_Update(snd_buffer[snd_bufidx]);

	// start DMA
	S_Sec_DMA1Handler();
#endif

	snd_init = 1;
}

static void S_Pri_CmdHandler(void)
{
#ifdef DISABLE_DMA_SOUND
	int __attribute__((aligned(16))) pcm_data[4];
#endif
	volatile int *pcm_cachethru = (volatile int *)((intptr_t)pcm_data | 0x20000000);
	unsigned int offs, len, freq;

	((volatile short *)pcm_cachethru)[7] = 0;	/* make sure data array isn't being read */

	// check comm0 for command
	if (MARS_SYS_COMM0 == 0xFFFF)
	{
		/* stop pcm channel */
		pcm_cachethru[2] = -1;
		pcm_cachethru[3] = 1;
	}
	else
	{
		freq = (unsigned)MARS_SYS_COMM0;

		/* top 8 bits of COMM2 */
		offs = ((unsigned)(MARS_SYS_COMM2 & 0x00FF) << 16);
		/* lower 8 bits of COMM2 */
		len = ((unsigned)(MARS_SYS_COMM2 & 0xFF00) << 8);

		/* handshake */
		for (MARS_SYS_COMM0 = 0; MARS_SYS_COMM0 == 0; );

		offs = offs | (unsigned)MARS_SYS_COMM2;
		/* COMM0 has the LSB set to 1 */ 
		len = len | ((unsigned)MARS_SYS_COMM0 >> 1);

		/* limit offset and length to 1M since that's our block size */
		offs &= 0xFFFFF;
		len &= 0xFFFFF;

		pcm_cachethru[0] = (freq << 14) / SAMPLE_RATE;
		pcm_cachethru[1] = len << 14;
		pcm_cachethru[2] = offs;
		pcm_cachethru[3] = 1;
	}

	Mars_ClearCacheLine(&snd_init);

	if (!snd_init)
	{
		Mars_InitSoundDMA(0);
	}
}
