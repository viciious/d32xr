/* R_data.c */

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"

int		firstflat, lastflat, numflats;

int			numtextures;
texture_t	*textures;

VINT			*flattranslation;		/* for global animation */
VINT			*texturetranslation;	/* for global animation */

void			**flatpixels;

texture_t	*skytexturep;

/*============================================================================ */

/*
==================
=
= R_InitTextures
=
= Initializes the texture list with the textures from the world map
=
==================
*/

	int			*maptex;

void R_InitTextures (void)
{
	maptexture_t	*mtexture;
	texture_t		*texture;
	int			i,j,c;
	int			offset;
	int			*directory;
	

/* */
/* load the map texture definitions from textures.lmp */
/* */
	maptex = W_CacheLumpName ("TEXTURE1", PU_STATIC);
	numtextures = LITTLELONG(*maptex);
	directory = maptex+1;
	
	if (numtextures > MAXTEXTURES)
		I_Error("numtextures == %d", numtextures);

	textures = Z_Malloc (numtextures * sizeof(*textures), PU_STATIC, 0);
	for (i=0 ; i<numtextures ; i++, directory++)
	{
		offset = LITTLELONG(*directory);
		mtexture = (maptexture_t *) ( (byte *)maptex + offset);
		texture = &textures[i];
		texture->width = LITTLESHORT(mtexture->width);
		texture->height = LITTLESHORT(mtexture->height);
		D_memcpy (texture->name, mtexture->name, 8);
		for (j=0 ; j<8 ; j++)
		{
			c = texture->name[j];
			if (c >= 'a' && c<='z')
				texture->name[j] = c - ('a'-'A');
		}
		texture->data = NULL;		/* not cached yet */
		texture->lumpnum = W_CheckNumForName (texture->name);
if (texture->lumpnum == -1)
	texture->lumpnum = 0;
	}

	Z_Free (maptex);
	
/* */
/* translation table for global animation */
/* */
	texturetranslation = Z_Malloc ((numtextures+1)*sizeof(*texturetranslation), PU_STATIC, 0);
	for (i=0 ; i<numtextures ; i++)
		texturetranslation[i] = i;	
}


/*
================
=
= R_InitFlats
=
=================
*/

void R_InitFlats (void)
{
	int		i;
	
	firstflat = W_GetNumForName ("F_START") + 1;
	lastflat = W_GetNumForName ("F_END") - 1;
	numflats = lastflat - firstflat + 1;
	
/* translation table for global animation */
	flattranslation = Z_Malloc ((numflats+1)*sizeof(*flattranslation), PU_STATIC, 0);
	for (i=0 ; i<numflats ; i++)
		flattranslation[i] = i;

	flatpixels = Z_Malloc(numflats * sizeof(*flatpixels), PU_STATIC, 0);
}

/*
================
=
= R_InitData
=
= Locates all the lumps that will be used by all views
= Must be called after W_Init
=================
*/

void R_InitData (void)
{
	R_InitTextures ();
	R_InitFlats ();
}


/*============================================================================= */

/*
================
=
= R_FlatNumForName
=
================
*/

#ifdef i386
#define HIBIT (1<<7)
#else
#define HIBIT (1<<31)
#endif
void strupr (char *s);

int	R_FlatNumForName (const char *name)
{
#if 0
	int		i;
	static char	namet[16];

	i = W_CheckNumForName (name);
	if (i == -1)
	{
		namet[8] = 0;
		D_memcpy (namet, name,8);
		I_Error ("R_FlatNumForName: %s not found",namet);
	}
	
	i -= firstflat;
	if (i>numflats)
		I_Error ("R_FlatNumForName: %s past f_end",namet);
	return i;
#endif
	int			i;
	lumpinfo_t	*lump_p;
	char	name8[12];
	int		v1,v2;
	int		c;
	
/* make the name into two integers for easy compares */
	*(int *)&name8[0] = 0;
	*(int *)&name8[4] = 0;
	for (i=0 ; i<8 && name[i] ; i++)
	{
		c = name[i];
		if (c >= 'a' && c <= 'z')
			c -= 'a'-'A';
		name8[i] = c;
	}

	v1 = *(int *)name8;
	v2 = *(int *)&name8[4];

#ifndef MARS
	v1 |= HIBIT;
#endif

	lump_p = &lumpinfo[firstflat];
	for (i=0 ; i<numflats; i++, lump_p++)
	{
		if (*(int *)&lump_p->name[4] == v2
		&&  (*(int *)lump_p->name) == v1)
			return i;
	}

	I_Error ("R_FlatNumForName: %s not found",name);
	return -1;
}


/*
================
=
= R_CheckTextureNumForName
=
================
*/

