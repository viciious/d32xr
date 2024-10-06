/* R_data.c */

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"
#ifdef MARS
#include "mars.h"
#endif

boolean	spr_rotations;

VINT		firstflat, numflats, col2flat;

VINT		firstsprite, numsprites, numspriteframes;

VINT		numtextures = 0;
texture_t	*textures = NULL;
boolean 	texmips = false;

VINT 		numdecals = 0;
texdecal_t  *decals = NULL;

spritedef_t sprites[NUMSPRITES];
spriteframe_t* spriteframes;
VINT 			*spritelumps;

uint8_t			*flattranslation;		/* for global animation */
uint8_t			*texturetranslation;	/* for global animation */

flattex_t		*flatpixels;

inpixel_t	*skytexturep;
int8_t 		*skycolormaps;
VINT 		col2sky;

uint8_t		*dc_playpals;

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

void *R_SkipJagObjHeader(void *data, int size, int width, int height)
{
	jagobj_t *header = data;

	if (!data) {
		return NULL;
	}
	if ((int)sizeof(jagobj_t) > size) {
		return data;
	}
	if (BIGSHORT(header->width) != width) {
		return data;
	}
	if (BIGSHORT(header->height) != height) {
		return data;
	}
	if (BIGSHORT(header->depth) != 3) {
		return data;
	}
	return (char *)data + sizeof(jagobj_t) - sizeof(header->data);
}


