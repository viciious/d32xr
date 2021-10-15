/* W_wad.c */

#include "doomdef.h"
/* include "r_local.h" */


/*=============== */
/*   TYPES */
/*=============== */


typedef struct
{
	char		identification[4];		/* should be IWAD */
	int			numlumps;
	int			infotableofs;
} wadinfo_t;


byte		*wadfileptr;

/*============= */
/* GLOBALS */
/*============= */

lumpinfo_t	*lumpinfo;			/* points directly to rom image */
int			numlumps;
#ifndef MARS
void		*lumpcache[MAXLUMPS];
#endif

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



#define WINDOW_SIZE	4096
#define LOOKAHEAD_SIZE	16

#define LENSHIFT 4		/* this must be log2(LOOKAHEAD_SIZE) */

unsigned char *decomp_input;
unsigned char *decomp_output;
extern int decomp_start;

void decode(unsigned char *input, unsigned char *output)
{
#ifdef JAGUAR
	decomp_input = input;
	decomp_output = output;
	
	gpufinished = zero;
	gpucodestart = (int)&decomp_start;
	while (!I_RefreshCompleted () )
	;
#else
  int getidbyte = 0;
  int len;
  int pos;
  int i;
  unsigned char *source;
  int idbyte = 0;

  while (1)
  {

    /* get a new idbyte if necessary */
    if (!getidbyte) idbyte = *input++;
    getidbyte = (getidbyte + 1) & 7;

    if (idbyte&1)
    {
      /* decompress */
      pos = *input++ << LENSHIFT;
      pos = pos | (*input >> LENSHIFT);
      source = output - pos - 1;
      len = (*input++ & 0xf)+1;
      if (len==1) break;
      for (i=0 ; i<len ; i++)
        *output++ = *source++;
    } else {
      *output++ = *input++;
    }

    idbyte = idbyte >> 1;

  }
#endif
}

/*
============================================================================

						LUMP BASED ROUTINES

============================================================================
*/

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
	
	wadfileptr = I_WadBase ();

	if (D_strncasecmp(((wadinfo_t*)wadfileptr)->identification,"IWAD",4))
		I_Error ("Wad file doesn't have IWAD id\n");
	
	numlumps = BIGLONG(((wadinfo_t*)wadfileptr)->numlumps);

	infotableofs = BIGLONG(((wadinfo_t*)wadfileptr)->infotableofs);
	lumpinfo = (lumpinfo_t *) (wadfileptr + infotableofs);
}


/*
====================
=
= W_CheckNumForName
=
= Returns -1 if name not found
=
====================
*/

int	W_CheckNumForName (const char *name)
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

	lump_p = lumpinfo + numlumps;

	/* used for stripping out the hi bit of the first character of the */
	/* name of the lump */

#ifdef i386
#define HIBIT (1<<7)
#else
#define HIBIT (1<<31)
#endif

	while (lump_p-- != lumpinfo)
		if (*(int *)&lump_p->name[4] == v2
		&&  (*(int *)lump_p->name & ~HIBIT) == v1)
			return lump_p - lumpinfo;


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
	if (lump >= numlumps)
		I_Error ("W_LumpLength: %i >= numlumps",lump);
	return BIGLONG(lumpinfo[lump].size);
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

void W_ReadLump (int lump, void *dest)
{
	lumpinfo_t	*l;
	
	if (lump >= numlumps)
		I_Error ("W_ReadLump: %i >= numlumps",lump);
	l = lumpinfo+lump;
	if (l->name[0] & 0x80) /* compressed */
	{
		 decode((unsigned char *) (wadfileptr + BIGLONG(l->filepos)),
		(unsigned char *) dest);
	}
	else
	  D_memcpy (dest, wadfileptr + BIGLONG(l->filepos), BIGLONG(l->size));
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
	int len;

	if ((unsigned)lump >= numlumps)
		I_Error ("W_CacheLumpNum: %i >= numlumps",lump);
	if (tag != PU_STATIC)
		I_Error("W_CacheLumpNum: %i tag != PU_STATIC", lump);

	len = W_LumpLength(lump);
	Z_Malloc(len+1, tag, &cache);
	W_ReadLump(lump, cache);
	((char *)cache)[len] = '\0';

	return cache;
#else
	if ((unsigned)lump >= numlumps)
		I_Error("W_CacheLumpNum: %i >= numlumps", lump);

	if (!lumpcache[lump])
	{	/* read the lump in */
/*printf ("cache miss on lump %i\n",lump); */
		len = W_LumpLength(lump);
		Z_Malloc (len+1, tag, &lumpcache[lump]);
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
	if (lump >= numlumps)
		I_Error ("W_GetNameForNum: %i >= numlumps",lump);
	return lumpinfo[lump].name;
}


