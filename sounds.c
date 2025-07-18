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
  "pistol",
  "shotgn",
  "sgcock",
  "dshtgn",
  "dbopn",
  "dbcls",
  "dbload",
  "plasma",
  "bfg",
  "sawup",
  "sawidl",
  "sawful",
  "sawhit",
  "rlaunc",
  "rfly",
  "rxplod",
  "firsht",
  "firbal",
  "firxpl",
  "pstart",
  "pstop",
  "doropn",
  "dorcls",
  "stnmov",
  "swtchn",
  "swtchx",
  "plpain",
  "dmpain",
  "popain",
  "mnpain",
  "slop",
  "itemup",
  "wpnup",
  "oof",
  "telept",
  "posit1",
  "posit2",
  "posit3",
  "bgsit1",
  "bgsit2",
  "sgtsit",
  "cacsit",
  "brssit",
  "cybsit",
  "spisit",
  "bspsit",
  "kntsit",
  "mansit",
  "sklatk",
  "sgtatk",
  "skepch",
  "claw",
  "skeswg", 
  "pldeth",
  "podth1",
  "podth2",
  "podth3",
  "bgdth1",
  "bgdth2",
  "sgtdth",
  "cacdth",
  "skldth",
  "brsdth",
  "cybdth",
  "spidth",
  "bspdth",
  "kntdth",
  "skedth",
  "posact",
  "bgact",
  "dmact",
  "bspact",
  "bspwlk",
  "noway",
  "barexp",
  "punch",
  "hoof",
  "metal",
  "itmbk",
  "bdopn",
  "bdcls",
  "manatk",
  "mandth",
  "skeact",
  "skesit",
  "skeatk",
  "bospn",
  "bosdth",
  "getpow",
  "bospit",
  "boscub",
  "secret",
  "keenpn",
  "keendt",
  "ppopai",
  "ppodth"
};

#ifdef MARS
#define SOUND(sing,name,pri) { sing, pri, -1 }
#else
#define SOUND(sing,name,pri) { sing, pri, -1, -1, NULL, NULL }
#endif

sfxinfo_t S_sfx[NUMSFX] =
{ 
  SOUND( 0, none, 0 ),
  SOUND( false, pistol, 64 ),
  SOUND( false, shotgn, 64 ),
  SOUND( false, sgcock, 64 ),
  SOUND( false, dshtgn, 64 ),
  SOUND( false, dbopn, 64 ),
  SOUND( false, dbcls, 64 ),
  SOUND( false, dbload, 64 ),
  SOUND( false, plasma, 64 ),
  SOUND( false, bfg, 64 ),
  SOUND( false, sawup, 64 ),
  SOUND( false, sawidl, 118 ),
  SOUND( true, sawful, 70 ),
  SOUND( false, sawhit, 70 ),
  SOUND( false, rlaunc, 70 ),
  SOUND( true, rfly, 70 ),
  SOUND( false, rxplod, 70 ),
  SOUND( false, firsht, 70 ),
  SOUND( false, firbal, 70 ),
  SOUND( false, firxpl, 70 ),
  SOUND( false, pstart, 70 ),
  SOUND( false, pstop, 70 ),
  SOUND( false, doropn, 100 ),
  SOUND( false, dorcls, 100 ),
  SOUND( false, stnmov, 119 ),
  SOUND( false, swtchn, 78 ),
  SOUND( false, swtchx, 78 ),
  SOUND( true, plpain, 96 ),
  SOUND( true, dmpain, 96 ),
  SOUND( false, popain, 96 ),
  SOUND( false, mnpain, 96 ),
  SOUND( true, slop, 78 ),
  SOUND( true, itemup, 78 ),
  SOUND( true, wpnup, 78 ),
  SOUND( false, oof, 96 ),
  SOUND( true, telept, 32 ),
  SOUND( false, posit1, 98 ),
  SOUND( false, posit2, 98 ),
  SOUND( false, posit3, 94 ),
  SOUND( false, bgsit1, 92 ),
  SOUND( false, bgsit2, 90 ),
  SOUND( false, sgtsit, 98 ),
  SOUND( false, cacsit, 98 ),
  SOUND( false, brssit, 94 ),
  SOUND( false, cybsit, 92 ),
  SOUND( false, spisit, 90 ),
  SOUND( false, bspsit, 90 ),
  SOUND( false, kntsit, 90 ),
  SOUND( false, mansit, 90 ),
  SOUND( false, sklatk, 70 ),
  SOUND( false, sgtatk, 70 ),
  SOUND( false, skepch, 70 ),
  SOUND( false, claw, 70 ),
  SOUND( false, skeswg, 70 ),
  SOUND( true, pldeth, 32 ),
  SOUND( false, podth1, 70 ),
  SOUND( false, podth2, 70 ),
  SOUND( false, podth3, 70 ),
  SOUND( false, bgdth1, 70 ),
  SOUND( false, bgdth2, 70 ),
  SOUND( false, sgtdth, 70 ),
  SOUND( false, cacdth, 70 ),
  SOUND( false, skldth, 70 ),
  SOUND( false, brsdth, 32 ),
  SOUND( false, cybdth, 32 ),
  SOUND( false, spidth, 32 ),
  SOUND( false, bspdth, 32 ),
  SOUND( false, kntdth, 32 ),
  SOUND( false, skedth, 32 ),
  SOUND( false, posact, 120 ),
  SOUND( false, bgact, 120 ),
  SOUND( false, dmact, 120 ),
  SOUND( false, bspact, 100 ),
  SOUND( false, bspwlk, 100 ),
  SOUND( false, noway, 78 ),
  SOUND( false, barexp, 60 ),
  SOUND( false, punch, 64 ),
  SOUND( false, hoof, 70 ),
  SOUND( false, metal, 70 ),
  SOUND( false, itmbk, 100 ),
  SOUND( false, bdopn, 100 ),
  SOUND( false, bdcls, 100 ),
  SOUND( false, manatk, 70 ),
  SOUND( false, mandth, 70 ),
  SOUND( false, skeact, 70 ),
  SOUND( false, skesit, 70 ),
  SOUND( false, skeatk, 70 ),
  SOUND( false, bospn, 70 ),
  SOUND( false, bosdth, 70 ),
  SOUND( false, getpow, 60 ),
  SOUND( false, bospit, 70 ),
  SOUND( false, boscub, 70 ),
  SOUND( false, secret, 60 ),
  SOUND( false, keenpn, 70 ),
  SOUND( false, keendt, 70 )
}; 
 
