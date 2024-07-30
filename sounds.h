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
  cdtrack_xtlife = -5,
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
  sfx_s3k_35,
  sfx_s3k_36,
  sfx_s3k_37,
  sfx_s3k_38,
  sfx_s3k_39,
  sfx_s3k_3a,
  sfx_s3k_3c,
  sfx_s3k_3d,
  sfx_s3k_59,
  sfx_s3k_5b,
  sfx_s3k_62,
  sfx_s3k_63,
  sfx_s3k_65,
  sfx_s3k_6a,
  sfx_s3k_6c,
  sfx_s3k_6e,
  sfx_s3k_ab,
  sfx_s3k_af,
  sfx_s3k_b0,
  sfx_s3k_b1,
  sfx_s3k_b4,
  sfx_s3k_b6,
  sfx_s3k_b8,
  sfx_s3k_b9,
  NUMSFX 
} sfxenum_t; 
 
extern musicinfo_t *S_music; 
extern const VINT mus_none;
extern VINT num_music;

extern sfxinfo_t   S_sfx[]; 
extern const char* const S_sfxnames[];

#endif 
 
