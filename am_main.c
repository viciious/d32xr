/* am_main.c -- automap */
/* am_main.c -- automap */

#include "doomdef.h"
#include "p_local.h"

#define	STEPVALUE	0x800000

#ifdef MARS

#define CRY_RED 	191
#define CRY_BLUE 	207
#define CRY_GREEN 	127
#define CRY_BROWN 	79
#define CRY_YELLOW 	167
#define CRY_GREY 	94
#define CRY_AQUA 	223

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
#ifndef MARS
static	VINT	scaley[MAXSCALES] = {18,19,20,21,22};
#endif
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

void DrawLine(pixel_t color, int x1, int y1, int x2, int y2);

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

#ifdef JAGUAR
void DrawLine(pixel_t color, int x1, int y1, int x2, int y2)
{
	int		dx, dy;
	int		adx, ady;
	int		quadcolor;
	int		xstep, ystep;
	int		count = 1;

	*(int*)0xf02200 = (int)workingscreen;	/* a1 base register */
	*(int*)0xf02204 = (4 << 3) /* 16 bit pixels */
		+ (29 << 9) /* 160 width image */
		+ (3 << 16) /* add increment */
		;							/* a1 flags */

	*(int*)0xf02208 = (180 << 16) + 160;		/* a1 clipping size */

	*(int*)0xf0220c = (y1 << 16) + x1;	/* a1 pixel pointer */
	*(int*)0xf02218 = zero;			/* a1 pixel pointer fraction */

	quadcolor = (color << 16) + color;
	*(int*)0xf02240 = quadcolor;
	*(int*)0xf02244 = quadcolor;	/* source data register */


	dx = x2 - x1;
	adx = dx < 0 ? -dx : dx;
	dy = y2 - y1;
	ady = dy < 0 ? -dy : dy;

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

		ystep = (dy << 16) / adx;
		count = adx;			/* count */
	}
	else
	{
		if (dy > 0)
			ystep = 0x10000;
		else
			ystep = -0x10000;

		xstep = (dx << 16) / ady;
		count = ady;			/* count */
	}

	*(int*)0xf0223c = (1 << 16) + count;			/* count */

	*(int*)0xf0221c = (ystep & 0xffff0000) + (((unsigned)xstep) >> 16); /* a1 icrement */
	*(int*)0xf02220 = (ystep << 16) + (xstep & 0xffff);	 /* a1 icrement frac */

	*(int*)0xf02238 = (1 << 6)				/* enable clipping */
		+ (12 << 21);				/* copy source */
}
#elif defined(MARS)

static void putPixel(byte* fb, pixel_t c, fixed_t x, fixed_t y, fixed_t brightness)
{
	if (brightness <= 0)
		brightness = 0;
	else if (brightness >= FRACUNIT)
		brightness = 0x7;
	else
		brightness = (brightness >> 13) & 0x7;

	y >>= FRACBITS;
	x >>= FRACBITS;
	fb[(y << 8) + (y << 6) + x] = c - brightness;
}

void DrawLine(pixel_t color, fixed_t x0, fixed_t y0, fixed_t x1, fixed_t y1)
{
	fixed_t dx, dy;
	fixed_t gradient;
	fixed_t x, steep, temp;
	fixed_t xpxl1, xpxl2, inters;
	byte* fb = (byte*)I_FrameBuffer();

	if ((x0 < 0 && x1 < 0) || (x0 >= 320 && x1 >= 320))
		return;
	if ((y0 < 0 && y1 < 0) || (y0 >= 224 && y1 >= 224))
		return;

	steep = D_abs(y1 - y0) > D_abs(x1 - x0);
	if (steep)
	{
		// x and y should be reversed
		temp = x0, x0 = y0, y0 = temp;
		temp = x1, x1 = y1, y1 = temp;
	}
	if (x0 > x1)
	{
		temp = x0, x0 = x1, x1 = temp;
		temp = y0, y0 = y1, y1 = temp;
	}

	x0 <<= FRACBITS;
	y0 <<= FRACBITS;
	x1 <<= FRACBITS;
	y1 <<= FRACBITS;

	// compute the slope
	dx = x1 - x0;
	dy = y1 - y0;
	gradient = FRACUNIT;
	if (dx != 0)
		gradient = FixedDiv(dy, dx);

	xpxl1 = x0;
	xpxl2 = x1;
	inters = y0;

	if (xpxl1 < 0)
	{
		inters += gradient * ((-xpxl1) >> FRACBITS);
		xpxl1 = 0;
	}

	// main loop
	if (steep)
	{
		if (xpxl2 > 223 * FRACUNIT) xpxl2 = 223 * FRACUNIT;
		for (x = xpxl1; x <= xpxl2; x += FRACUNIT, inters += gradient)
		{
			fixed_t y = inters & ~(FRACUNIT - 1);
			fixed_t b = inters & (FRACUNIT - 1);
			if (y >= 0 && y <= 318 * FRACUNIT)
			{
				putPixel(fb, color, y, x, FRACUNIT - b);
				putPixel(fb, color, y + FRACUNIT, x, b);
			}
		}
	}
	else
	{
		if (xpxl2 > 319 * FRACUNIT) xpxl2 = 319 * FRACUNIT;
		for (x = xpxl1; x <= xpxl2; x += FRACUNIT, inters += gradient)
		{
			fixed_t y = inters & ~(FRACUNIT - 1);
			fixed_t b = inters & (FRACUNIT - 1);
			if (y >= 0 && y <= 222 * FRACUNIT)
			{
				putPixel(fb, color, x, y, FRACUNIT - b);
				putPixel(fb, color, x, y + FRACUNIT, b);
			}
		}
	}
}
#else
void DrawLine(pixel_t color, int x1, int y1, int x2, int y2)
{
}
#endif

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
#ifdef MARS
	yshift = xshift;
#else
	yshift = scaley[scale];
#endif

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
		outcode = (y1 > 100) << 1;
		outcode |= (y1 < -100) ;
		outcode2 = (y2 > 100) << 1;
		outcode2 |= (y2 < -100) ;
#ifndef MARS
		if (outcode & outcode2) continue;
#endif
		outcode = (x1 > 160) << 1;
		outcode |= (x1 < -160) ;
		outcode2 = (x2 > 160) << 1;
		outcode2 |= (x2 < -160) ;
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
		
		DrawLine (color, 160+x1,100-y1,160+x2,100-y2);
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
			
			DrawLine(color,160+nx1,100-ny1,160+nx2,100-ny2);
			DrawLine(color,160+nx2,100-ny2,160+nx3,100-ny3);
			DrawLine(color,160+nx1,100-ny1,160+nx3,100-ny3);
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

			DrawLine(CRY_AQUA,160+nx1,100-ny1,160+nx2,100-ny2);
			DrawLine(CRY_AQUA,160+nx2,100-ny2,160+nx3,100-ny3);
			DrawLine(CRY_AQUA,160+nx1,100-ny1,160+nx3,100-ny3);
		}
	}
}
