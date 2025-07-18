/* P_main.c */

#include "doomdef.h"
#include "p_local.h"
#include "mars.h"

#define MAX_AUX_TEXTURES 32

VINT		numvertexes;
mapvertex_t	*vertexes;

VINT		numsegs;
seg_t		*segs;

VINT		numsectors;
sector_t	*sectors;

VINT		numsubsectors;
subsector_t	*subsectors;

VINT		numnodes;
node_t		*nodes;

VINT		numlines;
line_t		*lines;

VINT 		numlinetags;
int16_t 	*linetags; // [HASH_SIZE]{line,tag,line,tag...}

VINT		numsides;
side_t		*sides;

short		*blockmaplump;			/* offsets in blockmap are from here */
VINT		bmapwidth, bmapheight;	/* in mapblocks */
fixed_t		bmaporgx, bmaporgy;		/* origin of block map */
SPTR		*blocklinks;			/* for thing chains */

byte		*rejectmatrix;			/* for fast sight rejection */

mapthing_t	*deathmatchstarts, *deathmatch_p;

VINT		*validcount;			/* increment every time a check is made */

mapthing_t	playerstarts[MAXPLAYERS];

VINT		numthings;
spawnthing_t* spawnthings;

int16_t 	worldbbox[4];

/*
=================
=
= P_LoadVertexes
=
=================
*/

void P_LoadVertexes (int lump)
{
	byte		*data;
	int			i;
	mapvertex_t	*ml;
	mapvertex_t	*li;
	
	numvertexes = W_LumpLength (lump) / sizeof(mapvertex_t);
	vertexes = Z_Malloc (numvertexes*sizeof(mapvertex_t) + 16,PU_LEVEL);
	vertexes = (void*)(((uintptr_t)vertexes + 15) & ~15); // aline on cacheline boundary
	data = W_GetLumpData(lump);

	ml = (mapvertex_t *)data;
	li = vertexes;
	for (i=0 ; i<numvertexes ; i++, li++, ml++)
	{
		li->x = LITTLESHORT(ml->x);
		li->y = LITTLESHORT(ml->y);
	}

	worldbbox[BOXLEFT] = INT16_MAX;
	worldbbox[BOXRIGHT] = INT16_MIN;
	worldbbox[BOXBOTTOM] = INT16_MAX;
	worldbbox[BOXTOP] = INT16_MIN;

	li = vertexes;
	for (i=0 ; i<numvertexes ; i++, li++)
	{
		mapvertex_t *v = li;
		if (v->x < worldbbox[BOXLEFT])
			worldbbox[BOXLEFT] = v->x;
		if (v->x > worldbbox[BOXRIGHT])
			worldbbox[BOXRIGHT] = v->x;
		if (v->y < worldbbox[BOXBOTTOM])
			worldbbox[BOXBOTTOM] = v->y;
		if (v->y > worldbbox[BOXTOP])
			worldbbox[BOXTOP] = v->y;
	}
}


/*
=================
=
= P_LoadSegs
=
=================
*/

void P_LoadSegs (int lump)
{
	byte		*data;
	int			i;
	mapseg_t	*ml;
	seg_t		*li;
	int			linedef, side;

#ifdef USE_SMALL_LUMPS
	numsegs = W_LumpLength (lump) / sizeof(seg_t);

	// FIXME: this is strictly big-endian for now
	if (W_IsIWad(lump))
	{
		segs = W_POINTLUMPNUM(lump);
		return;
	}
	segs = Z_Malloc (numsegs*sizeof(seg_t)+16,PU_LEVEL);
	segs = (void*)(((uintptr_t)segs + 15) & ~15); // aline on cacheline boundary
	W_ReadLump(lump, segs);
	return;
#endif

	numsegs = W_LumpLength (lump) / sizeof(mapseg_t);
	segs = Z_Malloc (numsegs*sizeof(seg_t)+16,PU_LEVEL);
	segs = (void*)(((uintptr_t)segs + 15) & ~15); // aline on cacheline boundary
	D_memset (segs, 0, numsegs*sizeof(seg_t));
	data = W_GetLumpData(lump);

	ml = (mapseg_t *)data;
	li = segs;
	for (i=0 ; i<numsegs ; i++, li++, ml++)
	{
		li->v1 = (unsigned)LITTLESHORT(ml->v1);
		li->v2 = (unsigned)LITTLESHORT(ml->v2);

		linedef = (unsigned)LITTLESHORT(ml->linedef);

		li->linedef = linedef;

		side = (unsigned)LITTLESHORT(ml->side);

		SEG_PACK(li, side);
	}
}


