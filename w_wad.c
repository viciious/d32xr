/* W_wad.c */

#include "doomdef.h"
#include "lzss.h"
#include "mars.h"

/* include "r_local.h" */

/*============= */
/* GLOBALS */
/*============= */

#define MAXWADS 2 					/* IWAD + CD PWAD + virtual CD PWAD */

typedef struct
{
	char		identification[4];		/* should be IWAD */
	int			numlumps;
	int			infotableofs;
} wadinfo_t;

typedef struct
{
	byte		*fileptr;
	int 		cdlength, cdoffset;

	lumpinfo_t	*lumpinfo;			/* points directly to rom image */
	int 		firstlump;
	int			numlumps;
	int 		infotableofs;
#ifndef MARS
	void		*lumpcache[MAXLUMPS];
#endif
} wadfile_t;

static wadfile_t wadfile[MAXWADS];
static int wadnum = 0;

void strupr (char *s)
{
	char	c;
	
    while ( (c = *s) != 0)
	{
		if (c >= 'a' && c <= 'z')
			c -= 'a'-'A';
		*s++ = c;
	}
}


#ifdef JAGUAR
#define WINDOW_SIZE	4096
#define LOOKAHEAD_SIZE	16

#define LENSHIFT 4		/* this must be log2(LOOKAHEAD_SIZE) */

unsigned char *decomp_input;
unsigned char *decomp_output;
extern int decomp_start;

void decode(unsigned char *input, unsigned char *output)
{
	decomp_input = input;
	decomp_output = output;
	
	gpufinished = zero;
	gpucodestart = (int)&decomp_start;
	while (!I_RefreshCompleted () )
	;
}
#else
void decode(unsigned char* input, unsigned char* output)
{
	lzss_state_t lzss;
	lzss_setup(&lzss, input, output, LZSS_BUF_SIZE);
	lzss_read_all(&lzss);
}
#endif

/*
============================================================================

						LUMP BASED ROUTINES

============================================================================
*/

/*
====================
=
= W_InitCDPWAD
=
====================
*/

static void W_InitCDPWAD (void)
{
	int 			l;
	char 			*ptr;
	wadfile_t 		*wad;

	wad = &wadfile[1];
	wad->firstlump = wadfile[0].numlumps;
	wad->cdlength = I_OpenCDFileByName(PWAD_NAME, &wad->cdoffset);
	if (wad->cdlength <= 0)
		I_Error ("Failed to open %s", PWAD_NAME);

	l = sizeof(wadinfo_t);
	if (I_ReadCDFile(l) < 0)
		I_Error("Reading %d bytes failed", l);

	ptr = Mars_GetCDFileBuffer();

	if (D_strncasecmp(((wadinfo_t*)ptr)->identification,"PWAD",4))
		I_Error ("Wad file doesn't have PWAD id\n");
	wad->numlumps = LITTLELONG(((wadinfo_t*)ptr)->numlumps);
	wad->infotableofs = LITTLELONG(((wadinfo_t*)ptr)->infotableofs);

	W_LoadPWAD();
}

/*
====================
=
= W_Init
=
====================
*/

void W_Init (void)
{
	int				infotableofs;
	wadfile_t 		*wad;

	wadnum = 0;
	D_memset(wadfile, 0, sizeof(wadfile));

	wad = &wadfile[0];
	wad->fileptr = I_WadBase ();

	if (D_strncasecmp(((wadinfo_t*)wad->fileptr)->identification,"IWAD",4))
		I_Error ("Wad file doesn't have IWAD id\n");
	
	wad->numlumps = BIGLONG(((wadinfo_t*)wad->fileptr)->numlumps);

	infotableofs = BIGLONG(((wadinfo_t*)wad->fileptr)->infotableofs);
	wad->lumpinfo = (lumpinfo_t *) (wad->fileptr + infotableofs);

	W_InitCDPWAD();
}

/*
====================
=
= W_GetWadForLump
=
====================
*/
static wadfile_t *W_GetWadForLump (int lumpnum)
{
	int i;

	if (lumpnum < 0)
		return &wadfile[0];

	for (i = wadnum; i > 0; i--) {
		if (lumpnum >= wadfile[i].firstlump && lumpnum < wadfile[i].firstlump+wadfile[i].numlumps)
			break;
	}
	return &wadfile[i];
}