int	R_CheckTextureNumForName (const char *name)
{
	int		i,c;
	char	temp[8];
	int		v1, v2;
	texture_t	*texture_p;
		
	if (name[0] == '-')		/* no texture marker */
		return 0;
	
	*(int *)&temp[0] = 0;
	*(int *)&temp[4] = 0;
	
	for (i=0 ; i<8 && name[i] ; i++)
	{
		c = name[i];
		if (c >= 'a' && c<='z')
			c -= ('a'-'A');
		temp[i] = c;
	}

	v1 = *(int *)temp;
	v2 = *(int *)&temp[4];
		
	texture_p = textures;
	
	for (i=0 ; i<numtextures ; i++,texture_p++)
		if (*(int *)&texture_p->name[4] == v2
		&&  (*(int *)texture_p->name) == v1)
			return i;
		
	return 0;	/* FIXME -1; */
}


/*
================
=
= R_TextureNumForName
=
================
*/

int	R_TextureNumForName (const char *name)
{
	int		i;
	
	i = R_CheckTextureNumForName (name);
	if (i==-1)
		I_Error ("R_TextureNumForName: %s not found",name);
	
	return i;
}


/*
================
=
= R_InitMathTables
=
= Create all the math tables for the current screen size
================
*/
void R_InitMathTables(void)
{
	int i;
	fixed_t focallength;
	short* tempviewangletox;

	if (viewangletox)
		Z_Free(viewangletox);
	if (xtoviewangle)
		Z_Free(xtoviewangle);
	if (yslope)
		Z_Free(yslope);
	if (distscale)
		Z_Free(distscale);

	tempviewangletox = (short *)I_WorkBuffer();
	viewangletox = Z_Malloc(sizeof(*viewangletox) * FINEANGLES / 2, PU_STATIC, 0);
	xtoviewangle = Z_Malloc(sizeof(*xtoviewangle) * (SCREENWIDTH+1), PU_STATIC, 0);

	// Use tangent table to generate viewangletox:
	//  viewangletox will give the next greatest x
	//  after the view angle.
	//
	// Calc focallength
	//  so FIELDOFVIEW angles covers SCREENWIDTH.

	focallength = FixedDiv(CENTERXFRAC, finetangent(FINEANGLES / 4 + FIELDOFVIEW / 2));
	for (i = 0; i < FINEANGLES / 2; i++)
	{
		fixed_t t;

		if (finetangent(i) > FRACUNIT * 2) {
			t = -1;
		}
		else if (finetangent(i) < -FRACUNIT * 2) {
			t = SCREENWIDTH + 1;
		}
		else {
			t = FixedMul(finetangent(i), focallength);
			t = (CENTERXFRAC - t + FRACUNIT - 1) >> FRACBITS;
			if (t < -1) {
				t = -1;
			}
			else if (t > SCREENWIDTH + 1) {
				t = SCREENWIDTH + 1;
			}
		}
		tempviewangletox[i] = t;
	}

	// Scan viewangletox[] to generate xtoviewangle[]:
	//  xtoviewangle will give the smallest view angle
	//  that maps to x.
	for (i = 0; i <= SCREENWIDTH; i++)
	{
		int x;
		for (x = 0; tempviewangletox[x] > i; x++);
		xtoviewangle[i] = (x << (ANGLETOFINESHIFT)) - ANG90;
	}

	// Take out the fencepost cases from viewangletox.
	for (i = 0; i < FINEANGLES / 2; i++)
	{
		if (tempviewangletox[i] == -1) {
			tempviewangletox[i] = 0;
		}
		else if (tempviewangletox[i] == SCREENWIDTH + 1) {
			tempviewangletox[i] = SCREENWIDTH;
		}
		viewangletox[i] = tempviewangletox[i];
	}

	// Make the yslope table for floor and ceiling textures
	yslope = Z_Malloc(sizeof(*yslope) * SCREENHEIGHT, PU_STATIC, 0);
	for (i = 0; i < SCREENHEIGHT; i++)
	{
		fixed_t y = ((i - SCREENHEIGHT / 2) << FRACBITS) + FRACUNIT / 2;
		y = D_abs(y);
		y = FixedDiv(SCREENWIDTH / 2 * STRETCH, y);
		y >>= 6;
		if (y > 0xFFFF) {
			y = 0xFFFF;
		}
		yslope[i] = y;
	}

	// Create the distance scale table for floor and ceiling textures
	distscale = Z_Malloc(sizeof(*distscale) * SCREENWIDTH, PU_STATIC, 0);
	for (i = 0; i < SCREENWIDTH; i++)
	{
		fixed_t cosang = finecosine(xtoviewangle[i] >> ANGLETOFINESHIFT);
		cosang = D_abs(cosang);
		distscale[i] = FixedDiv(FRACUNIT, cosang)>>1;
	}
}
