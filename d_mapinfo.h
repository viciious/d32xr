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
#ifndef D_MAPINFO_H__
#define D_MAPINFO_H__

typedef struct
{
	VINT baronspecial;
	char *name;
	char *sky;
	int next; // lump num
	int secretnext;
	int lumpnum;
	int mapnumber;
	int music;
	char lumpname[9];
	void *data;
} dmapinfo_t;

int G_LumpNumForMapNum(int map);
int G_MapNumForMapName(const char* map);
char* G_GetMapNameForLump(int lump);

int G_FindMapinfo(int maplump, dmapinfo_t *mi);
dmapinfo_t** G_LoadMaplist(int *pmapcount);

#endif // D_MAPINFO_H__

