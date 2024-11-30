
/* in_main.c -- intermission */
// This is now only used for special stage intermission
#include "doomdef.h"
#include "st_main.h"
#include "st_inter.h"
#include "v_font.h"

#define BASEVIDWIDTH 320
#define ALL7EMERALDS(x) (false)

boolean stagefailed = false;
VINT emeraldpics[1][8];

//
// Y_SetPerfectBonus
//
static void Y_SetPerfectBonus(y_bonus_t *bstruct)
{
	int i;

	D_memset(bstruct, 0, sizeof(y_bonus_t));
	bstruct->patch = W_GetNumForName("YB_PERFE");

	int sharedringtotal = 0;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;
			
		sharedringtotal += (players[i].health - 1);
	}
	if (!sharedringtotal || totalitems == 0 || sharedringtotal < totalitems)
		bstruct->display = false;
	else
	{
		bstruct->display = true;
		bstruct->points = 50000;
	}
}

static void Y_SetSpecialRingBonus(y_bonus_t *bstruct)
{
	int i, sharedringtotal = 0;

	bstruct->patch = W_GetNumForName("YB_RING");
	bstruct->display = true;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		sharedringtotal += (players[i].health - 1);
	}

	if (sharedringtotal > 0)
		bstruct->points = sharedringtotal * 100;
	else
		bstruct->points = 0;
}

//
// Y_SetNullBonus
// No bonus in this slot, so just clear the struct
//
static void Y_SetNullBonus(y_bonus_t *bstruct)
{
	D_memset(bstruct, 0, sizeof(y_bonus_t));
}

//
// Y_AwardSpecialStageBonus
//
// Gives a ring bonus only.
static void Y_AwardSpecialStageBonus(void)
{
	int i, oldscore, ptlives;
	y_bonus_t localbonuses[2];

	data.spec.score = players[consoleplayer].score;
	D_memset(data.spec.bonuses, 0, sizeof(data.spec.bonuses));

	for (i = 0; i < MAXPLAYERS; i++)
	{
		oldscore = players[i].score;

		if (!playeringame[i] || players[i].lives < 1) // not active, or game over
		{
			Y_SetNullBonus(&localbonuses[0]);
			Y_SetNullBonus(&localbonuses[1]);
		}
		else
		{
			Y_SetSpecialRingBonus(&localbonuses[0]);
			Y_SetPerfectBonus(&localbonuses[1]);
		}

		players[i].score += localbonuses[0].points;
		players[i].score += localbonuses[1].points;

		// grant extra lives right away since tally is faked
		ptlives = (players[i].score/50000) - (oldscore/50000);
		players[i].lives++;
		if (players[i].lives > 99)
			players[i].lives = 99;

		if (i == consoleplayer)
		{
			data.spec.gotlife = ptlives;
			D_memcpy(&data.spec.bonuses, &localbonuses, sizeof(data.spec.bonuses));
		}
	}
}

static VINT intertic = 0;
static VINT endtic = 0;
static VINT tallydonetic = -1;

void IN_Start (void)
{
	intertype = int_spec;

	Y_AwardSpecialStageBonus();

	// Super form stuff (normally blank)
	data.spec.passed3 = "";
	data.spec.passed4 = "";

	if (gamemapinfo.spheresNeeded > 0) // stagefailed
	{
		data.spec.passed2 = "Special Stage";
		data.spec.passed1 = "";
	}
	else if (ALL7EMERALDS(emeralds))
	{
		data.spec.passed1 = "SONIC";
		data.spec.passed2 = "got them all!";
		data.spec.passed3 = "can now become";
		data.spec.passed4 = "SUPER SONIC";
	}
	else
	{
		data.spec.passed1 = "SONIC got";
		data.spec.passed2 = "a Chaos Emerald";
	}

	data.spec.passedx1 = 160;
	data.spec.passedx2 = 160;
	data.spec.passedx3 = 160;
	data.spec.passedx4 = 160;

	intertic = 0;
	tallydonetic = -1;
	endtic = -1;

	emeraldpics[0][0] = W_GetNumForName("CHAOS1");
	emeraldpics[0][1] = W_GetNumForName("CHAOS2");
	emeraldpics[0][2] = W_GetNumForName("CHAOS3");
	emeraldpics[0][3] = W_GetNumForName("CHAOS4");
	emeraldpics[0][4] = W_GetNumForName("CHAOS5");
	emeraldpics[0][5] = W_GetNumForName("CHAOS6");
	emeraldpics[0][6] = W_GetNumForName("CHAOS7");
	emeraldpics[0][7] = emeraldpics[0][0]; // For completeness

	data.spec.pscore = W_GetNumForName("YB_SCORE");

#ifndef MARS
	DoubleBufferSetup ();
#endif

	const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
	I_SetPalette(dc_playpals+5*768); // Completely white

	S_StartSong(gameinfo.intermissionMus, 0, cdtrack_intermission);
}

