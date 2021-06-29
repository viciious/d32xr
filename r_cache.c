/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2021 Victor Luchits, Derek John Evans, id Software and ZeniMax Media

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "doomdef.h"
#include "r_local.h"

#define R_CheckPixels(lumpnum) (void *)((intptr_t)(W_POINTLUMPNUM(lumpnum)))

/*
================
=
= R_InitTexCache
=
=================
*/
void R_InitTexCache(r_texcache_t* c, int maxobjects)
{
	D_memset(c, 0, sizeof(*c));
	c->bestobj = -1;

#ifdef MARS
	c->maxobjects = maxobjects;

	if (!maxobjects)
	{
		c->framecount = NULL;
		c->pixcount = NULL;
		return;
	}

	c->framecount = Z_Malloc(maxobjects * sizeof(*c->framecount), PU_STATIC, 0);
	c->pixcount = Z_Malloc(maxobjects * sizeof(*c->pixcount), PU_STATIC, 0);

	D_memset(c->framecount, 0, maxobjects * sizeof(*c->framecount));
	D_memset(c->pixcount, 0, maxobjects * sizeof(*c->pixcount));
#endif
}

/*
================
=
= R_InitTexCacheZone
=
=================
*/
void R_InitTexCacheZone(r_texcache_t* c, int zonesize)
{
	if (!zonesize)
	{
		c->zone = NULL;
		return;
	}

	c->zone = Z_Malloc(zonesize, PU_LEVEL, 0);

	Z_InitZone(c->zone, zonesize);
}

/*
================
=
= R_SetupTexCacheFrame
=
=================
*/
void R_SetupTexCacheFrame(r_texcache_t* c)
{
	c->bestcount = 0;
	c->bestobj = -1;
}

/*
================
=
= R_TestTexCacheCandidate
=
=================
*/
void R_TestTexCacheCandidate(r_texcache_t* c, int id)
{
	VINT* framec = c->framecount;
	unsigned short* pixcount = c->pixcount;
	
	if (!c->zone)
		return;

	if (framec[id] == 0 || framec[id] != framecount - 1)
		return;

	if (pixcount[id] > c->bestcount)
	{
		c->bestcount = pixcount[id];
		c->bestobj = id;
	}
}

/*
================
=
= R_AddPixelsToTexCache
=
=================
*/
void R_AddPixelsToTexCache(r_texcache_t* c, int id, int pixels)
{
	VINT* framec = c->framecount;
	unsigned short* pixcount = c->pixcount;

	if (!c->zone)
		return;

	if (framec[id] != framecount)
	{
		framec[id] = framecount;
		pixcount[id] = 0;
	}

	if (pixels + pixcount[id] >= 0xffff)
		pixcount[id] = 0xffff;
	else
		pixcount[id] += pixels;
}

/*
================
=
= R_UpdateCachedPixelcount
=
=================
*/
static void R_UpdateCachedPixelcount(void* ptr, void* userp)
{
	r_texcache_t* c = userp;
	texcacheblock_t* entry = ptr;
	int id = entry->id;

	if (!c->zone)
		return;

	if (c->framecount[id] == framecount)
		entry->pixelcount = c->pixcount[id];
	else
		entry->pixelcount = 0;

	// reset for the next frame so that it won't fight for cache in the next frame
	c->pixcount[id] = 0;
}

/*
================
=
= R_PostTexCacheFrame
=
=================
*/
void R_PostTexCacheFrame(r_texcache_t* c)
{
	if (!c->zone)
		return;

	Z_ForEachBlock(c->zone, &R_UpdateCachedPixelcount, c);
}

/*
================
=
= R_EvictFromTexCache
=
=================
*/
static void R_EvictFromTexCache(void* ptr, void* userp)
{
	texcacheblock_t* entry = ptr;
	r_texcache_t *c = userp;

	if (entry->pixelcount < c->reqcount)
	{
		*entry->userp = R_CheckPixels(entry->lumpnum);
		Z_Free2(c->zone, entry);
	}
}

/*
================
=
= R_AddToTexCache
=
=================
*/
void R_AddToTexCache(r_texcache_t* c, int id, int pixels, int lumpnum, void **userp)
{
	int size;
	void* data, * lumpdata;
	texcacheblock_t* entry;

	if (!c || !c->zone)
		return;
	if (id < 0)
		return;

	c->reqcount = c->pixcount[id];

	size = pixels + sizeof(texcacheblock_t) + 32;
	if (Z_LargestFreeBlock(c->zone) < size + 32)
	{
		// free unused entries when under pressure
		Z_ForEachBlock(c->zone, &R_EvictFromTexCache, c);

		// try allocating again
		if (Z_LargestFreeBlock(c->zone) < size + 32)
			return;
	}

	entry = Z_Malloc2(c->zone, size, PU_LEVEL, NULL);
	entry->id = id;
	entry->pixelcount = c->pixcount[id];
	entry->lumpnum = lumpnum;
	entry->userp = userp;

	lumpdata = R_CheckPixels(lumpnum);

	// align to the same cache line remainder
	data = (byte*)entry + sizeof(texcacheblock_t);
	data = (void *)(((intptr_t)data + 15) & ~15);
	data = (void *)((intptr_t)data + ((intptr_t)lumpdata & 15));

	D_memcpy(data, lumpdata, pixels);
	//D_memset(data, id&255, pixels); // DEBUG

	*userp = data;
}
