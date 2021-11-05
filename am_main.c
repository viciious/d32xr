/* am_main.c -- automap */
/* am_main.c -- automap */

#include "doomdef.h"
#include "p_local.h"

#define	STEPVALUE	0x800000

#ifdef MARS

#define CRY_RED 	176
#define CRY_BLUE 	199
#define CRY_GREEN 	121
#define CRY_BROWN 	73
#define CRY_YELLOW 	160
#define CRY_GREY 	96
#define CRY_AQUA 	250

#else

#define CRY_RED		0xd260
#define CRY_BLUE	0x3080
#define CRY_GREEN	0x6f80
#define CRY_BROWN	0xba20
#define CRY_YELLOW	0xff80
#define CRY_GREY	0x9826
#define CRY_AQUA	0x6aa0

#endif

static	VINT	blink = 0;
static	VINT	pause;
#define MAXSCALES	5
static	VINT	scale;
static	VINT	scalex[MAXSCALES] = {18,19,20,21,22};
static	VINT	scaley[MAXSCALES] = {17,18,19,20,21};
static	VINT	amcurmap = -1;
#define NOSELENGTH	0x200000		/* PLAYER'S TRIANGLE */
#define MOBJLENGTH	0x100000
		
static	fixed_t	oldplayerx;
static	fixed_t oldplayery;

#ifndef MARS
int	blockx;
int	blocky;
#endif

#ifdef JAGUAR
/* CHEATING STUFF */
typedef enum
{
	ch_allmap,
	ch_things,
	ch_maxcheats
} cheat_e;

const char cheatstrings[][11] =	/* order should mirror cheat_e */
{
	"8002545465",		/* allmap cheat */
	"8005778788"		/* show things cheat */
};

char currentcheat[11]="0000000000";
int	showAllThings;		/* CHEAT VARS */
int showAllLines;
#else
#define showAllThings 0
#define showAllLines 0
#endif

void DrawLine(pixel_t color, int x1, int y1, int x2, int y2) ATTR_OPTIMIZE_SIZE;

/*================================================================= */
/* */
/* Start up Automap */
/* */
/*================================================================= */
void AM_Start(void)
{
	if (amcurmap != gamemapinfo.mapNumber)
	{
#ifdef MARS
		scale = 1;
#else
		scale = 3;
#endif
		amcurmap = gamemapinfo.mapNumber;
	}

#ifdef JAGUAR
	showAllThings = showAllLines = 0;
#endif
	players[consoleplayer].automapflags &= ~AF_ACTIVE;
}

/*================================================================= */
/* */
/* Check for cheat codes for automap fun stuff! */
/* */
/*================================================================= */
#ifdef JAGUAR
cheat_e AM_CheckCheat(int buttons,int oldbuttons)
{
	int	codes[9] = {BT_1,BT_2,BT_3,BT_4,BT_5,BT_6,BT_7,BT_8,BT_9};
	char	chars[9] = "123456789";
	char	c;
	int		i;
	
	/* CONVERT BUTTON PRESS TO A CHARACTER */
	c = 'z';
	for (i = 0; i < 9; i++)
		if ((buttons & codes[i]) && !(oldbuttons & codes[i]))
		{
			c = chars[i];
			break;
		}
		
	if (c == 'z')	/* NO 1-9 BUTTON PRESSED */
		return -1;
	
	/* SHIFT STRING LEFT A CHARACTER */
	for (i = 1; i < 10; i++)
		currentcheat[i-1] = currentcheat[i];
	currentcheat[9] = c;
	currentcheat[10] = 0;
	
	/* SEE IF STRING MATCHES A CHEATSTRING */
	for (i = 0; i < ch_maxcheats; i++)
		if (!D_strncasecmp(currentcheat,cheatstrings[i],10))
			return i;

	return -1;
}
#endif

