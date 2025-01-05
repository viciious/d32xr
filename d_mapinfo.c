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

typedef struct
{
	VINT specials;
	VINT mapNumber;
	uint8_t songNum;
	uint8_t cdaNum;
	char *sky;
	char *name;
	char *next;
	char *secretNext;
	char *lumpName;
	char *interText;
	char *secretInterText;
	char *interFlat;
	char *interPic;
	char *secretInterPic;
} dworkmapinfo_t;

typedef void (*kvcall_t) (char *key, char *value, void *ptr);

int G_BuiltinMapNumForMapName(const char* map)
{
	if (mystrlen(map) != 5)
		return 0;
	if (D_strncasecmp(map, "MAP", 3))
		return 0;
	if ((map[3] >= '0' && map[3] <= '9') && (map[4] >= '0' && map[4] <= '9'))
		return (map[3] - '0') * 10 + (map[4] - '0');
	return 0;
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
		char *data;
		len = W_LumpLength(lump);
		data = W_GetLumpData(lump);
		if (buf != data)
		{
			D_memset(buf, 0, (len+1) & ~1);
			D_memcpy(buf, data, len);
		}
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
			if (!p) p = sectionstart + mystrlen(sectionstart);
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
	while (p && (*p == ' ' || *p == '\t')) p++;
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

static void G_AddMapinfoKey(char* key, char* value, dworkmapinfo_t* mi)
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

			mi->lumpName = stripquote(p);

			p = pp;
			if (p) {
				mi->name = stripquote(p);
			}

			mi->mapNumber = G_BuiltinMapNumForMapName(mi->lumpName);

			if (!mi->name) {
				mi->name = mi->lumpName;
			}
		}
		else if (!D_strcasecmp(key, "baronspecial"))
		{
			mi->specials |= MI_BARON_SPECIAL;
		}
		else if (!D_strcasecmp(key, "cyberdemonspecial"))
		{
			mi->specials |= MI_CYBER_SPECIAL;
		}
		else if (!D_strcasecmp(key, "spidermastermindspecial"))
		{
			mi->specials |= MI_SPIDER_SPECIAL;
		}
		else if (!D_strcasecmp(key, "mancubispecial"))
		{
			mi->specials |= MI_FATSO_SPECIAL;
		}
		else if (!D_strcasecmp(key, "arachnospecial"))
		{
			mi->specials |= MI_BABY_SPECIAL;
		}
		else if (!D_strcasecmp(key, "cyberdemonspecial2"))
		{
			mi->specials |= MI_CYBER_SPECIAL|MI_CYBER_SPECIAL2;
		}
		else if (!D_strcasecmp(key, "spidermastermindspecial2"))
		{
			mi->specials |= MI_SPIDER_SPECIAL|MI_SPIDER_SPECIAL2;
		}
		else if (!D_strcasecmp(key, "pistolstart"))
		{
			mi->specials |= MI_PISTOL_START;
		}

		return;
	}

	if (!D_strcasecmp(key, "next"))
		mi->next = value;
	else if (!D_strcasecmp(key, "sky"))
		mi->sky = value;
	else if (!D_strcasecmp(key, "secretnext"))
		mi->secretNext = value;
	else if (!D_strcasecmp(key, "mapnumber"))
		mi->mapNumber = D_atoi(value);
	else if (!D_strcasecmp(key, "music"))
		mi->songNum = S_SongForName(value);
	else if (!D_strcasecmp(key, "cdTrack"))
		mi->cdaNum = D_atoi(value);
	else if (!D_strcasecmp(key, "intermissionText"))
		mi->interText = value;
	else if (!D_strcasecmp(key, "secretIntermissionText"))
		mi->secretInterText = value;
	else if (!D_strcasecmp(key, "intermissionFlat"))
		mi->interFlat = value;
	else if (!D_strcasecmp(key, "intermissionPic"))
		mi->interPic = value;
	else if (!D_strcasecmp(key, "secretIntermissionPic"))
		mi->secretInterPic = value;
}

