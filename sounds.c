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
  "s3k_62",
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
  SOUND( false, 128 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
  SOUND( false, 64 ),
}; 
 
