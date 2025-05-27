#include "doomdef.h"
#include "p_local.h"
#ifdef MARS
#include "mars.h"
#endif

int	playertics, thinkertics, sighttics, basetics, latetics;
int	tictics, drawtics;

boolean		gamepaused;
jagobj_t	*pausepic;
char		clearscreen = 0;
VINT        distortion_action = DISTORTION_NONE;

#if defined(PLAY_POS_DEMO) || defined(REC_POS_DEMO)
	fixed_t prev_rec_values[4];
#endif

unsigned char rec_prev_buttons = 0;
unsigned char rec_buttons = 0;
unsigned char rec_button_count = 0;
unsigned short rec_start_time = 0;

/*
===============================================================================

								THINKERS

All thinkers should be allocated by Z_Malloc so they can be operated on uniformly.  The actual
structures will vary in size, but the first element must be thinker_t.

Mobjs are similar to thinkers, but kept seperate for more optimal list
processing
===============================================================================
*/

thinker_t	thinkercap;	/* both the head and tail of the thinker list */
degenmobj_t		mobjhead;	/* head and tail of mobj list */
degenmobj_t		freemobjhead, freestaticmobjhead;	/* head and tail of free mobj list */
degenmobj_t		limbomobjhead;

sectorBBox_t sectorBBoxes;

scenerymobj_t *scenerymobjlist;
ringmobj_t *ringmobjlist;
VINT numscenerymobjs = 0;
VINT numringmobjs = 0;
VINT numstaticmobjs = 0;
VINT numregmobjs = 0;

//int			activethinkers;	/* debug count */
//int			activemobjs;	/* debug count */

/*
===============
=
= P_InitThinkers
=
===============
*/

void P_InitThinkers (void)
{
	thinkercap.prev = thinkercap.next  = &thinkercap;
	mobjhead.next = mobjhead.prev = (void *)&mobjhead;
	freemobjhead.next = freemobjhead.prev = (void *)&freemobjhead;
	freestaticmobjhead.next = freestaticmobjhead.prev = (void *)&freestaticmobjhead;
	limbomobjhead.next = limbomobjhead.prev = (void*)&limbomobjhead;
	scenerymobjlist = NULL;
	ringmobjlist = NULL;
	sectorBBoxes.next = sectorBBoxes.prev = (void *)&sectorBBoxes;
}


/*
===============
=
= P_AddThinker
=
= Adds a new thinker at the end of the list
=
===============
*/

void P_AddThinker (thinker_t *thinker)
{
	thinkercap.prev->next = thinker;
	thinker->next = &thinkercap;
	thinker->prev = thinkercap.prev;
	thinkercap.prev = thinker;	
}


/*
===============
=
= P_RemoveThinker
=
= Deallocation is lazy -- it will not actually be freed until its
= thinking turn comes up
=
===============
*/

void P_RemoveThinker (thinker_t *thinker)
{
	thinker->function = (think_t)-1;
}

/*
===============
=
= P_RunThinkers
=
===============
*/

void P_RunThinkers (void)
{
	thinker_t	*currentthinker;
	
	//activethinkers = 0;
	
	currentthinker = thinkercap.next;
	while (currentthinker != &thinkercap)
		{
		if (currentthinker->function == (think_t)-1)
		{	/* time to remove it */
			currentthinker->next->prev = currentthinker->prev;
			currentthinker->prev->next = currentthinker->next;
			Z_Free (currentthinker);
		}
		else
		{
			if (currentthinker->function)
			{
				currentthinker->function (currentthinker);
			}
			//activethinkers++;
		}
		currentthinker = currentthinker->next;
	}
}

/*============================================================================= */

/* 
=================== 
= 
= P_RunMobjBase  
=
= Run stuff that doesn't happen every tick
=================== 
*/ 

void	P_RunMobjBase (void)
{
#ifdef JAGUAR
	extern	int p_base_start;
	 
	DSPFunction (&p_base_start);
#else

	P_RunMobjBase2();
#endif
}

/*============================================================================= */

int playernum;

void G_DoReborn (int playernum); 
void G_DoLoadLevel (void);

gameaction_t RecordDemo();
gameaction_t PlayDemo();

/*
=================
=
= P_Ticker
=
=================
*/

int		ticphase;

#ifdef MARS
#define frtc I_GetFRTCounter()
#else
#define frtc samplecount
#endif

