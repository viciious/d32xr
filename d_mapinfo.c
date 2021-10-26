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
#include <string.h>
#include <stdlib.h>

typedef void (*kvcall_t) (char *key, char *value, void *ptr);

int G_LumpNumForMapNum(int map)
{
	char            lumpname[8];

	lumpname[0] = 'M';
	lumpname[1] = 'A';
	lumpname[2] = 'P';
	lumpname[3] = '0' + map / 10;
	lumpname[4] = '0' + map % 10;
	lumpname[5] = 0;

	return W_CheckNumForName(lumpname);
}

int G_MapNumForMapName(const char* map)
{
	if (mystrlen(map) != 5)
		return 0;
	if (D_strncasecmp(map, "MAP", 3))
		return 0;
	if ((map[3] >= '0' && map[3] <= '9') && (map[4] >= '0' && map[4] <= '9'))
		return (map[3] - '0') * 10 + (map[4] - '0');
	return 0;
}

char* G_GetMapNameForLump(int lump)
{
	static char name[9];
	D_memcpy(name, W_GetNameForNum(lump), 8);
	name[8] = '0';
	return name;
}

int G_MapNumForLumpNum(int lump)
{
	dmapinfo_t mapinfo;
	const char* mapname;
	char buf[512];

	if (G_FindMapinfo(lump, &mapinfo, buf) != 0) {
		return mapinfo.mapNumber;
	}

	mapname = G_GetMapNameForLump(lump);
	return G_MapNumForMapName(mapname);
}

static char* G_LoadMapinfoLump(void)
{
	int len;
	int lump;
	char *buf;

	buf = (char*)I_WorkBuffer();

	lump = W_CheckNumForName("DMAPINFO");
	if (lump < 0) {
		return NULL;
	}
	else {
		len = W_LumpLength(lump);
		W_ReadLump(lump, buf);
	}
	buf[len] = '\0';

	return buf;
}

static const char* G_FindNextMapinfoSection(const char *buf, size_t *len)
{
	const char *l = buf;
	const char* sectionstart = NULL;
	int state = 0; // -1 == exit, expect section, expect {, expect property or }

	if (!buf || !*buf)
		return NULL;

	while (l) {
		const char* p;
		boolean bracket;
		
		if (state < 0)
			break;

		p = D_strchr(l, '\n');
		while (*l == ' ' || *l == '\t') l++;
		bracket = l[0] == '{' || l[0] == '}';

		switch (state) {
		case 0:
			if (bracket)
			{
				state = -1;
			}
			else if (*l != '\n' && *l != '\0')
			{
				sectionstart = l;
				state++;
			}
			break;
		case 1:
			if (l[0] != '{')
				state = -1;
			else
				state++;
			break;
		case 2:
			if (l[0] == '}')
				state = 0;
			else if (bracket)
				state = -1;
			break;
		default:
			state = -1;
			break;
		}

		if (state == 0 && sectionstart != NULL)
		{
			*len = p - sectionstart;
			return sectionstart;
		}

		if (!p) break;
		l = p + 1;
	}

	return NULL;
}

static char* skipspaces(char* p)
{
	while (p && *p == ' ') p++;
	return p;
}

static char* stripquote(char* s)
{
	boolean strip = false;
	size_t slen;

	if (*s == '"') {
		s++;
		strip = true;
	}
	slen = mystrlen(s);
	if (strip && s[slen - 1] == '"') {
		s[slen - 1] = '\0';
	}

	return s;
}

static int G_ParseMapinfo(char* buf, kvcall_t kvcall, void *ptr)
{
	char* l = buf;
	int linecount = 0;

	if (!buf || !*buf)
		return 0;

	while (l) {
		char *p, *e, *el;
		p = D_strchr(l, '\n');

		if (p)
		{
			if (*(p - 1) == '\r') --p;
			*p = '\0';
			el = p;
		}
		else
		{
			el = l + mystrlen(l);
		}
		while (*l == ' ' || *l == '\t') l++;

		if (*l != '\0' && *l != '}' && *l != '{') {
			if (linecount > 0) { // skip the first line
				char* val = NULL;

				e = D_strchr(l, '=');
				if (e && e + 1 != el) {
					val = e + 1;

					while (*val == ' ' || *val == '\t') val++;
					while (*(e - 1) == ' ' || *(e - 1) == '\t') e--;
					*e = '\0';

					val = stripquote(val);
				}

				kvcall(l, val, ptr);
			}
			else {
				kvcall(l, NULL, ptr);
			}
		}

		if (!p)
			break;

		l = p + 1;
		linecount++;
	}

	return linecount;
}