/*
=================
=
= P_LoadSubsectors
=
=================
*/

void P_LoadSubsectors (int lump)
{
	byte			*data;
	int				i;
	int 			count;
	mapsubsector_t	*ms;
	subsector_t		*ss;

#ifdef USE_SMALL_LUMPS
	int16_t 		*cn;
	int 			curline;

	numsubsectors = W_LumpLength (lump) / sizeof(int16_t);
	count = numsubsectors+1;
	subsectors = Z_Malloc (count*sizeof(subsector_t),PU_LEVEL);
	data = W_GetLumpData(lump);

	cn = (int16_t *)data;
	D_memset (subsectors,0, count*sizeof(subsector_t));

	if (!numsubsectors)
		return;

	curline = 0;
	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++, cn++)
	{
		count = BIGSHORT(*cn);
		ss->firstline = curline;
		curline += count;
	}

	ss->firstline = curline;
	return;
#endif

	numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
	count = numsubsectors+1;
	subsectors = Z_Malloc (count*sizeof(subsector_t),PU_LEVEL);
	data = W_GetLumpData(lump);

	ms = (mapsubsector_t *)data;
	D_memset (subsectors,0, count*sizeof(subsector_t));

	if (!numsubsectors)
		return;

	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++, ms++)
	{
		count = LITTLESHORT(ms->numsegs);
		ss->firstline = LITTLESHORT(ms->firstseg);
	}

	--ss;
	(ss + 1)->firstline = ss->firstline + count;
}


/*
=================
=
= P_LoadSectors
=
=================
*/

void P_LoadSectors (int lump)
{
	byte			*data;
	int				i;
	mapsector_t		*ms;
	sector_t		*ss;
			
	numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
	sectors = Z_Malloc (numsectors*sizeof(sector_t) + 16,PU_LEVEL);
	sectors = (void*)(((uintptr_t)sectors + 15) & ~15); // aline on cacheline boundary
	D_memset (sectors, 0, numsectors*sizeof(sector_t));
	data = W_GetLumpData(lump);

	ms = (mapsector_t *)data;
	ss = sectors;
	for (i=0 ; i<numsectors ; i++, ss++, ms++)
	{
		int lightlevel, special;

		ss->floorheight = LITTLESHORT(ms->floorheight)<<FRACBITS;
		ss->ceilingheight = LITTLESHORT(ms->ceilingheight)<<FRACBITS;
		ss->floorpic = R_FlatNumForName(ms->floorpic);
		if (!D_strncasecmp (ms->ceilingpic,"F_SKY1",6) )
			*(int8_t *)&ss->ceilingpic = -1;
		else
		{
			ss->ceilingpic = R_FlatNumForName(ms->ceilingpic);
		}
		ss->thinglist = (SPTR)0;

		lightlevel = LITTLESHORT(ms->lightlevel);
		if (lightlevel < 0)
			lightlevel = 0;
		else if (lightlevel > 255)
			lightlevel = 255;
		ss->lightlevel = lightlevel;

		special = LITTLESHORT(ms->special);
		if (special < 0 || special > 255)
			special = 0;
		ss->special = special;

		ss->tag = LITTLESHORT(ms->tag);
	}
}


/*
=================
=
= P_LoadNodes
=
=================
*/


