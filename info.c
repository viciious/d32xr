#include "doomdef.h"

// These are used to track a global animation state for MF_RINGMOBJ
VINT ringmobjstates[NUMMOBJTYPES];
VINT ringmobjtics[NUMMOBJTYPES];

const char * const sprnames[NUMSPRITES] = {
"PLAY",
"BBLS",
"BOM1",
"BOM2",
"BOM3",
"BUBL",
"BUS1",
"BUS2",
"DRWN",
"EGGM",
"FISH",
"FL01",
"FL02",
"FL03",
"FL12",
"FWR1",
"FWR2",
"FWR3",
"GFZD",
"JETF",
"MISL",
"MSTV",
"POSS",
"RING",
"ROIA",
"RSPR",
"SCOR",
"SIGN",
"SPLH",
"SPRK",
"SPRR",
"SPRY",
"STPT",
"TOKE",
"TV1U",
"TVAR",
"TVAT",
"TVEL",
"TVFO",
"TVIV",
"TVRI",
"TVSS",
"YSPR",
"GFZF",
"GFZG",
"GFZR",
};

void A_Chase(mobj_t *actor);
void A_Scream(mobj_t *actor);
void A_XScream(mobj_t *actor);
void A_Fall(mobj_t *actor);
void A_Look(mobj_t *actor);
void A_Pain(mobj_t *actor);

#define STATE(sprite,frame,tics,action,nextstate) {sprite,frame,tics,nextstate,action}

