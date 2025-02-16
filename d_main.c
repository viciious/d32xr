/* D_main.c  */
 
#include "doomdef.h"
#include "v_font.h"
#include "r_local.h"

#ifdef MARS
#include "marshw.h"
#endif

#include <string.h>

boolean		splitscreen = false;
VINT		controltype = 0;		/* determine settings for BT_* */

boolean		sky_md_layer = false;
boolean		sky_32x_layer = false;

boolean		extended_sky = false;

VINT			gamevbls;		/* may not really be vbls in multiplayer */
VINT			vblsinframe;		/* range from ticrate to ticrate*2 */

VINT		ticsperframe = MINTICSPERFRAME;

VINT		maxlevel;			/* highest level selectable in menu (1-25) */
jagobj_t	*backgroundpic;

int 		ticstart;

#ifdef PLAY_POS_DEMO
	int			realtic;
	fixed_t prev_rec_values[4];
#else 
#ifdef REC_POS_DEMO
	fixed_t prev_rec_values[4];
#endif
#endif

unsigned configuration[NUMCONTROLOPTIONS][3] =
{
#ifdef SHOW_DISCLAIMER
	{BT_SPIN, BT_JUMP, BT_SPIN},
#else
	{BT_FLIP, BT_JUMP, BT_SPIN},
#endif
};

/*============================================================================ */


#define WORDMASK	3

/*
====================
=
= D_memset
=
====================
*/

void D_memset (void *dest, int val, size_t count)
{
	byte	*p;
	int		*lp;

/* round up to nearest word */
	p = dest;
	while ((int)p & WORDMASK)
	{
		if (count-- == 0)
			return;
		*p++ = val;
	}
	
/* write 32 bytes at a time */
	lp = (int *)p;
	val = (val<<24) | (val<<16) | (val<<8) | val;
	while (count >= 32)
	{
		lp[0] = lp[1] = lp[2] = lp[3] = lp[4] = lp[5] = lp[6] = lp[7] = val;
		lp += 8;
		count -= 32;
	}
	
/* finish up */
	p = (byte *)lp;
	while (count > 0)
	{
		*p++ = val;
		count--;
	}
}

#ifdef MARS
void fast_memcpy(void *dest, const void *src, int count);
#endif

void D_memcpy (void *dest, const void *src, size_t count)
{
	byte	*d;
	const byte *s;

	if (dest == src)
		return;

#ifdef MARS
	if ( (((intptr_t)dest & 15) == ((intptr_t)src & 15)) && (count > 16)) {
		unsigned i;
		unsigned rem = ((intptr_t)dest & 15), wordbytes;

		d = (byte*)dest;
		s = (const byte*)src;
		for (i = 0; i < rem; i++)
			*d++ = *s++;

		wordbytes = (unsigned)(count - rem) >> 2;
		fast_memcpy(d, s, wordbytes);

		wordbytes <<= 2;
		d += wordbytes;
		s += wordbytes;

		for (i = rem + wordbytes; i < (unsigned)count; i++)
			*d++ = *s++;
		return;
	}
#endif

	d = (byte *)dest;
	s = (const byte *)src;
	while (count--)
		*d++ = *s++;
}


void D_strncpy (char *dest, const char *src, int maxcount)
{
	byte	*p1;
	const byte *p2;

	p1 = (byte *)dest;
	p2 = (const byte *)src;
	while (maxcount--)
		if (! (*p1++ = *p2++) )
			return;
}

int D_strncasecmp (const char *s1, const char *s2, int len)
{
	while (*s1 && *s2)
	{
		int c1 = *s1, c2 = *s2;

		if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
		if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';

		if (c1 != c2)
			return c1 - c2;

		s1++;
		s2++;

		if (!--len)
			return 0;
	}
	return *s1 - *s2;
}

// insertion sort
void D_isort(int* a, int len)
{
	int i, j;
	for (i = 1; i < len; i++)
	{
		int t = a[i];
		for (j = i - 1; j >= 0; j--)
		{
			if (a[j] <= t)
				break;
			a[j+1] = a[j];
		}
		a[j+1] = t;
	}
}



/*
===============
=
= M_Random
=
= Returns a 0-255 number
=
===============
*/

