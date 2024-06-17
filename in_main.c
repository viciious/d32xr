
/* in_main.c -- intermission */
#include "doomdef.h"
#include "st_main.h"

#define	KVALX			172
#define	KVALY			80
#define	IVALX			172
#define	IVALY			100
#define	SVALX			172
#define	SVALY			120
#define TVALX           172
#define TVALY           140
#define	FVALX			230
#define	FVALY			74

#define	PLAYERONEFACEX		137
#define	PLAYERONEFACEY		30
#define	PLAYERTWOFACEX		217
#define	PLAYERTWOFACEY		30

extern int nextmap;

typedef struct pstats_s
{
	int		killpercent;
	int		itempercent;
	int		secretpercent;
	int		fragcount;
} pstats_t;

typedef enum
{
	f_lookfront,
	f_lookright,
	f_lookleft,
	f_ohshit,
	f_splatone,
	f_splattwo,
	f_splatthree,
	f_splatfour,
	f_splatfive,
	f_splatsix
} faces_t;

typedef struct
{
	pstats_t	pstats[MAXPLAYERS];
	faces_t		facenum;

	boolean		earlyexit;
	boolean		statsdrawn;
	boolean		valsdrawn;
	boolean		negativefrag[MAXPLAYERS];
	int			killvalue[2], itemvalue[2], secretvalue[2], fragvalue[2];
	int 		timevalue;

	VINT		i_secret, i_percent, i_level, i_kills,
					i_items, i_finish, i_frags, i_par, i_time;

	VINT		infaces[10];
	jagobj_t 	*interpic;

	dmapinfo_t	*nextmapinfo;
} intermission_t;

VINT	uchar, snums;
static intermission_t *interm;

/* */
/* Lame-o print routine */
/* */
void print (int x, int y, const char *string)
{
	int i,c;
	int w;

	for (i = 0; i < mystrlen(string); i++)
	{
		c = string[i];
	
		if (c >= 22 && c <= 125)
		{
			DrawJagobjLump(uchar + (c - 'A'), x, y, &w, NULL);
			x = (x + (w + 1)) & ~1;
		}
		else // Space
		{
			x = (x + 7) & ~1;
			continue;
		}
	}
}

/* */
/* Draws 'value' at x, y  */
/* */
void IN_DrawValue(int x,int y,int value)
{
	char	v[4];
	int		j;
	int		index;
	
	valtostr(v,value);
	j = mystrlen(v) - 1;
	while(j >= 0)
	{
		int w;

		index = (v[j--] - '0');
		DrawJagobjLump(snums + index, 320, 200, &w, NULL);

		x -= w+2;
		DrawJagobjLump(snums + index, x, y, NULL, NULL);
	}
}

/* */
/* Draws 'value' at x, y  */
/* */
void IN_DrawPadValue(int x,int y,int value,int minl)
{
	char	v[4];
	int		j;
	int		index;

	valtostr(v,value);
	j = mystrlen(v) - 1;
	while(j >= 0)
	{
		int w;

		index = (v[j--] - '0');
		DrawJagobjLump(snums + index, 320, 200, &w, NULL);

		x -= w+2;
		DrawJagobjLump(snums + index, x, y, NULL, NULL);

		minl--;
	}

	while (minl > 0)
	{
		int w;

		DrawJagobjLump(snums + 0, 320, 200, &w, NULL);

		x -= w+2;
		DrawJagobjLump(snums + 0, x, y, NULL, NULL);

		minl--;
	}
}