const state_t	states[NUMSTATES] = {
STATE(SPR_PLAY,0,-1,NULL,S_NULL),	// S_NULL
STATE(SPR_PLAY,0,45,NULL,S_PLAY_TAP1),	// S_PLAY_STND
STATE(SPR_PLAY,1,4,NULL,S_PLAY_TAP2), // S_PLAY_TAP1
STATE(SPR_PLAY,2,4,NULL,S_PLAY_TAP1), // S_PLAY_TAP2
STATE(SPR_PLAY,3,1,NULL,S_PLAY_RUN2),	// S_PLAY_RUN1
STATE(SPR_PLAY,4,1,NULL,S_PLAY_RUN3),	// S_PLAY_RUN2
STATE(SPR_PLAY,5,1,NULL,S_PLAY_RUN4),	// S_PLAY_RUN3
STATE(SPR_PLAY,6,1,NULL,S_PLAY_RUN5),	// S_PLAY_RUN4
STATE(SPR_PLAY,7,1,NULL,S_PLAY_RUN6),	// S_PLAY_RUN5
STATE(SPR_PLAY,8,1,NULL,S_PLAY_RUN7),	// S_PLAY_RUN6
STATE(SPR_PLAY,9,1,NULL,S_PLAY_RUN8),	// S_PLAY_RUN7
STATE(SPR_PLAY,10,1,NULL,S_PLAY_RUN1),	// S_PLAY_RUN8
STATE(SPR_PLAY,11,1,NULL,S_PLAY_SPD2), // S_PLAY_SPD1
STATE(SPR_PLAY,12,1,NULL,S_PLAY_SPD3), // S_PLAY_SPD2
STATE(SPR_PLAY,13,1,NULL,S_PLAY_SPD4), // S_PLAY_SPD3
STATE(SPR_PLAY,14,1,NULL,S_PLAY_SPD1), // S_PLAY_SPD4
STATE(SPR_PLAY,15,350,NULL,S_PLAY_FALL1),	// S_PLAY_PAIN
STATE(SPR_PLAY,16,-1,A_Fall,S_NULL),	// S_PLAY_DIE
STATE(SPR_PLAY,17,-1,A_Fall,S_NULL),	// S_PLAY_DROWN
STATE(SPR_PLAY,18,1,NULL,S_PLAY_ATK2), // S_PLAY_ATK1
STATE(SPR_PLAY,19,1,NULL,S_PLAY_ATK3), // S_PLAY_ATK2
STATE(SPR_PLAY,20,1,NULL,S_PLAY_ATK4), // S_PLAY_ATK3
STATE(SPR_PLAY,21,1,NULL,S_PLAY_ATK5), // S_PLAY_ATK4
STATE(SPR_PLAY,22,1,NULL,S_PLAY_ATK1), // S_PLAY_ATK5
STATE(SPR_PLAY,23,1,NULL,S_PLAY_DASH2), // S_PLAY_DASH1
STATE(SPR_PLAY,24,1,NULL,S_PLAY_DASH3), // S_PLAY_DASH2
STATE(SPR_PLAY,25,1,NULL,S_PLAY_DASH4), // S_PLAY_DASH3
STATE(SPR_PLAY,26,1,NULL,S_PLAY_DASH1), // S_PLAY_DASH4
STATE(SPR_PLAY,27,14,NULL,S_PLAY_RUN1), // S_PLAY_GASP
STATE(SPR_PLAY,28,-1,NULL,S_PLAY_FALL1), // S_PLAY_SPRING
STATE(SPR_PLAY,29,2,NULL,S_PLAY_FALL2), // S_PLAY_FALL1
STATE(SPR_PLAY,30,2,NULL,S_PLAY_FALL1), // S_PLAY_FALL2
STATE(SPR_PLAY,31,12,NULL,S_PLAY_TEETER2), // S_PLAY_TEETER1
STATE(SPR_PLAY,32,12,NULL,S_PLAY_TEETER1), // S_PLAY_TEETER2
STATE(SPR_PLAY,33,-1,NULL,S_NULL), // S_PLAY_HANG
STATE(SPR_POSS,0,5,A_Look,S_POSS_STND2),	/* S_POSS_STND */
STATE(SPR_POSS,1,5,A_Look,S_POSS_STND),	/* S_POSS_STND2 */
STATE(SPR_POSS,0,2,A_Chase,S_POSS_RUN2),	/* S_POSS_RUN1 */
STATE(SPR_POSS,0,2,A_Chase,S_POSS_RUN3),	/* S_POSS_RUN2 */
STATE(SPR_POSS,1,2,A_Chase,S_POSS_RUN4),	/* S_POSS_RUN3 */
STATE(SPR_POSS,1,2,A_Chase,S_POSS_RUN5),	/* S_POSS_RUN4 */
STATE(SPR_POSS,2,2,A_Chase,S_POSS_RUN6),	/* S_POSS_RUN5 */
STATE(SPR_POSS,2,2,A_Chase,S_POSS_RUN7),	/* S_POSS_RUN6 */
STATE(SPR_POSS,3,2,A_Chase,S_POSS_RUN8),	/* S_POSS_RUN7 */
STATE(SPR_POSS,3,2,A_Chase,S_POSS_RUN1),	/* S_POSS_RUN8 */
STATE(SPR_RING,0,1,NULL,S_RING2), // S_RING1
STATE(SPR_RING,1,1,NULL,S_RING3), // S_RING2
STATE(SPR_RING,2,1,NULL,S_RING4), // S_RING3
STATE(SPR_RING,3,1,NULL,S_RING5), // S_RING4
STATE(SPR_RING,4,1,NULL,S_RING6), // S_RING5
STATE(SPR_RING,5,1,NULL,S_RING7), // S_RING6
STATE(SPR_RING,6,1,NULL,S_RING8), // S_RING7
STATE(SPR_RING,5|FF_FLIPPED,1,NULL,S_RING9), // S_RING8
STATE(SPR_RING,4|FF_FLIPPED,1,NULL,S_RING10), // S_RING9
STATE(SPR_RING,3|FF_FLIPPED,1,NULL,S_RING11), // S_RING10
STATE(SPR_RING,2|FF_FLIPPED,1,NULL,S_RING12), // S_RING11
STATE(SPR_RING,1|FF_FLIPPED,1,NULL,S_RING1), // S_RING12
STATE(SPR_SPRK,0,1,NULL,S_SPARK2), // S_SPARK1
STATE(SPR_SPRK,1,1,NULL,S_SPARK3), // S_SPARK2
STATE(SPR_SPRK,2,1,NULL,S_SPARK4), // S_SPARK3
STATE(SPR_SPRK,3,1,NULL,S_SPARK5), // S_SPARK4
STATE(SPR_SPRK,4,1,NULL,S_SPARK6), // S_SPARK5
STATE(SPR_SPRK,5,1,NULL,S_SPARK7), // S_SPARK6
STATE(SPR_SPRK,6,1,NULL,S_SPARK8), // S_SPARK7
STATE(SPR_SPRK,7,1,NULL,S_SPARK9), // S_SPARK8
STATE(SPR_SPRK,8,1,NULL,S_NULL), // S_SPARK9
STATE(SPR_FWR1,0,2,NULL,S_GFZFLOWERA2), // S_GFZFLOWERA1
STATE(SPR_FWR1,1,2,NULL,S_GFZFLOWERA3), // S_GFZFLOWERA2
STATE(SPR_FWR1,2,2,NULL,S_GFZFLOWERA4), // S_GFZFLOWERA3
STATE(SPR_FWR1,1,2,NULL,S_GFZFLOWERA5), // S_GFZFLOWERA4
STATE(SPR_FWR1,0,2,NULL,S_GFZFLOWERA6), // S_GFZFLOWERA5
STATE(SPR_FWR1,1|FF_FLIPPED,2,NULL,S_GFZFLOWERA7), // S_GFZFLOWERA6
STATE(SPR_FWR1,2|FF_FLIPPED,2,NULL,S_GFZFLOWERA8), // S_GFZFLOWERA7
STATE(SPR_FWR1,1|FF_FLIPPED,2,NULL,S_GFZFLOWERA1), // S_GFZFLOWERA8
STATE(SPR_FWR2,0,TICRATE,NULL,S_GFZFLOWERB2), // S_GFZFLOWERB1
STATE(SPR_FWR2,1,1,NULL,S_GFZFLOWERB3), // S_GFZFLOWERB2
STATE(SPR_FWR2,2,1,NULL,S_GFZFLOWERB4), // S_GFZFLOWERB3
STATE(SPR_FWR2,3,1,NULL,S_GFZFLOWERB5), // S_GFZFLOWERB4
STATE(SPR_FWR2,4,1,NULL,S_GFZFLOWERB6), // S_GFZFLOWERB5
STATE(SPR_FWR2,5,1,NULL,S_GFZFLOWERB7), // S_GFZFLOWERB6
STATE(SPR_FWR2,6,1,NULL,S_GFZFLOWERB8), // S_GFZFLOWERB7
STATE(SPR_FWR2,7,1,NULL,S_GFZFLOWERB9), // S_GFZFLOWERB8
STATE(SPR_FWR2,8,1,NULL,S_GFZFLOWERB10), // S_GFZFLOWERB9
STATE(SPR_FWR2,9,TICRATE,NULL,S_GFZFLOWERB11), // S_GFZFLOWERB10
STATE(SPR_FWR2,8,1,NULL,S_GFZFLOWERB12), // S_GFZFLOWERB11
STATE(SPR_FWR2,7,1,NULL,S_GFZFLOWERB13), // S_GFZFLOWERB12
STATE(SPR_FWR2,6,1,NULL,S_GFZFLOWERB14), // S_GFZFLOWERB13
STATE(SPR_FWR2,5,1,NULL,S_GFZFLOWERB15), // S_GFZFLOWERB14
STATE(SPR_FWR2,4,1,NULL,S_GFZFLOWERB16), // S_GFZFLOWERB15
STATE(SPR_FWR2,3,1,NULL,S_GFZFLOWERB17), // S_GFZFLOWERB16
STATE(SPR_FWR2,2,1,NULL,S_GFZFLOWERB18), // S_GFZFLOWERB17
STATE(SPR_FWR2,1,1,NULL,S_GFZFLOWERB1), // S_GFZFLOWERB18
STATE(SPR_FWR3,0,2,NULL,S_GFZFLOWERC2), // S_GFZFLOWERC1
STATE(SPR_FWR3,1,2,NULL,S_GFZFLOWERC3), // S_GFZFLOWERC2
STATE(SPR_FWR3,2,2,NULL,S_GFZFLOWERC4), // S_GFZFLOWERC3
STATE(SPR_FWR3,3,2,NULL,S_GFZFLOWERC5), // S_GFZFLOWERC4
STATE(SPR_FWR3,1,2,NULL,S_GFZFLOWERC6), // S_GFZFLOWERC5
STATE(SPR_FWR3,4,2,NULL,S_GFZFLOWERC7), // S_GFZFLOWERC6
STATE(SPR_FWR3,0|FF_FLIPPED,2,NULL,S_GFZFLOWERC8), // S_GFZFLOWERC7
STATE(SPR_FWR3,1|FF_FLIPPED,2,NULL,S_GFZFLOWERC9), // S_GFZFLOWERC8
STATE(SPR_FWR3,2|FF_FLIPPED,2,NULL,S_GFZFLOWERC10), // S_GFZFLOWERC9
STATE(SPR_FWR3,3|FF_FLIPPED,2,NULL,S_GFZFLOWERC11), // S_GFZFLOWERC10
STATE(SPR_FWR3,1|FF_FLIPPED,2,NULL,S_GFZFLOWERC12), // S_GFZFLOWERC11
STATE(SPR_FWR3,4|FF_FLIPPED,2,NULL,S_GFZFLOWERC1), // S_GFZFLOWERC12
STATE(SPR_BUS1,0,-1,NULL,S_NULL), // S_BERRYBUSH
STATE(SPR_BUS2,0,-1,NULL,S_NULL), // S_BUSH
STATE(SPR_BOM1,0,0,NULL,S_XPLD1), // S_XPLD_FLICKY
STATE(SPR_BOM1,0,1,NULL,S_XPLD2), // S_XPLD1
STATE(SPR_BOM1,1,1,NULL,S_XPLD3), // S_XPLD2
STATE(SPR_BOM1,2,1,NULL,S_XPLD4), // S_XPLD3
STATE(SPR_BOM1,3,1,NULL,S_XPLD5), // S_XPLD4
STATE(SPR_BOM1,4,1,NULL,S_XPLD6), // S_XPLD5
STATE(SPR_BOM1,5,1,NULL,S_NULL), // S_XPLD6
};

