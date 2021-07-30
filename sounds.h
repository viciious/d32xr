#ifndef __SOUNDSH__ 
#define __SOUNDSH__ 
 
#include "soundst.h" 
 
/* 
 *  Identifiers for all music in game. 
 */ 
enum
{
	cdtrack_none = -100,
	cdtrack_lastmap = -3,
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
 
extern musicinfo_t *S_music; 
extern const VINT mus_none;
extern VINT num_music;

extern sfxinfo_t   S_sfx[]; 
extern const char* const S_sfxnames[];

#endif 
 
