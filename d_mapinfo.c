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
#include "d_mapinfo.h"
#include <string.h>
#include <stdlib.h>

//#define TEST

#ifdef TEST
const char *testlump = ""
"map \"foox\" \"foobar1\"\n"
"{\n"
"\n"
"}\n"
"\n"
"map \"776\" \"foobar\"\n"
"{\n"
"	next = \"930\"\n"
"	sky = \"SKY1\"\n"
"}\n"
"\n"
"map \"776\" \"foobar\"\n"
"{\n"
"	next = \"730\"\n"
"}\n"
"\n"
"map \"foo1\" \"foobar2\"\n"
"{\n"
"\n"
"}\n"
"";
#endif

typedef void (*kvcall_t) (char *key, char *value, void *ptr);

static char* G_LoadMapinfoLump(void)
{
	int len;
	int lump;
	char *buf;

	lump = W_CheckNumForName("DMAPINFO");
	if (lump < 0)
		return NULL;

	len = W_LumpLength(lump);
	buf = (char *)I_TempBuffer();
	W_ReadLump(lump, buf);
	buf[len] = '\0';

	return buf;
}

static char* G_FindNextMapinfoSection(char *buf, size_t *len)
{
	char *l = buf;
	int state = 0; // -1 == exit, expect section, expect {, expect property or }
	char* sectionstart = NULL;

	if (!buf || !*buf)
		return NULL;

	while (l) {
		char* p;
		boolean bracket;
		
		if (state < 0)
			break;

		p = strchr(l, '\n');
		if (p) *p = '\0';
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
		
		if (p)
			*p = '\n';

		if (state == 0 && sectionstart != NULL)
		{
			*p = '\0';
			*len = p - sectionstart;
			return sectionstart;
		}

		if (!p) break;
		l = p + 1;
	}

	return NULL;
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
			el = l + strlen(l);
		}
		while (*l == ' ' || *l == '\t') l++;

		if (linecount > 0) { // skip the first line
			e = strchr(l, '=');
			if (e && e + 1 != el) {
				boolean stripquote = false;
				char* val = e + 1;
				size_t vallen;

				while (*val == ' ' || *val == '\t') val++;

				while (*(e-1) == ' ' || *(e-1) == '\t') e--;
				*e = '\0';
				
				stripquote = false;

				if (*val == '"') {
					val++;
					stripquote = true;
				}
				vallen = mystrlen(val);
				if (stripquote && val[vallen - 1] == '"') {
					val[vallen - 1] = '\0';
				}

				kvcall(l, val, ptr);
			}
		}

		if (!p)
			break;

		l = p + 1;
		linecount++;
	}

	return linecount;
}

static char* G_FindMapinfoSection(char* buf, int maplump)
{
	char name[16];
	char* section = NULL, *ptr;
	size_t namelen, sectionlen;

	namelen = D_snprintf(name, sizeof(name), "map \"%d\"", maplump);

	ptr = buf;
	while (true) {
		section = G_FindNextMapinfoSection(ptr, &sectionlen);
		if (!section)
			break;
		if (!D_strncasecmp(section, name, namelen))
			break;
		ptr = section + sectionlen + 1;
	}
	return section;
}

static void G_AddMapinfoKey(char *key, char *value, dmapinfo_t *mi)
{
	if (!D_strcasecmp(key, "next"))
		mi->next = atoi(value);
	else if (!D_strcasecmp(key, "sky"))
		mi->sky = value;
	else if (!D_strcasecmp(key, "secretnext"))
		mi->secretnext = atoi(value);
}

int G_FindMapinfo(int maplump, dmapinfo_t *mi)
{
	char* section;
	char* buf;
	int linecount;

	if (maplump < 0)
		return -1;

	D_memset(mi, 0, sizeof(*mi));

#ifdef TEST
	buf = I_TempBuffer();
	D_memcpy(buf, (void*)testlump, strlen(testlump) + 1);
#else
	buf = G_LoadMapinfoLump();
#endif

	if (!buf)
		return 0;

	section = G_FindMapinfoSection(buf, maplump);
	if (!section)
		goto notfound;

	linecount = G_ParseMapinfo(section, (kvcall_t)&G_AddMapinfoKey, mi);
	if (linecount < 2)
		goto notfound;

	mi->data = buf;
	return 1;

notfound:
	return 0;
}