unsigned char rndtable[256] = {
	0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66 ,
	74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36 ,
	95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188 ,
	52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224 ,
	149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242 ,
	145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0 ,
	175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235 ,
	25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113 ,
	94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75 ,
	136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196 ,
	135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113 ,
	80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241 ,
	24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224 ,
	145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95 ,
	28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226 ,
	71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36 ,
	17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106 ,
	197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136 ,
	120, 163, 236, 249 
};
VINT	rndindex = 0;
VINT prndindex = 0;

int P_Random (void)
{
	prndindex = (prndindex+1)&0xff;
	return rndtable[prndindex];
}

fixed_t P_RandomFixed(void)
{
	return (P_Random() << 8 | P_Random());
}

int P_RandomKey(int max)
{
	return (P_RandomFixed() * max) >> FRACBITS;
}

int M_Random (void)
{
	rndindex = (rndindex+1)&0xff;
	return rndtable[rndindex];
}

void M_ClearRandom (void)
{
	rndindex = prndindex = 0;
}

void P_RandomSeed(int seed)
{
	prndindex = seed & 0xff;
}


void M_ClearBox (fixed_t *box)
{
	box[BOXTOP] = box[BOXRIGHT] = D_MININT;
	box[BOXBOTTOM] = box[BOXLEFT] = D_MAXINT;
}

void M_AddToBox (fixed_t *box, fixed_t x, fixed_t y)
{
	if (x<box[BOXLEFT])
		box[BOXLEFT] = x;
	else if (x>box[BOXRIGHT])
		box[BOXRIGHT] = x;
	if (y<box[BOXBOTTOM])
		box[BOXBOTTOM] = y;
	else if (y>box[BOXTOP])
		box[BOXTOP] = y;
}

  
/*=============================================================================  */
 
static inline unsigned LocalToNet (unsigned cmd)
{
	return cmd;
}

static inline unsigned NetToLocal (unsigned cmd)
{
	return cmd;
}

 
/*=============================================================================  */

VINT		accum_time;
VINT		frames_to_skip = 0;
VINT		ticrate = 4;
VINT		ticsinframe;	/* how many tics since last drawer */
int		ticon;
int		frameon;
int		ticbuttons[MAXPLAYERS];
int		oldticbuttons[MAXPLAYERS];
int		ticmousex[MAXPLAYERS], ticmousey[MAXPLAYERS];
int		ticrealbuttons, oldticrealbuttons;
boolean	mousepresent;

extern	VINT	lasttics;

consistencymobj_t	emptymobj;
 
/*
===============
=
= MiniLoop
=
===============
*/

int last_frt_count = 0;
int total_frt_count = 0;
boolean optionsMenuOn = false;
int Mars_FRTCounter2Msec(int c);