static int P_EncodeBBoxSide(int16_t *b, int16_t *outerbbox, int pc, int nc)
{
	int length, unit;
	int nu, pu;

	length = outerbbox[pc] - outerbbox[nc] + 15;
	if (length < 16)
		return 0;
	unit = length / 16;

	// negative corner is increasing
	nu = 0;
	length = b[nc] - outerbbox[nc];
	if (length > 0) {
		nu = length / unit;
		if (nu > 15)
			nu = 15;
		b[nc] = outerbbox[nc] + nu * unit;
	}

	// positive corner is decreasing
	pu = 0;
	length = outerbbox[pc] - b[pc];
	if (length > 0) {
		pu = length / unit;
		if (pu > 15)
			pu = 15;
		b[pc] = outerbbox[pc] - pu * unit;
	}

	return (pu << 4) | nu;
}

// encodes bbox as the number of 1/16th units of parent bbox on each side
static int P_EncodeBBox(int16_t *cb, int16_t *outerbbox)
{
	int encbbox;
	encbbox = P_EncodeBBoxSide(cb, outerbbox, BOXRIGHT, BOXLEFT);
	encbbox <<= 8;
	encbbox |= P_EncodeBBoxSide(cb, outerbbox, BOXTOP, BOXBOTTOM);
	return encbbox;
}

static void P_EncodeNodeBBox_r(int nn, int16_t *bboxes, int16_t *outerbbox)
{
	int 		j;
	node_t 		*n;

	if (nn & NF_SUBSECTOR)
		return;

	n = nodes + nn;
	for (j=0 ; j<2 ; j++)
	{
		int16_t *bbox = &bboxes[nn*8+j*4];
		n->encbbox[j] = P_EncodeBBox(bbox, outerbbox);
		P_EncodeNodeBBox_r(n->children[j], bboxes, bbox);
	}
}

// set the world's bounding box
// recursively encode bounding boxes for all BSP nodes
static void P_EncodeNodesBBoxes(int16_t *bboxes)
{
	P_EncodeNodeBBox_r(numnodes-1, bboxes, worldbbox);
}

void P_LoadNodes (int lump)
{
	byte		*data;
	int			i,j,k;
	node_t		*no;
	mapnode_t 	*mn;
	int16_t 	*bboxes;

#ifdef USE_SMALL_LUMPS
	numnodes = W_LumpLength (lump) / sizeof(node_t);

	// FIXME: this is strictly big-endian for now
	if (W_IsIWad(lump))
	{
		nodes = W_POINTLUMPNUM(lump);
		return;
	}
	nodes = Z_Malloc (numnodes*sizeof(node_t) + 16,PU_LEVEL);
	nodes = (void*)(((uintptr_t)nodes + 15) & ~15); // aline on cacheline boundary
	W_ReadLump(lump, nodes);
	return;
#endif

	numnodes = W_LumpLength (lump) / sizeof(*mn);
	data = W_GetLumpData(lump);
	nodes = Z_Malloc (numnodes*sizeof(node_t) + 16,PU_LEVEL);
	nodes = (void*)(((uintptr_t)nodes + 15) & ~15); // aline on cacheline boundary
	bboxes = (void *)I_FrameBuffer();

	mn = (mapnode_t *)data;
	no = nodes;
	for (i=0 ; i<numnodes ; i++, no++, mn++)
	{
		no->x = LITTLESHORT(mn->x);
		no->y = LITTLESHORT(mn->y);
		no->dx = LITTLESHORT(mn->dx);
		no->dy = LITTLESHORT(mn->dy);

		for (j=0 ; j<2 ; j++)
		{
			no->children[j] = LITTLESHORT(mn->children[j]);
			for (k=0 ; k<4 ; k++)
				bboxes[i*8+j*4+k] = LITTLESHORT(mn->bbox[j][k]);
		}
	}

	P_EncodeNodesBBoxes(bboxes);
}

/*
=================
=
= P_LoadThings
=
=================
*/