void DrawLine (pixel_t color, int x1, int y1, int x2, int y2)
{
#ifdef JAGUAR
	int		dx, dy;
	int		adx, ady;
	int		quadcolor;
	int		xstep, ystep;
	int		count = 1;
		
	*(int *)0xf02200 = (int)workingscreen;	/* a1 base register */
	*(int *)0xf02204 = (4<<3) /* 16 bit pixels */
					+ (29<<9) /* 160 width image */
					+ (3<<16) /* add increment */
					;							/* a1 flags */

	*(int *)0xf02208 = (180<<16) + 160;		/* a1 clipping size */
	
	*(int *)0xf0220c = (y1<<16) + x1;	/* a1 pixel pointer */
	*(int *)0xf02218 = zero;			/* a1 pixel pointer fraction */

	quadcolor = (color<<16) + color;
	*(int *)0xf02240 = quadcolor;
	*(int *)0xf02244 = quadcolor;	/* source data register */


	dx = x2 - x1;
	adx = dx<0 ? -dx : dx;
	dy = y2 - y1;
	ady = dy<0 ? -dy : dy;
	
	if (!dx)
	{
		xstep = 0;
		ystep = dy > 0 ? 0x10000 : -0x10000;
		count = ady;			/* count */
	}
	else if (!dy)
	{
		ystep = 0;
		xstep = dx > 0 ? 0x10000 : -0x10000;
		count = adx;			/* count */
	}
	else if (adx > ady)
	{
		if (dx > 0)
			xstep = 0x10000;
		else
			xstep = -0x10000;
		
		ystep = (dy<<16)/adx;
		count = adx;			/* count */
	}
	else
	{
		if (dy > 0)
			ystep = 0x10000;
		else
			ystep = -0x10000;
		
		xstep = (dx<<16)/ady;
		count = ady;			/* count */
	}

	*(int *)0xf0223c = (1<<16) + count;			/* count */
	
	*(int *)0xf0221c = (ystep&0xffff0000) + (((unsigned)xstep)>>16); /* a1 icrement */
	*(int *)0xf02220 = (ystep<<16) + (xstep&0xffff);	 /* a1 icrement frac */

	*(int *)0xf02238 = (1<<6)				/* enable clipping */
					+ (12<<21);				/* copy source */
#endif

#ifdef MARS
	int		dx, dy;
	int		adx, ady;
	byte		quadcolor;
	int		xstep, ystep;
	int		count = 1;
	int 	x, y;
	int		zwidth = 320 * FRACUNIT;
	int		zheight = I_FrameBufferHeight() * FRACUNIT;
	byte 	*fb = (byte*)I_FrameBuffer();

	dx = x2 - x1;
	adx = dx<0 ? -dx : dx;
	dy = y2 - y1;
	ady = dy<0 ? -dy : dy;

	if (!dx)
	{
		xstep = 0;
		ystep = dy > 0 ? 1*FRACUNIT : -1*FRACUNIT;
		count = ady;			/* count */
	}
	else if (!dy)
	{
		ystep = 0;
		xstep = dx > 0 ? 1*FRACUNIT : -1*FRACUNIT;
		count = adx;			/* count */
	}
	else if (adx > ady)
	{
		if (dx > 0)
			xstep = 1*FRACUNIT;
		else
			xstep = -1*FRACUNIT;
		
		ystep = FixedDiv(dy<<FRACBITS,adx<<FRACBITS);
		count = adx;			/* count */
	}
	else
	{
		if (dy > 0)
			ystep = 1*FRACUNIT;
		else
			ystep = -1*FRACUNIT;
		
		xstep = FixedDiv(dx<<FRACBITS,ady<<FRACBITS);
		count = ady;			/* count */
	}

	quadcolor = color;

	x1 *= FRACUNIT;
	y1 *= FRACUNIT;
	x2 *= FRACUNIT;
	y2 *= FRACUNIT;

	x = x1;
	y = y1;
 	while(count-- > 0)
	{
		boolean clipped = false;
		byte *p = fb + (y>>FRACBITS)*320 + (x>>FRACBITS);

		if (x < 0 || x >= zwidth) clipped = true;
		if (y < 0 || y >= zheight) clipped = true;
		x += xstep;
		y += ystep;

		if (!clipped) *p = quadcolor;
	}
#endif
}


/*
==================
=
= AM_Control
=
= Called by P_PlayerThink before any other player processing
=
= Button bits can be eaten by clearing them in ticbuttons[playernum]
==================
*/