int P_Ticker (void)
{
	int		ticstart;
	player_t	*pl;

	if (!demoplayback) {
		players[0].buttons = Mars_ConvGamepadButtons(ticbuttons[consoleplayer]);
	}

	if (titlescreen)
	{
		int result = M_Ticker(); // Run menu logic.
		if (result)
			return result;
	}

	// This is needed so the fade isn't removed until the new world is drawn at least once
	if (gametic >= 2 && gametic < 10) {
		fadetime = 0;
	}

	while (!I_RefreshLatched () )
	;		/* wait for refresh to latch all needed data before */
			/* running the next tick */

#ifdef JAGAUR
	while (DSPRead (&dspfinished) != 0xdef6)
	;		/* wait for sound mixing to complete */
#endif

#ifdef MARS
    // bank-switch to the page with map data
    W_POINTLUMPNUM(gamemaplump);
#endif

	gameaction = ga_nothing;

/* */
/* do option screen processing */
/* */

	for (playernum = 0, pl = players; playernum < MAXPLAYERS; playernum++, pl++)
		if (playeringame[playernum])
			O_Control(pl);

	if (gameaction != ga_nothing)
		return gameaction;

	/* */
	/* run player actions */
	/* */
	if (gamepaused)
		return 0;

	playertics = 0;
	thinkertics = 0;
	ticstart = frtc;
	for (int skipCount = 0; skipCount < accum_time; skipCount++)
	{
		if (demoplayback) {
			players[0].buttons = Mars_ConvGamepadButtons(rec_buttons);
		}

		if (gameaction == ga_nothing) {
			if (demorecording) {
				gameaction = RecordDemo();
			}
			else if (demoplayback) {
				gameaction = PlayDemo();
			}
		}

		for (playernum = 0, pl = players; playernum < MAXPLAYERS; playernum++, pl++)
		{
			if (playeringame[playernum])
			{
				if (pl->playerstate == PST_REBORN)
					G_DoReborn(playernum);

				P_PlayerThink(pl);
			}
		}
		P_RunThinkers();

		{
	//		if (gametic != prevgametic)
			{
				// If we don't do this every tic, it seems sight checking is broken.
				// Is there a way we can do this infrequently? Even every half second would be fine.
				P_CheckSights();
//				sighttics = frtc - start;
			}

//			start = frtc;
			P_RunMobjBase();
//			basetics = frtc - start;

//			start = frtc;
			P_RunMobjLate();
//			latetics = frtc - start;

			P_UpdateSpecials();

			leveltime++;
		}

		if (skipCount == 0)
			tictics = frtc - ticstart;
	}

	ST_Ticker();			/* update status bar */

	return gameaction;		/* may have been set to ga_died, ga_completed, */
							/* or ga_secretexit */
}