int MiniLoop ( void (*start)(void),  void (*stop)(void)
		,  int (*ticker)(void), void (*drawer)(void)
		,  void (*update)(void) )
{
	int		i;
	int		exit;
	int		buttons;
	int		mx, my;
	boolean firstdraw = true;

/* */
/* setup (cache graphics, etc) */
/* */
	start ();
	exit = 0;
	
	ticon = 0;
	frameon = 0;
	
	gametic = 0;
	leveltime = 0;

	gameaction = 0;
	gamevbls = 0;
	vblsinframe = 0;
	lasttics = 0;
	last_frt_count = 0;
	total_frt_count = 0;

	ticbuttons[0] = ticbuttons[1] = oldticbuttons[0] = oldticbuttons[1] = 0;
	ticmousex[0] = ticmousex[1] = ticmousey[0] = ticmousey[1] = 0;

	do
	{
		ticstart = I_GetFRTCounter();

/* */
/* adaptive timing based on previous frame */
/* */
		vblsinframe = TICVBLS;

		int frt_count = I_GetFRTCounter();

		if (last_frt_count == 0)
			last_frt_count = frt_count;

		accum_time = 0;
		// Frame skipping based on FRT count
		total_frt_count += Mars_FRTCounter2Msec(frt_count - last_frt_count);
		const int frametime = 1000/30;//I_IsPAL() ? 1000/25 : 1000/30;

		while (total_frt_count > frametime)
		{
			accum_time++;
			total_frt_count -= frametime;
		}

		last_frt_count = frt_count;

		if (optionsMenuOn || gamemapinfo.mapNumber == TITLE_MAP_NUMBER || leveltime < TICRATE / 4) // Don't include map loading times into frameskip calculation
		{
			accum_time = 1;
			total_frt_count = 0;
		}

		/* */
		/* get buttons for next tic */
		/* */
		oldticbuttons[0] = ticbuttons[0];
		oldticbuttons[1] = ticbuttons[1];
		oldticrealbuttons = ticrealbuttons;

		buttons = I_ReadControls();
		buttons |= I_ReadMouse(&mx, &my);
		if (demoplayback)
		{
			ticmousex[consoleplayer] = 0;
			ticmousey[consoleplayer] = 0;
		}
		else
		{
			ticmousex[consoleplayer] = mx;
			ticmousey[consoleplayer] = my;
		}

		ticbuttons[consoleplayer] = buttons;
		ticrealbuttons = buttons;

		if (demoplayback)
		{
	#ifndef MARS
			if (buttons & (BT_ATTACK|BT_SPEED|BT_USE) )
			{
				exit = ga_exitdemo;
				break;
			}
	#endif

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

	#ifndef PLAY_POS_DEMO
			if (gamemapinfo.mapNumber == TITLE_MAP_NUMBER) {
				// Rotate on the title screen.
				ticbuttons[consoleplayer] = buttons = 0;
				players[0].mo->angle += TITLE_ANGLE_INC;
			}
			else {
				// This is for reading conventional input-based demos.
				ticbuttons[consoleplayer] = buttons = *((long *)demobuffer);
				demobuffer += 4;
			}
			#endif
		}

		#ifdef PLAY_POS_DEMO
		if (demoplayback) {
			players[0].mo->momx = 0;
			players[0].mo->momy = 0;
			players[0].mo->momz = 0;
		}
		#endif

		gamevbls += vblsinframe;

		if (demorecording) {
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
			*((long *)demo_p) = buttons;
			demo_p += 4;
#endif
		}
			
		if ((demorecording || demoplayback) && (buttons & BT_PAUSE) )
			exit = ga_completed;

		if (gameaction == ga_warped || gameaction == ga_startnew)
		{
			exit = gameaction;	/* hack for NeXT level reloading and net error */
			break;
		}

		if (!firstdraw)
		{
			if (update)
				update();
		}
		firstdraw = false;

		S_PreUpdateSounds();

		gametic++;
		ticon++;
		exit = ticker();

		S_UpdateSounds();

		/* */
		/* sync up with the refresh */
		/* */
		while (!I_RefreshCompleted())
			;

		drawer();

		#ifdef PLAY_POS_DEMO
		if (leveltime > 30) {
			V_DrawValueCenter(&menuFont, 160, 40, I_GetTime() - realtic);
			V_DrawValueCenter(&menuFont, 160, 50, I_GetFRTCounter());
		}
		else {
			realtic = I_GetTime();
		}
		#endif

#if 0
while (!I_RefreshCompleted ())
;	/* DEBUG */
#endif

#ifdef JAGUAR
		while ( DSPRead(&dspfinished) != 0xdef6 )
		;
#endif
	} while (!exit);

	stop ();
	S_Clear ();
	
	for (i = 0; i < MAXPLAYERS; i++)
		players[i].mo = (mobj_t*)&emptymobj;	/* for net consistency checks */

	return exit;
} 
 


/*=============================================================================  */

void ClearEEProm (void);
void DrawSinglePlaque (jagobj_t *pl);

jagobj_t	*titlepic;

int TIC_Abortable (void)
{
	if (ticon >= gameinfo.titleTime)
		return 1;		/* go on to next demo */

	return 0;
}

/*============================================================================= */

#ifdef SHOW_DISCLAIMER
VINT disclaimerCount = 0;
int TIC_Disclaimer(void)
{
	if (++disclaimerCount > 300)
		return 1;

	if (disclaimerCount == 270)
	{
		// Set to totally black
		const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
		I_SetPalette(dc_playpals+10*768);
	}

	return 0;
}

void START_Disclaimer(void)
{
	int		i;

	for (i = 0; i < 2; i++)
	{
		I_ClearFrameBuffer();
		UpdateBuffer();
	}

	disclaimerCount = 0;

	UpdateBuffer();

	const uint8_t *dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));
	I_SetPalette(dc_playpals);

	S_StartSong(gameinfo.gameoverMus, 0, cdtrack_gameover);

	R_InitColormap();
}