void AM_Control (player_t *player)
{
	int		buttons, oldbuttons, step;
#ifdef JAGUAR
	cheat_e	cheatcode;
#endif

	buttons = ticbuttons[playernum];
	oldbuttons = oldticbuttons[playernum];

	if ( (buttons & BT_AUTOMAP) && !(oldbuttons & BT_AUTOMAP) )
	{
		player->automapflags ^= AF_ACTIVE;
		player->automapx = player->mo->x;
		player->automapy = player->mo->y;
#ifndef MARS
		blockx = 80;
		blocky = 90;
#endif
	}

	if ( !(player->automapflags & AF_ACTIVE) )
		return;

	oldplayerx = player->automapx;
	oldplayery = player->automapy;
	
	blink = blink&7;	/* BLINK PLAYER'S BOX */
	blink++;
	pause++;				/* PAUSE BETWEEN SCALINGS */

	step = STEPVALUE;
	if (buttons & BT_A)
		step *= 2;

#ifdef JAGUAR
	cheatcode = AM_CheckCheat(buttons,oldbuttons);
	switch(cheatcode)
	{
		case ch_allmap:
			showAllLines ^= 1;
			break;
		case ch_things:
			showAllThings ^= 1;
		default:
			break;
	}
#endif

	if (buttons & BT_C)		/* IF 'C' IS HELD DOWN, MOVE AROUND */
	{
		player->automapx = player->mo->x;
		player->automapy = player->mo->y;

		ticbuttons[playernum] &= ~(BT_C | configuration[controltype][2] | BT_STRAFE | BT_STRAFELEFT | BT_STRAFERIGHT);
		oldticbuttons[playernum] &= ~(BT_C | configuration[controltype][2] | BT_STRAFE | BT_STRAFELEFT | BT_STRAFERIGHT);
		return;
	}

	if (buttons & BT_RIGHT)
	{
		if (buttons & BT_B)
		{
			if (pause > 5)
			{
				pause = 0;
				scale--;
				if (scale < 0)
					scale = 0;
			}
		}
		else
			player->automapx+=step;
	}
	if (buttons & BT_LEFT)
	{
		if (buttons & BT_B)
		{
			if (pause > 5)
			{
				pause = 0;
				scale++;
				if (scale == MAXSCALES)
					scale--;
			}
		}
		else
			player->automapx-=step;
	}
	if (buttons & BT_UP)
	{
		if (buttons & BT_B)
		{
			if (pause > 5)
			{
				pause = 0;
				scale--;
				if (scale < 0)
					scale = 0;
			}
		}
		else
			player->automapy+=step;
	}
	if (buttons & BT_DOWN)
	{
		if (buttons & BT_B)
		{
			if (pause > 5)
			{
				pause = 0;
				scale++;
				if (scale == MAXSCALES)
					scale--;
			}
		}
		else
			player->automapy-=step;
	}
	
	ticbuttons[playernum] &= ~(BT_B | BT_LEFT | BT_RIGHT |
		BT_UP | BT_DOWN | configuration[controltype][1]);
}

/*
==================
=
= AM_Drawer
=
= Draws the current frame to workingscreen
==================
*/

extern	int	workpage;
extern	pixel_t	*screens[2];	/* [SCREENWIDTH*SCREENHEIGHT];  */