void P_LoadThings (int lump, boolean *havebossspit)
{
	byte			*data;
	int				i;
	mapthing_t		*mt;
	spawnthing_t	*st;
	int 			numthingsreal, numstaticthings;

	data = W_GetLumpData(lump);
	numthings = W_LumpLength (lump) / sizeof(mapthing_t);
	numthingsreal = 0;
	numstaticthings = 0;
	*havebossspit = false;

	mt = (mapthing_t *)data;
	for (i=0 ; i<numthings ; i++, mt++)
	{
		mt->x = LITTLESHORT(mt->x);
		mt->y = LITTLESHORT(mt->y);
		mt->angle = LITTLESHORT(mt->angle);
		mt->type = LITTLESHORT(mt->type);
		mt->options = LITTLESHORT(mt->options);
	}

	if (netgame == gt_deathmatch)
	{
		spawnthings = Z_Malloc(numthings * sizeof(*spawnthings), PU_LEVEL);
		st = spawnthings;
		mt = (mapthing_t*)data;
		for (i = 0; i < numthings; i++, mt++)
		{
			st->x = mt->x, st->y = mt->y;
			st->angle = mt->angle;
			st->type = mt->type;
			st++;
		}
	}

	mt = (mapthing_t *)data;
	for (i=0 ; i<numthings ; i++, mt++)
	{
		switch (P_MapThingSpawnsMobj(mt))
		{
			case 0:
				break;
			case 1:
				if (mt->type == mobjinfo[MT_BOSSSPIT].doomednum)
					*havebossspit = true;
				numthingsreal++;
				break;
			case 2:
				numstaticthings++;
				break;
		}
	}

	// preallocate a few mobjs for puffs and projectiles
	numthingsreal += 40;
	P_PreSpawnMobjs(numthingsreal, numstaticthings);

	mt = (mapthing_t *)data;
	for (i=0 ; i<numthings ; i++, mt++)
		P_SpawnMapThing (mt, i);
}

void P_SetLineTag (int ld, int tag)
{
	VINT j;
	VINT rowsize = (unsigned)numlinetags / LINETAGS_HASH_SIZE;
	VINT h = (unsigned)ld % LINETAGS_HASH_SIZE;
	VINT s = h * rowsize;

	for (j = 0; j < numlinetags; j++)
	{
		int16_t *l;
		VINT e;

		e = s + j;
		if (e >= numlinetags)
			e -= numlinetags;

		l = &linetags[e * 2];
		if (l[0] == -1 || l[0] == ld) {
			l[0] = ld;
			l[1] = tag;
			break;
		}
	}
}

/*
=================
=
= P_LoadLineDefs
=
= Also counts secret lines for intermissions
=================
*/

void P_LoadLineDefs (int lump)
{
	byte			*data;
	int				i;
	maplinedef_t	*mld;
	line_t			*ld;

	numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
	lines = Z_Malloc (numlines*sizeof(line_t)+16,PU_LEVEL);
	lines = (void*)(((uintptr_t)lines + 15) & ~15); // aline on cacheline boundary
	D_memset (lines, 0, numlines*sizeof(line_t));
	data = W_GetLumpData(lump);
	numlinetags = 0;

	mld = (maplinedef_t *)data;
	ld = lines;
	for (i=0 ; i<numlines ; i++, mld++, ld++)
	{
		int tag;

		ld->flags = LITTLESHORT(mld->flags);
		ld->special = LITTLESHORT(mld->special);
		tag = LITTLESHORT(mld->tag);
		ld->v1 = LITTLESHORT(mld->v1);
		ld->v2 = LITTLESHORT(mld->v2);

		ld->sidenum[0] = LITTLESHORT(mld->sidenum[0]);
		ld->sidenum[1] = LITTLESHORT(mld->sidenum[1]);

		if (tag)
			numlinetags++;

		// HACK HACK HACK
		// backwards compatibility with JagDoom specials
		// that use values typically reserved for Doom2 maps
		if (tag)
			continue;

		switch (ld->special)
		{
		case 99:
			/* Blue Skull Door Open */
			ld->special = 32;
			break;
		case 100:
			/* Red Skull Door Open */
			ld->special = 33;
			break;
		case 105:
			/* Yellow Skull Door Open */
			ld->special = 34;
			break;
		case 106:
			/* Blue Skull Door Raise */
		case 107:
			/* Red Skull Door Raise */
		case 108:
			/* Yellow Skull Door Raise */
			ld->special = ld->special - 80;
			break;
		}
	}

	// load tags into hash table

	if (!numlinetags)
		return;

	numlinetags = (numlinetags + LINETAGS_HASH_SIZE - 1) & ~(LINETAGS_HASH_SIZE - 1);
	linetags = Z_Malloc(sizeof(*linetags)*numlinetags*2, PU_LEVEL);
	D_memset(linetags, -1, sizeof(*linetags)*numlinetags*2);

	mld = (maplinedef_t *)data;
	for (i=0 ; i<numlines ; i++, mld++)
	{
		int tag = LITTLESHORT(mld->tag);
		if (tag) {
			P_SetLineTag(i, tag);
		}
	}
}


