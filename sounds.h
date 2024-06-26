#ifndef __SOUNDSH__ 
#define __SOUNDSH__ 
 
#include "soundst.h" 
 
/* 
 *  Identifiers for all music in game. 
 */ 
enum
{
	cdtrack_none = -100,
	cdtrack_lastmap = -4,
	cdtrack_end = -3,
	cdtrack_victory = -2,
	cdtrack_intermission = -1,
	cdtrack_title = 0,
};

/* 
 *  Identifiers for all sfx in game. 
 */ 
 
typedef enum 
{ 
  sfx_None, 
  sfx_thok,
  sfx_steam1,
  sfx_steam2,
  sfx_token,
  sfx_trfire,
  sfx_trpowr,
  sfx_turhit,
  sfx_wdjump,
  sfx_wslap,
  sfx_s3k_33,
  sfx_s3k_62,
  NUMSFX 
} sfxenum_t; 
 
extern musicinfo_t *S_music; 
extern const VINT mus_none;
extern VINT num_music;

extern sfxinfo_t   S_sfx[]; 
extern const char* const S_sfxnames[];

#endif 
 
