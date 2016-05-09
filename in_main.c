
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

char mapnames[][30] =
{
	"Hangar",
	"Plant",
	"Toxin Refinery",
	"Command Control",
	"Phobos Lab",
	"Central Processing",
	"Computer Station",
	"Phobos Anomaly",
	"Deimos Anomaly",
	"Containment Area",
	"Refinery",
	"Deimos Lab",
	"Command Center",
	"Halls of the Damned",
	"Spawning Vats",
	"Tower of Babel",
	"Hell Keep",
	"Pandemonium",
	"House of Pain",
	"Unholy Cathedral",
	"Mt. Erebus",
	"Limbo",
	"Dis",
	"Military Base"
/*	"Fortress of Mystery", */
/*	"Warrens" */
};

pstats_t		pstats[MAXPLAYERS];
faces_t		facenum;

boolean		earlyexit;
boolean		statsdrawn;
boolean		valsdrawn;
boolean		negativefrag[MAXPLAYERS];
int			killvalue[2], itemvalue[2], secretvalue[2], fragvalue[2];
int			myticcount, myoldticcount;

jagobj_t		*i_secret, *i_percent, *i_level, *i_kills,
				*i_items, *i_finish, *i_frags, *i_par, *i_time;

jagobj_t		*snums[10];
jagobj_t		*infaces[10];
jagobj_t		*uchar[52];

/* */
/* Lame-o print routine */
/* */
void print (int x, int y, char *string)
{
	int i,c;

	for (i = 0; i < mystrlen(string); i++)
	{
		c = string[i];
	
		if (c >= 'A' && c <= 'Z')
		{
			DrawJagobj(uchar[c-'A'],x,y);
			x+= uchar[c-'A']->width;
		}
		else if (c >= 'a' && c <= 'z')
		{
			DrawJagobj(uchar[c-'a' + 26],x,y+4);
			x+= uchar[c-'a' + 26]->width;
		}
		else if (c >= '0' && c <= '9')
		{
			DrawJagobj(snums[c-48],x,y);
			x+= snums[c-48]->width + 1;
		}
		else
		{
			x+=6;
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
		index = (v[j--] - '0');
		x -= snums[index]->width+2;
		DrawJagobj(snums[index], x, y);
	}
}

/* */
/* Network intermission */
/* */
void IN_NetgameDrawer(void)
{	
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
			DrawJagobj(i_kills, 57, 80);				
	 
			DrawJagobj(i_items, 51, 110);				
	 	
			DrawJagobj(i_secret, 13, 140);		
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
		DrawJagobj(i_percent, KVALX, KVALY);
		DrawJagobj(i_percent, KVALX + 80, KVALY);
		DrawJagobj(i_percent, IVALX, IVALY);
		DrawJagobj(i_percent, IVALX + 80, IVALY);
		DrawJagobj(i_percent, SVALX, SVALY);
		DrawJagobj(i_percent, SVALX + 80, SVALY);
	}	
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

	if (statsdrawn == false)
	{
		length = mystrlen(mapnames[gamemap - 1]);
		print( (320 - (length * 14)) >> 1 , 10, mapnames[gamemap - 1]);
		length = mystrlen("Finished");
		print( (320 - (length * 14)) >> 1, 34, "Finished");
	
		if (nextmap != 23)
		{
			length = mystrlen("Entering");
			print( (320 - (length * 14)) >> 1, 162, "Entering");	
			length = mystrlen(mapnames[nextmap - 1]);
			print( (320 - (length*14)) >> 1, 182, mapnames[nextmap - 1]);
		}
				
		DrawJagobj(i_kills, 71, 70);				

		DrawJagobj(i_items, 65, 100);				

		DrawJagobj(i_secret, 27, 130);		
	}
	
	IN_DrawValue(KVALX + 60, KVALY - 10, killvalue[0]);
	DrawJagobj(i_percent, KVALX + 60, KVALY - 10);
	IN_DrawValue(IVALX + 60, IVALY - 10, itemvalue[0]);		
	DrawJagobj(i_percent, IVALX + 60, IVALY - 10);
	IN_DrawValue(SVALX + 60, SVALY - 10, secretvalue[0]);
	DrawJagobj(i_percent, SVALX + 60, SVALY - 10);	

}

