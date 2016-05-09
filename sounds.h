#ifndef __SOUNDSH__ 
#define __SOUNDSH__ 
 
#include "soundst.h" 
 
/* 
 *  Identifiers for all music in game. 
 */ 
 
typedef enum 
{ 
  mus_None, 
  mus_e1m1,
  mus_e1m2,
  mus_e1m4,
  mus_e1m6,
  mus_e2m1,
  mus_e2m2,
  mus_e2m3,
  mus_e2m6,
  mus_e2m8,
  mus_e3m2,
  mus_intro,
  NUMMUSIC 
} musicenum_t; 
 
 
/* 
 *  Identifiers for all sfx in game. 
 */ 
 
typedef enum 
{ 
  sfx_None, 
  sfx_pistol, 
  sfx_shotgn, 
  sfx_sgcock, 
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
  sfx_sklatk, 
  sfx_sgtatk, 
  sfx_claw, 
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
  sfx_posact, 
  sfx_bgact, 
  sfx_dmact, 
  sfx_noway, 
  sfx_barexp, 
  sfx_punch, 
  sfx_hoof, 
  sfx_metal, 
  sfx_itmbk, 
  NUMSFX 
} sfxenum_t; 
 
extern musicinfo_t S_music[]; 
extern sfxinfo_t   S_sfx[]; 
 
#endif 
 