void STOP_Disclaimer (void)
{
}

const unsigned char key[] = { 0x78, 0x11, 0xB6, 0x2A, 0x48, 0x6A, 0x30, 0xA7 };
const int keyValue = 3043;
void parse_data(unsigned char *data, size_t dataLen)
{
	size_t startPos = 0;
	size_t i;
	for (i = 0; i < dataLen; i++, startPos++)
		data[i] = data[i] ^ key[startPos & 7];
}

/*
==================
=
= BufferedDrawSprite
=
= Cache and draw a game sprite to the 8 bit buffered screen
==================
*/

void BufferedDrawSprite (int sprite, int frame, int rotation, int top, int left, boolean flip)
{
	spritedef_t	*sprdef;
	spriteframe_t	*sprframe;
	VINT 		*sprlump;
	patch_t		*patch;
	byte		*pixels, *src;
	int			x, sprleft, sprtop, spryscale;
	fixed_t 	spriscale;
	int			lump;
	int			texturecolumn;
	int			light = HWLIGHT(255);
	int 		height = I_FrameBufferHeight();

	if ((unsigned)sprite >= NUMSPRITES)
		I_Error ("BufferedDrawSprite: invalid sprite number %i "
		,sprite);
	sprdef = &sprites[sprite];
	if ( (frame&FF_FRAMEMASK) >= sprdef->numframes )
		I_Error ("BufferedDrawSprite: invalid sprite frame %i : %i "
		,sprite,frame);
	sprframe = &spriteframes[sprdef->firstframe + (frame & FF_FRAMEMASK)];
	sprlump = &spritelumps[sprframe->lump];

	if (sprlump[rotation] != -1)
		lump = sprlump[rotation];
	else
		lump = sprlump[0];

	if (lump < 0)
	{
		lump = -(lump + 1);
		flip = true;
	}

	if (lump <= 0)
		return;

	patch = (patch_t *)W_POINTLUMPNUM(lump);
	pixels = R_CheckPixels(lump + 1);
	 	
/* */
/* coordinates are in a 160*112 screen (doubled pixels) */
/* */
	sprtop = top;
	sprleft = left;
	spryscale = 1;
	spriscale = FRACUNIT/spryscale;

	sprtop -= patch->topoffset;
	sprleft -= patch->leftoffset;

/* */
/* draw it by hand */
/* */
	for (x=0 ; x<patch->width ; x++)
	{
		int 	colx;
		byte	*columnptr;

		if (sprleft+x < 0)
			continue;
		if (sprleft+x >= 320)
			break;

		if (flip)
			texturecolumn = patch->width-1-x;
		else
			texturecolumn = x;
			
		columnptr = (byte *)patch + BIGSHORT(patch->columnofs[texturecolumn]);

/* */
/* draw a masked column */
/* */
		for ( ; *columnptr != 0xff ; columnptr += sizeof(column_t)) 
		{
			column_t *column = (column_t *)columnptr;
			int top    = column->topdelta + sprtop;
			int bottom = top + column->length - 1;
			byte *dataofsofs = columnptr + offsetof(column_t, dataofs);
			int dataofs = (dataofsofs[0] << 8) | dataofsofs[1];

			top *= spryscale;
			bottom *= spryscale;
			src = pixels + dataofs;

			if (top < 0) top = 0;
			if (bottom >= height) bottom = height - 1;
			if (top > bottom) continue;

			colx = sprleft + x;
//			colx += colx;

			I_DrawColumn(colx, top, bottom, light, 0, spriscale, src, 128);
//			I_DrawColumn(colx+1, top, bottom, light, 0, spriscale, src, 128);
		}
	}
}

