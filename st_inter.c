#include "st_inter.h"


typedef union
{
	struct
	{
		uint32_t score; // fake score
		int32_t timebonus, ringbonus, perfbonus, total;
		VINT min, sec, tics;
		boolean gotperfbonus; // true if we should show the perfect bonus line
		VINT ttlnum; // act number being displayed
		VINT ptimebonus; // TIME BONUS
		VINT pringbonus; // RING BONUS
		VINT pperfbonus; // PERFECT BONUS
		VINT ptotal; // TOTAL
		char passed1[13]; // KNUCKLES GOT
		char passed2[16]; // THROUGH THE ACT
		VINT passedx1, passedx2;
		VINT gotlife; // Player # that got an extra life
	} coop;
} y_data;

static y_data data;

// graphics
static boolean gottimebonus;
static boolean gotemblem;

static int32_t intertic;
static int32_t endtic = -1;

static enum
{
	int_none,
	int_coop,     // Single Player/Cooperative
} inttype = int_none;
/*
//
// Y_IntermissionDrawer
//
// Called by D_Display. Nothing is modified here; all it does is draw.
// Neat concept, huh?
//
void Y_IntermissionDrawer(void)
{
	if (inttype == int_none)
		return;

	if (inttype == int_coop)
	{
		// draw score
		V_DrawScaledPatch(hudinfo[HUD_SCORE].x, hudinfo[HUD_SCORE].y, V_SNAPTOLEFT, sboscore);
		Y_DrawScaledNum(hudinfo[HUD_SCORENUM].x, hudinfo[HUD_SCORENUM].y, V_SNAPTOLEFT, data.coop.score);

		// draw time
		V_DrawScaledPatch(hudinfo[HUD_TIME].x, hudinfo[HUD_TIME].y, V_SNAPTOLEFT, sbotime);
		if (cv_timetic.value == 1)
			Y_DrawScaledNum(hudinfo[HUD_SECONDS].x, hudinfo[HUD_SECONDS].y, V_SNAPTOLEFT, data.coop.tics);
		else
		{
			if (data.coop.sec < 10)
				Y_DrawScaledNum(hudinfo[HUD_LOWSECONDS].x, hudinfo[HUD_LOWSECONDS].y, V_SNAPTOLEFT, 0);
			Y_DrawScaledNum(hudinfo[HUD_SECONDS].x, hudinfo[HUD_SECONDS].y, V_SNAPTOLEFT, data.coop.sec);
			Y_DrawScaledNum(hudinfo[HUD_MINUTES].x, hudinfo[HUD_MINUTES].y, V_SNAPTOLEFT, data.coop.min);
			V_DrawScaledPatch(hudinfo[HUD_TIMECOLON].x, hudinfo[HUD_TIMECOLON].y, V_SNAPTOLEFT, sbocolon);
			// we should show centiseconds on the intermission screen too, if the conditions are right.
			if (timeattacking || cv_timetic.value == 2)
			{
				INT32 tics = G_TicsToCentiseconds(data.coop.tics);
				if (tics < 10)
					Y_DrawScaledNum(hudinfo[HUD_LOWTICS].x, hudinfo[HUD_LOWTICS].y, V_SNAPTOLEFT, 0);
				V_DrawScaledPatch(hudinfo[HUD_TIMETICCOLON].x, hudinfo[HUD_TIMETICCOLON].y, V_SNAPTOLEFT, sbocolon);
				Y_DrawScaledNum(hudinfo[HUD_TICS].x, hudinfo[HUD_TICS].y, V_SNAPTOLEFT, tics);
			}
		}

		// draw the "got through act" lines and act number
		V_DrawLevelTitle(data.coop.passedx1, 49, 0, data.coop.passed1);
		V_DrawLevelTitle(data.coop.passedx2, 49+V_LevelNameHeight(data.coop.passed2)+2, 0, data.coop.passed2);

		if (mapheaderinfo[gamemap-1].actnum)
			V_DrawScaledPatch(244, 57, 0, data.coop.ttlnum);

		V_DrawScaledPatch(68, 84 + 3*SHORT(tallnum[0]->height)/2, 0, data.coop.ptimebonus);
		Y_DrawNum(BASEVIDWIDTH - 68, 85 + 3*SHORT(tallnum[0]->height)/2, data.coop.timebonus);

		V_DrawScaledPatch(68, 84 + 3*SHORT(tallnum[0]->height), 0, data.coop.pringbonus);
		Y_DrawNum(BASEVIDWIDTH - 68, 85 + 3*SHORT(tallnum[0]->height), data.coop.ringbonus);

		//PERFECT BONUS
		if (data.coop.gotperfbonus)
		{
			V_DrawScaledPatch(56, 84 + ((9*SHORT(tallnum[0]->height))+1)/2, 0, data.coop.pperfbonus);
			Y_DrawNum(BASEVIDWIDTH - 68, 85 + ((9*SHORT(tallnum[0]->height))+1)/2, data.coop.perfbonus);
		}

		V_DrawScaledPatch(88, 84 + 6*SHORT(tallnum[0]->height), 0, data.coop.ptotal);
		Y_DrawNum(BASEVIDWIDTH - 68, 85 + 6*SHORT(tallnum[0]->height), data.coop.total);

		if (gottimebonus && endtic != -1)
			V_DrawCenteredString(BASEVIDWIDTH/2, 136, V_YELLOWMAP, "GOT TIME BONUS EMBLEM!");
		if (gotemblem && !gottimebonus && endtic != -1)
			V_DrawCenteredString(BASEVIDWIDTH/2, 172, V_YELLOWMAP, "GOT PERFECT BONUS EMBLEM!");
	}
}

//
// Y_Ticker
//
// Manages fake score tally for single player end of act, and decides when intermission is over.
//
void Y_Ticker(void)
{
	if (inttype == int_none)
		return;

	// Check for pause or menu up in single player
	if (paused || (!netgame && menuactive && !demoplayback))
		return;

	intertic++;

	// single player is hardcoded to go away after awhile
	if (intertic == endtic)
	{
		Y_EndIntermission();
		Y_FollowIntermission();
		return;
	}

	if (endtic != -1)
		return; // tally is done

	if (inttype == int_coop) // coop or single player, normal level
	{
		if (!intertic) // first time only
			S_ChangeMusic(mus_lclear, false); // don't loop it

		if (intertic < TICRATE) // one second pause before tally begins
			return;

		if (data.coop.ringbonus || data.coop.timebonus || data.coop.perfbonus)
		{
			INT32 i;
			boolean skip = false;

			if (!(intertic & 1))
				S_StartSound(NULL, sfx_menu1); // tally sound effect

			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (players[i].cmd.buttons & BT_USE)
				{
					skip = true;
					break;
				}
			}

			// ring and time bonuses count down by 222 each tic
			if (data.coop.ringbonus)
			{
				data.coop.ringbonus -= 222;
				data.coop.total += 222;
				data.coop.score += 222;
				if (data.coop.ringbonus < 0 || skip == true) // went too far
				{
					data.coop.score += data.coop.ringbonus;
					data.coop.total += data.coop.ringbonus;
					data.coop.ringbonus = 0;

					if (skip == true && (data.coop.gotlife == consoleplayer || data.coop.gotlife == secondarydisplayplayer))
					{
						// lives are already added since tally is fake, but play the music
						if (mariomode)
							S_StartSound(NULL, sfx_marioa);
						else
						{
							S_StopMusic(); // otherwise it won't restart if this is done twice in a row
							S_ChangeMusic(mus_xtlife, false);
						}
					}
				}
			}
			if (data.coop.timebonus)
			{
				data.coop.timebonus -= 222;
				data.coop.total += 222;
				data.coop.score += 222;
				if (data.coop.timebonus < 0 || skip == true)
				{
					data.coop.score += data.coop.timebonus;
					data.coop.total += data.coop.timebonus;
					data.coop.timebonus = 0;
				}
			}
			if (data.coop.perfbonus)
			{
				data.coop.perfbonus -= 222;
				data.coop.total += 222;
				data.coop.score += 222;
				if (data.coop.perfbonus < 0 || skip == true)
				{
					data.coop.score += data.coop.perfbonus;
					data.coop.total += data.coop.perfbonus;
					data.coop.perfbonus = 0;
				}
			}
			if (!data.coop.timebonus && !data.coop.ringbonus && !data.coop.perfbonus)
			{
				endtic = intertic + 3*TICRATE; // 3 second pause after end of tally
				S_StartSound(NULL, sfx_chchng); // cha-ching!
			}

			if (data.coop.score % 50000 < 222) // just passed a 50000 point mark
			{
				// lives are already added since tally is fake, but play the music
				if (mariomode)
					S_StartSound(NULL, sfx_marioa);
				else
				{
					S_StopMusic(); // otherwise it won't restart if this is done twice in a row
					S_ChangeMusic(mus_xtlife, false);
				}
			}
		}
		else
		{
			endtic = intertic + 3*TICRATE; // 3 second pause after end of tally
			S_StartSound(NULL, sfx_chchng); // cha-ching!
		}
	}
}

//
// Y_StartIntermission
//
// Called by G_DoCompleted. Sets up data for intermission drawer/ticker.
//
void Y_StartIntermission(void)
{
	intertic = -1;

	inttype = int_coop;

	switch (inttype)
	{
		case int_coop: // coop or single player, normal level
		{
			gottimebonus = false;
			gotemblem = false;

			// award time and ring bonuses
			Y_AwardCoopBonuses();

			// setup time data
			data.coop.tics = players[consoleplayer].realtime; // used if cv_timetic is on
			data.coop.sec = players[consoleplayer].realtime / TICRATE;
			data.coop.min = data.coop.sec / 60;
			data.coop.sec %= 60;

			if ((!modifiedgame || savemoddata) && !multiplayer && !demoplayback)
			{
				if(timeattacking)
				{
					if ((players[consoleplayer].realtime < timedata[gamemap-1].time) ||
						(timedata[gamemap-1].time == 0))
						timedata[gamemap-1].time = players[consoleplayer].realtime;

					if (!savemoddata && !(grade & 512))
					{
						INT32 emblemcount = 0;
						INT32 i;

						if (M_GotLowEnoughTime(23*60))
						{
							gottimebonus = true;
							emblemlocations[MAXEMBLEMS-3].collected = true;
							gotemblem = true;
							grade |= 512;
						}

						for (i = 0; i < MAXEMBLEMS; i++)
						{
							if (emblemlocations[i].collected)
								emblemcount++;
						}

						if (emblemcount >= numemblems/2 && !(grade & 4)) // Got half of emblems
							grade |= 4;

						if (emblemcount >= numemblems/4 && !(grade & 16)) // NiGHTS
							grade |= 16;

						if (emblemcount == numemblems && !(grade & 8)) // Got ALL emblems!
							grade |= 8;
					}
					G_SaveGameData();
				}
				else if (gamemap == 40) // Cleared NAGZ
					grade |= 2048;
			}

			// get act number
			if (mapheaderinfo[prevmap].actnum)
				data.coop.ttlnum = W_CachePatchName(va("TTL%.2d", mapheaderinfo[prevmap].actnum),
					PU_STATIC);
			else
				data.coop.ttlnum = W_CachePatchName("TTL01", PU_STATIC);

			// get background patches
			widebgpatch = W_CachePatchName("INTERSCW", PU_STATIC);
			bgpatch = W_CachePatchName("INTERSCR", PU_STATIC);

			// grab an interscreen if appropriate
			if (mapheaderinfo[gamemap-1].interscreen[0] != '#')
			{
				interpic = W_CachePatchName(mapheaderinfo[gamemap-1].interscreen, PU_STATIC);
				useinterpic = true;
				usebuffer = false;
			}
			else
			{
				useinterpic = false;
				usebuffer = true;
			}
			usetile = false;

			// get single player specific patches
			data.coop.ptotal = W_CachePatchName("YTOTAL", PU_STATIC);
			data.coop.ptimebonus = W_CachePatchName("YTMBONUS", PU_STATIC);
			data.coop.pringbonus = W_CachePatchName("YRINGBNS", PU_STATIC);
			data.coop.pperfbonus = W_CachePatchName("YPFBONUS", PU_STATIC);

			// set up the "got through act" message according to skin name
			if (strlen(skins[players[consoleplayer].skin].name) <= 8)
			{
				snprintf(data.coop.passed1,
					sizeof data.coop.passed1, "%s GOT",
					skins[players[consoleplayer].skin].name);
				data.coop.passed1[sizeof data.coop.passed1 - 1] = '\0';
				if (mapheaderinfo[gamemap-1].actnum)
				{
					strcpy(data.coop.passed2, "THROUGH ACT");
					data.coop.passedx1 = 62 + (176 - V_LevelNameWidth(data.coop.passed1))/2;
					data.coop.passedx2 = 62 + (176 - V_LevelNameWidth(data.coop.passed2))/2;
				}
				else
				{
					strcpy(data.coop.passed2, "THROUGH THE ACT");
					data.coop.passedx1 = (BASEVIDWIDTH - V_LevelNameWidth(data.coop.passed1))/2;
					data.coop.passedx2 = (BASEVIDWIDTH - V_LevelNameWidth(data.coop.passed2))/2;
				}
				// The above value is not precalculated because it needs only be computed once
				// at the start of intermission, and precalculating it would preclude mods
				// changing the font to one of a slightly different width.
			}
			else
			{
				strcpy(data.coop.passed1, skins[players[consoleplayer].skin].name);
				data.coop.passedx1 = 62 + (176 - V_LevelNameWidth(data.coop.passed1))/2;
				if (mapheaderinfo[gamemap-1].actnum)
				{
					strcpy(data.coop.passed2, "PASSED ACT");
					data.coop.passedx2 = 62 + (176 - V_LevelNameWidth(data.coop.passed2))/2;
				}
				else
				{
					strcpy(data.coop.passed2, "PASSED THE ACT");
					data.coop.passedx2 = 62 + (240 - V_LevelNameWidth(data.coop.passed2))/2;
				}
			}
			break;
		}

		case int_none:
		default:
			break;
	}
}

//
// Y_AwardCoopBonuses
//
// Awards the time and ring bonuses.
//
static void Y_AwardCoopBonuses(void)
{
	int i;
	int sharedringtotal = 0;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		//for the sake of my sanity, let's get this out of the way first
		if (!playeringame[i])
			continue;

		sharedringtotal += players[i].health - 1;
	}

	// with that out of the way, go back to calculating bonuses like usual
	for (i = 0; i < MAXPLAYERS; i++)
	{
		int secs, bonus, oldscore;

		if (!playeringame[i])
			continue;

		// calculate time bonus
		secs = players[i].realtime / TICRATE;
		if (secs < 30)
			bonus = 50000;
		else if (secs < 45)
			bonus = 10000;
		else if (secs < 60)
			bonus = 5000;
		else if (secs < 90)
			bonus = 4000;
		else if (secs < 120)
			bonus = 3000;
		else if (secs < 180)
			bonus = 2000;
		else if (secs < 240)
			bonus = 1000;
		else if (secs < 300)
			bonus = 500;
		else
			bonus = 0;

		if (i == consoleplayer)
		{
			data.coop.timebonus = bonus;
			if (players[i].health)
				data.coop.ringbonus = (players[i].health-1) * 100;
			else
				data.coop.ringbonus = 0;
			data.coop.total = 0;
			data.coop.score = players[i].score;

			if (sharedringtotal && sharedringtotal >= nummaprings) //perfectionist!
			{
				data.coop.perfbonus = 50000;
				data.coop.gotperfbonus = true;
			}
			else
			{
				data.coop.perfbonus = 0;
				data.coop.gotperfbonus = false;
			}
		}

		oldscore = players[i].score;

		players[i].score += bonus;
		if (players[i].health)
			players[i].score += (players[i].health-1) * 100; // ring bonus

		//todo: more conditions where we shouldn't award a perfect bonus?
		if (sharedringtotal && sharedringtotal >= nummaprings)
		{
			players[i].score += 50000; //perfect bonus

			if ((!modifiedgame || savemoddata) && !(netgame || multiplayer) && !demoplayback)
			{
				if (!emblemlocations[MAXEMBLEMS-4].collected)
				{
					INT32 j;
					INT32 emblemcount = 0;
					emblemlocations[MAXEMBLEMS-4].collected = true;
					gotemblem = true;

					for (j = 0; j < MAXEMBLEMS; j++)
					{
						if (emblemlocations[j].collected)
							emblemcount++;
					}

					if (emblemcount >= numemblems/2 && !(grade & 4)) // Got half of emblems
						grade |= 4;

					if (emblemcount >= numemblems/4 && !(grade & 16)) // NiGHTS
						grade |= 16;

					if (emblemcount == numemblems && !(grade & 8)) // Got ALL emblems!
						grade |= 8;

					G_SaveGameData();
				}
			}
		}

		// grant extra lives right away since tally is faked
		P_GivePlayerLives(&players[i], (players[i].score/50000) - (oldscore/50000));

		if ((players[i].score/50000) - (oldscore/50000) > 0)
			data.coop.gotlife = i;
		else
			data.coop.gotlife = -1;
	}
}

//
// Y_DrawScaledNum
//
// Dumb display function for positive numbers.
// Like ST_DrawOverlayNum, but scales the start and isn't translucent.
//
static void Y_DrawScaledNum(INT32 x, INT32 y, INT32 flags, INT32 num)
{
	INT32 w = SHORT(tallnum[0]->width);

	// special case for 0
	if (!num)
	{
		V_DrawScaledPatch(x - w, y, flags, tallnum[0]);
		return;
	}

#ifdef PARANOIA
	if (num < 0)
		I_Error("Intermission drawer used negative number!");
#endif

	// draw the number
	while (num)
	{
		x -= w;
		V_DrawScaledPatch(x, y, flags, tallnum[num % 10]);
		num /= 10;
	}
}

//
// Y_EndIntermission
//
void Y_EndIntermission(void)
{
	Y_UnloadData();

	endtic = -1;
	inttype = int_none;
}

//
// Y_EndGame
//
// Why end the game?
// Because Y_FollowIntermission and F_EndCutscene would
// both do this exact same thing *in different ways* otherwise,
// which made it so that you could only unlock Ultimate mode
// if you had a cutscene after the final level and crap like that.
// This function simplifies it so only one place has to be updated
// when something new is added.
void Y_EndGame(void)
{
	if (nextmap == 1102-1) // end game with credits
		F_StartCredits();
	else if (nextmap == 1101-1) // end game with evaluation
		F_StartGameEvaluation();
    else
	    D_StartTitle();	// 1100 or competitive multiplayer, so go back to title screen.
}

//
// Y_FollowIntermission
//
static void Y_FollowIntermission(void)
{
	if (nextmap < 1100-1)
	{
		// normal level
		G_AfterIntermission();
		return;
	}

	Y_EndGame();
}

//
// Y_UnloadData
//
static void Y_UnloadData(void)
{
    // Nothing... for now
}
*/