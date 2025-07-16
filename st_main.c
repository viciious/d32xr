/* st_main.c -- status bar */

#include "doomdef.h"
#include "st_main.h"

stbar_t	*stbar;
int stbar_tics;

static short stbarframe;
static short   stbar_y;
static short micronums;
static short	micronums_x[NUMMICROS] = {125,137,149,125,137,149};
static short	micronums_y[NUMMICROS] = {15,15,15,25,25,25};

static short	card_x[NUMCARDS] = {KEYX,KEYX,KEYX,KEYX+3, KEYX+3, KEYX+3};
static short	card_y[NUMCARDS] = {BLUKEYY,YELKEYY,REDKEYY,BLUKEYY,YELKEYY,REDKEYY};

static short	spclfaceSprite[NUMSPCLFACES] =
		{0,sbf_facelft,sbf_facergt,sbf_ouch,sbf_gotgat,sbf_mowdown};

jagobj_t	*sbar;
VINT		sbar_height;

#ifndef MARS
byte		*sbartop;
#endif
static short		faces;
static jagobj_t	*sbobj[NUMSBOBJ];

#ifndef MARS
#define ST_EraseBlock EraseBlock
#endif

static void ST_EraseBlock(int x, int y, int width, int height);

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

	stbar = Z_Malloc(sizeof(*stbar)*MAXPLAYERS, PU_STATIC);

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

	l = W_CheckNumForName("MICRO_0");
	micronums = l;
}

void ST_ForceDraw(void)
{
	int p;

	stbarframe = 0;
	for (p = 0; p < MAXPLAYERS; p++)
	{
		if (playeringame[p])
			stbar[p].forcedraw = true;
	}
	ST_Ticker();
}

/*================================================== */
/* */
/*  Init this stuff EVERY LEVEL */
/* */
/*================================================== */
void ST_InitEveryLevel(void)
{
	int		i, p;
	int		numplayers;

	stbarframe = 0;

	numplayers = 0;
	for (i = 0; i < MAXPLAYERS; i++)
		numplayers += playeringame[i] ? 1 : 0;

	D_memset(stbar, 0, sizeof(stbar[0])*MAXPLAYERS);

	for (p = 0; p < numplayers; p++)
	{
		stbar_t* sb = &stbar[p];

		/* force everything to be updated on next ST_Update */
		sb->forcedraw = true;
		sb->drawface = -1;

		/* DRAW FRAG COUNTS INITIALLY */
		{
			sb->yourFrags.active = false;
			sb->yourFrags.x = YOURFRAGX;
			sb->yourFrags.y = YOURFRAGY;
			sb->yourFrags.w = 28;
			sb->yourFrags.h = 16;
			sb->hisFrags.active = false;
			sb->hisFrags.x = HISFRAGX;
			sb->hisFrags.y = HISFRAGY;
			sb->hisFrags.w = 30;
			sb->hisFrags.h = 8;
			sb->yourFragsCount = players[p].frags;
			sb->hisFragsCount = players[!p].frags;
		}
		sb->gibdelay = GIBTIME;
		sb->specialFace = f_none;
		sb->flashInitialDraw = true;

		for (i = 0; i < NUMCARDS; i++)
		{
			sb->flashCards[i].x = KEYX + (i > 2 ? 3 : 0);
			sb->flashCards[i].y = card_y[i];
			sb->flashCards[i].w = KEYW;
			sb->flashCards[i].h = KEYH;
		}

		for (i = 0; i < NUMAMMO; i++)
		{
			sb->ammocount[i] = sb->maxammocount[i] = -1;
		}
	}
	stbar_tics = 0;
}

/*
====================
=
= ST_Ticker
=
====================
*/

