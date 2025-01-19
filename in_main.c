
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
	boolean 	ticking;

	boolean		exit;
	boolean		statsdrawn;
	boolean		valsdrawn;
	boolean		negativefrag[MAXPLAYERS];
	int			killvalue[2], itemvalue[2], secretvalue[2], fragvalue[2];
	int 		timevalue;
	int 		exittic;

	jagobj_t	*i_secret, *i_percent, *i_level, *i_kills,
					*i_items, *i_finish, *i_frags, *i_par, *i_time;

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
		else if (c == '.')
		{
			DrawJagobjLump(uchar + 'z' - 'a' + 27, x, y + 4, &w, NULL);
			x = (x + (w + 1)) & ~1;
		}
		else if (c == '!')
		{
			DrawJagobjLump(uchar + 'z' - 'a' + 28, x, y + 4, &w, NULL);
			x = (x + (w + 1)) & ~1;
		}
		else if (c == '-')
		{
			DrawJagobjLump(uchar + 'z' - 'a' + 29, x, y + 4, &w, NULL);
			x = (x + (w + 1)) & ~1;
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

	if(interm->exit == true)
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
		const char *mapname = DMAPINFO_STRFIELD(gamemapinfo, name);
		const char *nextmapname = interm->nextmapinfo ? DMAPINFO_STRFIELD(interm->nextmapinfo, name) : "";

		if (!interm->exit)
		{
			if (*mapname != '\0')
			{
				length = mystrlen(mapname);
				print((320 - (length * 14)) >> 1, 10, mapname);
			}
		}

		if (*nextmapname != '\0')
		{
			length = mystrlen("Entering");
			print((320 - (length * 14)) >> 1, 162, "Entering");
			length = mystrlen(nextmapname);
			print((320 - (length * 14)) >> 1, 182, nextmapname);
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
				print(54, FVALY + 40, "Their Frags");
			}
		}
		else
		{
			print (28, 50, "Player");
			print (KVALX - 14, 50, "1");
			print (KVALX + 80 - 14, 50, "2");
			DrawJagobj(interm->i_kills, 57, KVALY);
	 
			DrawJagobj(interm->i_items, 51, IVALY);
	 	
			DrawJagobj(interm->i_secret, 13, SVALY);
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
		DrawJagobj(interm->i_percent, KVALX, KVALY);
		DrawJagobj(interm->i_percent, KVALX + 80, KVALY);
		DrawJagobj(interm->i_percent, IVALX, IVALY);
		DrawJagobj(interm->i_percent, IVALX + 80, IVALY);
		DrawJagobj(interm->i_percent, SVALX, SVALY);
		DrawJagobj(interm->i_percent, SVALX + 80, SVALY);
	}
}