gameaction_t RecordDemo()
{
	int buttons = players[0].buttons;
#ifdef REC_POS_DEMO
	if (((short *)demobuffer)[3] == -1) {
		// This is the first frame, so record the initial values in full.
		prev_rec_values[0] = players[0].mo->x;
		prev_rec_values[1] = players[0].mo->y;
		prev_rec_values[2] = players[0].mo->z;
		prev_rec_values[3] = players[0].mo->angle >> ANGLETOFINESHIFT;

		char *values_p = prev_rec_values;
		for (int i=0; i < 4; i++) {
			*demo_p++ = *values_p++;
			*demo_p++ = *values_p++;
			*demo_p++ = *values_p++;
			*demo_p++ = *values_p++;
		}

		((short *)demobuffer)[2] += 16; // 16 bytes written.
	}
	else {
		// Beyond the first frame, we record only the values that
		// have changed.
		unsigned char frame_bytes = 1; // At least one byte will be written.

		// Calculate the difference between values in the current
		// frame and previous frame.
		fixed_t delta[4];
		delta[0] = players[0].mo->x - prev_rec_values[0];
		delta[1] = players[0].mo->y - prev_rec_values[1];
		delta[2] = players[0].mo->z - prev_rec_values[2];
		delta[3] = (players[0].mo->angle >> ANGLETOFINESHIFT) - prev_rec_values[3];

		// Save the current frame's values in 'prev_rec_values' so
		// the next frame's comparisons can be done against the
		// current frame.
		prev_rec_values[0] = players[0].mo->x;
		prev_rec_values[1] = players[0].mo->y;
		prev_rec_values[2] = players[0].mo->z;
		prev_rec_values[3] = players[0].mo->angle >> ANGLETOFINESHIFT;

		unsigned char key = 0;

		// Record the values that have changed and the minimum number
		// of bytes needed to represent the deltas.
		for (int i=0; i < 4; i++) {
			key >>= 2;

			fixed_t d = delta[i];
			if (d != 0) {
				if (d <= 0x7F && d >= -0x80) {
					key |= 0x40; // Byte
					frame_bytes++;
				}
				else if (d <= 0x7FFF && d >= -0x8000) {
					key |= 0x80; // Short
					frame_bytes += 2;
				}
				else {
					key |= 0xC0; // Long
					frame_bytes += 4;
				}
			}
		}

		unsigned char *delta_bytes;
		unsigned char *prev_rec_values_bytes;

		*demo_p++ = key;

		// Based on the sizes put into the key, we record either the
		// deltas (in the case of char and short) or the full value.
		// Values with no difference will not be recorded.
		for (int i=0; i < 4; i++) {
			delta_bytes = &delta[i];
			prev_rec_values_bytes = &prev_rec_values[i];
			switch (key & 3) {
				case 3: // Long
					*demo_p++ = *prev_rec_values_bytes++;
					*demo_p++ = *prev_rec_values_bytes++;
					*demo_p++ = *prev_rec_values_bytes++;
					*demo_p++ = *prev_rec_values_bytes++;
					break;

				case 2: // Short
					*demo_p++ = delta_bytes[2];
					// fall-thru

				case 1: // Byte
					*demo_p++ = delta_bytes[3];
			}

			// Advance the key so the next two bits can be read to
			// check for updated values.
			key >>= 2;
		}

		((short *)demobuffer)[2] += frame_bytes;	// Increase data length.
	}

	((short *)demobuffer)[3] += 1;	// Increase frame count.
#endif

#ifdef REC_INPUT_DEMO
	if ((short)demo_p - (short)demobuffer >= (0x100 - 1)) {
		// The demo recording buffer has been filled up. End the recording.
		if (demobuffer[0xFE] == 0xFF) {
			// This byte is a count of 255. Set it to zero so the demo can be ended.
			demobuffer[0xFE] = 0;
		}
		demobuffer[0xFF] = 0x80;
		*((short *)&demobuffer[6]) = leveltime - rec_start_time;
		demorecording = false;
	}
	else if (leveltime - rec_start_time >= (30*TICRATE) || buttons & BT_START) {
		// The demo has been recording for 30 seconds, or START was pressed. End the recording.
		demo_p += 1;
		*demo_p++ = 0x80;
		*((short *)&demobuffer[6]) = leveltime - rec_start_time;
		demorecording = false;
	}
	else if (rec_start_time == 0x7FFF) {
		if (!(players[0].pflags & PF_CONTROLDISABLED)) {
			// Start recording!
			rec_start_time = leveltime;
			*demo_p++ = buttons;
			*demo_p = 1;
		}
	}
	else if (rec_prev_buttons == (buttons & 0xFF)) {
		// Same button combination as last frame. Increase the count.
		*demo_p += 1;
		if (*demo_p == 255) {
			// Count reached 255. Create an additional count and start it at zero.
			demo_p++;
			*demo_p = 0;
		}
	}
	else {
		// New button combination. Record the buttons with a count of 1.
		demo_p += 1;
		*demo_p++ = buttons;
		*demo_p = 1;
	}

	rec_prev_buttons = buttons;
#endif

	return ga_nothing;
}