/*
=================
=
= P_LoadSideDefs
=
=================
*/

void P_LoadSideDefs (int lump)
{
	byte			*data;
	int				i;
	mapsidedef_t	*msd;
	side_t			*sd;

#ifndef MARS
	for (i=0 ; i<numtextures ; i++)
		textures[i].usecount = 0;
#endif	
	numsides = W_LumpLength (lump) / sizeof(mapsidedef_t);
	sides = Z_Malloc (numsides*sizeof(side_t)+16,PU_LEVEL);
	sides = (void*)(((uintptr_t)sides + 15) & ~15); // aline on cacheline boundary
	D_memset (sides, 0, numsides*sizeof(side_t));
	data = W_GetLumpData(lump);

	msd = (mapsidedef_t *)data;
	sd = sides;
	for (i=0 ; i<numsides ; i++, msd++, sd++)
	{
		int rowoffset, textureoffset;
		textureoffset = LITTLESHORT(msd->textureoffset);
		rowoffset = LITTLESHORT(msd->rowoffset);
		sd->toptexture = R_TextureNumForName(msd->toptexture);
		sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
		sd->midtexture = R_TextureNumForName(msd->midtexture);
		sd->sector = LITTLESHORT(msd->sector);
		sd->rowoffset = rowoffset & 0xff;
		sd->textureoffset = (textureoffset & 0xfff) | ((rowoffset & 0x0f00) << 4);
#ifndef MARS
		textures[sd->toptexture].usecount++;
		textures[sd->bottomtexture].usecount++;
		textures[sd->midtexture].usecount++;
#endif
	}
}


/*
=================
=
= P_LoadRejectMatrix
=
=================
*/

void P_LoadRejectMatrix (int lump)
{
	int i, j;
	uint8_t *data, *out;
	int outsize;
	unsigned outbit, outbyte;

	// compress the reject table, assuming it is symmetrical
	outsize = (numsectors + 1) * numsectors / 2;
	outsize = (outsize + 7) / 8;

	rejectmatrix = Z_Malloc (outsize,PU_LEVEL);
	D_memset(rejectmatrix, 0, outsize);
	data = W_GetLumpData(lump);

	outbit = 1;
	outbyte = 0;
	out = rejectmatrix;

	for (i = 0; i < numsectors; i++) {
		unsigned k = i*numsectors;
		unsigned bit = 1 << ((k + i) & 7);

		for (j = i; j < numsectors; j++) {
			unsigned bytenum = (k + j) / 8;

			if (data[bytenum] & bit)
			{
				out[outbyte] |= outbit;
			}

			bit <<= 1;
			if (bit > 0xff) {
				bit = 1;
			}

			outbit <<= 1;
			if (outbit > 0xff) {
				outbyte++;
				outbit = 1;
			}
		}
	}
}

/*
=================
=
= P_LoadBlockMap
=
=================
*/

void P_LoadBlockMap (int lump)
{
	int		i;
	int		count;

	blockmaplump = Z_Malloc (W_LumpLength (lump),PU_LEVEL);
	W_ReadLump (lump,blockmaplump);
	count = W_LumpLength (lump)/2;
	for (i=0 ; i<count ; i++)
		blockmaplump[i] = LITTLESHORT(blockmaplump[i]);

	bmaporgx = blockmaplump[0]<<FRACBITS;
	bmaporgy = blockmaplump[1]<<FRACBITS;
	bmapwidth = blockmaplump[2];
	bmapheight = blockmaplump[3];

/* clear out mobj chains */
	count = sizeof(*blocklinks)* bmapwidth*bmapheight;
	blocklinks = Z_Malloc (count,PU_LEVEL);
	D_memset (blocklinks, 0, count);
}