void DRAW_Disclaimer (void)
{
	unsigned char text1[] = { 0x2C, 0x59, 0xFF, 0x79, 0x68, 0x2D, 0x71, 0xEA, 0x3D, 0x31, 0xE5, 0x62, 0x07, 0x3F, 0x7C, 0xE3, 0x78};
	unsigned char text2[] = { 0x36, 0x5E, 0xE2, 0x0A, 0x0A, 0x2F, 0x10, 0xF4, 0x37, 0x5D, 0xF2, 0x2A};
	unsigned char text3[] = { 0x10, 0x65, 0xC2, 0x5A, 0x3B, 0x50, 0x1F, 0x88, 0x0B, 0x62, 0xD8, 0x5E, 0x29, 0x03, 0x5C, 0xD4, 0x56, 0x62, 0xC4, 0x48, 0x7A, 0x44, 0x5F, 0xD5, 0x1F, 0x3E, 0xC5, 0x58, 0x2A, 0x59, 0x02, 0xDF, 0x78 };
	const VINT stext1 = 16;
	const VINT stext2 = 11;
	const VINT stext3 = 32;

	int sum = 0;
	for (int i = 0; i < stext1; i++)
		sum += text1[i] / 2;
	for (int i = 0; i < stext2; i++)
		sum += text2[i] / 2;
	for (int i = 0; i < stext3; i++)
		sum += text3[i] / 2;

	if (sum != keyValue)
		I_Error("");

	parse_data(text1, stext1+1);
	parse_data(text2, stext2+1);
	parse_data(text3, stext3+1);

	DrawFillRect(0, 0, 320, viewportHeight, COLOR_BLACK);

	if (disclaimerCount < 240)
	{
		viewportbuffer = (pixel_t*)I_FrameBuffer();
		I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);
		BufferedDrawSprite(SPR_PLAY, 1 + ((disclaimerCount / 8) & 1), 0, 80, 160, false);
	}
	else if (disclaimerCount < 250)
	{
		DrawJagobjLump(W_GetNumForName("ZOOM"), 136, 80-56, NULL, NULL);
	}
	else
	{
		VINT Xpos = disclaimerCount - 250;
		viewportbuffer = (pixel_t*)I_FrameBuffer();
		I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);
		BufferedDrawSprite(SPR_PLAY, 11 + ((disclaimerCount / 2) % 4), 2, 80, 160  + (Xpos * 16), true);
	}

	V_DrawStringCenter(&creditFont, 160, 64+32, (const char*)text1);
	V_DrawStringCenter(&creditFont, 160, 88+32, (const char*)text2);

	V_DrawStringCenter(&menuFont, 160, 128+32, (const char*)text3);
	Mars_ClearCache();
}
#endif

/*============================================================================= */

void START_Title(void)
{
#ifdef MARS
	int		i;

	for (i = 0; i < 2; i++)
	{
		I_ClearFrameBuffer();
		UpdateBuffer();
	}
#else
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
	DoubleBufferSetup();
#endif

	titlepic = gameinfo.titlePage != -1 ? W_CacheLumpNum(gameinfo.titlePage, PU_STATIC) : NULL;

	ticon = 0;

#ifdef MARS
	I_InitMenuFire(titlepic);
#else
	S_StartSong(gameinfo.titleMus, 0, cdtrack_title);
#endif
}

void STOP_Title (void)
{
	if (titlepic != NULL)
		Z_Free (titlepic);
#ifdef MARS
	I_StopMenuFire();
#else
	S_StopSong();
#endif
}

void DRAW_Title (void)
{
#ifdef MARS
	I_DrawMenuFire();
#endif
}

/*============================================================================= */

void RunMenu (void);

void RunTitle (void)
{
	startmap = 1;
	starttype = gt_single;
	consoleplayer = 0;

#ifdef SHOW_DISCLAIMER
	MiniLoop (START_Disclaimer, STOP_Disclaimer, TIC_Disclaimer, DRAW_Disclaimer, UpdateBuffer);
#endif
	MiniLoop (START_Title, STOP_Title, TIC_Abortable, DRAW_Title, UpdateBuffer);
}

int RunInputDemo (char *demoname)
{
	unsigned char *demo;
	int	exit;
	int lump;

	lump = W_CheckNumForName(demoname);
	if (lump == -1)
		return ga_exitdemo;

	// avoid zone memory fragmentation which is due to happen
	// if the demo lump cache is tucked after the level zone.
	// this will cause shrinking of the zone area available
	// for the level data after each demo playback and eventual
	// Z_Malloc failure
	Z_FreeTags(mainzone);

	demo = W_CacheLumpNum(lump, PU_STATIC);
	exit = G_PlayInputDemoPtr (demo);
	Z_Free(demo);

#ifndef MARS
	if (exit == ga_exitdemo)
		RunMenu ();
#endif
	return exit;
}