/* */
/* Single intermision */
/* */
void IN_SingleDrawer(void)
{
	int length;
	
	if(interm->exit == true)
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
		boolean countdown = interm->exit && ticon >= interm->exittic + 30;
		const char *mapname = DMAPINFO_STRFIELD(gamemapinfo, name);
		const char *nextmapname = interm->nextmapinfo ? DMAPINFO_STRFIELD(interm->nextmapinfo, name) : "";

		if (*nextmapname == '\0')
			nextmapname = "NEXT LEVEL";

		if (countdown)
		{
			int sec = 0;
			sec = (unsigned)(ticon - (interm->exittic + 30)) / 16;
			if (!(sec & 1))
			{
				length = mystrlen("Entering");
				print( (320 - (length * 14)) >> 1, 10, "Entering");
				length = mystrlen(nextmapname);
				print( (320 - (length*14)) >> 1, 34, nextmapname);
			}
			return;
		}

		if (*mapname != '\0')
		{
			length = mystrlen(mapname);
			print((320 - (length * 14)) >> 1, 10, mapname);
			length = mystrlen("Finished");
			print((320 - (length * 14)) >> 1, 34, "Finished");
		}

		DrawJagobj(interm->i_kills, 71, KVALY - 10);

		DrawJagobj(interm->i_items, 65, IVALY - 10);

		DrawJagobj(interm->i_secret, 27, SVALY - 10);

		print(73, TVALY - 10, "Time");
	}
	
	IN_DrawValue(KVALX + 60, KVALY - 10, interm->killvalue[0]);
	DrawJagobj(interm->i_percent, KVALX + 60, KVALY - 10);
	IN_DrawValue(IVALX + 60, IVALY - 10, interm->itemvalue[0]);		
	DrawJagobj(interm->i_percent, IVALX + 60, IVALY - 10);
	IN_DrawValue(SVALX + 60, SVALY - 10, interm->secretvalue[0]);
	DrawJagobj(interm->i_percent, SVALX + 60, SVALY - 10);

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
	VINT lumps[7];
	lumpinfo_t li[7];

	interm = Z_Malloc(sizeof(*interm), PU_STATIC);
	D_memset(interm, 0, sizeof(*interm));

	interm->exit = false;

	interm->valsdrawn = false;

	if (gameaction == ga_secretexit && gamemapinfo->secretNext)
		interm->nextmapinfo = G_MapInfoForLumpName(DMAPINFO_STRFIELD(gamemapinfo, secretNext));
	else if (gamemapinfo->next)
		interm->nextmapinfo = G_MapInfoForLumpName(DMAPINFO_STRFIELD(gamemapinfo, next));

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

#ifdef MARS
	Z_FreeTags (mainzone);

	W_LoadPWAD(PWAD_CD);

	/* build a temp in-memory PWAD */
	lumps[0] = - 1;
	if (netgame != gt_deathmatch)
	{
		if (gameaction == ga_secretexit && gamemapinfo->secretInterPic)
			lumps[0] = W_CheckNumForName(DMAPINFO_STRFIELD(gamemapinfo, secretInterPic));
		else if (gamemapinfo->interPic)
			lumps[0] = W_CheckNumForName(DMAPINFO_STRFIELD(gamemapinfo, interPic));
	}

	if (lumps[0] == -1)
		lumps[0] = W_CheckNumForName("INTERPIC");
	lumps[1] = W_CheckNumForName("I_SECRET");
	lumps[2] = W_CheckNumForName("PERCENT");
	lumps[3] = W_CheckNumForName("I_LEVEL");
	lumps[4] = W_CheckNumForName("I_KILLS");
	lumps[5] = W_CheckNumForName("I_ITEMS");
	lumps[6] = W_CheckNumForName("I_FINISH");

	W_CacheWADLumps(li, 7, lumps, true);

	l = -1;
	if (netgame != gt_deathmatch)
	{
		if (gameaction == ga_secretexit && gamemapinfo->secretInterPic)
			l = W_CheckNumForName(DMAPINFO_STRFIELD(gamemapinfo, secretInterPic));
		else if (gamemapinfo->interPic)
			l = W_CheckNumForName(DMAPINFO_STRFIELD(gamemapinfo, interPic));
	}

	if (l == -1)
		l = W_CheckNumForName("INTERPIC");
	if (l != -1)
		interm->interpic = W_CacheLumpNum(l, PU_STATIC);
	else
		interm->interpic = NULL;
#endif

/* cache all needed graphics */
#ifndef MARS
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));
#endif
	interm->i_secret = W_CacheLumpName("I_SECRET", PU_STATIC);
	interm->i_percent = W_CacheLumpName("PERCENT", PU_STATIC);
	interm->i_level = W_CacheLumpName("I_LEVEL", PU_STATIC);
	interm->i_kills = W_CacheLumpName("I_KILLS", PU_STATIC);
	interm->i_items = W_CacheLumpName("I_ITEMS", PU_STATIC);
	interm->i_finish = W_CacheLumpName("I_FINISH", PU_STATIC);
#ifndef MARS
	i_frags = W_CacheLumpNum("I_FRAGS");
#endif

#ifdef MARS
	W_LoadPWAD(PWAD_NONE);
#endif

	snums = W_CheckNumForName("NUM_0");

	uchar = W_CheckNumForName("CHAR_065");