/*
=================
=
= P_GroupLines
=
= Builds sector line lists and subsector sector numbers
= Finds block bounding boxes for sectors
=================
*/

void P_GroupLines (void)
{
	VINT		*linebuffer;
	int			i, j, total;
	sector_t	*sector;
	subsector_t	*ss;
	seg_t		*seg;
	line_t		*li;
	fixed_t		bbox[4];

/* look up sector number for each subsector */
	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++)
	{
		int side;
		side_t *sidedef;
		line_t* linedef;

		seg = &segs[ss->firstline];
		linedef = &lines[SEG_UNPACK_LINEDEF(seg)];
		side = SEG_UNPACK_SIDE(seg);
		sidedef = &sides[linedef->sidenum[side]];
		if (side == 1)
		{
			// set the guard flag for the back side, otherwise it
			// may be set to -1 in the loop below
			linedef->flags |= ML_TWOSIDED;
		}
		ss->sector = sidedef->sector;
	}

	/* if the two-sided flag isn't set, set the back side to -1 */
	li = lines;
	for (i=0 ; i<numlines ; i++, li++)
	{
		if (li->sidenum[1] >= 0 && !(li->flags & ML_TWOSIDED)) {
			li->sidenum[1] = -1;
		}
		li->flags &= ~ML_TWOSIDED;
	}

/* count number of lines in each sector */
	li = lines;
	total = 0;
	for (i=0 ; i<numlines ; i++, li++)
	{
		sector_t *front = LD_FRONTSECTOR(li);
		sector_t *back = LD_BACKSECTOR(li);

		front->linecount++;
		total++;
		if (back && back != front)
		{
			back->linecount++;
			total++;
		}
	}
	
/* build line tables for each sector	 */
	linebuffer = Z_Malloc (total*sizeof(*linebuffer), PU_LEVEL);
	sector = sectors;
	for (i=0 ; i<numsectors ; i++, sector++)
	{
		sector->lines = linebuffer;
		linebuffer += sector->linecount;
		sector->linecount = 0;
	}

	li = lines;
	for (i=0 ; i<numlines ; i++, li++)
	{
		sector_t *front = LD_FRONTSECTOR(li);
		sector_t *back = LD_BACKSECTOR(li);

		front->lines[front->linecount++] = i;
		if (back && back != front)
		{
			back->lines[back->linecount++] = i;
		}
	}

	sector = sectors;
	for (i=0 ; i<numsectors ; i++, sector++)
	{
		M_ClearBox (bbox);

		for (j=0 ; j<sector->linecount ; j++)
		{
			li = lines + sector->lines[j];
			M_AddToBox (bbox, vertexes[li->v1].x, vertexes[li->v1].y);
			M_AddToBox (bbox, vertexes[li->v2].x, vertexes[li->v2].y);
		}

		sector->bbox[BOXTOP]=bbox[BOXTOP];
		sector->bbox[BOXBOTTOM]=bbox[BOXBOTTOM];
		sector->bbox[BOXRIGHT]=bbox[BOXRIGHT];
		sector->bbox[BOXLEFT]=bbox[BOXLEFT];
	}
}

/*============================================================================= */


/*
=================
=
= P_LoadingPlaque
=
=================
*/

void P_LoadingPlaque (void)
{
#ifndef MARS
	jagobj_t	*pl;

	pl = W_CacheLumpName ("loading", PU_STATIC);	
	DrawPlaque (pl);
	Z_Free (pl);
#endif
}	

/*============================================================================= */

