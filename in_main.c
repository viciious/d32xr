
/* in_main.c -- intermission */
#include "doomdef.h"
#include "st_main.h"

extern int nextmap;
boolean earlyexit = false;

void IN_Start (void)
{
	earlyexit = false;

#ifndef MARS
	DoubleBufferSetup ();
#endif

	I_SetPalette(W_POINTLUMPNUM(W_GetNumForName("PLAYPALS")));

	S_StartSong(gameinfo.intermissionMus, 1, cdtrack_intermission);
}

void IN_Stop (void)
{	
	S_StopSong();
}

int IN_Ticker (void)
{
	int		buttons;
	int		oldbuttons;		
	int 		i;

	if (ticon < 5)
		return 0;		/* don't exit immediately */
	
	for (i=0 ; i<= (netgame > gt_single) ; i++)
	{
		buttons = ticbuttons[i];
		oldbuttons = oldticbuttons[i];
		
	/* exit menu if button press */
		if ( (buttons & BT_ATTACK) && !(oldbuttons & BT_ATTACK) )
		{
			earlyexit = true;
			return 1;		/* done with intermission */
		}
		if ( (buttons & BT_SPEED) && !(oldbuttons & BT_SPEED) )
		{
			earlyexit = true;
			return 1;		/* done with intermission */
		}
		if ( (buttons & BT_USE) && !(oldbuttons & BT_USE) )
		{
			earlyexit = true;
			return 1;		/* done with intermission */
		}
	}
	
	return 0;
}

void IN_Drawer (void)
{	
#ifdef MARS
	DrawTiledBackground();
#endif
}

