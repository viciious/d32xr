/* P_main.c */

#include "doomdef.h"
#include "p_local.h"
#include "mars.h"
#include "p_camera.h"
#include "st_inter.h"

#define DEFAULT_GAME_ZONE_MARGIN (4*1024)

uint16_t			numvertexes;
uint16_t			numsegs;
uint16_t			numsectors;
uint16_t			numsubsectors;
uint16_t			numnodes;
uint16_t			numlines;
uint16_t			numsides;

mapvertex_t	*vertexes;
seg_t		*segs;
sector_t	*sectors;
subsector_t	*subsectors;
node_t		*nodes;
line_t		*lines;
side_t		*sides;

short		*blockmaplump;			/* offsets in blockmap are from here */
VINT		bmapwidth, bmapheight;	/* in mapblocks */
fixed_t		bmaporgx, bmaporgy;		/* origin of block map */
mobj_t		**blocklinks;			/* for thing chains */

byte		*rejectmatrix;			/* for fast sight rejection */

VINT		*validcount;			/* increment every time a check is made */

mapthing_t	playerstarts[MAXPLAYERS];

uint16_t		numthings;

int16_t worldbbox[4];

#define LOADFLAGS_VERTEXES 1
#define LOADFLAGS_BLOCKMAP 2
#define LOADFLAGS_REJECT 4
#define LOADFLAGS_NODES 8
#define LOADFLAGS_SEGS 16

/*
=================
=
= P_LoadVertexes
=
=================
*/