/*
====================
=
= W_CacheWADLumps
=
====================
*/
int W_CacheWADLumps (lumpinfo_t *li, int numlumps, VINT *lumps, boolean setpwad)
{
	int n;
	wadfile_t *wad;

	n = 0;
	if (numlumps > 0)
	{
		int i, l;
		for (i = 0; i < numlumps; i++) {
			l = lumps[i];
			if (l < 0)
				continue;
			wad = W_GetWadForLump(lumps[i]);
			D_memcpy(&li[n], &wad->lumpinfo[l - wad->firstlump], sizeof(lumpinfo_t));
			n++;
		}
	}

	if (!setpwad)
		return n;

	wad = &wadfile[1];
	wad->numlumps = n;
	wad->lumpinfo = li;
	return n;
}

/*
====================
=
= W_LoadPWAD
=
====================
*/
void W_LoadPWAD (void)
{
	int i, l;
	wadfile_t *wad = &wadfile[1];
	lumpinfo_t *li;
	static int cache_size = -1;
	static int cache_num_lumps = 0;

	wadnum = 1;

	I_OpenCDFileByOffset(wad->cdlength, wad->cdoffset);

	if (cache_size != -1) {
		wad->numlumps = cache_num_lumps;
		wad->lumpinfo = I_GetCDFileCache(cache_size);
		return;
	}

	I_SeekCDFile(wad->infotableofs, SEEK_SET);

	l = wad->numlumps*sizeof(lumpinfo_t);
	if (I_ReadCDFile(l) < 0)
		I_Error("Reading %d bytes failed", l);

	li = I_GetCDFileBuffer();
	wad->lumpinfo = li;

	for (i = 0; i < wad->numlumps; i++) {
		li[i].filepos = LITTLELONG(li[i].filepos);
		li[i].size = LITTLELONG(li[i].size);
	}

	cache_size = l;
	cache_num_lumps = wad->numlumps;
	I_SetCDFileCache(l);
}

/*
====================
=
= W_UnloadPWAD
=
====================
*/
void W_UnloadPWAD (void)
{
	wadnum = 0;
}

/*
====================
=
= W_CheckWadForName
=
= Returns -1 if name not found
=
====================
*/

static int	W_CheckWadForName (wadfile_t *wad, const char *name, int start, int end)
{
	char	name8[12];
	int		v1,v2;
	lumpinfo_t	*lump_p;

/* make the name into two integers for easy compares */
	D_memset (name8,0,sizeof(name8));
	D_strncpy (name8,name,8);
	name8[8] = 0;			/* in case the name was a full 8 chars */
	strupr (name8);			/* case insensitive */

	v1 = *(int *)name8;
	v2 = *(int *)&name8[4];


/* scan backwards so patch lump files take precedence */
	lump_p = wad->lumpinfo + end;

	/* used for stripping out the hi bit of the first character of the */
	/* name of the lump */

#ifdef i386
#define HIBIT (1<<7)
#else
#define HIBIT (1<<31)
#endif

	while (lump_p-- != wad->lumpinfo + start)
		if (*(int *)&lump_p->name[4] == v2
		&&  (*(int *)lump_p->name & ~HIBIT) == v1)
			return lump_p - wad->lumpinfo;


	return -1;
}

int	W_CheckRangeForName (const char *name, int start, int end)
{
	int l;
	wadfile_t *wad;

	wad = W_GetWadForLump(start);
	if (start < 0 || end < 0)
		return -1;

	l = W_CheckWadForName(wad, name, start - wad->firstlump, end - wad->firstlump);
	if (l < 0)
		return l;
	return wad->firstlump + l;
}

int	W_CheckNumForName (const char *name)
{
	int i;
	for (i = wadnum; i >= 0; i--) {
		int l = W_CheckWadForName(&wadfile[i], name, 0, wadfile[i].numlumps);
		if (l >= 0) {
			return wadfile[i].firstlump + l;
		}
	}
	return -1;
}


/*
====================
=
= W_GetNumForName
=
= Calls W_CheckNumForName, but bombs out if not found
=
====================
*/

int	W_GetNumForName (const char *name)
{
	int	i;

	i = W_CheckNumForName (name);
	if (i != -1)
		return i;

	I_Error ("W_GetNumForName: %s not found!",name);
	return -1;
}


/*
====================
=
= W_LumpLength
=
= Returns the buffer size needed to load the given lump
=
====================
*/

int W_LumpLength (int lump)
{
	wadfile_t *wad;

	if (lump < 0)
		I_Error("W_LumpLength: %i < 0", lump);

	wad = W_GetWadForLump(lump);
	if (lump >= wad->firstlump+wad->numlumps)
		I_Error ("W_LumpLength: %i >= numlumps",lump);
	return BIGLONG(wad->lumpinfo[lump-wad->firstlump].size);
}


/*
====================
=
= W_ReadLump
=
= Loads the lump into the given buffer, which must be >= W_LumpLength()
=
====================
*/

