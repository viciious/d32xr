/* st_main.c -- status bar */

#include "doomdef.h"
#include "st_main.h"

stbar_t	stbar;
short   stbar_y;
short micronums;
short	micronums_x[NUMMICROS] = {249,261,272,249,261,272};
short	micronums_y[NUMMICROS] = {15,15,15,25,25,25};

int		facetics;
int		newface;
short	card_x[NUMCARDS] = {KEYX,KEYX,KEYX,KEYX+3, KEYX+3, KEYX+3};
short	card_y[NUMCARDS] = {BLUKEYY,YELKEYY,REDKEYY,BLUKEYY,YELKEYY,REDKEYY};

boolean flashInitialDraw;		/* INITIALLY DRAW FRAG AMOUNTS (flag) */
sbflash_t	yourFrags;			/* INFO FOR YOUR FRAG FLASHING */
sbflash_t	hisFrags;

boolean	gibdraw;
int		gibframe;
int		gibdelay;

short	spclfaceSprite[NUMSPCLFACES] =
		{0,sbf_facelft,sbf_facergt,sbf_ouch,sbf_gotgat,sbf_mowdown};
boolean doSpclFace;
spclface_e	spclFaceType;

jagobj_t	*sbar;
VINT		sbar_height;
#ifndef MARS
byte		*sbartop;
#endif
short		faces;
jagobj_t	*sbobj[NUMSBOBJ];

sbflash_t	flashCards[NUMCARDS];	/* INFO FOR FLASHING CARDS & SKULLS */

short stbarframe;
short numstbarcmds;
stbarcmd_t stbarcmds[STC_NUMCMDTYPES+NUMMICROS+NUMCARDS*2];

#ifndef MARS
#define ST_EraseBlock EraseBlock
#endif

void ST_EraseBlock(int x, int y, int width, int height);

/*
====================
=
= ST_Init
=
= Locate and load all needed graphics
====================
*/

void ST_Init (void)
{
	int i, l;

	l = W_CheckNumForName("STBAR");
	if (l != -1)
	{
		sbar = (jagobj_t*)W_POINTLUMPNUM(l);
		sbar_height = BIGSHORT(sbar->height);
	}
	else
	{
		sbar = NULL;
		sbar_height = 0;
	}

	faces = W_CheckNumForName("FACE00");

	l = W_CheckNumForName("MINUS");
	if (l != -1)
	{
		for (i = 0; i < NUMSBOBJ; i++)
			sbobj[i] = W_CacheLumpNum(l + i, PU_STATIC);
	}
	else
	{
		D_memset(sbobj, 0, sizeof(sbobj[0]) * NUMSBOBJ);
	}

	l = W_CheckNumForName("MICRO_2");
	micronums = l;
}

void ST_ForceDraw(void)
{
	stbarframe = 0;
	stbar.forcedraw = true;
	ST_Ticker();
}

/*================================================== */
/* */
/*  Init this stuff EVERY LEVEL */
/* */
/*================================================== */
void ST_InitEveryLevel(void)
{
	int		i;

	/* force everything to be updated on next ST_Update */
	stbar.forcedraw = true;
	stbar.drawface = -1;
	facetics = 0;

	stbarframe = 0;
	numstbarcmds = 0;

	/* DRAW FRAG COUNTS INITIALLY */
	if (netgame == gt_deathmatch)
	{
		yourFrags.active = false;
		yourFrags.x = YOURFRAGX;
		yourFrags.y = YOURFRAGY;
		yourFrags.w = 30;
		yourFrags.h = 16;
		hisFrags.active = false;
		hisFrags.x = HISFRAGX;
		hisFrags.y = HISFRAGY;
		hisFrags.w = 30;
		hisFrags.h = 16;
		flashInitialDraw = true;
		stbar.yourFrags = players[consoleplayer].frags;
		stbar.hisFrags = players[!consoleplayer].frags;
	}
	
	stbar.gotgibbed = false;
	gibdraw = false;	/* DON'T DRAW GIBBED HEAD SEQUENCE */
	gibframe = 0;
	gibdelay = GIBTIME;
	doSpclFace = false;
	stbar.specialFace = f_none;
	
	for (i = 0; i < NUMCARDS; i++)
	{
		stbar.tryopen[i] = false;
		flashCards[i].active = false;
		flashCards[i].x = KEYX + (i>2?3:0);
		flashCards[i].y = card_y[i];
		flashCards[i].w = KEYW;
		flashCards[i].h = KEYH;
	}
}

