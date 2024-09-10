#ifndef __SOUNDSH__ 
#define __SOUNDSH__ 
 
#include "soundst.h" 
 
/* 
 *  Identifiers for all music in game. 
 */ 
enum
{
	cdtrack_none = 0
};

/* 
 *  Identifiers for all sfx in game. 
 */ 
 
typedef enum 
{ 
  sfx_None, 
  sfx_pistol, 
  sfx_shotgn, 
  sfx_sgcock, 
  sfx_dshtgn,
  sfx_dbopn,
  sfx_dbcls,
  sfx_dbload,
  sfx_plasma, 
  sfx_bfg, 
  sfx_sawup, 
  sfx_sawidl, 
  sfx_sawful, 
  sfx_sawhit, 
  sfx_rlaunc, 
  sfx_rfly, 
  sfx_rxplod, 
  sfx_firsht, 
  sfx_firbal, 
  sfx_firxpl, 
  sfx_pstart, 
  sfx_pstop, 
  sfx_doropn, 
  sfx_dorcls, 
  sfx_stnmov, 
  sfx_swtchn, 
  sfx_swtchx, 
  sfx_plpain, 
  sfx_dmpain, 
  sfx_popain, 
  sfx_mnpain,
  sfx_slop, 
  sfx_itemup, 
  sfx_wpnup, 
  sfx_oof, 
  sfx_telept, 
  sfx_posit1, 
  sfx_posit2, 
  sfx_posit3, 
  sfx_bgsit1, 
  sfx_bgsit2, 
  sfx_sgtsit, 
  sfx_cacsit, 
  sfx_brssit, 
  sfx_cybsit, 
  sfx_spisit, 
  sfx_bspsit,
  sfx_kntsit,
  sfx_mansit,
  sfx_sklatk, 
  sfx_sgtatk, 
  sfx_skepch, 
  sfx_claw, 
  sfx_skeswg, 
  sfx_pldeth, 
  sfx_podth1, 
  sfx_podth2, 
  sfx_podth3, 
  sfx_bgdth1, 
  sfx_bgdth2, 
  sfx_sgtdth, 
  sfx_cacdth, 
  sfx_skldth, 
  sfx_brsdth, 
  sfx_cybdth, 
  sfx_spidth, 
  sfx_bspdth,
  sfx_kntdth,
  sfx_skedth,
  sfx_posact, 
  sfx_bgact, 
  sfx_dmact, 
  sfx_bspact,
  sfx_bspwlk,
  sfx_noway, 
  sfx_barexp, 
  sfx_punch, 
  sfx_hoof, 
  sfx_metal, 
  sfx_itmbk,
  sfx_bdopn,
  sfx_bdcls,
  sfx_manatk,
  sfx_mandth,
  sfx_skeact,
  sfx_skesit,
  sfx_skeatk,
  sfx_bospn,
  sfx_bosdth,
  sfx_getpow,
  sfx_bospit,
  sfx_boscub,
  sfx_secret,
  NUMSFX 
} sfxenum_t; 
 
extern musicinfo_t *S_music; 
extern const VINT mus_none;
extern VINT num_music;

extern sfxinfo_t   S_sfx[]; 
extern const char* const S_sfxnames[];

#endif 
 
