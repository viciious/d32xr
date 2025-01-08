/* am_main.c -- automap */

#include "doomdef.h"
#include "p_local.h"
#ifdef MARS
#include "mars.h"
#endif

#define	STEPVALUE		0x800000

#define SCALESTEP		0x7ff8
#define MAXSCALE		18 * FRACUNIT
#define MINSCALE		2 * FRACUNIT
#define DEFAULTSCALE	6 * FRACUNIT

#ifdef MARS

#define CRY_RED 	191
#define CRY_BLUE 	207
#define CRY_GREEN 	127
#define CRY_BROWN 	79
#define CRY_YELLOW 	167
#define CRY_GREY 	94
#define CRY_AQUA 	250
#define CRY_THINGS 	223

#else

#define CRY_RED		0xd260
#define CRY_BLUE	0x3080
#define CRY_GREEN	0x6f80
#define CRY_BROWN	0xba20
#define CRY_YELLOW	0xff80
#define CRY_GREY	0x9826
#define CRY_AQUA	0x6aa0
#define CRY_THINGS 	CRY_AQUA

#endif

static	VINT	blink = 0;
static	VINT	pause;
static	uint16_t linesdrawn = 0;
static fixed_t  scale;

static	VINT	amcurmap = -1;
#define NOSELENGTH	0x200000		/* PLAYER'S TRIANGLE */
#define MOBJLENGTH	0x100000
		
static	fixed_t	oldplayerx;
static	fixed_t oldplayery;
static	fixed_t oldscale;

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
#endif
VINT showAllThings = 0;		/* CHEAT VARS */
VINT showAllLines = 0;

static void DrawLine(uint8_t *fb,pixel_t color, fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, fixed_t miny, fixed_t maxy);