/*
====================
=
= ST_Ticker
=
====================
*/

void ST_Ticker(void)
{
	int			i;
	player_t* p;
	int			ind;
	short		drawface;
	stbarcmd_t* cmd;

#ifdef MARS
	// double-buffered renderer on MARS
	if ((stbarframe++ & 1) == 1)
	{
		--facetics;
		--yourFrags.delay;
		--hisFrags.delay;
		for (ind = 0; ind < NUMCARDS; ind++)
		{
			if (!flashCards[ind].active)
				continue;
			--flashCards[ind].delay;
		}
		if (gibdraw)
			--gibdelay;
		return;
	}
#endif

	numstbarcmds = 0;

	p = &players[consoleplayer];

	/* */
	/* Animate face */
	/* */
	if (--facetics <= 0)
	{
		facetics = M_Random ()&15;
		newface = M_Random ()&3;
		if (newface == 3)
			newface = 1;
		doSpclFace = false;
	}
	
	/* */
	/* Draw special face? */
	/* */
	if (stbar.specialFace)
	{
		doSpclFace = true;
		spclFaceType = stbar.specialFace;
		facetics = 15;
		stbar.specialFace = f_none;
	}
	
	/* */
	/* Flash YOUR FRAGS amount */
	/* */
	if (yourFrags.active && --yourFrags.delay <= 0)
	{
		yourFrags.delay = FLASHDELAY;
		yourFrags.doDraw ^= 1;
		if (!--yourFrags.times)
			yourFrags.active = false;
		if (yourFrags.doDraw && yourFrags.active)
			S_StartSound(NULL,sfx_itemup);
	}
	
	/* */
	/* Flash HIS FRAGS amount */
	/* */
	if (hisFrags.active && --hisFrags.delay <= 0)
	{
		hisFrags.delay = FLASHDELAY;
		hisFrags.doDraw ^= 1;
		if (!--hisFrags.times)
			hisFrags.active = false;
		if (hisFrags.doDraw && hisFrags.active)
			S_StartSound(NULL,sfx_itemup);
	}

	/* */
	/* Did we get gibbed? */
	/* */
	if (stbar.gotgibbed && !gibdraw)
	{
		gibdraw = true;
		gibframe = 0;
		gibdelay = GIBTIME;
		stbar.gotgibbed = false;
	}
	
	/* */
	/* Tried to open a CARD or SKULL door? */
	/* */
	for (ind = 0; ind < NUMCARDS; ind++)
	{
		/* CHECK FOR INITIALIZATION */
		if (stbar.tryopen[ind])
		{
			stbar.tryopen[ind] = false;
			flashCards[ind].active = true;
			flashCards[ind].delay = FLASHDELAY;
			flashCards[ind].times = FLASHTIMES+1;
			flashCards[ind].doDraw = false;
		}
		
		/* MIGHT AS WELL DO TICKING IN THE SAME LOOP! */
		if (!flashCards[ind].active)
			continue;
		if (--flashCards[ind].delay <= 0)
		{
			flashCards[ind].delay = FLASHDELAY;
			flashCards[ind].doDraw ^= 1;
			if (!--flashCards[ind].times)
				flashCards[ind].active = false;
			if (flashCards[ind].doDraw && flashCards[ind].active)
				S_StartSound(NULL,sfx_itemup);
		}
	}

	/* */
	/* Ammo */
	/* */
	if (p->readyweapon == wp_nochange)
		i = 0;
	else
	{
		i = weaponinfo[p->readyweapon].ammo;
		if (i == am_noammo)
			i = 0;
		else
			i = p->ammo[i];
	}
	
	if (stbar.ammo != i || stbar.forcedraw)
	{
		stbar.ammo = i;
		cmd = &stbarcmds[numstbarcmds++];
		cmd->id = stc_drawammo;
		cmd->value = i;
	}
	
	/* */
	/* Health */
	/* */
	i = p->health;
	if (stbar.health != i || stbar.forcedraw)
	{
		stbar.health = i;
		cmd = &stbarcmds[numstbarcmds++];
		cmd->id = stc_drawhealth;
		cmd->value = i;

		stbar.face = -1;	/* update face immediately */
	}

	/* */
	/* Armor */
	/* */
	i = p->armorpoints;
	if (stbar.armor != i || stbar.forcedraw)
	{
		stbar.armor = i;
		cmd = &stbarcmds[numstbarcmds++];
		cmd->id = stc_drawarmor;
		cmd->value = i;
	}

	/* */
	/* Cards & skulls */
	/* */
	for (ind = 0; ind < NUMCARDS; ind++)
	{
		i = p->cards[ind];
		if (stbar.cards[ind] != i || stbar.forcedraw)
		{
			cmd = &stbarcmds[numstbarcmds++];
			cmd->id = stc_drawcard;
			cmd->ind = ind;
			cmd->value = i;

			stbar.cards[ind] = i;
		}
	}
	
	/* */
	/* Weapons & level */
	/* */
	if (netgame != gt_deathmatch)
	{
		i = gamemapinfo.mapNumber;
		if (stbar.currentMap != i || stbar.forcedraw)
		{
			cmd = &stbarcmds[numstbarcmds++];
			cmd->id = stc_drawmap;
			cmd->value = i;

			stbar.currentMap = i;
		}
		
		for (ind = 0; ind < NUMMICROS; ind++)
		{
			if (p->weaponowned[ind+1] != stbar.weaponowned[ind] || stbar.forcedraw)
			{
				cmd = &stbarcmds[numstbarcmds++];
				cmd->id = stc_drawmicro;
				cmd->ind = ind;
				cmd->value = p->weaponowned[ind + 1];

				stbar.weaponowned[ind] = p->weaponowned[ind+1];
			}
		}
	}
	/* */
	/* Or, frag counts! */
	/* */
	else
	{
		int		yours;
		int		his;
		
		yours = players[consoleplayer].frags;
		his = players[!consoleplayer].frags;
		
		if (yours != stbar.yourFrags || stbar.forcedraw)
		{
			stbar.yourFrags = yours;
			
			/* SIGNAL THE FLASHING FRAGS! */
			yourFrags.active = true;
			yourFrags.delay = FLASHDELAY;
			yourFrags.times = FLASHTIMES;
			yourFrags.doDraw = false;
		}
		
		if (his != stbar.hisFrags || stbar.forcedraw)
		{
			stbar.hisFrags = his;
			
			/* SIGNAL THE FLASHING FRAGS! */
			hisFrags.active = true;
			hisFrags.delay = FLASHDELAY;
			hisFrags.times = FLASHTIMES;
			hisFrags.doDraw = false;
		}
	}
	
	/* */
	/* Draw YOUR FRAGS if it's time */
	/* */
	if (yourFrags.active)
	{
		cmd = &stbarcmds[numstbarcmds++];
		cmd->id = stc_drawyourfrags;
		cmd->ind = yourFrags.doDraw;
		cmd->value = stbar.yourFrags;
	}

	/* */
	/* Draw HIS FRAGS if it's time */
	/* */
	if (hisFrags.active)
	{
		cmd = &stbarcmds[numstbarcmds++];
		cmd->id = stc_drawhisfrags;
		cmd->ind = hisFrags.doDraw;
		cmd->value = stbar.hisFrags;
	}

	if (flashInitialDraw)
	{
		cmd = &stbarcmds[numstbarcmds++];
		cmd->id = stc_flashinitial;
		flashInitialDraw = false;
	}

	/* */
	/* Flash CARDS or SKULLS if no key for door */
	/* */
	for (ind = 0; ind < NUMCARDS; ind++)
		if (flashCards[ind].active)
		{
			cmd = &stbarcmds[numstbarcmds++];
			cmd->id = stc_drawcard;
			cmd->ind = ind;
			cmd->value = flashCards[ind].doDraw;
		}

	/* */
	/* Draw gibbed head */
	/* */
	if (gibdraw)
	{
		// CALICO: the original code performs out-of-bounds accesses on the faces
		// array here, up to an unknown upper bound. This will crash the code
		// when run on PC without bounds-checking added.
		// NB: This is also all bugged anyway, to such a degree that it never shows up.
		// Burger Becky (I assume, or maybe Dave Taylor?) fixed the code that is in
		// the 3DO version so that it appears properly.
		int gibframetodraw = gibframe;
		if (gibdelay-- <= 0)
		{
			gibframe++;
			gibdelay = GIBTIME;
		}

		if (gibframe > 6)
		{
			gibdraw = false;
			stbar.forcedraw = true;
			return;
		}

		cmd = &stbarcmds[numstbarcmds++];
		cmd->id = stc_drawgibhead;
		cmd->value = FIRSTSPLAT + gibframetodraw < NUMFACES ? FIRSTSPLAT + gibframetodraw : NUMFACES - 1;
	}
		
	/*                    */
	/* God mode cheat */
	/* */
	i = p->cheats & CF_GODMODE;
	if (stbar.godmode != i)
		stbar.godmode = i;

	/* */
	/* face change */
	/* */
	drawface = -1;
	if (stbar.godmode)
		drawface = GODFACE;
	else
	if (gibframe > 6)
		stbar.drawface = -1;
	else 
	if (!stbar.health)
		drawface = DEADFACE;
	else
	if (doSpclFace)
	{
		int	base = stbar.health / 20;
		base = base > 4 ? 4 : base;
		base = 4 - base;
		base *= 8;
		drawface = base + spclfaceSprite[spclFaceType];
	}
	else
	if ((stbar.face != newface || stbar.forcedraw) && !gibdraw)
	{
		int	base = stbar.health/20;
		base = base > 4 ? 4 : base;
		base = 4 - base;
		base *= 8;
		stbar.face = newface;
		drawface = base + newface;
	}

	if (drawface != -1)
		if (drawface != stbar.drawface) {
			stbar.drawface = drawface;
			stbar.forcedraw = true;
		}

	if (stbar.forcedraw) {
		cmd = &stbarcmds[numstbarcmds++];
		cmd->id = stc_drawhead;
		cmd->value = stbar.drawface;
	}

	stbar.forcedraw = false;
}