#ifndef MARS
	DoubleBufferSetup ();
#endif

	I_SetPalette(W_POINTLUMPNUM(W_GetNumForName("PLAYPALS")));

	S_StartSongByName(gameinfo.intermissionMus, 1, gameinfo.intermissionCdTrack);
}

void IN_Stop (void)
{	
	if (interm->interpic)
		Z_Free(interm->interpic);
	if (interm->i_secret)
		Z_Free(interm->i_secret);
	if (interm->i_percent)
		Z_Free(interm->i_percent);
	if (interm->i_level)
		Z_Free(interm->i_level);
	if (interm->i_kills)
		Z_Free(interm->i_kills);
	if (interm->i_items)
		Z_Free(interm->i_items);
	if (interm->i_finish)
		Z_Free(interm->i_finish);

	Z_Free(interm);
	D_memset(interm, 0, sizeof(interm));

	//S_StopSong();
}

int IN_Ticker (void)
{
	int		buttons;
	int		oldbuttons;		
	int 		i;
	boolean ticking = false;

	if (!interm->i_secret)
		return 1;
	if (ticon < 5)
		return 0;		/* don't exit immediately */

	if (interm->exit)
	{
		if (ticon > interm->exittic + 100)
			return 1;
		return 0;
	}

	for (i=0 ; i<= (netgame > gt_single) ; i++)
	{
		buttons = players[i].ticbuttons;
		oldbuttons = players[i].oldticbuttons;
		
	/* exit menu if button press */
		if ( (buttons & BT_ATTACK) && !(oldbuttons & BT_ATTACK) )
		{
			if (!interm->exit && interm->ticking)
				S_StartSound(0, sfx_barexp);
			interm->exit = true;
			interm->exittic = ticon;
			return 0;		/* done with intermission */
		}
		if ( (buttons & BT_SPEED) && !(oldbuttons & BT_SPEED) )
		{
			if (!interm->exit && interm->ticking)
				S_StartSound(0, sfx_barexp);
			interm->exit = true;
			interm->exittic = ticon;
			return 0;		/* done with intermission */
		}
		if ( (buttons & BT_USE) && !(oldbuttons & BT_USE) )
		{
			if (!interm->exit && interm->ticking)
				S_StartSound(0, sfx_barexp);
			interm->exit = true;
			interm->exittic = ticon;
			return 0;		/* done with intermission */
		}
	}
	
	if (netgame == gt_deathmatch)
		return 0;

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
			{
				killvalue[i]+=2;
				ticking = true;
			}
			if (killvalue[i] > pstats[i].killpercent)
				killvalue[i] = pstats[i].killpercent;

			if (itemvalue[i] < pstats[i].itempercent )
			{
				itemvalue[i]+=2;
				ticking = true;
			}
			if (itemvalue[i] > pstats[i].itempercent)
				itemvalue[i] = pstats[i].itempercent;

			if (secretvalue[i] < pstats[i].secretpercent )
			{
				secretvalue[i]+=2;
				ticking = true;
			}
			if (secretvalue[i] > pstats[i].secretpercent)
				secretvalue[i] = pstats[i].secretpercent;

			if (fragvalue[i] < pstats[i].fragcount)
			{
				fragvalue[i]+=2;
				ticking = true;
			}
			if (fragvalue[i] > pstats[i].fragcount)
				fragvalue[i] = pstats[i].fragcount;
		}
	}

	if (!(ticon & 3))
	{
		if (ticking)
			S_StartSound(0, sfx_pistol);
		else if (interm->ticking)
			S_StartSound(0, sfx_barexp);
		interm->ticking = ticking;
	}

	return 0;
}

void IN_Drawer (void)
{	
#ifdef MARS
	if (interm->interpic != NULL)
	{
		DrawJagobj(interm->interpic, 0, 0);
		DrawFillRect(0, BIGSHORT(interm->interpic->height), 320, 224-BIGSHORT(interm->interpic->height), 0);
	}
	else
		DrawTiledBackground();
#endif

	if (!interm->i_secret)
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

