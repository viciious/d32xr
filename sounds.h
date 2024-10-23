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
  cdtrack_sneakers = -6,
  cdtrack_invincibility = -7,
  cdtrack_drowning = -8,
  cdtrack_gameover = -9,
};

/*
 *  Identifiers for all sfx in game.
 */

typedef enum
{
  sfx_None,
  sfx_armasg,
  sfx_attrsg,
  sfx_cannon,
  sfx_dwnind,
  sfx_elemsg,
  sfx_forcsg,
  sfx_frcssg,
  sfx_lvpass,
  sfx_ngskid,
  sfx_rlaunc,
  sfx_telept,
  sfx_thok,
  sfx_steam1,
  sfx_steam2,
  sfx_token,
  sfx_trfire,
  sfx_trpowr,
  sfx_turhit,
  sfx_wdjump,
  sfx_wirlsg,
  sfx_wslap,
  sfx_s3k_33,
  sfx_s3k_35,
  sfx_s3k_36,
  sfx_s3k_37,
  sfx_s3k_38,
  sfx_s3k_39,
  sfx_s3k_3a,
  sfx_s3k_3b,
  sfx_s3k_3c,
  sfx_s3k_3d,
  sfx_s3k_43,
  sfx_s3k_4e,
  sfx_s3k_59,
  sfx_s3k_5b,
  sfx_s3k_5f,
  sfx_s3k_62,
  sfx_s3k_63,
  sfx_s3k_64,
  sfx_s3k_65,
  sfx_s3k_6a,
  sfx_s3k_6c,
  sfx_s3k_6d,
  sfx_s3k_6e,
  sfx_s3k_a9,
  sfx_s3k_ab,
  sfx_s3k_af,
  sfx_s3k_b0,
  sfx_s3k_b1,
  sfx_s3k_b4,
  sfx_s3k_b6,
  sfx_s3k_b9,
  sfx_drcymcr1,
  sfx_drkick1,
  sfx_drsnare1,
  sfx_drtom1,
  sfx_drtom2,
  sfx_drtom3,
  sfx_drtom4,
  sfx_drtom5,
  sfx_drtom6,
  sfx_drtimb1,
  sfx_drconga1,
  sfx_drconga2,
  sfx_drbongo1,
  sfx_drbongo2,
  sfx_drkick2,
  sfx_drsnare2,
  sfx_drohat1,
  sfx_drhit1,
  sfx_drhit2,
  NUMSFX
} sfxenum_t;

extern const unsigned char drumsfxmap[];

extern musicinfo_t *S_music;
extern const VINT mus_none;
extern VINT num_music;

extern sfxinfo_t   S_sfx[];
extern const char* const S_sfxnames[];

#endif