static void G_FixSPCMDirList(dgameinfo_t *gi)
{
	int i;
	char list[sizeof(gi->spcmDirList)+2], *p;

	D_memset(list, 0, sizeof(list));
	D_snprintf(list, sizeof(list), "%s", &gi->spcmDirList[0][0]);
	D_memset(gi->spcmDirList, 0, sizeof(gi->spcmDirList));

	for (i = mystrlen(list) - 1; i > 0; i--)  {
		if (list[i] == ',') {
			list[i] = 0;
		}
	}

	p = list;
	for (i = 0; i < MAX_SPCM_PACKS; i++) {
		if (*p == '\0') {
			break;
		}
		D_snprintf(gi->spcmDirList[i], sizeof(gi->spcmDirList[i]), "%s", p);
		p += mystrlen(p) + 1;
	}
}

static void G_AddGameinfoKey(char* key, char* value, dgameinfo_t* gi)
{
	if (!D_strcasecmp(key, "borderFlat"))
		gi->borderFlat = value;
	else if (!D_strcasecmp(key, "titleTime"))
		gi->titleTime = D_atoi(value);
	else if (!D_strcasecmp(key, "creditsTime"))
		gi->creditsTime = D_atoi(value);
	else if (!D_strcasecmp(key, "titlePage"))
		gi->titlePage = value;
	else if (!D_strcasecmp(key, "creditsPage"))
		gi->creditsPage = value;
	else if (!D_strcasecmp(key, "titleMus"))
		gi->titleMus = value;
	else if (!D_strcasecmp(key, "titleCdTrack"))
		gi->titleCdTrack = D_atoi(value);
	else if (!D_strcasecmp(key, "intermissionMus"))
		gi->intermissionMus = value;
	else if (!D_strcasecmp(key, "intermissionCdTrack"))
		gi->intermissionCdTrack = D_atoi(value);
	else if (!D_strcasecmp(key, "victoryMus"))
		gi->victoryMus = value;
	else if (!D_strcasecmp(key, "victoryCdTrack"))
		gi->victoryCdTrack = D_atoi(value);
	else if (!D_strcasecmp(key, "endMus"))
		gi->endMus = value;
	else if (!D_strcasecmp(key, "endCdTrack"))
		gi->endCdTrack = D_atoi(value);
	else if (!D_strcasecmp(key, "endText"))
		gi->endText = value;
	else if (!D_strcasecmp(key, "endFlat"))
		gi->endFlat = value;
	else if (!D_strcasecmp(key, "endShowCast"))
		gi->endShowCast = D_atoi(value);
	else if (!D_strcasecmp(key, "noAttractDemo"))
		gi->noAttractDemo = D_atoi(value);
	else if (!D_strcasecmp(key, "stopFireTime"))
		gi->stopFireTime = D_atoi(value);
	else if (!D_strcasecmp(key, "titleStartPos"))
		gi->titleStartPos = D_atoi(value);
	else if (!D_strcasecmp(key, "cdTrackOffset"))
		gi->cdTrackOffset = D_atoi(value);
	else if (!D_strcasecmp(key, "spcmDirs"))
	{
		char *p = &gi->spcmDirList[0][0];
		D_memset(gi->spcmDirList, 0, sizeof(gi->spcmDirList));
		D_snprintf(p, sizeof(gi->spcmDirList), "%s", value);
		p[sizeof(gi->spcmDirList)-2] = '\0';
		p[sizeof(gi->spcmDirList)-1] = '\0';
	}
}

static void G_ClearGameInfo(dgameinfo_t* gi)
{
	D_memset(gi, 0, sizeof(*gi));
	gi->endShowCast = 1;
	gi->borderFlat = "";
	gi->endFlat = "";
	gi->titlePage = "";
	gi->titleMus = "";
	gi->intermissionMus = "";
	gi->victoryMus = "";
	gi->endMus = "";
	gi->creditsPage = "";
	gi->endText = "";
}

