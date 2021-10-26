
/* in_main.c -- intermission */
#include "doomdef.h"
#include "st_main.h"

#define	KVALX			172
#define	KVALY			80
#define	IVALX			172
#define	IVALY			110	
#define	SVALX			172
#define	SVALY			140
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

pstats_t		pstats[MAXPLAYERS];
faces_t		facenum;

boolean		earlyexit;
boolean		statsdrawn;
boolean		valsdrawn;
boolean		negativefrag[MAXPLAYERS];
int			killvalue[2], itemvalue[2], secretvalue[2], fragvalue[2];
int			myticcount, myoldticcount;

VINT		i_secret, i_percent, i_level, i_kills,
				i_items, i_finish, i_frags, i_par, i_time;

VINT		snums;
VINT		infaces[10];
VINT		uchar;

static dmapinfo_t	*nextmapinfo;

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
	
		if (c >= 'A' && c <= 'Z')
		{
			DrawJagobjLump(uchar + c - 'A', x, y, &w, NULL);
			x = (x + (w + 1)) & ~1;
		}
		else if (c >= 'a' && c <= 'z')
		{
			DrawJagobjLump(uchar + c - 'a' + 26, x, y + 4, &w, NULL);
			x = (x + (w + 1)) & ~1;
		}
		else if (c >= '0' && c <= '9')
		{
			DrawJagobjLump(snums + c - 48, x, y, &w, NULL);
			x = (x + (w + 2)) & ~1;
		}
		else
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
/* Network intermission */
/* */
void IN_NetgameDrawer(void)
{	
#ifndef MARS
	int		i;
	
	if(earlyexit == true)
		for (i = 0; i < MAXPLAYERS; i++)
		{
			killvalue[i] = pstats[i].killpercent;
			itemvalue[i] = pstats[i].itempercent;
			secretvalue[i] = pstats[i].secretpercent;
			fragvalue[i] = 
				players[i].frags;
		}
		
			
	if (statsdrawn == false)
	{
		if (netgame == gt_deathmatch)
		{
			print(30 , FVALY, "Your Frags");
			print(54 , FVALY + 40, "His Frags");
			IN_DrawValue(FVALX, FVALY, fragvalue[consoleplayer]);
			IN_DrawValue(FVALX, FVALY + 40, fragvalue[!consoleplayer]);
		}
		else
		{
			print (28, 50, "Player");
			print (KVALX - 18, 50, "1");
			print (KVALX + 66, 50, "2");
			DrawJagobjLump(i_kills, 57, 80, NULL, NULL);
	 
			DrawJagobjLump(i_items, 51, 110, NULL, NULL);
	 	
			DrawJagobjLump(i_secret, 13, 140, NULL, NULL);
		}
	}			
	
	if (netgame == gt_deathmatch)
	{
		EraseBlock(30 + (mystrlen("Your Frags") * 15), FVALY,  80, 80);
		IN_DrawValue(FVALX, FVALY, fragvalue[consoleplayer]);
		IN_DrawValue(FVALX, FVALY + 40, fragvalue[!consoleplayer]);
	}
	else
	{
		EraseBlock(57 + (mystrlen("Kills") * 15), KVALY, 100, 100);
		IN_DrawValue(KVALX, KVALY, killvalue[consoleplayer]);
		IN_DrawValue(KVALX + 80, KVALY, killvalue[!consoleplayer]);
		IN_DrawValue(IVALX, IVALY, itemvalue[consoleplayer]);
		IN_DrawValue(IVALX + 80, IVALY, itemvalue[!consoleplayer]);
		IN_DrawValue(SVALX, SVALY, secretvalue[consoleplayer]);
		IN_DrawValue(SVALX + 80, SVALY, secretvalue[!consoleplayer]);
		DrawJagobjLump(i_percent, KVALX, KVALY, NULL, NULL);
		DrawJagobjLump(i_percent, KVALX + 80, KVALY, NULL, NULL);
		DrawJagobjLump(i_percent, IVALX, IVALY, NULL, NULL);
		DrawJagobjLump(i_percent, IVALX + 80, IVALY, NULL, NULL);
		DrawJagobjLump(i_percent, SVALX, SVALY, NULL, NULL);
		DrawJagobjLump(i_percent, SVALX + 80, SVALY, NULL, NULL);
	}	
#endif
}