void AM_Drawer (void)
{

	int		i;
	player_t	*p;
	line_t	*line;
	int		x1,y1;
	int		x2,y2;
	int		ox,oy;
	int		outcode;
	int		outcode2;
	int		color;
	int		xshift;
	int		yshift;
	int		drawn;		/* HOW MANY LINES DRAWN? */

#ifdef JAGUAR
	workingscreen = screens[workpage];
	
/*	D_memset (workingscreen, 0, 160*180*2); */
		
	*(int *)0xf0220c = zero;		/* a1 pixel pointers */

	*(int *)0xf02200 = (int)workingscreen;	/* a1 base register */
		
	*(int *)0xf02204 = (4<<3)		 	/* 16 bit pixels */
					+ (29<<9)			/* 160 wide */
					+ (0<<16)			/* add phrase */
					;					/* a1 flags */
					

	*(int *)0xf02240 = zero2;
	*(int *)0xf02244 = ZERO;		/* source data register */
	
	*(int *)0xf0223c = 0x10000 + 160*180;	/* count */

	*(int *)0xf02238 = (12<<21);	/* copy source */

#endif

#ifdef MARS
	I_ClearFrameBuffer();
#endif

	p = &players[consoleplayer];
	ox = p->automapx;
	oy = p->automapy;
	
	xshift = scalex[scale];
	yshift = scaley[scale];

	line = lines;
	drawn = 0;
	for (i=0 ; i<numlines ; i++,line++)
	{
		if ((!(line->flags & ML_MAPPED) ||		/* IF NOT MAPPED OR DON'T DRAW */
			line->flags & ML_DONTDRAW) &&
			(!(p->powers[pw_allmap] + showAllLines)))
			continue;
			
		x1 = line->v1->x;
		y1 = line->v1->y;
		x2 = line->v2->x;
		y2 = line->v2->y;
		
		x1 -= ox;
		x2 -= ox;
		y1 -= oy;
		y2 -= oy;
		x1 >>= xshift;
		x2 >>= xshift;
		y1 >>= yshift;
		y2 >>= yshift;
		outcode = (y1 > 90) << 1;
		outcode |= (y1 < -90) ;
		outcode2 = (y2 > 90) << 1;
		outcode2 |= (y2 < -90) ;
#ifndef MARS
		if (outcode & outcode2) continue;
#endif
		outcode = (x1 > 80) << 1;
		outcode |= (x1 < -80) ;
		outcode2 = (x2 > 80) << 1;
		outcode2 |= (x2 < -80) ;
#ifndef MARS
		if (outcode & outcode2) continue;
#endif

		/* */
		/* Figure out color */
		/* */
		color = CRY_BROWN;
		if ((p->powers[pw_allmap] +
			showAllLines) &&			/* IF COMPMAP && !MAPPED YET */
			!(line->flags & ML_MAPPED))
			color = CRY_GREY;
		else
		if (!(line->flags & ML_TWOSIDED))	/* ONE-SIDED LINE */
			color = CRY_RED;
		else
		if (line->special == 97 ||		/* TELEPORT LINE */
			line->special == 39)
			color = CRY_GREEN;
		else
		if (line->flags & ML_SECRET)
			color = CRY_RED;
		else
		if (line->special)
			color = CRY_BLUE;			/* SPECIAL LINE */
		else
		if (LD_BACKSECTOR(line) != NULL &&
			LD_FRONTSECTOR(line)->floorheight !=
			LD_BACKSECTOR(line)->floorheight)
			color = CRY_YELLOW;
		else
		if (LD_BACKSECTOR(line) != NULL &&
			LD_FRONTSECTOR(line)->ceilingheight !=
			LD_BACKSECTOR(line)->ceilingheight)
			color = CRY_BROWN;		
		
		DrawLine (color, 80+x1,90-y1,80+x2,90-y2);
		drawn++;
	}
	
	/* IF <5 LINES DRAWN, MOVE TO LAST POSITION! */
	if (drawn < 5)
	{
		p->automapx = oldplayerx;
		p->automapy = oldplayery;
	}
	
	if (blink > 2)
	{
		fixed_t	c;
		fixed_t	s;
		fixed_t x1,y1;
		fixed_t	nx1,ny1;
		fixed_t	nx2,ny2;
		fixed_t nx3,ny3;
		angle_t	angle;
		player_t *pl;
		
		for (i=0;i<MAXPLAYERS;i++)
		{
			if ((i != consoleplayer) &&
				(netgame != gt_coop))
				continue;
			
			pl = &players[i];
			x1 = (pl->mo->x - players[consoleplayer].automapx);
			y1 = (pl->mo->y - players[consoleplayer].automapy);
			angle = pl->mo->angle;
			color = !i ? CRY_GREEN : CRY_YELLOW;
			c = finecosine(angle >> ANGLETOFINESHIFT);
			s = finesine(angle >> ANGLETOFINESHIFT);		
			nx1 = FixedMul(c,NOSELENGTH) + x1;
			ny1 = FixedMul(s,NOSELENGTH) + y1;
			
			c = finecosine(((angle - ANG90) - ANG45) >> ANGLETOFINESHIFT);
			s = finesine(((angle - ANG90) - ANG45) >> ANGLETOFINESHIFT);
			nx2 = FixedMul(c, NOSELENGTH) + x1;
			ny2 = FixedMul(s, NOSELENGTH) + y1;
			
			c = finecosine(((angle + ANG90) + ANG45) >> ANGLETOFINESHIFT);
			s = finesine(((angle + ANG90) + ANG45) >> ANGLETOFINESHIFT);
			nx3 = FixedMul(c, NOSELENGTH) + x1;
			ny3 = FixedMul(s, NOSELENGTH) + y1;
			
			nx1 >>= xshift;
			ny1 >>= yshift;
			nx2 >>= xshift;
			ny2 >>= yshift;
			nx3 >>= xshift;
			ny3 >>= yshift;
			
			DrawLine(color,80+nx1,90-ny1,80+nx2,90-ny2);
			DrawLine(color,80+nx2,90-ny2,80+nx3,90-ny3);
			DrawLine(color,80+nx1,90-ny1,80+nx3,90-ny3);
		}
	}
	
	/* SHOW ALL MAP THINGS (CHEAT) */
	if (showAllThings)
	{
		fixed_t x1,y1;
		fixed_t	nx1,ny1;
		fixed_t	nx2,ny2;
		fixed_t nx3,ny3;
		mobj_t	*mo;
		mobj_t	*next;
		
		for (mo = mobjhead.next; mo != (void *)&mobjhead; mo=next)
		{
			next = mo->next;
			if (mo == p->mo)
				continue;
				
			x1 = mo->x - p->automapx;
			y1 = mo->y - p->automapy;
			
			nx1 = x1 >> xshift;
			ny1 = (y1 - MOBJLENGTH) >> yshift;
			nx2 = (x1 - MOBJLENGTH) >> xshift;
			ny2 = (y1 + MOBJLENGTH) >> yshift;
			nx3 = (x1 + MOBJLENGTH) >> xshift;
			ny3 = ny2;

			DrawLine(CRY_AQUA,80+nx1,90-ny1,80+nx2,90-ny2);
			DrawLine(CRY_AQUA,80+nx2,90-ny2,80+nx3,90-ny3);
			DrawLine(CRY_AQUA,80+nx1,90-ny1,80+nx3,90-ny3);
		}
	}
}
