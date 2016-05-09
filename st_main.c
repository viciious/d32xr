/* st_main.c -- status bar */

#include "doomdef.h"
#include "st_main.h"

stbar_t	stbar;
jagobj_t *micronums[NUMMICROS];
int		micronums_x[NUMMICROS] = {249,261,272,249,261,272};
int		micronums_y[NUMMICROS] = {15,15,15,25,25,25};

int		facetics;
int		newface;
int		card_x[NUMCARDS] = {KEYX,KEYX,KEYX,KEYX+3, KEYX+3, KEYX+3};
int		card_y[NUMCARDS] = {BLUKEYY,YELKEYY,REDKEYY,BLUKEYY,YELKEYY,REDKEYY};

boolean flashInitialDraw;		/* INITIALLY DRAW FRAG AMOUNTS (flag) */
sbflash_t	yourFrags;			/* INFO FOR YOUR FRAG FLASHING */
sbflash_t	hisFrags;

boolean	gibdraw;
int		gibframe;
int		gibdelay;

int		spclfaceSprite[NUMSPCLFACES] = 
		{0,sbf_facelft,sbf_facergt,sbf_ouch,sbf_gotgat,sbf_mowdown};
boolean doSpclFace;
spclface_e	spclFaceType;

jagobj_t	*sbar;
byte		*sbartop;
jagobj_t	*faces[NUMFACES];
jagobj_t	*sbobj[NUMSBOBJ];

sbflash_t	flashCards[NUMCARDS];	/* INFO FOR FLASHING CARDS & SKULLS */

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
	int		i,l;

	l = W_GetNumForName ("FACE00");
	for (i=0 ; i<NUMFACES ; i++)
		faces[i] = W_CacheLumpNum (l+i, PU_STATIC);
		
	l = W_GetNumForName ("MINUS");
	for (i = 0; i < NUMSBOBJ; i++)
		sbobj[i] = W_CacheLumpNum(l+i, PU_STATIC);

	l = W_GetNumForName ("MICRO_2");
	for (i = 0; i < NUMMICROS; i++)
		micronums[i] = W_CacheLumpNum(l+i, PU_STATIC);
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
	D_memset (&stbar, 0x80, sizeof(stbar) );
	facetics = 0;

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