int W_ReadLump (int lump, void *dest)
{
	lumpinfo_t	*l;
	volatile int size;
	wadfile_t 	*wad;
	void 		*data;
	volatile boolean compressed;

	if (lump < 0)
		I_Error("W_ReadLump: %i < 0", lump);
	wad = W_GetWadForLump(lump);
	if (lump >= wad->firstlump+wad->numlumps)
		I_Error ("W_ReadLump: %i >= numlumps",lump);

	l = &wad->lumpinfo[lump-wad->firstlump];
	// cache the file size because W_GetLumpData can overwrite 
	// the memory region that stores the lump info - the l pointer
	size = BIGLONG(l->size);
	compressed = l->name[0] & 0x80;

	data = W_GetLumpData(lump);

	if (compressed) /* compressed */
		decode(data, (unsigned char *) dest);
	else if (dest != data)
		D_memcpy (dest, data, size);
	return size;
}



/*
====================
=
= W_CacheLumpNum
=
====================
*/

void	*W_CacheLumpNum (int lump, int tag)
{
#ifdef MARS
	void* cache;
	volatile int len;
	volatile boolean compressed;
	wadfile_t *wad;
	lumpinfo_t	*l;
	void *data;

	if (lump < 0)
		I_Error("W_CacheLumpNum: %i < 0", lump);
	wad = W_GetWadForLump(lump);
	if (lump >= wad->firstlump+wad->numlumps)
		I_Error ("W_CacheLumpNum: %i >= numlumps",lump);
	if (tag != PU_STATIC)
		I_Error("W_CacheLumpNum: %i tag != PU_STATIC", lump);

	l = &wad->lumpinfo[lump-wad->firstlump];
	// cache the file size because W_GetLumpData can overwrite 
	// the memory region that stores the lump info - the l pointer
	len = BIGLONG(l->size);
	compressed = l->name[0] & 0x80;

	cache = Z_Malloc(len+1, tag);
	data = W_GetLumpData(lump);

	if (compressed) /* compressed */
		decode(data, (unsigned char *) cache);
	else
		D_memcpy (cache, data, len);
	((char *)cache)[len] = '\0';

	return cache;
#else
	if (lump < 0)
		I_Error("W_CacheLumpNum: %i < 0", lump);
	if (lump >= numlumps)
		I_Error("W_CacheLumpNum: %i >= numlumps", lump);

	if (!lumpcache[lump])
	{	/* read the lump in */
/*printf ("cache miss on lump %i\n",lump); */
		len = W_LumpLength(lump);
		Z_Malloc (len+1, tag);
		W_ReadLump (lump, lumpcache[lump]);
		((char*)lumpcache[lump])[len] = '\0';
	}
	else
		Z_ChangeTag (lumpcache[lump],tag);
/*else printf ("cache hit on lump %i\n",lump); */
	
	return lumpcache[lump];
#endif
}


/*
====================
=
= W_CacheLumpName
=
====================
*/

void	*W_CacheLumpName (const char *name, int tag)
{
	return W_CacheLumpNum (W_GetNumForName(name), tag);
}

/*
====================
=
= W_GetNameForNum
=
====================
*/

const char *W_GetNameForNum (int lump)
{
	wadfile_t *wad;

	if (lump < 0)
		I_Error("W_GetNameForNum: %i < 0", lump);
	wad = W_GetWadForLump(lump);
	if (lump >= wad->firstlump+wad->numlumps)
		I_Error ("W_GetNameForNum: %i >= numlumps",lump);

	return wad->lumpinfo[lump-wad->firstlump].name;
}

/*
====================
=
= W_GetLumpData
====================
*/

void * W_GetLumpData(int lump)
{
	wadfile_t *wad;
	lumpinfo_t* l;

	if (lump < 0)
		I_Error("W_GetLumpData: %i < 0", lump);

	wad = W_GetWadForLump(lump);
	if (lump >= wad->firstlump+wad->numlumps)
		I_Error ("W_GetLumpData: %i >= numlumps",lump);

	l = &wad->lumpinfo[lump-wad->firstlump];
	if (wad->cdlength)
	{
		volatile int pos = BIGLONG(l->filepos);
		volatile int len = BIGLONG(l->size);

		I_SeekCDFile(pos, SEEK_SET);

		if (I_ReadCDFile(len) < 0)
			I_Error("Reading %d bytes failed", len);

		return I_GetCDFileBuffer();
	}

	return I_RemapLumpPtr((void*)(wad->fileptr + BIGLONG(l->filepos)));
}

void I_Debug(void)
{
}