gameaction_t PlayDemo()
{
	gameaction_t exit = ga_nothing;
	int buttons = players[0].buttons;
#ifdef PLAY_POS_DEMO
	if (demo_p == demobuffer + 0xA) {
		// This is the first frame, so grab the initial values.
		prev_rec_values[0] = players[0].mo->x;
		prev_rec_values[1] = players[0].mo->y;
		prev_rec_values[2] = players[0].mo->z;
		prev_rec_values[3] = players[0].mo->angle >> ANGLETOFINESHIFT;

		demo_p += 16;
	}
	else {
		// Beyond the first frame, we update only the values that
		// have changed.
		unsigned char key = *demo_p++;

		int rec_value;
		unsigned char *prev_rec_values_bytes;

		for (int i=0; i < 4; i++) {
			// Check to see which values have changed and save them
			// in 'prev_rec_values' so the next frame's comparisons
			// can be done against the current frame.
			prev_rec_values_bytes = &prev_rec_values[i];
			rec_value = 0;

			switch (key&3) {
				case 3: // Long -- update the value as recorded (i.e. no delta).
					rec_value = *demo_p++;
					rec_value <<= 8;
					rec_value |= *demo_p++;
					rec_value <<= 8;
					rec_value |= *demo_p++;
					rec_value <<= 8;
					rec_value |= *demo_p++;
					prev_rec_values[i] = rec_value;
					break;

				case 2: // Short -- add the difference to the current value.
					rec_value = *demo_p++;
					rec_value <<= 8;
					rec_value |= *demo_p++;
					prev_rec_values[i] += (signed short)rec_value;
					break;

				case 1: // Byte -- add the difference to the current value.
					rec_value = *demo_p++;
					prev_rec_values[i] += (signed char)rec_value;
			}

			// Advance the key so the next two bits can be read to
			// check for updates.
			key >>= 2;
		}

		// Update the player variables with the newly updated
		// frame values.
		players[0].mo->x = prev_rec_values[0];
		players[0].mo->y = prev_rec_values[1];
		players[0].mo->z = prev_rec_values[2];
		players[0].mo->angle = prev_rec_values[3] << ANGLETOFINESHIFT;
	}
#endif

#ifndef PLAY_POS_DEMO	// Input demo code should *always* be present if position demo code is left out.
	int rec_current_time = leveltime - rec_start_time;
	int rec_end_time = ((short *)demobuffer)[3];

	if (rec_current_time >= rec_end_time) {
		ticbuttons[consoleplayer] = players[0].buttons = 0;
		demoplayback = false;
		exit = ga_completed;
	}
	else if (rec_start_time == 0x7FFF) {
		if (!(players[0].pflags & PF_CONTROLDISABLED)) {
			// Start demo playback!
			rec_start_time = leveltime;
			rec_buttons = *demo_p;
			ticbuttons[consoleplayer] = players[0].buttons = Mars_ConvGamepadButtons(rec_buttons);
			demo_p++;
			rec_button_count = *demo_p;
		}
	}
	else {
		rec_button_count--;
		if (rec_button_count == 0) {
			// Count is zero. Check if the next byte is a button mask or an additional count.
			if (*demo_p == 0xFF) {
				demo_p++;
				if (*demo_p > 0) {
					// Read next byte as an additional count.
					rec_button_count = *demo_p;
				}
			}
			if (rec_button_count == 0) {
				// Count is still zero. Read the next byte as a button mask.
				demo_p++;
				rec_buttons = *demo_p;
				if (rec_buttons & BT_START) {
					ticbuttons[consoleplayer] = players[0].buttons = 0;
					demoplayback = false;
					exit = ga_completed;
				}
				ticbuttons[consoleplayer] = players[0].buttons = Mars_ConvGamepadButtons(rec_buttons);
				demo_p++;
				rec_button_count = *demo_p;
			}
		}
		else {
			// Count is not zero. Reuse the previous button mask.
			ticbuttons[consoleplayer] = players[0].buttons = Mars_ConvGamepadButtons(rec_buttons);
		}
	}
	
	if (rec_end_time - rec_current_time <= 30) {
		int palette = PALETTE_SHIFT_CLASSIC_FADE_TO_BLACK + (((30 - (rec_end_time - rec_current_time)) * 2) / 3);
		R_FadePalette(dc_playpals, palette, dc_cshift_playpals);
		if (exit == ga_nothing) {
			exit = ga_demoending;
		}
	}
#endif

	return exit;
}


/* 
============= 
= 
= DrawPlaque 
= 
============= 
*/ 
 
void DrawPlaque (jagobj_t *pl)
{
#ifndef MARS
	int			x,y,w;
	short		*sdest;
	byte		*bdest, *source;
	extern		int isrvmode;
	
	while ( !I_RefreshCompleted () )
	;

	bufferpage = (byte *)screens[!workpage];
	source = pl->data;
	
	if (isrvmode == 0xc1 + (3<<9) )
	{	/* 320 mode, stretch pixels */
		bdest = (byte *)bufferpage + 80*320 + (160 - pl->width);
		w = pl->width;
		for (y=0 ; y<pl->height ; y++)
		{
			for (x=0 ; x<w ; x++)
			{
				bdest[x*2] = bdest[x*2+1] = *source++;
			}
			bdest += 320;
		}
	}
	else
	{	/* 160 mode, draw directly */
		sdest = (short *)bufferpage + 80*160 + (80 - pl->width/2);
		w = pl->width;
		for (y=0 ; y<pl->height ; y++)
		{
			for (x=0 ; x<w ; x++)
			{
				sdest[x] = palette8[*source++];
			}
			sdest += 160;
		}
	}
#else
	clearscreen = 2;
#endif
}