/*
====================
=
= ST_Drawer
=
====================
*/

void ST_Drawer (void)
{
	int i;
	int x, ind;
	boolean have_cards[NUMCARDS];

	if (!sbar || !sbobj[0])
		return;

#ifdef MARS
	stbar_y = I_FrameBufferHeight() - sbar_height;
#else
	stbar_y = 0;
	bufferpage = sbartop;		/* draw into status bar overlay */
#endif

	D_memset(have_cards, 0, sizeof(have_cards));

	for (i = 0; i < numstbarcmds; i++) {
		stbarcmd_t* cmd = &stbarcmds[i];

		switch (cmd->id) {
		case stc_drawammo:
			// CALICO: Fixed EraseBlock call to have proper x coord and width
			ST_EraseBlock(AMMOX - 14 * 3 - 4, AMMOY, 14 * 3 + 4, 16);
			ST_DrawValue(AMMOX, AMMOY, cmd->value);
			break;
		case stc_drawhealth:
			// CALICO: Fixed EraseBlock call to have proper width
			ST_EraseBlock(HEALTHX - 14 * 3 - 4, HEALTHY, 14 * 3 + 4, BIGSHORT(sbobj[0]->height));
			DrawJagobj(sbobj[sb_percent], HEALTHX, stbar_y + HEALTHY);
			ST_DrawValue(HEALTHX, HEALTHY, cmd->value);
			break;
		case stc_drawarmor:
			// CALICO: Fixed EraseBlock call to have proper width
			ST_EraseBlock(ARMORX - 14 * 3 - 4, ARMORY, 14 * 3 + 4, BIGSHORT(sbobj[0]->height));
			DrawJagobj(sbobj[sb_percent], ARMORX, stbar_y + ARMORY);
			ST_DrawValue(ARMORX, ARMORY, cmd->value);
			break;
		case stc_drawcard:
			ind = cmd->ind;
			ST_EraseBlock(KEYX, card_y[ind], KEYW, KEYH);
			have_cards[ind] = cmd->value;
			break;
		case stc_drawmap:
			x = MAPX;
			/* CENTER THE LEVEL # IF < 10 */
			if (cmd->value < 10)
				x -= 6;
			ST_EraseBlock(MAPX - 30, MAPY, 30, 16);
			ST_DrawValue(x, MAPY, cmd->value);
			break;
		case stc_drawmicro:
			if (micronums != -1)
			{
				ind = cmd->ind;
				if (cmd->value)
					DrawJagobjLump(micronums + ind,
						micronums_x[ind], stbar_y + micronums_y[ind], NULL, NULL);
				else
					ST_EraseBlock(micronums_x[ind], micronums_y[ind], 4, 6);
			}
			break;
		case stc_drawyourfrags:
			if (cmd->value)
				ST_DrawValue(yourFrags.x, yourFrags.y, cmd->value);
			else
				ST_EraseBlock(yourFrags.x - yourFrags.w,
					yourFrags.y, yourFrags.w, yourFrags.h);
			break;
		case stc_drawhisfrags:
			if (cmd->value)
				ST_DrawValue(hisFrags.x, hisFrags.y, cmd->value);
			else
				ST_EraseBlock(hisFrags.x - hisFrags.w,
					hisFrags.y, hisFrags.w, hisFrags.h);
			break;
		case stc_flashinitial:
#ifndef MARS
			ST_EraseBlock(yourFrags.x - yourFrags.w, yourFrags.y,
				yourFrags.w, yourFrags.h);
			ST_EraseBlock(hisFrags.x - hisFrags.w, hisFrags.y,
				hisFrags.w, hisFrags.h);
			ST_DrawValue(yourFrags.x, yourFrags.y, stbar.yourFrags);
			ST_DrawValue(hisFrags.x, hisFrags.y, stbar.hisFrags);
#endif
			break;
		case stc_drawgibhead:
		case stc_drawhead:
			ST_EraseBlock(FACEX, FACEY, FACEW, FACEH);
			if (cmd->value != -1)
				DrawJagobjLump(faces + cmd->value, FACEX, stbar_y + FACEY, NULL, NULL);
			break;
		}
	}

	// indicators for keys can erase one other via ST_EraseBlock
	// so draw indicators for keys in posession separately
	for (i = 0; i < NUMCARDS; i++) {
		ind = i;
		if (have_cards[ind])
			DrawJagobj(sbobj[sb_card_b + ind], card_x[ind], stbar_y + card_y[ind]);
	}
}