/*
=================
=
= P_SetupLevel
=
=================
*/
void P_SetupLevel (const char *lumpname, skill_t skill, const char *sky)
{
	int i;
#ifndef MARS
	mobj_t	*mobj;
#endif
	extern VINT	cy;
	VINT lumpnum, lumps[ML_BLOCKMAP+1+MAX_AUX_TEXTURES];
	lumpinfo_t li[ML_BLOCKMAP+1+MAX_AUX_TEXTURES];
	int skytexture;
	boolean havebossspit = false;
	int gamezonemargin;

	M_ClearRandom ();

	P_LoadingPlaque ();
	
D_printf ("P_SetupLevel(%s,%i)\n",lumpname,skill);

	P_InitThinkers ();

	R_ResetTextures();

	if (!sky || !*sky) {
		sky = "SKY1";
	}

	skytexture = W_GetNumForName(sky);
	if (W_IsCompressed(skytexture))
	{
		skytexturep = Z_Malloc (W_LumpLength(skytexture),PU_LEVEL);
		W_ReadLump(skytexture, skytexturep);
	}
	else
		skytexturep = W_POINTLUMPNUM(skytexture);
	skytexturep = R_SkipJagObjHeader(skytexturep, W_LumpLength(skytexture), 256, 128);
	skycolormaps = (col2sky > 0 && skytexture >= col2sky) ? dc_colormaps2 : dc_colormaps;

	W_LoadPWAD(PWAD_CD);

	lumpnum = W_CheckNumForName(lumpname);
	if (lumpnum < 0)
		I_Error("Map %s not found!", lumpname);

	/* build a temp in-memory PWAD */
	for (i = 0; i < ML_BLOCKMAP+1+MAX_AUX_TEXTURES; i++)
		lumps[i] = lumpnum+i;

	W_CacheWADLumps(li, ML_BLOCKMAP+1+MAX_AUX_TEXTURES, lumps, true);

	lumpnum = W_GetNumForName(lumpname);

/* note: most of this ordering is important	 */
	P_LoadLineDefs (lumpnum+ML_LINEDEFS);
	P_LoadSideDefs (lumpnum+ML_SIDEDEFS);
	P_LoadVertexes (lumpnum+ML_VERTEXES);
	P_LoadSegs (lumpnum+ML_SEGS);
	P_LoadSubsectors (lumpnum+ML_SSECTORS);
	P_LoadNodes (lumpnum+ML_NODES);
	P_LoadSectors (lumpnum+ML_SECTORS);
	P_LoadRejectMatrix (lumpnum+ML_REJECT);
	P_LoadBlockMap (lumpnum+ML_BLOCKMAP);

	validcount = Z_Malloc((numlines + 1) * sizeof(*validcount) * 2, PU_LEVEL);
	D_memset(validcount, 0, (numlines + 1) * sizeof(*validcount) * 2);
	validcount[0] = 1; // cpu 0
	validcount[numlines] = 1; // cpu 1

	// G_DeathMatchSpawnPlayer is going to need this
	I_SetThreadLocalVar(DOOMTLS_VALIDCOUNT, &validcount[0]);

	P_GroupLines ();

	if (netgame == gt_deathmatch)
	{
		itemrespawnque = Z_Malloc(sizeof(*itemrespawnque) * ITEMQUESIZE, PU_LEVEL);
		itemrespawntime = Z_Malloc(sizeof(*itemrespawntime) * ITEMQUESIZE, PU_LEVEL);
		deathmatchstarts = Z_Malloc(sizeof(*deathmatchstarts) * MAXDMSTARTS, PU_LEVEL);
	}
	else
	{
		itemrespawnque = NULL;
		itemrespawntime = NULL;
		deathmatchstarts = NULL;
	}

	bodyqueslot = 0;
	deathmatch_p = deathmatchstarts;
	P_LoadThings (lumpnum+ML_THINGS, &havebossspit);

	// load custom replacement textures from ROM/CD to RAM
	{
		boolean istexture = false;
		boolean isflat = false;

		for (i = 0; i < MAX_AUX_TEXTURES; i++)
		{
			int j, k;
			uint8_t *data;
			lumpinfo_t *l;

			k = ML_BLOCKMAP + 1 + i;
			l = &li[k];
			if (!l->name[0])
				break;

			if (!l->size)
			{
				if (!istexture && !D_strcasecmp(l->name, "T_START"))
					istexture = true;
				else if (istexture && !D_strcasecmp(l->name, "T_END"))
					istexture = false;
				else if (!istexture && !D_strcasecmp(l->name, "F_START"))
					isflat = true;
				else if (isflat && !D_strcasecmp(l->name, "F_END"))
					isflat = false;
				else
					break;
				continue;
			}

			// check if it's a texture
			if (istexture) {
				j = R_CheckTextureNumForName (l->name);
				if (j >= 0)
				{
					int n;
					int lumpnum2 = textures[j].lumpnum;
					data = Z_Malloc (l->size, PU_LEVEL);
					W_ReadLump(lumpnum+k, data);
					for (n = 0; n < numtextures; n++)
					{
						if (textures[n].lumpnum == lumpnum2)
							R_SetTextureData(&textures[n], data, l->size, true);
					}
				}
				continue;
			}

			// a flat?
			if (isflat) {
				j = R_FlatNumForName(l->name);
				if (j >= 0) {
					data = Z_Malloc (l->size, PU_LEVEL);
					W_ReadLump(lumpnum + k, data);
					R_SetFlatData(j, data, l->size);
					continue;
				}
			}

			// a sky?
			if (!D_strcasecmp(l->name, sky)) {
				data = Z_Malloc (l->size, PU_LEVEL);
				W_ReadLump(lumpnum + k, data);
				skytexturep = R_SkipJagObjHeader(data, l->size, 256, 128);
				continue;
			}
		}
	}

	W_LoadPWAD(PWAD_NONE);

/* */
/* if deathmatch, randomly spawn the active players */
/* */
	if (netgame == gt_deathmatch)
	{
		int i;
		for (i=0 ; i<MAXPLAYERS ; i++)
			if (playeringame[i])
			{	/* must give a player spot before deathmatchspawn */
				mobj_t *mobj = P_SpawnMobj (deathmatchstarts[0].x<<16
				,deathmatchstarts[0].y<<16,0, MT_PLAYER);
				mobj->player = i + 1;
				players[i].mo = mobj;
				G_DeathMatchSpawnPlayer (i);
				P_RemoveMobj (mobj);
				bodyqueslot = 0; // bodyque shouldn't he holding a reference to removed mobj
			}
	}

/* set up world state */
	P_SpawnSpecials ();
	ST_InitEveryLevel ();
	
/*I_Error("free memory: 0x%x\n", Z_FreeMemory(mainzone)); */
/*printf ("free memory: 0x%x\n", Z_FreeMemory(mainzone)); */

	cy = 4;

#ifdef JAGUAR
{
extern byte *debugscreen;
	D_memset (debugscreen,0,32*224);
	
}
#endif

	iquehead = iquetail = 0;
	gamepaused = false;

	gamezonemargin = DEFAULT_GAME_ZONE_MARGIN;
	if (havebossspit)
		gamezonemargin *= 2;
	R_SetupLevel(gamezonemargin);

#ifdef MARS
	Mars_CommSlaveClearCache();
#endif
}


/*
=================
=
= P_Init
=
=================
*/

void P_Init (void)
{
	anims = Z_Malloc(sizeof(*anims) * MAXANIMS, PU_STATIC);
	switchlist = Z_Malloc(sizeof(*switchlist)* MAXSWITCHES * 2, PU_STATIC);
	buttonlist = Z_Malloc(sizeof(*buttonlist) * MAXBUTTONS, PU_STATIC);
	linespeciallist = Z_Malloc(sizeof(*linespeciallist) * MAXLINEANIMS, PU_STATIC);

	activeplats = Z_Malloc(sizeof(*activeplats) * MAXPLATS, PU_STATIC);
	activeceilings = Z_Malloc(sizeof(*activeceilings) * MAXCEILINGS, PU_STATIC);

	P_InitSwitchList ();
	P_InitPicAnims ();
#ifndef MARS
	pausepic = W_CacheLumpName ("PAUSED",PU_STATIC);
#endif
}



