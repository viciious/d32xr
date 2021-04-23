#ifndef __MUSIC_H__
#define __MUSIC_H__

#include "sound.h"

/* */
/* masks for the info field of sfx_t */
/* */

extern	sfx_t			*instruments[];
extern	int				pitch_bend[];
extern	int				pitch_bend_bits;

extern	channel_t		music_channels[];

extern	int				samplecount;
extern	int				musictime;
extern	int				next_eventtime;
extern	unsigned char	*music;
extern	unsigned char	*music_start;
extern	unsigned char	*music_end;

extern	int				samples_per_midiclock;

void M_PaintUnsatSound(int samples);
void M_CalculateEndTime(channel_t *channel, int t);
void M_RestoreFromOldChannels(void);
void M_WriteOutSamples(int samples);
void M_PaintMusic(void);
int M_GetEvent(void);

void output_delay(int delay);
void output_command(void);
void output_noteon(int midi_channel, int pitch, int velocity);
void output_noteoff(int midi_channel, int pitch);
void output_vol(int midi_channel, int volume);
void output_pan(int midi_channel, int pan);
void output_pitchbend(int midi_channel, int pitchbend);
void output_program(int midi_channel, int program);
void output_alloff(void);
void output_reset(void);
void output_tempo(int samples_per_midiclock);
void init_output(void);
void shutdown_output(void);

#endif