int RunPositionDemo (char *demoname)
{
	unsigned *demo;
	int	exit;
	int lump;

	lump = W_CheckNumForName(demoname);
	if (lump == -1)
		return ga_exitdemo;

	// avoid zone memory fragmentation which is due to happen
	// if the demo lump cache is tucked after the level zone.
	// this will cause shrinking of the zone area available
	// for the level data after each demo playback and eventual
	// Z_Malloc failure
	Z_FreeTags(mainzone);

	demo = W_CacheLumpNum(lump, PU_STATIC);
	exit = G_PlayPositionDemoPtr ((unsigned char*)demo);
	Z_Free(demo);

#ifndef MARS
	if (exit == ga_exitdemo)
		RunMenu ();
#endif
	return exit;
}


void RunMenu (void)
{
#ifdef MARS
	int exit = ga_exitdemo;

	M_Start();
	if (!gameinfo.noAttractDemo) {
		do {
			int i;
			char demo[9];

			for (i = 1; i < 10; i++)
			{
				int lump;

				D_snprintf(demo, sizeof(demo), "DEMO%1d", i);
				lump = W_CheckNumForName(demo);
				if (lump == -1)
					break;

				exit = RunInputDemo(demo);
				if (exit == ga_exitdemo)
					break;
			}
		} while (exit != ga_exitdemo);
	}
	M_Stop();
#else
reselect:
	MiniLoop(M_Start, M_Stop, M_Ticker, M_Drawer, NULL);
#endif

	if (consoleplayer == 0)
	{
		if (starttype != gt_single && !startsplitscreen)
		{
			I_NetSetup();
#ifndef MARS
			if (starttype == gt_single)
				goto reselect;		/* aborted net startup */
#endif
		}
	}

	if (startsave != -1)
		G_LoadGame(startsave);
	else
		G_InitNew(startmap, starttype, startsplitscreen);

	G_RunGame ();
}

/*============================================================================ */


 
/* 
============= 
= 
= D_DoomMain 
= 
============= 
*/ 

VINT			startmap = 1;
gametype_t	starttype = gt_single;
VINT			startsave = -1;
boolean 	startsplitscreen = 0;

void D_DoomMain (void) 
{    
D_printf ("C_Init\n");
	C_Init ();		/* set up object list / etc	  */
D_printf ("Z_Init\n");
	Z_Init (); 
D_printf ("W_Init\n");
	W_Init ();
D_printf ("I_Init\n");
	I_Init (); 
D_printf ("R_Init\n");
	R_Init (); 
D_printf ("P_Init\n");
	P_Init (); 
D_printf ("S_Init\n");
	S_Init ();
D_printf("ST_Init\n");
	ST_Init ();
D_printf("O_Init\n");
	O_Init ();
D_printf("G_Init\n");
	G_Init();

/*========================================================================== */

D_printf ("DM_Main\n");

#ifdef PLAY_INPUT_DEMO
	while(1) {
		RunInputDemo("DEMO1");
	}
#endif
#ifdef PLAY_POS_DEMO
	while(1) {
		RunPositionDemo("DEMO9");
	}
#endif

#ifdef REC_INPUT_DEMO
	G_RecordInputDemo();	// set startmap and startskill
#endif
#ifdef REC_POS_DEMO
	G_RecordPositionDemo();
#endif

/*	MiniLoop (F_Start, F_Stop, F_Ticker, F_Drawer, UpdateBuffer); */

/*G_InitNew (startmap, gt_deathmatch, false); */
/*G_RunGame (); */

#ifdef NeXT
	if (M_CheckParm ("-dm") )
	{
		I_NetSetup ();
		G_InitNew (startmap, gt_deathmatch, false);
	}
	else if (M_CheckParm ("-dial") || M_CheckParm ("-answer") )
	{
		I_NetSetup ();
		G_InitNew (startmap, gt_coop, false);
	}
	else
		G_InitNew (startmap, gt_single, false);
	G_RunGame ();
#endif

#ifdef MARS
	while (1)
	{
		RunTitle();
		RunMenu();
	}
#else
	while (1)
	{
		RunTitle();
		RunInputDemo("DEMO1");
		RunCredits ();
		RunInputDemo("DEMO2");
	}
#endif
}