static void ST_Ticker_(stbar_t* sb)
{
	int			i;
	player_t* p;
	int			ind;
	int			pnum = sb - &stbar[0];
	short		drawface;
	stbarcmd_t* cmd;

#ifdef MARS
	// double-buffered renderer on MARS
	if ((stbarframe & 1) == 1)
	{
		--sb->facetics;
		--sb->yourFrags.delay;
		--sb->hisFrags.delay;
		for (ind = 0; ind < NUMCARDS; ind++)
		{
			if (!sb->flashCards[ind].active)
				continue;
			--sb->flashCards[ind].delay;
		}
		if (sb->gibdraw)
			--sb->gibdelay;
		return;
	}
#endif

	sb->numstbarcmds = 0;

	p = &players[pnum];

	/* */
	/* Animate face */
	/* */
	if (--sb->facetics <= 0)
	{
		sb->facetics = M_Random ()&15;
		sb->newface = M_Random ()&3;
		if (sb->newface == 3)
			sb->newface = 1;
		sb->doSpclFace = false;
	}
	
	/* */
	/* Draw special face? */
	/* */
	if (sb->specialFace)
	{
		sb->doSpclFace = true;
		sb->spclFaceType = sb->specialFace;
		sb->facetics = 15;
		sb->specialFace = f_none;
	}
	
	/* */
	/* Flash YOUR FRAGS amount */
	/* */
	if (sb->yourFrags.active && --sb->yourFrags.delay <= 0)
	{
		sb->yourFrags.delay = FLASHDELAY;
		sb->yourFrags.doDraw ^= 1;
		if (!--sb->yourFrags.times)
			sb->yourFrags.active = false;
		if (sb->yourFrags.doDraw && sb->yourFrags.active && !splitscreen)
			S_StartSound(NULL,sfx_itemup);
	}
	
	/* */
	/* Flash HIS FRAGS amount */
	/* */
	if (sb->hisFrags.active && --sb->hisFrags.delay <= 0)
	{
		sb->hisFrags.delay = FLASHDELAY;
		sb->hisFrags.doDraw ^= 1;
		if (!--sb->hisFrags.times)
			sb->hisFrags.active = false;
		if (sb->hisFrags.doDraw && sb->hisFrags.active && !splitscreen)
			S_StartSound(NULL,sfx_itemup);
	}

	/* */
	/* Did we get gibbed? */
	/* */
	if (sb->gotgibbed && !sb->gibdraw)
	{
		sb->gibdraw = true;
		sb->gibframe = 0;
		sb->gibdelay = GIBTIME;
		sb->gotgibbed = false;
	}
	
	/* */
	/* Tried to open a CARD or SKULL door? */
	/* */
	for (ind = 0; ind < NUMCARDS; ind++)
	{
		/* CHECK FOR INITIALIZATION */
		if (sb->tryopen[ind])
		{
			sb->tryopen[ind] = false;
			sb->flashCards[ind].active = true;
			sb->flashCards[ind].delay = FLASHDELAY;
			sb->flashCards[ind].times = FLASHTIMES+1;
			sb->flashCards[ind].doDraw = false;
		}
		
		/* MIGHT AS WELL DO TICKING IN THE SAME LOOP! */
		if (!sb->flashCards[ind].active)
			continue;
		if (--sb->flashCards[ind].delay <= 0)
		{
			sb->flashCards[ind].delay = FLASHDELAY;
			sb->flashCards[ind].doDraw ^= 1;
			if (!--sb->flashCards[ind].times)
				sb->flashCards[ind].active = false;
			if (sb->flashCards[ind].doDraw && sb->flashCards[ind].active)
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
	
	if (sb->ammo != i || sb->forcedraw)
	{
		sb->ammo = i;
		cmd = &sb->stbarcmds[sb->numstbarcmds++];
		cmd->id = stc_drawammo;
		cmd->value = i;
	}
	
	/* */
	/* Health */
	/* */
	i = p->health;
	if (sb->health != i || sb->forcedraw)
	{
		if (i > sb->health)
		{
			sb->gibframe = 0;
			sb->gibdraw = false;
		}

		sb->health = i;
		cmd = &sb->stbarcmds[sb->numstbarcmds++];
		cmd->id = stc_drawhealth;
		cmd->value = i;

		sb->face = -1;	/* update face immediately */
	}

	/* */
	/* Armor */
	/* */
	i = p->armorpoints;
	if (sb->armor != i || sb->forcedraw)
	{
		sb->armor = i;
		cmd = &sb->stbarcmds[sb->numstbarcmds++];
		cmd->id = stc_drawarmor;
		cmd->value = i;
	}

	/* */
	/* Cards & skulls */
	/* */
	for (ind = 0; ind < NUMCARDS; ind++)
	{
		i = p->cards[ind];
		if (sb->cards[ind] != i || sb->forcedraw)
		{
			cmd = &sb->stbarcmds[sb->numstbarcmds++];
			cmd->id = stc_drawcard;
			cmd->ind = ind;
			cmd->value = i;

			sb->cards[ind] = i;
		}
	}

	/* */
	/* Weapons */
	/* */
	for (ind = 0; ind < NUMMICROS; ind++)
	{
		int weaponowned = ind + 1 == wp_shotgun ? 
			(p->weaponowned[wp_shotgun] || p->weaponowned[wp_supershotgun]) :
				(ind + 1 < wp_shotgun ? p->weaponowned[ind + 1] : p->weaponowned[ind + 2]);

		if (weaponowned != sb->weaponowned[ind] || sb->forcedraw)
		{
			cmd = &sb->stbarcmds[sb->numstbarcmds++];
			cmd->id = stc_drawmicro;
			cmd->ind = ind;
			cmd->value = weaponowned;
			sb->weaponowned[ind] = weaponowned;
		}
	}

	/* */
	/* Level */
	/* */
	if (netgame != gt_deathmatch)
	{
		for (ind = 0; ind < NUMAMMO; ind++)
		{
			if (sb->ammocount[ind] != p->ammo[ind] || sb->maxammocount[ind] != p->maxammo[ind] || sb->forcedraw)
			{
				cmd = &sb->stbarcmds[sb->numstbarcmds++];
				cmd->id = stc_drawammocount;
				cmd->ind = ind;
				sb->ammocount[ind] = p->ammo[ind];
				sb->maxammocount[ind] = p->maxammo[ind];
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

		yours = players[pnum].frags;
		his = players[!pnum].frags;
		
		if (yours != sb->yourFragsCount || sb->forcedraw)
		{
			sb->yourFragsCount = yours;
			
			/* SIGNAL THE FLASHING FRAGS! */
			sb->yourFrags.active = true;
			sb->yourFrags.delay = FLASHDELAY;
			sb->yourFrags.times = FLASHTIMES;
			sb->yourFrags.doDraw = false;
		}
		
		if (his != sb->hisFragsCount || sb->forcedraw)
		{
			sb->hisFragsCount = his;
			
			/* SIGNAL THE FLASHING FRAGS! */
			sb->hisFrags.active = true;
			sb->hisFrags.delay = FLASHDELAY;
			sb->hisFrags.times = FLASHTIMES;
			sb->hisFrags.doDraw = false;
		}
	}
	
	/* */
	/* Draw YOUR FRAGS if it's time */
	/* */
	if (sb->yourFrags.active)
	{
		cmd = &sb->stbarcmds[sb->numstbarcmds++];
		cmd->id = stc_drawyourfrags;
		cmd->ind = sb->yourFrags.doDraw;
		cmd->value = sb->yourFragsCount;
	}

	/* */
	/* Draw HIS FRAGS if it's time */
	/* */
	if (sb->hisFrags.active)
	{
		cmd = &sb->stbarcmds[sb->numstbarcmds++];
		cmd->id = stc_drawhisfrags;
		cmd->ind = sb->hisFrags.doDraw;
		cmd->value = sb->hisFragsCount;
	}

	if (sb->flashInitialDraw)
	{
		cmd = &sb->stbarcmds[sb->numstbarcmds++];
		cmd->id = stc_flashinitial;
		sb->flashInitialDraw = false;
	}

	/* */
	/* Flash CARDS or SKULLS if no key for door */
	/* */
	for (ind = 0; ind < NUMCARDS; ind++)
		if (sb->flashCards[ind].active)
		{
			cmd = &sb->stbarcmds[sb->numstbarcmds++];
			cmd->id = stc_drawcard;
			cmd->ind = ind;
			cmd->value = sb->flashCards[ind].doDraw;
		}

	/* */
	/* Draw gibbed head */
	/* */
	if (sb->gibdraw)
	{
		// CALICO: the original code performs out-of-bounds accesses on the faces
		// array here, up to an unknown upper bound. This will crash the code
		// when run on PC without bounds-checking added.
		// NB: This is also all bugged anyway, to such a degree that it never shows up.
		// Burger Becky (I assume, or maybe Dave Taylor?) fixed the code that is in
		// the 3DO version so that it appears properly.
		int gibframetodraw = sb->gibframe;
		if (sb->gibdelay-- <= 0)
		{
			sb->gibframe++;
			sb->gibdelay = GIBTIME;
		}

		if (sb->gibframe > 6)
		{
			sb->gibdraw = false;
			sb->forcedraw = true;
			return;
		}

		cmd = &sb->stbarcmds[sb->numstbarcmds++];
		cmd->id = stc_drawgibhead;
		cmd->value = FIRSTSPLAT + gibframetodraw < NUMFACES ? FIRSTSPLAT + gibframetodraw : NUMFACES - 1;
	}
		
	/*                    */
	/* God mode cheat */
	/* */
	i = (p->cheats & CF_GODMODE || p->powers[pw_invulnerability]) != 0;
	if (sb->godmode != i)
		sb->godmode = i;

	/* */
	/* face change */
	/* */
	drawface = -1;
	if (sb->godmode)
		drawface = GODFACE;
	else
	if (sb->gibframe > 6)
		sb->drawface = -1;
	else 
	if (!sb->health)
		drawface = DEADFACE;
	else
	if (sb->doSpclFace)
	{
		int	base = sb->health / 20;
		base = base > 4 ? 4 : base;
		base = 4 - base;
		base *= 8;
		drawface = base + spclfaceSprite[sb->spclFaceType];
	}
	else
	if ((sb->face != sb->newface || sb->forcedraw) && !sb->gibdraw)
	{
		int	base = sb->health/20;
		base = base > 4 ? 4 : base;
		base = 4 - base;
		base *= 8;
		sb->face = sb->newface;
		drawface = base + sb->newface;
	}

	if (drawface != -1)
		if (drawface != sb->drawface) {
			sb->drawface = drawface;
			sb->forcedraw = true;
		}

	if (sb->forcedraw) {
		cmd = &sb->stbarcmds[sb->numstbarcmds++];
		cmd->id = stc_drawhead;
		cmd->value = sb->drawface;
	}

	sb->forcedraw = false;
}

void ST_Ticker(void)
{
	int	p = splitscreen ? 0 : consoleplayer;
	int e = splitscreen ? MAXPLAYERS : consoleplayer + 1;

	while (p < e)
	{
		if (playeringame[p])
			ST_Ticker_(&stbar[p]);
		p++;
	}

	stbarframe++;
	stbar_tics++;
}

/*
====================
=
= ST_Drawer
=
====================
*/

static void ST_Drawer_ (stbar_t* sb)
{
	int i;
	int x, y, ind;
	boolean have_cards[NUMCARDS];
	char ammochars[NUMAMMO] = { 14, 12, 16, 15 };
	char ammoorder[NUMAMMO] = { 0, 1, 3, 2 };

	if (!sbar || !sbobj[0])
		return;

	D_memset(have_cards, 0, sizeof(have_cards));

	for (i = 0; i < sb->numstbarcmds; i++) {
		stbarcmd_t* cmd = &sb->stbarcmds[i];

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
			ST_EraseBlock(KEYX, card_y[ind], KEYW+3, KEYH);
			have_cards[ind] = cmd->value;
			break;
		case stc_drawammocount:
			ind = cmd->ind;

			x = STATSX - 32;
			y = STATSY + ammoorder[ind] * 7;
			ST_EraseBlock(x, y, 38, 7);

			DrawJagobjLump(micronums + ammochars[ind], x, stbar_y + y, NULL, NULL);
			ST_DrawMicroValue(x + 18, y, sb->ammocount[ind]);
			DrawJagobjLump(micronums + 13, x + 20, stbar_y + y, NULL, NULL);
			ST_DrawMicroValue(x + 20 + 18, y, sb->maxammocount[ind]);
			break;
		case stc_drawmicro:
			if (micronums != -1)
			{
				ind = cmd->ind;
				if (cmd->value)
					DrawJagobjLump(micronums + ind + 2,
						micronums_x[ind], stbar_y + micronums_y[ind], NULL, NULL);
				else
					ST_EraseBlock(micronums_x[ind], micronums_y[ind], 4, 6);
			}
			break;
		case stc_drawyourfrags:
			ST_EraseBlock(STATSX - 30, STATSY, 14 * 3 + 4, 28);
			ST_DrawValue(sb->yourFrags.x, sb->yourFrags.y, cmd->value);
#ifndef MARS
			break;
		case stc_drawhisfrags:
			ST_EraseBlock(sb->hisFrags.x,
				sb->hisFrags.y, sb->hisFrags.w, sb->hisFrags.h);
			ST_Num(sb->hisFrags.x, sb->hisFrags.y, cmd->value);
			break;
		case stc_flashinitial:
			ST_EraseBlock(yourFrags.x - yourFrags.w, yourFrags.y,
				yourFrags.w, yourFrags.h);
			ST_EraseBlock(hisFrags.x - hisFrags.w, hisFrags.y,
				hisFrags.w, hisFrags.h);
			ST_DrawValue(yourFrags.x, yourFrags.y, sb->yourFragsCount);
			ST_DrawValue(hisFrags.x, hisFrags.y, sb->hisFragsCount);
#endif
			break;
		case stc_drawgibhead:
		case stc_drawhead:
			ST_EraseBlock(FACEX + 2, FACEY, FACEW, FACEH);
			if (cmd->value != -1)
				DrawJagobjLump(faces + cmd->value, FACEX + 2, stbar_y + FACEY, NULL, NULL);
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


void ST_Drawer(void)
{
	int	p = splitscreen ? 0 : consoleplayer;
	int e = splitscreen ? MAXPLAYERS : consoleplayer + 1;
	int y[MAXPLAYERS];

	if (debugmode == DEBUGMODE_NODRAW)
		return;

	y[consoleplayer] = I_FrameBufferHeight() - sbar_height;
	y[consoleplayer^1] = 0;

	while (p < e)
	{
#ifdef MARS
		stbar_y = y[p];
#else
		stbar_y = 0;
		bufferpage = sbartop;		/* draw into status bar overlay */
#endif

		if (playeringame[p])
			ST_Drawer_(&stbar[p]);
		p++;
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

	y += stbar_y;
	NumToStr (num, str);
	I_Print8 (x,y/8,str+1);
}

/*================================================= */
/* */
/*	Convert an int to a string (my_itoa?) */
/* */
/*================================================= */
void valtostr(char *string,int val)
{
	char temp[12];
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

	if (value < 0)
		value = 0;
	if (value > 999)
		value = 999;

	y += stbar_y;
	valtostr(v,value);
	v[sizeof(v)-1] = 0;

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

void ST_DrawMicroValue(int x,int y,int value)
{
	char	v[4];
	int		j;
	int		index;

	if (micronums < 0)
		return;

	if (value < 0)
		value = 0;
	if (value > 999)
		value = 999;

	y += stbar_y;
	valtostr(v,value);
	v[sizeof(v)-1] = 0;

	j = mystrlen(v) - 1;
	while(j >= 0)
	{
		int w;
		index = micronums + (v[j--] - '0');
		w = 4;
		x -= w;
		DrawJagobjLump(index, x, y, NULL, NULL);
	}
}

#ifdef MARS

void ST_EraseBlock(int x, int y, int width, int height)
{
	if (debugmode == DEBUGMODE_NODRAW)
		return;

	if (x & 1)
		x -= 1;
	if (width & 1)
		width -= 1;

	DrawJagobj2(sbar, x, stbar_y + y, x, y, width, height, I_FrameBuffer());
}

#endif