/* */
/* Network intermission */
/* */
void IN_NetgameDrawer(void)
{	
	int		i;
	int length;

	if(interm->earlyexit == true)
		for (i = 0; i < MAXPLAYERS; i++)
		{
			interm->killvalue[i] = interm->pstats[i].killpercent;
			interm->itemvalue[i] = interm->pstats[i].itempercent;
			interm->secretvalue[i] = interm->pstats[i].secretpercent;
			interm->fragvalue[i] = 
				players[i].frags;
		}

#ifndef MARS
	if (statsdrawn == false)
#endif
	{
		if (gamemapinfo.name != NULL)
		{
			length = mystrlen(gamemapinfo.name);
			print((320 - (length * 14)) >> 1, 10, gamemapinfo.name);
		}

		if (interm->nextmapinfo && interm->nextmapinfo->name != NULL)
		{
			length = mystrlen("Entering");
			print((320 - (length * 14)) >> 1, 162, "Entering");
			length = mystrlen(interm->nextmapinfo->name);
			print((320 - (length * 14)) >> 1, 182, interm->nextmapinfo->name);
		}

		if (netgame == gt_deathmatch)
		{
			if (splitscreen)
			{
				print(30, FVALY, "Player 1 Frags");
				print(30, FVALY + 40, "Player 2 Frags");
			}
			else
			{
				print(30, FVALY, "Your Frags");
				print(54, FVALY + 40, "His Frags");
			}
		}
		else
		{
			print (28, 50, "Player");
			print (KVALX - 18, 50, "1");
			print (KVALX + 66, 50, "2");
			DrawJagobjLump(interm->i_kills, 57, 80, NULL, NULL);
	 
			DrawJagobjLump(interm->i_items, 51, 110, NULL, NULL);
	 	
			DrawJagobjLump(interm->i_secret, 13, 140, NULL, NULL);
		}
	}			
	
	if (netgame == gt_deathmatch)
	{
		if (splitscreen)
		{
			EraseBlock(30 + (mystrlen("Player 1 Frags") * 15), FVALY, 80, 80);
			IN_DrawValue(FVALX + 40, FVALY, interm->fragvalue[0]);
			IN_DrawValue(FVALX + 40, FVALY + 40, interm->fragvalue[1]);
		}
		else
		{
			EraseBlock(30 + (mystrlen("Your Frags") * 15), FVALY, 80, 80);
			IN_DrawValue(FVALX, FVALY, interm->fragvalue[consoleplayer]);
			IN_DrawValue(FVALX, FVALY + 40, interm->fragvalue[!consoleplayer]);
		}
	}
	else
	{
		EraseBlock(57 + (mystrlen("Kills") * 15), KVALY, 100, 100);
		IN_DrawValue(KVALX, KVALY, interm->killvalue[consoleplayer]);
		IN_DrawValue(KVALX + 80, KVALY, interm->killvalue[!consoleplayer]);
		IN_DrawValue(IVALX, IVALY, interm->itemvalue[consoleplayer]);
		IN_DrawValue(IVALX + 80, IVALY, interm->itemvalue[!consoleplayer]);
		IN_DrawValue(SVALX, SVALY, interm->secretvalue[consoleplayer]);
		IN_DrawValue(SVALX + 80, SVALY, interm->secretvalue[!consoleplayer]);
		DrawJagobjLump(interm->i_percent, KVALX, KVALY, NULL, NULL);
		DrawJagobjLump(interm->i_percent, KVALX + 80, KVALY, NULL, NULL);
		DrawJagobjLump(interm->i_percent, IVALX, IVALY, NULL, NULL);
		DrawJagobjLump(interm->i_percent, IVALX + 80, IVALY, NULL, NULL);
		DrawJagobjLump(interm->i_percent, SVALX, SVALY, NULL, NULL);
		DrawJagobjLump(interm->i_percent, SVALX + 80, SVALY, NULL, NULL);
	}
}