static void G_AddMapinfoKey(char* key, char* value, dmapinfo_t* mi)
{
	if (!value) {
		char* p;

		if (!D_strncasecmp(key, "map ", 4)) {
			char* pp = NULL;

			p = skipspaces(D_strchr(key, ' '));
			if (p)
			{
				pp = D_strchr(p + 1, ' ');
				if (pp) *pp = '\0';
				pp = skipspaces(pp + 1);
			}

			mi->lumpNum = W_GetNumForName(stripquote(p));

			p = pp;
			if (p) {
				mi->name = stripquote(p);
			}

			D_memcpy(mi->lumpName, G_GetMapNameForLump(mi->lumpNum), 9);
			mi->mapNumber = G_MapNumForMapName(mi->lumpName);

			if (!mi->name) {
				mi->name = mi->lumpName;
			}
		}
		else if (!D_strcasecmp(key, "baronspecial"))
		{
			mi->baronSpecial = true;
		}

		return;
	}

	if (!D_strcasecmp(key, "next"))
		mi->next = W_GetNumForName(value);
	else if (!D_strcasecmp(key, "sky"))
		mi->sky = value;
	else if (!D_strcasecmp(key, "secretnext"))
		mi->secretNext = W_GetNumForName(value);
	else if (!D_strcasecmp(key, "mapnumber"))
		mi->mapNumber = D_atoi(value);
	else if (!D_strcasecmp(key, "music"))
		mi->musicLump = W_CheckNumForName(value);
}

static void G_AddGameinfoKey(char* key, char* value, dgameinfo_t* gi)
{
	if (!D_strcasecmp(key, "borderFlat"))
		gi->borderFlat = W_CheckNumForName(value);
	else if (!D_strcasecmp(key, "titleTime"))
		gi->titleTime = D_atoi(value);
	else if (!D_strcasecmp(key, "creditsTime"))
		gi->creditsTime = D_atoi(value);
	else if (!D_strcasecmp(key, "titlePage"))
		gi->titlePage = W_CheckNumForName(value);
	else if (!D_strcasecmp(key, "creditsPage"))
		gi->creditsPage = W_CheckNumForName(value);
	else if (!D_strcasecmp(key, "titleMus"))
		gi->titleMus = W_CheckNumForName(value);
	else if (!D_strcasecmp(key, "intermissionMus"))
		gi->intermissionMus = W_CheckNumForName(value);
	else if (!D_strcasecmp(key, "victoryMus"))
		gi->victoryMus = W_CheckNumForName(value);
}

static const char* G_FindMapinfoSection(const char* buf, const char *name, size_t *psectionlen)
{
	const char* section, *ptr;
	size_t namelen, sectionlen;

	namelen = mystrlen(name);
	*psectionlen = 0;

	section = NULL;
	sectionlen = 0;
	for (ptr = buf; ; ptr = section + sectionlen + 1) {
		section = G_FindNextMapinfoSection(ptr, &sectionlen);
		if (!section)
			break;
		if (D_strncasecmp(section, name, namelen))
			continue;
		*psectionlen = sectionlen;
		return section;
	}

	return NULL;
}

static char *G_MapinfoSectionCStr(const char* buf, const char *name, char *outmem)
{
	char* newstr;
	const char* section;
	size_t sectionlen;

	section = G_FindMapinfoSection(buf, name, &sectionlen);
	if (!section)
		return NULL;

	if (outmem)
		newstr = outmem;
	else
		newstr = Z_Malloc(sectionlen + 1, PU_STATIC, NULL);
	D_memcpy(newstr, section, sectionlen);
	newstr[sectionlen] = '\0';

	return newstr;
}

