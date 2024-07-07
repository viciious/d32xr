#include "doomdef.h" 
 
/* 
 *  Information about all the music 
 */ 
 
musicinfo_t		*S_music; 
VINT			num_music = 0;
const VINT		mus_none = 0;

/* 
 *  Information about all the sfx 
 */ 
const char * const S_sfxnames[NUMSFX] =
{
  0,
  "thok",
  "steam1",
  "steam2",
  "token",
  "trfire",
  "trpowr",
  "turhit",
  "wdjump",
  "wslap",
  "s3k_33",
  "s3k_35",
  "s3k_36",
  "s3k_37",
  "s3k_38",
  "s3k_39",
  "s3k_3a",
  "s3k_3c",
  "s3k_3d",
  "s3k_59",
  "s3k_5b",
  "s3k_62",
  "s3k_63",
  "s3k_65",
  "s3k_6a",
  "s3k_6c",
  "s3k_6e",
  "s3k_ab",
  "s3k_af",
  "s3k_b0",
  "s3k_b1",
  "s3k_b4",
  "s3k_b6",
  "s3k_b8",
  "s3k_b9",
};

#ifdef MARS
#define SOUND(sing,pri) { sing, pri, -1 }
#else
#define SOUND(sing,pri) { sing, pri, -1, -1, NULL, NULL }
#endif

sfxinfo_t S_sfx[NUMSFX] =
{ 
  SOUND( 0, 0 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
}; 
 