/* */
/* Single intermision */
/* */
void IN_SingleDrawer(void)
{
	int length;
	
	if(interm->earlyexit == true)
	{
		interm->killvalue[0] = interm->pstats[0].killpercent;
		interm->itemvalue[0] = interm->pstats[0].itempercent;
		interm->secretvalue[0] = interm->pstats[0].secretpercent;
	}
		
	EraseBlock(71 + (mystrlen("Secrets") * 15), 70, 55, 80);

#ifndef MARS
	if (statsdrawn == false)
#endif
	{
		if (gamemapinfo.name != NULL)
		{
			length = mystrlen(gamemapinfo.name);
			print((320 - (length * 14)) >> 1, 10, gamemapinfo.name);
			length = mystrlen("Finished");
			print((320 - (length * 14)) >> 1, 34, "Finished");
		}

		if (interm->nextmapinfo && interm->nextmapinfo->name != NULL)
		{
			length = mystrlen("Entering");
			print( (320 - (length * 14)) >> 1, 162, "Entering");	
			length = mystrlen(interm->nextmapinfo->name);
			print( (320 - (length*14)) >> 1, 182, interm->nextmapinfo->name);
		}

		DrawJagobjLump(interm->i_kills, 71, KVALY - 10, NULL, NULL);

		DrawJagobjLump(interm->i_items, 65, IVALY - 10, NULL, NULL);

		DrawJagobjLump(interm->i_secret, 27, SVALY - 10, NULL, NULL);

		print(73, TVALY - 10, "Time");
	}
	
	IN_DrawValue(KVALX + 60, KVALY - 10, interm->killvalue[0]);
	DrawJagobjLump(interm->i_percent, KVALX + 60, KVALY - 10, NULL, NULL);
	IN_DrawValue(IVALX + 60, IVALY - 10, interm->itemvalue[0]);		
	DrawJagobjLump(interm->i_percent, IVALX + 60, IVALY - 10, NULL, NULL);
	IN_DrawValue(SVALX + 60, SVALY - 10, interm->secretvalue[0]);
	DrawJagobjLump(interm->i_percent, SVALX + 60, SVALY - 10, NULL, NULL);

	if (interm->timevalue/60 > 0)
	{
		IN_DrawValue(TVALX + 8, TVALY - 10, interm->timevalue/60);
		print(TVALX + 8, TVALY - 10 - 1, "m");
	}
	IN_DrawPadValue(TVALX + 58, TVALY - 10, interm->timevalue%60, 2);
	print(TVALX + 58, TVALY - 10 - 1, "s");
}

void IN_Start (void)
{	
	int	i,l;
	VINT *infaces;

	interm = Z_Malloc(sizeof(*interm), PU_STATIC);
	D_memset(interm, 0, sizeof(*interm));

	interm->earlyexit = false;

	interm->valsdrawn = false;

	interm->nextmapinfo = Z_Malloc(sizeof(dmapinfo_t), PU_STATIC);
	D_memset(interm->nextmapinfo, 0, sizeof(dmapinfo_t));

	if (gameaction == ga_secretexit && gamemapinfo.secretNext)
		G_FindMapinfo(gamemapinfo.secretNext, interm->nextmapinfo, NULL);
	else if (gamemapinfo.next)
		G_FindMapinfo(gamemapinfo.next, interm->nextmapinfo, NULL);

	D_memset(interm->killvalue, 0, sizeof(*interm->killvalue)*MAXPLAYERS);
	D_memset(interm->itemvalue, 0, sizeof(*interm->itemvalue)*MAXPLAYERS);
	D_memset(interm->secretvalue, 0, sizeof(*interm->secretvalue)*MAXPLAYERS);
	D_memset(interm->fragvalue, 0, sizeof(*interm->fragvalue)*MAXPLAYERS);

	for (i = 0; i < MAXPLAYERS; i++) 
	{
		pstats_t *pstats = interm->pstats;

		if (totalkills)
			pstats[i].killpercent = (players[i].killcount * 100) / totalkills;
		else
			pstats[i].killpercent = 100;
		if (totalitems)
			pstats[i].itempercent = (players[i].itemcount * 100) / totalitems;
		else
			pstats[i].itempercent = 100;
		if (totalsecret)
			pstats[i].secretpercent = (players[i].secretcount * 100) / totalsecret;
		else
			pstats[i].secretpercent = 100;
			
		if(netgame)
			pstats[i].fragcount = players[i].frags;
	}	

	interm->timevalue = stbar_tics/TICRATE;

	infaces = interm->infaces;

/* cache all needed graphics */
#ifndef MARS
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
#endif
	interm->i_secret = W_CheckNumForName ("I_SECRET");
	interm->i_percent = W_CheckNumForName("PERCENT");
	interm->i_level = W_CheckNumForName("I_LEVEL");
	interm->i_kills = W_CheckNumForName("I_KILLS");
	interm->i_items = W_CheckNumForName("I_ITEMS");
	interm->i_finish = W_CheckNumForName("I_FINISH");
#ifndef MARS
	i_frags = W_CheckNumForName("I_FRAGS");
#endif
	infaces[0] = W_CheckNumForName("FACE00");
	infaces[1]	= W_CheckNumForName("FACE01");
	infaces[2] = W_CheckNumForName("FACE02");
	infaces[3] = W_CheckNumForName("FACE05");
	infaces[4] = W_CheckNumForName("STSPLAT0");
	infaces[5] = W_CheckNumForName("STSPLAT1");
	infaces[6] = W_CheckNumForName("STSPLAT2");
	infaces[7] = W_CheckNumForName("STSPLAT3");
	infaces[8] = W_CheckNumForName("STSPLAT4");
	infaces[9] = W_CheckNumForName("STSPLAT5");
	
#ifdef MARS
	Z_FreeTags (mainzone);
	l = W_CheckNumForName("INTERPIC");
	if (l != -1)
		interm->interpic = W_CacheLumpNum(l, PU_STATIC);
	else
		interm->interpic = NULL;
#endif

	snums = W_CheckNumForName("STTNUM0");

	uchar = W_CheckNumForName("STCFN065");

#ifndef MARS
	DoubleBufferSetup ();
#endif

	I_SetPalette(W_POINTLUMPNUM(W_GetNumForName("PLAYPALS")));

	S_StartSong(gameinfo.intermissionMus, 1, cdtrack_intermission);
}