/* 
============= 
= 
= DrawPlaque 
= 
============= 
*/ 
#ifndef MARS
void DrawSinglePlaque (jagobj_t *pl)
{
	int			x,y,w;
	byte		*bdest, *source;
	
	while ( !I_RefreshCompleted () )
	;

	bufferpage = (byte *)screens[!workpage];
	source = pl->data;
	
	bdest = (byte *)bufferpage + 80*320 + (160 - pl->width/2);
	w = pl->width;
	for (y=0 ; y<pl->height ; y++)
	{
		for (x=0 ; x<w ; x++)
			bdest[x] = *source++;
		bdest += 320;
	}
}
#endif


/* 
============= 
= 
= P_Drawer 
= 
= draw current display 
============= 
*/ 
 
 
void P_Drawer (void) 
{ 	
	static boolean o_wasactive = false;

#ifdef MARS
	extern	boolean	debugscreenactive;

	drawtics = frtc;

	if (!optionsMenuOn && o_wasactive)
		clearscreen = 2;

	if (distortion_action == DISTORTION_REMOVE) {
		// The other frame buffer has already been normalized.
		// Now normalize the current frame buffer.
		RemoveDistortionFilters();
		distortion_action = DISTORTION_NONE;
	}
	else if (distortion_action == DISTORTION_ADD) {
		ApplyHorizontalDistortionFilter(gametic << 1);
		distortion_action = DISTORTION_NONE;
	}

	if (clearscreen > 0) {
		I_ResetLineTable();

		if ((viewportWidth == 160 && lowResMode) || viewportWidth == 320)
			DrawTiledLetterbox();
		else
			DrawTiledBackground();
		
		if (clearscreen == 2 || optionsMenuOn)
			ST_ForceDraw();
		clearscreen--;
	}

	if (initmathtables)
	{
		R_InitMathTables();
		initmathtables--;
	}

	/* view the guy you are playing */
	R_RenderPlayerView(consoleplayer);
	/* view the other guy in split screen mode */
	if (splitscreen) {
		Mars_R_SecWait();
		R_RenderPlayerView(consoleplayer ^ 1);
	}

	ST_Drawer();

	Mars_R_SecWait();

	if (titlescreen)
		M_Drawer();	// Show title emblem and menus.
	if (optionsMenuOn)
		O_Drawer();	// Show options menu.

	o_wasactive = optionsMenuOn;

	drawtics = frtc - drawtics;

	if (debugscreenactive)
		I_DebugScreen();
#else
	if (optionsMenuOn)
	{
		O_Drawer ();
	}
	else if (gamepaused && refreshdrawn)
		DrawPlaque (pausepic);
	else
	{
#ifdef JAGUAR
		ST_Drawer();
#endif
		R_RenderPlayerView(consoleplayer);
		clearscreen = 0;
	}
	/* assume part of the refresh is now running parallel with main code */
#endif
}

void P_Start (void)
{
	/* load a level */
	G_DoLoadLevel();

#ifndef MARS
	S_RestartSounds ();
#endif
	optionsMenuOn = false;
	M_ClearRandom ();

	if (!demoplayback && !demorecording && !titlescreen) {
		if (!netgame || splitscreen) {
			P_RandomSeed(I_GetTime());
		}
	}

	if (demoplayback || demorecording) {
		rec_prev_buttons = 0;
		rec_buttons = 0;
		rec_button_count = 0;
		rec_start_time = 0x7FFF;

		players[0].pflags |= PF_CONTROLDISABLED;
		players[0].buttons = 0;
	}

	clearscreen = 2;
}

void P_Stop (void)
{
	M_Stop();
	Z_FreeTags (mainzone);
}

void P_Update (void)
{
	int ticratebak;

	ticratebak = ticsperframe;

	if (viewportWidth >= 320 && ticsperframe < 3)
		ticsperframe = 3;

	I_Update();

	ticsperframe = ticratebak;
}
