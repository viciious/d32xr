#include "doomdef.h"
#include "p_local.h"

/*
==============================================================================
=
= P_UseSpecialLine
=
= Called when a thing uses a special line
= Only the front sides of lines are usable
===============================================================================
*/

boolean P_UseSpecialLine ( mobj_t *thing, line_t *line)
{		
	/* */
	/*	Switches that other things can activate */
	/* */
	if (!thing->player)
	{
		if (line->flags & ML_SECRET)
			return false;		/* never open secret doors */
		switch(line->special)
		{
			case 1:		/* MANUAL DOOR RAISE */
			case 32:	/* MANUAL BLUE */
			case 33:	/* MANUAL RED */
			case 34:	/* MANUAL YELLOW */
				break;
			default:
				return false;
		}
	}
	
	/* */
	/* do something */
	/*	 */
	switch (line->special)
	{
		/*=============================================== */
		/*	MANUALS */
		/*=============================================== */
		case 1:			/* Vertical Door */
		case 26:		/* Blue Card Door Raise */
		case 27:		/* Yellow Card Door Raise */
		case 28:		/* Red Card Door Raise */

		case 31:		/* Manual door open */
		case 32:		/* Blue Card door open */
		case 33:		/* Red Card door open */
		case 34:		/* Yellow Card door open */

		case 117:		/* Blazing door raise */
		case 118:		/* Blazing door open */
			EV_VerticalDoor (line, thing);
			break;
	}
	
	return true;
}

