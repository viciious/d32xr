#include "doomdef.h"

typedef struct
{
    VINT patch;
    VINT display;
    int32_t points;
} y_bonus_t;

typedef enum
{
	int_none,
	int_coop,     // Single Player/Cooperative
	int_spec,     // Special Stage
} inttype_e;

typedef union
{
	struct
	{
		uint32_t score; // fake score
		int32_t timebonus, ringbonus, perfbonus, total;
		VINT min, sec;
		int tics;
		boolean gotperfbonus; // true if we should show the perfect bonus line
		VINT ptimebonus; // TIME BONUS
		VINT pringbonus; // RING BONUS
		VINT pperfbonus; // PERFECT BONUS
		VINT ptotal; // TOTAL
		const char *passed1; // KNUCKLES GOT
		const char *passed2; // THROUGH THE ACT
		VINT passedx1, passedx2;
	} coop;

    struct
	{
        const char *passed1; // KNUCKLES GOT
        const char *passed2; // A CHAOS EMERALD? / GOT THEM ALL!
        const char *passed3; // CAN NOW BECOME
        const char *passed4; // SUPER KNUCKLES
        VINT passedx1;
        VINT passedx2;
        VINT passedx3;
        VINT passedx4;

        VINT emeraldbounces;
        VINT emeraldmomy;
        VINT emeraldy;

        y_bonus_t bonuses[2];

        VINT gotlife; // Number of extra lives obtained

        VINT pscore; // SCORE
        uint32_t score; // fake score
	} spec;
} y_data;

extern y_data data;
extern inttype_e intertype;
extern boolean stagefailed;

void Y_IntermissionDrawer();
void Y_Ticker();
void Y_StartIntermission();