static dmapinfo_t *G_CompressMapInfo(dworkmapinfo_t *mi)
{
	int size = 0;
	char *buf;
	dmapinfo_t *nmi;

#define ALLOC_STR_FIELD(fld) do { int l = mi->fld ? mystrlen(mi->fld) : 0; size += (l > 0 ? l + 1 : 0); } while(0)

	size = sizeof(dmapinfo_t);
	ALLOC_STR_FIELD(name);
	ALLOC_STR_FIELD(next);
	ALLOC_STR_FIELD(secretNext);
	ALLOC_STR_FIELD(lumpName);
	ALLOC_STR_FIELD(interText);
	ALLOC_STR_FIELD(secretInterText);
	ALLOC_STR_FIELD(sky);
	ALLOC_STR_FIELD(interFlat);
	ALLOC_STR_FIELD(interPic);
	ALLOC_STR_FIELD(secretInterPic);

	buf = Z_Malloc(size, PU_STATIC);
	D_memset(buf, 0, size);

	nmi = (void*)buf;
	size = sizeof(dmapinfo_t);
	nmi->specials = mi->specials;
	nmi->mapNumber = mi->mapNumber;
	nmi->songNum = mi->songNum;
	nmi->cdaNum = mi->cdaNum;
	buf += size;

#define COPY_STR_FIELD(fld) do { \
		int l = mystrlen(mi->fld); \
		if (l > 0) { \
			nmi->fld = buf - (char *)nmi; \
			size = l + 1; \
			D_memcpy(buf, mi->fld, size); \
			buf += size; \
		} \
	} while (0)
	
	COPY_STR_FIELD(name);
	COPY_STR_FIELD(next);
	COPY_STR_FIELD(secretNext);
	COPY_STR_FIELD(lumpName);
	COPY_STR_FIELD(interText);
	COPY_STR_FIELD(secretInterText);
	COPY_STR_FIELD(sky);
	COPY_STR_FIELD(interFlat);
	COPY_STR_FIELD(interPic);
	COPY_STR_FIELD(secretInterPic);

	return nmi;
}

dmapinfo_t **G_LoadMaplist(int *pmapcount, dgameinfo_t* gi)
{
	const char* buf;
	size_t sectionlen = 0;
	const char* section = NULL, * ptr;
	int mapcount, i;
	dmapinfo_t** maplist;
	char tmpbuf[5000];

	G_ClearGameInfo(gi);

	mapcount = 0;
	*pmapcount = 0;

	buf = G_LoadMapinfoLump();
	if (!buf)
		return NULL;

	section = NULL;
	sectionlen = 0;
	for (ptr = buf; ; ptr = section + sectionlen + 1) {
		section = G_FindNextMapinfoSection(ptr, &sectionlen);
		if (!section)
			break;
		if (!D_strncasecmp(section, "gameinfo", 8))
			continue;
		mapcount++;
	}

	if (!mapcount)
		return NULL;

	maplist = Z_Malloc(sizeof(*maplist) * (mapcount + 1), PU_STATIC);

	i = 0;

	section = NULL;
	sectionlen = 0;
	for (ptr = buf; ; ptr = section + sectionlen + 1) {
		dworkmapinfo_t* mi;
		char* zsection;
		int linecount;

		section = G_FindNextMapinfoSection(ptr, &sectionlen);
		if (!section)
			break;

		if (!D_strncasecmp(section, "gameinfo", 8))
		{
			zsection = Z_Malloc(sectionlen + 1, PU_STATIC);
			D_memcpy(zsection, section, sectionlen);
			zsection[sectionlen] = '\0';
			gi->data = zsection;

			linecount = G_ParseMapinfo(zsection, (kvcall_t)&G_AddGameinfoKey, gi);
			if (linecount < 2)
			{
				Z_Free(gi->data);
				G_ClearGameInfo(gi);
			}

			G_FixSPCMDirList(gi);
			continue;
		}

		mi = (void *)tmpbuf;
		zsection = (char *)mi + sizeof(dworkmapinfo_t);
		D_memcpy(zsection, section, sectionlen);
		zsection[sectionlen] = '\0';

		D_memset(mi, 0, sizeof(*mi));
		linecount = G_ParseMapinfo(zsection, (kvcall_t)&G_AddMapinfoKey, mi);
		if (linecount < 2 || mi->mapNumber <= 0)
			continue;

		maplist[i] = G_CompressMapInfo(mi);
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