/*================================================= */
/* */
/* Debugging print */
/* */
/*================================================= */
void ST_Num (int x, int y, int num)
{
	char	str[8];

	NumToStr (num, str);
	I_Print8 (x,y,str+1);
}

/*================================================= */
/* */
/*	Convert an int to a string (my_itoa?) */
/* */
/*================================================= */
void valtostr(char *string,int val)
{
	char temp[10];
	int	index = 0, i, dindex = 0;
	
	do
	{
		temp[index++] = val%10 + '0';
		val /= 10;
	} while(val);
	
	string[index] = 0;
	for (i = index - 1; i >= 0; i--)
		string[dindex++] = temp[i];
}

/*================================================= */
/* */
/* Draws a number in the Status Bar # font */
/* */
/*================================================= */
void ST_DrawValue(int x,int y,int value)
{
	char	v[4];
	int		j;
	int		index;

	y += stbar_y;
	valtostr(v,value);
	j = mystrlen(v) - 1;
	while(j >= 0)
	{
		int w;
		index = sb_0 + (v[j--] - '0');
		w = sbobj[index]->width;
		x -= w + 1;
		DrawJagobj(sbobj[index], x, y);
	}
}

#ifdef MARS

void ST_EraseBlock(int x, int y, int width, int height)
{
	int i, j;
	int rowsize;
	short * source, * dest;

	if (debugmode == 3)
		return;

	if (x & 1)
		x -= 1;
	if (width & 1)
		width += 1;

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	if (x + width > BIGSHORT(sbar->width))
		width = BIGSHORT(sbar->width) - x;
	if (y + height > BIGSHORT(sbar->height))
		height = BIGSHORT(sbar->height) - y;
	rowsize = BIGSHORT(sbar->width) / 2;

	source = (short *)sbar->data + y * rowsize + (unsigned)x/2;

	y += I_FrameBufferHeight() - BIGSHORT(sbar->height);
	if (y > I_FrameBufferHeight())
		height = I_FrameBufferHeight() - y;
	if (height <= 0)
		return;

	dest = (short*)I_FrameBuffer() + y * 320/2 + (unsigned)x/2;

	width = (unsigned)width >> 1;
	for (j = 0; j < height; j++)
	{
		for (i = 0; i < width; i++)
			dest[i] = source[i];
		source += rowsize;
		dest += 320/2;
	}
}

#endif