void IN_Stop (void)
{	
	intertype = int_none;
}

// Returning a '1' means to finish
int IN_Ticker (void)
{
	intertic++;

	if (intertic == endtic)
	{
		gameaction = ga_completed;
		return 1;
	}

	if (endtic != -1)
		return 0; // tally is done

	if (intertype == int_spec) // coop or single player, special stage
	{
		int i;
		uint32_t oldscore = data.spec.score;
		boolean super = false, anybonuses = false;

		if (intertic==1) // first time only
		{
			tallydonetic = -1;
		}

		// emerald bounce
		if (intertic <= TICRATE)
		{
			data.spec.emeraldbounces = 0;
			data.spec.emeraldmomy = 20;
			data.spec.emeraldy = -40;
		}
		else// if (gamemapinfo.mapNumber - SSTAGE_START < 7)
		{
			if (!stagefailed)
			{
				if (data.spec.emeraldbounces < 3)
				{
					data.spec.emeraldy += (++data.spec.emeraldmomy);
					if (data.spec.emeraldy > 74)
					{
						S_StartSound(NULL, sfx_tink); // tink
						data.spec.emeraldbounces++;
						data.spec.emeraldmomy = -(data.spec.emeraldmomy/2);
						data.spec.emeraldy = 74;
					}
				}
			}
			else
			{
				data.spec.emeraldy += (++data.spec.emeraldmomy);

				if (data.spec.emeraldbounces < 1 && data.spec.emeraldy > 74)
				{
					S_StartSound(NULL, sfx_s3k_35); // nope
					data.spec.emeraldbounces++;
					data.spec.emeraldmomy = -(data.spec.emeraldmomy/2);
					data.spec.emeraldy = 74;
				}
			}
		}

		if (intertic < 2*TICRATE) // TWO second pause before tally begins, thank you mazmazz
			return 0;

		if (tallydonetic != -1 && (super && ALL7EMERALDS(emeralds)))
		{
			if ((intertic - tallydonetic) > (3*TICRATE)/2)
				endtic = intertic + 4*TICRATE; // 4 second pause after end of tally

			return 0;
		}

		// bonuses count down by 222 each tic
		for (i = 0; i < 2; ++i)
		{
			if (!data.spec.bonuses[i].points)
				continue;

			data.spec.bonuses[i].points -= 222;
			data.spec.score += 222;
			if (data.spec.bonuses[i].points < 0) // too far?
			{
				data.spec.score += data.spec.bonuses[i].points;
				data.spec.bonuses[i].points = 0;
			}

			if (data.spec.bonuses[i].points > 0)
				anybonuses = true;
		}

		if (!anybonuses)
		{
			tallydonetic = intertic;
			if (!(super && ALL7EMERALDS(emeralds))) // don't set endtic yet!
				endtic = intertic + 4*TICRATE; // 4 second pause after end of tally

			S_StartSound(NULL, sfx_s3k_b0); // cha-ching!

			// Update when done with tally
			if (!demoplayback)
			{
				//M_SilentUpdateUnlockablesAndEmblems(serverGamedata);

				//if (M_UpdateUnlockablesAndExtraEmblems(clientGamedata))
					//S_StartSound(NULL, sfx_s3k_68);

				//G_SaveGameData(clientGamedata);
			}
		}
		else if (!(intertic & 1))
			//S_StartSound(NULL, sfx_s3k_5b); // tally sound effect

		if (data.spec.gotlife > 0 && (data.spec.score % 50000 < oldscore % 50000)) // just passed a 50000 point mark
		{
			// lives are already added since tally is fake, but play the music
			S_StartSong(gameinfo.xtlifeMus, 0, cdtrack_xtlife);
			--data.spec.gotlife;
		}
	}

	return 0;
}