void IN_Stop (void)
{	
	if (interm->nextmapinfo)
	{
		if (interm->nextmapinfo->data)
			Z_Free(interm->nextmapinfo->data);
		Z_Free(interm->nextmapinfo);
	}
	if (interm->interpic)
	{
		Z_Free(interm->interpic);
	}

	Z_Free(interm);
	interm = NULL;

	S_StopSong();
}

int IN_Ticker (void)
{
	int		buttons;
	int		oldbuttons;		
	int 		i;

	if (interm->i_secret < 0)
		return 1;
	if (ticon < 5)
		return 0;		/* don't exit immediately */
	
	for (i=0 ; i<= (netgame > gt_single) ; i++)
	{
		buttons = ticbuttons[i];
		oldbuttons = oldticbuttons[i];
		
	/* exit menu if button press */
		if ( (buttons & BT_ATTACK) && !(oldbuttons & BT_ATTACK) )
		{
			interm->earlyexit = true;
			return 1;		/* done with intermission */
		}
		if ( (buttons & BT_SPEED) && !(oldbuttons & BT_SPEED) )
		{
			interm->earlyexit = true;
			return 1;		/* done with intermission */
		}
		if ( (buttons & BT_USE) && !(oldbuttons & BT_USE) )
		{
			interm->earlyexit = true;
			return 1;		/* done with intermission */
		}
	}
	
	for (i = 0; i < MAXPLAYERS; i++)
	{
		int *killvalue = interm->killvalue;
		int *itemvalue = interm->itemvalue;
		int *secretvalue = interm->secretvalue;
		int *fragvalue = interm->fragvalue;
		pstats_t *pstats = interm->pstats;

		if (interm->valsdrawn == true)
		{
			if (killvalue[i] < pstats[i].killpercent )
				killvalue[i]+=2;
			if (killvalue[i] > pstats[i].killpercent)
				killvalue[i] = pstats[i].killpercent;

			if (itemvalue[i] < pstats[i].itempercent )
				itemvalue[i]+=2;
			if (itemvalue[i] > pstats[i].itempercent)
				itemvalue[i] = pstats[i].itempercent;

			if (secretvalue[i] < pstats[i].secretpercent )
				secretvalue[i]+=2;
			if (secretvalue[i] > pstats[i].secretpercent)
				secretvalue[i] = pstats[i].secretpercent;

			if (fragvalue[i] < pstats[i].fragcount)
				fragvalue[i]+=2;
			if (fragvalue[i] > pstats[i].fragcount)
				fragvalue[i] = pstats[i].fragcount;
		}
	}

	return 0;
}

void IN_Drawer (void)
{	
#ifdef MARS
	if (interm->interpic != NULL)
		DrawJagobj(interm->interpic, 0, 0);
	else
		DrawTiledBackground();
#endif

	if (interm->i_secret < 0)
		return;

	if (netgame != gt_single)
		IN_NetgameDrawer();	
	else
		IN_SingleDrawer();

	if(interm->statsdrawn == false)
		interm->statsdrawn = true;
	if(interm->valsdrawn == false)
		interm->valsdrawn = true;
}

