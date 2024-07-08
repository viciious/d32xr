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

/*
================
=
= R_InitTexCache
=
=================
*/
void R_InitTexCache(r_texcache_t* c)
{
	D_memset(c, 0, sizeof(*c));
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

	c->zone = Z_Malloc(zonesize, PU_LEVEL);
	c->zonesize = zonesize;
	Z_InitZone(c->zone, zonesize);
}

/*
================
=
= R_InRam
=
=================
*/
static boolean R_InRam(void *p)
{
	if (((uintptr_t)p >= (uintptr_t)mainzone && (uintptr_t)p < (uintptr_t)mainzone + mainzone->size)) {
		return true;
	}
	return false;
}

/*
================
=
= R_InTexCache
=
=================
*/
int R_InTexCache(r_texcache_t* c, void *p)
{
	if (((uintptr_t)p >= (uintptr_t)c->zone && (uintptr_t)p < (uintptr_t)c->zone + c->zonesize)) {
		return 1;
	}
	if (R_InRam(p)) {
		return 2;
	}
	return 0;
}

/*
================
=
= R_TouchIfInTexCache
=
=================
*/
boolean R_TouchIfInTexCache(r_texcache_t* c, void *p)
{
	int s = R_InTexCache(c, p);

	if (s == 2) {
		// in ram
		return true;
	}
	if (s == 1) {
		// in texture cache
		texcacheblock_t *e = *(texcacheblock_t **)(((uintptr_t)p - 4) & ~3);
		e->lifecount = CACHE_FRAMES_DEFAULT;
		return true;
	}

	return false;
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

	if (entry->pixels < c->reqcount_le)
		if (entry->lifecount != CACHE_FRAMES_DEFAULT)
			entry->lifecount = 0;

	if (entry->lifecount <= 0)
	{
		c->reqfreed++;
		*entry->userp = entry->userpold;
		Z_Free2(c->zone, entry);
	}
}

static void R_AgeTexCacheEntries(void* ptr, void* userp)
{
	texcacheblock_t* entry = ptr;
	entry->lifecount--;
	if (entry->lifecount < 0)
		entry->lifecount = 0;
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
	Z_ForEachBlock(c->zone, &R_AgeTexCacheEntries, c);
}

/*
================
=
= R_AddToTexCache
=
=================
*/
void R_AddToTexCache(r_texcache_t* c, int id, int pixels, void **userp)
{
	int size;
	int trynum;
	const int pad = 16;
	void* data, * lumpdata;
	texcacheblock_t* entry, **ref;

	if (!c || !c->zone)
		return;
	if (debugmode == DEBUGMODE_NOTEXCACHE)
		return;

	size = pixels + sizeof(texcacheblock_t) + 36;
	if (c->zonesize < size + pad)
		return;

	if (Z_LargestFreeBlock(c->zone) < size + pad)
	{
		// free less frequently used entries 
		c->reqfreed = 0;
		c->reqcount_le = pixels;
		Z_ForEachBlock(c->zone, &R_EvictFromTexCache, c);

		// check if there were textures that got freed
		if (c->reqfreed == 0)
			return;

		if (Z_LargestFreeBlock(c->zone) < size + pad)
		{
			// check for fragmentation
			//if (Z_FreeBlocks(c->zone) > size + pad)
			//	R_ClearTexCache(c);
			return;
		}
	}

	trynum = 0;
retry:
	entry = Z_Malloc2(c->zone, size, PU_LEVEL, false);
	if (!entry)
	{
		if (trynum != 0)
			return;
		R_ClearTexCache(c);
		trynum++;
		goto retry;
	}

	entry->pixels = pixels;
	entry->userp = userp;
	entry->userpold = *userp;
	entry->lifecount = CACHE_FRAMES_DEFAULT;

	lumpdata = *userp;

	// align pointers so that the 4 least significant bits match
	data = (byte*)entry + sizeof(texcacheblock_t) + 20;
	data = (void*)((uintptr_t)data & ~15);
	data = (void *)((uintptr_t)data + ((uintptr_t)lumpdata & 15));

	// pointer to cache entry always preceeds the actual data
	ref = (void *)(((uintptr_t)data - 4) & ~3);
	*ref = entry;

	D_memcpy(data, lumpdata, pixels);
	if (debugmode == DEBUGMODE_TEXCACHE)
	{
		D_memset(data, id & 255, pixels); // DEBUG
	}

	*userp = data;
}

/*
================
=
= R_ForceEvictFromTexCache
=
=================
*/
static void R_ForceEvictFromTexCache(void* ptr, void* userp)
{
	texcacheblock_t* entry = ptr;
	r_texcache_t* c = userp;

	*entry->userp = entry->userpold;
	Z_Free2(c->zone, entry);
}

/*
================
=
= R_ClearTexCache
=
=================
*/
void R_ClearTexCache(r_texcache_t* c)
{
	if (!c || !c->zone)
		return;
	Z_ForEachBlock(c->zone, &R_ForceEvictFromTexCache, c);
	Z_InitZone(c->zone, c->zonesize);
}
