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
  "dsarmasg",
  "dsattrsg",
  "dscannon",
  "dsdwnind",
  "dselemsg",
  "dsforcsg",
  "dsfrcssg",
  "dslvpass",
  "dsngskid",
  "dsrlaunc",
  "dstelept",
  "dsthok",
  "dssteam1",
  "dssteam2",
  "dstoken",
  "dstrfire",
  "dstrpowr",
  "dsturhit",
  "dswdjump",
  "dswirlsg",
  "dswslap",
  "s3k_33",
  "s3k_35",
  "s3k_36",
  "s3k_37",
  "s3k_38",
  "s3k_39",
  "s3k_3a",
  "s3k_3b",
  "s3k_3c",
  "s3k_3d",
  "s3k_43",
  "s3k_4e",
  "s3k_59",
  "s3k_5b",
  "s3k_5f",
  "s3k_62",
  "s3k_63",
  "s3k_64",
  "s3k_65",
  "s3k_6a",
  "s3k_6c",
  "s3k_6e",
  "s3k_a9",
  "s3k_ab",
  "s3k_af",
  "s3k_b0",
  "s3k_b1",
  "s3k_b4",
  "s3k_b6",
  "s3k_b9",
  "drcymcr1",   // Drums: Cymbal crash 1
  "drkick1",    // Drums: Kick 1
  "drsnare1",   // Drums: Snare 1
  "drtom1",     // Drums: Tom 1
  "drtom2",     // Drums: Tom 2
  "drtom3",     // Drums: Tom 3
  "drtom4",     // Drums: Tom 4
  "drtom5",     // Drums: Tom 5
  "drtom6",     // Drums: Tom 6
  "drtimb1",    // Drums: Timbale 1
  "drconga1",   // Drums: Conga 1
  "drconga2",   // Drums: Conga 2
  "drbongo1",   // Drums: Bongo 1
  "drbongo2",   // Drums: Bongo 2
  "drkick2",
  "drsnare2",
  "drohat1",
  "drhit1",
  "drhit2"
};

const unsigned char drumsfxmap[] =
{
  sfx_None,     // 0
  sfx_drcymcr1, // 1
  sfx_drkick1,  // 2
  sfx_drsnare1, // 3
  sfx_drtom1,   // 4
  sfx_drtom2,   // 5
  sfx_drtom3,   // 6
  sfx_drtom4,   // 7
  sfx_drtom5,   // 8
  sfx_drtom6,   // 9
  sfx_drtimb1,  // 10
  sfx_drconga1, // 11
  sfx_drconga2, // 12
  sfx_drbongo1, // 13
  sfx_drbongo2, // 14
  sfx_drkick2,  // 15
  sfx_drsnare2, // 16
  sfx_ohat1,    // 17
  sfx_hit1,     // 18
  sfx_hit2,     // 19
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
  SOUND( false, 96 ),
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
  SOUND( false, 32 ), // sfx_s3k_33
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
  SOUND( false, 100 ),
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
 
