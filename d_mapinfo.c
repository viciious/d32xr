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

const char* defaultDMAPINFO = ""
"map \"MAP01\" \"Hangar\"\n"
"{\n"
"    sky = \"sky1\"\n"
"    next = \"MAP02\"\n"
"    mapnumber = 1\n"
"}\n"
"map \"MAP02\" \"Plant\"\n"
"{\n"
"    sky = \"sky1\"\n"
"    next = \"MAP03\"\n"
"    mapnumber = 2\n"
"}\n"
"map \"MAP03\" \"Toxin Refinery\"\n"
"{\n"
"    sky = \"sky1\"\n"
"    next = \"MAP04\"\n"
"    secretnext = \"MAP23\"\n"
"    mapnumber = 3\n"
"}\n"
"map \"MAP04\" \"Command Control\"\n"
"{\n"
"    sky = \"sky1\"\n"
"    next = \"MAP05\"\n"
"    mapnumber = 4\n"
"}\n"
"map \"MAP05\" \"Phobos Lab\"\n"
"{\n"
"    sky = \"sky1\"\n"
"    next = \"MAP06\"\n"
"    mapnumber = 5\n"
"}\n"
"map \"MAP06\" \"Central Processing\"\n"
"{\n"
"    sky = \"sky1\"\n"
"    next = \"MAP07\"\n"
"    mapnumber = 6\n"
"}\n"
"map \"MAP07\" \"Computer Station\"\n"
"{\n"
"    sky = \"sky1\"\n"
"    next = \"MAP08\"\n"
"    mapnumber = 7\n"
"}\n"
"map \"MAP08\" \"Phobos Anomaly\"\n"
"{\n"
"    sky = \"sky1\"\n"
"    next = \"MAP09\"\n"
"    mapnumber = 8\n"
"    BaronSpecial\n"
"}\n"
"map \"MAP09\" \"Deimos Anomaly\"\n"
"{\n"
"    sky = \"sky2\"\n"
"    next = \"MAP10\"\n"
"    mapnumber = 9\n"
"}\n"
"map \"MAP10\" \"Containment Area\"\n"
"{\n"
"    sky = \"sky2\"\n"
"    next = \"MAP11\"\n"
"    mapnumber = 10\n"
"}\n"
"map \"MAP11\" \"Refinery\"\n"
"{\n"
"    sky = \"sky2\"\n"
"    next = \"MAP12\"\n"
"    mapnumber = 11\n"
"}\n"
"map \"MAP12\" \"Deimos Lab\"\n"
"{\n"
"    sky = \"sky2\"\n"
"    next = \"MAP13\"\n"
"    mapnumber = 12\n"
"}\n"
"map \"MAP13\" \"Command Center\"\n"
"{\n"
"    sky = \"sky2\"\n"
"    next = \"MAP14\"\n"
"    mapnumber = 13\n"
"}\n"
"map \"MAP14\" \"Halls of the Damned\"\n"
"{\n"
"    sky = \"sky2\"\n"
"    next = \"MAP15\"\n"
"    mapnumber = 14\n"
"}\n"
"map \"MAP15\" \"Spawning Vats\"\n"
"{\n"
"    sky = \"sky2\"\n"
"    next = \"MAP23\"\n"
"    mapnumber = 15\n"
"}\n"
"map \"MAP23\" \"Dis\"\n"
"{\n"
"    sky = \"sky3\"\n"
"    mapnumber = 23\n"
"}\n"
"map \"MAP24\" \"Military Base\"\n"
"{\n"
"    sky = \"sky3\"\n"
"    next = \"MAP04\"\n"
"    mapnumber = 24\n"
"}\n"
"\n"
"";

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

static char* G_LoadMapinfoLump(void)
{
	int len;
	int lump;
	char *buf;

	buf = (char*)I_TempBuffer();

	lump = W_CheckNumForName("DMAPINFO");
	if (lump < 0) {
		len = mystrlen(defaultDMAPINFO);
		D_memcpy(buf, defaultDMAPINFO, len);
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

		p = strchr(l, '\n');
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
		p = strchr(l, '\n');

		if (p)
		{
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

				e = strchr(l, '=');
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

			p = skipspaces(strchr(key, ' '));
			if (p)
			{
				pp = strchr(p + 1, ' ');
				if (pp) *pp = '\0';
				pp = skipspaces(pp + 1);
			}

			mi->lumpnum = W_GetNumForName(stripquote(p));

			p = pp;
			if (p) {
				mi->name = stripquote(p);
			}

			D_memcpy(mi->lumpname, G_GetMapNameForLump(mi->lumpnum), 9);
			mi->mapnumber = G_MapNumForMapName(mi->lumpname);

			if (!mi->name) {
				mi->name = mi->lumpname;
			}
		}
		else if (!D_strcasecmp(key, "baronspecial"))
		{
			mi->baronspecial = true;
		}

		return;
	}

	if (!D_strcasecmp(key, "next"))
		mi->next = W_GetNumForName(value);
	else if (!D_strcasecmp(key, "sky"))
		mi->sky = value;
	else if (!D_strcasecmp(key, "secretnext"))
		mi->secretnext = W_GetNumForName(value);
	else if (!D_strcasecmp(key, "mapnumber"))
		mi->mapnumber = atoi(value);
}

static const char* G_FindMapinfoSection(const char* buf, const char *lumpname, size_t *psectionlen)
{
	int i;
	char name[16];
	const char* section, *ptr;
	size_t namelen, sectionlen;
	char lumpname8[9]; // null-terminated

	for (i = 0; i < 8 && lumpname[i] != '\0'; i++)
		lumpname8[i] = lumpname[i];
	lumpname8[i] = '\0';

	namelen = D_snprintf(name, sizeof(name), "map \"%s\"", lumpname8);
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

int G_FindMapinfo(int maplump, dmapinfo_t *mi)
{
	const char* section;
	const char* buf;
	char* zsection;
	int linecount;
	size_t sectionlen;

	if (maplump < 0)
		return -1;

	D_memset(mi, 0, sizeof(*mi));

	buf = G_LoadMapinfoLump();
	if (!buf)
		return 0;

	section = G_FindMapinfoSection(buf, G_GetMapNameForLump(maplump), &sectionlen);
	if (!section)
		return 0;

	zsection = Z_Malloc(sectionlen + 1, PU_STATIC, NULL);
	D_memcpy(zsection, section, sectionlen);
	zsection[sectionlen] = '\0';

	mi->data = zsection;

	linecount = G_ParseMapinfo(zsection, (kvcall_t)&G_AddMapinfoKey, mi);
	if (linecount < 2)
		goto error;

	return 1;

error:
	if (mi->data)
		Z_Free(mi->data);
	D_memset(mi, 0, sizeof(*mi));
	return 0;
}

static int G_MapInfoSortCmp(const void * pmi1, const void *pmi2)
{
	dmapinfo_t* mi1 = *((dmapinfo_t **)pmi1);
	dmapinfo_t* mi2 = *((dmapinfo_t **)pmi2);
	return mi1->mapnumber - mi2->mapnumber;
}

dmapinfo_t **G_LoadMaplist(int *pmapcount)
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

		mi->data = (byte *)mi;

		linecount = G_ParseMapinfo(zsection, (kvcall_t)&G_AddMapinfoKey, mi);
		if (linecount < 2)
		{
			Z_Free(mi);
			continue;
		}

		maplist[i] = mi;
		i++;
	}
	maplist[i] = NULL;
	*pmapcount = i;

	qsort(maplist, i, sizeof(dmapinfo_t*), &G_MapInfoSortCmp);

	return maplist;
}
