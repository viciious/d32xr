#include "doomdef.h"
#include "p_local.h"


/*
==============
=
= P_Telefrag
=
= Kill all monsters around the given spot
=
==============
*/

void P_Telefrag (mobj_t *thing, fixed_t x, fixed_t y)
{
	int		delta;
	int		size;
	mobj_t	*m;
	
	for (m=mobjhead.next ; m != (void *)&mobjhead ; m=m->next)
	{
		if (!(m->flags & MF_SHOOTABLE) )
			continue;		/* not shootable */
		size = mobjinfo[m->type].radius + mobjinfo[thing->type].radius + 4*FRACUNIT;
		delta = m->x - x;
		if (delta < - size || delta > size)
			continue;
		delta = m->y - y;
		if (delta < -size || delta > size)
			continue;
		P_DamageMobj (m, thing, thing, 10000);
		m->flags &= ~(MF_SOLID|MF_SHOOTABLE);
	}
}


/*================================================================== */
/* */
/*						TELEPORTATION */
/* */
/*================================================================== */
int	EV_Teleport( line_t *line,mobj_t *thing )
{
	return 0;
}