void P_LoadVertexes (int lump)
{
	numvertexes = W_LumpLength (lump) / sizeof(mapvertex_t);

	if (gamemapinfo.loadFlags & LOADFLAGS_VERTEXES)
	{
		vertexes = Z_Malloc (numvertexes*sizeof(mapvertex_t) + 16,PU_LEVEL);
		vertexes = (void*)(((uintptr_t)vertexes + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, vertexes);
	}
	else
		vertexes = (mapvertex_t*)W_POINTLUMPNUM(lump);
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
	numsegs = W_LumpLength (lump) / sizeof(seg_t);

	if (gamemapinfo.loadFlags & LOADFLAGS_SEGS)
	{
		segs = Z_Malloc (numsegs*sizeof(seg_t) + 16,PU_LEVEL);
		segs = (void*)(((uintptr_t)segs + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, segs);
	}
	else
		segs = (seg_t *)W_POINTLUMPNUM(lump);
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
	mapsubsector_t	*ms;
	mapsubsector_t  *msHead;
	subsector_t		*ss;

	numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
	subsectors = Z_Malloc (numsubsectors*sizeof(subsector_t),PU_LEVEL);
	data = I_TempBuffer ();
	W_ReadLump (lump,data);

	msHead = ms = (mapsubsector_t *)data;
	D_memset (subsectors,0, numsubsectors*sizeof(subsector_t));
	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++, ms++)
	{
		// The segments are already stored in sub sector order,
		// so the number of segments in sub sector n is actually
		// just subsector[n+1].firstseg - subsector[n].firsteg
		ss->firstline = LITTLESHORT(ms->firstseg);

		if (i < numsubsectors - 1)
			ss->numlines = LITTLESHORT(msHead[i+1].firstseg) - ss->firstline;
		else
			ss->numlines = numsegs - ss->firstline;
	}
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
	data = I_TempBuffer ();
	W_ReadLump (lump,data);
	
	ms = (mapsector_t *)data;
	ss = sectors;
	for (i=0 ; i<numsectors ; i++, ss++, ms++)
	{
		ss->floorheight = LITTLESHORT(ms->floorheight)<<FRACBITS;
		ss->ceilingheight = LITTLESHORT(ms->ceilingheight)<<FRACBITS;
		ss->floorpic = ms->floorpic;
		ss->ceilingpic = ms->ceilingpic;
		ss->thinglist = NULL;

		ss->lightlevel = ms->lightlevel;
		ss->special = ms->special;

		ss->tag = ms->tag;
		ss->heightsec = -1; // sector used to get floor and ceiling height
		ss->fofsec = -1;
		ss->floor_xoffs = 0;
		ss->flags = 0;

		// killough 3/7/98:
//		ss->floor_xoffs = 0;
//		ss->floor_yoffs = 0;      // floor and ceiling flats offsets
	}
}


/*
=================
=
= P_LoadNodes
=
=================
*/

void P_LoadNodes (int lump)
{
	numnodes = W_LumpLength (lump) / sizeof(node_t);

	if (gamemapinfo.loadFlags & LOADFLAGS_NODES)
	{
		nodes = Z_Malloc (numnodes*sizeof(node_t) + 16,PU_LEVEL);
		nodes = (void*)(((uintptr_t)nodes + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, nodes);
	}
	else
		nodes = (node_t *)W_POINTLUMPNUM(lump);

	// Calculate worldbbox
	worldbbox[BOXLEFT] = INT16_MAX;
	worldbbox[BOXRIGHT] = INT16_MIN;
	worldbbox[BOXBOTTOM] = INT16_MAX;
	worldbbox[BOXTOP] = INT16_MIN;
	for (int i = 0; i < numvertexes; i++)
	{
		const mapvertex_t *v = &vertexes[i];
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
= P_LoadThings
=
=================
*/
fixed_t P_GetMapThingSpawnHeight(const mobjtype_t mobjtype, const mapthing_t* mthing, const fixed_t x, const fixed_t y, const fixed_t z);

static void P_SpawnItemRow(mapthing_t *mt, VINT type, VINT count, VINT horizontalSpacing, VINT verticalSpacing)
{
	mapthing_t dummything = *mt;
	dummything.type = type;

	int mobjtype;
	/* find which type to spawn */
	for (mobjtype=0 ; mobjtype< NUMMOBJTYPES ; mobjtype++)
		if (mt->type == mobjinfo[mobjtype].doomednum)
			break;

	fixed_t spawnX = dummything.x << FRACBITS;
	fixed_t spawnY = dummything.y << FRACBITS;
	fixed_t spawnZ = P_GetMapThingSpawnHeight(mobjtype, mt, spawnX, spawnY, (mt->options >> 4) << FRACBITS);

	angle_t spawnAngle = dummything.angle * ANGLE_1;

	for (int i = 0; i < count ; i++)
	{
		P_ThrustValues(spawnAngle, horizontalSpacing << FRACBITS, &spawnX, &spawnY);

		const subsector_t *ss = R_PointInSubsector(spawnX, spawnY);

		fixed_t curZ = spawnZ + ((i+1) * (verticalSpacing<<FRACBITS)); // Literal height

		// Now we have to work backwards and calculate the mapthing z
		VINT mapthingZ = (curZ - ss->sector->floorheight) >> FRACBITS;
		mapthingZ <<= 4;
		dummything.options &= 15; // Clear the top 12 bits
		dummything.options |= mapthingZ; // Insert our new value

		dummything.x = spawnX >> FRACBITS;
		dummything.y = spawnY >> FRACBITS;

		P_SpawnMapThing(&dummything, mobjtype);
	}
}

void P_LoadThings (int lump)
{
	byte			*data;
	int				i;
	mapthing_t		*mt;
	short			numthingsreal, numstaticthings, numringthings;

	for (int i = 0; i < NUMMOBJTYPES; i++)
		ringmobjtics[i] = -1;

	data = I_TempBuffer ();
	W_ReadLump (lump,data);
	numthings = W_LumpLength (lump) / sizeof(mapthing_t);
	numthingsreal = 0;
	numstaticthings = 0;
	numringthings = 0;
	numscenerymobjs = 0;

	mt = (mapthing_t *)data;
	for (i=0 ; i<numthings ; i++, mt++)
	{
		mt->x = LITTLESHORT(mt->x);
		mt->y = LITTLESHORT(mt->y);
		mt->angle = LITTLESHORT(mt->angle);
		mt->type = LITTLESHORT(mt->type);
		mt->options = LITTLESHORT(mt->options);
	}

	mt = (mapthing_t *)data;
	for (i=0 ; i<numthings ; i++, mt++)
	{
		switch (P_MapThingSpawnsMobj(mt))
		{
			case 0:
				break;
			case 1:
				numthingsreal++;
				break;
			case 2:
				numstaticthings++;
				break;
			case 3:
				if (mt->type >= 600 && mt->type <= 602)
					numringthings += 5;
				else if (mt->type == 603)
					numringthings += 10;
				else
					numringthings++;
				break;
			case 4:
				numscenerymobjs++;
				break;
		}
	}

	// preallocate a few mobjs for puffs and projectiles
	numthingsreal += 48; // 32 rings, plus other items
	numstaticthings += 8;

	if (gamemapinfo.act == 3)
	{
		// Bosses spawn lots of stuff, so preallocate more.
		numthingsreal += 96;
		numstaticthings += 64;
	}
	P_PreSpawnMobjs(numthingsreal, numstaticthings, numringthings, numscenerymobjs);

	mt = (mapthing_t *)data;
	for (i=0 ; i<numthings ; i++, mt++)
	{
		if (mt->type == 600) // 5 vertical rings (yellow spring)
			P_SpawnItemRow(mt, mobjinfo[MT_RING].doomednum, 5, 0, 64);
		else if (mt->type == 601) // 5 vertical rings (red spring)
			P_SpawnItemRow(mt, mobjinfo[MT_RING].doomednum, 5, 0, 128);
		else if (mt->type == 602) // 5 diagonal rings (yellow spring)
			P_SpawnItemRow(mt, mobjinfo[MT_RING].doomednum, 5, 64, 64);
		else if (mt->type == 603) // 10 diagonal rings (yellow spring)
			P_SpawnItemRow(mt, mobjinfo[MT_RING].doomednum, 10, 64, 64);
		else
			P_SpawnMapThing(mt, i);
	}

	camBossMobj = P_FindFirstMobjOfType(MT_EGGMOBILE);
	if (!camBossMobj)
		camBossMobj = P_FindFirstMobjOfType(MT_EGGMOBILE2);

	if (players[0].starpostnum)
		P_SetStarPosts(players[0].starpostnum + 1);
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
	mapvertex_t		*v1, *v2;
	
	numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
	lines = Z_Malloc (numlines*sizeof(line_t)+16,PU_LEVEL);
	lines = (void*)(((uintptr_t)lines + 15) & ~15); // aline on cacheline boundary
	D_memset (lines, 0, numlines*sizeof(line_t));
	data = I_TempBuffer ();
	W_ReadLump (lump,data);

	mld = (maplinedef_t *)data;
	ld = lines;
	for (i=0 ; i<numlines ; i++, mld++, ld++)
	{
		fixed_t dx,dy;
		ld->flags = LITTLESHORT(mld->flags);
		ld->special = mld->special;
		ld->tag = mld->tag;
		ld->v1 = LITTLESHORT(mld->v1);
		ld->v2 = LITTLESHORT(mld->v2);
		
		v1 = &vertexes[ld->v1];
		v2 = &vertexes[ld->v2];
		dx = (v2->x - v1->x) << FRACBITS;
		dy = (v2->y - v1->y) << FRACBITS;
		if (!dx)
			ld->flags |= ML_ST_VERTICAL;
		else if (!dy)
			ld->flags |= ML_ST_HORIZONTAL;
		else
		{
			if (FixedDiv (dy , dx) > 0)
				ld->flags |= ML_ST_POSITIVE;
			else
				ld->flags |= ML_ST_NEGATIVE;
		}

		ld->sidenum[0] = LITTLESHORT(mld->sidenum[0]);
		ld->sidenum[1] = LITTLESHORT(mld->sidenum[1]);

		// if the two-sided flag isn't set, set the back side to -1
		if (ld->sidenum[1] != -1) {
			if (!(ld->flags & ML_TWOSIDED)) {
				ld->sidenum[1] = -1;
			}
		}
		ld->flags &= ~ML_TWOSIDED;
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
	data = I_TempBuffer ();
	W_ReadLump (lump,data);
	
	msd = (mapsidedef_t *)data;
	sd = sides;
	for (i=0 ; i<numsides ; i++, msd++, sd++)
	{
		sd->sector = LITTLESHORT(msd->sector);
		sd->rowoffset = msd->rowoffset;
		sd->textureoffset = LITTLESHORT(msd->textureoffset);
		sd->toptexture = msd->toptexture;
		sd->midtexture = msd->midtexture;
		sd->bottomtexture = msd->bottomtexture;

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
= P_LoadBlockMap
=
=================
*/

void P_LoadBlockMap (int lump)
{
	int		count;

	if (gamemapinfo.loadFlags & LOADFLAGS_BLOCKMAP)
	{
		blockmaplump = Z_Malloc (W_LumpLength(lump) + 16,PU_LEVEL);
		blockmaplump = (void*)(((uintptr_t)blockmaplump + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, blockmaplump);
	}
	else
		blockmaplump = (short *)W_POINTLUMPNUM(lump);

	bmaporgx = blockmaplump[0]<<FRACBITS;
	bmaporgy = blockmaplump[1]<<FRACBITS;
	bmapwidth = blockmaplump[2];
	bmapheight = blockmaplump[3];
	
/* clear out mobj chains */
	count = sizeof(*blocklinks)* bmapwidth*bmapheight;
	blocklinks = Z_Malloc (count,PU_LEVEL);
	D_memset (blocklinks, 0, count);
}

void P_LoadReject (int lump)
{
	if (gamemapinfo.loadFlags & LOADFLAGS_REJECT)
	{
		rejectmatrix = Z_Malloc (W_LumpLength(lump) + 16,PU_LEVEL);
		rejectmatrix = (void*)(((uintptr_t)rejectmatrix + 15) & ~15); // aline on cacheline boundary
		W_ReadLump(lump, rejectmatrix);
	}
	else
		rejectmatrix = (byte *)W_POINTLUMPNUM(lump);

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
	int			i, total;
	sector_t	*sector;
	subsector_t	*ss;
	seg_t		*seg;
	line_t		*li;

/* look up sector number for each subsector */
	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++)
	{
		side_t *sidedef;
		line_t* linedef;

		seg = &segs[ss->firstline];
		linedef = &lines[seg->linedef];
		sidedef = &sides[linedef->sidenum[seg->sideoffset & 1]];
		ss->sector = &sectors[sidedef->sector];
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
			back->lines[back->linecount++] = i;
	}
}

/*============================================================================= */

/*
=================
=
= P_SetupLevel
=
=================
*/

void P_SetupLevel (int lumpnum)
{
#ifndef MARS
	mobj_t	*mobj;
#endif
	extern	int	cy;
	int gamezonemargin;
	
	M_ClearRandom ();

	stagefailed = true;
	leveltime = 0;
	
D_printf ("P_SetupLevel(%i)\n",lumpnum);

	P_InitThinkers ();

	R_ResetTextures();

/* note: most of this ordering is important	 */
	P_LoadBlockMap (lumpnum+ML_BLOCKMAP);
	P_LoadVertexes (lumpnum+ML_VERTEXES);
	P_LoadSectors (lumpnum+ML_SECTORS);
	P_LoadSideDefs (lumpnum+ML_SIDEDEFS);
	P_LoadLineDefs (lumpnum+ML_LINEDEFS);
	P_LoadSegs (lumpnum+ML_SEGS);
	P_LoadSubsectors (lumpnum+ML_SSECTORS);
	P_LoadNodes (lumpnum+ML_NODES);
	P_LoadReject(lumpnum+ML_REJECT);

	validcount = Z_Malloc((numlines + 1) * sizeof(*validcount) * 2, PU_LEVEL);
	D_memset(validcount, 0, (numlines + 1) * sizeof(*validcount) * 2);
	validcount[0] = 1; // cpu 0
	validcount[numlines] = 1; // cpu 1

	P_GroupLines ();

	P_LoadThings (lumpnum+ML_THINGS);

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

	gamepaused = false;

	gamezonemargin = DEFAULT_GAME_ZONE_MARGIN;
	R_SetupLevel(gamezonemargin);

	I_SetThreadLocalVar(DOOMTLS_VALIDCOUNT, &validcount[0]);

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
	linespeciallist = Z_Malloc(sizeof(*linespeciallist) * MAXLINEANIMS, PU_STATIC);

	activeplats = Z_Malloc(sizeof(*activeplats) * MAXPLATS, PU_STATIC);
	activeceilings = Z_Malloc(sizeof(*activeceilings) * MAXCEILINGS, PU_STATIC);

	P_InitPicAnims ();
#ifndef MARS
	pausepic = W_CacheLumpName ("PAUSED",PU_STATIC);
#endif
}