int G_FindMapinfo(VINT maplump, dmapinfo_t *mi, char *outmem)
{
	int i;
	const char* buf;
	int linecount;
	const char* lumpname;
	char lumpname8[9]; // null-terminated
	char name[16];

	if (maplump < 0)
		return -1;

	buf = G_LoadMapinfoLump();
	if (!buf)
		return 0;

	lumpname = G_GetMapNameForLump(maplump);
	for (i = 0; i < 8 && lumpname[i] != '\0'; i++)
		lumpname8[i] = lumpname[i];
	lumpname8[i] = '\0';
	D_snprintf(name, sizeof(name), "map \"%s\"", lumpname8);

	D_memset(mi, 0, sizeof(*mi));
	mi->data = G_MapinfoSectionCStr(buf, name, outmem);
	if (!mi->data)
		return 0;

	linecount = G_ParseMapinfo(mi->data, (kvcall_t)&G_AddMapinfoKey, mi);
	if (linecount < 2)
		goto error;

	return 1;

error:
	if (mi->data)
		Z_Free(mi->data);
	D_memset(mi, 0, sizeof(*mi));
	return 0;
}

int G_FindGameinfo(dgameinfo_t* gi)
{
	const char* buf;
	int linecount;

	buf = G_LoadMapinfoLump();
	if (!buf)
		return 0;

	D_memset(gi, 0, sizeof(*gi));
	gi->data = G_MapinfoSectionCStr(buf, "gameinfo", NULL);
	if (!gi->data)
		return 0;

	linecount = G_ParseMapinfo(gi->data, (kvcall_t)&G_AddGameinfoKey, gi);
	if (linecount < 2)
		goto error;

	gi->titleMus = gi->titleMus > 0 ? S_SongForLump(gi->titleMus) : 0;
	gi->intermissionMus = gi->intermissionMus > 0 ? S_SongForLump(gi->intermissionMus) : 0;
	gi->victoryMus = gi->victoryMus > 0 ? S_SongForLump(gi->victoryMus) : 0;

	return 1;

error:
	if (gi->data)
		Z_Free(gi->data);
	D_memset(gi, 0, sizeof(*gi));
	return 0;
}

dmapinfo_t **G_LoadMaplist(VINT *pmapcount)
{
	const char* buf;
	size_t sectionlen = 0;
	const char* section = NULL, * ptr;
	int mapcount, i;
	dmapinfo_t** maplist;

	buf = G_LoadMapinfoLump();
	if (!buf)
		return NULL;

	mapcount = 0;
	*pmapcount = 0;

	section = NULL;
	sectionlen = 0;
	for (ptr = buf; ; ptr = section + sectionlen + 1) {
		section = G_FindNextMapinfoSection(ptr, &sectionlen);
		if (!section)
			break;
		mapcount++;
	}

	if (!mapcount)
		return NULL;

	maplist = Z_Malloc(sizeof(*maplist) * (mapcount + 1), PU_STATIC, NULL);

	i = 0;

	section = NULL;
	sectionlen = 0;
	for (ptr = buf; ; ptr = section + sectionlen + 1) {
		dmapinfo_t* mi;
		char* zsection;
		int linecount;

		section = G_FindNextMapinfoSection(ptr, &sectionlen);
		if (!section)
			break;

		mi = Z_Malloc(sizeof(dmapinfo_t) + sectionlen + 1, PU_STATIC, NULL);
		zsection = (char *)mi + sizeof(dmapinfo_t);
		D_memcpy(zsection, section, sectionlen);
		zsection[sectionlen] = '\0';

		memset(mi, 0, sizeof(*mi));
		mi->data = (byte *)mi;

		linecount = G_ParseMapinfo(zsection, (kvcall_t)&G_AddMapinfoKey, mi);
		if (linecount < 2 || mi->mapNumber <= 0)
		{
			Z_Free(mi);
			continue;
		}

		maplist[i] = mi;
		i++;
	}
	maplist[i] = NULL;

	mapcount = i;
	*pmapcount = mapcount;

	// sort by mapnumber (insertion sort)
	for (i = 1; i < mapcount; i++)
	{
		int j;
		for (j = i; j > 0; j--)
		{
			if (maplist[j - 1]->mapNumber <= maplist[j]->mapNumber)
				break;
			dmapinfo_t *t = maplist[j - 1];
			maplist[j - 1] = maplist[j];
			maplist[j] = t;
		}
	}

	return maplist;
}