void R_InitTextures (void)
{
	maptexture_t	*mtexture;
	texture_t		*texture;
	texdecal_t		*decal;
	int			i,j,c;
	int			offset;
	int			*directory;
	int 		start, end;
	int 		patchcount;
	int			*maptex;

/* */
/* load the map texture definitions from textures.lmp */
/* */
	maptex = (void *)I_TempBuffer();
	W_ReadLump(W_GetNumForName("TEXTURE1"), maptex);
	numtextures = LITTLELONG(*maptex);
	directory = maptex+1;

	start = W_CheckNumForName("T_START");
	end = W_CheckNumForName("T_END");

	textures = Z_Malloc (numtextures * sizeof(*textures), PU_STATIC);
	D_memset(textures, 0, numtextures * sizeof(*textures));

	for (i = 0; i < numtextures; i++, directory++)
	{
		offset = LITTLELONG(*directory);
		mtexture = (maptexture_t*)((byte*)maptex + offset);
		patchcount = LITTLESHORT(mtexture->patchcount);
		if (patchcount > 1)
			numdecals += patchcount - 1;
	}

	decals = Z_Malloc (numdecals * sizeof(*decals), PU_STATIC);
	D_memset(decals, 0, numdecals * sizeof(*decals));

	directory = maptex+1;
	for (i = 0; i < numtextures; i++, directory++)
	{
		boolean masked;

		offset = LITTLELONG(*directory);
		mtexture = (maptexture_t*)((byte*)maptex + offset);
		masked = *((byte *)&mtexture->masked);

		texture = &textures[i];
		texture->width = LITTLESHORT(mtexture->width);
		texture->height = LITTLESHORT(mtexture->height);
		texture->decals = 0;
		D_memcpy(texture->name, mtexture->name, 8);
		for (j = 0; j < 8; j++)
		{
			c = texture->name[j];
			if (c >= 'a' && c <= 'z')
				texture->name[j] = c - ('a' - 'A');
		}

		if (masked)
			texture->lumpnum = W_CheckRangeForName(texture->name, firstsprite, firstsprite + numsprites);
		else if (start >= 0 && end > 0)
			texture->lumpnum = W_CheckRangeForName(texture->name, start, end);
		else
			texture->lumpnum = W_CheckNumForName(texture->name);
	}

	// process patches/decals
	directory = maptex+1;
	decal = decals;
	for (i = 0; i < numtextures; i++, directory++)
	{
		int texnum;

		offset = LITTLELONG(*directory);
		mtexture = (maptexture_t*)((byte*)maptex + offset);
		patchcount = LITTLESHORT(mtexture->patchcount);

		if (patchcount == 0)
			continue;

		texture = &textures[i];
		if (texture->lumpnum >= 0)
			continue;

		texnum = LITTLESHORT(mtexture->patches[0].patch);
		if (texnum < 0 || texnum >= numtextures)
			continue;

		texture->lumpnum = textures[texnum].lumpnum;

		if (patchcount > 1)
		{
			int j;
			texture_t *texture2;
			int firstdecal = decal - decals;
			int numdecals = 0;

			for (j = 1; j < patchcount; j++) {
				mappatch_t *mp = &mtexture->patches[j];

				texnum = LITTLESHORT(mp->patch);
				if (texnum < 0 || texnum >= numtextures)
					continue;

				texture2 = &textures[texnum];
				if (texture2->lumpnum < 0)
					continue;

				decal->texturenum = texnum;
				decal->mincol = LITTLESHORT(mp->originx);
				decal->maxcol = decal->mincol + texture2->width - 1;
				decal->minrow = LITTLESHORT(mp->originy);
				decal->maxrow = decal->minrow + texture2->height - 1;

				if (decal->mincol >= texture->width || decal->maxcol < 0)
					continue;
				if (decal->minrow >= texture->height || decal->maxrow < 0)
					continue;

				if (decal->maxcol >= texture->width)
					decal->maxcol = texture->width - 1;
				if (decal->minrow >= texture->height)
					decal->minrow = texture->height - 1;

				decal++;
				numdecals++;
			}

			if (texture->decals == 0)
				texture->decals = (firstdecal << 2) | numdecals;
		}
	}

	for (i = 0; i < numtextures; i++)
	{
		texture = &textures[i];
		if (texture->lumpnum < 0)
			texture->lumpnum = 0;
	}

	// remap the dummy texture to the first valid texture
	textures[0].lumpnum = textures[1].lumpnum;
	textures[0].width = textures[1].width;
	textures[0].height = textures[1].height;
	textures[0].decals = textures[1].decals;
	D_memcpy(textures[0].data, textures[1].data, sizeof(textures[0].data));

	texmips = false;
#if MIPLEVELS > 1
	texture = textures;
	for (i = 0; i < numtextures; i++, texture++)
	{
		int w = texture->width, h = texture->height;
		uint8_t *start = R_CheckPixels(texture->lumpnum);
		int size = W_LumpLength(texture->lumpnum);
		uint8_t *end = start + size;
		uint8_t *data = R_SkipJagObjHeader(start, size, w, h);

		texture->mipcount = 0;

		// detect mipmaps
		for (j = 0; j < MIPLEVELS; j++)
		{
			int size = w * h;

			if (data+size > end) {
				// no mipmaps
				texture->mipcount = 1;
				break;
			}

			texture->mipcount++;

			data += size;

			w >>= 1;
			if (w < 1)
				w = 1;

			h >>= 1;
			if (h < 1)
				h = 1;
		}
	}

	/* textures can't have more mip levels than their respective decals */
	texture = textures;
	for (i = 0; i < numtextures; i++, texture++)
	{
		if (texture->mipcount <= 1)
			continue;
		if (texture->decals == 0)
			continue;

		int i, numdecals = texture->decals & 0x3;
		int firstdecal = texture->decals >> 2;
		for (i = 0; i < numdecals; i++) {
			texdecal_t *decal = &decals[firstdecal + i];
			texture_t *texture2 = &textures[decal->texturenum];
			if (texture->mipcount > texture2->mipcount)
				texture->mipcount = texture2->mipcount;
		}
	}

	texture = textures;
	for (i = 0; i < numtextures; i++, texture++)
	{
		if (texture->mipcount > 1)
		{
			texmips = true;
			break;
		}
	}
#endif

/* */
/* translation table for global animation */
/* */
	texturetranslation = Z_Malloc ((numtextures+1)*sizeof(*texturetranslation), PU_STATIC);
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
	int 	lastflat;

	firstflat = W_GetNumForName ("F_START") + 1;
	lastflat = W_GetNumForName ("F_END") - 1;
	numflats = lastflat - firstflat + 1;

/* translation table for global animation */
	flattranslation = Z_Malloc ((numflats+1)*sizeof(*flattranslation), PU_STATIC);
	for (i=0 ; i<numflats ; i++)
		flattranslation[i] = i;

	flatpixels = Z_Malloc(numflats * sizeof(*flatpixels), PU_STATIC);

	col2flat = R_FlatNumForName ("F_STCOL2");
	if (col2flat < 0)
		col2flat = numflats + 1;

#if MIPLEVELS > 1
	// detect mip-maps
	if (!texmips)
		return;

	for (i=0 ; i<numflats ; i++)
	{
		int j;
		int w = 64;
		uint8_t *start = R_CheckPixels(firstflat + i);
		uint8_t *end = start + W_LumpLength(firstflat + i);
		uint8_t *data = start;

		for (j = 0; j < MIPLEVELS; j++)
		{
			int size = w * w;

			if (data+size > end) {
				// no mipmaps
				texmips = false;
				return;
			}

			data += size;
			w >>= 1;
		}
	}
#endif
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
#if MIPLEVELS > 1
	int i;
#endif

	dc_playpals = (uint8_t*)W_POINTLUMPNUM(W_GetNumForName("PLAYPALS"));

	firstsprite = W_GetNumForName ("S_START") + 1;
	numsprites = W_GetNumForName ("S_END") - firstsprite;

	col2sky = W_CheckNumForName ("S_STCOL2");

	R_InitTextures ();
	R_InitFlats ();
	R_InitSpriteDefs((const char **)sprnames);
	R_InitColormap();

#if MIPLEVELS > 1
	if (!texmips) {
		for (i = 0; i < numtextures; i++)
			textures[i].mipcount = 1;
	}
#endif
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

int	R_FlatNumForName (const char *name)
{
	int f = W_CheckRangeForName (name, firstflat, firstflat + numflats);
	if (f < 0)
		return f;
	return f - firstflat;
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
		
	return -1;
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
	if (i == -1)
		return 0;
	//if (i==-1)
	//	I_Error ("R_TextureNumForName: %s not found",name);
	
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
	fixed_t focalLength;
	fixed_t stretchWidth;
	int fuzzunit;
	VINT *tempviewangletox;

	I_FreeWorkBuffer();

	viewangletox = (VINT *)I_AllocWorkBuffer(sizeof(*viewangletox) * (FINEANGLES / 2));
	distscale = (uint16_t *)I_AllocWorkBuffer(sizeof(*distscale) * SCREENWIDTH);
	yslope = (fixed_t *)I_AllocWorkBuffer(sizeof(*yslope) * SCREENHEIGHT);
	xtoviewangle = (uint16_t *)I_AllocWorkBuffer(sizeof(*xtoviewangle) * (SCREENWIDTH+1));
	tempviewangletox = (VINT *)I_WorkBuffer();

	// Use tangent table to generate viewangletox:
	//  viewangletox will give the next greatest x
	//  after the view angle.
	//
	// Calc focallength
	//  so FIELDOFVIEW angles covers viewportWidth.

	focalLength = FixedDiv(centerXFrac, finetangent(FINEANGLES / 4 + FIELDOFVIEW / 2));
	for (i = 0; i < FINEANGLES / 2; i++)
	{
		fixed_t t;

		if (finetangent(i) > FRACUNIT * 2) {
			t = -1;
		}
		else if (finetangent(i) < -FRACUNIT * 2) {
			t = viewportWidth + 1;
		}
		else {
			t = FixedMul(finetangent(i), focalLength);
			t = (centerXFrac - t + FRACUNIT - 1) >> FRACBITS;
			if (t < -1) {
				t = -1;
			}
			else if (t > viewportWidth + 1) {
				t = viewportWidth + 1;
			}
		}
		tempviewangletox[i] = t;
	}

	// Scan viewangletox[] to generate xtoviewangle[]:
	//  xtoviewangle will give the smallest view angle
	//  that maps to x.
	for (i = 0; i <= viewportWidth; i++)
	{
		int x;
		for (x = 0; tempviewangletox[x] > i; x++);
		xtoviewangle[i] = ((x << ANGLETOFINESHIFT) - ANG90) >> FRACBITS;
	}

	// Take out the fencepost cases from viewangletox.
	for (i = 0; i < FINEANGLES / 2; i++)
	{
		if (tempviewangletox[i] == -1) {
			tempviewangletox[i] = 0;
		}
		else if (tempviewangletox[i] == viewportWidth + 1) {
			tempviewangletox[i] = viewportWidth;
		}
		viewangletox[i] = tempviewangletox[i];
	}

	// Make the yslope table for floor and ceiling textures
	stretchWidth = viewportWidth / 2 * stretch;
	for (i = 0; i < viewportHeight; i++)
	{
		fixed_t y = ((i - viewportHeight / 2) << FRACBITS) + FRACUNIT / 2;
		y = D_abs(y);
		y = FixedDiv(stretchWidth, y);
		yslope[i] = y;
	}

	// Create the distance scale table for floor and ceiling textures
	for (i = 0; i < viewportWidth; i++)
	{
		fixed_t cosang = finecosine(xtoviewangle[i] >> (ANGLETOFINESHIFT-FRACBITS));
		cosang = D_abs(cosang);
		distscale[i] = FixedDiv(FRACUNIT, cosang)>>1;
	}

	fuzzunit = 320;
	for (i = 0; i < FUZZTABLE; i++)
	{
		fuzzoffset[i] = fuzzoffset[i] < 0 ? -fuzzunit : fuzzunit;
	}

#ifdef MARS
	// enable caching for LUTs
	viewangletox = (void *)(((intptr_t)viewangletox) & ~0x20000000);
	distscale = (void *)(((intptr_t)distscale) & ~0x20000000);
	yslope = (void *)(((intptr_t)yslope) & ~0x20000000);
	xtoviewangle = (void *)(((intptr_t)xtoviewangle) & ~0x20000000);
#endif
}


// variables used to look up
//  and range check thing_t sprites patches

typedef struct
{
	short		rotate;		/* if false use 0 for any position */
	short		lump[8];	/* lump to use for view angles 0-7 */
} tempspriteframe_t;

//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
void
R_InstallSpriteLump(const char* spritename, char letter, tempspriteframe_t* frame,
	int lump, int rotation, boolean flipped)
{
	int r;

	if (rotation == 0)
	{
		// the lump should be used for all rotations
		if (frame->rotate == 0)
			I_Error("R_InitSprites: Sprite %s frame %c has "
				"multip rot=0 lump", spritename, letter);

		if (frame->rotate == 1)
			I_Error("R_InitSprites: Sprite %s frame %c has rotations "
				"and a rot=0 lump", spritename, letter);

		frame->rotate = 0;
		for (r = 0; r < 8; r++)
		{
			frame->lump[r] = flipped ? lump|SL_FLIPPED : lump;
		}
		return;
	}

	// the lump is only used for one rotation
	if (frame->rotate == 0)
		I_Error("R_InitSprites: Sprite %s frame %c has rotations "
			"and a rot=0 lump", spritename, letter);

	frame->rotate = 1;

	// make 0 based
	rotation--;
	if (frame->lump[rotation] != -1)
		I_Error("R_InitSprites: Sprite %s : %c : %c "
			"has two lumps mapped to it",
			spritename, letter, '1' + rotation);

	frame->lump[rotation] = flipped ? lump|SL_FLIPPED : lump;
}




//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
//  (4 chars exactly) to be used.
// Builds the sprite rotation matrixes to account
//  for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant. 
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//  a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//  letter/number appended.
// The rotation character can be 0 to signify no rotations.
//
void R_InitSpriteDefs(const char** namelist)
{
	int		i;
	int		l;
	int		frame;
	int		rotation;
	int		start;
	int		end;
	tempspriteframe_t* sprtemp;
	int		maxframe;
	byte	*tempbuf;
	int		totalframes;
	int 	totallumps;
	VINT 	*lumps;

	spr_rotations = false;

	if (firstsprite < 0)
	{
		start = 0;
		end = W_GetNumForName("T_START");
		l = W_CheckNumForName("DS_START");
		if (l != -1 && l < end)
			end = l;
	}
	else
	{
		start = firstsprite;
		end = firstsprite + numsprites + 1;
	}

	// scan all the lump names for each of the names,
	//  noting the highest frame letter.
	maxframe = 29;
	totalframes = 0;
	totallumps = 0;

	tempbuf = I_WorkBuffer();
	sprtemp = (void*)tempbuf;
	for (i = 0; i < NUMSPRITES; i++)
	{
		const char* spritename = namelist[i];

		D_memset(sprtemp, -1, sizeof(tempspriteframe_t) * 29);
		maxframe = -1;

		// scan the lumps,
		//  filling in the frames for whatever is found
		for (l = start; l < end; l += 2)
		{
			int flip;
			const char* framename;

			framename = W_GetNameForNum(l);
			if (D_strncasecmp(framename, spritename, 4))
				continue;

			framename += 4;

			for (flip = 0; flip < 2; flip++)
			{
				if (!framename[0])
					break;

				frame = framename[0] - 'A';
				rotation = framename[1] - '0';
				if (frame > maxframe)
					maxframe = frame;
				if (frame >= 29 || rotation > 8)
					I_Error("Bad frame characters in lump %i", l);

				R_InstallSpriteLump(spritename, framename[0], sprtemp + frame, 
					l, rotation, flip);
				framename += 2;
			}
		}

		// check the frames that were found for completeness
		if (maxframe == -1)
		{
			sprites[i].numframes = 0;
			continue;
		}

		maxframe++;

		for (frame = 0; frame < maxframe; frame++)
		{
			switch ((int)sprtemp[frame].rotate)
			{
			case -1:
				sprtemp[frame].rotate = 0;
			case 0:
				// only the first rotation is needed
				totallumps++;
				break;
			case 1:
				// must have all 8 frames
				for (rotation = 0; rotation < 8; rotation++)
					if (sprtemp[frame].lump[rotation] == -1)
						I_Error("Sprite %s frame %c "
							"is missing rotations",
							spritename, frame + 'A');
				totallumps += 8;
				spr_rotations = true;
				break;
			}
		}

		// allocate space for the frames present and copy sprtemp to it
		sprites[i].numframes = maxframe;

		sprtemp += maxframe;
		totalframes += maxframe;
	}

	numspriteframes = totalframes;
	spriteframes = Z_Malloc(totalframes * sizeof(spriteframe_t), PU_STATIC);
	sprtemp = (void*)tempbuf;
	lumps = Z_Malloc(totallumps * sizeof(*lumps), PU_STATIC);
	spritelumps = lumps;

	for (i = 0; i < totalframes; i++)
	{
		spriteframes[i].lump = lumps - spritelumps;
		if (!sprtemp[i].rotate)
		{
			lumps[0] = sprtemp[i].lump[0]|SL_SINGLESIDED;
			lumps++;
		}
		else
		{
			for (l = 0; l < 8; l++)
				lumps[l] = sprtemp[i].lump[l];
			lumps += 8;
		}
#if 0
		int j;
		for (j = 0; j < 8; j++)
		{
			int x;
			int lump = sprtemp[i].lump[j];
			if (lump < 0)
				break;

			lump = lump < 0 ? -(lump + 1) : lump;
			if (!D_strncasecmp(W_GetNameForNum(lump) + 1, "KEY", 3))
				continue;
			if (!D_strncasecmp(W_GetNameForNum(lump), "CANDA0", 6))
				continue;
			if (!D_strncasecmp(W_GetNameForNum(lump), "CBRAA0", 6))
				continue;

			patch_t *patch = (patch_t*)W_POINTLUMPNUM(lump);
			uint8_t *pixels = R_CheckPixels(lump + 1);
			for (x = 0; x < patch->width; x++)
			{
				column_t* column = (column_t*)((byte*)patch +
					BIGSHORT(patch->columnofs[x]));

				/* */
				/* draw a masked column */
				/* */
				for (; column->topdelta != 0xff; column++)
				{
					int count = column->length;
					uint8_t* src = pixels + column->dataofs;
					while (count--)
					{
						uint8_t pix = *src++;
						if (pix == 248 || pix == 249)
							I_Error("%d %d", pix & 0xff, lump);
					}
				}
			}
		}
#endif
	}

	l = 0;
	for (i = 0; i < NUMSPRITES; i++)
	{
		int numframes;

		numframes = sprites[i].numframes;
		sprites[i].firstframe = l;

		l += numframes;
	}
}

static void *R_LoadColormap(int l)
{
	void *doomcolormap;

	doomcolormap = W_GetLumpData(l);
	return (void *)((int8_t*)doomcolormap + 128);
}

void R_InitColormap(void)
{
	int l;

	l = W_CheckNumForName("COLORMAP");
	dc_colormaps = R_LoadColormap(l);

	l -= 1;
	dc_colormaps2 = R_LoadColormap(l);
}

boolean R_CompositeColumn(int colnum, int numdecals, texdecal_t *decals, inpixel_t *src, inpixel_t *dst, int height, int miplevel)
{
	int i;
	boolean decaled = false;

	if (!numdecals)
		return false;

	i = 0;
	do
	{
		int count;
		texdecal_t *decal = &decals[i];
		texture_t *decaltex = &textures[decal->texturenum];

		if (colnum < decal->mincol || colnum > decal->maxcol)
			continue;

		if (!decaled)
		{
			decaled = true;
			D_memcpy(dst, src, sizeof(inpixel_t) * height);
		}

		src = (inpixel_t *)decaltex->data[0];
		count = (colnum - decal->mincol) * decaltex->height;

#if MIPLEVEL > 1
		// FIXME
		int j;
		for (j = 0; j < miplevel; j++)
			count >>= 1;
		src = (inpixel_t *)decaltex->data[miplevel];
#endif
		src += count;
		count = decal->maxrow - decal->minrow + 1;

#ifdef MARS
		dst = (void *)((intptr_t)dst | 0x20000); // overwrite area of VRAM
#endif
		D_memcpy(dst + decal->minrow, src, sizeof(inpixel_t) * count);
		src = dst;
	} while (++i < numdecals);

	return decaled;
}
