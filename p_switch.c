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