void IN_Start (void)
{	
	int	i,l;

	earlyexit = false;

	valsdrawn = false;

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
	backgroundpic = W_POINTLUMPNUM(W_GetNumForName("M_TITLE"));

	i_secret = W_CacheLumpName ("I_SECRET",PU_STATIC);
	i_percent = W_CacheLumpName ("PERCENT",PU_STATIC);
	i_level = W_CacheLumpName ("I_LEVEL",PU_STATIC);
	i_kills = W_CacheLumpName ("I_KILLS",PU_STATIC);
	i_items = W_CacheLumpName ("I_ITEMS",PU_STATIC);
	i_finish = W_CacheLumpName ("I_FINISH",PU_STATIC);
	i_frags = W_CacheLumpName ("I_FRAGS",PU_STATIC);
	infaces[0] = W_CacheLumpName ("FACE00",PU_STATIC);
	infaces[1]	= W_CacheLumpName ("FACE01",PU_STATIC);
	infaces[2] = W_CacheLumpName ("FACE02",PU_STATIC);
	infaces[3] = W_CacheLumpName ("FACE05",PU_STATIC);
	infaces[4] = W_CacheLumpName ("STSPLAT0",PU_STATIC);
	infaces[5] = W_CacheLumpName ("STSPLAT1",PU_STATIC);
	infaces[6] = W_CacheLumpName ("STSPLAT2",PU_STATIC);
	infaces[7] = W_CacheLumpName ("STSPLAT3",PU_STATIC);
	infaces[8] = W_CacheLumpName ("STSPLAT4",PU_STATIC);
	infaces[9] = W_CacheLumpName ("STSPLAT5",PU_STATIC);

	
	l = W_GetNumForName ("NUM_0");
	for (i = 0; i < 10; i++)
		snums[i] = W_CacheLumpNum(l+i, PU_STATIC);

	l = W_GetNumForName ("CHAR_065");
	for (i = 0; i < 52; i++)
		uchar[i] = W_CacheLumpNum(l+i, PU_STATIC);
		
	DoubleBufferSetup ();

	S_StartSong((gamemap%10)+1, 1);
}

void IN_Stop (void)
{	
#if 0
	int	i;

	Z_Free(i_secret);
	Z_Free(i_percent);
	Z_Free(i_level);
	Z_Free(i_kills);
	Z_Free(i_items);
	Z_Free(i_finish);
	Z_Free(i_frags);
			
	for (i = 0; i < 10; i++)
		Z_Free(snums[i]);

	for (i = 0; i < 52; i++)
		Z_Free(uchar[i]);

	for (i = 4; i < 10; i++)
		Z_Free(infaces[i]);
#endif

	statsdrawn = false;
	valsdrawn = false;

	S_StopSong();

}

int IN_Ticker (void)
{
	int		buttons;
	int		oldbuttons;		
	int 		i;
				
	if (ticon < 5)
		return 0;		/* don't exit immediately */
		
	for (i=0 ; i<= (netgame > gt_single) ; i++)
	{
		buttons = ticbuttons[i];
		oldbuttons = oldticbuttons[i];
		
	/* exit menu if button press */
		if ( (buttons & JP_A) && !(oldbuttons & JP_A) )
		{
			earlyexit = true;
			return 1;		/* done with intermission */
		}
		if ( (buttons & JP_B) && !(oldbuttons & JP_B) )
		{
			earlyexit = true;
			return 1;		/* done with intermission */
		}
		if ( (buttons & JP_C) && !(oldbuttons & JP_C) )
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
	if (netgame != gt_single)
		IN_NetgameDrawer();	
	else
		IN_SingleDrawer();

	if(statsdrawn == false)
		statsdrawn = true;
	if(valsdrawn == false)
		valsdrawn = true;

	UpdateBuffer ();
} 