/*================================================================= */
/* */
/* Start up Automap */
/* */
/*================================================================= */
void AM_Start(void)
{
	linesdrawn = 0;
	if (amcurmap != gamemapinfo->mapNumber)
	{
		scale = DEFAULTSCALE;
		oldscale = DEFAULTSCALE;
		amcurmap = gamemapinfo->mapNumber;
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
static void DrawLine(pixel_t color, fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, fixed_t miny, fixed_t maxy)
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

static inline void putPixel(byte* fb, pixel_t c, unsigned x, unsigned y, fixed_t brightness)
{
	if (brightness <= 0)
		brightness = 0;
	else if (brightness >= FRACUNIT)
		brightness = 0x7;
	else
		brightness = ((unsigned)brightness >> 13) & 0x7;

	y >>= FRACBITS;
	x >>= FRACBITS;
	y <<= 6;
	fb[(y << 2) + y + x] = c - brightness;
}

static void DrawLine(uint8_t *fb, pixel_t color, fixed_t x0, fixed_t y0, fixed_t x1, fixed_t y1, fixed_t miny, fixed_t maxy)
{
	fixed_t dx, dy;
	fixed_t gradient;
	fixed_t x, steep, temp;
	fixed_t xpxl1, xpxl2, inters;
	fixed_t minyfrac = miny * FRACUNIT;

	if ((x0 < 0 && x1 < 0) || (x0 >= 320 && x1 >= 320))
		return;
	if ((y0 < miny && y1 < miny) || (y0 >= maxy && y1 >= maxy))
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

	if (steep)
	{
		if (xpxl1 < minyfrac)
		{
			inters += gradient * (((minyfrac-xpxl1)) >> FRACBITS);
			xpxl1 = minyfrac;
		}
	}
	else
	{
		if (xpxl1 < 0)
		{
			inters += gradient * ((-xpxl1) >> FRACBITS);
			xpxl1 = 0;
		}
	}

	// main loop
	if (steep)
	{
		if (xpxl2 > (maxy-1) * FRACUNIT) xpxl2 = (maxy - 1) * FRACUNIT;
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
	else if (y0 == y1)
	{
		fixed_t y;
		int c1, c2;
		int16_t *sfb;

		x0 >>= FRACBITS;
		x1 >>= FRACBITS;
		y0 >>= FRACBITS;

		if (x0 < 0)
			x0 = 0;
		if (x1 > 319)
			x1 = 319;
		y = y0;

		if (y >= 222)
			return;
		if (y < miny)
			return;
		if (y >= maxy)
		{
			if (maxy < 223)
				return;
		}

		c1 = ((color - 0x7) << 8) | (color - 0x7);
		c2 = (color << 8) | color;

		x = x0;
		fb = &fb[(y << 8) + (y << 6) + x];
		if (x & 1)
		{
			fb[0] = c1 & 0xff;
			fb[320] = c2 & 0xff;
			fb++;
			x++;
		}

		sfb = (int16_t*)fb;

		for ( ; x <= (x1 & ~1); x += 2)
		{
			sfb[0] = c1;
			sfb[160] = c2;
			sfb++;
		}

		if (x < x1)
		{
			fb = (byte*)sfb;
			fb[0] = c1 & 0xff;
			fb[320] = c2 & 0xff;
		}
	}
	else
	{
		fixed_t maxyfrac = (maxy - 2) * FRACUNIT;

		if (xpxl2 > 319 * FRACUNIT) xpxl2 = 319 * FRACUNIT;
		for (x = xpxl1; x <= xpxl2; x += FRACUNIT, inters += gradient)
		{
			fixed_t y = inters & ~(FRACUNIT - 1);
			fixed_t b = inters & (FRACUNIT - 1);
			if (y >= minyfrac && y <= maxyfrac)
			{
				putPixel(fb, color, x, y, FRACUNIT - b);
				putPixel(fb, color, x, y + FRACUNIT, b);
			}
		}
	}
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
	int		buttons, oldbuttons;
	fixed_t	step;
#ifdef JAGUAR
	cheat_e	cheatcode;
#endif
	int actionbtns = BT_ATTACK | BT_STRAFE | BT_USE;

	buttons = player->ticbuttons;
	oldbuttons = player->oldticbuttons;

	if ( (buttons & BT_AUTOMAP) && !(oldbuttons & BT_AUTOMAP) )
	{
		player->automapflags ^= AF_ACTIVE;
		player->automapx = oldplayerx = player->mo->x;
		player->automapy = oldplayery = player->mo->y;
		linesdrawn = UINT16_MAX;
#ifndef MARS
		blockx = 80;
		blocky = 90;
#endif
	}

	/* IF <5 LINES DRAWN, MOVE TO LAST POSITION! */
	if (linesdrawn < 5)
	{
		player->automapx = oldplayerx;
		player->automapy = oldplayery;
		scale = oldscale;
	}
	else
	{
		oldplayerx = player->automapx;
		oldplayery = player->automapy;
		oldscale = scale;
	}

	if ( !(player->automapflags & AF_ACTIVE) )
		return;
	
	blink = blink&7;	/* BLINK PLAYER'S BOX */
	blink++;
	pause++;				/* PAUSE BETWEEN SCALINGS */

	step = scale * 16;
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

	player->ticbuttons &= ~actionbtns;
	player->oldticbuttons &= ~actionbtns;

	if (buttons & BT_C)		/* IF 'C' IS HELD DOWN, MOVE AROUND */
	{
		player->automapx = player->mo->x;
		player->automapy = player->mo->y;

		player->ticbuttons &= ~(BT_C | BT_STRAFELEFT | BT_STRAFERIGHT);
		player->oldticbuttons &= ~(BT_C | BT_STRAFELEFT | BT_STRAFERIGHT);
		return;
	}

	if (buttons & BT_MOVERIGHT)
	{
		player->automapx+=step;
	}
	if (buttons & BT_MOVELEFT)
	{
		player->automapx-=step;
	}
	if (buttons & BT_MOVEUP)
	{
		if (buttons & BT_B)
		{
			scale -= SCALESTEP;
			if (scale < MINSCALE)
				scale = MINSCALE;
		}
		else
			player->automapy+=step;
	}
	if (buttons & BT_MOVEDOWN)
	{
		if (buttons & BT_B)
		{
			scale += SCALESTEP;
			if (scale > MAXSCALE)
				scale = MAXSCALE;
		}
		else
			player->automapy-=step;
	}
	
	player->ticbuttons &= ~(BT_B | BT_MOVELEFT | BT_MOVERIGHT | BT_MOVEUP | BT_MOVEDOWN);
}

static void AM_DrawMapStats(void)
{
	int i;
	char buf[128];
	int kc, sc, ic;
	int kcol, scol, icol;

	switch (netgame)
	{
	default:
		kc = sc = ic = 0;
		kcol = scol = icol = COLOR_WHITE;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;
			kc += players[i].killcount;
			sc += players[i].secretcount;
			ic += players[i].itemcount;
		}

		if (kc == totalkills)
			kcol = CRY_AQUA;
		if (sc == totalsecret)
			scol = CRY_AQUA;
		if (ic == totalitems)
			icol = CRY_AQUA;

		D_snprintf(buf, sizeof(buf), "^%02xK:%d/%d ^%02xI:%d/%d ^%02xS:%d/%d",
			kcol, kc, totalkills,
			icol, ic, totalitems,
			scol, sc, totalsecret);
		I_Print8(12, 20, buf);
		break;
	case gt_coop:
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;
			D_snprintf(buf, sizeof(buf), "P%d K:%d/%d I:%d/%d S:%d/%d", i + 1, 
				players[i].killcount, totalkills, 
				players[i].itemcount, totalitems,
				players[i].secretcount, totalsecret);
			I_Print8(12, 20 - MAXPLAYERS + 1 + i, buf);
		}
		break;
	case gt_deathmatch:
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;
			D_snprintf(buf, sizeof(buf), "P%d Frags: %d", i + 1, players[i].frags);
			I_Print8(12, 20 - MAXPLAYERS + 1 + i, buf);
		}
		break;
	}

	I_Print8(12, 21, DMAPINFO_STRFIELD(gamemapinfo, name));
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

static void AM_Drawer_ (int c)
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
	int		drawn;		/* HOW MANY LINES DRAWN? */
	int		miny, maxy;
	extern VINT sbar_height;
	int		am_y;
	int		am_height;
	int		am_halfh;
	byte	*fb;

	am_y = 0;
	am_height = I_FrameBufferHeight() - sbar_height;
	if (splitscreen)
	{
		am_y = sbar_height;
		am_height -= sbar_height;
	}
	am_halfh = am_height / 2;

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

	p = &players[consoleplayer];
	ox = p->automapx;
	oy = p->automapy;

	line = lines;
	drawn = 0;

	switch (c)
	{
		case 1:
			miny = 0;
			maxy = am_halfh;
			break;
		case 2:
			miny = am_halfh-1;
			maxy = am_height-1;
			break;
		case 0:
		default:
			miny = 0;
			maxy = am_height-1;
			break;
	}

	fb = (byte*)I_FrameBuffer();
	fb += am_y * 320;

	{
		int* p = (int*)(fb + 320 * ((miny + 1)&~1));
		int* p_end = (int*)(fb + 320 * ((maxy + 1) & ~1));
		D_memset(p, 0, (p_end-p)*sizeof(*p));
	}

	for (i=0 ; i<numlines ; i++,line++)
	{
		int flags;
		int mapped;
		mapvertex_t *v1, *v2;
		boolean twoSided = line->sidenum[1] != -1;

		flags = line->flags;
		mapped = (line->flags & ML_MAPPED) != 0;
		if ((!mapped ||		/* IF NOT MAPPED OR DON'T DRAW */
			flags & ML_DONTDRAW) &&
			(!(p->powers[pw_allmap] + showAllLines)))
			continue;

		v1 = &vertexes[line->v1];
		v2 = &vertexes[line->v2];

		y1 = v1->y << FRACBITS;
		y2 = v2->y << FRACBITS;

		y1 -= oy;
		y2 -= oy;

		y1 = IDiv(y1, scale);
		y2 = IDiv(y2, scale);

		y1 = am_halfh-y1;
		y2 = am_halfh-y2;

		outcode = (y1 > maxy) << 1;
		outcode |= (y1 < miny) ;
		outcode2 = (y2 > maxy) << 1;
		outcode2 |= (y2 < miny) ;
		if (outcode & outcode2) continue;

		x1 = v1->x << FRACBITS;
		x2 = v2->x << FRACBITS;

		x1 -= ox;
		x2 -= ox;

		x1 = IDiv(x1, scale);
		x2 = IDiv(x2, scale);

		outcode = (x1 > 160) << 1;
		outcode |= (x1 < -160);
		outcode2 = (x2 > 160) << 1;
		outcode2 |= (x2 < -160);
		if (outcode & outcode2) continue;

		x1 += 160;
		x2 += 160;

		/* */
		/* Figure out color */
		/* */
		color = CRY_BROWN;
		if ((p->powers[pw_allmap] +
			showAllLines) &&			/* IF COMPMAP && !MAPPED YET */
			!mapped)
			color = CRY_GREY;
		else
		if (!twoSided)	/* ONE-SIDED LINE */
			color = CRY_RED;
		else
		if (line->special == 97 ||		/* TELEPORT LINE */
			line->special == 39)
			color = CRY_GREEN;
		else
		if (flags & ML_SECRET)
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
		
		DrawLine (fb,color,x1,y1,x2,y2,miny,maxy);
		drawn++;
	}

	if (c > 1)
		return;
	
#ifdef MARS
	Mars_R_SecWait();
#endif

	linesdrawn = drawn;

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
			if (pl->powers[pw_invisibility])
				color = 246; // close to black
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

			nx1 = IDiv(nx1, scale);
			nx2 = IDiv(nx2, scale);
			nx3 = IDiv(nx3, scale);
			ny1 = IDiv(ny1, scale);
			ny2 = IDiv(ny2, scale);
			ny3 = IDiv(ny3, scale);

			DrawLine(fb,color,160+nx1,am_halfh-ny1,160+nx2,am_halfh-ny2, 0, am_height-1);
			DrawLine(fb,color,160+nx2,am_halfh-ny2,160+nx3,am_halfh-ny3, 0, am_height-1);
			DrawLine(fb,color,160+nx1,am_halfh-ny1,160+nx3,am_halfh-ny3, 0, am_height-1);
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
			
			nx1 = x1;
			ny1 = y1 - MOBJLENGTH;
			nx2 = x1 - MOBJLENGTH;
			ny2 = y1 + MOBJLENGTH;
			nx3 = x1 + MOBJLENGTH;

			nx1 = IDiv(nx1, scale);
			nx2 = IDiv(nx2, scale);
			nx3 = IDiv(nx3, scale);
			ny1 = IDiv(ny1, scale);
			ny2 = IDiv(ny2, scale);

			ny3 = ny2;

			DrawLine(fb,CRY_THINGS,160+nx1,am_halfh-ny1,160+nx2,am_halfh-ny2, 0, am_height-1);
			DrawLine(fb,CRY_THINGS,160+nx2,am_halfh-ny2,160+nx3,am_halfh-ny3, 0, am_height-1);
			DrawLine(fb,CRY_THINGS,160+nx1,am_halfh-ny1,160+nx3,am_halfh-ny3, 0, am_height-1);
		}
	}

	AM_DrawMapStats();
}

#ifdef MARS

void Mars_Sec_AM_Drawer(void)
{
	Mars_ClearCache();
	AM_Drawer_(2);
}

void AM_Drawer(void)
{
	Mars_AM_BeginDrawer();

	AM_Drawer_(1);

	Mars_AM_EndDrawer();
}


#else

void AM_Drawer(void)
{
	AM_Drawer_(0);
}

#endif