#undef STATE

const mobjinfo_t mobjinfo[NUMMOBJTYPES] = {

{		/* MT_PLAYER */
-1,		/* doomednum */
S_PLAY_STND,		/* spawnstate */
1,		/* spawnhealth */
S_PLAY_RUN1,		/* seestate */
sfx_None,		/* seesound */
0,		/* reactiontime */
sfx_None,		/* attacksound */
S_PLAY_PAIN,		/* painstate */
255,		/* painchance */
sfx_s3k_b9,		/* painsound */
S_NULL,		/* meleestate */
S_PLAY_ATK1,		/* missilestate */
S_PLAY_DIE,		/* deathstate */
S_PLAY_DIE,		/* xdeathstate */
sfx_None,		/* deathsound */
0,		/* speed */
16*FRACUNIT,		/* radius */
56*FRACUNIT,		/* height */
100,		/* mass */
0,		/* damage */
sfx_None,		/* activesound */
MF_SOLID|MF_SHOOTABLE		/* flags */
 },

 {		/* MT_CAMERA */
-1,		/* doomednum */
S_NULL,		/* spawnstate */
20,		/* spawnhealth */
S_NULL,		/* seestate */
sfx_None,		/* seesound */
8,		/* reactiontime */
sfx_None,		/* attacksound */
S_NULL,		/* painstate */
200,		/* painchance */
sfx_None,		/* painsound */
0,		/* meleestate */
S_NULL,		/* missilestate */
S_NULL,		/* deathstate */
S_NULL,		/* xdeathstate */
sfx_None,		/* deathsound */
8,		/* speed */
20*FRACUNIT,		/* radius */
16*FRACUNIT,		/* height */
100,		/* mass */
0,		/* damage */
sfx_None,		/* activesound */
0		/* flags */
 },

{		/* MT_POSSESSED */
100,		/* doomednum */
S_POSS_STND,		/* spawnstate */
1,		/* spawnhealth */
S_POSS_RUN1,		/* seestate */
sfx_None,		/* seesound */
16,		/* reactiontime */
sfx_None,		/* attacksound */
S_NULL,		/* painstate */
200,		/* painchance */
sfx_None,		/* painsound */
0,		/* meleestate */
S_NULL,		/* missilestate */
S_XPLD_FLICKY,		/* deathstate */
S_NULL,		/* xdeathstate */
sfx_s3k_3d,		/* deathsound */
2,		/* speed */
24*FRACUNIT,		/* radius */
32*FRACUNIT,		/* height */
100,		/* mass */
0,		/* damage */
sfx_None,		/* activesound */
MF_SHOOTABLE|MF_ENEMY		/* flags */
 },

{           // MT_RING
	300,            // doomednum
	S_RING1,         // spawnstate
	1000,           // spawnhealth
	S_NULL,         // seestate
	sfx_None,       // seesound
	0,//MT_FLINGRING,   // reactiontime
	sfx_None,       // attacksound
	S_NULL,         // painstate
	0,              // painchance
	sfx_None,       // painsound
	S_NULL,         // meleestate
	S_NULL,         // missilestate
	S_NULL,//S_SPRK1,        // deathstate
	S_NULL,         // xdeathstate
	sfx_None,     // deathsound
	38,    // speed
	16*FRACUNIT,    // radius
	24*FRACUNIT,    // height
	100,            // mass
	0,              // damage
	sfx_None,       // activesound
	MF_SPECIAL|MF_NOGRAVITY|MF_STATIC|MF_RINGMOBJ, // flags
},
{           // MT_SPARK
	-1,            // doomednum
	S_SPARK1,         // spawnstate
	1000,           // spawnhealth
	S_NULL,         // seestate
	sfx_None,       // seesound
	0,//MT_FLINGRING,   // reactiontime
	sfx_None,       // attacksound
	S_NULL,         // painstate
	0,              // painchance
	sfx_None,       // painsound
	S_NULL,         // meleestate
	S_NULL,         // missilestate
	S_NULL,//S_SPRK1,        // deathstate
	S_NULL,         // xdeathstate
	sfx_None,     // deathsound
	38,    // speed
	16*FRACUNIT,    // radius
	24*FRACUNIT,    // height
	100,            // mass
	0,              // damage
	sfx_None,       // activesound
	MF_NOGRAVITY|MF_STATIC|MF_NOBLOCKMAP, // flags
},
{           // MT_FLINGRING
	-1,            // doomednum
	S_RING1,         // spawnstate
	4*TICRATE,           // spawnhealth
	S_NULL,         // seestate
	sfx_None,       // seesound
	0,//MT_FLINGRING,   // reactiontime
	sfx_None,       // attacksound
	S_NULL,         // painstate
	0,              // painchance
	sfx_None,       // painsound
	S_NULL,         // meleestate
	S_NULL,         // missilestate
	S_NULL,//S_SPRK1,        // deathstate
	S_NULL,         // xdeathstate
	sfx_None,     // deathsound
	38,    // speed
	16*FRACUNIT,    // radius
	24*FRACUNIT,    // height
	100,            // mass
	0,              // damage
	sfx_None,       // activesound
	MF_FLOAT|MF_SPECIAL, // flags
},
{           // MT_GFZFLOWERA
	800,            // doomednum
	S_GFZFLOWERA1,         // spawnstate
	1000,           // spawnhealth
	S_NULL,         // seestate
	sfx_None,       // seesound
	0,//MT_FLINGRING,   // reactiontime
	sfx_None,       // attacksound
	S_NULL,         // painstate
	0,              // painchance
	sfx_None,       // painsound
	S_NULL,         // meleestate
	S_NULL,         // missilestate
	S_NULL,//S_SPRK1,        // deathstate
	S_NULL,         // xdeathstate
	sfx_None,     // deathsound
	38,    // speed
	16*FRACUNIT,    // radius
	40*FRACUNIT,    // height
	100,            // mass
	0,              // damage
	sfx_None,       // activesound
	MF_NOGRAVITY|MF_STATIC|MF_NOBLOCKMAP|MF_RINGMOBJ, // flags
},
{           // MT_GFZFLOWERB
	801,            // doomednum
	S_GFZFLOWERB1,         // spawnstate
	1000,           // spawnhealth
	S_NULL,         // seestate
	sfx_None,       // seesound
	0,//MT_FLINGRING,   // reactiontime
	sfx_None,       // attacksound
	S_NULL,         // painstate
	0,              // painchance
	sfx_None,       // painsound
	S_NULL,         // meleestate
	S_NULL,         // missilestate
	S_NULL,//S_SPRK1,        // deathstate
	S_NULL,         // xdeathstate
	sfx_None,     // deathsound
	38,    // speed
	16*FRACUNIT,    // radius
	96*FRACUNIT,    // height
	100,            // mass
	0,              // damage
	sfx_None,       // activesound
	MF_NOGRAVITY|MF_STATIC|MF_NOBLOCKMAP|MF_RINGMOBJ, // flags
},
{           // MT_GFZFLOWERC
	802,            // doomednum
	S_GFZFLOWERC1,         // spawnstate
	1000,           // spawnhealth
	S_NULL,         // seestate
	sfx_None,       // seesound
	0,//MT_FLINGRING,   // reactiontime
	sfx_None,       // attacksound
	S_NULL,         // painstate
	0,              // painchance
	sfx_None,       // painsound
	S_NULL,         // meleestate
	S_NULL,         // missilestate
	S_NULL,//S_SPRK1,        // deathstate
	S_NULL,         // xdeathstate
	sfx_None,     // deathsound
	38,    // speed
	8*FRACUNIT,    // radius
	32*FRACUNIT,    // height
	100,            // mass
	0,              // damage
	sfx_None,       // activesound
	MF_NOGRAVITY|MF_STATIC|MF_NOBLOCKMAP|MF_RINGMOBJ, // flags
},
{           // MT_BERRYBUSH
	804,            // doomednum
	S_BERRYBUSH,         // spawnstate
	1000,           // spawnhealth
	S_NULL,         // seestate
	sfx_None,       // seesound
	0,//MT_FLINGRING,   // reactiontime
	sfx_None,       // attacksound
	S_NULL,         // painstate
	0,              // painchance
	sfx_None,       // painsound
	S_NULL,         // meleestate
	S_NULL,         // missilestate
	S_NULL,//S_SPRK1,        // deathstate
	S_NULL,         // xdeathstate
	sfx_None,     // deathsound
	38,    // speed
	16*FRACUNIT,    // radius
	16*FRACUNIT,    // height
	100,            // mass
	0,              // damage
	sfx_None,       // activesound
	MF_NOGRAVITY|MF_STATIC|MF_NOBLOCKMAP|MF_RINGMOBJ, // flags
},
{           // MT_BUSH
	805,            // doomednum
	S_BUSH,         // spawnstate
	1000,           // spawnhealth
	S_NULL,         // seestate
	sfx_None,       // seesound
	0,//MT_FLINGRING,   // reactiontime
	sfx_None,       // attacksound
	S_NULL,         // painstate
	0,              // painchance
	sfx_None,       // painsound
	S_NULL,         // meleestate
	S_NULL,         // missilestate
	S_NULL,//S_SPRK1,        // deathstate
	S_NULL,         // xdeathstate
	sfx_None,     // deathsound
	38,    // speed
	16*FRACUNIT,    // radius
	16*FRACUNIT,    // height
	100,            // mass
	0,              // damage
	sfx_None,       // activesound
	MF_NOGRAVITY|MF_STATIC|MF_NOBLOCKMAP|MF_RINGMOBJ, // flags
},
};