void ST_Ticker (void)
{
	int		ind;
	
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
	if (yourFrags.active && !--yourFrags.delay)
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
	if (hisFrags.active && !--hisFrags.delay)
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
		if (flashCards[ind].active && !--flashCards[ind].delay)
		{
			flashCards[ind].delay = FLASHDELAY;
			flashCards[ind].doDraw ^= 1;
			if (!--flashCards[ind].times)
				flashCards[ind].active = false;
			if (flashCards[ind].doDraw && flashCards[ind].active)
				S_StartSound(NULL,sfx_itemup);
		}
	}
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
	int			i;
	int			ind;
	player_t	*p;
	
	bufferpage = sbartop;		/* draw into status bar overlay */
	
	p = &players[consoleplayer];
	
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
	
	if (stbar.ammo != i)
	{
		stbar.ammo = i;
		EraseBlock(0,AMMOY,14*3,16);
		ST_DrawValue(AMMOX,AMMOY,i);
	}
	
	/* */
	/* Health */
	/* */
	i = p->health;
	if (stbar.health != i)
	{
		stbar.health = i;
		EraseBlock(HEALTHX - 14*3 - 4,HEALTHY,14*3,(sbobj[0])->height);
		DrawJagobj(sbobj[sb_percent],HEALTHX,HEALTHY);
		ST_DrawValue(HEALTHX,HEALTHY,i);
		stbar.face = -1;	/* update face immediately */
	}

	/* */
	/* Armor */
	/* */
	i = p->armorpoints;
	if (stbar.armor != i)
	{
		stbar.armor = i;
		EraseBlock(ARMORX - 14*3 - 4,ARMORY,14*3,(sbobj[0])->height);
		DrawJagobj(sbobj[sb_percent],ARMORX,ARMORY);
		ST_DrawValue(ARMORX,ARMORY,i);
	}

	/* */
	/* Cards & skulls */
	/* */
	for (ind = 0; ind < NUMCARDS; ind++)
	{
		i = p->cards[ind];
		if (stbar.cards[ind] != i)
		{
			stbar.cards[ind] = i;
			EraseBlock(KEYX,card_y[ind],KEYW,KEYH);
			if (stbar.cards[ind])
				DrawJagobj(sbobj[sb_card_b + ind],card_x[ind],card_y[ind]);
		}
	}
	
	/* */
	/* Weapons & level */
	/* */
	if (netgame != gt_deathmatch)
	{
		i = gamemap;
		if (stbar.currentMap != i)
		{
			int	x = MAPX;
			stbar.currentMap = i;
			/* CENTER THE LEVEL # IF < 10 */
			if (stbar.currentMap < 10)
				x -= 6;
			EraseBlock(MAPX - 30,MAPY,30,16);
			ST_DrawValue(x,MAPY,i);
		}
		
		for (ind = 0; ind < NUMMICROS; ind++)
		{
			if (p->weaponowned[ind+1] != stbar.weaponowned[ind])
			{
				stbar.weaponowned[ind] = p->weaponowned[ind+1];
				if (stbar.weaponowned[ind])
					DrawJagobj(micronums[ind],
						micronums_x[ind],micronums_y[ind]);
				else
					EraseBlock(micronums_x[ind],micronums_y[ind],4,6);
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
		
		if (yours != stbar.yourFrags)
		{
			stbar.yourFrags = yours;
			
			/* SIGNAL THE FLASHING FRAGS! */
			yourFrags.active = true;
			yourFrags.delay = FLASHDELAY;
			yourFrags.times = FLASHTIMES;
			yourFrags.doDraw = false;
		}
		
		if (his != stbar.hisFrags)
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
		if (yourFrags.doDraw)
			ST_DrawValue(yourFrags.x,yourFrags.y,stbar.yourFrags);
		else
			EraseBlock(yourFrags.x - yourFrags.w,
				yourFrags.y,yourFrags.w,yourFrags.h);
	}
	
	/* */
	/* Draw HIS FRAGS if it's time */
	/* */
	if (hisFrags.active)
	{
		if (hisFrags.doDraw)
			ST_DrawValue(hisFrags.x,hisFrags.y,stbar.hisFrags);
		else
			EraseBlock(hisFrags.x - hisFrags.w,
				hisFrags.y,hisFrags.w,hisFrags.h);
	}
	
	if (flashInitialDraw)
	{
		flashInitialDraw = false;
		EraseBlock(yourFrags.x - yourFrags.w,yourFrags.y,
			yourFrags.w,yourFrags.h);
		EraseBlock(hisFrags.x - hisFrags.w,hisFrags.y,
			hisFrags.w,hisFrags.h);
		ST_DrawValue(yourFrags.x,yourFrags.y,stbar.yourFrags);
		ST_DrawValue(hisFrags.x,hisFrags.y,stbar.hisFrags);
	}
	
	/* */
	/* Flash CARDS or SKULLS if no key for door */
	/* */
	for (ind = 0; ind < NUMCARDS; ind++)
		if (flashCards[ind].active)
		{
			if (flashCards[ind].doDraw)
				DrawJagobj(sbobj[sb_card_b + ind],
					flashCards[ind].x,flashCards[ind].y);
			else
				EraseBlock(flashCards[ind].x,flashCards[ind].y,
					flashCards[ind].w,flashCards[ind].h);
		}
	
	/* */
	/* Draw gibbed head */
	/* */
	if (gibdraw && !--gibdelay)
	{
		DrawJagobj(faces[FIRSTSPLAT + gibframe++],FACEX,FACEY);
		gibdelay = GIBTIME;
		if (gibframe > 6)
			gibdraw = false;
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
	if (stbar.godmode)
		DrawJagobj(faces[GODFACE],FACEX,FACEY);
	else
	if (!stbar.health)
		DrawJagobj(faces[DEADFACE],FACEX,FACEY);
	else
	if (doSpclFace)
	{
		int	base = stbar.health / 20;
		base = base > 4 ? 4 : base;
		base = 4 - base;
		base *= 8;
		DrawJagobj(faces[base + spclfaceSprite[spclFaceType]],FACEX,FACEY);
	}
	else
	if ((stbar.face != newface) && !gibdraw)
	{
		int	base = stbar.health/20;
		base = base > 4 ? 4 : base;
		base = 4 - base;
		base *= 8;
		stbar.face = newface;
		DrawJagobj (faces[base + newface], FACEX, FACEY);
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
	
	valtostr(v,value);
	j = mystrlen(v) - 1;
	while(j >= 0)
	{
		index = sb_0 + (v[j--] - '0');
		x -= (sbobj[index])->width + 1;
		DrawJagobj(sbobj[index],x,y);
	}
}