void IN_Drawer (void)
{
	if (intertic < TICRATE/2)
	{
		VINT palette = 5 - (intertic / 3);
		if (palette < 0)
			palette = 0;

		const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
		I_SetPalette(dc_playpals+palette*768); // Fade from white to normal
	}
	else if (endtic != -1 && endtic - intertic < TICRATE / 2)
	{
		VINT palette = 10 - (endtic - intertic) / 2;
			if (palette < 6)
				palette = 0;

		if (endtic <= intertic)
			palette = 10;

		const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
		I_SetPalette(dc_playpals+palette*768); // Fade from normal to black
	}
	else
	{
		const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
		I_SetPalette(dc_playpals); // Normal
	}

	DrawTiledBackground2(W_GetNumForName("SPECTILE"));

	if (intertype == int_spec)
	{
		static VINT animatetic = 0;
		VINT ttheight = 16;
		VINT xoffset1 = 0; // Line 1 x offset
		VINT xoffset2 = 0; // Line 2 x offset
		VINT xoffset3 = 0; // Line 3 x offset
		VINT xoffset4 = 0; // Line 4 x offset
		VINT xoffset5 = 0; // Line 5 x offset
		VINT xoffset6 = 0; // Line 6 x offset
		VINT drawsection = 0;

//		if (gottoken) // first to be behind everything else
//			Y_IntermissionTokenDrawer();

		// draw the header
		if (intertic <= 2*TICRATE)
			animatetic = 0;
		else if (!animatetic && data.spec.bonuses[0].points == 0 && data.spec.bonuses[1].points == 0 && data.spec.passed3[0] != '\0')
			animatetic = intertic + TICRATE;

		if (animatetic && (int)intertic >= animatetic)
		{
			const VINT scradjust = BASEVIDWIDTH>>3; // 40 for BASEVIDWIDTH
			VINT animatetimer = (intertic - animatetic);
			if (animatetimer <= 16)
			{
				xoffset1 = -(animatetimer      * scradjust);
				xoffset2 = -((animatetimer- 2) * scradjust);
				xoffset3 = -((animatetimer- 4) * scradjust);
				xoffset4 = -((animatetimer- 6) * scradjust);
				xoffset5 = -((animatetimer- 8) * scradjust);
				xoffset6 = -((animatetimer-10) * scradjust);
				if (xoffset2 > 0) xoffset2 = 0;
				if (xoffset3 > 0) xoffset3 = 0;
				if (xoffset4 > 0) xoffset4 = 0;
				if (xoffset5 > 0) xoffset5 = 0;
				if (xoffset6 > 0) xoffset6 = 0;
			}
			else if (animatetimer < 34)
			{
				drawsection = 1;
				xoffset1 = (24-animatetimer) * scradjust;
				xoffset2 = (26-animatetimer) * scradjust;
				xoffset3 = (28-animatetimer) * scradjust;
				xoffset4 = (30-animatetimer) * scradjust;
				xoffset5 = (32-animatetimer) * scradjust;
				xoffset6 = (34-animatetimer) * scradjust;
				if (xoffset1 < 0) xoffset1 = 0;
				if (xoffset2 < 0) xoffset2 = 0;
				if (xoffset3 < 0) xoffset3 = 0;
				if (xoffset4 < 0) xoffset4 = 0;
				if (xoffset5 < 0) xoffset5 = 0;
			}
			else
			{
				drawsection = 1;
				//if (animatetimer == 32)
					//S_StartSound(NULL, sfx_s3k_68);
			}
		}

		if (drawsection == 1)
		{
			const char *ringtext = "get 50 rings, then";
			const char *tut1text = "press B";
			const char *tut2text = "to transform";
			ttheight = 8;
			V_DrawStringLeft(&titleFont, data.spec.passedx1 + xoffset1, ttheight, data.spec.passed1);
			ttheight += 12;
			V_DrawStringLeft(&titleFont, data.spec.passedx3 + xoffset2, ttheight, data.spec.passed3);
			ttheight += 12;
			V_DrawStringLeft(&titleFont, data.spec.passedx4 + xoffset3, ttheight, data.spec.passed4);

			ttheight = 108;
			V_DrawStringCenter(&titleFont, BASEVIDWIDTH/2 + xoffset4 , ttheight, ringtext);
			ttheight += 12;
			V_DrawStringCenter(&titleFont, BASEVIDWIDTH/2 + xoffset5, ttheight, tut1text);
			ttheight += 12;
			V_DrawStringCenter(&titleFont, BASEVIDWIDTH/2 + xoffset6, ttheight, tut2text);
		}
		else
		{
			int yoffset = 0;

			if (data.spec.passed1[0] != '\0')
			{
				ttheight = 24;
				V_DrawStringCenter(&titleFont, data.spec.passedx1 + xoffset1, ttheight, data.spec.passed1);

				ttheight += 20;
				V_DrawStringCenter(&titleFont, data.spec.passedx2 + xoffset2, ttheight, data.spec.passed2);
			}
			else
			{
				ttheight = 24 + 12;
				V_DrawStringCenter(&titleFont, data.spec.passedx2 + xoffset1, ttheight, data.spec.passed2);
			}

			if (data.spec.bonuses[0].display)
			{
				int w;
				GetJagobjSize(data.spec.bonuses[0].patch, &w, NULL);
				DrawJagobjLump(data.spec.bonuses[0].patch, 152 - w + xoffset3, 108, NULL, NULL);
				V_DrawValuePaddedRight(&hudNumberFont, BASEVIDWIDTH + xoffset3 - 68, 109, data.spec.bonuses[0].points, 0);
			}

			if (data.spec.bonuses[1].display)
			{
				int w;
				GetJagobjSize(data.spec.bonuses[1].patch, &w, NULL);
				DrawJagobjLump(data.spec.bonuses[1].patch, 152 - w + xoffset4, 124, NULL, NULL);
				V_DrawValuePaddedRight(&hudNumberFont, BASEVIDWIDTH + xoffset4 - 68, 125, data.spec.bonuses[1].points, 0);
				yoffset = 16;
				// hack; pass the buck along...
				xoffset4 = xoffset5;
				xoffset5 = xoffset6;
			}

			int w;
			GetJagobjSize(data.spec.pscore, &w, NULL);
			DrawJagobjLump(data.spec.pscore, 152 - w + xoffset4, 124+yoffset, NULL, NULL);
			V_DrawValuePaddedRight(&hudNumberFont, BASEVIDWIDTH + xoffset4 - 68, 125+yoffset, data.spec.score, 0);
		}

		// draw the emeralds
		{
			boolean drawthistic = !(ALL7EMERALDS(emeralds) && (intertic & 1));
			VINT emeraldx = 152 - 3*28;
			VINT em = gamemapinfo.mapNumber - SSTAGE_START;

			if (em == 7)
			{
				if (!stagefailed)
				{
					fixed_t adjust = 2*finesine(((ANGLE_1 * (intertic + 1) >> 4) & FINEMASK));
					DrawJagobjLump(emeraldpics[0][em], 152, 74 - adjust, NULL, NULL);
				}
			}
			else if (em < 7)
			{
				if (drawthistic)
				{
					for (int i = 0; i < 7; ++i)
					{
						if ((i != em) && (emeralds & (1 << i)))
							DrawJagobjLump(emeraldpics[0][i], emeraldx, 74, NULL, NULL);
						emeraldx += 28;
					}
				}

				emeraldx = 152 + (em-3)*28;

				if (intertic > 1)
				{
					if (stagefailed && data.spec.emeraldy < 320+16)
						emeraldx += intertic - 6;

					if (drawthistic)
						DrawJagobjLump(emeraldpics[0][em], emeraldx, data.spec.emeraldy, NULL, NULL);
				}
			}
		}
	}
}