/* */
/* Single intermision */
/* */
void IN_SingleDrawer(void)
{
	int length;
	
	if(earlyexit == true)
	{
		killvalue[0] = pstats[0].killpercent;
		itemvalue[0] = pstats[0].itempercent;
		secretvalue[0] = pstats[0].secretpercent;
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

		if (nextmapinfo && nextmapinfo->name != NULL)
		{
			length = mystrlen("Entering");
			print( (320 - (length * 14)) >> 1, 162, "Entering");	
			length = mystrlen(nextmapinfo->name);
			print( (320 - (length*14)) >> 1, 182, nextmapinfo->name);
		}

		DrawJagobjLump(i_kills, 71, 70, NULL, NULL);

		DrawJagobjLump(i_items, 65, 100, NULL, NULL);

		DrawJagobjLump(i_secret, 27, 130, NULL, NULL);
	}
	
	IN_DrawValue(KVALX + 60, KVALY - 10, killvalue[0]);
	DrawJagobjLump(i_percent, KVALX + 60, KVALY - 10, NULL, NULL);
	IN_DrawValue(IVALX + 60, IVALY - 10, itemvalue[0]);		
	DrawJagobjLump(i_percent, IVALX + 60, IVALY - 10, NULL, NULL);
	IN_DrawValue(SVALX + 60, SVALY - 10, secretvalue[0]);
	DrawJagobjLump(i_percent, SVALX + 60, SVALY - 10, NULL, NULL);
}

void IN_Start (void)
{	
#ifdef MARS
	int	i;
#else
	int	i,l;
#endif
	earlyexit = false;

	valsdrawn = false;

	nextmapinfo = Z_Malloc(sizeof(*nextmapinfo), PU_STATIC, 0);
	D_memset(nextmapinfo, 0, sizeof(*nextmapinfo));

	if (gameaction == ga_secretexit && gamemapinfo.secretNext)
		G_FindMapinfo(gamemapinfo.secretNext, nextmapinfo, NULL);
	else if (gamemapinfo.next)
		G_FindMapinfo(gamemapinfo.next, nextmapinfo, NULL);

	for (i = 0; i < MAXPLAYERS; i++) 
	{
		killvalue[i] = itemvalue[i] = secretvalue[i] = fragvalue[i] = 0;
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

/* cache all needed graphics */
#ifndef MARS
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
#endif
	i_secret = W_CheckNumForName ("I_SECRET");
	i_percent = W_CheckNumForName("PERCENT");
	i_level = W_CheckNumForName("I_LEVEL");
	i_kills = W_CheckNumForName("I_KILLS");
	i_items = W_CheckNumForName("I_ITEMS");
	i_finish = W_CheckNumForName("I_FINISH");
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
	
	snums = W_CheckNumForName("NUM_0");

	uchar = W_CheckNumForName("CHAR_065");

	DoubleBufferSetup ();

	I_SetPalette(W_POINTLUMPNUM(W_GetNumForName("PLAYPALS")));

	S_StartSong(gameinfo.intermissionMus, 1, cdtrack_intermission);
}

void IN_Stop (void)
{	
	if (nextmapinfo)
	{
		if (nextmapinfo->data)
			Z_Free(nextmapinfo->data);
		Z_Free(nextmapinfo);
	}
	nextmapinfo = NULL;

	statsdrawn = false;
	valsdrawn = false;

	S_StopSong();

}

int IN_Ticker (void)
{
	int		buttons;
	int		oldbuttons;		
	int 		i;

	if (i_secret < 0)
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
			earlyexit = true;
			return 1;		/* done with intermission */
		}
		if ( (buttons & BT_SPEED) && !(oldbuttons & BT_SPEED) )
		{
			earlyexit = true;
			return 1;		/* done with intermission */
		}
		if ( (buttons & BT_USE) && !(oldbuttons & BT_USE) )
		{
			earlyexit = true;
			return 1;		/* done with intermission */
		}
	}
	
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (valsdrawn == true)
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
	DrawTiledBackground();
#endif

	if (i_secret < 0)
		return;

	if (netgame != gt_single)
		IN_NetgameDrawer();	
	else
		IN_SingleDrawer();

	if(statsdrawn == false)
		statsdrawn = true;
	if(valsdrawn == false)
		valsdrawn = true;

	UpdateBuffer();
}

